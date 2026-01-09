import glob

import os
from ._utils import *
from ._common import *

current_file = os.path.abspath(__file__)
current_dir = os.path.dirname(current_file)


global backend_dir
backend_dir = f"{build_backend}_impl"


global gwatch_common_headers, gwatch_backend_common_headers
# base
gwatch_common_headers = []
gwatch_common_headers += glob.glob(
    f"{current_dir}/../include/gwatch/*.h", recursive=False
)
gwatch_common_headers += glob.glob(
    f"{current_dir}/../include/gwatch/*.hpp", recursive=False
)
# backend
gwatch_backend_common_headers = []
gwatch_backend_common_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/*.h", recursive=False
)
gwatch_backend_common_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/*.hpp", recursive=False
)


global gwatch_capsule_headers, gwatch_backend_capsule_headers
# base
gwatch_capsule_headers = []
gwatch_capsule_headers += glob.glob(
    f"{current_dir}/../include/gwatch/capsule/*.h", recursive=False
)
gwatch_capsule_headers += glob.glob(
    f"{current_dir}/../include/gwatch/capsule/*.hpp", recursive=False
)
# backend
gwatch_backend_capsule_headers = []
gwatch_backend_capsule_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/capsule/*.h", recursive=False
)
gwatch_backend_capsule_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/capsule/*.hpp", recursive=False
)


global gwatch_scheduler_headers, gwatch_backend_scheduler_headers
# base
gwatch_scheduler_headers = []
gwatch_scheduler_headers += glob.glob(
    f"{current_dir}/../include/gwatch/scheduler/*.h", recursive=False
)
gwatch_scheduler_headers += glob.glob(
    f"{current_dir}/../include/gwatch/scheduler/*.hpp", recursive=False
)
# backend
gwatch_backend_scheduler_headers = []
gwatch_backend_scheduler_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/scheduler/*.h", recursive=False
)
gwatch_backend_scheduler_headers += glob.glob(
    f"{current_dir}/../include/gwatch/{backend_dir}/scheduler/*.hpp", recursive=False
)


global gwatch_utils_headers
gwatch_utils_headers = []
gwatch_utils_headers += glob.glob(
    f"{current_dir}/../include/gwatch/utils/*.h", recursive=False
)
gwatch_utils_headers += glob.glob(
    f"{current_dir}/../include/gwatch/utils/*.hpp", recursive=False
)


__all__ = [
    "backend_dir",

    "gwatch_utils_headers",

    "gwatch_common_headers",
    "gwatch_backend_common_headers",

    "gwatch_scheduler_headers",
    "gwatch_backend_scheduler_headers",

    "gwatch_capsule_headers",
    "gwatch_backend_capsule_headers",    
]
