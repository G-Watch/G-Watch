import os
from ._utils import *
from typing import List

current_file = os.path.abspath(__file__)
root_dir = os.path.dirname(os.path.dirname(current_file))
script_path = os.path.join(root_dir, "scripts", "get_cuda_version.sh")

cuda_version, _, ok = execute_command(cmd=["bash", script_path])
if ok == False:
    raise RuntimeError(f"failed to obtain CUDA version, {cuda_version}")
cuda_version_major = int(cuda_version.split(".")[0])
cuda_version_minor = int(cuda_version.split(".")[1])

cuda_pc_path = '/usr/lib/pkgconfig'

cuda_modules = [
    'cublas',
    'cuda',
    'cudart',
    'cufft',
    'cufftw',
    'cuinj64',
    'curand',
    'cusolver',
    'cusparse'
]

pkgconfig_env = os.environ.copy()

pkgconfig_env["PKG_CONFIG_PATH"] = cuda_pc_path


def get_cflags() -> List[str]:
    cflags = []
    for cuda_module in cuda_modules:
        cuda_module_cflags, _, ok = execute_command(cmd=["pkg-config", "--cflags", cuda_module+"-"+cuda_version], env=pkgconfig_env)
        if ok == False:
            raise RuntimeError(f"failed to cflag of {cuda_module} via pkgconfig")
        cflags += cuda_module_cflags.split()
    return cflags


def get_ldflags() -> List[str]:
    ldflags = []
    for cuda_module in cuda_modules:
        cuda_module_ldflags, _, ok = execute_command(cmd=["pkg-config", "--libs", '--static', cuda_module+"-"+cuda_version], env=pkgconfig_env)
        if ok == False:
            raise RuntimeError(f"failed to ldflag of {cuda_module} via pkgconfig")
        ldflags += cuda_module_ldflags.split()
    return ldflags


__all__ = [
    "cuda_version",
    "cuda_version_major",
    "cuda_version_minor",
    "get_cflags",
    "get_ldflags"
]
