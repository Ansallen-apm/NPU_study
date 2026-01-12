# SMMU SystemC TLM Wrapper

## 概述

本項目為 SMMU 功能模型提供了 SystemC TLM (Transaction Level Modeling) wrapper，使其能夠集成到基於 SystemC 的系統級模擬環境中。

## 架構

### 端口配置

```
                    ┌─────────────────────────────────┐
                    │      SMMU TLM Wrapper           │
                    │                                 │
  Device 0 ────────>│  Input Port 0 (Target)         │
                    │                                 │
  Device 1 ────────>│  Input Port 1 (Target)         │
                    │                                 │
  Device 2 ────────>│  Input Port 2 (Target)         │
                    │                                 │
  Device N ────────>│  Input Port N (Target)         │
                    │                                 │
                    │  ┌──────────────────────────┐  │
                    │  │   SMMU Core              │  │
                    │  │   - TLB                  │  │
                    │  │   - Page Table Walker    │  │
                    │  │   - Stream Table         │  │
                    │  └──────────────────────────┘  │
                    │                                 │
                    │  Data Output (Initiator) ──────>│───> Memory
                    │                                 │
                    │  PTW Output (Initiator)  ──────>│───> Memory
                    │  (with QoS)                     │     (高優先級)
                    └─────────────────────────────────┘
```

### 輸入端口（Target Sockets）

- **數量**：可配置（默認4個）
- **功能**：接收來自設備的 AXI 事務
- **特性**：
  - 每個端口對應一個或多個設備（通過 Stream ID 區分）
  - 支持 AXI 擴展（Stream ID, ASID, VMID）
  - 自動進行地址轉換
  - 記錄統計信息

### 輸出端口（Initiator Sockets）

#### 1. 數據端口（Data Output Port）
- **用途**：轉發設備 DMA 訪問到內存
- **QoS**：使用默認 QoS 配置
- **特性**：
  - 處理轉換後的數據訪問
  - 支持讀寫操作
  - 保留原始 AXI 屬性

#### 2. PTW 端口（Page Table Walk Port）
- **用途**：專門用於頁表遍歷訪問
- **QoS**：使用高優先級 QoS 配置
- **特性**：
  - 優先級：15（最高）
  - 緊急度：15
  - 不可搶占
  - 確保頁表訪問的低延遲

## 文件結構

### 核心文件

1. **tlm_types.h**
   - TLM 類型定義
   - AXI 擴展結構
   - QoS 配置
   - 統計結構

2. **smmu_tlm_target.h**
   - TLM target socket wrapper
   - 輸入端口實現
   - 地址轉換回調

3. **smmu_tlm_initiator.h**
   - TLM initiator socket wrapper
   - 輸出端口實現
   - QoS 管理

4. **smmu_tlm_wrapper.h**
   - 主要的 SystemC 模塊
   - 集成所有組件
   - 提供配置接口

5. **test_smmu_tlm.cpp**
   - SystemC testbench
   - 示例用法
   - 多設備模擬

## 編譯

### 前置條件

需要安裝 SystemC 2.3.x：

```bash
# 下載 SystemC
wget https://www.accellera.org/images/downloads/standards/systemc/systemc-2.3.3.tar.gz
tar xzf systemc-2.3.3.tar.gz
cd systemc-2.3.3

# 編譯安裝
mkdir build && cd build
../configure --prefix=/usr/local/systemc-2.3.3
make
sudo make install
```

### 使用 Makefile

```bash
# 設置 SystemC 路徑
export SYSTEMC_HOME=/usr/local/systemc-2.3.3

# 編譯 SystemC TLM 測試
make -f Makefile.systemc systemc

# 運行測試
make -f Makefile.systemc test-tlm
```

### 使用 CMake

```bash
# 創建構建目錄
mkdir build && cd build

# 配置（啟用 SystemC）
cmake -DBUILD_WITH_SYSTEMC=ON ..

# 編譯
make

# 運行測試
./test_smmu_tlm
```

## 使用示例

### 基本設置

```cpp
#include "smmu_tlm_wrapper.h"

// 創建 SMMU TLM wrapper
smmu::SMMUConfig smmu_config;
smmu_config.tlb_size = 128;

smmu_tlm::SMMUTLMConfig tlm_config;
tlm_config.num_input_ports = 4;  // 4個設備

auto smmu = new smmu_tlm::SMMUTLMWrapper("smmu", smmu_config, tlm_config);
```

### 配置設備

```cpp
// 配置流表項
smmu::StreamTableEntry ste;
ste.valid = true;
ste.s1_enabled = true;
ste.s2_enabled = false;
smmu->configure_stream(stream_id, ste);

// 配置上下文描述符
smmu::ContextDescriptor cd;
cd.valid = true;
cd.translation_table_base = ttb_address;
cd.translation_granule = 12;  // 4KB
cd.ips = 48;
cd.asid = asid;
smmu->configure_context(stream_id, asid, cd);
```

### 連接端口

```cpp
// 連接設備到 SMMU 輸入端口
device->initiator_socket.bind(smmu->input_ports[0]->target_socket);

// 連接 SMMU 輸出端口到內存
smmu->data_output_port->initiator_socket.bind(memory->target_socket);
smmu->ptw_output_port->initiator_socket.bind(memory->target_socket);
```

### 發送事務

```cpp
// 創建 TLM 事務
tlm::tlm_generic_payload trans;
trans.set_command(tlm::TLM_READ_COMMAND);
trans.set_address(virtual_address);
trans.set_data_ptr(data_buffer);
trans.set_data_length(length);

// 添加 AXI 擴展
smmu_tlm::AXIExtension axi_ext;
axi_ext.stream_id = stream_id;
axi_ext.asid = asid;
axi_ext.vmid = vmid;
smmu_tlm::set_axi_extension(trans, axi_ext);

// 發送事務
sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
initiator_socket->b_transport(trans, delay);
```

## QoS 配置

### 默認 QoS（數據端口）

```cpp
smmu_tlm::QoSConfig default_qos;
default_qos.priority = 8;        // 中等優先級
default_qos.urgency = 8;         // 中等緊急度
default_qos.preemptible = true;  // 可被搶占
```

### PTW QoS（PTW 端口）

```cpp
smmu_tlm::QoSConfig ptw_qos;
ptw_qos.priority = 15;           // 最高優先級
ptw_qos.urgency = 15;            // 最高緊急度
ptw_qos.preemptible = false;     // 不可搶占
```

### 自定義 QoS

```cpp
// 修改 PTW QoS
smmu_tlm::QoSConfig custom_qos;
custom_qos.priority = 12;
custom_qos.urgency = 10;
custom_qos.bandwidth_limit = 1024 * 1024;  // 1MB/cycle

smmu->ptw_output_port->set_qos_config(custom_qos);
```

## 統計信息

### 獲取統計

```cpp
// SMMU 核心統計
auto smmu_stats = smmu->get_smmu_statistics();
std::cout << "TLB hits: " << smmu_stats.tlb_hits << "\n";
std::cout << "TLB misses: " << smmu_stats.tlb_misses << "\n";

// TLM 接口統計
auto tlm_stats = smmu->get_tlm_statistics();
std::cout << "Total transactions: " << tlm_stats.total_transactions << "\n";
std::cout << "PTW transactions: " << tlm_stats.ptw_transactions << "\n";
std::cout << "Average latency: " << tlm_stats.get_average_latency() << " ns\n";
```

### 打印統計

```cpp
// 自動格式化輸出
smmu->print_statistics();
```

## AXI 擴展

### AXI 屬性

```cpp
class AXIExtension {
public:
    StreamID stream_id;      // 設備標識
    ASID asid;               // 地址空間ID
    VMID vmid;               // 虛擬機ID
    QoSConfig qos;           // QoS 配置
    bool is_ptw;             // 是否為 PTW 事務
    
    // AXI 突發屬性
    uint8_t burst_length;    // 突發長度
    uint8_t burst_size;      // 突發大小
    
    // 緩存和保護屬性
    uint8_t cache_attr;      // AxCACHE
    uint8_t prot_attr;       // AxPROT
};
```

### 使用 AXI 擴展

```cpp
// 獲取擴展
AXIExtension* ext = smmu_tlm::get_axi_extension(trans);

// 設置擴展
AXIExtension ext;
ext.stream_id = 0;
ext.asid = 1;
smmu_tlm::set_axi_extension(trans, ext);
```

## 性能考慮

### TLB 大小

- 較大的 TLB 可以提高命中率
- 建議：128-256 項

### 端口數量

- 根據系統中的設備數量配置
- 每個端口可以服務多個設備（通過 Stream ID）

### QoS 優先級

- PTW 使用最高優先級確保低延遲
- 根據設備重要性調整數據端口 QoS

### 延遲模型

- 地址轉換：~10ns
- 內存訪問：~50ns
- QoS 延遲：根據優先級調整

## 調試

### 啟用 SystemC 報告

```cpp
// 設置報告級別
sc_core::sc_report_handler::set_actions(
    sc_core::SC_INFO, sc_core::SC_DISPLAY);
```

### 查看事務

```cpp
// 在 b_transport 中添加日誌
std::cout << "[" << sc_time_stamp() << "] "
          << "Transaction: " << trans.get_address() << "\n";
```

### 監控統計

```cpp
// 定期打印統計
SC_THREAD(monitor_thread);

void monitor_thread() {
    while (true) {
        wait(sc_time(1, SC_MS));
        print_statistics();
    }
}
```

## 限制

1. **簡化的內存模型**
   - 當前使用簡單的數組模擬內存
   - 實際系統應連接到更複雜的內存模型

2. **同步模式**
   - 當前使用 blocking transport
   - 可以擴展支持 non-blocking transport

3. **DMI 不支持**
   - SMMU 需要動態地址轉換
   - 不支持 Direct Memory Interface

## 擴展

### 添加新的端口類型

```cpp
enum class OutputPortType {
    DATA_PORT,
    PTW_PORT,
    CONFIG_PORT  // 新增配置端口
};
```

### 自定義延遲模型

```cpp
// 在 b_transport 中
if (is_critical_transaction) {
    delay += sc_time(5, SC_NS);  // 快速路徑
} else {
    delay += sc_time(20, SC_NS); // 普通路徑
}
```

### 集成到更大的系統

```cpp
// 在系統級模塊中
class SoC : public sc_module {
    CPU* cpu;
    SMMUTLMWrapper* smmu;
    Memory* memory;
    
    // 連接所有組件
    void connect() {
        cpu->initiator_socket.bind(smmu->input_ports[0]->target_socket);
        smmu->data_output_port->initiator_socket.bind(memory->target_socket);
    }
};
```

## 參考資料

- [SystemC 官方文檔](https://www.accellera.org/downloads/standards/systemc)
- [TLM-2.0 用戶手冊](https://www.accellera.org/images/downloads/standards/systemc/TLM_2_0_User_Manual.pdf)
- [ARM SMMU 架構規範](https://developer.arm.com/documentation/)

## 支持

如有問題或建議，請參考：
- README.md - 項目概述
- API.md - API 參考
- QUICKSTART.md - 快速開始
