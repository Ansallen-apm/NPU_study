// SMMU SystemC TLM Wrapper
// 將 SMMU 功能模型封裝為 SystemC TLM 模塊
// 支持多組輸入端口和兩組輸出端口（數據端口和 PTW 端口）

#ifndef SMMU_TLM_WRAPPER_H
#define SMMU_TLM_WRAPPER_H

#include <systemc>
#include <tlm>
#include "smmu.h"
#include "smmu_tlm_target.h"
#include "smmu_tlm_initiator.h"
#include "tlm_types.h"
#include <vector>
#include <memory>

namespace smmu_tlm {

// ============================================================================
// SMMU SystemC TLM Wrapper
// 主要的 SystemC 模塊，集成所有 TLM 端口和 SMMU 核心
// ============================================================================

class SMMUTLMWrapper : public sc_core::sc_module {
public:
    // ========================================================================
    // TLM 端口
    // ========================================================================
    
    // 輸入端口（target sockets）- 接收來自設備的事務
    std::vector<std::unique_ptr<SMMUTLMTarget>> input_ports;
    
    // 輸出端口（initiator sockets）
    std::unique_ptr<SMMUTLMInitiator> data_output_port;  // 數據端口
    std::unique_ptr<SMMUTLMInitiator> ptw_output_port;   // PTW 專用端口
    
    // ========================================================================
    // 構造函數
    // ========================================================================
    
    SC_HAS_PROCESS(SMMUTLMWrapper);
    
    SMMUTLMWrapper(sc_core::sc_module_name name,
                   const smmu::SMMUConfig& smmu_config = smmu::SMMUConfig(),
                   const SMMUTLMConfig& tlm_config = SMMUTLMConfig())
        : sc_module(name)
        , smmu_config_(smmu_config)
        , tlm_config_(tlm_config)
        , memory_(std::make_shared<smmu::SimpleMemoryModel>())
        , smmu_(std::make_unique<smmu::SMMU>(smmu_config))
    {
        // 初始化 SMMU
        smmu_->set_memory_model(memory_);
        
        // 創建輸入端口
        create_input_ports();
        
        // 創建輸出端口
        create_output_ports();
        
        // 設置內存讀取回調（用於 PTW）
        setup_memory_callback();
        
        // 註冊 SystemC 線程
        SC_THREAD(smmu_process_thread);
        SC_THREAD(statistics_thread);
    }
    
    // ========================================================================
    // SMMU 配置接口
    // ========================================================================
    
    // 配置流表項
    void configure_stream(smmu::StreamID stream_id,
                         const smmu::StreamTableEntry& ste) {
        smmu_->configure_stream_table_entry(stream_id, ste);
    }
    
    // 配置上下文描述符
    void configure_context(smmu::StreamID stream_id,
                          smmu::ASID asid,
                          const smmu::ContextDescriptor& cd) {
        smmu_->configure_context_descriptor(stream_id, asid, cd);
    }
    
    // 啟用/禁用 SMMU
    void enable_smmu() {
        smmu_->enable();
        SC_REPORT_INFO("SMMU_TLM_WRAPPER", "SMMU enabled");
    }
    
    void disable_smmu() {
        smmu_->disable();
        SC_REPORT_INFO("SMMU_TLM_WRAPPER", "SMMU disabled");
    }
    
    // ========================================================================
    // 統計和監控
    // ========================================================================
    
    // 獲取 SMMU 統計信息
    smmu::SMMU::Statistics get_smmu_statistics() const {
        return smmu_->get_statistics();
    }
    
    // 獲取 TLM 統計信息
    TLMStatistics get_tlm_statistics() const {
        TLMStatistics total_stats;
        
        // 累加所有輸入端口的統計
        for (const auto& port : input_ports) {
            const auto& stats = port->get_statistics();
            total_stats.total_transactions += stats.total_transactions;
            total_stats.read_transactions += stats.read_transactions;
            total_stats.write_transactions += stats.write_transactions;
            total_stats.translation_errors += stats.translation_errors;
            total_stats.total_latency_cycles += stats.total_latency_cycles;
        }
        
        // 添加輸出端口的統計
        if (data_output_port) {
            const auto& stats = data_output_port->get_statistics();
            total_stats.ptw_transactions += stats.ptw_transactions;
        }
        
        if (ptw_output_port) {
            const auto& stats = ptw_output_port->get_statistics();
            total_stats.ptw_transactions += stats.ptw_transactions;
        }
        
        return total_stats;
    }
    
    // 打印統計信息
    void print_statistics() const {
        auto smmu_stats = get_smmu_statistics();
        auto tlm_stats = get_tlm_statistics();
        
        std::cout << "\n╔════════════════════════════════════════╗\n";
        std::cout << "║   SMMU TLM Wrapper Statistics          ║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";
        
        std::cout << "SMMU Core Statistics:\n";
        std::cout << "  Total translations:    " << smmu_stats.total_translations << "\n";
        std::cout << "  TLB hits:              " << smmu_stats.tlb_hits << "\n";
        std::cout << "  TLB misses:            " << smmu_stats.tlb_misses << "\n";
        std::cout << "  Page table walks:      " << smmu_stats.page_table_walks << "\n";
        std::cout << "  Translation faults:    " << smmu_stats.translation_faults << "\n\n";
        
        std::cout << "TLM Interface Statistics:\n";
        std::cout << "  Total transactions:    " << tlm_stats.total_transactions << "\n";
        std::cout << "  Read transactions:     " << tlm_stats.read_transactions << "\n";
        std::cout << "  Write transactions:    " << tlm_stats.write_transactions << "\n";
        std::cout << "  PTW transactions:      " << tlm_stats.ptw_transactions << "\n";
        std::cout << "  Translation errors:    " << tlm_stats.translation_errors << "\n";
        std::cout << "  Average latency:       " << tlm_stats.get_average_latency() 
                  << " ns\n\n";
    }
    
    // ========================================================================
    // 內存訪問接口（用於測試）
    // ========================================================================
    
    std::shared_ptr<smmu::SimpleMemoryModel> get_memory_model() {
        return memory_;
    }

private:
    // ========================================================================
    // 初始化函數
    // ========================================================================
    
    // 創建輸入端口
    void create_input_ports() {
        for (size_t i = 0; i < tlm_config_.num_input_ports; i++) {
            TLMPortConfig port_config;
            port_config.name = "input_port_" + std::to_string(i);
            port_config.enabled = true;
            
            auto port = std::make_unique<SMMUTLMTarget>(
                port_config.name.c_str(), i, port_config);
            
            // 設置地址轉換回調
            port->set_translation_callback(
                [this](smmu::VirtualAddress va, smmu::StreamID sid, 
                       smmu::ASID asid, smmu::VMID vmid) {
                    return smmu_->translate(va, sid, asid, vmid);
                });
            
            input_ports.push_back(std::move(port));
        }
        
        SC_REPORT_INFO("SMMU_TLM_WRAPPER", 
            ("Created " + std::to_string(tlm_config_.num_input_ports) + 
             " input ports").c_str());
    }
    
    // 創建輸出端口
    void create_output_ports() {
        // 創建數據輸出端口
        data_output_port = std::make_unique<SMMUTLMInitiator>(
            "data_output_port",
            OutputPortType::DATA_PORT,
            tlm_config_.default_qos);
        
        // 創建 PTW 輸出端口（使用更高的 QoS）
        ptw_output_port = std::make_unique<SMMUTLMInitiator>(
            "ptw_output_port",
            OutputPortType::PTW_PORT,
            tlm_config_.ptw_qos);
        
        SC_REPORT_INFO("SMMU_TLM_WRAPPER", "Created 2 output ports (DATA + PTW)");
    }
    
    // 設置內存讀取回調（用於 PTW）
    void setup_memory_callback() {
        // PTW 需要通過 PTW 端口訪問內存
        // 這裡我們直接使用內存模型，實際硬件中會通過 TLM 端口
    }
    
    // ========================================================================
    // SystemC 線程
    // ========================================================================
    
    // SMMU 處理線程
    void smmu_process_thread() {
        while (true) {
            // 處理 SMMU 命令隊列
            smmu_->process_commands();
            
            // 處理事件隊列
            while (smmu_->has_events()) {
                auto event = smmu_->pop_event();
                SC_REPORT_WARNING("SMMU_EVENT",
                    ("Fault: " + event.description + 
                     " at VA 0x" + std::to_string(event.va)).c_str());
            }
            
            // 等待一段時間
            wait(sc_core::sc_time(100, sc_core::SC_NS));
        }
    }
    
    // 統計線程
    void statistics_thread() {
        while (true) {
            wait(sc_core::sc_time(1, sc_core::SC_MS));
            
            // 可以在這裡定期輸出統計信息
            // print_statistics();
        }
    }
    
    // ========================================================================
    // 私有成員
    // ========================================================================
    
    smmu::SMMUConfig smmu_config_;        // SMMU 配置
    SMMUTLMConfig tlm_config_;            // TLM 配置
    
    std::shared_ptr<smmu::SimpleMemoryModel> memory_;  // 內存模型
    std::unique_ptr<smmu::SMMU> smmu_;                 // SMMU 核心
};

} // namespace smmu_tlm

#endif // SMMU_TLM_WRAPPER_H
