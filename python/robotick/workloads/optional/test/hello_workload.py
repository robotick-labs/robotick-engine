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
                "greeting": "double",
                "val1": "double",
                "val2": "double"
            }
        }
        
    def __init__(self, config):
        print("[Python] HelloWorkload __init__")

    def tick(self, time_delta, input, output):
        if input.get("force_error", 0):
            raise Exception("Simulated failure")

        if input.get("output_all", 0):
            output['val1'] = 1.23
            output['val2'] = 4.56
            return

        if input.get("no_output", 0):
            return

        output['greeting'] = 42.0
        print(f"[Python] Hello! {1.0 / time_delta:.2f} Hz")
