from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class PidControl(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 100
        self.kp = 1.0
        self.ki = 0.0
        self.kd = 0.0
        self.integral = 0.0
        self.prev_error = 0.0

        # Writable inputs
        self.state.writable['setpoint'] = 0.0
        self.state.writable['measured'] = 0.0

        # Readable outputs
        self.state.readable['error'] = 0.0
        self.state.readable['p_term'] = 0.0
        self.state.readable['i_term'] = 0.0
        self.state.readable['d_term'] = 0.0
        self.state.readable['control_output'] = 0.0

    def tick(self, time_delta):
        setpoint = self.safe_get('setpoint') or 0.0
        measured = self.safe_get('measured') or 0.0

        error = setpoint - measured
        self.integral += error * time_delta if time_delta else error
        derivative = (error - self.prev_error) / time_delta if time_delta else 0.0

        p_term = (self.kp * error)
        i_term = (self.ki * self.integral)
        d_term = (self.kd * derivative)

        output = p_term + i_term + d_term

        self.prev_error = error

        # Write to local readable states (auto-propagated by bindings)
        self.safe_set('error', error)
        self.safe_set('p_term', p_term)
        self.safe_set('i_term', i_term)
        self.safe_set('d_term', d_term)
        self.safe_set('control_output', output)

register_workload_type(PidControl)
