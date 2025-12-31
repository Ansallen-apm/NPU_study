// SMMU SystemC TLM Testbench
// 測試 SMMU TLM wrapper 的功能
// 模擬多個設備通過 SMMU 進行 DMA 訪問

#include <systemc>
#include <tlm>
#include "smmu_tlm_wrapper.h"
#include <iostream>
#include <iomanip>

using namespace smmu_tlm;
using namespace smmu;

// ============================================================================
// 簡單的內存模型（TLM target）
// 接收來自 SMMU 的轉換後事務
// ============================================================================

class SimpleMemory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimpleMemory> target_socket;
    
    SC_HAS_PROCESS(SimpleMemory);
    
    SimpleMemory(sc_core::sc_module_name name, size_t size = 256 * 1024 * 1024)
        : sc_module(name)
        , target_socket("target_socket")
        , memory_size_(size)
    {
        memory_.resize(size, 0);
        target_socket.register_b_transport(this, &SimpleMemory::b_transport);
    }
    
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        tlm::tlm_command cmd = trans.get_command();
        uint64_t addr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        
        // 檢查地址範圍
        if (addr + len > memory_size_) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        
        // 執行讀寫操作
        if (cmd == tlm::TLM_READ_COMMAND) {
            std::memcpy(ptr, &memory_[addr], len);
        } else if (cmd == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&memory_[addr], ptr, len);
        }
        
        // 添加內存訪問延遲
        delay += sc_core::sc_time(50, sc_core::SC_NS);
        
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    
    std::vector<unsigned char> memory_;
    size_t memory_size_;
};

// ============================================================================
// 設備模擬器（TLM initiator）
// 模擬設備發起 DMA 訪問
// ============================================================================

class DeviceSimulator : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<DeviceSimulator> initiator_socket;
    
    SC_HAS_PROCESS(DeviceSimulator);
    
    DeviceSimulator(sc_core::sc_module_name name,
                    StreamID stream_id,
                    ASID asid,
                    const std::string& device_name)
        : sc_module(name)
        , initiator_socket("initiator_socket")
        , stream_id_(stream_id)
        , asid_(asid)
        , device_name_(device_name)
    {
        SC_THREAD(device_thread);
    }
    
private:
    void device_thread() {
        // 等待系統初始化
        wait(sc_core::sc_time(100, sc_core::SC_NS));
        
        std::cout << "\n[" << sc_core::sc_time_stamp() << "] "
                  << device_name_ << " starting DMA operations...\n";
        
        // 執行多次 DMA 訪問
        for (int i = 0; i < 5; i++) {
            // 讀操作
            perform_dma_read(i * 0x1000, 64);
            wait(sc_core::sc_time(200, sc_core::SC_NS));
            
            // 寫操作
            perform_dma_write(i * 0x1000 + 0x100, 64);
            wait(sc_core::sc_time(200, sc_core::SC_NS));
        }
        
        std::cout << "[" << sc_core::sc_time_stamp() << "] "
                  << device_name_ << " completed DMA operations\n";
    }
    
    void perform_dma_read(uint64_t address, unsigned int length) {
        unsigned char data[256];
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        
        // 設置事務
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        // 添加 AXI 擴展
        AXIExtension axi_ext;
        axi_ext.stream_id = stream_id_;
        axi_ext.asid = asid_;
        axi_ext.vmid = 0;
        set_axi_extension(trans, axi_ext);
        
        // 發送事務
        initiator_socket->b_transport(trans, delay);
        
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            std::cout << "[" << sc_core::sc_time_stamp() << "] "
                      << device_name_ << " READ  VA=0x" << std::hex << address
                      << " len=" << std::dec << length << " - SUCCESS\n";
        } else {
            std::cout << "[" << sc_core::sc_time_stamp() << "] "
                      << device_name_ << " READ  VA=0x" << std::hex << address
                      << " - FAILED\n";
        }
        
        wait(delay);
    }
    
    void perform_dma_write(uint64_t address, unsigned int length) {
        unsigned char data[256];
        std::memset(data, 0xAA, length);
        
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        
        // 設置事務
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        // 添加 AXI 擴展
        AXIExtension axi_ext;
        axi_ext.stream_id = stream_id_;
        axi_ext.asid = asid_;
        axi_ext.vmid = 0;
        set_axi_extension(trans, axi_ext);
        
        // 發送事務
        initiator_socket->b_transport(trans, delay);
        
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            std::cout << "[" << sc_core::sc_time_stamp() << "] "
                      << device_name_ << " WRITE VA=0x" << std::hex << address
                      << " len=" << std::dec << length << " - SUCCESS\n";
        } else {
            std::cout << "[" << sc_core::sc_time_stamp() << "] "
                      << device_name_ << " WRITE VA=0x" << std::hex << address
                      << " - FAILED\n";
        }
        
        wait(delay);
    }
    
    StreamID stream_id_;
    ASID asid_;
    std::string device_name_;
};

// ============================================================================
// 頂層測試模塊
// ============================================================================

class TopLevel : public sc_core::sc_module {
public:
    // 組件
    std::unique_ptr<SMMUTLMWrapper> smmu;
    std::unique_ptr<SimpleMemory> memory;
    std::vector<std::unique_ptr<DeviceSimulator>> devices;
    
    SC_HAS_PROCESS(TopLevel);
    
    TopLevel(sc_core::sc_module_name name)
        : sc_module(name)
    {
        // 創建 SMMU
        SMMUConfig smmu_config;
        smmu_config.tlb_size = 128;
        
        SMMUTLMConfig tlm_config;
        tlm_config.num_input_ports = 3;  // 3個設備
        
        smmu = std::make_unique<SMMUTLMWrapper>("smmu", smmu_config, tlm_config);
        
        // 創建內存
        memory = std::make_unique<SimpleMemory>("memory");
        
        // 連接 SMMU 輸出端口到內存
        smmu->data_output_port->initiator_socket.bind(memory->target_socket);
        smmu->ptw_output_port->initiator_socket.bind(memory->target_socket);
        
        // 創建設備
        devices.push_back(std::make_unique<DeviceSimulator>("gpu", 0, 1, "GPU"));
        devices.push_back(std::make_unique<DeviceSimulator>("nic", 1, 2, "Network"));
        devices.push_back(std::make_unique<DeviceSimulator>("disk", 2, 3, "Storage"));
        
        // 連接設備到 SMMU 輸入端口
        for (size_t i = 0; i < devices.size(); i++) {
            devices[i]->initiator_socket.bind(smmu->input_ports[i]->target_socket);
        }
        
        // 配置 SMMU
        SC_THREAD(setup_thread);
    }
    
private:
    void setup_thread() {
        // 等待一點時間
        wait(sc_core::sc_time(10, sc_core::SC_NS));
        
        std::cout << "\n╔════════════════════════════════════════╗\n";
        std::cout << "║   SMMU TLM Testbench                   ║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";
        
        // 設置頁表
        setup_page_tables();
        
        // 配置 SMMU 流表項和上下文
        configure_smmu();
        
        // 啟用 SMMU
        smmu->enable_smmu();
        
        std::cout << "SMMU configuration complete\n\n";
        
        // 等待模擬完成
        wait(sc_core::sc_time(10, sc_core::SC_US));
        
        // 打印統計信息
        smmu->print_statistics();
        
        // 停止模擬
        sc_core::sc_stop();
    }
    
    void setup_page_tables() {
        auto memory_model = smmu->get_memory_model();
        
        // 為每個設備設置頁表
        for (int dev = 0; dev < 3; dev++) {
            PhysicalAddress l0 = memory_model->allocate_page();
            PhysicalAddress l1 = memory_model->allocate_page();
            PhysicalAddress l2 = memory_model->allocate_page();
            PhysicalAddress l3 = memory_model->allocate_page();
            
            // 設置頁表層次結構
            memory_model->write_pte(l0, l1 | 0x3);
            memory_model->write_pte(l1, l2 | 0x3);
            memory_model->write_pte(l2, l3 | 0x3);
            
            // 映射頁面
            PhysicalAddress base_pa = 0x100000 + (dev * 0x100000);
            for (int i = 0; i < 16; i++) {
                PhysicalAddress pa = base_pa + (i * 0x1000);
                uint64_t pte = pa | 0x403 | (0x4 << 2);
                memory_model->write_pte(l3 + (i * 8), pte);
            }
            
            // 配置上下文描述符
            ContextDescriptor cd;
            cd.valid = true;
            cd.translation_table_base = l0;
            cd.translation_granule = 12;
            cd.ips = 48;
            cd.asid = dev + 1;
            
            smmu->configure_context(dev, dev + 1, cd);
            
            std::cout << "Configured page tables for device " << dev 
                      << " (ASID " << (dev + 1) << ")\n";
        }
    }
    
    void configure_smmu() {
        // 配置每個設備的流表項
        for (int dev = 0; dev < 3; dev++) {
            StreamTableEntry ste;
            ste.valid = true;
            ste.s1_enabled = true;
            ste.s2_enabled = false;
            
            smmu->configure_stream(dev, ste);
        }
    }
};

// ============================================================================
// sc_main - SystemC 入口點
// ============================================================================

int sc_main(int argc, char* argv[]) {
    // 創建頂層模塊
    TopLevel top("top");
    
    // 運行模擬
    sc_core::sc_start();
    
    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║   Simulation Complete! ✅              ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
    
    return 0;
}
