import subprocess
import socket
import time
import tempfile
import os

class MqttBroker:
    def __init__(self, mqtt_port=1883, websocket_port=9001):
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
            time.sleep(0.5)  # Give it time to start

            mqtt_ok = self.is_port_open(self.mqtt_port)
            ws_ok = self.is_port_open(self.websocket_port)

            if mqtt_ok and ws_ok:
                print(f"MqttBroker - started new broker on ports {self.mqtt_port} (MQTT) and {self.websocket_port} (WebSocket)")
            else:
                print("MqttBroker - failed to start broker on one or both ports")
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
