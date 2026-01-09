import glob
import copy
from setuptools import setup, Extension, Command, find_packages
from typing import Tuple
from ._utils import *
from ._common import *


# ==================== Hijacker ====================
def build_libgwatch_capsule_hijack(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    capsule_hijack_sources = copy.deepcopy(common_sources)
    capsule_hijack_sources += glob.glob(f"{root_dir}/src/capsule/*.cpp", recursive=False)
    capsule_hijack_sources += glob.glob(f"{root_dir}/src/profiler/*.cpp", recursive=False)
    if build_backend == "cuda":
        capsule_hijack_sources += glob.glob(f"{root_dir}/src/capsule/cuda_impl/**/*.cpp", recursive=True)
        capsule_hijack_sources += glob.glob(f"{root_dir}/src/profiler/cuda_impl/**/*.cpp", recursive=True)
        capsule_hijack_sources += glob.glob(f"{root_dir}/src/common/cuda_impl/cupti/**/*.cpp", recursive=True)

    # includes
    capsule_hijack_includes = copy.deepcopy(common_includes)

    # ldflags
    capsule_hijack_ldflags = copy.deepcopy(common_ldflags)
    capsule_hijack_ldflags += [ '-lnuma' ]

    # cflags
    capsule_hijack_cflags = copy.deepcopy(common_cflags)

    product_path, ok = build_with_meson(
        name = "gwatch_capsule_hijack",
        sources=capsule_hijack_sources,
        includes=capsule_hijack_includes,
        ldflags=capsule_hijack_ldflags,
        cflags=capsule_hijack_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )
    return "lib", product_path, ok


__all__ = [
    "build_libgwatch_capsule_hijack"
]
