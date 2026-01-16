import gwatch.libpygwatch as pygwatch
from gwatch.cuda.assemble import KernelCUDA, KernelDefSASS
from typing import List, Dict, Optional
from .profiler import Profiler
from .device import ProfileDevice


class ProfileContext:
    def __init__(self, interactive: bool = False):
        self._gw_instance = pygwatch.ProfileContext(interactive)


    def create_profiler(
        self,
        device_id: int,
        metric_names: List[str],
        mode: str = "range"
    ) -> Profiler:
        """
        Create a profiler with the given mode.

        Args:
            device_id: The ID of the device to profile.
            metric_names: The names of the metrics to profile.
            mode: The mode of the profiler, options: "pm", "pc", "range".
        Returns:
            A profiler object.
        """
        if mode not in ["pm", "pc", "range"]:
            raise ValueError(f"Invalid mode: {mode}, please use either 'pm', 'pc', or 'range'")

        gw_profiler = self._gw_instance.create_profiler(device_id, metric_names, mode)
        return Profiler(gw_instance=gw_profiler, mode=mode)


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


    # ======================== Runtime Control ========================
    def start_tracing_kernel_launch(self):
        """
        Start tracing kernel launch.
        """

        pygwatch.start_tracing_kernel_launch()


    def stop_tracing_kernel_launch(self) -> List[KernelCUDA]:
        """
        Stop tracing kernel launch.
        """

        raw_list = pygwatch.stop_tracing_kernel_launch()
        return [KernelCUDA(k) for k in raw_list]


    def get_kernel_def_by_name(self, name: str) -> Optional[KernelDefSASS]:
        """
        Get the kernel definition by name.
        """

        raw_kernel_def = pygwatch.get_kernel_def_by_name(name)

        if raw_kernel_def is None:
            return None

        return KernelDefSASS(raw_kernel_def)


    def get_map_kerneldef(self) -> Dict[str, KernelDefSASS]:
        """
        Get map of kernel definitions.
        """

        raw_map = pygwatch.get_map_kerneldef()
        return {k: KernelDefSASS(v) for k, v in raw_map.items()}

    # ======================== Runtime Control ========================


__all__ = [
    "ProfileContext"
]
