import numpy as np
from .workload_base import WorkloadBase
from .registry import *
from collections import namedtuple

# transformer nodes have no tick() function - just safe_set and safe_get - in which
# they transform input signal(s) to output signal(s) on-demand
import inspect
from robotick.framework.workload_base import WorkloadBase  # adjust as needed

class TransformerBase(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0

        self.on_construction()

        self._input_arg_names = list(inspect.signature(self.transform).parameters.keys())
        self._return_names = self._get_declared_or_default_outputs()

        # Register state keys
        for name in self._input_arg_names:
            self.state.writable[name] = None
        for name in self._return_names:
            self.state.readable[name] = None

    def _get_declared_or_default_outputs(self):
        if hasattr(self, 'get_output_names'):
            return self.get_output_names()
        return ['output_0']  # fallback single output

    def safe_set(self, attr, value):
        super().safe_set(attr, value)
        if attr in self._input_arg_names:
            self._trigger_transform()

    def safe_get(self, attr):
        if attr in self._return_names:
            self._trigger_transform()
            return super().safe_get(attr)
        return super().safe_get(attr)

    def _trigger_transform(self):
        args = [super().safe_get(name) for name in self._input_arg_names]
        result = self.transform(*args)

        if isinstance(result, tuple):
            for name, val in zip(self._return_names, result):
                super().safe_set(name, val)
        else:
            super().safe_set(self._return_names[0], result)


    def on_construction(self):
        pass

    def transform(self, *args):
        raise NotImplementedError("You must override transform(...) in your Transformer")

