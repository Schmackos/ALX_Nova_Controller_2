"""Network connectivity and security tests."""

import pytest
import requests


@pytest.mark.network
class TestNetwork:
    """Verify network connectivity and HTTP security."""

    def test_device_reachable(self, base_url):
        """Device HTTP server must respond."""
        resp = requests.get(f"{base_url}/", timeout=5)
        assert resp.status_code == 200

    def test_wifi_status(self, api):
        """WiFi status endpoint returns valid state."""
        resp = api.get("/api/wifi/status")
        assert resp.status_code == 200
        data = resp.json()
        # Device should be either connected to a network or in AP mode
        mode = data.get("mode", "")
        assert mode in ("STA", "AP", "STA+AP", "sta", "ap"), (
            f"Unexpected WiFi mode: {mode}"
        )

    def test_security_headers_present(self, base_url, auth_session):
        """All HTTP responses must include security headers."""
        resp = auth_session.get(f"{base_url}/api/diagnostics", timeout=5)
        assert resp.status_code == 200
        assert resp.headers.get("X-Frame-Options") == "DENY", (
            "Missing or wrong X-Frame-Options header"
        )
        assert resp.headers.get("X-Content-Type-Options") == "nosniff", (
            "Missing or wrong X-Content-Type-Options header"
        )

    def test_auth_required_on_protected_endpoints(self, base_url):
        """Protected API endpoints must reject unauthenticated requests."""
        # Use a fresh session with no cookies
        resp = requests.get(
            f"{base_url}/api/diagnostics", timeout=5
        )
        # Should get 401 or 403
        assert resp.status_code in (401, 403), (
            f"Expected 401/403 without auth, got {resp.status_code}"
        )

    def test_auth_status_endpoint(self, api):
        """Auth status should confirm we are authenticated."""
        resp = api.get("/api/auth/status")
        assert resp.status_code == 200
        data = resp.json()
        assert data.get("authenticated") is True

    def test_websocket_port_accessible(self, device_ip):
        """WebSocket port 81 should be open and accepting connections."""
        import socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3)
        try:
            result = sock.connect_ex((device_ip, 81))
            assert result == 0, f"WebSocket port 81 not open (error: {result})"
        finally:
            sock.close()
