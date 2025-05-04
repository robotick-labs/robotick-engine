import threading
import time

class WorkloadBase:
    def __init__(self, tick_rate_hz=10, tick_parent=None):
        self._stop_event = threading.Event()
        self._tick_rate_hz = tick_rate_hz
        self._tick_interval = 1.0 / tick_rate_hz if tick_rate_hz else None
        self._tick_parent = tick_parent

        self._wake_event = threading.Event()
        self._done_event = threading.Event()

        # parent keeps list of child references
        if self._tick_parent:
            if not hasattr(self._tick_parent, '_tick_children'):
                self._tick_parent._tick_children = []
            self._tick_parent._tick_children.append(self)

        self._thread = threading.Thread(target=self._run_loop)

    def start(self):
        self._thread.start()

    def stop(self):
        self._stop_event.set()
        self._thread.join()

    def tick(self, time_delta):
        """Override in subclass"""
        pass

    def _run_loop(self):
        if self._tick_parent:
            # child workload: waits for parent signal
            while not self._stop_event.is_set():
                self._wake_event.wait()
                self._wake_event.clear()

                self.tick(None)  # optional: pass actual time_delta if needed

                self._done_event.set()
        else:
            # independent workload (parent)
            last_time = time.perf_counter()
            while not self._stop_event.is_set():
                now = time.perf_counter()
                time_delta = now - last_time
                last_time = now

                self.tick(time_delta)

                # wait for children to finish (if any)
                if hasattr(self, '_tick_children'):
                    for child in self._tick_children:
                        child._wake_event.set()
                    for child in self._tick_children:
                        child._done_event.wait()
                        child._done_event.clear()

                # keep tick rate
                if self._tick_interval:
                    elapsed = time.perf_counter() - now
                    sleep_time = self._tick_interval - elapsed
                    if sleep_time > 0:
                        time.sleep(sleep_time)
