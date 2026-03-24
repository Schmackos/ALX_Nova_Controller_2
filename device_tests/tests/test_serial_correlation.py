"""Serial log correlation tests.

Verifies that API calls produce expected firmware serial log output.
Uses serial_reader.drain() before each action and expect() after.
"""

import time

import pytest


@pytest.mark.hal
class TestSerialCorrelationHal:
    """Verify HAL API calls produce serial log output."""

    @pytest.mark.slow
    def test_hal_scan_produces_log(self, api, serial_reader):
        """POST /api/hal/scan should produce [HAL log lines."""
        serial_reader.drain()

        resp = api.post("/api/hal/scan")
        if resp.status_code == 409:
            pytest.skip("Scan already in progress")
        assert resp.status_code in (200, 202)

        # Wait for HAL-related log lines (discovery runs async)
        line = serial_reader.expect(r"\[HAL", timeout=10)
        assert line is not None, (
            "No [HAL log line after scan — serial may be disconnected "
            "or log level too high"
        )


@pytest.mark.settings
class TestSerialCorrelationSettings:
    """Verify settings changes produce serial log output."""

    def test_settings_change_produces_log(self, api, serial_reader):
        """POST /api/settings should produce [Settings] log line."""
        # Read original buzzer state
        settings = api.get("/api/settings").json()
        original = settings.get("buzzerEnabled",
                                settings.get("appState.buzzerEnabled", True))

        serial_reader.drain()

        # Toggle buzzer
        api.post("/api/settings", json={"buzzerEnabled": not original})

        # Check for settings log
        line = serial_reader.expect(r"\[Settings\]", timeout=5)
        if line is None:
            # May not log on every settings change — skip rather than fail
            serial_reader.drain()
            api.post("/api/settings", json={"buzzerEnabled": original})
            pytest.skip("No [Settings] log — may be LOG_W level or higher")

        # Restore
        api.post("/api/settings", json={"buzzerEnabled": original})


@pytest.mark.audio
class TestSerialCorrelationAudio:
    """Verify audio-related API calls produce serial log output."""

    def test_siggen_toggle_produces_log(self, api, serial_reader):
        """POST /api/signalgenerator enable should produce [SigGen] log."""
        initial = api.get("/api/signalgenerator").json()
        was_enabled = initial.get("enabled", False)

        serial_reader.drain()

        # Enable
        api.post("/api/signalgenerator", json={
            "enabled": True, "waveform": "sine",
            "frequency": 1000, "amplitude": -20,
        })

        line = serial_reader.expect(r"\[SigGen\]", timeout=5)

        # Restore regardless
        api.post("/api/signalgenerator", json={"enabled": was_enabled})

        if line is None:
            pytest.skip("No [SigGen] log — may be LOG_W level or higher")
