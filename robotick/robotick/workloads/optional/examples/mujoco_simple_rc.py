from ...core.devices.remote_control_device import RemoteControlDevice
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

def apply_dead_zone(value, dead_zone):
    if abs(value) < dead_zone:
        return 0
    else:
        return (abs(value) - dead_zone) / (1 - dead_zone) * (1 if value > 0 else -1)

class MujocoSimpleRc(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 20
        self.stick_dead_zone = 0.2

    def setup(self):
        self.mujoco_simulator = get_all_workload_instances_of_type("mujoco_simulator")[0]
        self.remote_control_device = get_all_workload_instances_of_type("remote_control_device")[0]

        # Get actuator names from Mujoco
        self.actuators = self.mujoco_simulator._input_names if self.mujoco_simulator else []

    def tick(self, time_delta):
        left_stick = self.remote_control_device.get_left_stick() if self.remote_control_device else {'x': 0, 'y': 0}
        x = apply_dead_zone(left_stick.get('x', 0), self.stick_dead_zone)
        y = apply_dead_zone(left_stick.get('y', 0), self.stick_dead_zone)

        linear_speed = y
        turn_speed = x
        max_speed_differential = 0.5

        left_power = right_power = linear_speed
        left_power += turn_speed * max_speed_differential
        right_power -= turn_speed * max_speed_differential

        left_power = max(min(left_power, 1), -1)
        right_power = max(min(right_power, 1), -1)

        right_power *= -1.0 # negate it as its facing the opposite direction

        left_power = left_power ** 3
        right_power = right_power ** 3

        control_range = 100.0

        self.set_actuator_powers(left_power * control_range, right_power * control_range)

    def set_actuator_powers(self, left_power, right_power):
        if self.mujoco_simulator and len(self.actuators) >= 2:
            left_actuator = self.actuators[0]
            right_actuator = self.actuators[1]
            self.mujoco_simulator.safe_set(left_actuator, left_power)
            self.mujoco_simulator.safe_set(right_actuator, right_power)

register_workload_type(MujocoSimpleRc)
