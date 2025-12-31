// SMMU 功能模型測試程序
// 測試 SMMU 的各種功能，包括地址轉換、TLB、命令隊列等

#include "smmu.h"
#include "smmu_registers.h"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace smmu;

// ============================================================================
// 輔助函數：打印地址轉換結果
// ============================================================================

void print_translation_result(const TranslationResult& result, 
                              VirtualAddress va) {
    std::cout << "Translation for VA 0x" << std::hex << va << ":\n";
    if (result.success) {
        std::cout << "  ✅ Success\n";
        std::cout << "  PA: 0x" << result.physical_addr << "\n";
        std::cout << "  Permission: ";
        switch (result.permission) {
            case AccessPermission::READ_ONLY: 
                std::cout << "READ_ONLY\n"; 
                break;
            case AccessPermission::READ_WRITE: 
                std::cout << "READ_WRITE\n"; 
                break;
            default: 
                std::cout << "NONE\n"; 
                break;
        }
        std::cout << "  Cacheable: " << (result.cacheable ? "Yes" : "No") << "\n";
        std::cout << "  Shareable: " << (result.shareable ? "Yes" : "No") << "\n";
    } else {
        std::cout << "  ❌ Failed: " << result.fault_reason << "\n";
    }
    std::cout << std::dec << "\n";
}

// ============================================================================
// 輔助函數：設置簡單的頁表結構
// 創建4級頁表並映射一些虛擬地址到物理地址
// ============================================================================

void setup_simple_page_table(SimpleMemoryModel& memory,
                             PhysicalAddress& ttb_out) {
    // 分配4級頁表（L0-L3）
    PhysicalAddress l0_table = memory.allocate_page(4096);
    PhysicalAddress l1_table = memory.allocate_page(4096);
    PhysicalAddress l2_table = memory.allocate_page(4096);
    PhysicalAddress l3_table = memory.allocate_page(4096);
    
    ttb_out = l0_table;  // 返回頂層頁表地址
    
    std::cout << "Setting up page tables:\n";
    std::cout << "  L0 table: 0x" << std::hex << l0_table << "\n";
    std::cout << "  L1 table: 0x" << l1_table << "\n";
    std::cout << "  L2 table: 0x" << l2_table << "\n";
    std::cout << "  L3 table: 0x" << l3_table << "\n" << std::dec;
    
    // L0 描述符指向 L1 表
    // Valid (bit 0) = 1, Table (bit 1) = 1
    uint64_t l0_desc = l1_table | 0x3;
    memory.write_pte(l0_table, l0_desc);
    
    // L1 描述符指向 L2 表
    uint64_t l1_desc = l2_table | 0x3;
    memory.write_pte(l1_table, l1_desc);
    
    // L2 描述符指向 L3 表
    uint64_t l2_desc = l3_table | 0x3;
    memory.write_pte(l2_table, l2_desc);
    
    // L3 頁描述符
    // 映射虛擬地址到物理地址：
    // VA 0x0000 -> PA 0x100000
    // VA 0x1000 -> PA 0x101000
    // VA 0x2000 -> PA 0x102000
    // ... 共16個頁面
    
    for (int i = 0; i < 16; i++) {
        PhysicalAddress pa = 0x100000 + (i * 0x1000);
        // Valid (bit 0) = 1, Page (bit 1) = 1
        // Access flag (bit 10) = 1
        // Read-write (bits [7:6] = 00)
        // Normal memory (bits [4:2] = 100)
        uint64_t l3_desc = pa | 0x403 | (0x4 << 2);
        memory.write_pte(l3_table + (i * 8), l3_desc);
    }
    
    std::cout << "Page table setup complete\n\n";
}

// ============================================================================
// 測試1：基本地址轉換
// 測試 SMMU 的基本轉換功能
// ============================================================================

void test_basic_translation() {
    std::cout << "=== Test 1: Basic Translation ===\n\n";
    
    // 創建內存模型
    auto memory = std::make_shared<SimpleMemoryModel>();
    
    // 創建 SMMU，配置 TLB 大小為64
    SMMUConfig config;
    config.tlb_size = 64;
    SMMU smmu(config);
    smmu.set_memory_model(memory);
    
    // 設置頁表
    PhysicalAddress ttb;
    setup_simple_page_table(*memory, ttb);
    
    // 配置流表項（流ID = 0）
    StreamTableEntry ste;
    ste.valid = true;           // 表項有效
    ste.s1_enabled = true;      // 啟用階段1轉換
    ste.s2_enabled = false;     // 禁用階段2轉換
    ste.s1_format = 0;
    smmu.configure_stream_table_entry(0, ste);
    
    // 配置上下文描述符（ASID = 1）
    ContextDescriptor cd;
    cd.valid = true;
    cd.translation_table_base = ttb;  // 頁表基地址
    cd.translation_granule = 12;      // 4KB 頁面
    cd.ips = 48;                      // 48位物理地址
    cd.asid = 1;
    smmu.configure_context_descriptor(0, 1, cd);
    
    // 啟用 SMMU
    smmu.enable();
    
    // 測試多個虛擬地址的轉換
    VirtualAddress test_vas[] = {0x0000, 0x1000, 0x2000, 0x5000};
    
    for (auto va : test_vas) {
        auto result = smmu.translate(va, 0, 1, 0);
        print_translation_result(result, va);
    }
    
    // 打印統計信息
    auto stats = smmu.get_statistics();
    std::cout << "Statistics:\n";
    std::cout << "  Total translations: " << stats.total_translations << "\n";
    std::cout << "  TLB hits: " << stats.tlb_hits << "\n";
    std::cout << "  TLB misses: " << stats.tlb_misses << "\n";
    std::cout << "  Page table walks: " << stats.page_table_walks << "\n";
    std::cout << "  Translation faults: " << stats.translation_faults << "\n\n";
}

// ============================================================================
// 測試2：TLB 緩存功能
// 驗證 TLB 的命中和未命中行為
// ============================================================================

void test_tlb_caching() {
    std::cout << "=== Test 2: TLB Caching ===\n\n";
    
    auto memory = std::make_shared<SimpleMemoryModel>();
    
    SMMUConfig config;
    config.tlb_size = 64;
    SMMU smmu(config);
    smmu.set_memory_model(memory);
    
    PhysicalAddress ttb;
    setup_simple_page_table(*memory, ttb);
    
    StreamTableEntry ste;
    ste.valid = true;
    ste.s1_enabled = true;
    ste.s2_enabled = false;
    smmu.configure_stream_table_entry(0, ste);
    
    ContextDescriptor cd;
    cd.valid = true;
    cd.translation_table_base = ttb;
    cd.translation_granule = 12;
    cd.ips = 48;
    cd.asid = 1;
    smmu.configure_context_descriptor(0, 1, cd);
    
    smmu.enable();
    
    // First translation - should miss TLB
    std::cout << "First translation (TLB miss expected):\n";
    auto result1 = smmu.translate(0x1000, 0, 1, 0);
    print_translation_result(result1, 0x1000);
    
    // Second translation - should hit TLB
    std::cout << "Second translation (TLB hit expected):\n";
    auto result2 = smmu.translate(0x1000, 0, 1, 0);
    print_translation_result(result2, 0x1000);
    
    auto stats = smmu.get_statistics();
    std::cout << "TLB Statistics:\n";
    std::cout << "  TLB hits: " << stats.tlb_hits << "\n";
    std::cout << "  TLB misses: " << stats.tlb_misses << "\n";
    std::cout << "  Hit rate: " << std::fixed << std::setprecision(2)
              << (100.0 * stats.tlb_hits / stats.total_translations) << "%\n\n";
}

// ============================================================================
// 測試3：TLB 無效化
// 測試 TLB 無效化命令的效果
// ============================================================================

void test_tlb_invalidation() {
    std::cout << "=== Test 3: TLB Invalidation ===\n\n";
    
    auto memory = std::make_shared<SimpleMemoryModel>();
    
    SMMU smmu;
    smmu.set_memory_model(memory);
    
    PhysicalAddress ttb;
    setup_simple_page_table(*memory, ttb);
    
    StreamTableEntry ste;
    ste.valid = true;
    ste.s1_enabled = true;
    ste.s2_enabled = false;
    smmu.configure_stream_table_entry(0, ste);
    
    ContextDescriptor cd;
    cd.valid = true;
    cd.translation_table_base = ttb;
    cd.translation_granule = 12;
    cd.ips = 48;
    cd.asid = 1;
    smmu.configure_context_descriptor(0, 1, cd);
    
    smmu.enable();
    
    // Populate TLB
    std::cout << "Populating TLB...\n";
    smmu.translate(0x1000, 0, 1, 0);
    smmu.translate(0x2000, 0, 1, 0);
    
    auto stats1 = smmu.get_statistics();
    std::cout << "TLB misses before invalidation: " << stats1.tlb_misses << "\n\n";
    
    // Invalidate TLB
    std::cout << "Invalidating TLB by ASID...\n";
    smmu.invalidate_tlb_by_asid(1);
    
    // Translate again - should miss
    std::cout << "Translating after invalidation...\n";
    smmu.translate(0x1000, 0, 1, 0);
    
    auto stats2 = smmu.get_statistics();
    std::cout << "TLB misses after invalidation: " << stats2.tlb_misses << "\n\n";
}

// ============================================================================
// 測試4：命令隊列
// 測試命令提交和處理功能
// ============================================================================

void test_command_queue() {
    std::cout << "=== Test 4: Command Queue ===\n\n";
    
    auto memory = std::make_shared<SimpleMemoryModel>();
    
    SMMU smmu;
    smmu.set_memory_model(memory);
    smmu.enable();
    
    // Submit commands
    Command cmd1;
    cmd1.type = CommandType::CMD_TLBI_NH_ALL;
    smmu.submit_command(cmd1);
    
    Command cmd2;
    cmd2.type = CommandType::CMD_CFGI_ALL;
    smmu.submit_command(cmd2);
    
    Command cmd3;
    cmd3.type = CommandType::CMD_SYNC;
    smmu.submit_command(cmd3);
    
    std::cout << "Submitted 3 commands\n";
    
    // Process commands
    smmu.process_commands();
    
    auto stats = smmu.get_statistics();
    std::cout << "Commands processed: " << stats.commands_processed << "\n\n";
}

// ============================================================================
// 測試5：寄存器接口
// 測試寄存器的讀寫功能
// ============================================================================

void test_register_interface() {
    std::cout << "=== Test 5: Register Interface ===\n\n";
    
    RegisterInterface regs;
    
    // Read identification registers
    uint32_t idr0 = regs.read_register(RegisterOffset::IDR0);
    std::cout << "IDR0: 0x" << std::hex << idr0 << std::dec << "\n";
    std::cout << "  Stage 1 support: " << ((idr0 & IDR0::S1P) ? "Yes" : "No") << "\n";
    std::cout << "  Stage 2 support: " << ((idr0 & IDR0::S2P) ? "Yes" : "No") << "\n";
    std::cout << "  16-bit ASID: " << ((idr0 & IDR0::ASID16) ? "Yes" : "No") << "\n\n";
    
    // Configure control registers
    std::cout << "Enabling SMMU...\n";
    regs.set_smmu_enabled(true);
    regs.set_cmdq_enabled(true);
    regs.set_eventq_enabled(true);
    
    std::cout << "SMMU enabled: " << (regs.is_smmu_enabled() ? "Yes" : "No") << "\n";
    std::cout << "CMDQ enabled: " << (regs.is_cmdq_enabled() ? "Yes" : "No") << "\n";
    std::cout << "EVENTQ enabled: " << (regs.is_eventq_enabled() ? "Yes" : "No") << "\n\n";
    
    // Configure queue bases
    regs.set_cmdq_base(0x80000000);
    regs.set_eventq_base(0x80010000);
    regs.set_stream_table_base(0x80020000);
    
    std::cout << "Queue configuration:\n";
    std::cout << "  CMDQ base: 0x" << std::hex << regs.get_cmdq_base() << "\n";
    std::cout << "  EVENTQ base: 0x" << regs.get_eventq_base() << "\n";
    std::cout << "  Stream table base: 0x" << regs.get_stream_table_base() << "\n";
    std::cout << std::dec << "\n";
}

// ============================================================================
// 主函數：運行所有測試
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║   SMMU Functional Model Test Suite    ║\n";
    std::cout << "║   SMMU 功能模型測試套件                ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    try {
        // 運行所有測試
        test_basic_translation();      // 測試1：基本轉換
        test_tlb_caching();            // 測試2：TLB 緩存
        test_tlb_invalidation();       // 測試3：TLB 無效化
        test_command_queue();          // 測試4：命令隊列
        test_register_interface();     // 測試5：寄存器接口
        
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║      All tests completed! ✅           ║\n";
        std::cout << "║      所有測試完成！                    ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "錯誤：" << e.what() << "\n";
        return 1;
    }
}
