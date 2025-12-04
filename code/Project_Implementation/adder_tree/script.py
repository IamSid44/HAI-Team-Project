#!/usr/bin/env python3

import subprocess
import sys
import os

def check_iverilog_installed():
    """Check if iverilog is installed on the system."""
    try:
        subprocess.run(['iverilog', '-V'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def check_data_file_exists():
    """Check if data_out.txt exists."""
    if not os.path.exists('data_out.txt'):
        print("ERROR: data_out.txt not found in current directory")
        print("Please create data_out.txt with the following format:")
        print("  Line 1: <bit_width> <number_of_rows>")
        print("  Following lines: Binary numbers (one per line)")
        print("\nExample:")
        print("  4 4")
        print("  1010")
        print("  0110")
        print("  1100")
        print("  0011")
        return False
    return True

def compile_verilog():
    """Compile the Verilog code using iverilog."""
    print("Compiling Verilog code with iverilog...")
    try:
        result = subprocess.run(
            ['iverilog', '-o', 'adder_tree.vvp', 'adder_tree.v'],
            capture_output=True,
            text=True,
            check=True
        )
        print("Compilation successful.")
        return True
    except subprocess.CalledProcessError as e:
        print("Compilation failed!")
        print("STDOUT:", e.stdout)
        print("STDERR:", e.stderr)
        return False

def run_simulation():
    """Run the compiled Verilog simulation."""
    print("\nRunning simulation...\n")
    try:
        result = subprocess.run(
            ['vvp', 'adder_tree.vvp'],
            capture_output=True,
            text=True,
            check=True
        )
        print(result.stdout)
        if result.stderr:
            print("Warnings/Errors:", result.stderr)
        return True
    except subprocess.CalledProcessError as e:
        print("Simulation failed!")
        print("STDOUT:", e.stdout)
        print("STDERR:", e.stderr)
        return False

def cleanup_files():
    """Clean up intermediate files."""
    files_to_remove = ['adder_tree.vvp']
    for filename in files_to_remove:
        if os.path.exists(filename):
            os.remove(filename)
            print(f"Cleaned up: {filename}")

def main():
    print("=" * 50)
    print("ADDER TREE SIMULATION RUNNER")
    print("=" * 50)
    print()
    
    # Check if iverilog is installed
    if not check_iverilog_installed():
        print("ERROR: iverilog is not installed or not in PATH")
        print("Please install Icarus Verilog:")
        print("  - Ubuntu/Debian: sudo apt-get install iverilog")
        print("  - macOS: brew install icarus-verilog")
        print("  - Windows: Download from http://bleyer.org/icarus/")
        sys.exit(1)
    
    # Check if data file exists
    if not check_data_file_exists():
        sys.exit(1)
    
    # Check if Verilog file exists
    if not os.path.exists('adder_tree.v'):
        print("ERROR: adder_tree.v not found in current directory")
        sys.exit(1)
    
    # Compile Verilog
    if not compile_verilog():
        sys.exit(1)
    
    # Run simulation
    if not run_simulation():
        cleanup_files()
        sys.exit(1)
    
    # Cleanup
    print("\nCleaning up intermediate files...")
    cleanup_files()
    
    print("\nSimulation completed successfully!")
    print("Check adder_tree_trace.vcd for waveform analysis")
    print("=" * 50)

if __name__ == "__main__":
    main()