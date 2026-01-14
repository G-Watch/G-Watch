import gwatch.libpygwatch as pygwatch
from typing import List, Dict
from .profiler import Profiler
from .device import ProfileDevice


class ProfileContext:
    def __init__(self, interactive: bool = False):
        self._gw_instance = pygwatch.ProfileContext(interactive)


    def create_profiler(
        self,
        device_id: int,
        metric_names: List[str],
        profile_mode: str = "range"
    ) -> Profiler:
        """
        Create a profiler with the given mode.

        Args:
            device_id: The ID of the device to profile.
            metric_names: The names of the metrics to profile.
            profile_mode: The mode of the profiler, options: "pm", "pc", "range".
        Returns:
            A profiler object.
        """

        gw_profiler = self._gw_instance.create_profiler(device_id, metric_names, profile_mode)
        return Profiler(gw_instance=gw_profiler)


    def destroy_profiler(
        self,
        profiler: Profiler
    ) -> None:
        """
        Destroy a profiler.
        
        Args:
            profiler: The profiler to destroy.
        """

        self._gw_instance.destroy_profiler(profiler._gw_instance)


    def get_devices(self) -> Dict[int, ProfileDevice]:
        """
        Get the list of available devices for profiling.
        
        Returns:
            A dictionary of device IDs and their corresponding profile devices.
        """

        raw_map = self._gw_instance.get_devices()
        return {k: ProfileDevice(gw_instance=v) for k, v in raw_map.items()}


    def get_clock_freq(self, device_id: int) -> Dict[str, int]:
        """
        Get the clock frequencies for a device.
        
        Args:
            device_id: The ID of the device to get the clock frequencies for.
        Returns:
            A dictionary of clock domains and their corresponding frequencies.
        """

        return self._gw_instance.get_clock_freq(device_id)


__all__ = [
    "ProfileContext"
]
