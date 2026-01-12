// SMMU 類型定義頭文件
// 定義了 SMMU 功能模型中使用的所有基本類型、枚舉和數據結構

#ifndef SMMU_TYPES_H
#define SMMU_TYPES_H

#include <cstdint>
#include <string>

namespace smmu {

// ============================================================================
// 地址類型定義
// ============================================================================

using PhysicalAddress = uint64_t;  // 物理地址類型（64位）
using VirtualAddress = uint64_t;   // 虛擬地址類型（64位）
using StreamID = uint32_t;         // 流ID，用於識別不同的設備
using SubstreamID = uint32_t;      // 子流ID
using ASID = uint16_t;             // 地址空間ID（Address Space ID）
using VMID = uint16_t;             // 虛擬機ID（Virtual Machine ID）

// ============================================================================
// 頁面大小枚舉
// ============================================================================

enum class PageSize : uint64_t {
    SIZE_4KB = 0x1000,        // 4KB 頁面（4096 字節）
    SIZE_16KB = 0x4000,       // 16KB 頁面（16384 字節）
    SIZE_64KB = 0x10000,      // 64KB 頁面（65536 字節）
    SIZE_2MB = 0x200000,      // 2MB 大頁面
    SIZE_32MB = 0x2000000,    // 32MB 大頁面
    SIZE_512MB = 0x20000000,  // 512MB 大頁面
    SIZE_1GB = 0x40000000     // 1GB 大頁面
};

// ============================================================================
// 地址轉換階段
// ============================================================================

enum class TranslationStage {
    STAGE1,              // 階段1轉換（虛擬地址 -> 中間物理地址）
    STAGE2,              // 階段2轉換（中間物理地址 -> 物理地址）
    STAGE1_AND_STAGE2    // 兩階段轉換
};

// ============================================================================
// 內存屬性類型
// ============================================================================

enum class MemoryType {
    DEVICE_nGnRnE,  // 設備內存：不可聚合、不可重排序、無提前寫確認
    DEVICE_nGnRE,   // 設備內存：不可聚合、不可重排序、有提前寫確認
    DEVICE_nGRE,    // 設備內存：不可聚合、可重排序、有提前寫確認
    DEVICE_GRE,     // 設備內存：可聚合、可重排序、有提前寫確認
    NORMAL_NC,      // 普通內存：不可緩存
    NORMAL_WT,      // 普通內存：寫穿透（Write-Through）
    NORMAL_WB       // 普通內存：寫回（Write-Back）
};

// ============================================================================
// 訪問權限
// ============================================================================

enum class AccessPermission {
    NONE,        // 無訪問權限
    READ_ONLY,   // 只讀
    WRITE_ONLY,  // 只寫
    READ_WRITE   // 讀寫
};

// ============================================================================
// 地址轉換結果結構
// ============================================================================

struct TranslationResult {
    bool success;                    // 轉換是否成功
    PhysicalAddress physical_addr;   // 轉換後的物理地址
    MemoryType memory_type;          // 內存類型
    AccessPermission permission;     // 訪問權限
    bool cacheable;                  // 是否可緩存
    bool shareable;                  // 是否可共享
    std::string fault_reason;        // 失敗原因（如果轉換失敗）
    
    // 默認構造函數：初始化為失敗狀態
    TranslationResult() 
        : success(false), physical_addr(0), 
          memory_type(MemoryType::NORMAL_WB),
          permission(AccessPermission::NONE),
          cacheable(true), shareable(false) {}
};

// ============================================================================
// 錯誤類型
// ============================================================================

enum class FaultType {
    NONE,                                // 無錯誤
    TRANSLATION_FAULT,                   // 轉換錯誤（頁表項無效）
    PERMISSION_FAULT,                    // 權限錯誤
    ACCESS_FAULT,                        // 訪問錯誤
    ADDRESS_SIZE_FAULT,                  // 地址大小錯誤
    TLB_CONFLICT_FAULT,                  // TLB 衝突錯誤
    UNSUPPORTED_UPSTREAM_TRANSACTION     // 不支持的上游事務
};

// ============================================================================
// 命令類型（用於命令隊列）
// ============================================================================

enum class CommandType {
    CMD_SYNC,              // 同步命令
    CMD_PREFETCH_CONFIG,   // 預取配置
    CMD_PREFETCH_ADDR,     // 預取地址
    CMD_CFGI_STE,          // 使流表項緩存無效
    CMD_CFGI_CD,           // 使上下文描述符緩存無效
    CMD_CFGI_ALL,          // 使所有配置緩存無效
    CMD_TLBI_NH_ALL,       // 使所有 TLB 項無效
    CMD_TLBI_NH_ASID,      // 按 ASID 使 TLB 項無效
    CMD_TLBI_NH_VA,        // 按虛擬地址使 TLB 項無效
    CMD_TLBI_S12_VMALL     // 按 VMID 使所有 TLB 項無效
};

// ============================================================================
// 流表項（Stream Table Entry）
// 每個設備（流）的配置信息
// ============================================================================

struct StreamTableEntry {
    bool valid;                                  // 表項是否有效
    bool s1_enabled;                             // 是否啟用階段1轉換
    bool s2_enabled;                             // 是否啟用階段2轉換
    PhysicalAddress s1_context_ptr;              // 階段1上下文描述符指針
    PhysicalAddress s2_translation_table_base;   // 階段2轉換表基地址
    VMID vmid;                                   // 虛擬機ID
    uint8_t s1_format;                           // 階段1格式
    uint8_t s2_granule;                          // 階段2頁面粒度
    
    // 默認構造函數：初始化為無效狀態
    StreamTableEntry() 
        : valid(false), s1_enabled(false), s2_enabled(false),
          s1_context_ptr(0), s2_translation_table_base(0),
          vmid(0), s1_format(0), s2_granule(0) {}
};

// ============================================================================
// 上下文描述符（Context Descriptor）
// 定義地址空間的轉換配置
// ============================================================================

struct ContextDescriptor {
    bool valid;                              // 描述符是否有效
    PhysicalAddress translation_table_base;  // 轉換表基地址
    ASID asid;                               // 地址空間ID
    uint8_t translation_granule;             // 轉換粒度（12=4KB, 14=16KB, 16=64KB）
    uint8_t ips;                             // 中間物理地址大小
    uint8_t tg;                              // 轉換粒度
    uint8_t sh;                              // 共享性屬性
    uint8_t orgn;                            // 外部緩存屬性
    uint8_t irgn;                            // 內部緩存屬性
    
    // 默認構造函數：初始化為無效狀態
    ContextDescriptor()
        : valid(false), translation_table_base(0), asid(0),
          translation_granule(0), ips(0), tg(0), sh(0), orgn(0), irgn(0) {}
};

} // namespace smmu

#endif // SMMU_TYPES_H
