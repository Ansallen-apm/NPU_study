// SMMU（System Memory Management Unit）實現文件
// 實現 SMMU 的所有核心功能

#include "smmu.h"
#include <cstring>

namespace smmu {

// ============================================================================
// 構造函數和初始化
// ============================================================================

// SMMU 構造函數
SMMU::SMMU(const SMMUConfig& config)
    : config_(config), enabled_(false), timestamp_counter_(0) {
    
    // 創建 TLB，使用配置中指定的大小
    tlb_ = std::make_unique<TLB>(config.tlb_size);
    
    // 初始化統計信息為零
    std::memset(&stats_, 0, sizeof(stats_));
}

// 設置內存模型
// 創建頁表遍歷器並綁定內存讀取回調
void SMMU::set_memory_model(std::shared_ptr<SimpleMemoryModel> memory) {
    memory_ = memory;
    
    // 創建內存讀取回調函數（Lambda表達式）
    // 這個回調會被頁表遍歷器用來讀取頁表項
    auto memory_read = [this](PhysicalAddress addr, uint64_t& data, size_t size) {
        return memory_->read(addr, &data, size);
    };
    
    // 創建頁表遍歷器
    page_table_walker_ = std::make_unique<PageTableWalker>(memory_read);
}

// ============================================================================
// 流表配置
// ============================================================================

// 配置流表項
// 每個設備（流）都有一個流表項，定義該設備的轉換配置
void SMMU::configure_stream_table_entry(StreamID stream_id,
                                        const StreamTableEntry& ste) {
    stream_table_[stream_id] = ste;
}

// 獲取流表項
// 如果流ID不存在，返回默認的無效表項
StreamTableEntry SMMU::get_stream_table_entry(StreamID stream_id) const {
    auto it = stream_table_.find(stream_id);
    if (it != stream_table_.end()) {
        return it->second;
    }
    return StreamTableEntry();  // 返回無效的默認表項
}

// ============================================================================
// 上下文描述符配置
// ============================================================================

// 配置上下文描述符
// 上下文描述符定義一個地址空間的轉換配置
void SMMU::configure_context_descriptor(StreamID stream_id,
                                        ASID asid,
                                        const ContextDescriptor& cd) {
    // 使用 stream_id 和 asid 組合作為鍵
    uint64_t key = make_cd_key(stream_id, asid);
    context_descriptors_[key] = cd;
}

// 獲取上下文描述符
// 如果不存在，返回默認的無效描述符
ContextDescriptor SMMU::get_context_descriptor(StreamID stream_id, ASID asid) const {
    uint64_t key = make_cd_key(stream_id, asid);
    auto it = context_descriptors_.find(key);
    if (it != context_descriptors_.end()) {
        return it->second;
    }
    return ContextDescriptor();  // 返回無效的默認描述符
}

// ============================================================================
// 階段1轉換（虛擬地址 -> 中間物理地址）
// ============================================================================

TranslationResult SMMU::translate_stage1(VirtualAddress va,
                                         const StreamTableEntry& ste,
                                         const ContextDescriptor& cd) {
    // 檢查上下文描述符是否有效
    if (!cd.valid) {
        TranslationResult result;
        result.fault_reason = "Invalid context descriptor";
        // 生成轉換錯誤事件
        generate_event(FaultType::TRANSLATION_FAULT, 0, cd.asid, 
                      ste.vmid, va, result.fault_reason);
        stats_.translation_faults++;
        return result;
    }
    
    // 執行頁表遍歷
    // 使用上下文描述符中的配置信息
    TranslationResult result = page_table_walker_->translate(
        va,                           // 虛擬地址
        cd.translation_table_base,    // 頁表基地址
        cd.translation_granule,       // 頁面粒度
        cd.ips,                       // 中間物理地址大小
        TranslationStage::STAGE1      // 階段1
    );
    
    stats_.page_table_walks++;  // 增加頁表遍歷計數
    
    // 如果轉換失敗，生成事件
    if (!result.success) {
        generate_event(FaultType::TRANSLATION_FAULT, 0, cd.asid,
                      ste.vmid, va, result.fault_reason);
        stats_.translation_faults++;
    }
    
    return result;
}

// ============================================================================
// 階段2轉換（中間物理地址 -> 物理地址）
// ============================================================================

TranslationResult SMMU::translate_stage2(PhysicalAddress ipa,
                                         const StreamTableEntry& ste) {
    // 如果階段2未啟用，直接返回輸入地址
    if (!ste.s2_enabled) {
        TranslationResult result;
        result.success = true;
        result.physical_addr = ipa;  // IPA 就是最終的 PA
        return result;
    }
    
    // 執行階段2頁表遍歷
    TranslationResult result = page_table_walker_->translate(
        ipa,                              // 中間物理地址（作為輸入）
        ste.s2_translation_table_base,    // 階段2頁表基地址
        ste.s2_granule,                   // 階段2頁面粒度
        48,                               // 假設48位物理地址
        TranslationStage::STAGE2          // 階段2
    );
    
    stats_.page_table_walks++;
    
    // 如果轉換失敗，生成事件
    if (!result.success) {
        generate_event(FaultType::TRANSLATION_FAULT, 0, 0,
                      ste.vmid, ipa, result.fault_reason);
        stats_.translation_faults++;
    }
    
    return result;
}

// ============================================================================
// 主地址轉換函數
// 這是 SMMU 的核心功能，協調所有組件完成地址轉換
// ============================================================================

TranslationResult SMMU::translate(VirtualAddress va,
                                  StreamID stream_id,
                                  ASID asid,
                                  VMID vmid) {
    stats_.total_translations++;  // 增加總轉換計數
    
    // 檢查 SMMU 是否已啟用
    if (!enabled_) {
        TranslationResult result;
        result.fault_reason = "SMMU is disabled";
        return result;
    }
    
    // 步驟1：首先檢查 TLB（快速路徑）
    auto tlb_entry = tlb_->lookup(va, stream_id, asid, vmid);
    if (tlb_entry.has_value()) {
        // TLB 命中！直接返回緩存的結果
        stats_.tlb_hits++;
        
        TranslationResult result;
        result.success = true;
        result.physical_addr = tlb_entry->pa;
        result.memory_type = tlb_entry->memory_type;
        result.permission = tlb_entry->permission;
        result.cacheable = tlb_entry->cacheable;
        result.shareable = tlb_entry->shareable;
        return result;
    }
    
    // TLB 未命中，需要進行完整的頁表遍歷
    stats_.tlb_misses++;
    
    // 步驟2：獲取流表項
    StreamTableEntry ste = get_stream_table_entry(stream_id);
    if (!ste.valid) {
        // 流表項無效，生成錯誤
        TranslationResult result;
        result.fault_reason = "Invalid stream table entry";
        generate_event(FaultType::TRANSLATION_FAULT, stream_id, asid,
                      vmid, va, result.fault_reason);
        stats_.translation_faults++;
        return result;
    }
    
    TranslationResult result;
    
    // 步驟3：根據配置執行轉換
    if (ste.s1_enabled) {
        // 階段1轉換已啟用
        ContextDescriptor cd = get_context_descriptor(stream_id, asid);
        result = translate_stage1(va, ste, cd);
        
        if (!result.success) {
            return result;  // 階段1失敗，直接返回
        }
        
        // 如果階段2也啟用，繼續進行階段2轉換
        if (ste.s2_enabled) {
            PhysicalAddress ipa = result.physical_addr;  // 階段1的輸出是階段2的輸入
            result = translate_stage2(ipa, ste);
        }
    } else if (ste.s2_enabled) {
        // 僅階段2轉換（虛擬機場景）
        result = translate_stage2(va, ste);
    } else {
        // 沒有啟用任何轉換階段
        result.fault_reason = "No translation stages enabled";
        generate_event(FaultType::TRANSLATION_FAULT, stream_id, asid,
                      vmid, va, result.fault_reason);
        stats_.translation_faults++;
        return result;
    }
    
    // 步驟4：如果轉換成功，將結果插入 TLB
    if (result.success) {
        TLBEntry entry;
        entry.va = va;
        entry.pa = result.physical_addr;
        entry.stream_id = stream_id;
        entry.asid = asid;
        entry.vmid = vmid;
        entry.page_size = PageSize::SIZE_4KB;  // 簡化處理，使用4KB
        entry.memory_type = result.memory_type;
        entry.permission = result.permission;
        entry.cacheable = result.cacheable;
        entry.shareable = result.shareable;
        entry.stage = ste.s1_enabled ? TranslationStage::STAGE1 : TranslationStage::STAGE2;
        
        tlb_->insert(entry);  // 插入 TLB 以加速後續訪問
    }
    
    return result;
}

// ============================================================================
// 命令隊列操作
// ============================================================================

// 提交命令到命令隊列
void SMMU::submit_command(const Command& cmd) {
    // 檢查隊列是否已滿
    if (command_queue_.size() < config_.command_queue_size) {
        command_queue_.push(cmd);
    }
    // 注意：如果隊列已滿，命令會被丟棄（實際硬件可能會阻塞）
}

// 處理單個命令
void SMMU::process_command(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::CMD_SYNC:
            // 同步命令：確保之前的命令都已完成
            // 在這個簡化實現中不需要特殊處理
            break;
            
        case CommandType::CMD_CFGI_STE:
            // 使流表項緩存無效
            // 當流表項被修改時使用
            invalidate_tlb_by_stream(cmd.data.cfgi_ste.stream_id);
            break;
            
        case CommandType::CMD_CFGI_CD:
            // 使上下文描述符緩存無效
            // 當上下文描述符被修改時使用
            invalidate_tlb_by_asid(cmd.data.cfgi_cd.asid);
            break;
            
        case CommandType::CMD_CFGI_ALL:
            // 使所有配置緩存無效
            // 全局配置更改時使用
            invalidate_tlb_all();
            break;
            
        case CommandType::CMD_TLBI_NH_ALL:
            // 使所有 TLB 項無效
            invalidate_tlb_all();
            break;
            
        case CommandType::CMD_TLBI_NH_ASID:
            // 按 ASID 使 TLB 項無效
            // 地址空間切換時使用
            invalidate_tlb_by_asid(cmd.data.tlbi_asid.asid);
            break;
            
        case CommandType::CMD_TLBI_NH_VA:
            // 按虛擬地址使 TLB 項無效
            // 頁表更新時使用
            invalidate_tlb_by_va(cmd.data.tlbi_va.va, cmd.data.tlbi_va.asid);
            break;
            
        case CommandType::CMD_TLBI_S12_VMALL:
            // 按 VMID 使所有 TLB 項無效
            // 虛擬機切換時使用
            invalidate_tlb_by_vmid(cmd.data.tlbi_vmall.vmid);
            break;
            
        default:
            break;
    }
    
    stats_.commands_processed++;  // 增加已處理命令計數
}

// 處理所有待處理的命令
void SMMU::process_commands() {
    while (!command_queue_.empty()) {
        Command cmd = command_queue_.front();
        command_queue_.pop();
        process_command(cmd);
    }
}

// ============================================================================
// 事件生成和處理
// ============================================================================

// 生成事件並添加到事件隊列
// 用於記錄錯誤和異常情況
void SMMU::generate_event(FaultType fault_type, StreamID stream_id,
                          ASID asid, VMID vmid, VirtualAddress va,
                          const std::string& description) {
    // 檢查事件隊列是否已滿
    if (event_queue_.size() < config_.event_queue_size) {
        Event event;
        event.fault_type = fault_type;
        event.stream_id = stream_id;
        event.asid = asid;
        event.vmid = vmid;
        event.va = va;
        event.description = description;
        event.timestamp = timestamp_counter_++;  // 分配時間戳
        
        event_queue_.push(event);
        stats_.events_generated++;
    }
    // 注意：如果隊列已滿，事件會被丟棄
}

// 檢查是否有待處理的事件
bool SMMU::has_events() const {
    return !event_queue_.empty();
}

// 彈出並返回下一個事件
Event SMMU::pop_event() {
    if (!event_queue_.empty()) {
        Event event = event_queue_.front();
        event_queue_.pop();
        return event;
    }
    return Event();  // 返回空事件
}

// ============================================================================
// TLB 無效化操作
// 這些函數直接委託給 TLB 對象
// ============================================================================

// 使所有 TLB 項無效
void SMMU::invalidate_tlb_all() {
    tlb_->invalidate_all();
}

// 按 ASID 使 TLB 項無效
void SMMU::invalidate_tlb_by_asid(ASID asid) {
    tlb_->invalidate_by_asid(asid);
}

// 按 VMID 使 TLB 項無效
void SMMU::invalidate_tlb_by_vmid(VMID vmid) {
    tlb_->invalidate_by_vmid(vmid);
}

// 按虛擬地址使 TLB 項無效
void SMMU::invalidate_tlb_by_va(VirtualAddress va, ASID asid) {
    tlb_->invalidate_by_va(va, asid);
}

// 按流ID使 TLB 項無效
void SMMU::invalidate_tlb_by_stream(StreamID stream_id) {
    tlb_->invalidate_by_stream(stream_id);
}

// ============================================================================
// 統計和控制
// ============================================================================

// 重置所有統計計數器
void SMMU::reset_statistics() {
    std::memset(&stats_, 0, sizeof(stats_));
}

// 啟用 SMMU
void SMMU::enable() {
    enabled_ = true;
}

// 禁用 SMMU
void SMMU::disable() {
    enabled_ = false;
}

} // namespace smmu
