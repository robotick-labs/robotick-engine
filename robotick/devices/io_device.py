from ..framework.workload_base import WorkloadBase

class IODevice(WorkloadBase):
    def __init__(self):
        super().__init__()
        self._tick_rate_hz=100
        self.state = 0

    def tick(self, time_delta):
        # Example: increment state
        with self._lock:
            self.state += 1

    def read_state(self):
        return self.safe_get('state')

    def write_state(self, value):
        self.safe_set('state', value)
