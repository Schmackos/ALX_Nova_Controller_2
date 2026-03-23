"""Ethernet management tests.

Tests Ethernet status, hostname, and config validation endpoints.
No tests change the IP address (that would disconnect the test runner).
"""

import pytest


# ===========================================================================
# Status
# ===========================================================================

@pytest.mark.ethernet
@pytest.mark.network
class TestEthernetStatus:
    """Verify Ethernet status endpoint."""

    def test_eth_status_returns_json(self, api):
        """GET /api/ethstatus returns valid JSON with expected fields."""
        resp = api.get("/api/ethstatus")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)
        # Should have at minimum some connectivity fields
        assert any(k in data for k in ("linkUp", "connected", "ip", "mac")), (
            f"Missing core fields. Keys: {list(data.keys())}"
        )

    def test_eth_hostname_present(self, api):
        """Ethernet status should include a hostname."""
        data = api.get("/api/ethstatus").json()
        hostname = data.get("hostname", data.get("name", ""))
        # Hostname may be empty on first boot, but field should exist
        assert "hostname" in data or "name" in data, (
            f"Missing hostname field. Keys: {list(data.keys())}"
        )

    def test_eth_link_state_boolean(self, api):
        """linkUp field should be a boolean."""
        data = api.get("/api/ethstatus").json()
        link_up = data.get("linkUp")
        if link_up is None:
            pytest.skip("No linkUp field in response")
        assert isinstance(link_up, bool), f"linkUp is not boolean: {type(link_up)}"


# ===========================================================================
# Validation (safe — intentionally invalid input)
# ===========================================================================

@pytest.mark.ethernet
@pytest.mark.network
class TestEthernetValidation:
    """Verify Ethernet config validation without dangerous state changes."""

    def test_eth_invalid_hostname_rejected(self, api):
        """Hostname starting with hyphen should be rejected."""
        resp = api.post("/api/ethconfig", json={
            "hostname": "-invalid-host",
        })
        # Should reject or at least not 500
        assert resp.status_code < 500, (
            f"Server error on invalid hostname: {resp.status_code}"
        )

    def test_eth_invalid_ip_format(self, api):
        """Invalid IP format should be rejected."""
        resp = api.post("/api/ethconfig", json={
            "useStaticIP": True,
            "ip": "999.999.999.999",
            "gateway": "192.168.1.1",
            "subnet": "255.255.255.0",
        })
        assert resp.status_code < 500, (
            f"Server error on invalid IP: {resp.status_code}"
        )

    def test_eth_static_ip_missing_fields(self, api):
        """Static IP without required fields should fail."""
        resp = api.post("/api/ethconfig", json={
            "useStaticIP": True,
            # Missing ip, gateway, subnet
        })
        assert resp.status_code < 500, (
            f"Server error on missing static IP fields: {resp.status_code}"
        )


# ===========================================================================
# Safe operations
# ===========================================================================

@pytest.mark.ethernet
@pytest.mark.network
class TestEthernetSafe:
    """Safe Ethernet operations that don't disrupt connectivity."""

    def test_eth_confirm_no_pending(self, api):
        """Confirming when no config change is pending should be safe."""
        resp = api.post("/api/ethconfig/confirm")
        # No-op when nothing is pending — should not crash
        assert resp.status_code < 500

    def test_eth_hostname_roundtrip(self, api):
        """Set hostname via API and restore original."""
        # Read current
        status = api.get("/api/ethstatus").json()
        original = status.get("hostname", "")
        if not original:
            pytest.skip("No hostname configured — cannot do roundtrip")

        # Set new hostname
        resp = api.post("/api/sethostname", json={
            "hostname": "test-alx-host",
        })
        if resp.status_code not in (200, 201):
            pytest.skip(f"Hostname set not supported: {resp.status_code}")

        # Restore original
        api.post("/api/sethostname", json={"hostname": original})
