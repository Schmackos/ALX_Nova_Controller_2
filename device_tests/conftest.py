"""Shared fixtures for on-device integration tests.

Usage:
    pytest --device-port COM8 --device-ip 192.168.4.1 -v
"""

import time

import pytest
import requests

from utils.serial_reader import SerialReader
from utils.health_parser import HealthParser
from utils.issue_creator import IssueCreator


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
        default="test1234",
        help="Device web password (default: test1234 when firmware built with TEST_MODE)",
    )
    parser.addoption(
        "--baud",
        default=115200,
        type=int,
        help="Serial baud rate (default: 115200)",
    )
    parser.addoption(
        "--create-issues",
        action="store_true",
        default=False,
        help="Auto-create GitHub issues for test failures via gh CLI",
    )


# ---------------------------------------------------------------------------
# GitHub issue creation hook
# ---------------------------------------------------------------------------

def pytest_runtest_makereport(item, call):
    """Create a GitHub issue when a test fails and --create-issues is set."""
    if call.when != "call":
        return
    if not call.excinfo:
        return
    if not item.config.getoption("--create-issues", default=False):
        return

    test_name = item.nodeid.replace("::", "/")
    check_name = item.name
    firmware_version = getattr(item.config, "_firmware_version", "unknown")

    failure_text = str(call.excinfo.value)[:1000]
    traceback_text = str(call.excinfo.traceback[-1]) if call.excinfo.traceback else ""

    failed_table = (
        "| Test | Error |\n"
        "|------|-------|\n"
        f"| `{test_name}` | `{failure_text[:200]}` |"
    )
    device_ip = item.config.getoption("--device-ip", default="unknown")
    device_port = item.config.getoption("--device-port", default="unknown")
    device_state = (
        f"- Device IP: `{device_ip}`\n"
        f"- Serial port: `{device_port}`\n"
        f"- Traceback: `{traceback_text}`"
    )

    creator = IssueCreator()
    try:
        result, created = creator.report_failure(
            check_name=check_name,
            firmware_version=firmware_version,
            failed_checks=failed_table,
            device_state=device_state,
        )
        action = "Created" if created else "Updated"
        print(f"\n  [{action} GitHub issue: {result}]")
    except Exception as exc:  # noqa: BLE001 — gh CLI may not be available
        print(f"\n  [Could not create GitHub issue: {exc}]")


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
    """Thin wrapper around requests.Session with device base URL and auth.

    All requests default to a 30s timeout (ESP32-P4 can be slow on
    PBKDF2 and I2C operations). Callers can override per-request via
    ``api.get("/path", timeout=60)``.
    """

    DEFAULT_TIMEOUT = 30  # seconds

    def __init__(self, session, base_url):
        self._session = session
        self._base_url = base_url

    def get(self, path, **kwargs):
        kwargs.setdefault("timeout", self.DEFAULT_TIMEOUT)
        return self._session.get(f"{self._base_url}{path}", **kwargs)

    def post(self, path, **kwargs):
        kwargs.setdefault("timeout", self.DEFAULT_TIMEOUT)
        return self._session.post(f"{self._base_url}{path}", **kwargs)

    def put(self, path, **kwargs):
        kwargs.setdefault("timeout", self.DEFAULT_TIMEOUT)
        return self._session.put(f"{self._base_url}{path}", **kwargs)

    def delete(self, path, **kwargs):
        kwargs.setdefault("timeout", self.DEFAULT_TIMEOUT)
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
                timeout=30,  # PBKDF2 50k iterations takes ~15-20s on ESP32-P4
            )
            if resp.status_code == 200:
                return session
            # 429 = rate limited — respect retryAfter from device
            if resp.status_code == 429:
                try:
                    retry_after = resp.json().get("retryAfter", 30)
                except Exception:
                    retry_after = 30
                time.sleep(retry_after + 2)
                continue
            last_exc = Exception(f"HTTP {resp.status_code}: {resp.text[:200]}")
        except requests.ConnectionError as exc:
            last_exc = exc
        time.sleep(2)

    pytest.fail(
        f"Could not authenticate after 10 attempts. Last error: {last_exc}"
    )


@pytest.fixture(scope="session")
def api(auth_session, base_url, request):
    """Authenticated API client for REST calls."""
    client = ApiClient(auth_session, base_url)
    # Cache firmware version for GitHub issue reporting
    if not hasattr(request.config, "_firmware_version"):
        try:
            resp = client.get("/api/settings", timeout=10)
            if resp.status_code == 200:
                data = resp.json()
                ver = data.get("deviceInfo", {}).get("firmwareVersion", "unknown")
            else:
                ver = "unknown"
        except Exception:
            ver = "unknown"
        request.config._firmware_version = ver
    return client


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
