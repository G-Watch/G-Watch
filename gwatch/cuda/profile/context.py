import gwatch.libpygwatch as pygwatch
from typing import List, Dict
from .profiler import Profiler
from .device import ProfileDevice

class ProfileContext:
    def __init__(self, interactive: bool = False):
        self._gw_instance = pygwatch.ProfileContext(interactive)

    def create_profiler(self, device_id: int, metric_names: List[str], profiler_mode_str: str = "range") -> Profiler:
        gw_profiler = self._gw_instance.create_profiler(device_id, metric_names, profiler_mode_str)
        return Profiler(gw_instance=gw_profiler)

    def destroy_profiler(self, profiler: Profiler):
        self._gw_instance.destroy_profiler(profiler._gw_instance)

    def get_devices(self) -> Dict[int, ProfileDevice]:
        raw_map = self._gw_instance.get_devices()
        return {k: ProfileDevice(gw_instance=v) for k, v in raw_map.items()}

    def get_clock_freq(self, device_id: int) -> Dict[str, int]:
        return self._gw_instance.get_clock_freq(device_id)

__all__ = [
    "ProfileContext"
]

