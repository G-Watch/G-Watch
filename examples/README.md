# G-Watch Examples

This directory contains examples demonstrating how to use different components of the G-Watch library.

## CUDA Examples

### Binary Parsing (`cuda/binary`)

The `cuda/binary` directory contains examples for parsing CUDA binary files (cubin).

- **`parse_cubin.py`**: A script that demonstrates how to load and parse a CUDA cubin file using `gwatch.cuda.binary`.
  - Usage: `python parse_cubin.py <path_to_cubin_file> [--list-kernels]`
  - Functionality:
    - Loads a cubin file.
    - Parses it to extract information.
    - Displays the architecture version.
    - Optionally lists all available kernel names and their demangled versions.
  - Sample assets are provided in `assets/` directory (e.g., `libtorch_cuda.1822.sm_90.cubin`).

### Profiling (`cuda/profiler`)

The `cuda/profiler` directory contains examples for profiling CUDA applications using G-Watch.

- **`profile_torch_model.py`**: A script demonstrating how to profile a PyTorch model using `gwatch.cuda.profile`.
  - Usage: `python profile_torch_model.py`
  - Functionality:
    - Defines a simple neural network (`LargeNN`) in PyTorch.
    - Initializes a `gwatch.cuda.profile.ProfileContext` and creates a profiler for specific metrics (e.g., `dram__bytes_read.sum.per_second`).
    - Uses the `gw_profiler.RangeProfile` context manager to profile the `forward` pass of the model.
    - Prints the collected metrics after the profiled range execution.
