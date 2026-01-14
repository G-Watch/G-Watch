import gwatch
import gwatch.libpygwatch as pygwatch
import json
from typing import Dict, List, Any

# Constants for Range Profiling
GW_CUPTI_RANGE_MODE_AUTO = 0
GW_CUPTI_RANGE_MODE_USER = 1

GW_CUPTI_REPLAY_MODE_KERNEL = 0
GW_CUPTI_REPLAY_MODE_USER = 1


class RangeProfiler:
    def __init__(self, profiler: 'Profiler',
        range_name: str = "default profile range",
        max_launches_per_pass: int = 512,
        max_ranges_per_pass: int = 64,
        cupti_profile_range_mode: int = GW_CUPTI_RANGE_MODE_USER,
        cupti_profile_reply_mode: int = GW_CUPTI_REPLAY_MODE_USER,
        cupti_profile_min_nesting_level: int = 1,
        cupti_profile_num_nesting_levels: int = 1,
        cupti_profile_target_nesting_levels: int = 1
    ):
        self.profiler = profiler
        self.range_name = range_name
        self.max_launches_per_pass = max_launches_per_pass
        self.max_ranges_per_pass = max_ranges_per_pass
        self.cupti_profile_range_mode = cupti_profile_range_mode
        self.cupti_profile_reply_mode = cupti_profile_reply_mode
        self.cupti_profile_min_nesting_level = cupti_profile_min_nesting_level
        self.cupti_profile_num_nesting_levels = cupti_profile_num_nesting_levels
        self.cupti_profile_target_nesting_levels = cupti_profile_target_nesting_levels
        self.metrics = None


    def __enter__(self):
        if gwatch._is_capsule_hijacked():
            self.profiler.reset_counter_data()
            self.profiler.RangeProfile_start_session(
                max_launches_per_pass=self.max_launches_per_pass,
                max_ranges_per_pass=self.max_ranges_per_pass,
                cupti_profile_range_mode=self.cupti_profile_range_mode,
                cupti_profile_reply_mode=self.cupti_profile_reply_mode,
                cupti_profile_min_nesting_level=self.cupti_profile_min_nesting_level,
                cupti_profile_num_nesting_levels=self.cupti_profile_num_nesting_levels,
                cupti_profile_target_nesting_levels=self.cupti_profile_target_nesting_levels
            )
            self.profiler.RangeProfile_enable_profiling()
            self.profiler.RangeProfile_begin_pass()
            if self.cupti_profile_range_mode == GW_CUPTI_RANGE_MODE_USER:
                self.profiler.RangeProfile_push_range(self.range_name)
        else:
            pass
        return self


    def __exit__(self, exc_type, exc_val, exc_tb):
        if gwatch._is_capsule_hijacked():
            if self.cupti_profile_range_mode == GW_CUPTI_RANGE_MODE_USER:
                self.profiler.RangeProfile_pop_range()
            last_pass = self.profiler.RangeProfile_end_pass()
            self.profiler.RangeProfile_flush_data()
            self.profiler.RangeProfile_disable_profiling()
            self.metrics = self.profiler.RangeProfile_get_metrics()
            self.profiler.RangeProfile_destroy_session()
        else:
            pass


class PmSampler:
    def __init__(self, profiler: 'Profiler',
        hw_buf_size: int = 1024 * 1024,
        sampling_interval: int = 0, # 0 means auto
        max_samples: int = 100000
    ):
        self.profiler = profiler
        self.hw_buf_size = hw_buf_size
        self.sampling_interval = sampling_interval
        self.max_samples = max_samples
        self.metrics = None


    def __enter__(self):
        self.profiler.PmSampling_set_config(
            self.hw_buf_size,
            self.sampling_interval,
            self.max_samples
        )
        self.profiler.PmSampling_enable_profiling()
        self.profiler.PmSampling_start_profiling()
        return self


    def __exit__(self, exc_type, exc_val, exc_tb):
        self.profiler.PmSampling_stop_profiling()
        self.profiler.PmSampling_disable_profiling()
        self.metrics = self.profiler.PmSampling_get_metrics()


class PcSampler:
    def __init__(self, profiler: 'Profiler', kernel_def: Any):
        self.profiler = profiler
        self.kernel_def = kernel_def
        self.metrics = None


    def __enter__(self):
        self.profiler.PcSampling_enable_profiling(self.kernel_def)
        self.profiler.PcSampling_start_profiling()
        return self


    def __exit__(self, exc_type, exc_val, exc_tb):
        self.profiler.PcSampling_stop_profiling()
        self.profiler.PcSampling_disable_profiling()
        self.metrics = self.profiler.PcSampling_get_metrics()


class Profiler:
    def __init__(self, gw_instance: pygwatch.Profiler):
        self._gw_instance = gw_instance

    # ================== Context manager based APIs ==================
    def RangeProfile(
        self,
        range_name: str = "default profile range",
        max_launches_per_pass: int = 512,
        max_ranges_per_pass: int = 64,
        cupti_profile_range_mode: int = GW_CUPTI_RANGE_MODE_USER,
        cupti_profile_reply_mode: int = GW_CUPTI_REPLAY_MODE_USER,
        cupti_profile_min_nesting_level: int = 1,
        cupti_profile_num_nesting_levels: int = 1,
        cupti_profile_target_nesting_levels: int = 1
    ) -> RangeProfiler:
        """
        Create a range profiler context manager with the given name.

        Args:
            range_name: The name of the range to profile.
            max_launches_per_pass: The maximum number of launches per pass.
            max_ranges_per_pass: The maximum number of ranges per pass.
            cupti_profile_range_mode: The mode of the range profiling.
            cupti_profile_reply_mode: The mode of the range profiling reply.
            cupti_profile_min_nesting_level: The minimum nesting level.
            cupti_profile_num_nesting_levels: The number of nesting levels.
            cupti_profile_target_nesting_levels: The target nesting levels.
        """

        return RangeProfiler(
            self,
            range_name=range_name,
            max_launches_per_pass=max_launches_per_pass,
            max_ranges_per_pass=max_ranges_per_pass,
            cupti_profile_range_mode=cupti_profile_range_mode,
            cupti_profile_reply_mode=cupti_profile_reply_mode,
            cupti_profile_min_nesting_level=cupti_profile_min_nesting_level,
            cupti_profile_num_nesting_levels=cupti_profile_num_nesting_levels,
            cupti_profile_target_nesting_levels=cupti_profile_target_nesting_levels
        )


    def PmSampling(
        self,
        hw_buf_size: int = 1024 * 1024,
        sampling_interval: int = 0,
        max_samples: int = 100000
    ) -> PmSampler:
        """
        Create a Performance Monitoring (PM) sampling context manager.
        
        Args:
            hw_buf_size: The size of the hardware buffer.
            sampling_interval: The sampling interval.
            max_samples: The maximum number of samples.
        Returns:
            A PM sampling context manager object.
        """

        return PmSampler(
            self,
            hw_buf_size=hw_buf_size,
            sampling_interval=sampling_interval,
            max_samples=max_samples
        )


    def PcSampling(self, kernel_def: Any) -> PcSampler:
        """
        Create a Program Counter (PC) sampling context manager.

        Args:
            kernel_def: The kernel definition to profile.
        Returns:
            A Program Counter (PC) sampling context manager object.
        """

        return PcSampler(self, kernel_def)
    # ================== Context manager based APIs ==================


    # ================== Easy-to-use APIs ==================
    def start_range_profile(
        self,
        range_name: str = "default profile range",
        max_launches_per_pass: int = 512,
        max_ranges_per_pass: int = 64,
        cupti_profile_range_mode: int = GW_CUPTI_RANGE_MODE_USER,
        cupti_profile_reply_mode: int = GW_CUPTI_REPLAY_MODE_USER,
        cupti_profile_min_nesting_level: int = 1,
        cupti_profile_num_nesting_levels: int = 1,
        cupti_profile_target_nesting_levels: int = 1
    ):
        """
        Start a range profiling context.
        """
    
        self._range_profile_config = {
            "range_name": range_name,
            "cupti_profile_range_mode": cupti_profile_range_mode
        }

        self.reset_counter_data()
        self.RangeProfile_start_session(
            max_launches_per_pass=max_launches_per_pass,
            max_ranges_per_pass=max_ranges_per_pass,
            cupti_profile_range_mode=cupti_profile_range_mode,
            cupti_profile_reply_mode=cupti_profile_reply_mode,
            cupti_profile_min_nesting_level=cupti_profile_min_nesting_level,
            cupti_profile_num_nesting_levels=cupti_profile_num_nesting_levels,
            cupti_profile_target_nesting_levels=cupti_profile_target_nesting_levels
        )
        self.RangeProfile_enable_profiling()
        self.RangeProfile_begin_pass()
        if cupti_profile_range_mode == GW_CUPTI_RANGE_MODE_USER:
            self.RangeProfile_push_range(range_name)


    def stop_range_profile(self) -> Dict[str, Dict[str, float]]:
        """
        Stop the range profiling context and return metrics.
        """
    
        if not hasattr(self, '_range_profile_config'):
             raise RuntimeError("Range profiling was not started. Please call start_range_profile() first.")
        
        config = self._range_profile_config
        
        if config["cupti_profile_range_mode"] == GW_CUPTI_RANGE_MODE_USER:
            self.RangeProfile_pop_range()
            
        self.RangeProfile_end_pass()
        self.RangeProfile_flush_data()
        self.RangeProfile_disable_profiling()
        metrics = self.RangeProfile_get_metrics()
        self.RangeProfile_destroy_session()
        
        del self._range_profile_config
        return metrics


    # ================== Low-level APIs ==================
    # Range Profile APIs
    def RangeProfile_start_session(
        self, 
        max_launches_per_pass: int, 
        max_ranges_per_pass: int, 
        cupti_profile_range_mode: int, 
        cupti_profile_reply_mode: int, 
        cupti_profile_min_nesting_level: int, 
        cupti_profile_num_nesting_levels: int, 
        cupti_profile_target_nesting_levels: int
    ):
        self._gw_instance.RangeProfile_start_session(
            max_launches_per_pass,
            max_ranges_per_pass,
            cupti_profile_range_mode,
            cupti_profile_reply_mode,
            cupti_profile_min_nesting_level,
            cupti_profile_num_nesting_levels,
            cupti_profile_target_nesting_levels
        )

    def RangeProfile_destroy_session(self):
        self._gw_instance.RangeProfile_destroy_session()

    def RangeProfile_is_session_created(self) -> bool:
        return self._gw_instance.RangeProfile_is_session_created()

    def RangeProfile_begin_pass(self):
        self._gw_instance.RangeProfile_begin_pass()

    def RangeProfile_end_pass(self) -> bool:
        return self._gw_instance.RangeProfile_end_pass()

    def RangeProfile_enable_profiling(self):
        self._gw_instance.RangeProfile_enable_profiling()

    def RangeProfile_disable_profiling(self):
        self._gw_instance.RangeProfile_disable_profiling()

    def RangeProfile_push_range(self, range_name: str):
        self._gw_instance.RangeProfile_push_range(range_name)

    def RangeProfile_pop_range(self):
        self._gw_instance.RangeProfile_pop_range()

    def RangeProfile_flush_data(self):
        self._gw_instance.RangeProfile_flush_data()

    def RangeProfile_get_metrics(self) -> Dict[str, Dict[str, float]]:
        return self._gw_instance.RangeProfile_get_metrics()

    # PM Sampling APIs
    def PmSampling_enable_profiling(self):
        self._gw_instance.PmSampling_enable_profiling()

    def PmSampling_disable_profiling(self):
        self._gw_instance.PmSampling_disable_profiling()

    def PmSampling_set_config(self, hw_buf_size: int, sampling_interval: int, max_samples: int):
        self._gw_instance.PmSampling_set_config(hw_buf_size, sampling_interval, max_samples)

    def PmSampling_start_profiling(self):
        self._gw_instance.PmSampling_start_profiling()

    def PmSampling_stop_profiling(self):
        self._gw_instance.PmSampling_stop_profiling()

    def PmSampling_get_metrics(self) -> List[Any]:
        # C++ returns a JSON string, so we parse it
        json_str = self._gw_instance.PmSampling_get_metrics()
        return json.loads(json_str)

    # PC Sampling APIs
    def PcSampling_enable_profiling(self, kernel_def: Any):
        # Assumes kernel_def has a underlying _gw_instance or passed as is if binding supports it
        # Since binding is missing in C++, this will fail if called.
        # But if we assume binding accepts the python object which wraps the C++ object:
        if hasattr(kernel_def, '_gw_instance'):
             self._gw_instance.PcSampling_enable_profiling(kernel_def._gw_instance)
        else:
             self._gw_instance.PcSampling_enable_profiling(kernel_def)

    def PcSampling_disable_profiling(self):
        self._gw_instance.PcSampling_disable_profiling()

    def PcSampling_start_profiling(self):
        self._gw_instance.PcSampling_start_profiling()

    def PcSampling_stop_profiling(self):
        self._gw_instance.PcSampling_stop_profiling()

    def PcSampling_get_metrics(self) -> Dict[int, Dict[int, int]]:
         # Returns map<uint64_t, std::map<uint32_t, uint64_t>>
         return self._gw_instance.PcSampling_get_metrics()

    # Checkpoint/Restore APIs
    def checkpoint(self):
        self._gw_instance.checkpoint()

    def restore(self, do_pop: bool):
        self._gw_instance.restore(do_pop)

    def free_checkpoint(self):
        self._gw_instance.free_checkpoint()

    # Common APIs
    def reset_counter_data(self):
        self._gw_instance.reset_counter_data()
    # ================== Low-level APIs ==================


__all__ = [
    "Profiler",
    "RangeProfiler",
    "PmSampler",
    "PcSampler",
    "GW_CUPTI_RANGE_MODE_AUTO",
    "GW_CUPTI_RANGE_MODE_USER",
    "GW_CUPTI_REPLAY_MODE_KERNEL",
    "GW_CUPTI_REPLAY_MODE_USER"
]
