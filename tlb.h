// TLB（Translation Lookaside Buffer）頭文件
// 實現地址轉換緩存，使用 LRU（最近最少使用）淘汰策略

#ifndef SMMU_TLB_H
#define SMMU_TLB_H

#include "smmu_types.h"
#include <unordered_map>
#include <list>
#include <optional>

namespace smmu {

// ============================================================================
// TLB 表項結構
// 存儲單個地址轉換的緩存信息
// ============================================================================

struct TLBEntry {
    VirtualAddress va;              // 虛擬地址
    PhysicalAddress pa;             // 物理地址
    StreamID stream_id;             // 流ID（設備標識）
    ASID asid;                      // 地址空間ID
    VMID vmid;                      // 虛擬機ID
    PageSize page_size;             // 頁面大小
    MemoryType memory_type;         // 內存類型
    AccessPermission permission;    // 訪問權限
    bool cacheable;                 // 是否可緩存
    bool shareable;                 // 是否可共享
    TranslationStage stage;         // 轉換階段
    uint64_t timestamp;             // 時間戳（用於LRU）
    
    // 默認構造函數：初始化所有字段
    TLBEntry() : va(0), pa(0), stream_id(0), asid(0), vmid(0),
                 page_size(PageSize::SIZE_4KB),
                 memory_type(MemoryType::NORMAL_WB),
                 permission(AccessPermission::NONE),
                 cacheable(true), shareable(false),
                 stage(TranslationStage::STAGE1),
                 timestamp(0) {}
};

// ============================================================================
// TLB 類
// 實現帶 LRU 淘汰策略的轉換查找緩衝區
// ============================================================================

class TLB {
public:
    // 構造函數：創建指定容量的 TLB
    // capacity: TLB 可以存儲的最大表項數量（默認128）
    explicit TLB(size_t capacity = 128);
    
    // 在 TLB 中查找地址轉換
    // 返回：如果找到則返回 TLBEntry，否則返回空
    std::optional<TLBEntry> lookup(VirtualAddress va, StreamID stream_id, 
                                    ASID asid, VMID vmid);
    
    // 插入新的 TLB 表項
    // 如果 TLB 已滿，會使用 LRU 策略淘汰最舊的表項
    void insert(const TLBEntry& entry);
    
    // ========================================================================
    // TLB 無效化操作
    // ========================================================================
    
    void invalidate_all();                              // 使所有表項無效
    void invalidate_by_asid(ASID asid);                 // 按 ASID 使表項無效
    void invalidate_by_vmid(VMID vmid);                 // 按 VMID 使表項無效
    void invalidate_by_va(VirtualAddress va, ASID asid); // 按虛擬地址使表項無效
    void invalidate_by_stream(StreamID stream_id);      // 按流ID使表項無效
    
    // ========================================================================
    // 統計信息查詢
    // ========================================================================
    
    size_t size() const { return entries_.size(); }      // 當前表項數量
    size_t capacity() const { return capacity_; }        // TLB 容量
    uint64_t hit_count() const { return hit_count_; }    // 命中次數
    uint64_t miss_count() const { return miss_count_; }  // 未命中次數
    
private:
    // ========================================================================
    // TLB 鍵結構
    // 用於在哈希表中唯一標識一個 TLB 表項
    // ========================================================================
    
    struct TLBKey {
        VirtualAddress va_base;  // 頁面基地址
        StreamID stream_id;      // 流ID
        ASID asid;               // 地址空間ID
        VMID vmid;               // 虛擬機ID
        
        // 相等比較運算符（用於哈希表查找）
        bool operator==(const TLBKey& other) const {
            return va_base == other.va_base && 
                   stream_id == other.stream_id &&
                   asid == other.asid && 
                   vmid == other.vmid;
        }
    };
    
    // ========================================================================
    // TLB 鍵哈希函數
    // 為 TLBKey 生成哈希值
    // ========================================================================
    
    struct TLBKeyHash {
        size_t operator()(const TLBKey& key) const {
            // 組合多個字段的哈希值
            return std::hash<uint64_t>()(key.va_base) ^
                   (std::hash<uint32_t>()(key.stream_id) << 1) ^
                   (std::hash<uint16_t>()(key.asid) << 2) ^
                   (std::hash<uint16_t>()(key.vmid) << 3);
        }
    };
    
    // 獲取虛擬地址的頁面基地址
    VirtualAddress get_page_base(VirtualAddress va, PageSize page_size) const;
    
    // 淘汰最近最少使用的表項
    void evict_lru();
    
    // ========================================================================
    // 私有成員變量
    // ========================================================================
    
    size_t capacity_;              // TLB 容量
    uint64_t timestamp_counter_;   // 時間戳計數器
    uint64_t hit_count_;           // 命中計數
    uint64_t miss_count_;          // 未命中計數
    
    // 使用哈希表存儲 TLB 表項（快速查找）
    std::unordered_map<TLBKey, TLBEntry, TLBKeyHash> entries_;
    
    // 使用鏈表維護 LRU 順序（最近使用的在前面）
    std::list<TLBKey> lru_list_;
};

} // namespace smmu

#endif // SMMU_TLB_H
