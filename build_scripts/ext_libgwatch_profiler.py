import glob
import copy
from setuptools import setup, Extension, Command, find_packages
from ._utils import *
from ._common import *
from typing import Tuple


# ==================== Profiler Executable ====================
def build_gwatch_profiler(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    profiler_exe_sources = copy.deepcopy(common_sources)
    profiler_exe_sources += glob.glob(f"{root_dir}/src/profiler/*.cpp", recursive=False)
    profiler_exe_sources += glob.glob(f"{root_dir}/src/profiler_main/*.cpp", recursive=False)
    if build_backend == "cuda":
        profiler_exe_sources += glob.glob(f"{root_dir}/src/profiler/cuda_impl/**/*.cpp", recursive=True)
        profiler_exe_sources += glob.glob(f"{root_dir}/src/common/cuda_impl/cupti/**/*.cpp", recursive=True)

    # includes
    profiler_exe_includes = copy.deepcopy(common_includes)

    # ldflags
    profiler_exe_ldflags = copy.deepcopy(common_ldflags)

    # cflags
    profiler_exe_cflags = copy.deepcopy(common_cflags)

    product_path, ok = build_with_meson(
        name = "gwatch_profiler_exe",
        sources=profiler_exe_sources,
        includes=profiler_exe_includes,
        ldflags=profiler_exe_ldflags,
        cflags=profiler_exe_cflags,
        root_dir=root_dir,
        version=common_version,
        type="exe"
    )
    return "exe", product_path, ok


__all__ = [
    "build_gwatch_profiler"
]
