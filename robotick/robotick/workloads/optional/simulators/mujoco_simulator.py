import mujoco
import mujoco.viewer
import json
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class MujocoSimulator(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0

        self.xml_path = ""
        self.display_enabled = False

        self.model = None
        self.data = None
        self.viewer = None

        self._input_names = []
        self._output_names = []

    def load(self):
        self.model = mujoco.MjModel.from_xml_path(self.xml_path)
        self.data = mujoco.MjData(self.model)

        self._input_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_ACTUATOR, i) or f'actuator_{i}'
            for i in range(self.model.nu)
        ]
        for name in self._input_names:
            self.state.writable[name] = 0.0

        nsensordata = self.model.nsensordata
        self._sensor_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_SENSOR, i) or f'sensor_{i}'
            for i in range(self.model.nsensor)
        ]
        for name in self._sensor_names:
            self.state.readable[name] = 0.0

        if self.tick_rate_hz > 0:
            self.model.opt.timestep = 1.0 / self.tick_rate_hz
        else:
            self.tick_rate_hz = int(1.0 / self.model.opt.timestep)

    def setup(self):
        if self.display_enabled:
            self.viewer = mujoco.viewer.launch_passive(self.model, self.data)

    def tick(self, time_delta):
        for i, name in enumerate(self._input_names):
            val = self.safe_get(name)
            self.data.ctrl[i] = val if val is not None else 0.0

        offset = 0
        for i, name in enumerate(self._sensor_names):
            dim = self.model.sensor_dim[i]
            values = self.data.sensordata[offset:offset + dim]
            if dim == 1:
                self.safe_set(name, values[0])
            else:
                self.safe_set(name, list(values))
            offset += dim

        mujoco.mj_step(self.model, self.data)
        if self.viewer:
            self.viewer.sync()

    def stop(self):
        if self.viewer:
            self.viewer.close()
            self.viewer = None
        super().stop()

register_workload_type(MujocoSimulator)
