import paho.mqtt.client as mqtt
import time
import sys
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
        return random.randint(50, 60)  # fake sensor data

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

def move_cursor_up(lines=1):
    sys.stdout.write(f"\x1b[{lines}A")
    sys.stdout.flush()

try:
    # print once outside loop
    print("== Raspberry Pi 2 Control Panel ==")
    print()

    lines_to_move_up = 0

    while True:
        # update all motors
        for motor in motors.values():
            motor.update()

        # move cursor back to top ONCE
        sys.stdout.write(f"\x1b[{lines_to_move_up}F")
        sys.stdout.flush()

        sys.stdout.write("Motors:\x1b[K\n")
        for mname, m in motors.items():
            actual = m.get_speed()
            goal = m.get_goal_speed()
            sys.stdout.write(f"  {mname}: actual = {actual}, goal = {goal}\x1b[K\n")

        sys.stdout.write("\nSensors:\x1b[K\n")
        for sname, sensor in sensors.items():
            value = sensor.read()
            sys.stdout.write(f"  {sname}: value = {value}\x1b[K\n")
            client.publish(f"status/sensor/{sname}", value, retain=True)

        sys.stdout.flush()
        time.sleep(0.1)
        
        lines_to_move_up = 3 + len(motors) + len(sensors)


except KeyboardInterrupt:
    print("\nExiting...")

client.loop_stop()
client.disconnect()
