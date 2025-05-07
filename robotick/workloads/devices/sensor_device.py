from .io_device import IODevice
from ...framework.registry import *
import random

class SensorDevice(IODevice):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz=10
        self.state = random.randint(50, 60)
        self._readable_states = ['state']

    def tick(self, time_delta):
        # In real sensor, would poll hardware
        # We just keep the initial random value for now
        pass

    def read(self):
        return self.safe_get('state')

# Register class on import
register_workload_type(SensorDevice)