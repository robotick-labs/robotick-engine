import brickpi3
from ..framework.workload_base import WorkloadBase
from ..framework.registry import register_workload

class BrickPiDevice(WorkloadBase):
    def __init__(self):
        super().__init__()
        self._tick_rate_hz=100
        self.bp = brickpi3.BrickPi3()

        self._buffer_a = {}
        self._buffer_b = {}
        self._current_buffer = self._buffer_a
        self._next_buffer = self._buffer_b

        register_workload(self)

    def get_current_state(self):
        return self._current_buffer

    def pre_tick(self, time_delta):
        self._current_buffer, self._next_buffer = self._next_buffer, self._current_buffer

    def tick(self, time_delta):
        try:
            next_buf = {
                'motor_left': self.bp.get_motor_encoder(self.bp.PORT_A),
                'motor_right': self.bp.get_motor_encoder(self.bp.PORT_B),
                'sensor_ultrasonic': self.bp.get_sensor(self.bp.PORT_1),
                'sensor_light': self.bp.get_sensor(self.bp.PORT_2)
            }
        except Exception as e:
            print(f"[BrickPiDevice] Read error: {e}")
            next_buf = {}

        self._next_buffer.update(next_buf)
