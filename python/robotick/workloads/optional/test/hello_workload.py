class HelloWorkload:

    @staticmethod
    def describe():
        return {
            "config": {},
            "inputs": {
                "output_all": "int",
                "no_output": "int",
                "force_error": "int"
            },
            "outputs": {
                "greeting": "FixedString32",
                "val_double": "double",
                "val_int": "int"
            }
        }
        
    def __init__(self, config):
        print("[Python] HelloWorkload __init__")

    def tick(self, time_delta, input, output):
        if input.get("force_error", 0):
            raise Exception("Simulated failure")

        if input.get("output_all", 0):
            output['val_double'] = 1.23
            output['val_int'] = 456
            return

        if input.get("no_output", 0):
            return

        output['greeting'] = f"[Python] Hello! {1.0 / time_delta:.2f} Hz"

        print(output['greeting'])
