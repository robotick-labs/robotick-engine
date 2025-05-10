import os
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class RemoteControlDevice(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0 # no need to tick - we're just somewhere to store data and serve our static web page from

        self.state.writable['left_stick'] = [0, 0]
        self.state.writable['right_stick'] = [0, 0]

        self._server_thread = None
        self._httpd = None

    def setup(self):
        # Set directory to serve (relative to script location)
        web_dir = os.path.join(os.path.dirname(__file__), 'remote_control_device_web')
        os.chdir(web_dir)

        handler = SimpleHTTPRequestHandler
        self._httpd = ThreadingHTTPServer(('0.0.0.0', 8080), handler)
        self._server_thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._server_thread.start()
        print("RemoteControlDevice - web server running at http://localhost:8080")

    def stop(self):
        if self._httpd:
            self._httpd.shutdown()
            self._httpd.server_close()
            print("Web server on port 8080 stopped")
        super().stop()

    def get_left_stick(self):
        return self.safe_get('left_stick')

    def get_right_stick(self):
        return self.safe_get('right_stick')

register_workload_type(RemoteControlDevice)
