# SMMU Trace Runner Instructions

This tool allows you to run custom memory access traces through the SMMU functional model using a simple CSV format.

## Quick Start

1.  **Edit `trace.csv`**: Add your configuration and access patterns.
2.  **Run the script**:
    ```bash
    python3 run_trace.py trace.csv
    ```

## CSV Format

The CSV file supports three commands. Lines starting with `#` are comments.

### 1. STREAM
Maps a StreamID (Device) to an ASID (Address Space/Process).
```csv
STREAM, <StreamID>, <ASID>
```
Example:
```csv
STREAM, 0, 1  # Stream 0 uses ASID 1
```

### 2. MAP
Creates page table mappings for a specific ASID. This automatically sets up the 4-level page tables in the simulation memory.
```csv
MAP, <ASID>, <VirtualAddress>, <PhysicalAddress>, [Permission]
```
- **Permission**: `RW` (Read/Write, default) or `RO` (Read Only).

Example:
```csv
MAP, 1, 0x1000, 0xABC000, RW
```

### 3. ACCESS
Performs a memory access (translation request) via the SMMU.
```csv
ACCESS, <StreamID>, <VirtualAddress>, [Type]
```
- **Type**: `R` (Read, default) or `W` (Write). *Note: Currently strictly informational in log.*

Example:
```csv
ACCESS, 0, 0x1000
```

## Example Workflow

1.  Define which device (StreamID) belongs to which process (ASID).
2.  Map the virtual memory for that process to physical memory.
3.  Simulate accesses.

**trace.csv**:
```csv
# Configure Device 0 to use Page Table 10
STREAM, 0, 10

# Map VA 0x1000 -> PA 0x200000 for Process 10
MAP, 10, 0x1000, 0x200000, RW

# Access VA 0x1000 from Device 0 (Should succeed)
ACCESS, 0, 0x1000
```
