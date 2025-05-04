from robotick.composer import load
import time

def main():
    system = load('ev3-interface-config.json')
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        system['stop_all']()

if __name__ == "__main__":
    main()
