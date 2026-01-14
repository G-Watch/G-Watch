import argparse
import os
import sys
import subprocess
from pathlib import Path
from typing import List, Optional


class ProfileCommand:
    def register(self, subparsers):
        self.parser = subparsers.add_parser(
            "profile", 
            help="Run application with G-Watch profiler",
            usage="gwatch profile [options] <command>"
        )
        self.parser.add_argument(
            "-o", "--output",
            help="Path to save the trace file",
            default="trace.json"
        )
        self.parser.add_argument(
            "command_args", 
            nargs=argparse.REMAINDER, 
            help="The command to run"
        )
        self.parser.set_defaults(func=self.run)


    def run(self, args: argparse.Namespace):
        cmd_args = args.command_args
        if not cmd_args:
            self.parser.print_help()
            sys.exit(1)

        self._run_profile(cmd_args, args)


    def _get_library_path(self, lib_name: str) -> Optional[str]:
        """
        Find the path to the specified shared library within the gwatch package.
        """
        current_dir = Path(__file__).resolve().parent
        gwatch_root = current_dir.parent
        
        # Check common locations where libs might be placed during build/install
        possible_paths = [
            gwatch_root / lib_name,
            gwatch_root / "libs" / lib_name,
            # Helper for local dev if libs are in build/ or root
            gwatch_root.parent / lib_name,
        ]

        for p in possible_paths:
            if p.exists():
                return str(p)

        return None


    def _run_profile(self, command_args: List[str], args: argparse.Namespace):
        """
        Handle the 'profile' command by setting up the environment and executing the target command.
        """
        lib_name = "libgwatch_capsule_hijack.so" 
        lib_path = self._get_library_path(lib_name)
        
        # set environment variables
        env = os.environ.copy()
        env["GW_OUTPUT_PATH"] = args.output

        # add hijack library to LD_PRELOAD
        if lib_path:
            current_preload = env.get("LD_PRELOAD", "")
            if current_preload:
                env["LD_PRELOAD"] = f"{lib_path}:{current_preload}"
            else:
                env["LD_PRELOAD"] = lib_path
        else:
            raise RuntimeError(f"Error: Capsule hijack library not found: {lib_name}")

        # Execute the command
        try:
            # Using execvp-like behavior via subprocess to replace/run the process
            subprocess.run(command_args, env=env, check=True)
        except subprocess.CalledProcessError as e:
            sys.exit(e.returncode)
        except:
            raise RuntimeError(f"Error: Failed to execute command: {command_args}")
