import glob
import copy
from setuptools import setup, Extension, Command, find_packages
from typing import Tuple
from ._utils import *
from ._common import *


# extention
def build_libgwatch_scheduler(opt: _BuildOptions) -> Tuple[str,str,bool]:
    # sources
    scheduler_sources = copy.deepcopy(common_sources)
    scheduler_sources += glob.glob(f"{root_dir}/src/scheduler/**/*.cpp", recursive=True)

    # includes
    scheduler_includes = copy.deepcopy(common_includes)
    scheduler_includes += [ f'{root_dir}/third_parties/pybind11/include' ]

    # ldflags
    scheduler_ldflags = copy.deepcopy(common_ldflags)
    scheduler_ldflags += python_ldflags
    scheduler_ldflags += [ "-lcurl" ]

    # cflags
    scheduler_cflags = copy.deepcopy(common_cflags)
    scheduler_cflags += python_cflags

    product_path, ok = build_with_meson(
        name = "gwatch_scheduler",
        sources=scheduler_sources,
        includes=scheduler_includes,
        ldflags=scheduler_ldflags,
        cflags=scheduler_cflags,
        root_dir=root_dir,
        version=common_version,
        type="lib"
    )
    return "pybind_lib", product_path, ok


__all__ = [
    "build_libgwatch_scheduler"
]
