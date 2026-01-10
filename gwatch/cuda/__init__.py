# load libgwatch_dark.so
import ctypes
import os
import sys

def _load_gwatch_dark():
    # find libgwatch_dark.so in the package root
    current_dir = os.path.dirname(os.path.abspath(__file__))
    gwatch_pkg_root = os.path.dirname(current_dir)
    lib_path = os.path.join(gwatch_pkg_root, 'libgwatch_dark.so')

    if os.path.exists(lib_path):
        try:
            ctypes.CDLL(lib_path, mode=ctypes.RTLD_GLOBAL)
            return
        except OSError as e:
            raise RuntimeError(f"failed to load '{lib_path}': {e}")
    else:
        raise RuntimeError(f"cannot find the shared library '{lib_path}'")


_load_gwatch_dark()
