import numpy as np
from ....framework.transformer_base import TransformerBase
from ....framework.registry import *

class DeadZoneScaleAndSplitTransformer(TransformerBase):
 
    def on_construction(self):
        self.dead_zone = 0.2
        self.scale_x = 1.0
        self.scale_y = 1.0

    def get_output_names(self):
        return ['output_x', 'output_y'] 
     
    def transform(self, input_vector):

        if input_vector is None or len(input_vector) < 2:
            return (0, 0)
        
        x = self.apply_dead_zone(input_vector[0], self.dead_zone)
        y = self.apply_dead_zone(input_vector[1], self.dead_zone)

        x *= self.scale_x
        y *= self.scale_y

        return (x, y)

    def apply_dead_zone(self, value, dead_zone):
        if abs(value) < dead_zone:
            return 0
        else:
            return (abs(value) - dead_zone) / (1 - dead_zone) * (1 if value > 0 else -1)

register_workload_type(DeadZoneScaleAndSplitTransformer)
