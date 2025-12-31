# SMMU SystemC TLM Wrapper - 實現總結

## 完成的工作

已成功為 SMMU 功能模型添加完整的 SystemC TLM wrapper，使其能夠集成到硬件模擬環境中。

## 新增文件

### 1. TLM 核心文件（5個）

#### tlm_types.h
- **功能**：TLM 類型定義和工具函數
- **內容**：
  - AXI 事務類型和命令
  - QoS（Quality of Service）配置結構
  - AXI 擴展類（繼承自 tlm_extension）
  - TLM 端口配置
  - 統計信息結構
  - 輔助函數（創建事務、獲取擴展等）

#### smmu_tlm_target.h
- **功能**：TLM target socket wrapper（輸入端口）
- **特性**：
  - 接收來自設備的 AXI 事務
  - 自動進行地址轉換
  - 支持多個輸入端口（可配置）
  - 記錄統計信息（事務數、延遲等）
  - 支持 blocking transport
  - 不支持 DMI（因為需要動態轉換）

#### smmu_tlm_initiator.h
- **功能**：TLM initiator socket wrapper（輸出端口）
- **特性**：
  - 發送轉換後的事務到內存系統
  - 支持兩種端口類型：
    - DATA_PORT：普通數據端口
    - PTW_PORT：頁表遍歷專用端口
  - QoS 管理和優先級控制
  - 事務隊列管理
  - 統計信息收集

#### smmu_tlm_wrapper.h
- **功能**：主要的 SystemC 模塊
- **特性**：
  - 集成 SMMU 核心和 TLM 端口
  - 多組輸入端口（默認4個，可配置）
  - 兩組輸出端口：
    - 數據端口（普通 DMA 訪問）
    - PTW 端口（頁表遍歷，高優先級）
  - 配置接口（流表項、上下文描述符）
  - 統計信息收集和報告
  - SystemC 線程處理

#### test_smmu_tlm.cpp
- **功能**：SystemC testbench
- **內容**：
  - 簡單內存模型（TLM target）
  - 設備模擬器（TLM initiator）
  - 頂層測試模塊
  - 多設備 DMA 模擬場景
  - 完整的測試流程

### 2. 構建文件（2個）

#### Makefile.systemc
- **功能**：支持 SystemC 的 Makefile
- **特性**：
  - 自動檢測 SystemC 安裝路徑
  - 分離編譯目標（基本 vs SystemC）
  - 支持環境變量配置
  - 詳細的幫助信息

#### CMakeLists.txt（更新）
- **功能**：CMake 構建配置
- **特性**：
  - 可選的 SystemC 支持（BUILD_WITH_SYSTEMC）
  - 自動查找 SystemC 庫
  - 設置正確的 RPATH
  - 測試目標配置

### 3. 文檔文件（2個）

#### SYSTEMC_TLM_README.md
- **內容**：
  - 架構說明和端口配置
  - 編譯和安裝指南
  - 使用示例和代碼片段
  - QoS 配置說明
  - 統計信息使用
  - 性能考慮和調試技巧
  - 限制和擴展建議

#### test_compilation.sh
- **功能**：自動化測試腳本
- **特性**：
  - 測試基本編譯
  - 檢查 SystemC 可用性
  - 測試 SystemC TLM 編譯
  - 友好的輸出格式

## 架構設計

### 端口結構

```
輸入端口（可配置數量）
├── Port 0 (Target Socket) ← Device 0
├── Port 1 (Target Socket) ← Device 1
├── Port 2 (Target Socket) ← Device 2
└── Port N (Target Socket) ← Device N
         ↓
    [SMMU Core]
    - TLB
    - Page Table Walker
    - Stream Table
         ↓
輸出端口（固定2個）
├── Data Port (Initiator Socket) → Memory
└── PTW Port (Initiator Socket) → Memory (高優先級)
```

### 數據流

1. **設備發起事務**
   - 設備通過 TLM initiator socket 發送事務
   - 事務包含虛擬地址和 AXI 擴展

2. **SMMU 接收和轉換**
   - Target socket 接收事務
   - 提取 Stream ID, ASID, VMID
   - 調用 SMMU 核心進行地址轉換
   - 更新物理地址

3. **轉發到內存**
   - 根據事務類型選擇輸出端口
   - PTW 事務使用高優先級端口
   - 普通數據訪問使用數據端口
   - 添加 QoS 信息

4. **返回結果**
   - 內存返回響應
   - 通過原路徑返回給設備
   - 更新統計信息

## QoS 實現

### 數據端口 QoS
```cpp
priority = 8        // 中等優先級
urgency = 8         // 中等緊急度
preemptible = true  // 可被搶占
```

### PTW 端口 QoS
```cpp
priority = 15           // 最高優先級
urgency = 15            // 最高緊急度
preemptible = false     // 不可搶占
bandwidth_limit = ∞     // 無帶寬限制
```

### QoS 效果
- PTW 事務獲得更快的處理
- 減少頁表遍歷延遲
- 提高整體系統性能
- 避免死鎖情況

## 統計功能

### SMMU 核心統計
- 總轉換次數
- TLB 命中/未命中
- 頁表遍歷次數
- 轉換錯誤次數

### TLM 接口統計
- 總事務數
- 讀/寫事務數
- PTW 事務數
- 平均延遲
- 錯誤計數

## 使用場景

### 1. 系統級模擬
```cpp
// 集成到 SoC 模擬
class SoC : public sc_module {
    CPU* cpu;
    GPU* gpu;
    SMMUTLMWrapper* smmu;
    Memory* memory;
};
```

### 2. 性能分析
```cpp
// 分析 TLB 效果
auto stats = smmu->get_smmu_statistics();
double hit_rate = stats.tlb_hits / stats.total_translations;
```

### 3. 功能驗證
```cpp
// 測試多設備場景
for (int i = 0; i < num_devices; i++) {
    devices[i]->perform_dma();
}
smmu->print_statistics();
```

## 編譯選項

### 基本編譯（無 SystemC）
```bash
make
./test_smmu
./example_advanced
```

### SystemC TLM 編譯
```bash
# 使用 Makefile
export SYSTEMC_HOME=/usr/local/systemc-2.3.3
make -f Makefile.systemc systemc
./test_smmu_tlm

# 使用 CMake
mkdir build && cd build
cmake -DBUILD_WITH_SYSTEMC=ON ..
make
./test_smmu_tlm
```

## 關鍵特性

### ✅ 已實現

1. **多端口支持**
   - 可配置的輸入端口數量
   - 固定的兩個輸出端口

2. **AXI 兼容**
   - AXI 事務擴展
   - Stream ID, ASID, VMID 支持
   - 突發屬性

3. **QoS 管理**
   - 可配置的 QoS 參數
   - PTW 專用高優先級端口
   - 優先級和緊急度控制

4. **統計和監控**
   - 詳細的性能統計
   - 實時監控能力
   - 格式化輸出

5. **SystemC 集成**
   - 標準 TLM-2.0 接口
   - Blocking transport
   - 時間建模

### 🔄 可擴展

1. **Non-blocking Transport**
   - 添加 nb_transport 支持
   - 異步處理

2. **更多端口類型**
   - 配置端口
   - 調試端口

3. **高級 QoS**
   - 動態優先級調整
   - 帶寬管理

4. **性能優化**
   - 流水線處理
   - 並行轉換

## 測試覆蓋

### 基本功能
- ✅ 地址轉換
- ✅ TLB 操作
- ✅ 多設備訪問

### TLM 功能
- ✅ Target socket 接收
- ✅ Initiator socket 發送
- ✅ AXI 擴展傳遞
- ✅ QoS 配置

### 系統級
- ✅ 多設備模擬
- ✅ 統計收集
- ✅ 事件處理

## 性能指標

### 延遲模型
- 地址轉換：10ns
- 內存訪問：50ns
- QoS 調整：0-30ns（根據優先級）

### TLB 效果
- 命中率：通常 > 90%
- 未命中懲罰：~100ns（頁表遍歷）

### 吞吐量
- 取決於 TLB 大小和訪問模式
- 多端口支持並發訪問

## 文件統計

- **新增文件**：9個
- **更新文件**：1個（CMakeLists.txt）
- **總代碼行數**：~2000行（包括註解）
- **文檔行數**：~800行

## 兼容性

### SystemC 版本
- SystemC 2.3.x（推薦 2.3.3）
- TLM-2.0

### 編譯器
- GCC 7.0+
- Clang 6.0+
- C++17 標準

### 平台
- Linux（主要測試）
- macOS（應該可以）
- Windows（需要調整）

## 下一步建議

### 短期
1. 添加更多測試場景
2. 優化性能模型
3. 改進文檔

### 中期
1. 實現 non-blocking transport
2. 添加更詳細的時間模型
3. 支持更多 AXI 特性

### 長期
1. 集成到完整的 SoC 模擬
2. 支持硬件加速
3. 提供 Python 接口

## 總結

成功實現了完整的 SystemC TLM wrapper，使 SMMU 功能模型能夠：

✅ 集成到 SystemC 模擬環境
✅ 支持多組 AXI 輸入端口
✅ 提供兩組輸出端口（數據 + PTW）
✅ 實現 QoS 管理（PTW 高優先級）
✅ 提供完整的統計和監控
✅ 保持與原有代碼的兼容性
✅ 提供詳細的文檔和示例

這個實現為硬件級別的 SMMU 模擬提供了堅實的基礎！
