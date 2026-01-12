// SMMU 高級示例程序
// 演示多設備、多地址空間的複雜場景
// 模擬多個設備（流）使用不同的地址空間進行 DMA 訪問

#include "smmu.h"
#include "smmu_registers.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace smmu;

// ============================================================================
// SMMU 模擬器類
// 封裝 SMMU 功能，提供高級接口用於設備管理和 DMA 模擬
// ============================================================================

class SMMUSimulator {
public:
    // 構造函數：初始化 SMMU 和內存模型
    SMMUSimulator() : memory_(std::make_shared<SimpleMemoryModel>()) {
        SMMUConfig config;
        config.tlb_size = 256;           // 較大的 TLB 以支持多設備
        config.stream_table_size = 16;   // 支持16個設備
        smmu_ = std::make_unique<SMMU>(config);
        smmu_->set_memory_model(memory_);
    }
    
    // ========================================================================
    // 設置設備及其地址空間
    // 為每個設備配置獨立的頁表和轉換上下文
    // ========================================================================
    
    void setup_device(StreamID stream_id, ASID asid, const std::string& name) {
        std::cout << "Setting up device: " << name 
                  << " (Stream " << stream_id << ", ASID " << asid << ")\n";
        
        // Allocate and setup page tables
        PhysicalAddress ttb = setup_page_table_for_device(stream_id);
        
        // Configure stream table entry
        StreamTableEntry ste;
        ste.valid = true;
        ste.s1_enabled = true;
        ste.s2_enabled = false;
        ste.vmid = 0;
        smmu_->configure_stream_table_entry(stream_id, ste);
        
        // Configure context descriptor
        ContextDescriptor cd;
        cd.valid = true;
        cd.translation_table_base = ttb;
        cd.translation_granule = 12; // 4KB
        cd.ips = 48;
        cd.asid = asid;
        smmu_->configure_context_descriptor(stream_id, asid, cd);
        
        device_names_[stream_id] = name;
        
        std::cout << "  Page table base: 0x" << std::hex << ttb << std::dec << "\n\n";
    }
    
    // ========================================================================
    // 模擬設備 DMA 訪問
    // 設備通過 SMMU 進行地址轉換後訪問物理內存
    // ========================================================================
    
    void device_dma_access(StreamID stream_id, ASID asid, 
                          VirtualAddress va, size_t size) {
        std::cout << "Device " << device_names_[stream_id] 
                  << " DMA access:\n";
        std::cout << "  VA: 0x" << std::hex << va 
                  << " Size: " << std::dec << size << " bytes\n";
        
        auto result = smmu_->translate(va, stream_id, asid, 0);
        
        if (result.success) {
            std::cout << "  ✅ Translation successful\n";
            std::cout << "  PA: 0x" << std::hex << result.physical_addr << std::dec << "\n";
            std::cout << "  Permission: " << permission_to_string(result.permission) << "\n";
            std::cout << "  Memory type: " << memory_type_to_string(result.memory_type) << "\n";
        } else {
            std::cout << "  ❌ Translation failed: " << result.fault_reason << "\n";
        }
        std::cout << "\n";
    }
    
    // ========================================================================
    // 模擬上下文切換
    // 當設備切換地址空間時，需要無效化舊地址空間的 TLB 項
    // ========================================================================
    
    void context_switch(StreamID stream_id, ASID old_asid, ASID new_asid) {
        std::cout << "Context switch for " << device_names_[stream_id] << ":\n";
        std::cout << "  ASID " << old_asid << " -> " << new_asid << "\n";
        
        // 使舊 ASID 的 TLB 項無效
        Command cmd;
        cmd.type = CommandType::CMD_TLBI_NH_ASID;
        cmd.data.tlbi_asid.asid = old_asid;
        smmu_->submit_command(cmd);
        smmu_->process_commands();
        
        std::cout << "  TLB invalidated for old ASID\n\n";
    }
    
    // ========================================================================
    // 打印統計信息
    // 顯示 SMMU 的性能指標
    // ========================================================================
    
    void print_statistics() {
        auto stats = smmu_->get_statistics();
        
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║         SMMU Statistics                ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
        std::cout << "Total translations:    " << stats.total_translations << "\n";
        std::cout << "TLB hits:              " << stats.tlb_hits << "\n";
        std::cout << "TLB misses:            " << stats.tlb_misses << "\n";
        
        if (stats.total_translations > 0) {
            double hit_rate = 100.0 * stats.tlb_hits / stats.total_translations;
            std::cout << "TLB hit rate:          " << std::fixed 
                      << std::setprecision(2) << hit_rate << "%\n";
        }
        
        std::cout << "Page table walks:      " << stats.page_table_walks << "\n";
        std::cout << "Translation faults:    " << stats.translation_faults << "\n";
        std::cout << "Permission faults:     " << stats.permission_faults << "\n";
        std::cout << "Commands processed:    " << stats.commands_processed << "\n";
        std::cout << "Events generated:      " << stats.events_generated << "\n";
        std::cout << "\n";
    }
    
    // ========================================================================
    // 處理待處理的事件
    // 顯示 SMMU 生成的錯誤和事件
    // ========================================================================
    
    void process_events() {
        if (!smmu_->has_events()) {
            return;
        }
        
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║         Pending Events                 ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
        
        while (smmu_->has_events()) {
            Event event = smmu_->pop_event();
            std::cout << "Event #" << event.timestamp << ":\n";
            std::cout << "  Type: " << fault_type_to_string(event.fault_type) << "\n";
            std::cout << "  Stream: " << event.stream_id << "\n";
            std::cout << "  ASID: " << event.asid << "\n";
            std::cout << "  VA: 0x" << std::hex << event.va << std::dec << "\n";
            std::cout << "  Description: " << event.description << "\n\n";
        }
    }
    
    void enable() {
        smmu_->enable();
        std::cout << "SMMU enabled\n\n";
    }
    
private:
    PhysicalAddress setup_page_table_for_device(StreamID stream_id) {
        // Allocate page tables
        PhysicalAddress l0 = memory_->allocate_page();
        PhysicalAddress l1 = memory_->allocate_page();
        PhysicalAddress l2 = memory_->allocate_page();
        PhysicalAddress l3 = memory_->allocate_page();
        
        // Setup page table hierarchy
        memory_->write_pte(l0, l1 | 0x3);
        memory_->write_pte(l1, l2 | 0x3);
        memory_->write_pte(l2, l3 | 0x3);
        
        // Map pages with device-specific physical addresses
        PhysicalAddress base_pa = 0x200000 + (stream_id * 0x100000);
        
        for (int i = 0; i < 16; i++) {
            PhysicalAddress pa = base_pa + (i * 0x1000);
            uint64_t pte = pa | 0x403 | (0x4 << 2); // Valid, page, AF, RW
            memory_->write_pte(l3 + (i * 8), pte);
        }
        
        return l0;
    }
    
    std::string permission_to_string(AccessPermission perm) {
        switch (perm) {
            case AccessPermission::READ_ONLY: return "READ_ONLY";
            case AccessPermission::READ_WRITE: return "READ_WRITE";
            case AccessPermission::WRITE_ONLY: return "WRITE_ONLY";
            default: return "NONE";
        }
    }
    
    std::string memory_type_to_string(MemoryType type) {
        switch (type) {
            case MemoryType::DEVICE_nGnRnE: return "DEVICE_nGnRnE";
            case MemoryType::DEVICE_nGnRE: return "DEVICE_nGnRE";
            case MemoryType::NORMAL_NC: return "NORMAL_NC";
            case MemoryType::NORMAL_WT: return "NORMAL_WT";
            case MemoryType::NORMAL_WB: return "NORMAL_WB";
            default: return "UNKNOWN";
        }
    }
    
    std::string fault_type_to_string(FaultType type) {
        switch (type) {
            case FaultType::TRANSLATION_FAULT: return "TRANSLATION_FAULT";
            case FaultType::PERMISSION_FAULT: return "PERMISSION_FAULT";
            case FaultType::ACCESS_FAULT: return "ACCESS_FAULT";
            case FaultType::ADDRESS_SIZE_FAULT: return "ADDRESS_SIZE_FAULT";
            case FaultType::TLB_CONFLICT_FAULT: return "TLB_CONFLICT_FAULT";
            default: return "NONE";
        }
    }
    
    std::shared_ptr<SimpleMemoryModel> memory_;
    std::unique_ptr<SMMU> smmu_;
    std::unordered_map<StreamID, std::string> device_names_;
};

// ============================================================================
// 主函數：運行多設備 DMA 模擬
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║   SMMU Advanced Usage Example          ║\n";
    std::cout << "║   Multi-Device DMA Simulation          ║\n";
    std::cout << "║   SMMU 高級使用示例                    ║\n";
    std::cout << "║   多設備 DMA 模擬                      ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    SMMUSimulator sim;
    
    // 設置多個設備
    std::cout << "=== Device Setup ===\n";
    std::cout << "=== 設備設置 ===\n\n";
    sim.setup_device(0, 1, "GPU");                  // GPU 設備
    sim.setup_device(1, 2, "Network Controller");   // 網絡控制器
    sim.setup_device(2, 3, "Storage Controller");   // 存儲控制器
    
    sim.enable();
    
    // 模擬 DMA 訪問
    std::cout << "=== DMA Operations ===\n";
    std::cout << "=== DMA 操作 ===\n\n";
    
    // GPU 訪問
    sim.device_dma_access(0, 1, 0x0000, 4096);
    sim.device_dma_access(0, 1, 0x1000, 2048);
    sim.device_dma_access(0, 1, 0x1000, 2048); // 應該命中 TLB
    
    // 網絡控制器訪問
    sim.device_dma_access(1, 2, 0x0000, 1500);
    sim.device_dma_access(1, 2, 0x2000, 1500);
    
    // 存儲控制器訪問
    sim.device_dma_access(2, 3, 0x0000, 8192);
    sim.device_dma_access(2, 3, 0x4000, 4096);
    
    // 模擬 GPU 的上下文切換
    std::cout << "=== Context Switch ===\n";
    std::cout << "=== 上下文切換 ===\n\n";
    sim.context_switch(0, 1, 4);
    
    // 使用新 ASID 訪問（應該未命中 TLB）
    sim.device_dma_access(0, 4, 0x1000, 2048);
    
    // 無效訪問測試（未映射的地址）
    std::cout << "=== Invalid Access Test ===\n";
    std::cout << "=== 無效訪問測試 ===\n\n";
    sim.device_dma_access(0, 4, 0x100000, 4096);
    
    // 處理所有事件
    sim.process_events();
    
    // 打印最終統計信息
    sim.print_statistics();
    
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║      Simulation Complete! ✅           ║\n";
    std::cout << "║      模擬完成！                        ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
    
    return 0;
}
