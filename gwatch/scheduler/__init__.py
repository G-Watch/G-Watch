# load close-source portion of G-Watch
import ctypes
from ctypes.util import find_library
lib_name = "gwatch_dark"
lib_path = find_library(lib_name)
if lib_path is None:
    raise RuntimeError(f"cannot find the shared library '{lib_name}'")
try:
    ctypes.CDLL(lib_path)
except OSError as e:
    raise RuntimeError(f"failed to load '{lib_path}': {e}")

from .scheduler import *
