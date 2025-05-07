from ...framework.workload_base import WorkloadBase
from ...framework.registry import *

from rich.live import Live
from rich.table import Table

class ConsoleUpdate(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz=10

    def setup(self):
        self._live = Live(refresh_per_second=self.tick_rate_hz)
        self._live.start()

    def tick(self, time_delta):
        workloads = get_all_workload_instances()

        table = Table(title="== Robotick Console ==")
        table.add_column("Workload Type")
        table.add_column("Workload Name")
        table.add_column("State(s)")
        table.add_column("Tick / ms")
        table.add_column("Goal / ms")
        table.add_column("%")

        for type_name, instances in workloads.items():
            for inst in instances:
                states = []
                for state in inst.get_readable_states():
                    value = inst.safe_get(state)
                    states.append(f"{state}={value}")
                state_str = ", ".join(states)

                duration_self = inst.get_last_tick_duration_self() * 1000.0
                duration_goal = inst.get_tick_interval() * 1000.0
                duration_percent = 100.0 * duration_self / duration_goal
                duration_str = f"{duration_self:.1f}"
                duration_goal_str = f"{duration_goal:.1f}"
                duration_percent_str = f"{duration_percent:.1f}%"

                table.add_row(type_name, getattr(inst, 'name', repr(inst)) or "-", state_str, duration_str, duration_goal_str, duration_percent_str)

        self._live.update(table)

    def stop(self):
        super().stop()
        self._live.stop()

# Register class on import
register_workload_type(ConsoleUpdate)