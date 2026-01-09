from .scheduler import *
from typing import Set

import gwatch.libgwatch_scheduler as _C_scheduler

options : Set[str] = []

# parse CLI options
args = GWScheduler.parse_cli_args()
if args.options:
    args.options = set(args.options.split(','))
else:
    args.options = set()

# parse yaml config
scheduler_config : _C_scheduler.GWSchedulerConfig = GWScheduler.parse_yaml_config(args.config)

# create scheduler instance
scheduler : GWScheduler = GWScheduler(
    scheduler_config = scheduler_config,
    backend = args.backend,
    world_size = args.world_size,
    command = args.command,
    options = args.options
)

# start scheduler serving process
scheduler.serve()

# start capsules
scheduler.start_capsule()

# asynchrounously start parsing codebase

while True:
    pass
