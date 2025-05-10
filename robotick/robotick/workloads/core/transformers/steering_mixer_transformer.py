from ....framework.transformer_base import TransformerBase
from ....framework.registry import *

class SteeringMixerTransformer(TransformerBase):
    def on_construction(self):
        self.max_speed_differential = 0.4
        self.power_scale = 1.0

    def get_output_names(self):
        return ['output_left_motor', 'output_right_motor'] 
    
    def transform(self, input_speed, input_turn_rate):

        input_speed = input_speed or 0
        input_turn_rate = input_turn_rate or 0
        
        left = input_speed + input_turn_rate * self.max_speed_differential
        right = input_speed - input_turn_rate * self.max_speed_differential

        # Clamp to [-1, 1]
        left = -self.power_scale * max(min(left, 1), -1)
        right = self.power_scale * max(min(right, 1), -1)

        return ( left, right )

register_workload_type(SteeringMixerTransformer)
