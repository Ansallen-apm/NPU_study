# SMMU Trace Runner Instructions (SMMU Trace 執行工具說明)

This tool allows you to run custom memory access traces through the SMMU functional model using a simple CSV format.
這個工具允許您使用簡單的 CSV 格式，透過 SMMU 功能模型執行自定義的記憶體存取 trace。

## Quick Start (快速開始)

1.  **Edit `trace.csv` (編輯 trace.csv)**: Add your configuration and access patterns. (加入您的設定和存取模式)
2.  **Run the script (執行腳本)**:
    ```bash
    python3 run_trace.py trace.csv
    ```

## CSV Format (CSV 格式)

The CSV file supports three commands. Lines starting with `#` are comments.
CSV 檔案支援三種指令。以 `#` 開頭的行是註解。

### 1. STREAM
Maps a StreamID (Device) to an ASID (Address Space/Process).
將 StreamID (裝置 ID) 對應到 ASID (位址空間 ID / 行程 ID)。

```csv
STREAM, <StreamID>, <ASID>
```
Example (範例):
```csv
STREAM, 0, 1  # Stream 0 uses ASID 1 (裝置 0 使用 ASID 1)
```

### 2. MAP
Creates page table mappings for a specific ASID. This automatically sets up the 4-level page tables in the simulation memory.
為特定的 ASID 建立頁表映射。這會自動在模擬記憶體中建立 4 層頁表。

```csv
MAP, <ASID>, <VirtualAddress>, <PhysicalAddress>, [Permission]
```
- **Permission (權限)**: `RW` (Read/Write, default 讀寫，預設) or `RO` (Read Only 唯讀).

Example (範例):
```csv
MAP, 1, 0x1000, 0xABC000, RW
```
*(Maps Virtual Address 0x1000 to Physical Address 0xABC000 with Read/Write permission for ASID 1)*
*(將 ASID 1 的虛擬位址 0x1000 映射到物理位址 0xABC000，權限為讀寫)*

### 3. ACCESS
Performs a memory access (translation request) via the SMMU.
透過 SMMU 執行記憶體存取 (位址轉換請求)。

```csv
ACCESS, <StreamID>, <VirtualAddress>, [Type]
```
- **Type (類型)**: `R` (Read, default 讀取，預設) or `W` (Write 寫入). *Note: Currently strictly informational in log. (註：目前僅用於日誌顯示)*

Example (範例):
```csv
ACCESS, 0, 0x1000
```
*(Device with StreamID 0 accesses Virtual Address 0x1000)*
*(StreamID 為 0 的裝置存取虛擬位址 0x1000)*

## Example Workflow (範例流程)

1.  Define which device (StreamID) belongs to which process (ASID). (定義哪個裝置屬於哪個行程)
2.  Map the virtual memory for that process to physical memory. (將該行程的虛擬記憶體映射到物理記憶體)
3.  Simulate accesses. (模擬存取)

**trace.csv**:
```csv
# Configure Device 0 to use Page Table 10
# 設定裝置 0 使用頁表 (ASID) 10
STREAM, 0, 10

# Map VA 0x1000 -> PA 0x200000 for Process 10
# 為行程 10 將虛擬位址 0x1000 映射到物理位址 0x200000
MAP, 10, 0x1000, 0x200000, RW

# Access VA 0x1000 from Device 0 (Should succeed)
# 裝置 0 存取虛擬位址 0x1000 (應成功)
ACCESS, 0, 0x1000
```
