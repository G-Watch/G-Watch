import os
import subprocess
from enum import Enum
import gwatch.libpygwatch as pygwatch
from typing import Dict, List, Literal, Optional, Any


class Fatbin:
    def __init__(self, *args, **kwargs):
        self._gw_instance = pygwatch.Fatbin()

    @property
    def params(self) -> pygwatch.FatbinParams:
        return self._gw_instance.params

    def fill(self, file_path: str):
        self._gw_instance.fill(file_path)

    def parse(self):
        self._gw_instance.parse()


__all__ = [
    "Fatbin"
]
