import brickpi3
from ...framework.registry import *
from ...framework.workload_base import WorkloadBase

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

        # Add motor_a_enabled, motor_b_enabled, etc.
        for p in ['a', 'b', 'c', 'd']:
            setattr(self, f"motor_{p}_enabled", 0)

        # Add sensor_1_type, sensor_2_type, etc.
        for i in range(4):
            setattr(self, f"sensor_{i+1}_type", "NONE")

        register_workload(self)

    def setup(self):
        self.bp.reset_all()
        self._writable_states = []
        self._readable_states = []

        # Setup motors if enabled
        for p, port in self.motor_ports.items():
            enabled = getattr(self, f"motor_{p}_enabled", 0)
            if enabled:
                motor_power_attr = f"motor_{p}_power"
                setattr(self, motor_power_attr, 0)
                self.motor_power_states[port] = 0
                self._writable_states.append(motor_power_attr)
                self._readable_states.append(motor_power_attr)

        # Setup sensors if type != NONE
        for i, port in enumerate(self.sensor_ports):
            sensor_type_str = getattr(self, f"sensor_{i+1}_type")
            sensor_attr = f"sensor_{i+1}_state"
            if sensor_type_str != "NONE":
                try:
                    sensor_type_enum = getattr(brickpi3.BrickPi3.SENSOR_TYPE, sensor_type_str)
                    self.bp.set_sensor_type(port, sensor_type_enum)
                    self.sensor_types[port] = sensor_type_enum
                    setattr(self, sensor_attr, None)
                    self._readable_states.append(sensor_attr)
                except AttributeError:
                    print(f"[BrickPi3Device] Invalid sensor type: {sensor_type_str}")
                except Exception as e:
                    print(f"[BrickPi3Device] Error setting sensor on port {port}: {e}")

    def pre_tick(self, time_delta):
        self._current_buffer, self._next_buffer = self._next_buffer, self._current_buffer

    def tick(self, time_delta):
        # Note - we read and write every tick - our most performance-sensitive use-case so far is
        # a self-balancing (inverted pendulum) robot, which needs motor-control and gyro-state
        # as rapidly as possible - which forces us to read both every frame anyway.
        # (we could always have sensor-read frequencies specified, in case any don't need to be as
        # regular, as a feature request)

        # === Write motors every tick ===
        for p, port in self.motor_ports.items():
            if port in self.motor_power_states:
                attr = f"motor_{p}_power"
                desired_power = getattr(self, attr, 0)
                try:
                    self.bp.set_motor_power(port, desired_power)
                    self.motor_power_states[port] = desired_power
                except Exception as e:
                    print(f"[BrickPi3Device] Motor set error on port {port}: {e}")

                # Write to back-buffer
                self._next_buffer[attr] = desired_power

        # === Read sensors every tick ===
        for i, port in enumerate(self.sensor_ports):
            if port in self.sensor_types and self.sensor_types[port] != brickpi3.BrickPi3.SENSOR_TYPE.NONE:
                sensor_attr = f"sensor_{i + 1}_state"
                try:
                    value = self.bp.get_sensor(port)
                    if self.sensor_types[port] == brickpi3.BrickPi3.SENSOR_TYPE.EV3_GYRO_ABS_DPS and isinstance(value, list):
                        value = {'abs': value[0], 'dps': value[1]}
                except Exception as e:
                    value = 'Error'

                setattr(self, sensor_attr, value)
                self._next_buffer[sensor_attr] = value

# Register class on import
register_workload_type(BrickPi3Device)

