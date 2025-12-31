# SMMU Functional Model - Project Structure

## Directory Layout

```
NPU_study/
├── README.md                  # Project overview and features
├── QUICKSTART.md             # Quick start guide
├── API.md                    # Complete API reference
├── PROJECT_STRUCTURE.md      # This file
├── Makefile                  # Build system (Make)
├── CMakeLists.txt           # Build system (CMake)
├── .gitignore               # Git ignore patterns
│
├── Core Headers
│   ├── smmu_types.h         # Type definitions and structures
│   ├── tlb.h                # TLB interface
│   ├── page_table.h         # Page table walker interface
│   ├── smmu.h               # Main SMMU controller interface
│   └── smmu_registers.h     # Register interface
│
├── Implementation
│   ├── tlb.cpp              # TLB implementation
│   ├── page_table.cpp       # Page table walker implementation
│   ├── smmu.cpp             # Main SMMU controller implementation
│   └── smmu_registers.cpp   # Register interface implementation
│
└── Examples & Tests
    ├── test_smmu.cpp        # Unit tests and basic examples
    └── example_advanced.cpp # Advanced multi-device simulation
```

## Component Dependencies

```
┌─────────────────────────────────────────────────────────┐
│                      Application                         │
│              (test_smmu, example_advanced)              │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                    SMMU Controller                       │
│                     (smmu.h/cpp)                        │
│  - Address translation coordination                      │
│  - Stream/context management                            │
│  - Command/event queue processing                       │
└──────┬──────────────┬──────────────┬───────────────────┘
       │              │              │
       │              │              │
┌──────▼──────┐ ┌────▼─────┐ ┌─────▼──────────────────┐
│     TLB     │ │   Page   │ │  Register Interface    │
│ (tlb.h/cpp) │ │  Table   │ │ (smmu_registers.h/cpp) │
│             │ │  Walker  │ │                        │
│ - Caching   │ │(page_    │ │ - Control registers    │
│ - LRU       │ │table.    │ │ - Status registers     │
│ - Inval.    │ │h/cpp)    │ │ - Queue management     │
└─────────────┘ └──────────┘ └────────────────────────┘
       │              │
       │              │
       └──────┬───────┘
              │
┌─────────────▼──────────────────────────────────────────┐
│                  Common Types                           │
│                (smmu_types.h)                          │
│  - Address types                                        │
│  - Structures (STE, CD, TranslationResult)             │
│  - Enumerations (PageSize, MemoryType, etc.)           │
└─────────────────────────────────────────────────────────┘
```

## File Descriptions

### Core Headers

#### `smmu_types.h`
- Common type definitions
- Data structures (StreamTableEntry, ContextDescriptor, etc.)
- Enumerations (PageSize, MemoryType, AccessPermission, etc.)
- No dependencies on other project files

#### `tlb.h`
- TLB class interface
- TLBEntry structure
- Lookup and invalidation methods
- Depends on: smmu_types.h

#### `page_table.h`
- PageTableWalker class interface
- PageTableDescriptor structure
- SimpleMemoryModel class
- Translation logic
- Depends on: smmu_types.h

#### `smmu.h`
- Main SMMU controller class
- Command and Event structures
- SMMUConfig structure
- High-level translation API
- Depends on: smmu_types.h, tlb.h, page_table.h

#### `smmu_registers.h`
- RegisterInterface class
- Register offset definitions
- Control register bit definitions
- Independent component

### Implementation Files

#### `tlb.cpp`
- LRU-based TLB implementation
- Hash-based lookup
- Multiple invalidation strategies
- ~200 lines

#### `page_table.cpp`
- Multi-level page table walking
- Descriptor parsing
- Simple memory model implementation
- ~250 lines

#### `smmu.cpp`
- Main translation orchestration
- Stage 1 and Stage 2 translation
- Command queue processing
- Event generation
- Statistics tracking
- ~350 lines

#### `smmu_registers.cpp`
- Register read/write implementation
- Helper methods for common operations
- ~150 lines

### Examples and Tests

#### `test_smmu.cpp`
- 5 test suites:
  1. Basic translation
  2. TLB caching
  3. TLB invalidation
  4. Command queue
  5. Register interface
- ~400 lines

#### `example_advanced.cpp`
- Multi-device simulation
- Context switching
- Event handling
- Statistics reporting
- ~350 lines

## Build Artifacts

### Generated Files (not in git)

```
build/                  # CMake build directory
*.o                    # Object files
test_smmu              # Test executable
example_advanced       # Example executable
```

## Code Statistics

| Component          | Lines | Files |
|-------------------|-------|-------|
| Headers           | ~800  | 5     |
| Implementation    | ~950  | 4     |
| Tests/Examples    | ~750  | 2     |
| Documentation     | ~1500 | 4     |
| **Total**         | **~4000** | **15** |

## Key Features by Component

### SMMU Controller
- Multi-stage translation (Stage 1, Stage 2)
- Stream-based configuration
- Command queue processing
- Event generation
- Statistics tracking

### TLB
- Configurable capacity
- LRU eviction policy
- Multiple invalidation modes
- Hit/miss tracking

### Page Table Walker
- 4-level translation support
- Multiple granule sizes (4KB, 16KB, 64KB)
- Descriptor parsing
- Block and page mappings

### Register Interface
- SMMUv3-compatible registers
- Control and status registers
- Queue management registers
- Identification registers

## Testing Coverage

### Unit Tests
- ✅ Basic address translation
- ✅ TLB caching behavior
- ✅ TLB invalidation
- ✅ Command queue operations
- ✅ Register interface

### Integration Tests
- ✅ Multi-device scenarios
- ✅ Context switching
- ✅ Event handling
- ✅ Statistics collection

## Build Systems

### Make
- Simple, direct compilation
- Explicit dependencies
- Fast incremental builds
- Targets: all, clean, test, example

### CMake
- Cross-platform support
- Library + executables
- Install targets
- Testing integration

## Documentation

### User Documentation
- `README.md` - Overview, features, architecture
- `QUICKSTART.md` - Quick start guide with examples
- `API.md` - Complete API reference

### Developer Documentation
- `PROJECT_STRUCTURE.md` - This file
- Inline code comments
- Header file documentation

## Extension Points

### Adding New Features

1. **New Command Types**
   - Add to `CommandType` enum in smmu_types.h
   - Implement in `process_command()` in smmu.cpp

2. **New Fault Types**
   - Add to `FaultType` enum in smmu_types.h
   - Generate in translation logic

3. **New Memory Types**
   - Add to `MemoryType` enum in smmu_types.h
   - Update descriptor parsing in page_table.cpp

4. **New Registers**
   - Add to `RegisterOffset` enum in smmu_registers.h
   - Implement read/write logic in smmu_registers.cpp

### Testing New Features

1. Add test case to test_smmu.cpp
2. Run `make test`
3. Verify statistics and events
4. Add to example_advanced.cpp if applicable

## Performance Considerations

### TLB Size
- Default: 128 entries
- Larger = better hit rate, more memory
- Tune based on workload

### Memory Model
- Current: 256MB simple array
- Can be replaced with custom implementation
- Interface: MemoryReadCallback

### Page Table Depth
- 4 levels supported
- Deeper = more walks, slower translation
- Cached in TLB after first walk

## Future Enhancements

Potential areas for expansion:
- [ ] Stage 1 + Stage 2 combined translation
- [ ] More granule sizes
- [ ] Hardware coherency support
- [ ] Performance optimizations
- [ ] Multi-threading support
- [ ] More comprehensive fault handling
- [ ] ATS (Address Translation Service) support
- [ ] PRI (Page Request Interface) support

## License and Attribution

Educational implementation for study purposes.
Based on ARM SMMUv3 architecture specification.
