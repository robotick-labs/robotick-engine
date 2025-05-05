import math
from ..framework.workload_base import WorkloadBase

class BalancingRobotSimulator(WorkloadBase):
    def __init__(self):
        super().__init__()

        self._tick_rate_hz = 500

        # Robot parameters (can be overridden after init)
        self.mass = 10.0               # kg
        self.wheel_radius = 0.025      # meters
        self.track_width = 0.2         # meters
        self.body_width = 0.15         # meters
        self.body_depth = 0.10         # meters
        self.g = 9.81                  # m/s^2

        # Initial state variables
        self.x = 0.0      # world-pos-x in meters
        self.y = 0.0      # world-pos-y in meters
        self.yaw = 0.0    # radians
        self.legs_height = 0.3      # average leg-height (or mid-hips height) in meters
        self.pitch = 0.0  # radians (tilt)
        self.roll = 0.0   # radians

        self.dx = 0.0
        self.dy = 0.0
        self.dyaw = 0.0
        self.dpitch = 0.0
        self.dh = 0.0  # not used dynamically, but included
        self.droll = 0.0  # treated kinematically

        # Control inputs (external should set these!)
        self.wheel_torque_L = 0.0
        self.wheel_torque_R = 0.0
        self.leg_height_L = self.legs_height
        self.leg_height_R = self.legs_height

        # Optionally add readable states here for external inspection
        self._readable_states = [
            'x', 'y', 'yaw', 'pitch', 'roll', 'legs_height',
            'dx', 'dy', 'dyaw', 'dpitch'
        ]

    def tick(self, time_delta):
        dt = time_delta if time_delta is not None else self.get_tick_interval()

        # Forces from wheel torques
        F_L = self.wheel_torque_L / self.wheel_radius
        F_R = self.wheel_torque_R / self.wheel_radius
        F_total = F_L + F_R

        a_x_robot = F_total / self.mass

        # Acceleration in world frame
        a_x_world = a_x_robot * math.cos(self.yaw)
        a_y_world = a_x_robot * math.sin(self.yaw)

        # Update velocities
        self.dx += a_x_world * dt
        self.dy += a_y_world * dt

        # Update positions
        self.x += self.dx * dt
        self.y += self.dy * dt

        # Yaw dynamics
        yaw_moment = (F_R - F_L) * self.track_width / 2  # torque = force * half-track
        self.dyaw += yaw_moment / (self.mass * self.track_width) * dt
        self.yaw += self.dyaw * dt

        # Height and roll (kinematic)
        self.legs_height = (self.leg_height_L + self.leg_height_R) / 2
        if self.track_width != 0:
            self.roll = math.atan((self.leg_height_L - self.leg_height_R) / self.track_width)
        else:
            self.roll = 0.0

        # Pitch inertia
        I_theta = self.mass * self.legs_height ** 2

        # Tilt dynamics
        try:
            theta_accel = (
                self.mass * self.g * self.legs_height * math.sin(self.pitch) +
                self.mass * self.legs_height * a_x_robot * math.cos(self.pitch)
            ) / I_theta
        except ZeroDivisionError:
            theta_accel = 0.0

        self.dpitch += theta_accel * dt
        self.pitch += self.dpitch * dt

        # Clamp tilt to ±90 degrees
        max_tilt = math.pi / 2
        if self.pitch > max_tilt:
            self.pitch = max_tilt
            self.dpitch = 0.0
        elif self.pitch < -max_tilt:
            self.pitch = -max_tilt
            self.dpitch = 0.0
