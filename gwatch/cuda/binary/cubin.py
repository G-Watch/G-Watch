import os
import subprocess
from enum import Enum
import gwatch.libpygwatch as pygwatch
from typing import Dict, List, Literal, Optional, Any

from gwatch.cuda.assemble.kernel_def_sass import KernelDefSASS

class Cubin:
    def __init__(self, *args, **kwargs):
        self._gw_instance = pygwatch.Cubin()

    @property
    def params(self) -> pygwatch.CubinParams:
        return self._gw_instance.params

    def fill(self, file_path: str):
        self._gw_instance.fill(file_path)

    def parse(self):
        self._gw_instance.parse()

    def get_kerneldef_by_name(self, kernel_name: str) -> pygwatch.KernelDefSASS:
        return self._gw_instance.get_kerneldef_by_name(kernel_name)

    def get_map_kernel_def(self) -> Dict[str, pygwatch.KernelDefSASS]:
        return self._gw_instance.get_map_kernel_def()

    def export_parse_result(self, list_target_kernel: List[str], list_export_content: List[str], export_directory: str):
        self._gw_instance.export_parse_result(list_target_kernel, list_export_content, export_directory)


__all__ = [
    "Cubin"
]
