import os
import glob
import copy
import pathlib
from setuptools import setup, Extension, Command, find_packages
from ._utils import *
from ._common import *
from typing import Tuple, List


def build_libgwatch_instrumentation(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # ==================== Library ====================
    # sources
    instrumentation_sources = copy.deepcopy(common_sources)
    instrumentation_sources += glob.glob(f"{root_dir}/src/instrumentation/*.cpp", recursive=False)
    if build_backend == "cuda":
        instrumentation_sources += glob.glob(f"{root_dir}/src/instrumentation/cuda_impl/**/*.cpp", recursive=True)

    # includes
    instrumentation_includes = copy.deepcopy(common_includes)

    # ldflags
    instrumentation_ldflags = copy.deepcopy(common_ldflags)

    # cflags
    instrumentation_cflags = copy.deepcopy(common_cflags)

    product_path, ok = build_with_meson(
        name = "gwatch_instrumentation",
        sources=instrumentation_sources,
        includes=instrumentation_includes,
        ldflags=instrumentation_ldflags,
        cflags=instrumentation_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )
    return "lib", product_path, ok


__all__ = [
    "build_libgwatch_instrumentation"
]
