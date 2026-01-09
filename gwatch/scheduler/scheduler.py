import os
import argparse
import yaml
import re
from typing import Literal, Callable, List, Any, Set
from loguru import logger
import gwatch.config as config


import gwatch.libgwatch_scheduler as _C_scheduler


current_file_path = os.path.abspath(__file__)
gwatch_dir_path = os.path.dirname(os.path.dirname(current_file_path))


class GWScheduler:

    _gwatch_scheduler = None

    def __new__(
        cls,
        scheduler_config : _C_scheduler.GWSchedulerConfig,
        backend : Literal["cuda", "rocm"],
        world_size : int = 1,
        command : List[str] = [],
        options : Set[str] = {}
    ):
        # create single instance if not created
        if config.SI_gw_scheduler == None:
            config.SI_gw_scheduler = super(GWScheduler, cls).__new__(cls)
            assert(cls._gwatch_scheduler == None)

            # assign python package path here
            scheduler_config.COMMON_python_package_installtion_path = gwatch_dir_path

            # start scheduler backend
            if backend == "cuda":
                cls._gwatch_scheduler = _C_scheduler.GWScheduler(scheduler_config)
            else:
                raise NotImplementedError(f"{backend} backend is not implemented yet")

        # return single instance
        return config.SI_gw_scheduler


    def __init__(
        self,
        scheduler_config : _C_scheduler.GWSchedulerConfig,
        backend : Literal["cuda", "rocm"],
        world_size : int = 1,
        command : List[str] = [],
        options : Set[str] = set()
    ):
        self._backend : str = backend
        self._world_size : int = world_size
        self._command : str = command
        self._options : Set[str] = options
        self._scheduler_config : _C_scheduler.GWSchedulerConfig = scheduler_config

        # should be created in __new__
        assert(self._gwatch_scheduler != None)


    def serve(self):
        self._gwatch_scheduler.serve()


    def start_capsule(self):
        self._gwatch_scheduler.start_capsule(self._command, self._options)

        # block until world size is match!
        while(self._gwatch_scheduler.get_capsule_world_size() < self._world_size):
            continue


    def start_gtrace(self):
        self._gwatch_scheduler.start_gtrace()


    @staticmethod
    def parse_cli_args() -> argparse.Namespace:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '-b', '--backend',
            type=str,
            default='cuda',
            help='backend to use',
            choices=['cuda', 'rocm'],
            required=False
        )
        parser.add_argument(
            '-w', '--world-size',
            type=str,
            default=1,
            help='world size (#processes) of the program to be profiled',
            required=False
        )
        parser.add_argument(
            '-c', '--config',
            type=str,
            default='./gwatch.yaml',
            help='config file path',
            required=False
        )
        parser.add_argument(
            '-o', '--options',
            type=str,
            default='',
            help='options to start the capsule',
            required=False
        )
        parser.add_argument(
            'command',
            type=str,
            nargs=argparse.REMAINDER,
            help='command to be profiled'
        )
        return parser.parse_args()
    

    @staticmethod
    def parse_yaml_config(config_filepath: str) -> _C_scheduler.GWSchedulerConfig:
        def norm_key(k: str) -> str:
            return re.sub(r'[^0-9a-zA-Z_]', '_', k)

        def assign(cfg_obj: Any, section: str, key: str, value: Any):
            attr_name = f"{section.upper()}_{norm_key(key)}" if section else norm_key(key)
            if hasattr(cfg_obj, attr_name):
                old = getattr(cfg_obj, attr_name)
                try:
                    if isinstance(old, bool):
                        if isinstance(value, str):
                            v = value.lower() in ("1", "true", "yes", "on")
                        else:
                            v = bool(value)
                    else:
                        v = type(old)(value)
                except Exception:
                    v = value
                setattr(cfg_obj, attr_name, v)
            else:
                try:
                    setattr(cfg_obj, attr_name, value)
                except Exception:
                    logger.warning(
                        f"no attribute {attr_name} exists in scheduler, "
                        f"try assigned with value {value}"
                    )

        def walk_and_assign(cfg_obj: Any, data: Any, section: str = ""):
            if not isinstance(data, dict):
                return
            for k, v in data.items():
                if isinstance(v, dict):
                    # treat k as a new section prefix
                    new_section = k if not section else f"{section}_{k}"
                    walk_and_assign(cfg_obj, v, new_section)
                else:
                    assign(cfg_obj, section if section else k if isinstance(v, dict) else section, k, v)

        cfg = _C_scheduler.GWSchedulerConfig()
        try:
            with open(config_filepath, "r", encoding="utf-8") as f:
                yaml_data = yaml.safe_load(f) or {}

            for top_k, top_v in yaml_data.items():
                if isinstance(top_v, dict):
                    walk_and_assign(cfg, top_v, top_k)
                else:
                    assign(cfg, "", top_k, top_v)
        except Exception:
            logger.warn(f"failed to load yaml config file from {config_filepath}, use default config")

        return cfg


__all__ = [ "GWScheduler" ]
