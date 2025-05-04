from .workload_base import WorkloadBase
from .registry import get_all

import sys

class ConsoleUpdate(WorkloadBase):
    def __init__(self, tick_rate_hz=10):
        super().__init__(tick_rate_hz)
        self._first_run = True
        self._lines_to_move_up = 0

    def tick(self, time_delta):
        workloads = get_all()

        if self._first_run:
            print("== Robotick Console ==")
            print()
            self._first_run = False
        else:
            sys.stdout.write(f"\x1b[{self._lines_to_move_up}F")
            sys.stdout.flush()

        for type_name, instances in workloads.items():
            sys.stdout.write(f"{type_name}s:\x1b[K\n")
            for inst in instances:
                line = f"  {getattr(inst, 'name', repr(inst))}:"
                for state in inst.get_readable_states():
                    value = inst.safe_get(state)
                    line += f" {state}={value}"
                sys.stdout.write(line + "\x1b[K\n")
        sys.stdout.flush()

        self._lines_to_move_up = sum(1 + len(instances) for instances in workloads.values()) + 1
