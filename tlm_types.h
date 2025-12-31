// SystemC TLM 類型定義
// 定義 AXI 事務、QoS 和其他 TLM 相關的數據結構

#ifndef TLM_TYPES_H
#define TLM_TYPES_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "smmu_types.h"

namespace smmu_tlm {

// ============================================================================
// AXI 事務類型
// ============================================================================

enum class AXICommand {
    READ,   // 讀取操作
    WRITE   // 寫入操作
};

// ============================================================================
// QoS（Quality of Service）配置
// 用於 PTW（Page Table Walk）端口的優先級控制
// ============================================================================

struct QoSConfig {
    uint8_t priority;        // 優先級 (0-15, 15最高)
    uint8_t urgency;         // 緊急度 (0-15)
    bool preemptible;        // 是否可被搶占
    uint32_t bandwidth_limit; // 帶寬限制 (bytes/cycle)
    
    QoSConfig() 
        : priority(8), urgency(8), preemptible(true), 
          bandwidth_limit(0xFFFFFFFF) {}
};

// ============================================================================
// AXI 事務擴展
// 擴展 TLM generic payload 以支持 AXI 特定的屬性
// ============================================================================

class AXIExtension : public tlm::tlm_extension<AXIExtension> {
public:
    // 必須實現的虛函數
    virtual tlm_extension_base* clone() const override {
        return new AXIExtension(*this);
    }
    
    virtual void copy_from(const tlm_extension_base& ext) override {
        const AXIExtension& axi_ext = static_cast<const AXIExtension&>(ext);
        stream_id = axi_ext.stream_id;
        asid = axi_ext.asid;
        vmid = axi_ext.vmid;
        qos = axi_ext.qos;
        is_ptw = axi_ext.is_ptw;
        burst_length = axi_ext.burst_length;
        burst_size = axi_ext.burst_size;
        cache_attr = axi_ext.cache_attr;
        prot_attr = axi_ext.prot_attr;
    }
    
    // AXI 屬性
    smmu::StreamID stream_id;    // 流ID（設備標識）
    smmu::ASID asid;              // 地址空間ID
    smmu::VMID vmid;              // 虛擬機ID
    QoSConfig qos;                // QoS 配置
    bool is_ptw;                  // 是否為頁表遍歷事務
    
    // AXI 突發屬性
    uint8_t burst_length;         // 突發長度 (1-256)
    uint8_t burst_size;           // 突發大小 (log2(bytes))
    
    // AXI 緩存和保護屬性
    uint8_t cache_attr;           // 緩存屬性 (AxCACHE)
    uint8_t prot_attr;            // 保護屬性 (AxPROT)
    
    AXIExtension() 
        : stream_id(0), asid(0), vmid(0), is_ptw(false),
          burst_length(1), burst_size(2), // 默認4字節
          cache_attr(0x0F), prot_attr(0x00) {}
};

// ============================================================================
// TLM 端口配置
// ============================================================================

struct TLMPortConfig {
    std::string name;             // 端口名稱
    uint32_t base_address;        // 基地址
    uint32_t address_range;       // 地址範圍
    bool enabled;                 // 是否啟用
    
    TLMPortConfig() 
        : name("port"), base_address(0), 
          address_range(0xFFFFFFFF), enabled(true) {}
};

// ============================================================================
// SMMU TLM 配置
// ============================================================================

struct SMMUTLMConfig {
    size_t num_input_ports;       // 輸入端口數量
    size_t num_output_ports;      // 輸出端口數量（固定為2）
    bool ptw_qos_enabled;         // PTW QoS 是否啟用
    QoSConfig default_qos;        // 默認 QoS 配置
    QoSConfig ptw_qos;            // PTW 專用 QoS 配置
    
    SMMUTLMConfig() 
        : num_input_ports(4), num_output_ports(2), 
          ptw_qos_enabled(true) {
        // PTW 使用更高的優先級
        ptw_qos.priority = 15;
        ptw_qos.urgency = 15;
        ptw_qos.preemptible = false;
    }
};

// ============================================================================
// 事務統計
// ============================================================================

struct TLMStatistics {
    uint64_t total_transactions;   // 總事務數
    uint64_t read_transactions;    // 讀事務數
    uint64_t write_transactions;   // 寫事務數
    uint64_t ptw_transactions;     // PTW 事務數
    uint64_t translation_errors;   // 轉換錯誤數
    uint64_t total_latency_cycles; // 總延遲週期
    
    TLMStatistics() 
        : total_transactions(0), read_transactions(0), 
          write_transactions(0), ptw_transactions(0),
          translation_errors(0), total_latency_cycles(0) {}
    
    void reset() {
        total_transactions = 0;
        read_transactions = 0;
        write_transactions = 0;
        ptw_transactions = 0;
        translation_errors = 0;
        total_latency_cycles = 0;
    }
    
    double get_average_latency() const {
        if (total_transactions == 0) return 0.0;
        return static_cast<double>(total_latency_cycles) / total_transactions;
    }
};

// ============================================================================
// 輔助函數
// ============================================================================

// 從 TLM payload 獲取 AXI 擴展
inline AXIExtension* get_axi_extension(tlm::tlm_generic_payload& trans) {
    AXIExtension* ext = nullptr;
    trans.get_extension(ext);
    return ext;
}

// 為 TLM payload 設置 AXI 擴展
inline void set_axi_extension(tlm::tlm_generic_payload& trans, 
                              const AXIExtension& ext) {
    AXIExtension* new_ext = new AXIExtension(ext);
    trans.set_extension(new_ext);
}

// 創建 AXI 讀事務
inline void create_axi_read(tlm::tlm_generic_payload& trans,
                           uint64_t address, 
                           unsigned char* data,
                           unsigned int length,
                           const AXIExtension& axi_ext) {
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(data);
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    set_axi_extension(trans, axi_ext);
}

// 創建 AXI 寫事務
inline void create_axi_write(tlm::tlm_generic_payload& trans,
                            uint64_t address,
                            unsigned char* data,
                            unsigned int length,
                            const AXIExtension& axi_ext) {
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(data);
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    set_axi_extension(trans, axi_ext);
}

} // namespace smmu_tlm

#endif // TLM_TYPES_H
