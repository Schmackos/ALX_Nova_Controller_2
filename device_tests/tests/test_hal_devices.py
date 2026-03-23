"""HAL device discovery and state tests."""

import pytest


# Expected onboard devices by compatible string
EXPECTED_ONBOARD = [
    "ti,pcm5102a",
    "everest,es8311",
]


@pytest.mark.hal
class TestHalDevices:
    """Verify HAL device manager state and discovered devices."""

    def test_device_list_returns_json(self, api):
        """GET /api/hal/devices returns a valid device list."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, list), "HAL devices response is not a list"
        assert len(data) > 0, "No HAL devices found"

    def test_onboard_devices_present(self, api):
        """Core onboard devices must be discovered."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()

        compatibles = {
            d.get("compatible", "") for d in devices
        }

        for expected in EXPECTED_ONBOARD:
            assert expected in compatibles, (
                f"Expected onboard device '{expected}' not found. "
                f"Discovered: {sorted(compatibles)}"
            )

    def test_no_error_state_devices(self, api):
        """No device should be stuck in ERROR state."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()

        error_devices = [
            d for d in devices
            if d.get("state", "").upper() == "ERROR"
        ]
        assert len(error_devices) == 0, (
            f"Devices in ERROR state:\n"
            + "\n".join(
                f"  - {d.get('name', '?')} ({d.get('compatible', '?')}): "
                f"{d.get('error', d.get('errorReason', 'unknown'))}"
                for d in error_devices
            )
        )

    def test_device_configs_valid(self, api):
        """Each device must have a valid slot and state."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()

        for dev in devices:
            name = dev.get("name", "unnamed")
            assert "slot" in dev or "halSlot" in dev, (
                f"Device '{name}' has no slot field"
            )
            state = dev.get("state", "")
            valid_states = {
                "UNKNOWN", "DETECTED", "CONFIGURING", "AVAILABLE",
                "UNAVAILABLE", "ERROR", "REMOVED", "MANUAL",
            }
            assert state.upper() in valid_states, (
                f"Device '{name}' has invalid state: '{state}'"
            )

    def test_device_db_presets_accessible(self, api):
        """HAL device database presets endpoint must respond."""
        resp = api.get("/api/hal/db/presets")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, (list, dict)), (
            "DB presets response has unexpected type"
        )

    def test_scan_endpoint_responds(self, api):
        """POST /api/hal/scan should return a result (not error out)."""
        resp = api.post("/api/hal/scan")
        # 200 = scan complete, 409 = scan already in progress — both acceptable
        assert resp.status_code in (200, 409), (
            f"HAL scan returned unexpected status: {resp.status_code}"
        )

    def test_no_pin_conflicts(self, api):
        """Diagnostics should not report GPIO pin conflicts."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # Check diagnostic journal for pin conflict events
        journal = data.get("journal", [])
        pin_conflicts = [
            e for e in journal
            if "pin" in str(e).lower() and "conflict" in str(e).lower()
        ]
        assert len(pin_conflicts) == 0, (
            f"Pin conflicts detected: {pin_conflicts}"
        )
