from ....framework.registry import *
from ....framework.workload_base import WorkloadBase

try:
    import brickpi3
except ImportError:
    print("[WARNING] brickpi3 module not found, using mock implementation")
    from . import brickpi3_mock as brickpi3

class BrickPi3Device(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 100
        self.bp = brickpi3.BrickPi3()

        self._buffer_a = {}
        self._buffer_b = {}
        self._current_buffer = self._buffer_a
        self._next_buffer = self._buffer_b

        self.motor_ports = {
            'a': self.bp.PORT_A,
            'b': self.bp.PORT_B,
            'c': self.bp.PORT_C,
            'd': self.bp.PORT_D
        }
        self.sensor_ports = [self.bp.PORT_1, self.bp.PORT_2, self.bp.PORT_3, self.bp.PORT_4]

        self.motor_power_states = {}
        self.sensor_types = {}

        for p in ['a', 'b', 'c', 'd']:
            self.state.writable[f"motor_{p}_enabled"] = 0

        for i in range(4):
            self.state.writable[f"sensor_{i+1}_type"] = "NONE"

        register_workload(self)

    def setup(self):
        self.bp.reset_all()
        for p, port in self.motor_ports.items():
            if self.safe_get(f"motor_{p}_enabled"):
                attr = f"motor_{p}_power"
                self.state.writable[attr] = 0
                self.motor_power_states[port] = 0

        for i, port in enumerate(self.sensor_ports):
            sensor_type_str = self.safe_get(f"sensor_{i+1}_type")
            attr = f"sensor_{i+1}_state"
            if sensor_type_str != "NONE":
                try:
                    sensor_type_enum = getattr(brickpi3.BrickPi3.SENSOR_TYPE, sensor_type_str)
                    self.bp.set_sensor_type(port, sensor_type_enum)
                    self.sensor_types[port] = sensor_type_enum
                    self.state.readable[attr] = None
                except AttributeError:
                    print(f"[BrickPi3Device] Invalid sensor type: {sensor_type_str}")
                except Exception as e:
                    print(f"[BrickPi3Device] Error setting sensor on port {port}: {e}")

    def pre_tick(self, time_delta):
        self._current_buffer, self._next_buffer = self._next_buffer, self._current_buffer

    def tick(self, time_delta):
        for p, port in self.motor_ports.items():
            if port in self.motor_power_states:
                attr = f"motor_{p}_power"
                desired_power = self.safe_get(attr)
                try:
                    self.bp.set_motor_power(port, desired_power)
                    self.motor_power_states[port] = desired_power
                except Exception as e:
                    print(f"[BrickPi3Device] Motor set error on port {port}: {e}")
                self._next_buffer[attr] = desired_power

        for i, port in enumerate(self.sensor_ports):
            if port in self.sensor_types and self.sensor_types[port] != brickpi3.BrickPi3.SENSOR_TYPE.NONE:
                attr = f"sensor_{i+1}_state"
                try:
                    value = self.bp.get_sensor(port)
                    if self.sensor_types[port] == brickpi3.BrickPi3.SENSOR_TYPE.EV3_GYRO_ABS_DPS and isinstance(value, list):
                        value = {'abs': value[0], 'dps': value[1]}
                except Exception as e:
                    value = 'Error'
                self.safe_set(attr, value)
                self._next_buffer[attr] = value

register_workload_type(BrickPi3Device)
