# mqtt_broker.py

import subprocess
import socket
import time
import tempfile
import os

class MqttBroker:
    def __init__(self, mqtt_port, websocket_port):
        self.mqtt_port = mqtt_port
        self.websocket_port = websocket_port
        self._process = None
        self._config_file = None

    def is_port_open(self, port):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(1)
            return s.connect_ex(('localhost', port)) == 0

    def _write_temp_config(self):
        config = f"""
        listener {self.mqtt_port}
        protocol mqtt

        listener {self.websocket_port}
        protocol websockets

        allow_anonymous true
        """
        temp = tempfile.NamedTemporaryFile(delete=False, mode='w', suffix='.conf')
        temp.write(config)
        temp.close()
        self._config_file = temp.name
        print(f"MqttBroker - created temp config at {self._config_file}")

    def wait_for_ports_open(self, timeout=5.0, interval=0.1):
        """Wait until both MQTT and WebSocket ports are open or timeout occurs."""
        start_time = time.time()
        while time.time() - start_time < timeout:
            mqtt_ok = self.is_port_open(self.mqtt_port)
            ws_ok = self.is_port_open(self.websocket_port)
            if mqtt_ok and ws_ok:
                return True
            time.sleep(interval)
        return False

    def start(self):
        if self.is_port_open(self.mqtt_port) or self.is_port_open(self.websocket_port):
            print(f"MqttBroker - using existing broker on ports {self.mqtt_port}/{self.websocket_port}")
            return

        self._write_temp_config()

        cmd = [
            'mosquitto',
            '-c', self._config_file
        ]

        try:
            self._process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            if self.wait_for_ports_open():
                print(f"MqttBroker - started new broker on ports {self.mqtt_port} (MQTT) and {self.websocket_port} (WebSocket)")
            else:
                print(f"MqttBroker - failed to start broker on port(s): "
                    f"{'[mqtt]' if not self.is_port_open(self.mqtt_port) else ''} "
                    f"{'[websocket]' if not self.is_port_open(self.websocket_port) else ''}")

        except FileNotFoundError:
            print("Mosquitto is not installed or not in PATH. Please install it (e.g., `sudo apt install mosquitto`).")

    def stop(self):
        if self._process:
            self._process.terminate()
            try:
                self._process.wait(timeout=5)
                print("MqttBroker - broker stopped")
            except subprocess.TimeoutExpired:
                self._process.kill()
                print("MqttBroker - broker forcibly killed")
            self._process = None

        if self._config_file and os.path.exists(self._config_file):
            os.remove(self._config_file)
            print(f"MqttBroker - deleted temp config {self._config_file}")
            self._config_file = None

    def is_running(self):
        return self._process is not None and self._process.poll() is None
