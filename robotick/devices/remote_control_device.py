from .io_device import IODevice

class RemoteControlDevice(IODevice):
    def __init__(self):
        super().__init__()
        
        self.left_stick = {'x': 0, 'y': 0}
        self.right_stick = {'x': 0, 'y': 0}

        self._readable_states = ['left_stick', 'right_stick']
        self._writable_states = ['left_stick', 'right_stick']

    # TODO - allow tick-rate to be 0, meaning "no tick please"

    def get_left_stick(self):
        return self.safe_get('left_stick')

    def get_right_stick(self):
        return self.safe_get('right_stick')
