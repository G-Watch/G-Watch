import os
import copy
import shutil
import glob
import multiprocessing
from setuptools import Command
from setuptools.command.build_ext import build_ext
from distutils import ccompiler, unixccompiler

from ._common import *
from ._utils import *
from .install_headers import *


class BuildExtCommand(build_ext):
    def finalize_options(self):
        if self.parallel is None:
            self.parallel = multiprocessing.cpu_count()
        super().finalize_options()

    def build_extension(self, ext):
        # Append the extension name to the temp build directory
        # so that each module builds to its own directory.
        # We need to make a (shallow) copy of 'self' here
        # so that we don't overwrite this value when running in parallel.
        self_copy = copy.copy(self)
        self_copy.build_temp = os.path.join(self.build_temp, ext.name)
        build_ext.build_extension(self_copy, ext)

    def build_extensions(self) -> None:
        # add cuda support
        self.compiler.src_extensions += ['.cu', '.cuh']

        original_compile_func = self.compiler._compile

        def unix_wrap_single_compile(obj, src, ext, cc_args, extra_postargs, pp_opts) -> None:
            cflags = copy.deepcopy(extra_postargs)
            original_compiler = self.compiler.compiler_so.copy()
            try:
                if is_cuda_file(src):
                    nvcc = ['/usr/local/cuda/bin/nvcc']
                    self.compiler.set_executable('compiler_so', nvcc)
                    if isinstance(cflags, dict):
                        cflags = cflags['nvcc']
                else:
                    if isinstance(cflags, dict):
                        cflags = cflags['cxx']
                original_compile_func(obj, src, ext, cc_args, cflags, pp_opts)
            finally:
                # Put the original compiler back in place.
                self.compiler.set_executable('compiler_so', original_compiler)
        self.compiler._compile = unix_wrap_single_compile
        self.parallel = multiprocessing.cpu_count()
        self.compiler.thread = multiprocessing.cpu_count()

        super().build_extensions()


    def run(self):
        # build extensions
        super().run()

        # rename .so of scheduler lib
        so_files = glob.glob(f"{self.build_lib}/gwatch/libgwatch_scheduler.*.so", recursive=True)
        if len(so_files) > 0:
            assert(len(so_files) == 1)
            shutil.copy(so_files[0], f"{self.build_lib}/gwatch/libgwatch_scheduler.so")

        # rename.so of capsule lib
        so_files = glob.glob(f"{self.build_lib}/gwatch/libpygwatch.*.so", recursive=True)
        if len(so_files) > 0:
            assert(len(so_files) == 1)
            shutil.copy(so_files[0], f"{self.build_lib}/gwatch/libpygwatch.so")

        # rename.so of capsule hijack lib
        so_files = glob.glob(f"{self.build_lib}/gwatch/libgwatch_capsule_hijack.*.so", recursive=True)
        if len(so_files) > 0:
            assert(len(so_files) == 1)
            shutil.copy(so_files[0], f"{self.build_lib}/gwatch/libgwatch_capsule_hijack.so")

        # rename.so of profiler lib
        so_files = glob.glob(f"{self.build_lib}/gwatch/libgwatch_profiler.*.so", recursive=True)
        if len(so_files) > 0:
            assert(len(so_files) == 1)
            shutil.copy(so_files[0], f"{self.build_lib}/gwatch/libgwatch_profiler.so")


class CleanCommand(Command):
    description = 'clean all built assets'
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        # ================ delete all built assets ================
        print("removing build assets...")
        # delete build dir
        if os.path.exists('build'):
            shutil.rmtree('build')
            print("removed build")
        # delete dist dir
        if os.path.exists('dist'):
            shutil.rmtree('dist')
            print("removed dist")
        # delete gwatch.egg-info dir
        if os.path.exists('gwatch.egg-info'):
            shutil.rmtree('gwatch.egg-info')
            print("removed gwatch.egg-info")
        # delete all .so files
        so_files = glob.glob('*.so') + glob.glob('gwatch/*.so')
        if so_files:
            for f in so_files:
                os.remove(f)
                print(f"removed {f}")

        # ================ delete all installed assets ================
        print("removing installed assets...")
        if os.path.exists('/usr/local/include/gwatch/utils'):
            shutil.rmtree('/usr/local/include/gwatch/utils')



__all__ = [
    "BuildExtCommand",
    "CleanCommand"
]
