import threading
import time
from .registry import register_workload

class StateContainer:
    def __init__(self):
        self.readable = {}
        self.writable = {}

    def get_readable_states(self):
        return list(self.readable.keys()) + list(self.writable.keys())

    def get_writable_states(self):
        return list(self.writable.keys())

    def safe_get(self, attr, lock):
        with lock:
            if attr in self.writable:
                return self.writable[attr]
            return self.readable.get(attr, None)

    def safe_set(self, attr, value, lock):
        with lock:
            if attr in self.writable:
                self.writable[attr] = value
            elif attr in self.readable:
                self.readable[attr] = value
            else:
                raise AttributeError(f"Attribute {attr} not found in state container")

class WorkloadBase:
    def __init__(self):
        self._stop_event = threading.Event()
        self.tick_rate_hz = 10
        self._tick_parent = None
        self._last_tick_duration_self = 0
        self._lock = threading.Lock()
        self.state = StateContainer()

        self._outgoing_bindings = {}  # local_state -> list of (target_workload, target_state)
        self._incoming_bindings = {}  # local_state -> (source_workload, source_state)

        register_workload(self)

        self._wake_event = threading.Event()
        self._done_event = threading.Event()

        if self._tick_parent:
            if not hasattr(self._tick_parent, '_tick_children'):
                self._tick_parent._tick_children = []
            self._tick_parent._tick_children.append(self)

        self._thread = threading.Thread(target=self._run_loop)

    def get_last_tick_duration_self(self):
        return self._last_tick_duration_self

    def get_tick_interval(self):
        return 1.0 / self.tick_rate_hz if self.tick_rate_hz > 0 else 0

    def get_readable_states(self):
        return self.state.get_readable_states()

    def get_writable_states(self):
        return self.state.get_writable_states()

    def safe_get(self, attr):
        # Check incoming pull bindings first
        if attr in self._incoming_bindings:
            source_workload, source_attr = self._incoming_bindings[attr]
            return source_workload.safe_get(source_attr)

        # Otherwise get local value
        return self.state.safe_get(attr, self._lock)

    def safe_set(self, attr, value):
        self.state.safe_set(attr, value, self._lock)

        # Propagate to bound targets
        if attr in self._outgoing_bindings:
            for target_workload, target_attr in self._outgoing_bindings[attr]:
                target_workload.safe_set(target_attr, value)

    def pre_load(self):
        pass

    def load(self):
        pass

    def setup(self):
        pass

    def start(self):
        if self.tick_rate_hz > 0:
            self._thread.start()

    def stop(self):
        if self.tick_rate_hz > 0:
            self._stop_event.set()
            self._thread.join()

    def pre_tick(self, time_delta):
        pass

    def tick(self, time_delta):
        pass

    def _run_loop(self):
        if self._tick_parent:
            while not self._stop_event.is_set():
                self._wake_event.wait()
                self._wake_event.clear()
                self.tick(None)
                self._done_event.set()
        else:
            last_time = time.perf_counter()
            while not self._stop_event.is_set():
                now = time.perf_counter()
                time_delta = now - last_time
                last_time = now

                self.pre_tick(time_delta)

                if hasattr(self, '_tick_children'):
                    for child in self._tick_children:
                        child._wake_event.set()

                self.tick(time_delta)

                if hasattr(self, '_tick_children'):
                    for child in self._tick_children:
                        child._done_event.wait()
                        child._done_event.clear()

                tick_interval = self.get_tick_interval()
                elapsed = time.perf_counter() - now
                sleep_time = max(0, tick_interval - elapsed)
                self._last_tick_duration_self = elapsed

                if sleep_time > 0:
                    time.sleep(sleep_time)

    def parse_bindings(self, bindings_list, workloads):
        """Parse arrow bindings like 'local -> target.workload' or 'local <- source.workload'."""
        for binding_str in bindings_list:
            if '->' in binding_str:
                key, _, rest = binding_str.partition('->')
                direction = 'out'
            elif '<-' in binding_str:
                key, _, rest = binding_str.partition('<-')
                direction = 'in'
            else:
                continue  # skip invalid

            local_state = key.strip()
            workload_dot_state = rest.strip()
            workload_name, _, other_state = workload_dot_state.partition('.')

            # Find workload instance
            other_instance = None
            for _, instances in workloads.items():
                for inst in instances:
                    if getattr(inst, 'name', None) == workload_name:
                        other_instance = inst
                        break
            if not other_instance:
                raise ValueError(f"Cannot find workload '{workload_name}'")

            if direction == 'out':
                self._outgoing_bindings.setdefault(local_state, []).append((other_instance, other_state))
            else:
                self._incoming_bindings[local_state] = (other_instance, other_state)
