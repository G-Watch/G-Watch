import os
import glob
from ._utils import *


current_file = os.path.abspath(__file__)
current_dir = os.path.dirname(current_file)


global root_dir
root_dir = os.path.dirname(current_dir)


global build_backend
build_backend = "cuda"


global common_version
common_version = "0.0.1"


global common_sources
common_sources = []
common_sources += glob.glob(f"{root_dir}/src/*.cpp", recursive=True)
common_sources += glob.glob(f"{root_dir}/src/common/*.cpp", recursive=True)
common_sources += glob.glob(f"{root_dir}/src/common/assemble/*.cpp", recursive=True)
common_sources += glob.glob(f"{root_dir}/src/common/utils/*.cpp", recursive=True)


global common_includes
common_includes = []
common_includes += [ '/usr/include/eigen3' ]
common_includes += [ f'{root_dir}/src' ]
common_includes += [ f'{root_dir}/third_parties/yaml-cpp/include' ]
common_includes += [ f'{root_dir}/third_parties/json/include' ]
common_includes += [ f'{root_dir}/third_parties/pybind11_json/include' ]
common_includes += [ f'{root_dir}/third_parties/sqlite/build' ]
if build_backend == "cuda":
    common_includes += [ '/usr/local/cuda/include' ]
    common_includes += [ f'{root_dir}/src/common/cuda_impl/cupti/extensions/include/c_util' ]
    common_includes += [ f'{root_dir}/src/common/cuda_impl/cupti/extensions/include/profilerhost_util' ]


global python_includes
python_includes, _, ok = execute_command(cmd=["python3-config", "--includes"])
if ok == False:
    raise RuntimeError(f"failed to includes of python3 via python3-config")
python_includes = python_includes.split()
python_includes = [include[2:] if include.startswith("-I") else include for include in python_includes]


global common_ldflags
common_ldflags = []
common_ldflags += [ '-lelf', '-ldwarf', '-lwebsockets' ]
common_ldflags += [ '-L' + root_dir + '/src/dark', '-lgwatch_dark' ]
common_ldflags += [ '-L' + root_dir + '/third_parties/yaml-cpp/build', '-lyaml-cpp' ]
common_ldflags += [ '-L' + root_dir + '/third_parties/sqlite/build', '-lsqlite3' ]


global python_ldflags
python_ldflags, _, ok = execute_command(cmd=["python3-config", "--ldflags"])
if ok == False:
    raise RuntimeError(f"failed to ldflags of python3 via python3-config")
python_ldflags = python_ldflags.split()
python_ldflags += [ "-lpython3.12" ]    # TODO(zhuobin): we need to adapt to acutal python version


global all_targets
all_targets = [
    "scheduler",
    "capsule",
    "profiler",
    "dark",
    "instrumentation",
    # "gtrace"
]


if build_backend == "cuda":
    import build_scripts._cuda
    common_ldflags += build_scripts._cuda.get_ldflags()
    # it's under /usr/local/cuda/lib64/libcupti.so libnvperf_host.so libnvperf_target.so
    common_ldflags += [
        '-lcupti_static',
        '-lcheckpoint',
        '-lnvperf_host',
        '-lnvperf_target',
        '-lnvidia-ml'
    ]

global common_cflags
common_cflags = []
common_cflags += ['-O3', '-std=c++20']
common_cflags += [ "-DGW_CODE_ROOT_PATH=\"" + root_dir + "\"" ]
common_cflags += [ "-DGW_BUILDING" ]
if build_backend == "cuda":
    import build_scripts._cuda
    common_cflags += build_scripts._cuda.get_cflags()
    common_cflags += [ "-DGW_BACKEND_CUDA" ]


global python_cflags
python_cflags, _, ok = execute_command(cmd=["python3-config", "--cflags"])
if ok == False:
    raise RuntimeError(f"failed to cflags of python3 via python3-config")
python_cflags = python_cflags.split()


class _BuildOptions:
    dev_mode = True

    def print(self):
        print(
            "BUILD OPTIONS:\n"
            f"   -> Dev Mode: {self.dev_mode}"
        )


__all__ = [
    "root_dir",
    "build_backend",
    "common_version",
    "common_sources",
    "common_includes",
    "common_ldflags",
    "common_cflags",
    "python_includes",
    "python_ldflags",
    "python_cflags",
    "all_targets",
    "_BuildOptions"
]
