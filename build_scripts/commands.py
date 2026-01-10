import os
import shutil
import glob
import threading
import stat
import shutil
import glob
from typing import List, Callable, Any, Tuple
from packaging import version

from setuptools import Command
from setuptools.command.build import build as _build
from setuptools.command.install import install as _install

from ._common import *
from ._utils import *

from .build_3rd import *
from .ext_libgwatch_capsule import *
from .ext_libgwatch_binding import *
from .ext_libgwatch_scheduler import *
from .ext_libgwatch_profiler import *
from .ext_libgwatch_instrumentation import *
from .ext_gtrace import *


# command for building G-Watch & gTrace
class BuildCommand(_build):
    description = "Build all products"
    user_options = [
        ('dev', None, 'Build with dev mode'),
        ('verbose', None, 'Build with verbose mode'),
        ('targets=', None, 'Targets to be built'),
    ]

    class _BuildThread(threading.Thread):
        def __init__(self, target: Callable, args=(), kwargs=None):
            super().__init__(target=target, args=args, kwargs=kwargs or {})
            self.result: Any = None

        def run(self) -> None:
            try:
                if self._target:
                    self.result = self._target(*self._args, **self._kwargs)
            finally:
                del self._target, self._args, self._kwargs

    def initialize_options(self):
        self.dev = False
        self.targets = all_targets
        super().initialize_options()

    def finalize_options(self):
        super().finalize_options()
        if self.targets and type(self.targets) == str:
            if self.targets == "all":
                self.targets = all_targets
            else:
                self.targets = [t.strip() for t in self.targets.split(',') if t.strip()]
        else:
            self.targets = all_targets

    def run(self):
        build_tasks : List[Callable] = []

        # gather build tasks
        if "scheduler" in self.targets:
            build_tasks += [build_libgwatch_scheduler]
        if "instrumentation" in self.targets:
            build_tasks += [build_libgwatch_instrumentation]
        if "capsule" in self.targets:
            build_tasks += [build_cgwatch, build_pygwatch, build_libgwatch_capsule_hijack]
        if "profiler" in self.targets:
            build_tasks += [build_gwatch_profiler]
        if "gtrace" in self.targets:
            build_tasks += [build_and_install_gtrace]

        # make build options
        opt: _BuildOptions = _BuildOptions()
        opt.dev_mode = self.dev
        opt.print()

        # build third_parties
        build_third_parties()

        # run build tasks
        threads = [BuildCommand._BuildThread(target=task, args=[opt]) for task in build_tasks]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()

        # save build result
        self.build_results = [thread.result for thread in threads]

        # check whether there's any failed
        failed_build = []
        for result in self.build_results:
            if not result or not isinstance(result, Tuple) or len(result) != 3:
                continue
            if not result[2]:
                failed_build.append(result[0])
        if len(failed_build) > 0:
            print(f"some targets built failed, please check their logs")
            exit(-1)
        
        # distribute artifacts
        gwatch_pkg_dir = os.path.join(root_dir, 'gwatch')
        libs_dir = gwatch_pkg_dir
        bin_dir = os.path.join(gwatch_pkg_dir, 'bin')
        
        # copy assets to package root
        assets_src = os.path.join(root_dir, 'assets')
        assets_dst = os.path.join(gwatch_pkg_dir, 'assets')
        if os.path.exists(assets_src):
            if os.path.exists(assets_dst):
                 shutil.rmtree(assets_dst)
            shutil.copytree(assets_src, assets_dst)
        
        if not os.path.exists(bin_dir):
            os.makedirs(bin_dir)

        # add third-party libs
        sqlite_lib = os.path.join(root_dir, 'third_parties/sqlite/build/libsqlite3.so')
        if os.path.exists(sqlite_lib):
            dst = os.path.join(libs_dir, os.path.basename(sqlite_lib))
            shutil.copy(sqlite_lib, dst)
            _, stderr, ok = execute_command(cmd=["patchelf", "--set-rpath", "$ORIGIN", dst], title=f"patching rpath for {os.path.basename(dst)}")
            if not ok:
                print(f"WARNING: Failed to patch RPATH for {dst}: {stderr}")

        # add dark lib
        if "dark" in self.targets:
            dark_lib = os.path.join(root_dir, 'src/dark/libgwatch_dark.so')
            if os.path.exists(dark_lib):
                dst = os.path.join(libs_dir, os.path.basename(dark_lib))
                shutil.copy(dark_lib, dst)
                _, stderr, ok = execute_command(cmd=["patchelf", "--set-rpath", "$ORIGIN", dst], title=f"patching rpath for {os.path.basename(dst)}")
                if not ok:
                    print(f"WARNING: Failed to patch RPATH for {dst}: {stderr}")

        for result in self.build_results:
            if not result or not isinstance(result, Tuple) or len(result) != 3:
                continue
            
            type_tag, src_path, ok = result
            if not ok: continue
            
            filename = os.path.basename(src_path)
            
            if type_tag == 'pybind_lib':
                # pybind libs go to package root
                dst = os.path.join(gwatch_pkg_dir, filename)
                shutil.copyfile(src_path, dst)
                _, stderr, ok = execute_command(cmd=["patchelf", "--set-rpath", "$ORIGIN", dst], title=f"patching rpath for {filename}")
                if not ok:
                    print(f"WARNING: Failed to patch RPATH for {dst}: {stderr}")
            elif type_tag == 'lib':
                # shared libs go to gwatch/libs
                dst = os.path.join(libs_dir, filename)
                shutil.copyfile(src_path, dst)
                # strip shared libs
                execute_command(cmd=["strip", "--strip-unneeded", dst], title=f"stripping {filename}")
                _, stderr, ok = execute_command(cmd=["patchelf", "--set-rpath", "$ORIGIN", dst], title=f"patching rpath for {filename}")
                if not ok:
                    print(f"WARNING: Failed to patch RPATH for {dst}: {stderr}")
            elif type_tag == 'exe':
                # executables go to gwatch/bin
                dst = os.path.join(bin_dir, filename)
                shutil.copyfile(src_path, dst)
                # strip executable
                execute_command(cmd=["strip", "--strip-unneeded", dst], title=f"stripping {filename}")
                # make executable
                st = os.stat(dst)
                os.chmod(dst, st.st_mode | stat.S_IEXEC)

        super().run()


# command for clean built G-Watch & gTrace
class CleanCommand(Command):
    description = "clean all built products"
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        # delete build dir
        if os.path.exists('build'):
            shutil.rmtree('build')
        # delete dist dir
        if os.path.exists('dist'):
            shutil.rmtree('dist')
        # delete gwatch.egg-info dir
        if os.path.exists('gwatch.egg-info'):
            shutil.rmtree('gwatch.egg-info')
        # delete all built products
        if os.path.exists('build_scripts/build'):
            shutil.rmtree('build_scripts/build')
        # delete all built log
        if os.path.exists('build_log'):
            shutil.rmtree('build_log')
        # delete all .so
        for so_file in glob.glob('gwatch/*.so'):
            os.remove(so_file)
        
        # delete libs and bin in gwatch
        # if os.path.exists('gwatch/libs'):
        #     shutil.rmtree('gwatch/libs')
        if os.path.exists('gwatch/bin'):
            shutil.rmtree('gwatch/bin')
        if os.path.exists('gwatch/assets'):
            shutil.rmtree('gwatch/assets')

        # delete all gtrace build
        try:
            shutil.rmtree('gtrace/plugins/gwatch-controller-app/dist')
        except:
            pass
        try:
            shutil.rmtree('gtrace/plugins/gwatch-main-datasource/dist')
        except:
            pass
        try:
            shutil.rmtree('gtrace/plugins/gwatch-systemtrace-panel/dist')
        except:
            pass
        


class InstallCommand(_install):
    description = "Install all products"
    user_options = _install.user_options + [
        ('dev', None, 'Build with dev mode'),
        ('targets=', None, 'Targets to be built'),
    ]

    def initialize_options(self):
        self.dev = False
        self.targets = all_targets
        super().initialize_options()
        

    def finalize_options(self):
        super().finalize_options()
        if self.targets and type(self.targets) == str:
            if self.targets == "all":
                self.targets = all_targets
            else:
                self.targets = [t.strip() for t in self.targets.split(',') if t.strip()]
        else:
            self.targets = all_targets
    
    def run(self):
        # build first
        build_cmd = self.get_finalized_command('build')
        build_cmd.dev = self.dev
        self.run_command('build')
        
        # install
        super().run()


__all__ = [
    "BuildCommand",
    "CleanCommand",
    "InstallCommand"
]
