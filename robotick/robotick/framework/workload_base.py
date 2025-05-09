import concurrent.futures
import threading
import time
from .registry import *

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
        self.tick_parent_name = None
        self._tick_parent = None
        self._tick_children = None
        self._last_tick_duration_self = 0
        self._lock = threading.Lock()
        self.state = StateContainer()

        self._outgoing_bindings = {}  # local_state -> list of (target_workload, target_state)
        self._incoming_bindings = {}  # local_state -> (source_workload, source_state)

        register_workload(self)

        self._thread = None  # thread is created later only if needed

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
        # fix up tick-parent
        if self.tick_parent_name:
            all_workloads = get_all_workload_instances()
            all_instances = [instance for instances in all_workloads.values() for instance in instances]

            # Find the workload instance whose name matches self.tick_parent_name
            self._tick_parent = next(
                (workload for workload in all_instances if workload.name == self.tick_parent_name),
                None
            )

            if self._tick_parent is not None:
                self.tick_rate_hz = 0 # parent ticks for us
                if self._tick_parent._tick_children is None:
                    self._tick_parent._tick_children = []
                self._tick_parent._tick_children.append(self)
            else:
                raise ValueError(f"Tick parent '{self.tick_parent_name}' not found among workloads")

        # Only top-level workloads need their own thread
        if self.tick_rate_hz > 0 and not self._tick_parent:
            self._thread = threading.Thread(target=self._run_loop)
            self._thread.start()

    def stop(self):
        if self._thread:
            self._stop_event.set()
            self._thread.join()

    def pre_tick(self, time_delta):
        pass

    def tick(self, time_delta):
        pass

    def _run_loop(self):
        last_time = time.perf_counter()

        # Only create a thread pool if we have children
        executor = None
        if hasattr(self, '_tick_children') and self._tick_children:
            executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

        while not self._stop_event.is_set():
            now = time.perf_counter()
            time_delta = now - last_time
            last_time = now

            self.pre_tick(time_delta)

            # Submit child workload task
            child_future = None
            if executor:
                def child_task():
                    for child in self._tick_children:
                        child_start = time.perf_counter()
                        child.pre_tick(time_delta)
                        child.tick(time_delta)
                        child._last_tick_duration_self = time.perf_counter() - child_start

                child_future = executor.submit(child_task)

            # Run parent tick
            parent_start = time.perf_counter()
            self.tick(time_delta)
            self._last_tick_duration_self = time.perf_counter() - parent_start

            # Wait for child task to finish before next cycle
            if child_future:
                child_future.result()

            # Calculate cycle timing
            tick_interval = self.get_tick_interval()
            elapsed = time.perf_counter() - now
            sleep_time = max(0, tick_interval - elapsed)
            if sleep_time > 0:
                time.sleep(sleep_time)

        # Clean up the executor on stop
        if executor:
            executor.shutdown(wait=True)

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
