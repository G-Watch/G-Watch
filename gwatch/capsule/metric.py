import sys
from abc import ABC
import inspect
import hashlib

import gwatch.libpygwatch as pygwatch


class GWAppMetric(ABC):
    def __init__(self, name : str):
        self._name : str = name
        self._begin_hash : int = None
        self._end_hash : int = None
        self._start_line_position : str = None
        self._end_line_position : str = None
    
        # calculate unique hash for the metric based on its code position
        frame = inspect.currentframe().f_back
        self._start_line_position = f"{frame.f_code.co_filename}:{frame.f_lineno}"
        self._begin_hash = self._compute_hash(self._start_line_position)

        # start capturing of the current metric
        pygwatch.start_trace(self._name, self._begin_hash, self._start_line_position)


    def eclipse(self):
        # calculate unique hash for the metric based on its code position
        frame = inspect.currentframe().f_back
        self._end_line_position = f"{frame.f_code.co_filename}:{frame.f_lineno}"
        self._end_hash = self._compute_hash(self._end_line_position)

        # stop capturing of the current metric
        pygwatch.stop_trace(self._begin_hash, self._end_hash, self._end_line_position)


    @staticmethod
    def _compute_hash(identifier : str):
        hash_bytes = hashlib.sha256(identifier.encode()).digest()
        return int.from_bytes(hash_bytes[:8], byteorder='big', signed=False)


    def __hash__(self):
        return self._begin_hash


__all__ = [ "GWAppMetric" ]
