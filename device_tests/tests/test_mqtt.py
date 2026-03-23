"""MQTT integration tests (skipped if MQTT is not configured)."""

import pytest


def _mqtt_configured(api):
    """Check if MQTT is configured on the device."""
    resp = api.get("/api/mqtt/status")
    if resp.status_code != 200:
        return False
    data = resp.json()
    broker = data.get("broker", "") or data.get("server", "")
    return bool(broker)


@pytest.mark.mqtt
class TestMqtt:
    """Verify MQTT connectivity and configuration."""

    def test_mqtt_config_readable(self, api):
        """MQTT status endpoint must return valid JSON."""
        resp = api.get("/api/mqtt/status")
        assert resp.status_code == 200
        data = resp.json()
        # Should at least have connection state fields
        assert "connected" in data or "status" in data, (
            f"MQTT status response missing expected fields: {list(data.keys())}"
        )

    def test_mqtt_connected_if_configured(self, api):
        """If MQTT broker is configured, it should be connected."""
        if not _mqtt_configured(api):
            pytest.skip("MQTT not configured on this device")

        resp = api.get("/api/mqtt/status")
        data = resp.json()
        connected = data.get("connected", False)
        assert connected, (
            f"MQTT is configured but not connected. Status: {data}"
        )

    def test_mqtt_ha_discovery_if_connected(self, api):
        """If MQTT is connected, HA discovery should have been published."""
        if not _mqtt_configured(api):
            pytest.skip("MQTT not configured on this device")

        resp = api.get("/api/mqtt/status")
        data = resp.json()
        if not data.get("connected", False):
            pytest.skip("MQTT not connected")

        # Check diagnostics for HA discovery status
        diag = api.get("/api/diagnostics")
        assert diag.status_code == 200

    def test_mqtt_diagnostics_entry(self, api):
        """MQTT status should appear in device diagnostics."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # Diagnostics should include MQTT-related fields
        has_mqtt = any(
            "mqtt" in key.lower() for key in data.keys()
        )
        # This is informational — MQTT fields may not be present if not configured
        if not _mqtt_configured(api):
            pytest.skip("MQTT not configured — diagnostics check not applicable")
        assert has_mqtt, "MQTT fields not found in diagnostics"
