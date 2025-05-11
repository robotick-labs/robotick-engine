import importlib
import pkgutil
import yaml
from concurrent.futures import ThreadPoolExecutor

import robotick.workloads.core 

from .registry import *

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

    print("*** Composer - importing workload modules... ***")
    auto_import_workloads(robotick.workloads.core)

    print(f"*** Composer - loading config '{config_file}'... ***")
    with open(config_file) as f:
        config = yaml.safe_load(f)  # ← use safe_load for security

    for workload_cfg in config.get('workloads', []):
        type_name = workload_cfg['type']
        name_arg = workload_cfg.get('name')
        args = workload_cfg.get('args', {})

        cls = get_workload_type(type_name)
        if not cls:
            raise ValueError(f"Unknown workload type: {type_name}")

        instance = cls()

        if name_arg:
            setattr(instance, "name", name_arg)
        
        for key, value in args.items():
            setattr(instance, key, value)

        # store raw bindings if present
        if 'data_bindings' in args:
            instance.data_bindings = args['data_bindings']
        else:
            instance.data_bindings = []
    
    # ALL workloads are now registered at this point

    all_workloads = get_all_workload_instances() 
    all_instances = [instance for instances in all_workloads.values() for instance in instances]

    print("*** Composer - pre-load... ***")
    for inst in all_instances:
        inst.pre_load()

    # allow each instance to do independent time-consuming loading - multithreaded
    print("*** Composer - load... ***")
    with ThreadPoolExecutor() as executor:
        executor.map(lambda inst: inst.load(), all_instances)

    # allow instances to do fixup (e.g. to each other) - single-threaded
    print("*** Composer - setup... ***")
    for inst in all_instances:
        if hasattr(inst, 'data_bindings') and inst.data_bindings is not None:
            inst.parse_bindings(inst.data_bindings, all_workloads)
        inst.setup()

    print("*** Composer - start... ***")
    for inst in all_instances:
        inst.start()

    def stop_all():
        print("*** Composer - stop... ***")
        for inst in all_instances:
            inst.stop()

    return {
        'instances': all_instances,
        'stop_all': stop_all
    }
