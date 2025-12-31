// TLB（Translation Lookaside Buffer）實現文件
// 實現地址轉換緩存的所有功能

#include "tlb.h"
#include <algorithm>

namespace smmu {

// ============================================================================
// 構造函數
// ============================================================================

TLB::TLB(size_t capacity) 
    : capacity_(capacity), timestamp_counter_(0), 
      hit_count_(0), miss_count_(0) {}

// ============================================================================
// 獲取頁面基地址
// 通過頁面大小的掩碼清除偏移位，得到頁面起始地址
// ============================================================================

VirtualAddress TLB::get_page_base(VirtualAddress va, PageSize page_size) const {
    // 創建掩碼：頁面大小減1後取反，用於清除偏移位
    // 例如：4KB (0x1000) -> 掩碼 0xFFFFFFFFFFFFF000
    uint64_t mask = ~(static_cast<uint64_t>(page_size) - 1);
    return va & mask;
}

// ============================================================================
// TLB 查找操作
// 在 TLB 中查找給定虛擬地址的轉換結果
// ============================================================================

std::optional<TLBEntry> TLB::lookup(VirtualAddress va, StreamID stream_id,
                                     ASID asid, VMID vmid) {
    // 嘗試不同的頁面大小（從大到小）
    // 因為一個虛擬地址可能被不同大小的頁面映射
    for (auto page_size : {PageSize::SIZE_1GB, PageSize::SIZE_2MB, 
                           PageSize::SIZE_64KB, PageSize::SIZE_4KB}) {
        // 計算該頁面大小下的基地址
        VirtualAddress va_base = get_page_base(va, page_size);
        TLBKey key{va_base, stream_id, asid, vmid};
        
        // 在哈希表中查找
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            // 找到了！更新 LRU 列表（移到最前面）
            lru_list_.remove(key);
            lru_list_.push_front(key);
            
            hit_count_++;  // 增加命中計數
            return it->second;
        }
    }
    
    // 未找到，增加未命中計數
    miss_count_++;
    return std::nullopt;
}

// ============================================================================
// TLB 插入操作
// 將新的轉換結果插入 TLB
// ============================================================================

void TLB::insert(const TLBEntry& entry) {
    // 計算頁面基地址作為鍵
    VirtualAddress va_base = get_page_base(entry.va, entry.page_size);
    TLBKey key{va_base, entry.stream_id, entry.asid, entry.vmid};
    
    // 檢查表項是否已存在
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        // 表項已存在，更新它並移到 LRU 列表前面
        lru_list_.remove(key);
    } else if (entries_.size() >= capacity_) {
        // TLB 已滿，需要淘汰最舊的表項
        evict_lru();
    }
    
    // 插入新表項
    TLBEntry new_entry = entry;
    new_entry.timestamp = timestamp_counter_++;  // 分配新時間戳
    entries_[key] = new_entry;
    lru_list_.push_front(key);  // 添加到 LRU 列表前面
}

// ============================================================================
// 淘汰 LRU 表項
// 移除最近最少使用的表項（LRU 列表末尾）
// ============================================================================

void TLB::evict_lru() {
    if (lru_list_.empty()) return;
    
    // 獲取 LRU 列表末尾的鍵（最舊的表項）
    TLBKey lru_key = lru_list_.back();
    lru_list_.pop_back();
    entries_.erase(lru_key);  // 從哈希表中刪除
}

// ============================================================================
// TLB 無效化操作
// 根據不同條件使 TLB 表項無效
// ============================================================================

// 使所有 TLB 表項無效
void TLB::invalidate_all() {
    entries_.clear();
    lru_list_.clear();
}

// 按 ASID 使 TLB 表項無效
// 用於地址空間切換時清除舊地址空間的緩存
void TLB::invalidate_by_asid(ASID asid) {
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (it->second.asid == asid) {
            lru_list_.remove(it->first);  // 從 LRU 列表移除
            it = entries_.erase(it);      // 從哈希表移除
        } else {
            ++it;
        }
    }
}

// 按 VMID 使 TLB 表項無效
// 用於虛擬機切換時清除舊虛擬機的緩存
void TLB::invalidate_by_vmid(VMID vmid) {
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (it->second.vmid == vmid) {
            lru_list_.remove(it->first);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

// 按虛擬地址使 TLB 表項無效
// 用於頁表更新後清除特定地址的緩存
void TLB::invalidate_by_va(VirtualAddress va, ASID asid) {
    // 嘗試所有頁面大小，因為不知道該地址使用哪種頁面大小
    for (auto page_size : {PageSize::SIZE_1GB, PageSize::SIZE_2MB,
                           PageSize::SIZE_64KB, PageSize::SIZE_4KB}) {
        VirtualAddress va_base = get_page_base(va, page_size);
        
        auto it = entries_.begin();
        while (it != entries_.end()) {
            // 檢查 ASID 和頁面基地址是否匹配
            if (it->second.asid == asid && 
                get_page_base(it->second.va, it->second.page_size) == va_base) {
                lru_list_.remove(it->first);
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// 按流ID使 TLB 表項無效
// 用於設備配置更改時清除該設備的所有緩存
void TLB::invalidate_by_stream(StreamID stream_id) {
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (it->second.stream_id == stream_id) {
            lru_list_.remove(it->first);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace smmu
