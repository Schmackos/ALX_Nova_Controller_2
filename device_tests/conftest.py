"""Shared fixtures for on-device integration tests.

Usage:
    pytest --device-port COM8 --device-ip 192.168.4.1 -v
"""

import time

import pytest
import requests

from utils.serial_reader import SerialReader
from utils.health_parser import HealthParser


# ---------------------------------------------------------------------------
# CLI options
# ---------------------------------------------------------------------------

def pytest_addoption(parser):
    parser.addoption(
        "--device-port",
        default="COM8",
        help="Serial port for the device (default: COM8)",
    )
    parser.addoption(
        "--device-ip",
        default="192.168.4.1",
        help="IP address of the device (default: 192.168.4.1)",
    )
    parser.addoption(
        "--device-password",
        default="admin",
        help="Device web password (default: admin)",
    )
    parser.addoption(
        "--baud",
        default=115200,
        type=int,
        help="Serial baud rate (default: 115200)",
    )


# ---------------------------------------------------------------------------
# Core fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def device_ip(request):
    return request.config.getoption("--device-ip")


@pytest.fixture(scope="session")
def device_port(request):
    return request.config.getoption("--device-port")


@pytest.fixture(scope="session")
def device_password(request):
    return request.config.getoption("--device-password")


@pytest.fixture(scope="session")
def base_url(device_ip):
    return f"http://{device_ip}"


# ---------------------------------------------------------------------------
# Serial reader (session-scoped, started once)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def serial_reader(request):
    port = request.config.getoption("--device-port")
    baud = request.config.getoption("--baud")
    reader = SerialReader(port, baudrate=baud)
    reader.start()
    yield reader
    reader.stop()


# ---------------------------------------------------------------------------
# Health parser
# ---------------------------------------------------------------------------

@pytest.fixture
def health_parser(serial_reader):
    parser = HealthParser()
    parser.feed(serial_reader.get_buffer())
    return parser


# ---------------------------------------------------------------------------
# Authenticated HTTP session
# ---------------------------------------------------------------------------

class ApiClient:
    """Thin wrapper around requests.Session with device base URL and auth."""

    def __init__(self, session, base_url):
        self._session = session
        self._base_url = base_url

    def get(self, path, **kwargs):
        return self._session.get(f"{self._base_url}{path}", **kwargs)

    def post(self, path, **kwargs):
        return self._session.post(f"{self._base_url}{path}", **kwargs)

    def put(self, path, **kwargs):
        return self._session.put(f"{self._base_url}{path}", **kwargs)

    def delete(self, path, **kwargs):
        return self._session.delete(f"{self._base_url}{path}", **kwargs)


@pytest.fixture(scope="session")
def auth_session(base_url, device_password):
    """Create an authenticated requests session.

    Retries login up to 10 times (device may still be booting).
    """
    session = requests.Session()
    login_url = f"{base_url}/api/auth/login"

    last_exc = None
    for attempt in range(10):
        try:
            resp = session.post(
                login_url,
                json={"password": device_password},
                timeout=5,
            )
            if resp.status_code == 200:
                return session
            # 429 = rate limited, wait longer
            if resp.status_code == 429:
                time.sleep(3)
                continue
        except requests.ConnectionError as exc:
            last_exc = exc
        time.sleep(2)

    pytest.fail(
        f"Could not authenticate after 10 attempts. Last error: {last_exc}"
    )


@pytest.fixture(scope="session")
def api(auth_session, base_url):
    """Authenticated API client for REST calls."""
    return ApiClient(auth_session, base_url)


# ---------------------------------------------------------------------------
# Device reboot helper
# ---------------------------------------------------------------------------

@pytest.fixture
def reboot_device(api, serial_reader, base_url, device_password):
    """Fixture that returns a callable to reboot the device and wait for ready.

    Usage in tests:
        def test_something(reboot_device):
            reboot_device()
            # device is back up and re-authenticated
    """

    def _reboot():
        # Drain serial buffer before reboot
        serial_reader.drain()

        # Trigger reboot
        resp = api.post("/api/reboot")
        assert resp.status_code == 200, f"Reboot request failed: {resp.status_code}"

        # Wait for the device to come back (auth system ready)
        time.sleep(3)  # give it time to actually reboot
        line = serial_reader.expect(
            r"\[Auth\].*initialized", timeout=45
        )
        assert line is not None, "Device did not finish booting after reboot"

        # Re-authenticate (session cookies are invalidated by reboot)
        for attempt in range(10):
            try:
                resp = api._session.post(
                    f"{base_url}/api/auth/login",
                    json={"password": device_password},
                    timeout=5,
                )
                if resp.status_code == 200:
                    return
            except requests.ConnectionError:
                pass
            time.sleep(2)

        pytest.fail("Could not re-authenticate after reboot")

    return _reboot
