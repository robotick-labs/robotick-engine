class HelloWorkload:

    @staticmethod
    def describe():
        return {
            "config": {},
            "inputs": {
                "no_output": "int",
                "force_error": "int"
            },
            "outputs": {
                "greeting": "FixedString64",
                "val_double": "double",
                "val_int": "int"
            }
        }
        
    def __init__(self, config):
        print("[Python] HelloWorkload __init__")

    def tick(self, time_delta, input, output):
        print("[Python] tick inputs:", dict(input))

        if input.get("force_error", 0):
            raise Exception("Simulated failure")

        if input.get("no_output", 0):
            return

        output['val_double'] = 1.23
        output['val_int'] = 456
        output['greeting'] = f"[Python] Hello! {1.0 / time_delta:.2f} Hz"

        print(output['greeting'])
