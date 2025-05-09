from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class SteeringMixer(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 20

        # Inputs
        self.state.readable['balance_input'] = 0.0
        self.state.readable['turn_input'] = 0.0

        # Outputs
        self.state.writable['left_motor'] = 0.0
        self.state.writable['right_motor'] = 0.0

        # Tuning param
        self.max_speed_differential = 0.4

    def tick(self, time_delta):
        balance = self.safe_get('balance_input') or 0.0
        turn = self.safe_get('turn_input') or 0.0

        left = balance + turn * self.max_speed_differential
        right = balance - turn * self.max_speed_differential

        power_scale = 100.0

        # Clamp to [-1, 1]
        left = power_scale * max(min(left, 1), -1)
        right = power_scale * max(min(right, 1), -1)

        self.safe_set('left_motor', -left)
        self.safe_set('right_motor', right)

register_workload_type(SteeringMixer)
