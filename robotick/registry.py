registry = {}

def register_workload(instance):
    cls_name = type(instance).__name__
    snake_case = ''.join(['_' + c.lower() if c.isupper() else c for c in cls_name]).lstrip('_')
    if snake_case not in registry:
        registry[snake_case] = []
    registry[snake_case].append(instance)

def get_all():
    return registry

def get_by_type(cls_name):
    return registry.get(cls_name, [])
