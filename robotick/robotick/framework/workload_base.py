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
        return self.state.safe_get(attr, self._lock)

    def safe_set(self, attr, value):
        self.state.safe_set(attr, value, self._lock)

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
