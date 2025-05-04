import json
import importlib

def load(config_file):
    """Load workloads from JSON config and start them."""
    from robotick import motor_device, sensor_device, mqtt_update, console_update

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

        instance = cls(name_arg, **args) if name_arg else cls(**args)
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
