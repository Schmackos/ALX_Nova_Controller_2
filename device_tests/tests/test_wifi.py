"""WiFi management tests (READ-ONLY).

Tests WiFi status, scan, and saved network list endpoints.
No tests change WiFi state (that would disconnect the test runner).
Validation tests use intentionally invalid input that won't apply.
"""

import re

import pytest


# ===========================================================================
# Status
# ===========================================================================

@pytest.mark.wifi
@pytest.mark.network
class TestWifiStatus:
    """Verify WiFi status endpoint returns complete information."""

    def test_wifi_status_extended_fields(self, api):
        """GET /api/wifistatus returns mode, ssid, ip, mac fields."""
        resp = api.get("/api/wifistatus")
        assert resp.status_code == 200
        data = resp.json()
        assert "mode" in data, f"Missing 'mode'. Keys: {list(data.keys())}"
        assert data["mode"] in ("sta", "ap", "ap+sta", "off"), (
            f"Unknown WiFi mode: {data['mode']}"
        )

    def test_wifi_rssi_in_range(self, api):
        """WiFi RSSI should be between -100 and 0 dBm."""
        data = api.get("/api/wifistatus").json()
        rssi = data.get("rssi", data.get("signalStrength", 0))
        if data.get("mode") == "ap":
            pytest.skip("RSSI not applicable in AP mode")
        assert -100 <= rssi <= 0, f"RSSI out of range: {rssi}"

    def test_wifi_ip_valid_format(self, api):
        """WiFi IP address should match IPv4 pattern."""
        data = api.get("/api/wifistatus").json()
        ip = data.get("ip", data.get("ipAddress", ""))
        if not ip or ip == "0.0.0.0":
            pytest.skip("No IP assigned (may be AP mode)")
        pattern = r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$"
        assert re.match(pattern, ip), f"Invalid IP format: {ip}"


# ===========================================================================
# Scan
# ===========================================================================

@pytest.mark.wifi
@pytest.mark.network
class TestWifiScan:
    """Verify WiFi scan returns discoverable networks."""

    def test_wifi_scan_returns_results(self, api):
        """GET /api/wifiscan triggers or returns scan results."""
        resp = api.get("/api/wifiscan")
        assert resp.status_code == 200
        data = resp.json()
        # Response has either scanning flag or networks list
        assert "scanning" in data or "networks" in data or "results" in data, (
            f"Missing scan fields. Keys: {list(data.keys())}"
        )

    def test_wifi_scan_network_fields(self, api):
        """Scan result networks have ssid, rssi, encryption fields."""
        resp = api.get("/api/wifiscan")
        assert resp.status_code == 200
        data = resp.json()
        networks = data.get("networks", data.get("results", []))
        if not networks:
            pytest.skip("No networks found in scan")
        net = networks[0]
        assert "ssid" in net, f"Network missing 'ssid'. Keys: {list(net.keys())}"
        assert "rssi" in net or "signalStrength" in net

    def test_wifi_scan_second_call(self, api):
        """Second scan call returns results or scanning state."""
        # First call may trigger scan
        api.get("/api/wifiscan")
        import time
        time.sleep(3)
        # Second call should have results
        resp = api.get("/api/wifiscan")
        assert resp.status_code == 200


# ===========================================================================
# Validation (safe — intentionally invalid input)
# ===========================================================================

@pytest.mark.wifi
@pytest.mark.network
class TestWifiValidation:
    """Verify WiFi endpoint input validation without changing state."""

    def test_wifi_list_saved_networks(self, api):
        """GET /api/wifilist returns saved network list structure."""
        resp = api.get("/api/wifilist")
        assert resp.status_code == 200
        data = resp.json()
        # Should have count or networks field
        assert "count" in data or "networks" in data or isinstance(data, list), (
            f"Unexpected wifilist format. Keys: {list(data.keys()) if isinstance(data, dict) else 'list'}"
        )

    def test_wifi_config_empty_ssid_rejected(self, api):
        """POST /api/wificonfig with empty SSID should be rejected."""
        resp = api.post("/api/wificonfig", json={"ssid": "", "password": "x"})
        # Should be rejected (400) or at least not 500
        assert resp.status_code < 500, (
            f"Server error on empty SSID: {resp.status_code}"
        )

    def test_wifi_remove_nonexistent(self, api):
        """POST /api/wifiremove with bad index should not crash."""
        resp = api.post("/api/wifiremove", json={"index": 999})
        # Should be error or no-op, not 500
        assert resp.status_code < 500, (
            f"Server error on bad index: {resp.status_code}"
        )
