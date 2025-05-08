import mujoco
import mujoco.viewer
import json

from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class MujocoSimulator(WorkloadBase):
    def __init__(self):
        super().__init__()

        self.tick_rate_hz = 0 # use mujoco's by default (we fetch that once we've loaded the data-file in setup() below)

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

        # Inputs → actuators
        self._input_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_ACTUATOR, i) or f'actuator_{i}'
            for i in range(self.model.nu)
        ]
        self._writable_states = self._input_names
        for name in self._input_names:
            setattr(self, name, 0.0)

        # Outputs → grouped qpos/qvel

        nsensordata = self.model.nsensordata

        # Build sensor names
        self._sensor_names = [
            mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_SENSOR, i) or f'sensor_{i}'
            for i in range(self.model.nsensor)
        ]
        self._readable_states = self._sensor_names + self._input_names

        # Initialize attributes
        for name in self._sensor_names:
            setattr(self, name, 0.0)

        # Timestep override
        if self.tick_rate_hz > 0:
            self.model.opt.timestep = 1.0 / self.tick_rate_hz
        else:
            self.tick_rate_hz = int(1.0 / self.model.opt.timestep)

        if self.display_enabled:
            self.viewer = mujoco.viewer.launch_passive(self.model, self.data)

    def tick(self, time_delta):
        # Apply controls
        for i, name in enumerate(self._input_names):
            val = self.safe_get(name)
            self.data.ctrl[i] = val if val is not None else 0.0

        # Update sensor values
        offset = 0
        for i, name in enumerate(self._sensor_names):
            dim = self.model.sensor_dim[i]
            values = self.data.sensordata[offset:offset + dim]
            if dim == 1:
                self.safe_set(name, values[0])
            else:
                self.safe_set(name, json.dumps(list(values)))
            offset += dim


        # Step simulation
        mujoco.mj_step(self.model, self.data)

        if self.viewer:
            self.viewer.sync()


    def stop(self):
        # Close viewer if active
        if self.viewer:
            self.viewer.close()
            self.viewer = None
        super().stop()

# Register class on import
register_workload_type(MujocoSimulator)