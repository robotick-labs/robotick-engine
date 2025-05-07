from ..devices.remote_control_device import RemoteControlDevice
from ...framework.workload_base import WorkloadBase
from ...framework.registry import *

def apply_dead_zone(value, dead_zone):
    if abs(value) < dead_zone:
        return 0
    else:
        return (abs(value) - dead_zone) / (1 - dead_zone) * (1 if value > 0 else -1)

class BrickPi3SimpleRc(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 20  # control loop rate (adjust as needed)

        self.stick_dead_zone = 0.2

    def setup(self):
        self.brickpi3_device = get_all_workload_instances_of_type("brick_pi3_device")[0]
        self.remote_control_device = get_all_workload_instances_of_type("remote_control_device")[0]
    
    def tick(self, time_delta):
        left_stick = self.remote_control_device.get_left_stick() if self.remote_control_device else {'x': 0, 'y': 0}

        x = left_stick.get('x', 0)
        y = left_stick.get('y', 0)

        # Stick dead-zone
        x = apply_dead_zone(x, self.stick_dead_zone)
        y = apply_dead_zone(y, self.stick_dead_zone)

        linear_speed = y
        turn_speed = x

        max_speed_differential = 0.4

        # Calculate left/right motor powers
        left_power = right_power = linear_speed

        left_power += turn_speed * max_speed_differential
        right_power -= turn_speed * max_speed_differential

        # Clamp values to [-100, 100]
        left_power = max(min(left_power, 1), -1) * 100
        right_power = max(min(right_power, 1), -1) * 100

        # Set potor powers:
        self.set_motor_powers(left_power, right_power)

    def set_motor_powers(self, left_power, right_power):
        """Send power values to BrickPi3 motors (stub to wire up)."""
        if self.brickpi3_device:
            self.brickpi3_device.safe_set('motor_a_power', int(left_power))
            self.brickpi3_device.safe_set('motor_d_power', int(right_power))

# Register class on import
register_workload_type(BrickPi3SimpleRc)