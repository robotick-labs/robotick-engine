from .workload_base import WorkloadBase
from .registry import register_workload

class PIDControl(WorkloadBase):
    def __init__(self, name="pid", tick_parent=None, kp=1.0, ki=0.0, kd=0.0):
        super().__init__(tick_rate_hz=None, tick_parent=tick_parent)
        self.name = name
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.integral = 0
        self.prev_error = 0

        register_workload(self)

    def tick(self, time_delta):
        state = self._tick_parent.get_current_state()

        gyro = state.get('sensor_gyro', 0)  # update key as needed
        target_angle = 0

        error = target_angle - gyro
        self.integral += error
        derivative = error - self.prev_error

        output = (self.kp * error) + (self.ki * self.integral) + (self.kd * derivative)
        self.prev_error = error

        state['motor_command'] = output
