import glob
import copy
from setuptools import setup, Extension, Command, find_packages
from ._utils import *
from ._common import *
from typing import Tuple


# ==================== Python Bindings ====================
def build_libgwatch_toolbox(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    toolbox_sources = copy.deepcopy(common_sources)
    toolbox_sources += glob.glob(f"{root_dir}/src/toolbox/*.cpp", recursive=False)
    if build_backend == "cuda":
        toolbox_sources += glob.glob(f"{root_dir}/src/cuda_impl/**/*.cpp", recursive=True)

    # sources: add inline profiler
    toolbox_sources += glob.glob(f"{root_dir}/src/profiler/*.cpp", recursive=False)
    toolbox_sources = [s for s in toolbox_sources if "main.cpp" not in s] # exclude main.cpp from library build
    if build_backend == "cuda":
        toolbox_sources += glob.glob(f"{root_dir}/src/profiler/cuda_impl/**/*.cpp", recursive=True)

    # includes
    toolbox_includes = copy.deepcopy(common_includes)
    toolbox_includes += [ f'{root_dir}/third_parties/pybind11/include' ]
    toolbox_includes += python_includes

    # ldflags
    toolbox_ldflags = copy.deepcopy(common_ldflags)
    toolbox_ldflags += python_ldflags

    # cflags
    toolbox_cflags = copy.deepcopy(common_cflags)
    toolbox_cflags += python_cflags

    product_path, ok = build_with_meson(
        name = "gwatch_toolbox",
        sources=toolbox_sources,
        includes=toolbox_includes,
        ldflags=toolbox_ldflags,
        cflags=toolbox_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )
    return "pybind_lib", product_path, ok


__all__ = [
    "build_libgwatch_toolbox"
]
