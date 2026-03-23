"""WebSocket client utility for on-device testing.

Provides a synchronous WebSocket client that connects to the firmware's
WebSocket server (port 81), handles the token-based auth handshake, and
offers helpers for sending commands and receiving JSON/binary responses.
"""

import json
import time
import threading
from collections import deque

import websocket


class DeviceWebSocket:
    """Synchronous WebSocket client for firmware testing.

    Usage::

        ws = DeviceWebSocket("192.168.4.1")
        ws.connect()
        ws.authenticate(token)
        resp = ws.recv_until("hardwareStats", timeout=5)
        ws.close()

    Or as a context manager::

        with DeviceWebSocket("192.168.4.1") as ws:
            ws.authenticate(token)
            ws.send_command("getHardwareStats")
            resp = ws.recv_until("hardwareStats")
    """

    DEFAULT_PORT = 81
    DEFAULT_TIMEOUT = 5  # seconds

    def __init__(self, device_ip, port=None):
        self._url = f"ws://{device_ip}:{port or self.DEFAULT_PORT}"
        self._ws = None
        self._json_queue = deque()
        self._binary_queue = deque()
        self._lock = threading.Lock()
        self._recv_thread = None
        self._running = False
        self._connected = False

    # ------------------------------------------------------------------
    # Context manager
    # ------------------------------------------------------------------

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def connect(self, timeout=5):
        """Open WebSocket connection to device."""
        self._ws = websocket.WebSocket()
        self._ws.settimeout(timeout)
        self._ws.connect(self._url)
        self._connected = True
        self._running = True
        self._recv_thread = threading.Thread(
            target=self._reader_loop, daemon=True,
        )
        self._recv_thread.start()

    def close(self):
        """Close the WebSocket connection."""
        self._running = False
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass
        if self._recv_thread and self._recv_thread.is_alive():
            self._recv_thread.join(timeout=2)
        self._connected = False

    @property
    def connected(self):
        return self._connected

    # ------------------------------------------------------------------
    # Background reader
    # ------------------------------------------------------------------

    def _reader_loop(self):
        """Read frames in the background and route to queues."""
        while self._running:
            try:
                opcode, data = self._ws.recv_data(control_frame=True)
            except websocket.WebSocketTimeoutException:
                continue
            except (
                websocket.WebSocketConnectionClosedException,
                OSError,
                ConnectionError,
            ):
                self._connected = False
                break

            if opcode == websocket.ABNF.OPCODE_TEXT:
                try:
                    msg = json.loads(data.decode("utf-8"))
                except (json.JSONDecodeError, UnicodeDecodeError):
                    continue
                with self._lock:
                    self._json_queue.append(msg)
            elif opcode == websocket.ABNF.OPCODE_BINARY:
                with self._lock:
                    self._binary_queue.append(data)
            elif opcode == websocket.ABNF.OPCODE_CLOSE:
                self._connected = False
                break

    # ------------------------------------------------------------------
    # Authentication
    # ------------------------------------------------------------------

    def authenticate(self, token, timeout=10):
        """Perform the firmware WS auth handshake.

        Returns True on success, False on failure.
        """
        # First message should be authRequired
        auth_req = self.recv_until("authRequired", timeout=timeout)
        if auth_req is None:
            return False

        self.send_command("auth", token=token)

        result = self.recv_json(timeout=timeout)
        if result and result.get("type") == "authSuccess":
            return True
        return False

    # ------------------------------------------------------------------
    # Send
    # ------------------------------------------------------------------

    def send_command(self, msg_type, **fields):
        """Send a JSON command frame."""
        payload = {"type": msg_type, **fields}
        self._ws.send(json.dumps(payload))

    def send_raw(self, data):
        """Send a raw string (for oversized / malformed message tests)."""
        self._ws.send(data)

    # ------------------------------------------------------------------
    # Receive
    # ------------------------------------------------------------------

    def recv_json(self, timeout=None):
        """Return the next JSON message from the queue, or None on timeout."""
        timeout = timeout or self.DEFAULT_TIMEOUT
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                if self._json_queue:
                    return self._json_queue.popleft()
            time.sleep(0.05)
        return None

    def recv_until(self, msg_type, timeout=None):
        """Wait for a JSON message with a specific ``type`` field.

        Messages that don't match are discarded.
        """
        timeout = timeout or self.DEFAULT_TIMEOUT
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self.recv_json(timeout=0.2)
            if msg is None:
                continue
            if msg.get("type") == msg_type:
                return msg
        return None

    def recv_binary(self, timeout=None):
        """Return the next binary frame, or None on timeout."""
        timeout = timeout or self.DEFAULT_TIMEOUT
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                if self._binary_queue:
                    return self._binary_queue.popleft()
            time.sleep(0.05)
        return None

    def drain_json(self):
        """Drain and return all queued JSON messages."""
        with self._lock:
            msgs = list(self._json_queue)
            self._json_queue.clear()
        return msgs

    def drain_binary(self):
        """Drain and return all queued binary frames."""
        with self._lock:
            frames = list(self._binary_queue)
            self._binary_queue.clear()
        return frames
