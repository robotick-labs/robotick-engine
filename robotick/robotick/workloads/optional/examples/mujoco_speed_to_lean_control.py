from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class MujocoSpeedToLeanControl(WorkloadBase):
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

    def tick(self, time_delta):
        desired_speed = self.safe_get('desired_speed') or 0.0
        current_speed = self.safe_get('current_speed') or 0.0
        
        damping_ratio = 0.8  # slightly underdamped (ζ ~ 0.7–0.9)
        natural_freq = 5.0   # adjust to tune how fast it reacts (rad/s)

        desired_accel = (desired_speed - current_speed) * time_delta

        # clamp to max_acceleration
        desired_accel = min(max(desired_accel, -self.max_acceleration), self.max_acceleration);

        k_speed = 1.0
        k_accel = 1.5

        goal_lean = k_speed * desired_speed + k_accel * desired_accel

        # clamp to max_lean_angle
        goal_lean = min(max(goal_lean, -self.max_lean_angle), self.max_lean_angle);

        # Write to local readable states (auto-propagated by bindings)
        self.safe_set('goal_lean', goal_lean)

register_workload_type(MujocoSpeedToLeanControl)