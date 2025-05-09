import numpy as np
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class AxisToAngleTransformer(WorkloadBase):
    def __init__(self):
        super().__init__()

        # Input vector (from framexaxis, frameyaxis, framezaxis)
        self.state.writable['input_vector'] = [1, 0, 0]

        # Configurable axis: 'x', 'y', or 'z'
        self.axis_name = 'x'

        # Output angle in radians
        self.state.readable['output_value'] = 0.0

    def safe_set(self, attr, value):
        super().safe_set(attr, value)

        if attr == 'input_vector':
            angle = self.compute_axis_angle(value)
            super().safe_set('output_value', angle)

    def safe_get(self, attr):
        if attr == 'output_value':
            input_vec = super().safe_get('input_vector')
            angle = self.compute_axis_angle(input_vec)
            return angle
        else:
            return super().safe_get(attr)

    def compute_axis_angle(self, vec):
        vec = np.array(vec)

        if vec.size < 3:
            return 0.0
        
        axis_map = {'x': 0, 'y': 1, 'z': 2}
        axis_idx = axis_map.get(self.axis_name.lower())

        if axis_idx is None:
            raise ValueError(f"Invalid axis_name '{self.axis_name}', must be 'x', 'y', or 'z'")

        # Compute angle against global up (approximate tilt)
        angle = np.arcsin(-vec[axis_idx])
        return angle

register_workload_type(AxisToAngleTransformer)
