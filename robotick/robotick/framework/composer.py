import importlib
import pkgutil
import yaml

import robotick.workloads 

from .registry import get_workload_type

def auto_import_workloads(package):
    """Recursively import all modules and subpackages in a package."""
    for loader, module_name, is_pkg in pkgutil.iter_modules(package.__path__):
        full_module_name = f"{package.__name__}.{module_name}"
        importlib.import_module(full_module_name)

        if is_pkg:
            subpackage = importlib.import_module(full_module_name)
            auto_import_workloads(subpackage)

def load(config_file):
    """Load workloads from YAML config and start them."""

    auto_import_workloads(robotick.workloads)

    with open(config_file) as f:
        config = yaml.safe_load(f)  # ← use safe_load for security

    instances = []

    for workload_cfg in config.get('workloads', []):
        type_name = workload_cfg['type']
        name_arg = workload_cfg.get('name')
        args = workload_cfg.get('args', {})

        cls = get_workload_type(type_name)
        if not cls:
            raise ValueError(f"Unknown workload type: {type_name}")

        instance = cls()

        setattr(instance, "name", name_arg)
        
        for key, value in args.items():
            setattr(instance, key, value)

        instances.append(instance)
    
    # ALL workloads are now registered at this point

    for inst in instances:
        inst.load()

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
