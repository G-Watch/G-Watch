import argparse
import sys
from .profile import ProfileCommand

def main():
    parser = argparse.ArgumentParser(
        description="G-Watch command line interface",
        usage="gwatch <command> [options] [target_command...]"
    )
    subparsers = parser.add_subparsers(dest="subcommand", help="Available commands")

    # register commands
    commands = [
        ProfileCommand(),
    ]
    for command in commands:
        command.register(subparsers)

    # run command
    args = parser.parse_args()
    if hasattr(args, 'func'):
        args.func(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
