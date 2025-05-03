import paho.mqtt.client as mqtt
import time
import os
import random
import json

# ---- MOCK API ----
class Motor:
    def __init__(self, name):
        self.name = name
        self.speed = 0         # actual current speed
        self.goal_speed = 0    # desired target speed

    def set_goal_speed(self, speed):
        self.goal_speed = speed

    def update(self):
        """Simulate motor gradually reaching goal speed."""
        if self.speed < self.goal_speed:
            self.speed += 1
        elif self.speed > self.goal_speed:
            self.speed -= 1
        # else already at goal

    def get_speed(self):
        return self.speed

    def get_goal_speed(self):
        return self.goal_speed

class Sensor:
    def __init__(self, name):
        self.name = name

    def read(self):
        return random.randint(0, 100)  # fake sensor data

# ---- Setup motors and sensors ----
motors = {
    'left': Motor('left'),
    'right': Motor('right')
}

sensors = {
    'ultrasonic': Sensor('ultrasonic'),
    'light': Sensor('light')
}

# ---- MQTT callbacks ----
def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    # Subscribe dynamically
    for motor_name in motors.keys():
        client.subscribe(f"control/motor/{motor_name}")
    client.subscribe("control/stop")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()

    if topic.startswith("control/motor/"):
        motor_name = topic.split("/")[-1]
        if motor_name in motors:
            try:
                motors[motor_name].set_goal_speed(int(payload))
                # Publish updated goal (retained) when set
                client.publish(f"status/motor/{motor_name}/goal", motors[motor_name].get_goal_speed(), retain=True)
            except ValueError:
                print(f"Invalid speed value for {motor_name}: {payload}")
    elif topic == "control/stop":
        for motor_name, motor in motors.items():
            motor.set_goal_speed(0)
            client.publish(f"status/motor/{motor_name}/goal", motor.get_goal_speed(), retain=True)

# ---- MQTT setup ----
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

broker_address = "localhost"  # or broker IP if remote
client.connect(broker_address, 1883, 60)

# ---- Auto-generate available I/O manifest ----
available_io = {
    "control": { f"motor/{name}": "int: set desired goal speed (-100 to 100)" for name in motors.keys() }
}
available_io["control"]["stop"] = "signal: stop all motors"
available_io["status"] = {}
for name in motors.keys():
    available_io["status"][f"motor/{name}/goal"] = "int: current goal speed"
    available_io["status"][f"motor/{name}/actual"] = "int: current measured speed"
for name in sensors.keys():
    available_io["status"][f"sensor/{name}"] = "int: sensor reading"

client.publish("system/available_io", json.dumps(available_io), retain=True)

# ---- Publish initial goal speed (retained) ----
for motor_name, motor in motors.items():
    client.publish(f"status/motor/{motor_name}/goal", motor.get_goal_speed(), retain=True)

# Also publish initial sensor values (retained)
for sensor_name, sensor in sensors.items():
    sensor_value = sensor.read()
    client.publish(f"status/sensor/{sensor_name}", sensor_value, retain=True)

client.loop_start()

# ---- Console display ----
def clear_console():
    os.system('clear' if os.name == 'posix' else 'cls')

try:
    while True:
        clear_console()
        print("== Raspberry Pi 2 Control Panel ==")

        print("\nMotors:")
        for name, motor in motors.items():
            motor.update()  # update actual speed toward goal
            actual_speed = motor.get_speed()
            goal_speed = motor.get_goal_speed()
            print(f"  {name}: actual = {actual_speed}, goal = {goal_speed}")
            client.publish(f"status/motor/{name}/actual", actual_speed, retain=True)

        print("\nSensors:")
        for name, sensor in sensors.items():
            value = sensor.read()
            print(f"  {name}: value = {value}")
            client.publish(f"status/sensor/{name}", value, retain=True)

        print("\nAvailable topics listed in 'system/available_io'")

        time.sleep(1)

except KeyboardInterrupt:
    print("\nExiting...")

client.loop_stop()
client.disconnect()
