# SMMU 功能模型使用指南 (SMMU Functional Model Usage Guide)

本指南將引導您如何在 `NPU_study` 環境下編譯並執行 SMMU (System Memory Management Unit) 的模擬程式。

## 1. 環境準備

請確保您的系統已安裝以下工具：
- **C++ 編譯器**: 支援 C++17 標準 (例如 `g++`)
- **Make**: 用於自動化編譯流程

## 2. 編譯程式

在專案根目錄下，使用 `make` 指令來編譯所有程式。

```bash
make clean
make
```

編譯成功後，將會產生兩個可執行檔：
1. `test_smmu`: 包含基礎功能的單元測試。
2. `example_advanced`: 一個進階的多裝置 DMA 模擬範例。

## 3. 執行測試程式 (`test_smmu`)

這個程式會執行一系列的測試來驗證 SMMU 的核心功能，包括位址轉換、TLB (Translation Lookaside Buffer) 快取行為、TLB 無效化指令、命令隊列以及暫存器介面。

**執行指令：**
```bash
./test_smmu
```

**預期輸出解讀：**
- **Test 1: Basic Translation**: 測試基本的虛擬位址 (VA) 到物理位址 (PA) 的轉換。您應該會看到 `✅ Success` 以及轉換後的 PA。
- **Test 2: TLB Caching**: 驗證 TLB 的快取機制。第一次存取會發生 Miss，第二次存取應該會 Hit。
- **Test 3: TLB Invalidation**: 測試當 TLB 被無效化後，快取是否被正確清除。
- **Test 4: Command Queue**: 模擬向命令隊列發送指令的過程。
- **Test 5: Register Interface**: 讀取 SMMU 的識別暫存器 (IDR) 並啟用 SMMU。

## 4. 執行進階範例 (`example_advanced`)

這個範例模擬了一個更複雜的系統場景，包含三個裝置（GPU、網路控制器、儲存控制器）同時透過 SMMU 進行 DMA (Direct Memory Access) 存取。

**執行指令：**
```bash
./example_advanced
```

**範例情境說明：**
1. **Device Setup (裝置設定)**:
   - **GPU**: 使用 Stream 0, ASID 1
   - **Network Controller**: 使用 Stream 1, ASID 2
   - **Storage Controller**: 使用 Stream 2, ASID 3

2. **DMA Operations (DMA 操作)**:
   - 每個裝置嘗試存取不同的虛擬位址，SMMU 會將其轉換為物理位址。
   - 您會看到成功的轉換結果 (`✅ Translation successful`)。

3. **Context Switch (上下文切換)**:
   - 模擬 GPU 進行上下文切換 (ASID 從 1 變為 4)。
   - 舊的 ASID 對應的 TLB 項目會被無效化。
   - 切換後，若嘗試使用舊的上下文設定存取，將會失敗。

4. **Invalid Access Test (無效存取測試)**:
   - 模擬存取未映射的位址，預期會發生 Translation Fault。
   - 程式會顯示產生的事件 (Events)，包含 Fault 的詳細資訊。

## 5. 專案結構簡介

如果您想深入研究程式碼，可以參考以下關鍵檔案：

- **核心邏輯**:
  - `smmu.cpp/h`: SMMU 控制器主邏輯，處理轉換請求和命令。
  - `tlb.cpp/h`: TLB 的實作，負責快取轉換結果。
  - `page_table.cpp/h`: 負責遍歷頁表 (Page Table Walk)。

- **主程式**:
  - `test_smmu.cpp`: 測試程式的原始碼。
  - `example_advanced.cpp`: 進階範例的原始碼，展示如何設定和使用 SMMU。

## 6. SystemC 支援 (選用)

本專案也包含 SystemC TLM (Transaction Level Modeling) 的支援。若您的環境已安裝 SystemC 函式庫，可以使用以下指令編譯 SystemC 版本的測試：

```bash
# 請根據您的安裝路徑設定 SYSTEMC_HOME
export SYSTEMC_HOME=/usr/local/systemc-2.3.3
make -f Makefile.systemc systemc
./test_smmu_tlm
```
*(若未安裝 SystemC，請忽略此步驟)*
