import os
import glob
import shutil
from typing import Tuple, Optional
from setuptools import setup, Extension, Command, find_packages

from ._utils import *
from ._common import *


def build_and_install_gtrace(opt: _BuildOptions) -> Tuple[str,str,bool]:
    def __find_npm() -> Optional[str]:
        candidates = glob.glob("/root/.nvm/versions/node/v*/bin/npm")
        # select the latest one
        candidates.sort(reverse=True)
        for path in candidates:
            if os.access(path, os.X_OK):
                return path
        return shutil.which("npm")

    def __build_and_install_plugin(plugin_name: str) -> bool:
        npm_path = __find_npm()
        if not npm_path:
            print(f"[FAILED] failed to find npm when installing {plugin_name}")
            return False

        # install dependencies of the plugin
        _, stderr, ok = execute_command(
            cmd=[npm_path, "install"],
            cwd=f"/root/gtrace/plugins/{plugin_name}",
            title=f"installing dependencies of gtrace plugin {plugin_name}"
        )
        if ok == False:
            print(f"[FAILED] failed to install dependencies of gtrace plugin {plugin_name}: error({stderr})")
            return ok

        # build plugin
        _, stderr, ok = execute_command(
            cmd=[npm_path, "run", "build"],
            cwd=f"/root/gtrace/plugins/{plugin_name}",
            title=f"building gtrace plugin {plugin_name}"
        )
        if ok == False:
            print(f"[FAILED] failed to build gtrace plugin {plugin_name}: error({stderr})")
            return ok

        # remove any previous installation
        execute_command(cmd=["rm", "-rf", f"/usr/share/grafana/data/plugins/{plugin_name}"])

        # create plugin directory if needed
        execute_command(cmd=["mkdir", "-p", "/usr/share/grafana/data/plugins"])

        # install plugin
        install_cmd = []
        if opt.dev_mode:
            install_cmd = [
                "ln", "-s",
                f"/root/gtrace/plugins/{plugin_name}",
                f"/usr/share/grafana/data/plugins/{plugin_name}"
            ]
        else:
            install_cmd = [
                "cp", "-r",
                f"/root/gtrace/plugins/{plugin_name}/dist",
                f"/usr/share/grafana/data/plugins/{plugin_name}"
            ]
        _, stderr, ok = execute_command(
            cmd=install_cmd,
            title=f"installing gtrace plugin {plugin_name}"
        )
        if ok == False:
            print(
                f"[FAILED] failed to install gtrace plugin {plugin_name}: "
                f"dev({opt.dev_mode}), error({stderr})"
            )
            return ok

        print(
            f"[SUCCESS] built and installed gtrace plugin {plugin_name}: dev({opt.dev_mode})"
        )
        return ok

    # install grafana.ini file
    _, stderr, ok = execute_command(
        cmd=["cp", "/root/gtrace/grafana.ini", "/etc/grafana/grafana.ini"],
        title=f"copying grafana config file"
    )
    if ok == False:
        print(f"failed to copy grafana config file: error({stderr})")
        return "", "", False
    print(f"[SUCCESS] copied grafana config file")

    # build and install plugins
    ok_datasource   = __build_and_install_plugin("gwatch-main-datasource")
    ok_panel        = __build_and_install_plugin("gwatch-systemtrace-panel")
    # NOTE(zhuobin): put app plugin at the final since it depends on the above plugins
    ok_app          = __build_and_install_plugin("gwatch-controller-app")

    if ok_app == False or ok_datasource == False or ok_panel == False:
        return "", "", False
    else:
        return "", "", True


__all__ = [
    "build_and_install_gtrace"
]
