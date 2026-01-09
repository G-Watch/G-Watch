import os
import sys
import ctypes


# make sure current directory exist in sys.path
gwatch_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
capsule_module_dir = os.path.dirname(os.path.abspath(__file__))
if capsule_module_dir not in sys.path:
    sys.path.append(capsule_module_dir)


# load modules
import gwatch.capsule.metric
try:
    import torch
    import gwatch.capsule.torch_adaptor
except ImportError:
    pass
