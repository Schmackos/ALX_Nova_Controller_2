"""REST-to-WebSocket state synchronization tests.

Verifies that REST API state changes produce matching WebSocket broadcasts.
The firmware pipeline: REST sets state + markXxxDirty() → main loop (~5ms)
calls sendXxxState() → WS JSON broadcast to all authenticated clients.
"""

import time

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _find_mutable_device(api):
    """Find a device with HAL_CAP_MUTE (bit 2 = 0x04)."""
    devices = api.get("/api/hal/devices").json()
    for d in devices:
        caps = d.get("capabilities", 0)
        if caps & 0x04:  # HAL_CAP_MUTE
            return d
    return None


# ===========================================================================
# REST → WS sync tests
# ===========================================================================

@pytest.mark.ws
@pytest.mark.sync
@pytest.mark.network
class TestRestWsSync:
    """Verify REST state changes produce matching WebSocket broadcasts."""

    def test_dsp_bypass_syncs_ws(self, api, ws_client):
        """POST /api/dsp/bypass triggers dspState WS broadcast."""
        ws_client.drain_json()

        # Toggle bypass ON
        resp = api.post("/api/dsp/bypass", json={"bypass": True})
        assert resp.status_code == 200

        # Wait for WS broadcast
        msg = ws_client.recv_until("dspState", timeout=5)
        assert msg is not None, "No dspState broadcast after bypass toggle"
        assert msg.get("dspBypass") is True or msg.get("bypass") is True

        # Restore
        api.post("/api/dsp/bypass", json={"bypass": False})
        ws_client.drain_json()

    def test_buzzer_enabled_syncs_ws(self, api, ws_client):
        """POST /api/settings {buzzerEnabled} triggers buzzerState broadcast."""
        # Read original
        settings = api.get("/api/settings").json()
        original = settings.get("buzzerEnabled", settings.get("appState.buzzerEnabled", True))

        ws_client.drain_json()

        # Toggle
        new_val = not original
        resp = api.post("/api/settings", json={"buzzerEnabled": new_val})
        assert resp.status_code == 200

        # Wait for WS broadcast
        msg = ws_client.recv_until("buzzerState", timeout=5)
        assert msg is not None, "No buzzerState broadcast after toggle"
        assert msg.get("enabled") == new_val

        # Restore
        api.post("/api/settings", json={"buzzerEnabled": original})
        ws_client.drain_json()

    @pytest.mark.slow
    def test_hal_scan_syncs_ws(self, api, ws_client):
        """POST /api/hal/scan triggers halDeviceState broadcast on completion."""
        ws_client.drain_json()

        resp = api.post("/api/hal/scan")
        assert resp.status_code in (200, 202, 409)

        if resp.status_code == 409:
            pytest.skip("Scan already in progress")

        # Async scan — wait up to 10s for broadcast
        msg = ws_client.recv_until("halDeviceState", timeout=10)
        assert msg is not None, "No halDeviceState broadcast after scan"
        # Should have devices array
        assert "devices" in msg or isinstance(msg.get("devices"), list) or len(msg) > 2

    def test_siggen_enable_syncs_ws(self, api, ws_client):
        """POST /api/signalgenerator triggers signalGenState broadcast."""
        initial = api.get("/api/signalgenerator").json()
        was_enabled = initial.get("enabled", False)

        ws_client.drain_json()

        # Enable with safe params
        resp = api.post("/api/signalgenerator", json={
            "enabled": True, "waveform": "sine",
            "frequency": 1000, "amplitude": -20,
        })
        assert resp.status_code == 200

        # WS type may be "signalGenState" or "signalGenerator"
        msg = ws_client.recv_until("signalGenState", timeout=5)
        if msg is None:
            msg = ws_client.recv_until("signalGenerator", timeout=3)
        assert msg is not None, "No signal gen broadcast after enable"

        # Restore
        api.post("/api/signalgenerator", json={"enabled": was_enabled})
        ws_client.drain_json()

    def test_hal_config_mute_syncs_ws(self, api, ws_client):
        """PUT /api/hal/devices mute change triggers halDeviceState broadcast."""
        device = _find_mutable_device(api)
        if device is None:
            pytest.skip("No device with HAL_CAP_MUTE")

        slot = device["slot"]
        original_mute = device.get("cfgMute", False)

        ws_client.drain_json()

        # Toggle mute
        resp = api.put("/api/hal/devices", json={
            "slot": slot, "cfgMute": not original_mute,
        })
        assert resp.status_code == 200

        # Wait for broadcast (halDeviceState or hardwareStats)
        msg = ws_client.recv_until("halDeviceState", timeout=5)
        if msg is None:
            msg = ws_client.recv_until("hardwareStats", timeout=3)
        assert msg is not None, "No device state broadcast after mute toggle"

        # Restore
        api.put("/api/hal/devices", json={
            "slot": slot, "cfgMute": original_mute,
        })
        ws_client.drain_json()
