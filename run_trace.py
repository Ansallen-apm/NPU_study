import os
import sys
import subprocess

def main():
    print("=== SMMU Trace Runner Tool ===")

    # Check if trace file is provided
    trace_file = "trace.csv"
    if len(sys.argv) > 1:
        trace_file = sys.argv[1]

    if not os.path.exists(trace_file):
        print(f"Error: Trace file '{trace_file}' not found.")
        return 1

    # Compilation
    print("Compiling trace runner...")
    source_files = ["trace_runner.cpp", "tlb.cpp", "page_table.cpp", "smmu.cpp", "smmu_registers.cpp"]
    output_bin = "trace_runner"

    compile_cmd = ["g++", "-std=c++17", "-Wall", "-O2", "-o", output_bin] + source_files

    try:
        subprocess.check_call(compile_cmd)
        print("Compilation successful.")
    except subprocess.CalledProcessError:
        print("Compilation failed.")
        return 1

    # Execution
    print(f"\nRunning trace: {trace_file}")
    print("-" * 40)

    try:
        subprocess.check_call([f"./{output_bin}", trace_file])
    except subprocess.CalledProcessError:
        print("Execution failed.")
        return 1

    print("-" * 40)
    print("Done.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
