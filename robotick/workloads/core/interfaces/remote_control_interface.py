import os
import socket
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from ....framework.workload_base import WorkloadBase
from ....framework.registry import *

class RemoteControlInterface(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz = 0 # no need to tick - we're just somewhere to store data and serve our static web page from

        self.state.writable['left_stick'] = [0, 0]
        self.state.writable['right_stick'] = [0, 0]

        self._server_thread = None
        self._httpd = None

    def get_local_ip(self):
        """Gets the IP address that remote machines can use to connect to this host."""
        try:
            # Connect to a dummy address to determine the preferred outbound interface
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 80))  # Doesn't actually send packets
            ip = s.getsockname()[0]
            s.close()
            return ip
        except Exception:
            return '127.0.0.1'
        
    def setup(self):
        # Set directory to serve (relative to script location)
        web_dir = os.path.join(os.path.dirname(__file__), 'remote_control_interface_web')
        os.chdir(web_dir)

        handler = SimpleHTTPRequestHandler
        self._httpd = ThreadingHTTPServer(('0.0.0.0', 8080), handler)
        self._server_thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._server_thread.start()

        ip = self.get_local_ip()

        print(f"RemoteControlInterface - web server running at http://{ip}:8080")

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

register_workload_type(RemoteControlInterface)
