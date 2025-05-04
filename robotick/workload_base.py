import threading
import time

from .registry import register_workload

class WorkloadBase:
    def __init__(self, tick_rate_hz=100, name=None):
        self._tick_rate_hz = tick_rate_hz
        self._target_dt = 1.0 / tick_rate_hz
        self._thread = None
        self._running = threading.Event()
        self._lock = threading.Lock()
        self._readable_states = []
        self._writable_states = []
        self.name = name or self.__class__.__name__.lower()
        register_workload(self)

    def setup(self):
        """Optional post-registration setup step. Override in subclasses."""
        pass

    def get_readable_states(self):
        return self._readable_states

    def get_writable_states(self):
        return self._writable_states

    def start(self):
        if self._thread is None or not self._thread.is_alive():
            self._running.set()
            self._thread = threading.Thread(target=self._run_loop, daemon=True)
            self._thread.start()

    def stop(self):
        self._running.clear()
        if self._thread:
            self._thread.join()

    def _run_loop(self):
        last_time = time.perf_counter()
        while self._running.is_set():
            now = time.perf_counter()
            time_delta = now - last_time
            last_time = now

            self.tick(time_delta)

            elapsed = time.perf_counter() - now
            sleep_time = max(0, self._target_dt - elapsed)
            time.sleep(sleep_time)

    def tick(self, time_delta):
        """Override in subclass to define per-tick behavior."""
        pass

    def safe_get(self, attr):
        with self._lock:
            return getattr(self, attr)

    def safe_set(self, attr, value):
        with self._lock:
            setattr(self, attr, value)
