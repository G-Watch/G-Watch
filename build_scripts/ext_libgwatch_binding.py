import glob
import copy
from setuptools import setup, Extension, Command, find_packages
from typing import Tuple
from ._utils import *
from ._common import *


# ==================== C Bindings ====================
def build_cgwatch(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    cgwatch_sources = copy.deepcopy(common_sources)
    cgwatch_sources += glob.glob(f"{root_dir}/src/capsule/*.cpp", recursive=False)
    cgwatch_sources += glob.glob(f"{root_dir}/src/profiler/*.cpp", recursive=False)
    cgwatch_sources += glob.glob(f"{root_dir}/src/binding/*.cpp", recursive=False)
    cgwatch_sources += glob.glob(f"{root_dir}/src/binding/c/*.cpp", recursive=False)
    if build_backend == "cuda":
        cgwatch_sources += glob.glob(f"{root_dir}/src/binding/c/cuda_impl/*.cpp", recursive=False)
        cgwatch_sources += glob.glob(f"{root_dir}/src/profiler/cuda_impl/**/*.cpp", recursive=True)
        cgwatch_sources += glob.glob(f"{root_dir}/src/common/cuda_impl/cupti/**/*.cpp", recursive=True)

    # includes
    cgwatch_includes = copy.deepcopy(common_includes)
    cgwatch_includes += [ f'{root_dir}/include' ]

    # ldflags
    cgwatch_ldflags = copy.deepcopy(common_ldflags)
    cgwatch_ldflags += [ '-lnuma' ]
    cgwatch_ldflags += python_ldflags

    # cflags
    cgwatch_cflags = copy.deepcopy(common_cflags)
    cgwatch_cflags += python_cflags

    product_path, ok = build_with_meson(
        name = "cgwatch",
        sources=cgwatch_sources,
        includes=cgwatch_includes,
        ldflags=cgwatch_ldflags,
        cflags=cgwatch_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )

    # install c headers
    if ok:
        # We don't install headers to system paths for pip package
        pass
        # install_hdr_cmd = [
        #     "cp", "-r", f"{root_dir}/include/gwatch", f"/usr/local/include"
        # ]
        # execute_command(cmd=["rm", "-rf", f"/usr/local/include/gwatch"], title=f"removing previous gwatch headers")
        # execute_command(cmd=install_hdr_cmd, title=f"installing gwatch headers")

    return "lib", product_path, ok


# ==================== Python Bindings ====================
def build_pygwatch(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    pygwatch_sources = copy.deepcopy(common_sources)
    pygwatch_sources += glob.glob(f"{root_dir}/src/capsule/*.cpp", recursive=False)
    pygwatch_sources += glob.glob(f"{root_dir}/src/profiler/*.cpp", recursive=False)
    pygwatch_sources += glob.glob(f"{root_dir}/src/binding/*.cpp", recursive=False)
    pygwatch_sources += glob.glob(f"{root_dir}/src/binding/python/*.cpp", recursive=False)
    if build_backend == "cuda":
        pygwatch_sources += glob.glob(f"{root_dir}/src/binding/python/cuda_impl/**/*.cpp", recursive=False)
        pygwatch_sources += glob.glob(f"{root_dir}/src/profiler/cuda_impl/**/*.cpp", recursive=True)
        pygwatch_sources += glob.glob(f"{root_dir}/src/common/cuda_impl/cupti/**/*.cpp", recursive=True)

    # includes
    pygwatch_includes = copy.deepcopy(common_includes)
    pygwatch_includes += [ f'{root_dir}/third_parties/pybind11/include' ]
    pygwatch_includes += python_includes

    # ldflags
    pygwatch_ldflags = copy.deepcopy(common_ldflags)
    pygwatch_ldflags += [ '-lnuma' ]
    pygwatch_ldflags += python_ldflags

    # cflags
    pygwatch_cflags = copy.deepcopy(common_cflags)
    pygwatch_cflags += python_cflags

    product_path, ok = build_with_meson(
        name = "pygwatch",
        sources=pygwatch_sources,
        includes=pygwatch_includes,
        ldflags=pygwatch_ldflags,
        cflags=pygwatch_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )
    return "pybind_lib", product_path, ok


__all__ = [
    "build_cgwatch",
    "build_pygwatch",
]
