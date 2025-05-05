from ..framework.workload_base import WorkloadBase
from ..framework.registry import get_all
from rich.live import Live
from rich.table import Table

class ConsoleUpdate(WorkloadBase):
    def __init__(self):
        super().__init__()
        self._tick_rate_hz=10
        self._live = Live(refresh_per_second=self._tick_rate_hz)
        self._live.start()

    def tick(self, time_delta):
        workloads = get_all()

        table = Table(title="== Robotick Console ==")
        table.add_column("Workload Type")
        table.add_column("Workload Name")
        table.add_column("State(s)")

        for type_name, instances in workloads.items():
            for inst in instances:
                states = []
                for state in inst.get_readable_states():
                    value = inst.safe_get(state)
                    states.append(f"{state}={value}")
                state_str = ", ".join(states)
                table.add_row(type_name, getattr(inst, 'name', repr(inst)) or "-", state_str)

        self._live.update(table)

    def stop(self):
        super().stop()
        self._live.stop()
