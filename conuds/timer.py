from datetime import timedelta
from signal import signal, SIGTERM, SIGINT
from threading import Thread, Event
from time import sleep

import sys


class ProgramKilled(Exception):
    pass


def signal_handler(signum, frame):
    raise ProgramKilled


class Timer(Thread):
    def __init__(self, period_ms: float) -> None:
        Thread.__init__(self)
        self.stopped = Event()
        self.interval_ms = timedelta(milliseconds=period_ms).total_seconds()
        print(self.interval_ms)
        self.tick: int = 0

    def stop(self) -> None:
        self.stopped.set()
        self.join()

    def run(self) -> None:
        while not self.stopped.is_set():
            sleep(self.interval_ms)
            self.tick += 1

    def getTick(self) -> int:
        return self.tick


def init():
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)

if __name__ == "__main__":
    init()
    timer = Timer(period_ms=10)
    timer.start()

    lastTick = 0
    try:
        while True:
            tick = timer.getTick()

            # if lastTick != tick:
            #     print(tick)

            if tick == 500:
                timer.stop()
                sys.exit(0)

            lastTick = tick
    except ProgramKilled:
        print("Program killed: running cleanup code")
        timer.stop()
