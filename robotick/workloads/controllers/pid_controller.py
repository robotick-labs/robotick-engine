from ...framework.workload_base import WorkloadBase
from ...framework.registry import *

class PIDControl(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.kp = 1.0
        self.ki = 0.0
        self.kd = 0.0
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

# Register class on import
register_workload_type(PIDControl)
