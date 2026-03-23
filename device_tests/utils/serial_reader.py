"""Serial port reader with expect/drain functionality for device testing."""

import re
import time
import threading
from collections import deque

import serial


class SerialReader:
    """Non-blocking serial reader that accumulates output in a ring buffer.

    Usage:
        reader = SerialReader("COM8", baudrate=115200)
        reader.start()
        line = reader.expect(r"\\[Auth\\] Authentication system initialized", timeout=30)
        reader.stop()
    """

    DEFAULT_BUFFER_SIZE = 5000  # lines

    def __init__(self, port, baudrate=115200, buffer_size=None):
        self._port = port
        self._baudrate = baudrate
        self._buffer = deque(maxlen=buffer_size or self.DEFAULT_BUFFER_SIZE)
        self._lock = threading.Lock()
        self._serial = None
        self._thread = None
        self._running = False

    def start(self):
        """Open serial port and begin background reading."""
        self._serial = serial.Serial(self._port, self._baudrate, timeout=0.1)
        self._running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def stop(self):
        """Stop reading and close the serial port."""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        if self._serial and self._serial.is_open:
            self._serial.close()

    def _read_loop(self):
        """Background thread: read lines and append to buffer."""
        while self._running:
            try:
                if self._serial and self._serial.in_waiting:
                    raw = self._serial.readline()
                    if raw:
                        line = raw.decode("utf-8", errors="replace").rstrip()
                        with self._lock:
                            self._buffer.append(line)
            except (serial.SerialException, OSError):
                break

    def get_buffer(self):
        """Return a snapshot of all buffered lines."""
        with self._lock:
            return list(self._buffer)

    def get_last_n(self, n=50):
        """Return the last N lines from the buffer."""
        with self._lock:
            items = list(self._buffer)
        return items[-n:] if len(items) >= n else items

    def drain(self):
        """Clear the buffer and return all lines that were in it."""
        with self._lock:
            lines = list(self._buffer)
            self._buffer.clear()
        return lines

    def expect(self, pattern, timeout=10):
        """Wait for a line matching regex pattern. Returns the matching line or None."""
        regex = re.compile(pattern)
        deadline = time.time() + timeout
        seen_index = 0

        while time.time() < deadline:
            with self._lock:
                buf = list(self._buffer)
            for i in range(seen_index, len(buf)):
                if regex.search(buf[i]):
                    return buf[i]
            seen_index = len(buf)
            time.sleep(0.05)

        return None

    def expect_exact(self, substring, timeout=10):
        """Wait for a line containing the exact substring. Returns line or None."""
        deadline = time.time() + timeout
        seen_index = 0

        while time.time() < deadline:
            with self._lock:
                buf = list(self._buffer)
            for i in range(seen_index, len(buf)):
                if substring in buf[i]:
                    return buf[i]
            seen_index = len(buf)
            time.sleep(0.05)

        return None

    def count_matches(self, pattern):
        """Count lines matching regex pattern in the current buffer."""
        regex = re.compile(pattern)
        with self._lock:
            return sum(1 for line in self._buffer if regex.search(line))
