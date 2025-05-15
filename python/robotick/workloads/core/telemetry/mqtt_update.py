import json
import paho.mqtt.client as mqtt
from .mqtt_broker import MqttBroker

def get_all_workload_instances():
    return {}

class MqttUpdate:
    def __init__(self, config):
        super().__init__()

        self.config = config
        self.mqtt_port=7080
        self.websocket_port=7081
        self.client = mqtt.Client()
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self._last_published_value = {}
        self._mqtt_broker = MqttBroker(mqtt_port=self.mqtt_port, websocket_port=self.websocket_port)

    def load(self):
        print("MqttUpdate.setup - setting up broker...")
        self._mqtt_broker.start()

        print("MqttUpdate.setup - connecting to broker...")
        self.client.connect("localhost", self.mqtt_port, 60)
        self.client.loop_start()

        print("MqttUpdate.setup - complete")

    def _on_connect(self, client, userdata, flags, rc):
        print("Connected to MQTT broker")
        for type_name, instances in get_all_workload_instances().items():
            for inst in instances:
                name = getattr(inst, 'name', '') or ''
                for state in inst.get_writable_states():
                    topic = f"control/{name}/{state}"
                    value = inst.safe_get(state)
                    payload = self._format_payload(value)
                    client.publish(topic, payload, retain=True)
                    client.subscribe(topic)

                for state in inst.get_readable_states():
                    topic = f"state/{name}/{state}"
                    value = inst.safe_get(state)
                    payload = self._format_payload(value)
                    client.publish(topic, payload, retain=True)

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode()
        parts = topic.split('/')

        if parts[0] == 'control' and len(parts) >= 3:
            type_name = parts[1]
            name, state = (parts[2], parts[3]) if len(parts) == 4 else (None, parts[2])
            all_instances = get_all_workload_instances().get(type_name) or []
            instances = [i for i in all_instances if getattr(i, 'name', None) == name] if name else all_instances[:1]

            if instances:
                inst = instances[0]
                if state in inst.get_writable_states():
                    try:
                        parsed_value = self._parse_payload(payload)
                        inst.safe_set(state, parsed_value)
                    except Exception as e:
                        print(f"Warning: failed to set value for {state}: {payload} ({e})")

    def tick(self, time_delta, input, output):
        for type_name, instances in get_all_workload_instances().items():
            for inst in instances:
                name = getattr(inst, 'name', '') or ''
                inst_last = self._last_published_value.setdefault(type_name, {}).setdefault(name, {})
                for state in inst.get_readable_states():
                    current_value = inst.safe_get(state)
                    last_value = inst_last.get(state)
                    if current_value != last_value:
                        topic = f"state/{name}/{state}"
                        payload = self._format_payload(current_value)
                        self.client.publish(topic, payload, retain=True)
                        inst_last[state] = current_value

    def _parse_payload(self, payload):
        try:
            return int(payload)
        except ValueError:
            try:
                return json.loads(payload)
            except json.JSONDecodeError:
                raise ValueError("Invalid payload format")

    def _format_payload(self, value):
        if isinstance(value, int):
            return str(value)
        try:
            return json.dumps(value)
        except TypeError:
            return str(value)

