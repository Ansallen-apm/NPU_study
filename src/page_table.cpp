// 頁表（Page Table）實現文件
// 實現多級頁表遍歷和地址轉換的核心邏輯

#include "page_table.h"
#include <cstring>
#include <cmath>

namespace smmu {

// ============================================================================
// 頁表遍歷器實現
// ============================================================================

// 構造函數
PageTableWalker::PageTableWalker(MemoryReadCallback memory_read)
    : memory_read_(memory_read) {}

// ============================================================================
// 獲取頁面大小
// 根據頁表級別和粒度大小返回對應的頁面大小
// ============================================================================

PageSize PageTableWalker::get_page_size(uint8_t level, uint8_t granule_size) const {
    if (granule_size == 12) { // 4KB 粒度
        switch (level) {
            case 0: return PageSize::SIZE_512MB;  // L0 塊大小
            case 1: return PageSize::SIZE_2MB;    // L1 塊大小
            case 2: return PageSize::SIZE_4KB;    // L2 頁面大小
            case 3: return PageSize::SIZE_4KB;    // L3 頁面大小
            default: return PageSize::SIZE_4KB;
        }
    } else if (granule_size == 14) { // 16KB 粒度
        switch (level) {
            case 0: return PageSize::SIZE_1GB;    // L0 塊大小
            case 1: return PageSize::SIZE_32MB;   // L1 塊大小
            case 2: return PageSize::SIZE_16KB;   // L2 頁面大小
            case 3: return PageSize::SIZE_16KB;   // L3 頁面大小
            default: return PageSize::SIZE_16KB;
        }
    } else if (granule_size == 16) { // 64KB 粒度
        switch (level) {
            case 1: return PageSize::SIZE_512MB;  // L1 塊大小
            case 2: return PageSize::SIZE_64KB;   // L2 頁面大小
            case 3: return PageSize::SIZE_64KB;   // L3 頁面大小
            default: return PageSize::SIZE_64KB;
        }
    }
    return PageSize::SIZE_4KB;
}

// ============================================================================
// 從虛擬地址提取索引位
// 根據頁表級別和粒度大小，從虛擬地址中提取對應的索引
// ============================================================================

uint64_t PageTableWalker::get_index_bits(VirtualAddress va, uint8_t level,
                                         uint8_t granule_size) const {
    // 計算每級頁表的索引位數
    // 每個描述符8字節，所以減3（log2(8) = 3）
    uint8_t bits_per_level = granule_size - 3;
    
    // 計算需要右移的位數
    // 例如：4KB粒度（granule_size=12），L0級別需要右移 12 + (3-0)*9 = 39位
    uint64_t shift = granule_size + (3 - level) * bits_per_level;
    
    // 創建掩碼以提取索引位
    uint64_t mask = (1ULL << bits_per_level) - 1;
    
    return (va >> shift) & mask;
}

// ============================================================================
// 計算描述符地址
// 根據頁表基地址和索引計算描述符在內存中的地址
// ============================================================================

uint64_t PageTableWalker::get_descriptor_address(PhysicalAddress table_base,
                                                 uint64_t index,
                                                 uint8_t granule_size) const {
    // 每個描述符佔8字節
    return table_base + (index * 8);
}

// ============================================================================
// 讀取描述符
// 從物理內存讀取64位描述符值
// ============================================================================

bool PageTableWalker::read_descriptor(PhysicalAddress addr, uint64_t& desc) {
    return memory_read_(addr, desc, 8);
}

// ============================================================================
// 解析頁表描述符
// 將64位描述符值解析為結構化的描述符信息
// ============================================================================

PageTableDescriptor PageTableWalker::parse_descriptor(uint64_t desc,
                                                      uint8_t level,
                                                      uint8_t granule_size) {
    PageTableDescriptor result;
    
    // 檢查有效位（bit 0）
    result.valid = (desc & 0x1) != 0;
    if (!result.valid) {
        return result;  // 無效描述符，直接返回
    }
    
    // 檢查描述符類型（bit 1）
    uint8_t desc_type = (desc >> 1) & 0x1;
    
    if (level < 3) {
        // L0-L2 級別可以是表描述符或塊描述符
        // bit 1 = 1: 表描述符（指向下一級頁表）
        // bit 1 = 0: 塊描述符（直接映射大塊內存）
        result.is_table = (desc_type == 1);
    } else {
        // L3 級別總是頁描述符（最小粒度）
        result.is_table = false;
    }
    
    // 提取輸出地址（bits [47:12]，適用於4KB粒度）
    // 這是下一級頁表的地址或最終的物理地址
    uint64_t addr_mask = 0x0000FFFFFFFFF000ULL;
    result.address = desc & addr_mask;
    
    // 訪問權限（bits [7:6]）
    // AP[1:0] 編碼訪問權限
    uint8_t ap_bits = (desc >> 6) & 0x3;
    switch (ap_bits) {
        case 0: result.ap = AccessPermission::READ_WRITE; break;  // 特權讀寫
        case 1: result.ap = AccessPermission::READ_WRITE; break;  // 全部讀寫
        case 2: result.ap = AccessPermission::READ_ONLY; break;   // 特權只讀
        case 3: result.ap = AccessPermission::READ_ONLY; break;   // 全部只讀
    }
    
    // 共享性屬性（bits [9:8]）
    // SH[1:0]: 00=非共享, 01=保留, 10=外部共享, 11=內部共享
    uint8_t sh = (desc >> 8) & 0x3;
    result.shareable = (sh != 0);
    
    // 訪問標誌（bit 10）
    // AF=1 表示頁面已被訪問過
    result.access_flag = (desc >> 10) & 0x1;
    
    // 內存屬性（bits [4:2]，用於階段1轉換）
    // AttrIndx[2:0] 索引到 MAIR 寄存器
    uint8_t attr_idx = (desc >> 2) & 0x7;
    switch (attr_idx) {
        case 0: result.mem_attr = MemoryType::DEVICE_nGnRnE; break;
        case 1: result.mem_attr = MemoryType::DEVICE_nGnRE; break;
        case 2: result.mem_attr = MemoryType::NORMAL_NC; break;
        case 3: result.mem_attr = MemoryType::NORMAL_WT; break;
        default: result.mem_attr = MemoryType::NORMAL_WB; break;
    }
    
    // 執行權限
    // PXN (bit 53): 特權執行永不
    // XN (bit 54): 執行永不
    result.privileged_execute_never = (desc >> 53) & 0x1;
    result.execute_never = (desc >> 54) & 0x1;
    
    // 連續位（bit 52）
    // 用於TLB優化，表示連續的頁面映射
    result.contiguous = (desc >> 52) & 0x1;
    
    // 髒位（bit 51）- DBM（Dirty Bit Modifier）
    // 表示頁面已被寫入
    result.dirty = (desc >> 51) & 0x1;
    
    return result;
}

// ============================================================================
// 頁表遍歷主邏輯
// 從頂層頁表開始，逐級遍歷直到找到最終的物理地址
// ============================================================================

TranslationResult PageTableWalker::walk_table(const WalkContext& ctx) {
    TranslationResult result;
    
    PhysicalAddress table_base = ctx.ttb;      // 當前頁表基地址
    uint8_t current_level = ctx.start_level;   // 當前頁表級別
    
    // 逐級遍歷頁表
    while (current_level <= ctx.max_level) {
        // 步驟1：從虛擬地址中提取當前級別的索引
        uint64_t index = get_index_bits(ctx.va, current_level, ctx.granule_size);
        
        // 步驟2：計算描述符在頁表中的地址
        PhysicalAddress desc_addr = get_descriptor_address(table_base, index, 
                                                           ctx.granule_size);
        
        // 步驟3：從內存讀取描述符
        uint64_t desc_value;
        if (!read_descriptor(desc_addr, desc_value)) {
            result.fault_reason = "Failed to read descriptor";
            return result;
        }
        
        // 步驟4：解析描述符
        PageTableDescriptor desc = parse_descriptor(desc_value, current_level,
                                                    ctx.granule_size);
        
        // 步驟5：檢查描述符是否有效
        if (!desc.valid) {
            result.fault_reason = "Translation fault: invalid descriptor";
            return result;
        }
        
        // 步驟6：判斷描述符類型
        if (!desc.is_table) {
            // 塊描述符或頁描述符 - 轉換完成
            
            // 計算頁面大小和偏移
            PageSize page_size = get_page_size(current_level, ctx.granule_size);
            uint64_t page_mask = static_cast<uint64_t>(page_size) - 1;
            uint64_t offset = ctx.va & page_mask;  // 頁內偏移
            
            // 填充轉換結果
            result.success = true;
            result.physical_addr = desc.address + offset;  // 物理地址 = 基地址 + 偏移
            result.permission = desc.ap;
            result.memory_type = desc.mem_attr;
            result.cacheable = (desc.mem_attr == MemoryType::NORMAL_WB ||
                               desc.mem_attr == MemoryType::NORMAL_WT);
            result.shareable = desc.shareable;
            return result;
        }
        
        // 表描述符 - 繼續到下一級頁表
        table_base = desc.address;  // 更新頁表基地址為下一級頁表
        current_level++;            // 進入下一級
    }
    
    // 超過最大級別仍未完成轉換
    result.fault_reason = "Translation fault: exceeded max level";
    return result;
}

// ============================================================================
// 地址轉換入口函數
// 設置遍歷上下文並啟動頁表遍歷
// ============================================================================

TranslationResult PageTableWalker::translate(VirtualAddress va,
                                            PhysicalAddress ttb,
                                            uint8_t granule_size,
                                            uint8_t ips_bits,
                                            TranslationStage stage) {
    // 初始化遍歷上下文
    WalkContext ctx;
    ctx.va = va;
    ctx.ttb = ttb;
    ctx.granule_size = granule_size;
    ctx.ips_bits = ips_bits;
    ctx.stage = stage;
    
    // 根據粒度大小確定起始級別和最大級別
    if (granule_size == 12) { // 4KB 粒度
        ctx.start_level = 0;  // 從 L0 開始
        ctx.max_level = 3;    // 最多到 L3
    } else if (granule_size == 14) { // 16KB 粒度
        ctx.start_level = 0;
        ctx.max_level = 3;
    } else if (granule_size == 16) { // 64KB 粒度
        ctx.start_level = 1;  // 從 L1 開始（64KB 沒有 L0）
        ctx.max_level = 3;
    } else {
        // 無效的粒度大小
        TranslationResult result;
        result.fault_reason = "Invalid granule size";
        return result;
    }
    
    // 執行頁表遍歷
    return walk_table(ctx);
}

// ============================================================================
// 簡單內存模型實現
// 用於測試的物理內存模擬器
// ============================================================================

// 構造函數：分配256MB內存，從0x1000開始分配（保留低地址）
SimpleMemoryModel::SimpleMemoryModel() 
    : memory_(MEMORY_SIZE, 0), next_alloc_(0x1000) {}

// 向物理內存寫入數據
void SimpleMemoryModel::write(PhysicalAddress addr, const void* data, size_t size) {
    // 檢查地址範圍是否有效
    if (addr + size <= MEMORY_SIZE) {
        std::memcpy(&memory_[addr], data, size);
    }
}

// 從物理內存讀取數據
bool SimpleMemoryModel::read(PhysicalAddress addr, void* data, size_t size) {
    // 檢查地址範圍是否有效
    if (addr + size <= MEMORY_SIZE) {
        std::memcpy(data, &memory_[addr], size);
        return true;  // 讀取成功
    }
    return false;  // 地址越界
}

// 寫入頁表項（64位）
void SimpleMemoryModel::write_pte(PhysicalAddress addr, uint64_t pte) {
    write(addr, &pte, sizeof(pte));
}

// 分配物理頁面
// 簡單的順序分配器，不支持釋放
PhysicalAddress SimpleMemoryModel::allocate_page(size_t size) {
    PhysicalAddress addr = next_alloc_;
    next_alloc_ += size;
    
    // 檢查是否超出內存範圍
    if (next_alloc_ > MEMORY_SIZE) {
        return 0; // 內存不足
    }
    
    return addr;
}

} // namespace smmu
