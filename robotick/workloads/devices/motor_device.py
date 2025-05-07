from .io_device import IODevice
from ...framework.registry import *

class MotorDevice(IODevice):
    def __init__(self):
        super().__init__()
        self.speed = 0
        self.goal_speed = 0
        self._readable_states = ['speed', 'goal_speed']
        self._writable_states = ['goal_speed']

    def tick(self, time_delta):
        with self._lock:
            if self.speed < self.goal_speed:
                self.speed += 1
            elif self.speed > self.goal_speed:
                self.speed -= 1

    def set_goal_speed(self, value):
        self.safe_set('goal_speed', value)

    def get_speed(self):
        return self.safe_get('speed')

# Register class on import
register_workload_type(MotorDevice)