from ....framework.workload_base import WorkloadBase
from ....framework.registry import *
from rich.live import Live
from rich.table import Table
import json

class ConsoleUpdate(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 10

    def setup(self):
        self._live = Live(refresh_per_second=self.tick_rate_hz)
        self._live.start()

    def tick(self, time_delta):
        workloads = get_all_workload_instances()
        table = Table(title="== Robotick Console ==")
        table.add_column("Workload Type")
        table.add_column("Workload Name")
        table.add_column("Input(s)")
        table.add_column("Output(s)")
        table.add_column("Tick / ms")
        table.add_column("Goal / ms")
        table.add_column("%")

        for type_name, instances in workloads.items():
            for inst in instances:
                if inst._tick_parent is None:
                    self.add_instance_to_table(table, inst)

        self._live.update(table)
    
    def add_instance_to_table(self, table, inst, prefix=""):

        if inst._tick_children is not None:
            prefix = "^ "
            table.add_row()  # adds a fully blank row

            first_child = True
            for child in inst._tick_children:
                self.add_instance_to_table(table, child, " > " if first_child else " > " )
                first_child = False

        state_inputs = []
        for state in inst.get_writable_states():
            value = inst.safe_get(state)
            value_str = self._format_payload(value)
            state_inputs.append(f"{state}={value_str}")
        state_inputs_str = ", ".join(state_inputs)

        state_outputs = []
        for state in inst.get_readable_states():
            if state not in inst.get_writable_states():
                # ^- strictly speaking, writable states are readable too, but neater not to indicate this
                #    on console - just show each once only
                value = inst.safe_get(state)
                value_str = self._format_payload(value)
                state_outputs.append(f"{state}={value_str}")
        state_outputs_str = ", ".join(state_outputs)

        duration_str = "-"
        duration_goal_str = "-"
        duration_percent_str = "-"

        if inst.tick_rate_hz > 0:
            duration_self = inst.get_last_tick_duration_self() * 1000.0
            duration_goal = inst.get_tick_interval() * 1000.0
            duration_percent = 100.0 * duration_self / duration_goal if duration_goal > 0 else 0
            duration_str = f"{duration_self:.2f}"
            duration_goal_str = f"{duration_goal:.2f}"
            duration_percent_str = f"{duration_percent:.1f}%"
        elif inst._tick_parent is not None:
            duration_self = inst.get_last_tick_duration_self() * 1000.0
            duration_str = f"{duration_self:.2f}"

        table.add_row(prefix + inst._type_name, getattr(inst, 'name', repr(inst)) or "-", state_inputs_str, state_outputs_str, duration_str, duration_goal_str, duration_percent_str)

        if inst._tick_children is not None:
            table.add_row()  # adds a fully blank row

    def stop(self):
        super().stop()
        self._live.stop()

    def _format_payload(self, value):
        def round_floats(obj):
            if isinstance(obj, float):
                return round(obj, 2)
            elif isinstance(obj, dict):
                return {k: round_floats(v) for k, v in obj.items()}
            elif isinstance(obj, list):
                return [round_floats(elem) for elem in obj]
            else:
                return obj

        if isinstance(value, int):
            return str(value)
        elif isinstance(value, float):
            return f"{value:.2f}"
        else:
            try:
                cleaned_value = round_floats(value)
                return json.dumps(cleaned_value)
            except TypeError:
                return str(value)

register_workload_type(ConsoleUpdate)
