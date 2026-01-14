import ctypes
import os


def _is_capsule_hijacked():
    """
    Check whether libgwatch_capsule_hijack.so is loaded.

    Returns:
        bool: True if libgwatch_capsule_hijack.so is loaded, False otherwise.
    """

    try:
        ctypes.CDLL("libgwatch_capsule_hijack.so", mode=os.RTLD_NOLOAD)
        return True
    except OSError:
        return False
