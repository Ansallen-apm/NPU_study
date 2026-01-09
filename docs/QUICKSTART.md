# SMMU Functional Model - Quick Start Guide

## Build

```bash
make
```

## Run Tests

```bash
make test
```

## Run Advanced Example

```bash
make example
```

## Basic Usage (5 Steps)

### 1. Create SMMU and Memory Model

```cpp
#include "smmu.h"

auto memory = std::make_shared<smmu::SimpleMemoryModel>();
smmu::SMMU smmu;
smmu.set_memory_model(memory);
```

### 2. Setup Page Tables

```cpp
// Allocate page table levels
PhysicalAddress l0 = memory->allocate_page();
PhysicalAddress l1 = memory->allocate_page();
PhysicalAddress l2 = memory->allocate_page();
PhysicalAddress l3 = memory->allocate_page();

// Link tables
memory->write_pte(l0, l1 | 0x3);  // Valid + Table
memory->write_pte(l1, l2 | 0x3);
memory->write_pte(l2, l3 | 0x3);

// Map pages (VA 0x0 -> PA 0x100000)
for (int i = 0; i < 16; i++) {
    PhysicalAddress pa = 0x100000 + (i * 0x1000);
    uint64_t pte = pa | 0x403 | (0x4 << 2);  // Valid, Page, AF, RW, Normal
    memory->write_pte(l3 + (i * 8), pte);
}
```

### 3. Configure Stream

```cpp
smmu::StreamTableEntry ste;
ste.valid = true;
ste.s1_enabled = true;
ste.s2_enabled = false;
smmu.configure_stream_table_entry(0, ste);
```

### 4. Configure Context

```cpp
smmu::ContextDescriptor cd;
cd.valid = true;
cd.translation_table_base = l0;  // From step 2
cd.translation_granule = 12;     // 4KB pages
cd.ips = 48;                     // 48-bit PA
cd.asid = 1;
smmu.configure_context_descriptor(0, 1, cd);
```

### 5. Enable and Translate

```cpp
smmu.enable();

auto result = smmu.translate(0x1000, 0, 1, 0);
if (result.success) {
    std::cout << "PA: 0x" << std::hex << result.physical_addr << "\n";
}
```

## Common Operations

### TLB Invalidation

```cpp
// Invalidate all
smmu.invalidate_tlb_all();

// Invalidate by ASID
smmu.invalidate_tlb_by_asid(1);

// Invalidate by VA
smmu.invalidate_tlb_by_va(0x1000, 1);
```

### Command Queue

```cpp
smmu::Command cmd;
cmd.type = smmu::CommandType::CMD_TLBI_NH_ASID;
cmd.data.tlbi_asid.asid = 1;
smmu.submit_command(cmd);
smmu.process_commands();
```

### Event Handling

```cpp
while (smmu.has_events()) {
    smmu::Event event = smmu.pop_event();
    std::cout << "Fault: " << event.description << "\n";
}
```

### Statistics

```cpp
auto stats = smmu.get_statistics();
std::cout << "TLB hits: " << stats.tlb_hits << "\n";
std::cout << "TLB misses: " << stats.tlb_misses << "\n";
double hit_rate = 100.0 * stats.tlb_hits / stats.total_translations;
std::cout << "Hit rate: " << hit_rate << "%\n";
```

## Page Table Entry Format

### Descriptor Bits

```
[63:52] - Upper attributes
[51:48] - Reserved
[47:12] - Output address
[11:10] - Access flag
[9:8]   - Shareability
[7:6]   - Access permissions
[5:4]   - Reserved
[4:2]   - Memory attributes index
[1]     - Type (0=Block/Page, 1=Table)
[0]     - Valid
```

### Common PTE Values

```cpp
// Valid table descriptor
uint64_t table_pte = next_level_addr | 0x3;

// Valid page descriptor (RW, Normal memory)
uint64_t page_pte = physical_addr | 0x403 | (0x4 << 2);

// Valid page descriptor (RO, Device memory)
uint64_t device_pte = physical_addr | 0x483;
```

## Granule Sizes

| Granule | Value | Page Size | Levels |
|---------|-------|-----------|--------|
| 4KB     | 12    | 4096      | 0-3    |
| 16KB    | 14    | 16384     | 0-3    |
| 64KB    | 16    | 65536     | 1-3    |

## Memory Types

- `DEVICE_nGnRnE` - Device, non-gathering, non-reordering, no early write ack
- `DEVICE_nGnRE` - Device, non-gathering, non-reordering, early write ack
- `NORMAL_NC` - Normal memory, non-cacheable
- `NORMAL_WT` - Normal memory, write-through
- `NORMAL_WB` - Normal memory, write-back (default)

## Access Permissions

- `NONE` - No access
- `READ_ONLY` - Read-only access
- `WRITE_ONLY` - Write-only access
- `READ_WRITE` - Read-write access

## Troubleshooting

### Translation Fails

1. Check SMMU is enabled: `smmu.is_enabled()`
2. Verify stream table entry is valid
3. Verify context descriptor is valid
4. Check page table entries are valid
5. Review events: `smmu.has_events()`

### Low TLB Hit Rate

1. Increase TLB size in config
2. Check for excessive invalidations
3. Review access patterns

### Build Errors

```bash
# Clean and rebuild
make clean
make

# Check compiler version
g++ --version  # Requires C++17 support
```

## Examples

See:
- `test_smmu.cpp` - Basic functionality tests
- `example_advanced.cpp` - Multi-device simulation

## Documentation

- `README.md` - Overview and features
- `API.md` - Complete API reference
- `QUICKSTART.md` - This file

## Support

For issues or questions, refer to the source code comments and API documentation.
