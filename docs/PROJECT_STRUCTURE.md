# SMMU Functional Model - Project Structure

## Directory Layout

```
NPU_study/
├── README.md                  # Project overview and features
├── include/                   # Header files
│   ├── smmu_types.h         # Type definitions and structures
│   ├── tlb.h                # TLB interface
│   ├── page_table.h         # Page table walker interface
│   ├── smmu.h               # Main SMMU controller interface
│   └── smmu_registers.h     # Register interface
│
├── src/                       # Source files
│   ├── tlb.cpp              # TLB implementation
│   ├── page_table.cpp       # Page table walker implementation
│   ├── smmu.cpp             # Main SMMU controller implementation
│   └── smmu_registers.cpp   # Register interface implementation
│
├── tests/                     # Tests and examples
│   ├── test_smmu.cpp        # Unit tests and basic examples
│   └── example_advanced.cpp # Advanced multi-device simulation
│
├── docs/                      # Documentation
│   ├── API.md               # Complete API reference
│   ├── PROJECT_STRUCTURE.md # This file
│   ├── QUICKSTART.md        # Quick start guide
│   ├── SMMU_USAGE_GUIDE_ZH.md # Usage guide in Traditional Chinese
│   └── TRACE_README.md      # Trace tool documentation
│
├── scripts/                   # Helper scripts
│   ├── test_compilation.sh  # Compilation test script
│   └── run_trace.py         # Trace runner script
│
├── systemc/                   # SystemC integration files
│   ├── Makefile.systemc
│   ├── test_smmu_tlm.cpp
│   ├── smmu_tlm_wrapper.h
│   ├── smmu_tlm_initiator.h
│   ├── smmu_tlm_target.h
│   └── tlm_types.h
│
├── trace/                     # Trace tool files
│   ├── trace.csv            # Example trace file
│   └── trace_runner.cpp     # Trace runner source code
│
└── Makefile                   # Build system (Make)
```

## Component Dependencies

```
┌─────────────────────────────────────────────────────────┐
│                      Application                         │
│              (tests/test_smmu, tests/example_advanced)  │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                    SMMU Controller                       │
│                     (include/smmu.h, src/smmu.cpp)      │
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
│                (include/smmu_types.h)                   │
│  - Address types                                        │
│  - Structures (STE, CD, TranslationResult)             │
│  - Enumerations (PageSize, MemoryType, etc.)           │
└─────────────────────────────────────────────────────────┘
```
