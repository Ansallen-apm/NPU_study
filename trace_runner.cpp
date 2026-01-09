#include "smmu.h"
#include "smmu_registers.h"
#include "page_table.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>

using namespace smmu;

// Helper class to manage page table creation
class PageTableManager {
    std::shared_ptr<SimpleMemoryModel> memory;
    PhysicalAddress root_table_pa;

public:
    PageTableManager(std::shared_ptr<SimpleMemoryModel> mem) : memory(mem) {
        // Allocate L0 table immediately
        root_table_pa = memory->allocate_page(4096);
    }

    PhysicalAddress get_root_pa() const { return root_table_pa; }

    // Map VA to PA with permissions using 4-level walking (4KB granule)
    void map(VirtualAddress va, PhysicalAddress pa, AccessPermission ap) {
        // L0 (Root) - Index bits 47:39
        uint64_t l0_index = (va >> 39) & 0x1FF;
        PhysicalAddress l0_entry_addr = root_table_pa + (l0_index * 8);
        uint64_t l0_desc = 0;
        memory->read(l0_entry_addr, &l0_desc, 8);

        if ((l0_desc & 1) == 0) { // Invalid, allocate L1
            PhysicalAddress l1_table = memory->allocate_page(4096);
            l0_desc = l1_table | 3; // Table descriptor, Valid
            memory->write_pte(l0_entry_addr, l0_desc);
        }
        PhysicalAddress l1_table_pa = l0_desc & 0xFFFFFFFFF000ULL;

        // L1 - Index bits 38:30
        uint64_t l1_index = (va >> 30) & 0x1FF;
        PhysicalAddress l1_entry_addr = l1_table_pa + (l1_index * 8);
        uint64_t l1_desc = 0;
        memory->read(l1_entry_addr, &l1_desc, 8);

        if ((l1_desc & 1) == 0) { // Invalid, allocate L2
            PhysicalAddress l2_table = memory->allocate_page(4096);
            l1_desc = l2_table | 3;
            memory->write_pte(l1_entry_addr, l1_desc);
        }
        PhysicalAddress l2_table_pa = l1_desc & 0xFFFFFFFFF000ULL;

        // L2 - Index bits 29:21
        uint64_t l2_index = (va >> 21) & 0x1FF;
        PhysicalAddress l2_entry_addr = l2_table_pa + (l2_index * 8);
        uint64_t l2_desc = 0;
        memory->read(l2_entry_addr, &l2_desc, 8);

        if ((l2_desc & 1) == 0) { // Invalid, allocate L3
            PhysicalAddress l3_table = memory->allocate_page(4096);
            l2_desc = l3_table | 3;
            memory->write_pte(l2_entry_addr, l2_desc);
        }
        PhysicalAddress l3_table_pa = l2_desc & 0xFFFFFFFFF000ULL;

        // L3 - Index bits 20:12
        uint64_t l3_index = (va >> 12) & 0x1FF;
        PhysicalAddress l3_entry_addr = l3_table_pa + (l3_index * 8);

        // Create Page Descriptor
        // PA | AF(1) | SH(0) | AP(RW=0) | Valid(1) | Page(1)
        uint64_t l3_desc = (pa & 0xFFFFFFFFF000ULL) | (1 << 10) | 3;

        // Set permissions
        // AP[2:1]
        // 00 = RW, EL1
        // 10 = RO, EL1
        if (ap == AccessPermission::READ_ONLY) {
             l3_desc |= (1 << 7); // Set AP[2] = 1 (RO)
        }

        memory->write_pte(l3_entry_addr, l3_desc);
    }
};

struct TraceLine {
    std::string type;
    std::vector<std::string> args;
};

std::vector<TraceLine> parse_csv(const std::string& filename) {
    std::vector<TraceLine> lines;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << "\n";
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cell;
        TraceLine trace_line;
        bool first = true;

        while (std::getline(ss, cell, ',')) {
            // Trim whitespace
            size_t first_char = cell.find_first_not_of(" \t");
            if (first_char == std::string::npos) continue; // empty or whitespace only
            size_t last_char = cell.find_last_not_of(" \t");
            cell = cell.substr(first_char, (last_char - first_char + 1));

            if (first) {
                trace_line.type = cell;
                first = false;
            } else {
                trace_line.args.push_back(cell);
            }
        }

        if (!trace_line.type.empty()) {
            lines.push_back(trace_line);
        }
    }
    return lines;
}

uint64_t parse_hex_or_dec(const std::string& s) {
    if (s.find("0x") == 0 || s.find("0X") == 0) {
        return std::stoull(s, nullptr, 16);
    }
    return std::stoull(s, nullptr, 10);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <trace_file.csv>\n";
        return 1;
    }

    std::string trace_file = argv[1];
    auto commands = parse_csv(trace_file);

    // Setup SMMU
    auto memory = std::make_shared<SimpleMemoryModel>();
    SMMUConfig config;
    config.tlb_size = 128; // Larger TLB for trace
    SMMU smmu(config);
    smmu.set_memory_model(memory);
    smmu.enable();

    std::map<uint32_t, std::shared_ptr<PageTableManager>> asid_tables;
    std::map<uint32_t, uint32_t> stream_asid_map; // StreamID -> ASID

    std::cout << "Starting SMMU Trace Runner with " << trace_file << "\n";
    std::cout << "================================================\n";

    for (const auto& cmd : commands) {
        if (cmd.type == "STREAM") {
            // STREAM, <StreamID>, <ASID>
            if (cmd.args.size() < 2) {
                std::cerr << "Error: STREAM command requires StreamID and ASID\n";
                continue;
            }
            uint32_t stream_id = parse_hex_or_dec(cmd.args[0]);
            uint32_t asid = parse_hex_or_dec(cmd.args[1]);

            stream_asid_map[stream_id] = asid;

            StreamTableEntry ste;
            ste.valid = true;
            ste.s1_enabled = true;
            ste.s2_enabled = false;
            smmu.configure_stream_table_entry(stream_id, ste);

            // Ensure context descriptor is set up if we have a table for this ASID
            if (asid_tables.find(asid) != asid_tables.end()) {
                 ContextDescriptor cd;
                 cd.valid = true;
                 cd.translation_table_base = asid_tables[asid]->get_root_pa();
                 cd.translation_granule = 12; // 4KB
                 cd.ips = 48;
                 cd.asid = asid;
                 smmu.configure_context_descriptor(stream_id, asid, cd);
                 std::cout << "[CONFIG] Stream " << stream_id << " -> ASID " << asid << " (Table: 0x" << std::hex << cd.translation_table_base << std::dec << ")\n";
            } else {
                 // Create table if not exists, just so we have one
                 asid_tables[asid] = std::make_shared<PageTableManager>(memory);
                 ContextDescriptor cd;
                 cd.valid = true;
                 cd.translation_table_base = asid_tables[asid]->get_root_pa();
                 cd.translation_granule = 12;
                 cd.ips = 48;
                 cd.asid = asid;
                 smmu.configure_context_descriptor(stream_id, asid, cd);
                 std::cout << "[CONFIG] Stream " << stream_id << " -> ASID " << asid << " (New Table: 0x" << std::hex << cd.translation_table_base << std::dec << ")\n";
            }

        } else if (cmd.type == "MAP") {
            // MAP, <ASID>, <VA>, <PA>, [RW/RO]
            if (cmd.args.size() < 3) {
                 std::cerr << "Error: MAP command requires ASID, VA, PA\n";
                 continue;
            }
            uint32_t asid = parse_hex_or_dec(cmd.args[0]);
            uint64_t va = parse_hex_or_dec(cmd.args[1]);
            uint64_t pa = parse_hex_or_dec(cmd.args[2]);
            AccessPermission ap = AccessPermission::READ_WRITE;
            if (cmd.args.size() > 3 && cmd.args[3] == "RO") {
                ap = AccessPermission::READ_ONLY;
            }

            if (asid_tables.find(asid) == asid_tables.end()) {
                asid_tables[asid] = std::make_shared<PageTableManager>(memory);
            }

            asid_tables[asid]->map(va, pa, ap);
            std::cout << "[MAP] ASID " << asid << ": VA 0x" << std::hex << va << " -> PA 0x" << pa << std::dec << "\n";

        } else if (cmd.type == "ACCESS") {
            // ACCESS, <StreamID>, <VA>, [RW/RO]
            if (cmd.args.size() < 2) {
                std::cerr << "Error: ACCESS command requires StreamID and VA\n";
                continue;
            }
            uint32_t stream_id = parse_hex_or_dec(cmd.args[0]);
            uint64_t va = parse_hex_or_dec(cmd.args[1]);
            bool is_write = false;
            if (cmd.args.size() > 2 && cmd.args[2] == "W") {
                is_write = true;
            }

            uint32_t inferred_asid = 0;
            if (stream_asid_map.find(stream_id) != stream_asid_map.end()) {
                 inferred_asid = stream_asid_map[stream_id];
            }

             auto result = smmu.translate(va, stream_id, inferred_asid, 0);

             std::cout << "[ACCESS] Stream " << stream_id << " (ASID " << inferred_asid << ") VA 0x" << std::hex << va;
             if (result.success) {
                 std::cout << " -> PA 0x" << result.physical_addr << " ✅";
             } else {
                 std::cout << " -> FAULT (" << result.fault_reason << ") ❌";
             }
             std::cout << std::dec << "\n";
        }
    }

    // Print stats
    auto stats = smmu.get_statistics();
    std::cout << "\nFinal Statistics:\n";
    std::cout << "  Hits: " << stats.tlb_hits << "\n";
    std::cout << "  Misses: " << stats.tlb_misses << "\n";
    std::cout << "  Faults: " << stats.translation_faults << "\n";

    return 0;
}
