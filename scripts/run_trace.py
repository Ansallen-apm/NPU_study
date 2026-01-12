import os
import sys
import subprocess

def main():
    print("=== SMMU Trace Runner Tool ===")

    # Setup paths relative to script location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    src_dir = os.path.join(project_root, "src")
    include_dir = os.path.join(project_root, "include")
    trace_dir = os.path.join(project_root, "trace")

    # Check if trace file is provided
    trace_file = "trace.csv"
    if len(sys.argv) > 1:
        trace_file = sys.argv[1]

    # If trace file is not absolute path, look in trace dir or current dir
    if not os.path.isabs(trace_file):
        if os.path.exists(trace_file):
            pass # use as is
        elif os.path.exists(os.path.join(trace_dir, trace_file)):
            trace_file = os.path.join(trace_dir, trace_file)
        else:
             print(f"Error: Trace file '{trace_file}' not found.")
             return 1

    # Compilation
    print("Compiling trace runner...")

    source_files = [
        os.path.join(trace_dir, "trace_runner.cpp"),
        os.path.join(src_dir, "tlb.cpp"),
        os.path.join(src_dir, "page_table.cpp"),
        os.path.join(src_dir, "smmu.cpp"),
        os.path.join(src_dir, "smmu_registers.cpp")
    ]

    output_bin = os.path.join(script_dir, "trace_runner")

    compile_cmd = ["g++", "-std=c++17", "-Wall", "-O2", f"-I{include_dir}", "-o", output_bin] + source_files

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
        subprocess.check_call([output_bin, trace_file])
    except subprocess.CalledProcessError:
        print("Execution failed.")
        # Cleanup
        if os.path.exists(output_bin):
            os.remove(output_bin)
        return 1

    print("-" * 40)
    print("Done.")

    # Cleanup
    if os.path.exists(output_bin):
        os.remove(output_bin)

    return 0

if __name__ == "__main__":
    sys.exit(main())
