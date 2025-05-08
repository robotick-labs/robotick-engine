import math
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class BalancingRobotSimulator(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 500

        self.mass = 10.0
        self.wheel_radius = 0.025
        self.track_width = 0.2
        self.body_width = 0.15
        self.body_depth = 0.10
        self.g = 9.81

        variables = [
            'x', 'y', 'yaw', 'pitch', 'roll', 'legs_height',
            'dx', 'dy', 'dyaw', 'dpitch'
        ]

        for var in variables:
            self.state.readable[var] = 0.0

        self.state.writable['wheel_torque_L'] = 0.0
        self.state.writable['wheel_torque_R'] = 0.0
        self.state.writable['leg_height_L'] = 0.3
        self.state.writable['leg_height_R'] = 0.3

    def tick(self, time_delta):
        dt = time_delta if time_delta is not None else self.get_tick_interval()

        F_L = self.safe_get('wheel_torque_L') / self.wheel_radius
        F_R = self.safe_get('wheel_torque_R') / self.wheel_radius
        F_total = F_L + F_R

        a_x_robot = F_total / self.mass

        a_x_world = a_x_robot * math.cos(self.state.readable['yaw'])
        a_y_world = a_x_robot * math.sin(self.state.readable['yaw'])

        self.state.readable['dx'] += a_x_world * dt
        self.state.readable['dy'] += a_y_world * dt

        self.state.readable['x'] += self.state.readable['dx'] * dt
        self.state.readable['y'] += self.state.readable['dy'] * dt

        yaw_moment = (F_R - F_L) * self.track_width / 2
        self.state.readable['dyaw'] += yaw_moment / (self.mass * self.track_width) * dt
        self.state.readable['yaw'] += self.state.readable['dyaw'] * dt

        legs_height = (self.safe_get('leg_height_L') + self.safe_get('leg_height_R')) / 2
        self.state.readable['legs_height'] = legs_height

        if self.track_width != 0:
            self.state.readable['roll'] = math.atan(
                (self.safe_get('leg_height_L') - self.safe_get('leg_height_R')) / self.track_width)
        else:
            self.state.readable['roll'] = 0.0

        I_theta = self.mass * legs_height ** 2
        try:
            theta_accel = (
                self.mass * self.g * legs_height * math.sin(self.state.readable['pitch']) +
                self.mass * legs_height * a_x_robot * math.cos(self.state.readable['pitch'])
            ) / I_theta
        except ZeroDivisionError:
            theta_accel = 0.0

        self.state.readable['dpitch'] += theta_accel * dt
        self.state.readable['pitch'] += self.state.readable['dpitch'] * dt

        max_tilt = math.pi / 2
        if self.state.readable['pitch'] > max_tilt:
            self.state.readable['pitch'] = max_tilt
            self.state.readable['dpitch'] = 0.0
        elif self.state.readable['pitch'] < -max_tilt:
            self.state.readable['pitch'] = -max_tilt
            self.state.readable['dpitch'] = 0.0

register_workload_type(BalancingRobotSimulator)
