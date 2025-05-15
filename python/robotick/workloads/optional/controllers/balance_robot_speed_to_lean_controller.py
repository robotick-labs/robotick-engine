from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class BalanceRobotSpeedToLeanController(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 100
        self.max_lean_angle = 0.5
        self.max_acceleration = 1.0

        # Writable inputs
        self.state.writable['desired_speed'] = 0.0
        self.state.writable['current_speed'] = 0.0

        # Readable outputs
        self.state.readable['goal_lean'] = 0.0
        self.state.readable['debug_vis'] = ""

    def tick(self, time_delta):
        desired_speed = self.safe_get('desired_speed') or 0.0
        current_speed = self.safe_get('current_speed') or 0.0
        
        desired_accel = (desired_speed - current_speed) * time_delta * 10.0

        # clamp to max_acceleration
        desired_accel = min(max(desired_accel, -self.max_acceleration), self.max_acceleration)

        k_speed = 1.0
        k_accel = -1.0

        goal_lean = k_speed * desired_speed + k_accel * desired_accel

        # clamp to max_lean_angle
        goal_lean = min(max(goal_lean, -self.max_lean_angle), self.max_lean_angle)

        # Write to local readable states
        self.safe_set('goal_lean', goal_lean)

        # Generate debug visualization
        bar_length = 21  # total length inside [ ]
        center_index = bar_length // 2
        frac = (goal_lean + self.max_lean_angle) / (2 * self.max_lean_angle)
        pos = int(round(frac * (bar_length - 1)))

        vis = ['-' for _ in range(bar_length)]

        if pos == center_index:
            vis[center_index] = '*'
        else:
            vis[pos] = '*'
            vis[center_index] = '|'

        debug_str = '[' + ''.join(vis) + ']'

        self.safe_set('debug_vis', debug_str)

register_workload_type(BalanceRobotSpeedToLeanController)
