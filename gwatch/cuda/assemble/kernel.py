import gwatch.libpygwatch as pygwatch
from .kernel_def_sass import KernelDefSASS

class KernelCUDA:
    def __init__(self, gw_instance: pygwatch.KernelCUDA):
        self._gw_instance = gw_instance

    @property
    def grid_dim_x(self) -> int:
        return self._gw_instance.grid_dim_x

    @grid_dim_x.setter
    def grid_dim_x(self, val: int):
        self._gw_instance.grid_dim_x = val

    @property
    def grid_dim_y(self) -> int:
        return self._gw_instance.grid_dim_y

    @grid_dim_y.setter
    def grid_dim_y(self, val: int):
        self._gw_instance.grid_dim_y = val

    @property
    def grid_dim_z(self) -> int:
        return self._gw_instance.grid_dim_z

    @grid_dim_z.setter
    def grid_dim_z(self, val: int):
        self._gw_instance.grid_dim_z = val

    @property
    def block_dim_x(self) -> int:
        return self._gw_instance.block_dim_x

    @block_dim_x.setter
    def block_dim_x(self, val: int):
        self._gw_instance.block_dim_x = val

    @property
    def block_dim_y(self) -> int:
        return self._gw_instance.block_dim_y

    @block_dim_y.setter
    def block_dim_y(self, val: int):
        self._gw_instance.block_dim_y = val

    @property
    def block_dim_z(self) -> int:
        return self._gw_instance.block_dim_z

    @block_dim_z.setter
    def block_dim_z(self, val: int):
        self._gw_instance.block_dim_z = val

    @property
    def shared_mem_bytes(self) -> int:
        return self._gw_instance.shared_mem_bytes

    @shared_mem_bytes.setter
    def shared_mem_bytes(self, val: int):
        self._gw_instance.shared_mem_bytes = val

    @property
    def stream(self) -> int:
        return self._gw_instance.stream

    @stream.setter
    def stream(self, val: int):
        self._gw_instance.stream = val

    @property
    def definition(self) -> KernelDefSASS:
        # Access 'def' property from C++ binding which is a reserved keyword in Python
        return KernelDefSASS(getattr(self._gw_instance, 'def'))

__all__ = [
    "KernelCUDA"
]
