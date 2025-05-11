import time

from robotick.framework import composer

# optional workloads - we need to explicitly import in order for them to get registered
from robotick.workloads.optional.interfaces import brickpi3_interface 

def main():
    system = composer.load('examples/mujoco_balancing_robot/config_brickpi3_simple_rc.yaml')
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        system['stop_all']()

if __name__ == "__main__":
    main()
