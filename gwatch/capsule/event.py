import gwatch.libpygwatch as pygwatch


class GWEvent:
    def __init__(self, name: str):
        self._C_instance = pygwatch.GWEvent(name)


    def record_tick(self, tick_name: str):
        if self._C_instance is not None:
            self._C_instance.record_tick(tick_name)


    def set_metadata(self, key: str, value: object):
        if self._C_instance is not None:
            self._C_instance.set_metadata(key, value)


    def archive(self):
        if self._C_instance is not None:
            self._C_instance.archive()


    def is_archived(self):
        if self._C_instance is not None:
            return self._C_instance.is_archived()
        else:
            return False
