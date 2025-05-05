import json

from robotick.devices import motor_device, sensor_device
from robotick.workloads import console_update, mqtt_update

def load(config_file):
    """Load workloads from JSON config and start them."""

    module_lookup = {
        'MotorDevice': motor_device,
        'SensorDevice': sensor_device,
        'MqttUpdate': mqtt_update,
        'ConsoleUpdate': console_update
    }

    with open(config_file) as f:
        config = json.load(f)

    instances = []

    for workload_cfg in config.get('workloads', []):
        type_name = workload_cfg['type']
        name_arg = workload_cfg.get('name')
        args = workload_cfg.get('args', {})

        module = module_lookup.get(type_name)
        if not module:
            raise ValueError(f"Unknown workload type: {type_name}")

        cls = getattr(module, type_name)

        instance = cls()

        setattr(instance, "name", name_arg)
        
        for key, value in args.items():
            setattr(instance, key, value)

        instances.append(instance)
    
     # ALL workloads are now registered at this point

    for inst in instances:
        inst.setup()

    for inst in instances:
        inst.start()

    def stop_all():
        for inst in instances:
            inst.stop()

    return {
        'instances': instances,
        'stop_all': stop_all
    }
