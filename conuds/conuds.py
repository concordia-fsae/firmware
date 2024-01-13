import sys
from time import sleep

from can.interfaces.socketcan import SocketcanBus
from isotp import Address, CanStack, AddressingMode
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.client import Client
from udsoncan.exceptions import TimeoutException, NegativeResponseException

# from udsoncan.services import *


from conuds.timer import init, Timer, ProgramKilled


# udsoncan.setup_logging()


def get_client() -> Client:
    bus = SocketcanBus(channel="can0")

    # Network layer addressing scheme
    tp_addr = Address(AddressingMode.Normal_11bits, txid=0x456, rxid=0x123)

    # Network/Transport layer (IsoTP protocol)
    stack = CanStack(bus=bus, address=tp_addr)

    # Speed First (do not sleep)
    stack.set_sleep_timing(0, 0)

    # interface between Application and Transport layer
    conn = PythonIsoTpConnection(stack)

    # client = Client(conn, request_timeout=0.1, config={"exception_on_negative_response": False})
    return Client(conn, request_timeout=0.1)


def send_tp(client, response_required=True):
    if response_required:
        client.tester_present()
    else:
        with client.suppress_positive_response(wait_nrc=False):
            client.tester_present()


def do_stuff(event_loop: Timer):
    client = get_client()
    client.open()

    lastTick = 0

    while True:
        tick = event_loop.getTick()

        if lastTick != tick:
            print("sending tp ", tick)
            send_tp(client, False)

        lastTick = tick

        try:
            pass
        except NegativeResponseException as e:
            print(e)
        except TimeoutException:
            pass

        # try:
        #     client.start_routine(0xF00F)
        # except TimeoutException:
        #     pass


def main():
    init()

    event_loop = Timer(period_ms=10)
    event_loop.start()

    try:
        do_stuff(event_loop)
    except ProgramKilled:
        event_loop.stop()
        sys.exit(0)


if __name__ == "__main__":
    main()
