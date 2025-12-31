# SMMU Functional Model API Reference

## Core Classes

### SMMU

Main SMMU controller class.

#### Constructor

```cpp
SMMU(const SMMUConfig& config = SMMUConfig())
```

Creates an SMMU instance with the specified configuration.

#### Methods

##### Configuration

```cpp
void set_memory_model(std::shared_ptr<SimpleMemoryModel> memory)
```
Sets the memory model for page table access.

```cpp
void configure_stream_table_entry(StreamID stream_id, const StreamTableEntry& ste)
```
Configures a stream table entry for a specific stream ID.

```cpp
StreamTableEntry get_stream_table_entry(StreamID stream_id) const
```
Retrieves the stream table entry for a stream ID.

```cpp
void configure_context_descriptor(StreamID stream_id, ASID asid, const ContextDescriptor& cd)
```
Configures a context descriptor for a stream and ASID.

```cpp
ContextDescriptor get_context_descriptor(StreamID stream_id, ASID asid) const
```
Retrieves the context descriptor for a stream and ASID.

##### Translation

```cpp
TranslationResult translate(VirtualAddress va, StreamID stream_id, ASID asid = 0, VMID vmid = 0)
```
Translates a virtual address to physical address.

**Parameters:**
- `va`: Virtual address to translate
- `stream_id`: Stream identifier
- `asid`: Address Space ID (for Stage 1)
- `vmid`: Virtual Machine ID (for Stage 2)

**Returns:** `TranslationResult` containing success status, physical address, and attributes.

##### Command Queue

```cpp
void submit_command(const Command& cmd)
```
Submits a command to the command queue.

```cpp
void process_commands()
```
Processes all pending commands in the queue.

##### Event Queue

```cpp
bool has_events() const
```
Checks if there are pending events.

```cpp
Event pop_event()
```
Retrieves and removes the next event from the queue.

##### TLB Management

```cpp
void invalidate_tlb_all()
```
Invalidates all TLB entries.

```cpp
void invalidate_tlb_by_asid(ASID asid)
```
Invalidates TLB entries for a specific ASID.

```cpp
void invalidate_tlb_by_vmid(VMID vmid)
```
Invalidates TLB entries for a specific VMID.

```cpp
void invalidate_tlb_by_va(VirtualAddress va, ASID asid)
```
Invalidates TLB entries for a specific virtual address.

```cpp
void invalidate_tlb_by_stream(StreamID stream_id)
```
Invalidates TLB entries for a specific stream.

##### Control

```cpp
void enable()
```
Enables the SMMU.

```cpp
void disable()
```
Disables the SMMU.

```cpp
bool is_enabled() const
```
Checks if the SMMU is enabled.

##### Statistics

```cpp
Statistics get_statistics() const
```
Returns performance statistics.

```cpp
void reset_statistics()
```
Resets all statistics counters.

---

### TLB

Translation Lookaside Buffer with LRU eviction.

#### Constructor

```cpp
TLB(size_t capacity = 128)
```

#### Methods

```cpp
std::optional<TLBEntry> lookup(VirtualAddress va, StreamID stream_id, ASID asid, VMID vmid)
```
Looks up a translation in the TLB.

```cpp
void insert(const TLBEntry& entry)
```
Inserts a new entry into the TLB.

```cpp
void invalidate_all()
void invalidate_by_asid(ASID asid)
void invalidate_by_vmid(VMID vmid)
void invalidate_by_va(VirtualAddress va, ASID asid)
void invalidate_by_stream(StreamID stream_id)
```
Various invalidation operations.

```cpp
size_t size() const
size_t capacity() const
uint64_t hit_count() const
uint64_t miss_count() const
```
Query TLB state and statistics.

---

### PageTableWalker

Walks multi-level page tables.

#### Constructor

```cpp
PageTableWalker(MemoryReadCallback memory_read)
```

#### Methods

```cpp
TranslationResult translate(VirtualAddress va, PhysicalAddress ttb, 
                           uint8_t granule_size, uint8_t ips_bits,
                           TranslationStage stage)
```
Performs page table walk for address translation.

**Parameters:**
- `va`: Virtual address to translate
- `ttb`: Translation table base address
- `granule_size`: Page granule size (12=4KB, 14=16KB, 16=64KB)
- `ips_bits`: Intermediate physical address size
- `stage`: Translation stage (STAGE1 or STAGE2)

```cpp
PageTableDescriptor parse_descriptor(uint64_t desc, uint8_t level, uint8_t granule_size)
```
Parses a page table descriptor.

```cpp
PageSize get_page_size(uint8_t level, uint8_t granule_size) const
```
Returns the page size for a given level and granule.

---

### RegisterInterface

Memory-mapped register interface.

#### Methods

```cpp
uint32_t read_register(RegisterOffset offset) const
void write_register(RegisterOffset offset, uint32_t value)
```
Read/write 32-bit registers.

```cpp
uint64_t read_register_64(RegisterOffset offset) const
void write_register_64(RegisterOffset offset, uint64_t value)
```
Read/write 64-bit registers.

##### Helper Methods

```cpp
bool is_smmu_enabled() const
void set_smmu_enabled(bool enabled)

bool is_cmdq_enabled() const
void set_cmdq_enabled(bool enabled)

bool is_eventq_enabled() const
void set_eventq_enabled(bool enabled)

uint64_t get_stream_table_base() const
void set_stream_table_base(uint64_t base)

uint64_t get_cmdq_base() const
void set_cmdq_base(uint64_t base)

uint64_t get_eventq_base() const
void set_eventq_base(uint64_t base)

uint32_t get_cmdq_prod() const
void set_cmdq_prod(uint32_t prod)

uint32_t get_cmdq_cons() const
void set_cmdq_cons(uint32_t cons)

uint32_t get_eventq_prod() const
void set_eventq_prod(uint32_t prod)

uint32_t get_eventq_cons() const
void set_eventq_cons(uint32_t cons)
```

---

### SimpleMemoryModel

Simple memory model for testing.

#### Methods

```cpp
void write(PhysicalAddress addr, const void* data, size_t size)
```
Writes data to physical memory.

```cpp
bool read(PhysicalAddress addr, void* data, size_t size)
```
Reads data from physical memory.

```cpp
void write_pte(PhysicalAddress addr, uint64_t pte)
```
Writes a page table entry.

```cpp
PhysicalAddress allocate_page(size_t size = 4096)
```
Allocates a physical page.

---

## Data Structures

### SMMUConfig

```cpp
struct SMMUConfig {
    size_t tlb_size;
    size_t stream_table_size;
    size_t command_queue_size;
    size_t event_queue_size;
    bool stage1_enabled;
    bool stage2_enabled;
};
```

### StreamTableEntry

```cpp
struct StreamTableEntry {
    bool valid;
    bool s1_enabled;
    bool s2_enabled;
    PhysicalAddress s1_context_ptr;
    PhysicalAddress s2_translation_table_base;
    VMID vmid;
    uint8_t s1_format;
    uint8_t s2_granule;
};
```

### ContextDescriptor

```cpp
struct ContextDescriptor {
    bool valid;
    PhysicalAddress translation_table_base;
    ASID asid;
    uint8_t translation_granule;
    uint8_t ips;
    uint8_t tg;
    uint8_t sh;
    uint8_t orgn;
    uint8_t irgn;
};
```

### TranslationResult

```cpp
struct TranslationResult {
    bool success;
    PhysicalAddress physical_addr;
    MemoryType memory_type;
    AccessPermission permission;
    bool cacheable;
    bool shareable;
    std::string fault_reason;
};
```

### TLBEntry

```cpp
struct TLBEntry {
    VirtualAddress va;
    PhysicalAddress pa;
    StreamID stream_id;
    ASID asid;
    VMID vmid;
    PageSize page_size;
    MemoryType memory_type;
    AccessPermission permission;
    bool cacheable;
    bool shareable;
    TranslationStage stage;
    uint64_t timestamp;
};
```

### Command

```cpp
struct Command {
    CommandType type;
    union {
        struct { StreamID stream_id; } cfgi_ste;
        struct { StreamID stream_id; ASID asid; } cfgi_cd;
        struct { ASID asid; } tlbi_asid;
        struct { VirtualAddress va; ASID asid; } tlbi_va;
        struct { VMID vmid; } tlbi_vmall;
    } data;
};
```

### Event

```cpp
struct Event {
    FaultType fault_type;
    StreamID stream_id;
    ASID asid;
    VMID vmid;
    VirtualAddress va;
    std::string description;
    uint64_t timestamp;
};
```

### Statistics

```cpp
struct Statistics {
    uint64_t total_translations;
    uint64_t tlb_hits;
    uint64_t tlb_misses;
    uint64_t page_table_walks;
    uint64_t translation_faults;
    uint64_t permission_faults;
    uint64_t commands_processed;
    uint64_t events_generated;
};
```

---

## Enumerations

### PageSize

```cpp
enum class PageSize : uint64_t {
    SIZE_4KB = 0x1000,
    SIZE_16KB = 0x4000,
    SIZE_64KB = 0x10000,
    SIZE_2MB = 0x200000,
    SIZE_32MB = 0x2000000,
    SIZE_512MB = 0x20000000,
    SIZE_1GB = 0x40000000
};
```

### TranslationStage

```cpp
enum class TranslationStage {
    STAGE1,
    STAGE2,
    STAGE1_AND_STAGE2
};
```

### MemoryType

```cpp
enum class MemoryType {
    DEVICE_nGnRnE,
    DEVICE_nGnRE,
    DEVICE_nGRE,
    DEVICE_GRE,
    NORMAL_NC,
    NORMAL_WT,
    NORMAL_WB
};
```

### AccessPermission

```cpp
enum class AccessPermission {
    NONE,
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE
};
```

### FaultType

```cpp
enum class FaultType {
    NONE,
    TRANSLATION_FAULT,
    PERMISSION_FAULT,
    ACCESS_FAULT,
    ADDRESS_SIZE_FAULT,
    TLB_CONFLICT_FAULT,
    UNSUPPORTED_UPSTREAM_TRANSACTION
};
```

### CommandType

```cpp
enum class CommandType {
    CMD_SYNC,
    CMD_PREFETCH_CONFIG,
    CMD_PREFETCH_ADDR,
    CMD_CFGI_STE,
    CMD_CFGI_CD,
    CMD_CFGI_ALL,
    CMD_TLBI_NH_ALL,
    CMD_TLBI_NH_ASID,
    CMD_TLBI_NH_VA,
    CMD_TLBI_S12_VMALL
};
```

---

## Type Aliases

```cpp
using PhysicalAddress = uint64_t;
using VirtualAddress = uint64_t;
using StreamID = uint32_t;
using SubstreamID = uint32_t;
using ASID = uint16_t;
using VMID = uint16_t;
```

---

## Usage Patterns

### Basic Translation Setup

```cpp
// 1. Create and configure SMMU
auto memory = std::make_shared<SimpleMemoryModel>();
SMMU smmu;
smmu.set_memory_model(memory);

// 2. Setup page tables in memory
PhysicalAddress ttb = setup_page_tables(memory);

// 3. Configure stream
StreamTableEntry ste;
ste.valid = true;
ste.s1_enabled = true;
smmu.configure_stream_table_entry(stream_id, ste);

// 4. Configure context
ContextDescriptor cd;
cd.valid = true;
cd.translation_table_base = ttb;
cd.translation_granule = 12;
smmu.configure_context_descriptor(stream_id, asid, cd);

// 5. Enable and translate
smmu.enable();
auto result = smmu.translate(va, stream_id, asid);
```

### TLB Invalidation

```cpp
// Invalidate all
smmu.invalidate_tlb_all();

// Invalidate by ASID
smmu.invalidate_tlb_by_asid(1);

// Invalidate specific VA
smmu.invalidate_tlb_by_va(0x1000, 1);

// Via command queue
Command cmd;
cmd.type = CommandType::CMD_TLBI_NH_ASID;
cmd.data.tlbi_asid.asid = 1;
smmu.submit_command(cmd);
smmu.process_commands();
```

### Event Handling

```cpp
while (smmu.has_events()) {
    Event event = smmu.pop_event();
    std::cout << "Fault: " << event.description << "\n";
    std::cout << "VA: 0x" << std::hex << event.va << "\n";
}
```
