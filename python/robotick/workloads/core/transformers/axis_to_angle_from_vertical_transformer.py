import numpy as np
from ....framework.transformer_base import TransformerBase
from ....framework.registry import *

class AxisToAngleFromVerticalTransformer(TransformerBase):
 
    def on_construction(self):
        self.axis_name = "x"

    def get_output_names(self):
        return ['output_angle']
    
    def setup(self):
        axis_map = {'x': 0, 'y': 1, 'z': 2}
        self.axis_idx = axis_map.get(self.axis_name.lower())    

        if self.axis_idx is None:
            raise ValueError(f"Invalid axis_name '{self.axis_name}', must be 'x', 'y', or 'z'")    
     
    def transform(self, input_vector):
        vec = np.array(input_vector)

        if vec.size < 3:
            return 0.0

        # Compute angle against global up (approximate tilt)
        angle = np.arcsin(-vec[self.axis_idx])

        # if we return as a tuple then the system can use its name:
        return angle

register_workload_type(AxisToAngleFromVerticalTransformer)
