import gwatch.libpygwatch as pygwatch
from typing import Optional

class ProfileDevice:
    def __init__(self, device_id: int = None, gw_instance: Optional[pygwatch.ProfileDevice] = None):
        if gw_instance:
            self._gw_instance = gw_instance
        elif device_id is not None:
            self._gw_instance = pygwatch.ProfileDevice(device_id)
        else:
             raise ValueError("Either device_id or gw_instance must be provided")

    def export_metric_properties(self, metric_properties_cache_path: str):
        self._gw_instance.export_metric_properties(metric_properties_cache_path)

__all__ = [
    "ProfileDevice"
]
