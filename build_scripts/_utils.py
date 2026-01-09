import os
import sys
import subprocess
import sysconfig
from typing import List, Tuple, Dict
import json
import shutil


def execute_command(cmd:List[str], title="", env=None, cwd=None) -> Tuple[str,str,bool]:
    stdout = ""
    stderr = ""
    retstatus = False

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd, check=True)
        stdout = result.stdout
        stderr = result.stderr
        retstatus = True
    except Exception as e:
        stderr = str(e)
        retstatus = False
    finally:
        return stdout, stderr, retstatus


def is_cuda_file(path: str) -> bool:
    valid_ext = ['.cu', '.cuh']
    return os.path.splitext(path)[1] in valid_ext


def build_with_meson(
    name : str,
    sources : List[str],
    includes : List[str],
    ldflags : List[str],
    cflags : List[str],
    root_dir : str = "/root",
    version : str = "0.0.1",
    type : str = "lib"
):
    def deduplicate(target_list : List) -> List:
        return list(set(target_list))

    sources = deduplicate(sources)
    includes = deduplicate(includes)
    ldflags = deduplicate(ldflags)
    cflags = deduplicate(cflags)

    print_dict : Dict = {}
    print_dict = {
        "sources": sources,
        "includes": includes,
        "ldflags": ldflags,
        "cflags": cflags
    }
    print(f"building {name}...: {json.dumps(print_dict, indent=4)}")

    # makeup parameters to run bash script
    sources = ",".join(sources)
    includes = ",".join(includes)
    ldflags = ",".join(ldflags)
    cflags = ",".join(cflags)

    # build log path
    log_path = f"{root_dir}/build_log/build_{name}.log"
    if not os.path.exists(f"{root_dir}/build_log"):
        os.makedirs(f"{root_dir}/build_log")

    # check build type
    assert(type == "exe" or type == "lib")

    # makeup command
    cmd=[
        "bash", 
        f"{root_dir}/build_scripts/build_with_meson.sh",
        "--name",       f"{name}",
        "--type",       f"{type}",
        "--log",        f"{log_path}",
        "--version",    f"{version}",
        "--sources",    f"{sources}",
        "--includes",   f"{includes}",
        "--ldflags",    f"{ldflags}",
        "--cflags",     f"{cflags}",
    ]

    # execute
    _, _, ok = execute_command(cmd=cmd, title='building profiler main process')
    if not ok:
        print(f"[FAILED] failed to build {name}, check build log at {log_path}")
    else:
        print(f"[SUCCESS] built {name}")

    product_path : str = ""
    if type == "exe":
        product_path = f"{root_dir}/build_scripts/build/{name}/{name}"
    else:
        product_path = f"{root_dir}/build_scripts/build/{name}/lib{name}.so"

    return product_path, ok


__all__ = [
    "execute_command",
    "build_with_meson",
    "is_cuda_file",
]
