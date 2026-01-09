import os
from ._utils import *

current_file = os.path.abspath(__file__)
current_dir = os.path.dirname(current_file)


def build_third_parties():
    print("building pybind11...")
    build_pybind_11, stderr, ok = execute_command(cmd=["bash", f"{current_dir}/_build_pybind11_json.sh"], title='building pybind11 and json support')
    if ok == False:
        raise RuntimeError(f"failed to build pybind11 and json support, stdout: {build_pybind_11}, stderr: {stderr}")
    print("built pybind11 and json support")

    print("building yamlcpp...")
    build_yamlcpp, stderr, ok = execute_command(cmd=["bash", f"{current_dir}/_build_yamlcpp.sh"], title='building yaml-cpp')
    if ok == False:
        raise RuntimeError(f"failed to build yaml-cpp, stdout: {build_yamlcpp}, stderr: {stderr}")
    print("built yaml-cpp")

    print("building sqlite...")
    build_sqlite, stderr, ok = execute_command(cmd=["bash", f"{current_dir}/_build_sqlite.sh"], title='building sqlite')
    if ok == False:
        raise RuntimeError(f"failed to build sqlite, stdout: {build_sqlite}, stderr: {stderr}")
    print("built sqlite")

    print("installing nodejs...")
    build_sqlite, stderr, ok = execute_command(cmd=["bash", f"{current_dir}/_install_nodejs.sh"], title='installing nodejs')
    if ok == False:
        raise RuntimeError(f"failed to install nodejs, stdout: {build_sqlite}, stderr: {stderr}")
    print("installed nodejs")

    # print("installing grafana...")
    # build_sqlite, stderr, ok = execute_command(cmd=["bash", f"{current_dir}/_install_grafana.sh"], title='installing grafana')
    # if ok == False:
    #     raise RuntimeError(f"failed to install grafana, stdout: {build_sqlite}, stderr: {stderr}")
    # print("installed grafana")


__all__ = [
    "build_third_parties"
]
