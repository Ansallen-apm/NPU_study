// SMMU 寄存器接口實現文件
// 實現寄存器的讀寫和管理功能

#include "smmu_registers.h"

namespace smmu {

// ============================================================================
// 構造函數和初始化
// ============================================================================

// 構造函數：初始化識別寄存器
RegisterInterface::RegisterInterface() {
    init_idr_registers();
}

// 初始化識別寄存器
// 這些寄存器是只讀的，描述 SMMU 的功能和配置
void RegisterInterface::init_idr_registers() {
    // IDR0 - 識別寄存器0
    // 設置支持的功能位
    uint32_t idr0 = IDR0::S1P |          // 支持階段1轉換
                    IDR0::S2P |          // 支持階段2轉換
                    IDR0::TTF_AARCH64 |  // 支持 AArch64 轉換表格式
                    IDR0::COHACC |       // 支持一致性訪問
                    IDR0::ASID16 |       // 支持16位 ASID
                    IDR0::VMID16;        // 支持16位 VMID
    registers_[get_offset_value(RegisterOffset::IDR0)] = idr0;
    
    // IDR1 - 識別寄存器1
    // 在這個簡化實現中設置為0
    registers_[get_offset_value(RegisterOffset::IDR1)] = 0x00000000;
    
    // IDR5 - 識別寄存器5
    // 在這個簡化實現中設置為0
    registers_[get_offset_value(RegisterOffset::IDR5)] = 0x00000000;
}

// ============================================================================
// 基本讀寫操作
// ============================================================================

// 讀取32位寄存器
uint32_t RegisterInterface::read_register(RegisterOffset offset) const {
    uint32_t offset_val = get_offset_value(offset);
    auto it = registers_.find(offset_val);
    if (it != registers_.end()) {
        return it->second;
    }
    return 0;  // 未初始化的寄存器返回0
}

// 寫入32位寄存器
void RegisterInterface::write_register(RegisterOffset offset, uint32_t value) {
    uint32_t offset_val = get_offset_value(offset);
    
    // 處理只讀寄存器
    if (offset == RegisterOffset::IDR0 || 
        offset == RegisterOffset::IDR1 ||
        offset == RegisterOffset::IDR5) {
        return; // 忽略對只讀寄存器的寫入
    }
    
    // 處理特殊寄存器
    if (offset == RegisterOffset::CR0) {
        // CR0 寫入後需要在 CR0ACK 中確認
        registers_[offset_val] = value;
        registers_[get_offset_value(RegisterOffset::CR0ACK)] = value;
    } else if (offset == RegisterOffset::IRQ_CTRL) {
        // IRQ_CTRL 寫入後需要在 IRQ_CTRLACK 中確認
        registers_[offset_val] = value;
        registers_[get_offset_value(RegisterOffset::IRQ_CTRLACK)] = value;
    } else {
        // 普通寄存器直接寫入
        registers_[offset_val] = value;
    }
}

// 讀取64位寄存器
// 通過讀取兩個連續的32位寄存器實現
uint64_t RegisterInterface::read_register_64(RegisterOffset offset) const {
    uint32_t low = read_register(offset);  // 低32位
    uint32_t high = read_register(static_cast<RegisterOffset>(
        get_offset_value(offset) + 4));     // 高32位（偏移+4）
    return (static_cast<uint64_t>(high) << 32) | low;
}

// 寫入64位寄存器
// 通過寫入兩個連續的32位寄存器實現
void RegisterInterface::write_register_64(RegisterOffset offset, uint64_t value) {
    write_register(offset, static_cast<uint32_t>(value));  // 低32位
    write_register(static_cast<RegisterOffset>(get_offset_value(offset) + 4),
                  static_cast<uint32_t>(value >> 32));     // 高32位
}

// ============================================================================
// 控制寄存器輔助方法
// ============================================================================

// 檢查 SMMU 是否已啟用
bool RegisterInterface::is_smmu_enabled() const {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    return (cr0 & CR0::SMMUEN) != 0;
}

// 設置 SMMU 啟用狀態
void RegisterInterface::set_smmu_enabled(bool enabled) {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    if (enabled) {
        cr0 |= CR0::SMMUEN;   // 設置啟用位
    } else {
        cr0 &= ~CR0::SMMUEN;  // 清除啟用位
    }
    write_register(RegisterOffset::CR0, cr0);
}

// 檢查命令隊列是否已啟用
bool RegisterInterface::is_cmdq_enabled() const {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    return (cr0 & CR0::CMDQEN) != 0;
}

// 設置命令隊列啟用狀態
void RegisterInterface::set_cmdq_enabled(bool enabled) {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    if (enabled) {
        cr0 |= CR0::CMDQEN;   // 設置啟用位
    } else {
        cr0 &= ~CR0::CMDQEN;  // 清除啟用位
    }
    write_register(RegisterOffset::CR0, cr0);
}

// 檢查事件隊列是否已啟用
bool RegisterInterface::is_eventq_enabled() const {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    return (cr0 & CR0::EVENTQEN) != 0;
}

// 設置事件隊列啟用狀態
void RegisterInterface::set_eventq_enabled(bool enabled) {
    uint32_t cr0 = read_register(RegisterOffset::CR0);
    if (enabled) {
        cr0 |= CR0::EVENTQEN;   // 設置啟用位
    } else {
        cr0 &= ~CR0::EVENTQEN;  // 清除啟用位
    }
    write_register(RegisterOffset::CR0, cr0);
}

// ============================================================================
// 基地址寄存器輔助方法
// ============================================================================

// 獲取流表基地址
uint64_t RegisterInterface::get_stream_table_base() const {
    return read_register_64(RegisterOffset::STRTAB_BASE);
}

// 設置流表基地址
void RegisterInterface::set_stream_table_base(uint64_t base) {
    write_register_64(RegisterOffset::STRTAB_BASE, base);
}

// 獲取命令隊列基地址
uint64_t RegisterInterface::get_cmdq_base() const {
    return read_register_64(RegisterOffset::CMDQ_BASE);
}

// 設置命令隊列基地址
void RegisterInterface::set_cmdq_base(uint64_t base) {
    write_register_64(RegisterOffset::CMDQ_BASE, base);
}

// 獲取事件隊列基地址
uint64_t RegisterInterface::get_eventq_base() const {
    return read_register_64(RegisterOffset::EVENTQ_BASE);
}

// 設置事件隊列基地址
void RegisterInterface::set_eventq_base(uint64_t base) {
    write_register_64(RegisterOffset::EVENTQ_BASE, base);
}

// ============================================================================
// 隊列索引寄存器輔助方法
// 生產者索引：軟件寫入，指示下一個要寫入的位置
// 消費者索引：硬件更新，指示下一個要處理的位置
// ============================================================================

// 獲取命令隊列生產者索引
uint32_t RegisterInterface::get_cmdq_prod() const {
    return read_register(RegisterOffset::CMDQ_PROD);
}

// 設置命令隊列生產者索引
void RegisterInterface::set_cmdq_prod(uint32_t prod) {
    write_register(RegisterOffset::CMDQ_PROD, prod);
}

// 獲取命令隊列消費者索引
uint32_t RegisterInterface::get_cmdq_cons() const {
    return read_register(RegisterOffset::CMDQ_CONS);
}

// 設置命令隊列消費者索引
void RegisterInterface::set_cmdq_cons(uint32_t cons) {
    write_register(RegisterOffset::CMDQ_CONS, cons);
}

// 獲取事件隊列生產者索引
uint32_t RegisterInterface::get_eventq_prod() const {
    return read_register(RegisterOffset::EVENTQ_PROD);
}

// 設置事件隊列生產者索引
void RegisterInterface::set_eventq_prod(uint32_t prod) {
    write_register(RegisterOffset::EVENTQ_PROD, prod);
}

// 獲取事件隊列消費者索引
uint32_t RegisterInterface::get_eventq_cons() const {
    return read_register(RegisterOffset::EVENTQ_CONS);
}

// 設置事件隊列消費者索引
void RegisterInterface::set_eventq_cons(uint32_t cons) {
    write_register(RegisterOffset::EVENTQ_CONS, cons);
}

} // namespace smmu
