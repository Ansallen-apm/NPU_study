// SMMU 寄存器接口頭文件
// 實現 SMMUv3 兼容的內存映射寄存器接口

#ifndef SMMU_REGISTERS_H
#define SMMU_REGISTERS_H

#include <cstdint>
#include <unordered_map>

namespace smmu {

// ============================================================================
// SMMU 寄存器偏移量（簡化子集）
// 這些偏移量與 SMMUv3 規範兼容
// ============================================================================

enum class RegisterOffset : uint32_t {
    // 識別寄存器（只讀）
    IDR0 = 0x0000,  // 識別寄存器0：功能支持
    IDR1 = 0x0004,  // 識別寄存器1：隊列大小等
    IDR5 = 0x0014,  // 識別寄存器5：輸出地址大小
    
    // 控制寄存器
    CR0 = 0x0020,      // 控制寄存器0：主要控制位
    CR0ACK = 0x0024,   // CR0 確認寄存器
    CR1 = 0x0028,      // 控制寄存器1：隊列控制
    CR2 = 0x002C,      // 控制寄存器2：其他控制
    
    // 狀態寄存器
    STATUSR = 0x0040,  // 狀態寄存器
    GBPA = 0x0044,     // 全局旁路屬性
    
    // 中斷寄存器
    IRQ_CTRL = 0x0050,     // 中斷控制
    IRQ_CTRLACK = 0x0054,  // 中斷控制確認
    
    // 命令隊列寄存器
    CMDQ_BASE = 0x0090,  // 命令隊列基地址
    CMDQ_PROD = 0x0098,  // 命令隊列生產者索引
    CMDQ_CONS = 0x009C,  // 命令隊列消費者索引
    
    // 事件隊列寄存器
    EVENTQ_BASE = 0x00A0,  // 事件隊列基地址
    EVENTQ_PROD = 0x00A8,  // 事件隊列生產者索引
    EVENTQ_CONS = 0x00AC,  // 事件隊列消費者索引
    
    // 流表寄存器
    STRTAB_BASE = 0x0080,      // 流表基地址
    STRTAB_BASE_CFG = 0x0088   // 流表配置
};

// ============================================================================
// CR0（控制寄存器0）位定義
// ============================================================================

namespace CR0 {
    constexpr uint32_t SMMUEN = (1U << 0);    // SMMU 啟用位
    constexpr uint32_t EVENTQEN = (1U << 1);  // 事件隊列啟用
    constexpr uint32_t CMDQEN = (1U << 2);    // 命令隊列啟用
    constexpr uint32_t ATSCHK = (1U << 4);    // ATS 檢查啟用
}

// ============================================================================
// CR1（控制寄存器1）位定義
// ============================================================================

namespace CR1 {
    constexpr uint32_t QUEUE_IC = (1U << 0);   // 隊列完成中斷
    constexpr uint32_t QUEUE_OC = (1U << 1);   // 隊列溢出檢查
    constexpr uint32_t TABLE_SH = (3U << 10);  // 表共享性
}

// ============================================================================
// IDR0（識別寄存器0）位定義
// 指示 SMMU 支持的功能
// ============================================================================

namespace IDR0 {
    constexpr uint32_t S1P = (1U << 1);          // 階段1支持
    constexpr uint32_t S2P = (1U << 2);          // 階段2支持
    constexpr uint32_t TTF_AARCH64 = (2U << 4);  // AArch64 轉換表格式
    constexpr uint32_t COHACC = (1U << 6);       // 一致性訪問支持
    constexpr uint32_t ASID16 = (1U << 12);      // 16位 ASID 支持
    constexpr uint32_t VMID16 = (1U << 18);      // 16位 VMID 支持
}

// ============================================================================
// 寄存器接口類
// 提供對 SMMU 寄存器的讀寫訪問
// ============================================================================

class RegisterInterface {
public:
    // 構造函數：初始化寄存器接口
    RegisterInterface();
    
    // ========================================================================
    // 基本讀寫操作
    // ========================================================================
    
    // 讀取32位寄存器
    uint32_t read_register(RegisterOffset offset) const;
    
    // 寫入32位寄存器
    void write_register(RegisterOffset offset, uint32_t value);
    
    // 讀取64位寄存器（讀取兩個連續的32位寄存器）
    uint64_t read_register_64(RegisterOffset offset) const;
    
    // 寫入64位寄存器（寫入兩個連續的32位寄存器）
    void write_register_64(RegisterOffset offset, uint64_t value);
    
    // ========================================================================
    // 特定寄存器的輔助方法
    // 提供更方便的訪問接口
    // ========================================================================
    
    // SMMU 啟用狀態
    bool is_smmu_enabled() const;
    void set_smmu_enabled(bool enabled);
    
    // 命令隊列啟用狀態
    bool is_cmdq_enabled() const;
    void set_cmdq_enabled(bool enabled);
    
    // 事件隊列啟用狀態
    bool is_eventq_enabled() const;
    void set_eventq_enabled(bool enabled);
    
    // 流表基地址
    uint64_t get_stream_table_base() const;
    void set_stream_table_base(uint64_t base);
    
    // 命令隊列基地址
    uint64_t get_cmdq_base() const;
    void set_cmdq_base(uint64_t base);
    
    // 事件隊列基地址
    uint64_t get_eventq_base() const;
    void set_eventq_base(uint64_t base);
    
    // 命令隊列生產者索引
    uint32_t get_cmdq_prod() const;
    void set_cmdq_prod(uint32_t prod);
    
    // 命令隊列消費者索引
    uint32_t get_cmdq_cons() const;
    void set_cmdq_cons(uint32_t cons);
    
    // 事件隊列生產者索引
    uint32_t get_eventq_prod() const;
    void set_eventq_prod(uint32_t prod);
    
    // 事件隊列消費者索引
    uint32_t get_eventq_cons() const;
    void set_eventq_cons(uint32_t cons);
    
    // 初始化識別寄存器（只讀寄存器）
    void init_idr_registers();
    
private:
    // 寄存器存儲（使用哈希表模擬內存映射寄存器）
    std::unordered_map<uint32_t, uint32_t> registers_;
    
    // 將寄存器偏移量轉換為uint32_t
    uint32_t get_offset_value(RegisterOffset offset) const {
        return static_cast<uint32_t>(offset);
    }
};

} // namespace smmu

#endif // SMMU_REGISTERS_H
