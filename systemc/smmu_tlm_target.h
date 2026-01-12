// SMMU TLM Target Socket Wrapper
// 實現輸入端口，接收來自設備的 AXI 事務

#ifndef SMMU_TLM_TARGET_H
#define SMMU_TLM_TARGET_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include "tlm_types.h"
#include "smmu.h"
#include <queue>
#include <functional>

namespace smmu_tlm {

// ============================================================================
// SMMU TLM Target Port
// 每個輸入端口對應一個或多個設備
// ============================================================================

class SMMUTLMTarget : public sc_core::sc_module {
public:
    // TLM target socket（接收事務）
    tlm_utils::simple_target_socket<SMMUTLMTarget> target_socket;
    
    // 構造函數
    SC_HAS_PROCESS(SMMUTLMTarget);
    
    SMMUTLMTarget(sc_core::sc_module_name name, 
                  uint32_t port_id,
                  const TLMPortConfig& config)
        : sc_module(name)
        , target_socket("target_socket")
        , port_id_(port_id)
        , config_(config)
        , translation_callback_(nullptr)
    {
        // 註冊 TLM blocking transport 回調
        target_socket.register_b_transport(this, &SMMUTLMTarget::b_transport);
        
        // 註冊 TLM debug transport 回調
        target_socket.register_transport_dbg(this, &SMMUTLMTarget::transport_dbg);
        
        // 註冊 DMI 回調
        target_socket.register_get_direct_mem_ptr(this, &SMMUTLMTarget::get_direct_mem_ptr);
        
        SC_THREAD(process_thread);
    }
    
    // 設置地址轉換回調函數
    using TranslationCallback = std::function<smmu::TranslationResult(
        smmu::VirtualAddress, smmu::StreamID, smmu::ASID, smmu::VMID)>;
    
    void set_translation_callback(TranslationCallback callback) {
        translation_callback_ = callback;
    }
    
    // 獲取統計信息
    const TLMStatistics& get_statistics() const {
        return stats_;
    }
    
    void reset_statistics() {
        stats_.reset();
    }

private:
    // ========================================================================
    // TLM Blocking Transport
    // 處理來自設備的事務
    // ========================================================================
    
    virtual void b_transport(tlm::tlm_generic_payload& trans, 
                            sc_core::sc_time& delay) {
        // 檢查端口是否啟用
        if (!config_.enabled) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        
        // 獲取 AXI 擴展
        AXIExtension* axi_ext = get_axi_extension(trans);
        if (!axi_ext) {
            SC_REPORT_ERROR("SMMU_TLM_TARGET", "Missing AXI extension");
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        
        // 記錄事務開始時間
        sc_core::sc_time start_time = sc_core::sc_time_stamp();
        
        // 執行地址轉換
        smmu::VirtualAddress va = trans.get_address();
        smmu::TranslationResult result;
        
        if (translation_callback_) {
            result = translation_callback_(va, axi_ext->stream_id, 
                                          axi_ext->asid, axi_ext->vmid);
        } else {
            // 沒有設置回調，直接通過（bypass 模式）
            result.success = true;
            result.physical_addr = va;
        }
        
        // 處理轉換結果
        if (result.success) {
            // 轉換成功，更新物理地址
            trans.set_address(result.physical_addr);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            
            // 添加轉換延遲（模擬 SMMU 處理時間）
            delay += sc_core::sc_time(10, sc_core::SC_NS);
        } else {
            // 轉換失敗
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            stats_.translation_errors++;
            
            SC_REPORT_WARNING("SMMU_TLM_TARGET", 
                            ("Translation failed: " + result.fault_reason).c_str());
        }
        
        // 更新統計信息
        stats_.total_transactions++;
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            stats_.read_transactions++;
        } else {
            stats_.write_transactions++;
        }
        
        // 計算延遲
        sc_core::sc_time end_time = sc_core::sc_time_stamp() + delay;
        sc_core::sc_time latency = end_time - start_time;
        stats_.total_latency_cycles += latency.value() / sc_core::sc_time(1, sc_core::SC_NS).value();
    }
    
    // ========================================================================
    // TLM Debug Transport
    // 用於調試訪問，不經過地址轉換
    // ========================================================================
    
    virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        // Debug transport 直接通過，不進行地址轉換
        return trans.get_data_length();
    }
    
    // ========================================================================
    // DMI（Direct Memory Interface）
    // 本實現不支持 DMI
    // ========================================================================
    
    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                   tlm::tlm_dmi& dmi_data) {
        // SMMU 不支持 DMI（因為需要動態地址轉換）
        return false;
    }
    
    // ========================================================================
    // 處理線程
    // 用於異步處理和事件生成
    // ========================================================================
    
    void process_thread() {
        while (true) {
            wait(sc_core::sc_time(1, sc_core::SC_US));
            // 可以在這裡處理異步事件
        }
    }
    
    // ========================================================================
    // 私有成員
    // ========================================================================
    
    uint32_t port_id_;                    // 端口ID
    TLMPortConfig config_;                // 端口配置
    TranslationCallback translation_callback_; // 地址轉換回調
    TLMStatistics stats_;                 // 統計信息
};

} // namespace smmu_tlm

#endif // SMMU_TLM_TARGET_H
