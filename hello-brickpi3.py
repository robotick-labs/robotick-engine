import time
import threading
import sys
import select
from rich.console import Console
from rich.live import Live
from rich.table import Table
from brickpi3 import BrickPi3

# Setup
BP = BrickPi3()
BP.reset_all()

console = Console()
motor_powers = ["-"] * 4  # "-" if motor not active
sensor_values = ["-"] * 4  # "-" if sensor not configured
detected_sensor_types = ["-"] * 4
motor_ports = [BP.PORT_A, BP.PORT_B, BP.PORT_C, BP.PORT_D]
sensor_ports = [BP.PORT_1, BP.PORT_2, BP.PORT_3, BP.PORT_4]
pressed_keys = set()
shift_pressed = False
running = True

# ====== ✅ CONFIGURE SENSORS HERE ======
sensor_config = {
    BP.PORT_1: ("GYRO_ABS_DPS", BrickPi3.SENSOR_TYPE.EV3_GYRO_ABS_DPS),
    BP.PORT_2: ("TOUCH", BrickPi3.SENSOR_TYPE.TOUCH),
    BP.PORT_3: ("INFRARED_PROXIMITY", BrickPi3.SENSOR_TYPE.EV3_INFRARED_PROXIMITY),
    # BP.PORT_4: ("COLOR_REFLECTED", BrickPi3.SENSOR_TYPE.EV3_COLOR_REFLECTED),
}
# ========================================

def setup_sensors():
    for i, port in enumerate(sensor_ports):
        if port in sensor_config:
            name, sensor_type = sensor_config[port]
            try:
                BP.set_sensor_type(port, sensor_type)
                detected_sensor_types[i] = name
            except Exception as e:
                detected_sensor_types[i] = "Setup Error"
                print(f"Error setting up sensor on port {port}: {e}")
        else:
            detected_sensor_types[i] = "-"
    time.sleep(2)  # let it stabilize

def make_table():
    table = Table(title="BrickPi3 Status")
    table.add_column("Motor", justify="center")
    table.add_column("Power", justify="center")
    table.add_column("Sensor", justify="center")
    table.add_column("Type", justify="center")
    table.add_column("Value", justify="center")

    for i in range(4):
        # Motor power or "-"
        power_display = motor_powers[i] if motor_powers[i] != "-" else "-"

        # Sensor type
        sensor_type = detected_sensor_types[i]

        # Sensor value or "-"
        if sensor_type == "-":
            sensor_value = "-"
        else:
            try:
                sensor_data = BP.get_sensor(sensor_ports[i])
                sensor_value = str(sensor_data)
            except Exception:
                sensor_value = "Error"
        table.add_row(
            f"{i + 1}",
            str(power_display),
            f"{i + 1}",
            sensor_type,
            sensor_value
        )
    return table

def update_display():
    with Live(make_table(), console=console, refresh_per_second=5) as live:
        while running:
            live.update(make_table())
            time.sleep(0.2)

def control_motors():
    while running:
        for i, port in enumerate(motor_ports):
            if str(i + 1) in pressed_keys:
                power = -50 if shift_pressed else 50
                motor_powers[i] = power
                BP.set_motor_power(port, power)
            else:
                motor_powers[i] = "0"
                BP.set_motor_power(port, 0)
        time.sleep(0.1)

def read_input():
    global shift_pressed, running
    print("Press 1-4 to toggle motors, SHIFT + 1-4 for reverse, Enter to stop all, q to quit.")
    while running:
        if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
            ch = sys.stdin.read(1)
            if ch == 'q':
                running = False
                break
            elif ch == '\n':
                pressed_keys.clear()
                shift_pressed = False
            elif ch in '1234':
                if ch in pressed_keys:
                    pressed_keys.remove(ch)
                else:
                    pressed_keys.add(ch)
            elif ch == '\x1b':  # Escape sequence
                next1 = sys.stdin.read(1)
                if next1 == '[':
                    next2 = sys.stdin.read(1)
                    if next2 == '2':  # Insert = SHIFT on some keyboards
                        shift_pressed = not shift_pressed

def stop_all_motors():
    for port in motor_ports:
        BP.set_motor_power(port, 0)

# Switch terminal to raw mode
import tty, termios
fd = sys.stdin.fileno()
old_settings = termios.tcgetattr(fd)
tty.setcbreak(fd)

try:
    # Setup sensors from explicit config
    print("Setting up sensors (explicit config)...")
    setup_sensors()
    print("Setup complete:", detected_sensor_types)

    display_thread = threading.Thread(target=update_display, daemon=True)
    motor_thread = threading.Thread(target=control_motors, daemon=True)
    display_thread.start()
    motor_thread.start()

    read_input()
finally:
    running = False
    stop_all_motors()
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    print("\nMotors stopped. Exiting program.")
