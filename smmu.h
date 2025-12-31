// SMMU（System Memory Management Unit）主控制器頭文件
// 實現完整的 SMMU 功能，包括地址轉換、命令處理、事件生成等

#ifndef SMMU_H
#define SMMU_H

#include "smmu_types.h"
#include "tlb.h"
#include "page_table.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <queue>
#include <cstring>

namespace smmu {

// ============================================================================
// 命令隊列表項
// 用於配置和控制 SMMU 的命令
// ============================================================================

struct Command {
    CommandType type;  // 命令類型
    
    // 命令數據（使用聯合體節省空間）
    union {
        // 使流表項緩存無效
        struct {
            StreamID stream_id;
        } cfgi_ste;
        
        // 使上下文描述符緩存無效
        struct {
            StreamID stream_id;
            ASID asid;
        } cfgi_cd;
        
        // 按 ASID 使 TLB 無效
        struct {
            ASID asid;
        } tlbi_asid;
        
        // 按虛擬地址使 TLB 無效
        struct {
            VirtualAddress va;
            ASID asid;
        } tlbi_va;
        
        // 按 VMID 使所有 TLB 無效
        struct {
            VMID vmid;
        } tlbi_vmall;
    } data;
    
    // 默認構造函數：初始化為同步命令
    Command() : type(CommandType::CMD_SYNC) {
        std::memset(&data, 0, sizeof(data));
    }
};

// ============================================================================
// 事件隊列表項
// 記錄 SMMU 產生的錯誤和事件
// ============================================================================

struct Event {
    FaultType fault_type;        // 錯誤類型
    StreamID stream_id;          // 相關的流ID
    ASID asid;                   // 相關的地址空間ID
    VMID vmid;                   // 相關的虛擬機ID
    VirtualAddress va;           // 相關的虛擬地址
    std::string description;     // 錯誤描述
    uint64_t timestamp;          // 時間戳
    
    // 默認構造函數
    Event() : fault_type(FaultType::NONE), stream_id(0), 
              asid(0), vmid(0), va(0), timestamp(0) {}
};

// ============================================================================
// SMMU 配置結構
// 定義 SMMU 的各種參數
// ============================================================================

struct SMMUConfig {
    size_t tlb_size;              // TLB 大小（表項數量）
    size_t stream_table_size;     // 流表大小
    size_t command_queue_size;    // 命令隊列深度
    size_t event_queue_size;      // 事件隊列深度
    bool stage1_enabled;          // 是否啟用階段1轉換
    bool stage2_enabled;          // 是否啟用階段2轉換
    
    // 默認配置
    SMMUConfig() 
        : tlb_size(128), stream_table_size(256),
          command_queue_size(64), event_queue_size(64),
          stage1_enabled(true), stage2_enabled(false) {}
};

// ============================================================================
// SMMU 主類
// 協調所有組件，提供完整的 SMMU 功能
// ============================================================================

class SMMU {
public:
    // 構造函數：使用指定配置創建 SMMU
    explicit SMMU(const SMMUConfig& config = SMMUConfig());
    
    // ========================================================================
    // 初始化和配置
    // ========================================================================
    
    // 設置內存模型（用於訪問頁表）
    void set_memory_model(std::shared_ptr<SimpleMemoryModel> memory);
    
    // ========================================================================
    // 流表配置
    // ========================================================================
    
    // 配置流表項（每個設備一個表項）
    void configure_stream_table_entry(StreamID stream_id, 
                                     const StreamTableEntry& ste);
    
    // 獲取流表項
    StreamTableEntry get_stream_table_entry(StreamID stream_id) const;
    
    // ========================================================================
    // 上下文描述符配置
    // ========================================================================
    
    // 配置上下文描述符（定義地址空間）
    void configure_context_descriptor(StreamID stream_id, 
                                     ASID asid,
                                     const ContextDescriptor& cd);
    
    // 獲取上下文描述符
    ContextDescriptor get_context_descriptor(StreamID stream_id, ASID asid) const;
    
    // ========================================================================
    // 地址轉換
    // ========================================================================
    
    // 執行地址轉換（主要接口）
    // va: 虛擬地址
    // stream_id: 流ID（設備標識）
    // asid: 地址空間ID（默認0）
    // vmid: 虛擬機ID（默認0）
    // 返回：轉換結果（包含物理地址和屬性）
    TranslationResult translate(VirtualAddress va, 
                               StreamID stream_id,
                               ASID asid = 0,
                               VMID vmid = 0);
    
    // ========================================================================
    // 命令隊列操作
    // ========================================================================
    
    // 提交命令到命令隊列
    void submit_command(const Command& cmd);
    
    // 處理所有待處理的命令
    void process_commands();
    
    // ========================================================================
    // 事件隊列操作
    // ========================================================================
    
    // 檢查是否有待處理的事件
    bool has_events() const;
    
    // 彈出並返回下一個事件
    Event pop_event();
    
    // ========================================================================
    // TLB 管理
    // ========================================================================
    
    void invalidate_tlb_all();                              // 使所有 TLB 項無效
    void invalidate_tlb_by_asid(ASID asid);                 // 按 ASID 無效化
    void invalidate_tlb_by_vmid(VMID vmid);                 // 按 VMID 無效化
    void invalidate_tlb_by_va(VirtualAddress va, ASID asid); // 按虛擬地址無效化
    void invalidate_tlb_by_stream(StreamID stream_id);      // 按流ID無效化
    
    // ========================================================================
    // 統計信息
    // ========================================================================
    
    struct Statistics {
        uint64_t total_translations;    // 總轉換次數
        uint64_t tlb_hits;              // TLB 命中次數
        uint64_t tlb_misses;            // TLB 未命中次數
        uint64_t page_table_walks;      // 頁表遍歷次數
        uint64_t translation_faults;    // 轉換錯誤次數
        uint64_t permission_faults;     // 權限錯誤次數
        uint64_t commands_processed;    // 已處理命令數
        uint64_t events_generated;      // 已生成事件數
    };
    
    // 獲取統計信息
    Statistics get_statistics() const { return stats_; }
    
    // 重置統計信息
    void reset_statistics();
    
    // ========================================================================
    // 啟用/禁用控制
    // ========================================================================
    
    void enable();                      // 啟用 SMMU
    void disable();                     // 禁用 SMMU
    bool is_enabled() const { return enabled_; }  // 檢查是否已啟用
    
private:
    // ========================================================================
    // 內部轉換函數
    // ========================================================================
    
    // 階段1轉換（虛擬地址 -> 中間物理地址）
    TranslationResult translate_stage1(VirtualAddress va,
                                      const StreamTableEntry& ste,
                                      const ContextDescriptor& cd);
    
    // 階段2轉換（中間物理地址 -> 物理地址）
    TranslationResult translate_stage2(PhysicalAddress ipa,
                                      const StreamTableEntry& ste);
    
    // ========================================================================
    // 事件生成
    // ========================================================================
    
    // 生成並添加事件到事件隊列
    void generate_event(FaultType fault_type, StreamID stream_id,
                       ASID asid, VMID vmid, VirtualAddress va,
                       const std::string& description);
    
    // ========================================================================
    // 命令處理
    // ========================================================================
    
    // 處理單個命令
    void process_command(const Command& cmd);
    
    // ========================================================================
    // 私有成員變量
    // ========================================================================
    
    SMMUConfig config_;  // SMMU 配置
    bool enabled_;       // 是否已啟用
    
    // 核心組件
    std::unique_ptr<TLB> tlb_;                              // TLB 緩存
    std::unique_ptr<PageTableWalker> page_table_walker_;    // 頁表遍歷器
    std::shared_ptr<SimpleMemoryModel> memory_;             // 內存模型
    
    // 配置表
    std::unordered_map<StreamID, StreamTableEntry> stream_table_;        // 流表
    std::unordered_map<uint64_t, ContextDescriptor> context_descriptors_; // 上下文描述符表
    
    // 命令和事件隊列
    std::queue<Command> command_queue_;  // 命令隊列
    std::queue<Event> event_queue_;      // 事件隊列
    
    // 統計信息
    Statistics stats_;           // 性能統計
    uint64_t timestamp_counter_; // 時間戳計數器
    
    // ========================================================================
    // 輔助函數
    // ========================================================================
    
    // 創建上下文描述符鍵（用於哈希表）
    // 將 stream_id 和 asid 組合成唯一的64位鍵
    uint64_t make_cd_key(StreamID stream_id, ASID asid) const {
        return (static_cast<uint64_t>(stream_id) << 16) | asid;
    }
};

} // namespace smmu

#endif // SMMU_H
