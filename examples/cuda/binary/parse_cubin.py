import os
import argparse
import gwatch.cuda.binary as gwatch_binary


def main():
    parser = argparse.ArgumentParser(description="Parse a CUDA cubin file.")
    parser.add_argument("input_file", type=str, help="Path to the cubin file to parse")
    parser.add_argument("--list-kernels", action="store_true", help="List all available kernel names")
    args = parser.parse_args()

    if not os.path.exists(args.input_file):
        print(f"Error: File not found: {args.input_file}")
        return

    cubin = gwatch_binary.Cubin()
    print(f"Loading cubin from: {args.input_file}")
    cubin.fill(args.input_file)
    
    print("Parsing cubin...")
    cubin.parse()
    
    print(f"Successfully parsed.")
    print(f"Arch version: {cubin.params.arch_version}")

    if args.list_kernels:
        print("Available kernels:")
        map_kernels = cubin.get_map_kernel_def()
        for kernel_name in map_kernels:
            demangled_name = gwatch_binary.BinaryUtility.demangle(kernel_name)
            print(f" - {kernel_name}")
            print(f"   (demangled: {demangled_name})")


if __name__ == "__main__":
    main()
