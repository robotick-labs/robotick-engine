from ...core.devices.remote_control_device import RemoteControlDevice
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

def apply_dead_zone(value, dead_zone):
    if abs(value) < dead_zone:
        return 0
    else:
        return (abs(value) - dead_zone) / (1 - dead_zone) * (1 if value > 0 else -1)

# TODO - we no longer need this to access Mujoco at all - rename and move it to core location, as its handy (deadzone etc); and make it a non-ticking transformer

class MujocoSimpleRc(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 20
        self.stick_dead_zone = 0.2
        self.stick_scale_x = 1.0
        self.stick_scale_y = 1.0

        self.state.readable['linear_speed'] = 0
        self.state.readable['yaw_speed'] = 0

    def setup(self):
        self.remote_control_device = get_all_workload_instances_of_type("remote_control_device")[0]

    def tick(self, time_delta):
        left_stick = self.remote_control_device.get_left_stick() if self.remote_control_device else {'x': 0, 'y': 0}
        x = apply_dead_zone(left_stick.get('x', 0), self.stick_dead_zone)
        y = apply_dead_zone(left_stick.get('y', 0), self.stick_dead_zone)

        x *= self.stick_scale_x
        y *= self.stick_scale_y

        self.safe_set('linear_speed', y)
        self.safe_set('yaw_speed', x)

register_workload_type(MujocoSimpleRc)
