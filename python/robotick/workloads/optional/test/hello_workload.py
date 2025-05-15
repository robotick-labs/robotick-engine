class HelloWorkload:
    def __init__(self, config):
        print(f"[Python] HelloWorkload init! {config["tick_rate_hz"]}Hz")

    def tick(self, time_delta, input, output):
        if input.get("force_error"):
            raise Exception("Simulated failure")

        if input.get("output_all"):
            output['val1'] = 1.23
            output['val2'] = 4.56
            return

        if input.get("no_output"):
            return

        output['greeting'] = 42.0

        print(f"[Python] Hello! {1.0/time_delta} Hz")
