import gwatch.libpygwatch as pygwatch


class _GWTraceTask:
    def __init__(self, type: str = "kernel:block_schedule"):
        self._C_instance = pygwatch.create_trace_task(type)


    # def __del__(self):
    #     pygwatch.destory_trace_task(self._C_instance)


    def set_metadata(self, name, value):
        if self._C_instance is not None:
            self._C_instance.set_metadata(name, value)


    def get_metadata(self, name) -> any:
        if self._C_instance is not None:
            return self._C_instance.get_metadata(name)
        else:
            return None


__all__ = [
    "_GWTraceTask"
]
