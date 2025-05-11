_registered_workload_instances = {}

_registered_workload_types = {}

def register_workload_type(cls):
    """Register a workload class using its class name as type name."""
    type_name = cls.__name__
    _registered_workload_types[type_name] = cls

def get_workload_type(type_name):
    return _registered_workload_types.get(type_name)

def get_all_workload_types():
    return _registered_workload_types

def register_workload(instance):
    cls_name = type(instance).__name__
    snake_case = ''.join(['_' + c.lower() if c.isupper() else c for c in cls_name]).lstrip('_')
    if snake_case not in _registered_workload_instances:
        _registered_workload_instances[snake_case] = []

    instance._type_name = snake_case;
    _registered_workload_instances[snake_case].append(instance)

def get_all_workload_instances():
    return _registered_workload_instances

def get_all_workload_instances_of_type(cls_name):
    return _registered_workload_instances.get(cls_name, [])
