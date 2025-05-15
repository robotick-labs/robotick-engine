"""
brickpi3_mock.py

Purpose:
    This is a public, independent mock module for the BrickPi3 Python API.
    It allows users to run and test Robotick-based robotics code on non-Raspberry Pi systems
    without requiring BrickPi3 hardware or the official drivers.
    It simulates motor control and sensor responses for realistic local testing.

Usage:
    In your code, use a try/except block to import 'brickpi3' and fall back to this mock:
        try:
            import brickpi3
        except ImportError:
            from . import brickpi3_mock as brickpi3

Installing the real brickpi3 module:
    The real 'brickpi3' module is Raspberry Pi-specific and not available via PyPI.
    Install it as per instructions in the repo: https://github.com/DexterInd/BrickPi3

Attribution:
    The BrickPi3 system and original Python API are developed by Dexter Industries and licensed under the MIT license.
    This mock is an independent tool designed for use with the Robotick framework and is not affiliated with or endorsed by Dexter Industries.
"""

import random

class SensorType:
    NONE = "NONE"
    EV3_GYRO_ABS_DPS = "EV3_GYRO_ABS_DPS"

class BrickPi3:
    PORT_A = "A"
    PORT_B = "B"
    PORT_C = "C"
    PORT_D = "D"
    PORT_1 = "1"
    PORT_2 = "2"
    PORT_3 = "3"
    PORT_4 = "4"

    SENSOR_TYPE = SensorType()

    def __init__(self):
        self.motor_power = {
            self.PORT_A: 0,
            self.PORT_B: 0,
            self.PORT_C: 0,
            self.PORT_D: 0,
        }
        self.sensor_type = {
            self.PORT_1: self.SENSOR_TYPE.NONE,
            self.PORT_2: self.SENSOR_TYPE.NONE,
            self.PORT_3: self.SENSOR_TYPE.NONE,
            self.PORT_4: self.SENSOR_TYPE.NONE,
        }
        self.sensor_state = {
            self.PORT_1: 0,
            self.PORT_2: 0,
            self.PORT_3: 0,
            self.PORT_4: 0,
        }

    def reset_all(self):
        for port in self.motor_power:
            self.motor_power[port] = 0
        for port in self.sensor_type:
            self.sensor_type[port] = self.SENSOR_TYPE.NONE
            self.sensor_state[port] = 0

    def set_motor_power(self, port, power):
        if port in self.motor_power:
            self.motor_power[port] = power

    def set_sensor_type(self, port, sensor_type):
        if port in self.sensor_type:
            self.sensor_type[port] = sensor_type
            # Initialize mock sensor value
            if sensor_type == self.SENSOR_TYPE.EV3_GYRO_ABS_DPS:
                self.sensor_state[port] = [0, 0]
            else:
                self.sensor_state[port] = 0

    def get_sensor(self, port):
        if port in self.sensor_type:
            s_type = self.sensor_type[port]
            if s_type == self.SENSOR_TYPE.EV3_GYRO_ABS_DPS:
                # Simulate gyro values: abs angle + rotational speed
                angle = random.randint(-180, 180)
                speed = random.randint(-500, 500)
                self.sensor_state[port] = [angle, speed]
            else:
                # Simple numeric value
                self.sensor_state[port] = random.randint(0, 100)
            return self.sensor_state[port]
        return None

    # For extending the mock later if needed:
    def set_motor_position(self, port, position):
        pass

    def set_motor_position_relative(self, port, degrees):
        pass

    def set_motor_position_kp(self, port, kp=25):
        pass

    def set_motor_position_kd(self, port, kd=70):
        pass

    def set_motor_dps(self, port, dps):
        pass

    def set_motor_limits(self, port, power=0, dps=0):
        pass

    def get_motor_status(self, port):
        if port in self.motor_power:
            return [0, self.motor_power[port], 0, 0]  # flags, power, encoder, dps
        return [0, 0, 0, 0]

    def get_motor_encoder(self, port):
        return 0

    def offset_motor_encoder(self, port, position):
        pass

    def reset_motor_encoder(self, port):
        pass
