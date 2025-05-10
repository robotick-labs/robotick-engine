import numpy as np
from .workload_base import WorkloadBase
from .registry import *

# transformer nodes have no tick() function - just safe_set and safe_get - in which
# they transform input signal(s) to output signal(s) on-demand
import inspect
from robotick.framework.workload_base import WorkloadBase  # adjust as needed

class TransformerBase(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0  # No ticking
        self._input_arg_names = list(inspect.signature(self.transform).parameters.keys())
        self._return_names = None  # Inferred on first use

        # Register readable/writable states
        for name in self._input_arg_names:
            self.state.writable[name] = None
        for name in self._return_names_or_guess():
            self.state.readable[name] = None

    def _return_names_or_guess(self):
        if self._return_names is not None:
            return self._return_names

        args = [None for _ in self._input_arg_names]
        try:
            result = self.transform(*args)
        except Exception:
            result = None  # allow delayed inference

        if hasattr(result, '_fields'):  # NamedTuple
            self._return_names = list(result._fields)
        elif isinstance(result, tuple):
            self._return_names = [f"output_{i}" for i in range(len(result))]
        else:
            self._return_names = ['output']
        return self._return_names

    def safe_set(self, attr, value):
        super().safe_set(attr, value)
        if attr in self._input_arg_names:
            self._trigger_transform()

    def safe_get(self, attr):
        if attr in self._return_names_or_guess():
            self._trigger_transform()
            return super().safe_get(attr)
        return super().safe_get(attr)

    def _trigger_transform(self):
        args = [super().safe_get(name) for name in self._input_arg_names]
        result = self.transform(*args)

        # Map result to output fields
        if hasattr(result, '_fields'):  # NamedTuple
            for name in result._fields:
                super().safe_set(name, getattr(result, name))
        elif isinstance(result, tuple):
            for i, val in enumerate(result):
                super().safe_set(f"output_{i}", val)
        else:
            super().safe_set('output', result)

    def transform(self, *args):
        raise NotImplementedError("You must override transform(...) in your Transformer")
