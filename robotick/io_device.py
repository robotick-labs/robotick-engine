from .workload_base import WorkloadBase

class IODevice(WorkloadBase):
    def __init__(self, name, tick_rate_hz=100):
        super().__init__(tick_rate_hz)
        self.name = name
        self.state = 0

    def tick(self, time_delta):
        # Example: increment state
        with self._lock:
            self.state += 1

    def read_state(self):
        return self.safe_get('state')

    def write_state(self, value):
        self.safe_set('state', value)
