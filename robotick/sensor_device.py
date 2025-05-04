from .io_device import IODevice
import random

class SensorDevice(IODevice):
    def __init__(self, name, tick_rate_hz=10):
        super().__init__(name, tick_rate_hz)
        self.state = random.randint(50, 60)
        self._readable_states = ['state']

    def tick(self, time_delta):
        # In real sensor, would poll hardware
        # We just keep the initial random value for now
        pass

    def read(self):
        return self.safe_get('state')
