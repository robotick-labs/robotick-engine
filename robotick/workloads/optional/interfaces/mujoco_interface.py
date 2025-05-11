import mujoco
import mujoco.viewer
import json
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class MujocoInterface(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0

        self.xml_path = ""
        self.display_enabled = False

        self.model = None
        self.data = None
        self.viewer = None

        self._input_names = []
        self._sensor_names = []

        self.world_simulator = MujocoWorldSimulator(self.xml_path, self.display_enabled)
        # ^- once created, expect that it will automatically go through the load/setup lifecycle
        #    automatically.

    def pre_load(self):
        self.world_simulator.name = self.name + "_world"
        self.world_simulator.xml_path = self.xml_path
        self.world_simulator.display_enabled = self.display_enabled

    def setup(self):

        self.model = self.world_simulator.model
        self.data = self.world_simulator.data
        self.viewer = self.world_simulator.viewer

        # Discover actuators
        self._input_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_ACTUATOR, i) or f'actuator_{i}'
            for i in range(self.model.nu)
        ]
        for name in self._input_names:
            self.state.writable[name] = 0.0

        # Discover sensors
        self._sensor_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_SENSOR, i) or f'sensor_{i}'
            for i in range(self.model.nsensor)
        ]
        for name in self._sensor_names:
            self.state.readable[name] = 0.0

    def tick(self, time_delta):
        # Send actuator commands
        for i, name in enumerate(self._input_names):
            val = self.safe_get(name)
            self.data.ctrl[i] = val if val is not None else 0.0

        # Read sensor outputs
        offset = 0
        for i, name in enumerate(self._sensor_names):
            dim = self.model.sensor_dim[i]
            values = self.data.sensordata[offset:offset + dim]
            if dim == 1:
                self.safe_set(name, values[0])
            else:
                self.safe_set(name, list(values))
            offset += dim

    def stop(self):
        if self.world_simulator:
            self.world_simulator.stop()
            self.world_simulator = None
        super().stop()

class MujocoWorldSimulator(WorkloadBase):
    def __init__(self, xml_path, display_enabled=False):
        super().__init__()
        self.xml_path = xml_path
        self.display_enabled = display_enabled

        self.model = None
        self.data = None
        self.viewer = None

    def load(self):
        try:
            self.model = mujoco.MjModel.from_xml_path(self.xml_path)
        except Exception as e:
            raise RuntimeError(f"Failed to load MuJoCo model: {e}")

        self.data = mujoco.MjData(self.model)
        self.tick_rate_hz = int(1.0 / self.model.opt.timestep)

    def setup(self):
        if self.display_enabled:
            self.viewer = mujoco.viewer.launch_passive(self.model, self.data)

    def tick(self, time_delta):
        mujoco.mj_step(self.model, self.data)
        if self.viewer:
            self.viewer.sync()

    def stop(self):
        if self.viewer:
            self.viewer.close()
            self.viewer = None
        super().stop()

register_workload_type(MujocoWorldSimulator)
register_workload_type(MujocoInterface)
