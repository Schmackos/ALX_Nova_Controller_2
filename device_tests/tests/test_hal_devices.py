"""HAL device discovery and state tests."""

import pytest


# Expected onboard devices by compatible string (from hal_builtin_devices.cpp)
# Only devices that are ALWAYS present on the base board regardless of I2C state.
# ES8311 is on I2C bus 1 and may not always init — excluded from mandatory check.
EXPECTED_ONBOARD = [
    "ti,pcm5102a",    # Primary DAC, I2S-only, always present
    "ti,pcm1808",     # ADC, I2S-only, always present
]

# HAL state enum values (from hal_types.h)
HAL_STATE_UNKNOWN     = 0
HAL_STATE_DETECTED    = 1
HAL_STATE_CONFIGURING = 2
HAL_STATE_AVAILABLE   = 3
HAL_STATE_UNAVAILABLE = 4
HAL_STATE_ERROR       = 5
HAL_STATE_MANUAL      = 6
HAL_STATE_REMOVED     = 7

HAL_STATE_NAMES = {
    0: "UNKNOWN", 1: "DETECTED", 2: "CONFIGURING", 3: "AVAILABLE",
    4: "UNAVAILABLE", 5: "ERROR", 6: "MANUAL", 7: "REMOVED",
}


def _state_name(state_val):
    """Convert numeric or string state to a display name."""
    if isinstance(state_val, int):
        return HAL_STATE_NAMES.get(state_val, f"UNKNOWN({state_val})")
    return str(state_val).upper()


def _state_is_error(state_val):
    """Return True if the state represents an ERROR condition."""
    if isinstance(state_val, int):
        return state_val == HAL_STATE_ERROR
    return str(state_val).upper() == "ERROR"


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
        """No device should be stuck in ERROR state.

        Devices in UNAVAILABLE or CONFIGURING states are acceptable —
        only ERROR (state=5) indicates a real problem.
        """
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()

        error_devices = [
            d for d in devices
            if _state_is_error(d.get("state", 0))
        ]
        assert len(error_devices) == 0, (
            f"Devices in ERROR state:\n"
            + "\n".join(
                f"  - {d.get('name', '?')} ({d.get('compatible', '?')}): "
                f"{d.get('lastError', d.get('error', 'unknown'))}"
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
            # API uses "slot" field (numeric)
            assert "slot" in dev, (
                f"Device '{name}' has no slot field. Keys: {list(dev.keys())}"
            )
            state = dev.get("state", -1)
            # State is numeric (0-7) in the firmware API
            if isinstance(state, int):
                assert 0 <= state <= 7, (
                    f"Device '{name}' has invalid state value: {state}"
                )
            else:
                # Fallback: accept string states for forward compatibility
                valid_states = {
                    "UNKNOWN", "DETECTED", "CONFIGURING", "AVAILABLE",
                    "UNAVAILABLE", "ERROR", "REMOVED", "MANUAL",
                }
                assert str(state).upper() in valid_states, (
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

    @pytest.mark.slow
    def test_scan_endpoint_responds(self, api):
        """POST /api/hal/scan should return a result (not error out).

        The endpoint is non-blocking: it spawns a background FreeRTOS task
        and returns 202 Accepted immediately so the web server stays responsive.
        200 is also accepted for backward compatibility (e.g. native-test
        fallback path). 409 means a scan is already in progress — also fine.
        """
        resp = api.post("/api/hal/scan", timeout=30)
        # 202 = scan accepted (async), 200 = sync fallback, 409 = already in progress
        assert resp.status_code in (200, 202, 409), (
            f"HAL scan returned unexpected status: {resp.status_code}"
        )

    def test_no_pin_conflicts(self, api):
        """Diagnostics should not report GPIO pin conflicts.

        The journal API returns entries with a ``"c"`` field containing the
        hex diagnostic code (e.g. ``"0x1003"``).  DIAG_HAL_PIN_CONFLICT is
        code 0x1003.
        """
        resp = api.get("/api/diagnostics/journal")
        assert resp.status_code == 200
        data = resp.json()
        # Journal entries are under "entries" key
        journal = data.get("entries", data.get("journal", []))
        pin_conflicts = [
            e for e in journal
            if e.get("c") == "0x1003"
        ]
        assert len(pin_conflicts) == 0, (
            f"Pin conflicts detected (DIAG_HAL_PIN_CONFLICT 0x1003): {pin_conflicts}"
        )
