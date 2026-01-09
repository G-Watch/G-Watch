import gwatch.libpygwatch as pygwatch
from typing import List, Any
from enum import Enum

class CubinParseContent(Enum):
    # These values need to match gw_cuda_cubin_parse_content_t in C++
    # Assuming standard enum mapping, but should verify if they are exposed via pybind11
    # If not exposed, we might need to use int or strings if bound that way.
    # For now, relying on what's likely exposed or passing raw values if needed.
    # But since the C++ binding expects gw_cuda_cubin_parse_content_t, 
    # we should check if that enum is bound.
    pass

class BinaryUtility:
    @staticmethod
    def demangle(mangled_name: str) -> str:
        return pygwatch.BinaryUtility.demangle(mangled_name)

    @staticmethod
    def parse_fatbin(fatbin_file_path: str, dump_cubin_path: str = ""):
        return pygwatch.BinaryUtility.parse_fatbin(fatbin_file_path, dump_cubin_path)

    @staticmethod
    def parse_cubin(
        cubin_file_path: str, 
        list_target_kernel: List[str], 
        list_export_content: List[Any], # using Any for enum list for now
        export_directory: str
    ):
        return pygwatch.BinaryUtility.parse_cubin(
            cubin_file_path, 
            list_target_kernel, 
            list_export_content, 
            export_directory
        )

    @staticmethod
    def parse_ptx(
        ptx_file_path: str, 
        list_target_kernel: List[str], 
        export_directory: str
    ):
        return pygwatch.BinaryUtility.parse_ptx(
            ptx_file_path, 
            list_target_kernel, 
            export_directory
        )

__all__ = [
    "BinaryUtility"
]

