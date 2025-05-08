import time

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "robotick")))

from robotick.framework import composer

# optional workloads - we need to explicitly import in order for them to get registered
from robotick.workloads.optional.devices import brickpi3_device 
from robotick.workloads.optional.simulators import mujoco_simulator 
from robotick.workloads.optional.examples import mujoco_simple_rc

def main():
    # system = composer.load('config_brickpi3_simple_rc.yaml')
    system = composer.load('config_mujoco_test.yaml')
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        system['stop_all']()

if __name__ == "__main__":
    main()
