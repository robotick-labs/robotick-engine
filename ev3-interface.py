from robotick.framework.composer import load
import time

def main():
    system = load('brickpi3_simple_rc.yaml')
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        system['stop_all']()

if __name__ == "__main__":
    main()
