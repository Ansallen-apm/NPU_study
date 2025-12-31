# SMMU Functional Model

A functional model of an ARM System Memory Management Unit (SMMU) implemented in C++. This implementation provides a software model of SMMUv3 architecture for address translation, TLB management, and stream-based I/O virtualization.

## Features

- **Multi-stage Address Translation**: Support for Stage 1 and Stage 2 translations
- **TLB Implementation**: LRU-based Translation Lookaside Buffer with configurable size
- **Page Table Walking**: ARMv8-A compliant page table walker supporting 4KB, 16KB, and 64KB granules
- **Stream Table Management**: Per-stream configuration with context descriptors
- **Command Queue**: Support for configuration and TLB invalidation commands
- **Event Queue**: Fault reporting and event generation
- **Register Interface**: Memory-mapped register interface compatible with SMMUv3 specification
- **Statistics**: Performance counters for translations, TLB hits/misses, and faults

## Architecture

### Core Components

1. **SMMU Class** (`smmu.h`, `smmu.cpp`)
   - Main controller coordinating all components
   - Handles address translation requests
   - Manages stream table and context descriptors
   - Processes commands and generates events

2. **TLB** (`tlb.h`, `tlb.cpp`)
   - LRU-based caching of translation results
   - Supports invalidation by ASID, VMID, VA, and Stream ID
   - Configurable capacity

3. **Page Table Walker** (`page_table.h`, `page_table.cpp`)
   - Walks multi-level page tables
   - Supports 4-level translation (L0-L3)
   - Parses page table descriptors
   - Handles block and page mappings

4. **Register Interface** (`smmu_registers.h`, `smmu_registers.cpp`)
   - Memory-mapped control and status registers
   - Configuration registers (CR0, CR1, CR2)
   - Identification registers (IDR0, IDR1, IDR5)
   - Queue management registers

5. **Type Definitions** (`smmu_types.h`)
   - Common types and structures
   - Translation results
   - Stream table entries
   - Context descriptors

## Building

### Using Make

```bash
make clean
make
```

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

## Running Tests

```bash
# Using Make
make test

# Or directly
./test_smmu
```

## Usage Example

```cpp
#include "smmu.h"
#include <memory>

// Create memory model
auto memory = std::make_shared<smmu::SimpleMemoryModel>();

// Create SMMU with configuration
smmu::SMMUConfig config;
config.tlb_size = 128;
smmu::SMMU smmu(config);
smmu.set_memory_model(memory);

// Configure stream table entry
smmu::StreamTableEntry ste;
ste.valid = true;
ste.s1_enabled = true;
ste.s2_enabled = false;
smmu.configure_stream_table_entry(0, ste);

// Configure context descriptor
smmu::ContextDescriptor cd;
cd.valid = true;
cd.translation_table_base = 0x1000;  // Page table base address
cd.translation_granule = 12;          // 4KB pages
cd.ips = 48;                          // 48-bit physical address
cd.asid = 1;
smmu.configure_context_descriptor(0, 1, cd);

// Enable SMMU
smmu.enable();

// Perform translation
auto result = smmu.translate(0x1000, 0, 1, 0);
if (result.success) {
    std::cout << "PA: 0x" << std::hex << result.physical_addr << "\n";
}

// Get statistics
auto stats = smmu.get_statistics();
std::cout << "TLB hit rate: " 
          << (100.0 * stats.tlb_hits / stats.total_translations) 
          << "%\n";
```

## Address Translation Flow

1. **TLB Lookup**: Check if translation exists in TLB
2. **Stream Table Lookup**: Retrieve stream configuration
3. **Stage 1 Translation** (if enabled):
   - Lookup context descriptor
   - Walk page tables from translation table base
   - Parse descriptors and extract physical address
4. **Stage 2 Translation** (if enabled):
   - Use Stage 1 output as input
   - Walk Stage 2 page tables
5. **TLB Update**: Cache successful translation
6. **Return Result**: Physical address and attributes

## Page Table Format

The implementation supports ARMv8-A page table format:

### Descriptor Types
- **Table Descriptor**: Points to next level page table
- **Block Descriptor**: Maps large memory region (L1/L2)
- **Page Descriptor**: Maps single page (L3)

### Descriptor Fields
- **Valid bit [0]**: Entry is valid
- **Type bit [1]**: Table (1) or Block/Page (0)
- **Memory attributes [4:2]**: Cacheability and memory type
- **Access permissions [7:6]**: Read/write permissions
- **Shareability [9:8]**: Inner/outer shareable
- **Access flag [10]**: Page has been accessed
- **Output address [47:12]**: Physical address

## Command Queue

Supported commands:
- `CMD_SYNC`: Synchronization barrier
- `CMD_CFGI_STE`: Invalidate stream table entry cache
- `CMD_CFGI_CD`: Invalidate context descriptor cache
- `CMD_CFGI_ALL`: Invalidate all configuration caches
- `CMD_TLBI_NH_ALL`: Invalidate all TLB entries
- `CMD_TLBI_NH_ASID`: Invalidate by ASID
- `CMD_TLBI_NH_VA`: Invalidate by virtual address
- `CMD_TLBI_S12_VMALL`: Invalidate by VMID

## Event Queue

Generated events:
- Translation faults
- Permission faults
- Access faults
- Address size faults
- TLB conflict faults

## Performance Statistics

The SMMU tracks:
- Total translations
- TLB hits and misses
- Page table walks
- Translation faults
- Permission faults
- Commands processed
- Events generated

## Configuration Options

```cpp
struct SMMUConfig {
    size_t tlb_size;              // TLB capacity (default: 128)
    size_t stream_table_size;     // Stream table size (default: 256)
    size_t command_queue_size;    // Command queue depth (default: 64)
    size_t event_queue_size;      // Event queue depth (default: 64)
    bool stage1_enabled;          // Enable Stage 1 (default: true)
    bool stage2_enabled;          // Enable Stage 2 (default: false)
};
```

## Limitations

This is a functional model for simulation and testing:
- Simplified memory model (256MB)
- No hardware-specific optimizations
- Single-threaded operation
- Limited error checking for invalid configurations
- Simplified descriptor parsing

## References

- ARM System Memory Management Unit Architecture Specification (SMMUv3)
- ARM Architecture Reference Manual ARMv8-A
- ARM SMMU Architecture Specification Version 3.3

## License

This is an educational implementation for study purposes.

## Author

Developed as part of NPU study project.