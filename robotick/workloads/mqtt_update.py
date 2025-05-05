import paho.mqtt.client as mqtt
from ..framework.workload_base import WorkloadBase
from ..framework.registry import get_all

class MqttUpdate(WorkloadBase):
    def __init__(self):
        super().__init__()
        self.tick_rate_hz=30
        self.broker_host = "localhost"
        self.broker_port = 1883
        self.client = mqtt.Client()
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self._last_published_value = {}  # to track last known published values

    def setup(self):
        self.client.connect(self.broker_host, self.broker_port, 60)
        self.client.loop_start()

    def _on_connect(self, client, userdata, flags, rc):
        print("Connected to MQTT broker")

        # subscribe to all writable states
        for type_name, instances in get_all().items():
            for inst in instances:
                for state in inst.get_writable_states():
                    topic = f"control/{type_name}/{getattr(inst, 'name', repr(inst))}/{state}"

                    # public current state of this item
                    value = inst.safe_get(state)
                    client.publish(topic, str(value), retain=True)

                    # subscribe to be notified of changes to this item
                    client.subscribe(topic)

        # publish initial readable states
        for type_name, instances in get_all().items():
            for inst in instances:
                for state in inst.get_readable_states():
                    value = inst.safe_get(state)
                    topic = f"state/{type_name}/{getattr(inst, 'name', repr(inst))}/{state}"
                    client.publish(topic, str(value), retain=True)

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode()

        # parse topic to find instance + state
        parts = topic.split('/')
        if len(parts) >= 4 and parts[0] == 'control':
            _, type_name, name, state = parts[:4]
            instances = [i for i in get_all().get(type_name, []) if getattr(i, 'name', None) == name]
            if instances:
                inst = instances[0]
                if state in inst.get_writable_states():
                    try:
                        inst.safe_set(state, int(payload))
                    except ValueError:
                        print(f"Invalid value for {state}: {payload}")

    def tick(self, time_delta):
        """Poll all readable states and publish any changes."""
        for type_name, instances in get_all().items():
            for inst in instances:
                for state in inst.get_readable_states():
                    current_value = inst.safe_get(state)
                    last_value = self._last_published_value \
                        .setdefault(type_name, {}) \
                        .setdefault(inst.name, {}) \
                        .get(state)

                    if current_value != last_value:
                        topic = f"state/{type_name}/{getattr(inst, 'name', repr(inst))}/{state}"
                        self.client.publish(topic, str(current_value), retain=True)
                        self._last_published_value[type_name][inst.name][state] = current_value
