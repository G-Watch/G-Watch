import gwatch.libpygwatch as pygwatch
from typing import List, Any


class KernelDefSASS:
    def __init__(self, gw_instance: pygwatch.KernelDefSASS):
        self._gw_instance = gw_instance

    @property
    def params(self) -> pygwatch.KernelDefSASSParams:
        return self._gw_instance.params

    def get_list_instruction(self) -> List[Any]:
        return self._gw_instance.get_list_instruction()


__all__ = [
    "KernelDefSASS"
]
