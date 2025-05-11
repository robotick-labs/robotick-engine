from ....framework.registry import *
from ....framework.workload_base import WorkloadBase
import copy

try:
    import brickpi3
except ImportError:
    print("[WARNING] brickpi3 module not found, using mock implementation")
    from . import brickpi3_mock as brickpi3

class BrickPi3Device(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 100

        self._state_internal_copy = None
    
    def pre_load(self):
        self.bp = brickpi3.BrickPi3()

        self.motor_ports = {
            'a': self.bp.PORT_A,
            'b': self.bp.PORT_B,
            'c': self.bp.PORT_C,
            'd': self.bp.PORT_D
        }
        self.sensor_ports = [self.bp.PORT_1, self.bp.PORT_2, self.bp.PORT_3, self.bp.PORT_4]

        self.motor_power_states = {}
        self.sensor_types = {}

        # copy motor and sensor settings to writable states (they are state-values too since it might be useful)
        # to be able to change them dynamically at runtime)

        for p in ['a', 'b', 'c', 'd']:
            attr = f"motor_{p}_enabled"
            self.state.writable[attr] = getattr(self, attr, 0)

        for i in range(4):
            attr = f"sensor_{i+1}_type"
            self.state.writable[attr] = getattr(self, attr, "NONE")

    def load(self):
        self.bp.reset_all()

        for p, port in self.motor_ports.items():
            attr_enabled = f"motor_{p}_enabled"
            if getattr(self, attr_enabled, False):
                attr = f"motor_{p}_power"
                self.state.writable[attr] = 0
                self.motor_power_states[port] = 0

        for i, port in enumerate(self.sensor_ports):
            attr_sensor_type = f"sensor_{i+1}_type"
            sensor_type_str = getattr(self, attr_sensor_type, "NONE")
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
        
        self._state_internal_copy = copy.deepcopy(self.state)

    def pre_tick(self, time_delta):
        # Copy current writable state so that this tick can safely apply the last tick’s control inputs,
        # and prepare next tick’s outputs — without interfering with any child-workloads using previous sensor data
        # or setting controls for next tick

        self._state_internal_copy.writable = copy.deepcopy(self.state.writable)

    def tick(self, time_delta):
        # Apply motor commands first, allowing time for hardware to react,
        # so subsequent sensor reads reflect the new control state.
        for p, port in self.motor_ports.items():
            if port in self.motor_power_states:
                attr = f"motor_{p}_power"
                desired_power = self._state_internal_copy.writable[attr] or 0
                try:
                    self.bp.set_motor_power(port, desired_power)
                    self.motor_power_states[port] = desired_power
                except Exception as e:
                    print(f"[BrickPi3Device] Motor set error on port {port}: {e}")

        # Read sensors last (see note above)
        for i, port in enumerate(self.sensor_ports):
            if port in self.sensor_types and self.sensor_types[port] != brickpi3.BrickPi3.SENSOR_TYPE.NONE:
                attr = f"sensor_{i+1}_state"
                try:
                    value = self.bp.get_sensor(port)
                    if self.sensor_types[port] == brickpi3.BrickPi3.SENSOR_TYPE.EV3_GYRO_ABS_DPS and isinstance(value, list):
                        value = {'abs': value[0], 'dps': value[1]}
                except Exception as e:
                    value = 'Error'
                self._state_internal_copy.readable[attr] = value

    def post_tick(self, time_delta):
        # Publish updated sensor readings from internal state to shared readable state,
        # making them visible to other child-workloads next tick

        self.state.readable = copy.deepcopy(self._state_internal_copy.readable)

register_workload_type(BrickPi3Device)
