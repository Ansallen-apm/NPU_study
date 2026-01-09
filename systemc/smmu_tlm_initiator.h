// SMMU TLM Initiator Socket Wrapper
// 實現輸出端口，發送轉換後的事務到內存系統
// 包括普通數據端口和 PTW（Page Table Walk）專用端口

#ifndef SMMU_TLM_INITIATOR_H
#define SMMU_TLM_INITIATOR_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "tlm_types.h"
#include <queue>

namespace smmu_tlm {

// ============================================================================
// 輸出端口類型
// ============================================================================

enum class OutputPortType {
    DATA_PORT,  // 普通數據端口（用於設備 DMA）
    PTW_PORT    // PTW 專用端口（用於頁表遍歷）
};

// ============================================================================
// SMMU TLM Initiator Port
// 發送轉換後的事務到下游內存系統
// ============================================================================

class SMMUTLMInitiator : public sc_core::sc_module {
public:
    // TLM initiator socket（發送事務）
    tlm_utils::simple_initiator_socket<SMMUTLMInitiator> initiator_socket;
    
    // 構造函數
    SC_HAS_PROCESS(SMMUTLMInitiator);
    
    SMMUTLMInitiator(sc_core::sc_module_name name,
                     OutputPortType port_type,
                     const QoSConfig& qos_config = QoSConfig())
        : sc_module(name)
        , initiator_socket("initiator_socket")
        , port_type_(port_type)
        , qos_config_(qos_config)
        , enabled_(true)
    {
        SC_THREAD(process_thread);
    }
    
    // ========================================================================
    // 發送事務
    // ========================================================================
    
    // 發送讀事務
    tlm::tlm_response_status send_read(uint64_t address,
                                       unsigned char* data,
                                       unsigned int length,
                                       sc_core::sc_time& delay,
                                       const AXIExtension* axi_ext = nullptr) {
        if (!enabled_) {
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        }
        
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        // 添加 AXI 擴展和 QoS 信息
        if (axi_ext) {
            AXIExtension ext = *axi_ext;
            ext.qos = qos_config_;
            ext.is_ptw = (port_type_ == OutputPortType::PTW_PORT);
            set_axi_extension(trans, ext);
        } else {
            AXIExtension ext;
            ext.qos = qos_config_;
            ext.is_ptw = (port_type_ == OutputPortType::PTW_PORT);
            set_axi_extension(trans, ext);
        }
        
        // 發送事務
        initiator_socket->b_transport(trans, delay);
        
        // 更新統計
        stats_.total_transactions++;
        stats_.read_transactions++;
        if (port_type_ == OutputPortType::PTW_PORT) {
            stats_.ptw_transactions++;
        }
        
        return trans.get_response_status();
    }
    
    // 發送寫事務
    tlm::tlm_response_status send_write(uint64_t address,
                                        unsigned char* data,
                                        unsigned int length,
                                        sc_core::sc_time& delay,
                                        const AXIExtension* axi_ext = nullptr) {
        if (!enabled_) {
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        }
        
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        // 添加 AXI 擴展和 QoS 信息
        if (axi_ext) {
            AXIExtension ext = *axi_ext;
            ext.qos = qos_config_;
            ext.is_ptw = (port_type_ == OutputPortType::PTW_PORT);
            set_axi_extension(trans, ext);
        } else {
            AXIExtension ext;
            ext.qos = qos_config_;
            ext.is_ptw = (port_type_ == OutputPortType::PTW_PORT);
            set_axi_extension(trans, ext);
        }
        
        // 發送事務
        initiator_socket->b_transport(trans, delay);
        
        // 更新統計
        stats_.total_transactions++;
        stats_.write_transactions++;
        if (port_type_ == OutputPortType::PTW_PORT) {
            stats_.ptw_transactions++;
        }
        
        return trans.get_response_status();
    }
    
    // ========================================================================
    // 配置和控制
    // ========================================================================
    
    void set_qos_config(const QoSConfig& qos) {
        qos_config_ = qos;
    }
    
    const QoSConfig& get_qos_config() const {
        return qos_config_;
    }
    
    OutputPortType get_port_type() const {
        return port_type_;
    }
    
    bool is_ptw_port() const {
        return port_type_ == OutputPortType::PTW_PORT;
    }
    
    void enable() {
        enabled_ = true;
    }
    
    void disable() {
        enabled_ = false;
    }
    
    bool is_enabled() const {
        return enabled_;
    }
    
    // ========================================================================
    // 統計信息
    // ========================================================================
    
    const TLMStatistics& get_statistics() const {
        return stats_;
    }
    
    void reset_statistics() {
        stats_.reset();
    }
    
    // ========================================================================
    // 事務隊列管理（用於非阻塞模式）
    // ========================================================================
    
    void enqueue_transaction(tlm::tlm_generic_payload* trans) {
        transaction_queue_.push(trans);
        queue_event_.notify();
    }
    
    size_t get_queue_size() const {
        return transaction_queue_.size();
    }

private:
    // ========================================================================
    // 處理線程
    // 處理排隊的事務
    // ========================================================================
    
    void process_thread() {
        while (true) {
            // 等待事務到達
            if (transaction_queue_.empty()) {
                wait(queue_event_);
            }
            
            // 處理隊列中的事務
            while (!transaction_queue_.empty()) {
                tlm::tlm_generic_payload* trans = transaction_queue_.front();
                transaction_queue_.pop();
                
                if (trans && enabled_) {
                    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                    
                    // 根據 QoS 優先級添加延遲
                    if (qos_config_.priority < 15) {
                        delay += sc_core::sc_time(
                            (15 - qos_config_.priority) * 2, sc_core::SC_NS);
                    }
                    
                    // 發送事務
                    initiator_socket->b_transport(*trans, delay);
                    
                    // 等待延遲
                    wait(delay);
                }
                
                // 清理事務
                if (trans) {
                    delete trans;
                }
            }
        }
    }
    
    // ========================================================================
    // 私有成員
    // ========================================================================
    
    OutputPortType port_type_;            // 端口類型
    QoSConfig qos_config_;                // QoS 配置
    bool enabled_;                        // 是否啟用
    TLMStatistics stats_;                 // 統計信息
    
    // 事務隊列（用於非阻塞模式）
    std::queue<tlm::tlm_generic_payload*> transaction_queue_;
    sc_core::sc_event queue_event_;       // 隊列事件
};

} // namespace smmu_tlm

#endif // SMMU_TLM_INITIATOR_H
