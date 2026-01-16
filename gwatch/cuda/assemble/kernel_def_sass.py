import gwatch.libpygwatch as pygwatch
from typing import List, Any, Dict


class KernelDefSASSParams:
    def __init__(self, gw_instance: pygwatch.KernelDefSASSParams):
        self._gw_instance = gw_instance

    @property
    def binary_cuda_cubin(self) -> Any:
        return self._gw_instance.binary_cuda_cubin

    @binary_cuda_cubin.setter
    def binary_cuda_cubin(self, val: Any):
        self._gw_instance.binary_cuda_cubin = val

    @property
    def arch_version(self) -> str:
        return self._gw_instance.arch_version

    @arch_version.setter
    def arch_version(self, val: str):
        self._gw_instance.arch_version = val

    def reset(self):
        self._gw_instance.reset()


class KernelDefSASS:
    def __init__(self, gw_instance: pygwatch.KernelDefSASS):
        self._gw_instance = gw_instance

    @property
    def params(self) -> KernelDefSASSParams:
        return KernelDefSASSParams(self._gw_instance.params)

    @property
    def mangled_prototype(self) -> str:
        return self._gw_instance.mangled_prototype

    @property
    def list_param_sizes(self) -> List[int]:
        return self._gw_instance.list_param_sizes

    @property
    def list_param_sizes_reversed(self) -> List[int]:
        return self._gw_instance.list_param_sizes_reversed

    @property
    def list_param_offsets_reversed(self) -> List[int]:
        return self._gw_instance.list_param_offsets_reversed

    @property
    def list_instructions(self) -> List[Any]:
        return self._gw_instance.list_instructions

    @property
    def map_pc_to_instruction(self) -> Dict[int, Any]:
        return self._gw_instance.map_pc_to_instruction

    @property
    def list_basic_blocks(self) -> List[Any]:
        return self._gw_instance.list_basic_blocks

    def get_list_instruction(self) -> List[Any]:
        return self._gw_instance.get_list_instruction()


__all__ = [
    "KernelDefSASSParams",
    "KernelDefSASS"
]
