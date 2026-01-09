// 頁表（Page Table）頭文件
// 實現多級頁表遍歷和地址轉換功能

#ifndef SMMU_PAGE_TABLE_H
#define SMMU_PAGE_TABLE_H

#include "smmu_types.h"
#include <memory>
#include <vector>
#include <functional>

namespace smmu {

// ============================================================================
// 頁表描述符結構
// 表示頁表中的一個表項，可以是表描述符或塊/頁描述符
// ============================================================================

struct PageTableDescriptor {
    bool valid;                        // 描述符是否有效
    bool is_table;                     // true = 表描述符, false = 塊/頁描述符
    PhysicalAddress address;           // 輸出地址（下一級表地址或物理地址）
    AccessPermission ap;               // 訪問權限
    MemoryType mem_attr;               // 內存屬性
    bool shareable;                    // 是否可共享
    bool access_flag;                  // 訪問標誌（硬件設置）
    bool dirty;                        // 髒位（頁面已被修改）
    bool contiguous;                   // 連續位（用於TLB優化）
    bool privileged_execute_never;     // 特權執行永不（PXN）
    bool execute_never;                // 執行永不（XN）
    
    // 默認構造函數：初始化為無效狀態
    PageTableDescriptor() 
        : valid(false), is_table(false), address(0),
          ap(AccessPermission::NONE),
          mem_attr(MemoryType::NORMAL_WB),
          shareable(false), access_flag(false), dirty(false),
          contiguous(false), privileged_execute_never(false),
          execute_never(false) {}
};

// ============================================================================
// 內存讀取回調函數類型
// 用於從物理內存讀取數據（頁表項）
// ============================================================================

using MemoryReadCallback = std::function<bool(PhysicalAddress addr, 
                                               uint64_t& data, 
                                               size_t size)>;

// ============================================================================
// 頁表遍歷器類
// 實現多級頁表的遍歷和地址轉換
// ============================================================================

class PageTableWalker {
public:
    // 構造函數：需要提供內存讀取回調函數
    PageTableWalker(MemoryReadCallback memory_read);
    
    // 執行地址轉換
    // va: 虛擬地址
    // ttb: 轉換表基地址（Translation Table Base）
    // granule_size: 頁面粒度大小（12=4KB, 14=16KB, 16=64KB）
    // ips_bits: 中間物理地址大小（位數）
    // stage: 轉換階段
    TranslationResult translate(VirtualAddress va,
                                PhysicalAddress ttb,
                                uint8_t granule_size,
                                uint8_t ips_bits,
                                TranslationStage stage);
    
    // 從內存中解析描述符
    // desc: 64位描述符值
    // level: 頁表級別（0-3）
    // granule_size: 頁面粒度大小
    PageTableDescriptor parse_descriptor(uint64_t desc, 
                                         uint8_t level,
                                         uint8_t granule_size);
    
    // 獲取指定級別的頁面大小
    PageSize get_page_size(uint8_t level, uint8_t granule_size) const;
    
private:
    // ========================================================================
    // 遍歷上下文結構
    // 存儲頁表遍歷過程中需要的所有信息
    // ========================================================================
    
    struct WalkContext {
        VirtualAddress va;           // 要轉換的虛擬地址
        PhysicalAddress ttb;         // 轉換表基地址
        uint8_t granule_size;        // 頁面粒度大小
        uint8_t ips_bits;            // 中間物理地址大小
        uint8_t start_level;         // 起始級別
        uint8_t max_level;           // 最大級別
        TranslationStage stage;      // 轉換階段
    };
    
    // 執行頁表遍歷
    TranslationResult walk_table(const WalkContext& ctx);
    
    // 從虛擬地址中提取指定級別的索引位
    uint64_t get_index_bits(VirtualAddress va, uint8_t level, 
                           uint8_t granule_size) const;
    
    // 計算描述符在頁表中的地址
    uint64_t get_descriptor_address(PhysicalAddress table_base,
                                   uint64_t index,
                                   uint8_t granule_size) const;
    
    // 從物理內存讀取描述符
    bool read_descriptor(PhysicalAddress addr, uint64_t& desc);
    
    // 內存讀取回調函數
    MemoryReadCallback memory_read_;
};

// ============================================================================
// 簡單內存模型類
// 用於測試的簡化物理內存模擬器
// ============================================================================

class SimpleMemoryModel {
public:
    SimpleMemoryModel();
    
    // 向物理內存寫入數據
    void write(PhysicalAddress addr, const void* data, size_t size);
    
    // 從物理內存讀取數據
    bool read(PhysicalAddress addr, void* data, size_t size);
    
    // 寫入頁表項（Page Table Entry）
    void write_pte(PhysicalAddress addr, uint64_t pte);
    
    // 分配物理頁面
    // size: 頁面大小（默認4096字節）
    // 返回：分配的物理地址
    PhysicalAddress allocate_page(size_t size = 4096);
    
private:
    std::vector<uint8_t> memory_;           // 內存數組
    PhysicalAddress next_alloc_;            // 下一個分配地址
    static constexpr size_t MEMORY_SIZE = 256 * 1024 * 1024; // 256MB
};

} // namespace smmu

#endif // SMMU_PAGE_TABLE_H
