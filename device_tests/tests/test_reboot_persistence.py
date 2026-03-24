"""Reboot persistence tests.

Verifies that settings survive a device reboot across all 3 storage
backends: /config.json, /hal_config.json, and NVS.

All tests are marked @reboot and @slow (each triggers a ~30-45s reboot).
"""

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_setting(api, key, dotted_key=None):
    """Read a setting value, handling both flat and dotted key formats."""
    data = api.get("/api/settings").json()
    if dotted_key:
        return data.get(dotted_key, data.get(key))
    return data.get(key)


def _find_mutable_device(api):
    """Find a device with HAL_CAP_MUTE (bit 2 = 0x04)."""
    devices = api.get("/api/hal/devices").json()
    for d in devices:
        caps = d.get("capabilities", 0)
        if caps & 0x04:
            return d
    return None


# ===========================================================================
# /config.json persistence
# ===========================================================================

@pytest.mark.reboot
@pytest.mark.slow
@pytest.mark.settings
class TestRebootPersistenceConfigJson:
    """Settings in /config.json survive reboot."""

    def test_buzzer_survives_reboot(self, api, reboot_device):
        """buzzerEnabled persists across reboot."""
        original = _get_setting(api, "buzzerEnabled", "appState.buzzerEnabled")
        if original is None:
            original = True

        target = not original
        resp = api.post("/api/settings", json={"buzzerEnabled": target})
        assert resp.status_code == 200

        reboot_device()

        after = _get_setting(api, "buzzerEnabled", "appState.buzzerEnabled")
        assert after == target, (
            f"buzzerEnabled did not survive reboot: expected {target}, got {after}"
        )

        # Restore
        api.post("/api/settings", json={"buzzerEnabled": original})

    def test_debug_mode_survives_reboot(self, api, reboot_device):
        """debugMode persists across reboot."""
        original = _get_setting(api, "debugMode", "appState.debugMode")
        if original is None:
            original = False

        target = not original
        resp = api.post("/api/settings", json={"debugMode": target})
        assert resp.status_code == 200

        reboot_device()

        after = _get_setting(api, "debugMode", "appState.debugMode")
        assert after == target, (
            f"debugMode did not survive reboot: expected {target}, got {after}"
        )

        # Restore
        api.post("/api/settings", json={"debugMode": original})


# ===========================================================================
# /hal_config.json persistence
# ===========================================================================

@pytest.mark.reboot
@pytest.mark.slow
@pytest.mark.hal
class TestRebootPersistenceHalConfig:
    """HAL device config in /hal_config.json survives reboot."""

    def test_hal_mute_survives_reboot(self, api, reboot_device):
        """HAL device cfgMute persists across reboot."""
        device = _find_mutable_device(api)
        if device is None:
            pytest.skip("No device with HAL_CAP_MUTE")

        slot = device["slot"]
        original_mute = device.get("cfgMute", False)
        target_mute = not original_mute

        resp = api.put("/api/hal/devices", json={
            "slot": slot, "cfgMute": target_mute,
        })
        assert resp.status_code == 200

        reboot_device()

        # Re-read device list
        devices = api.get("/api/hal/devices").json()
        after = next((d for d in devices if d["slot"] == slot), None)
        assert after is not None, f"Device at slot {slot} gone after reboot"
        assert after.get("cfgMute") == target_mute, (
            f"cfgMute did not survive reboot: expected {target_mute}"
        )

        # Restore
        api.put("/api/hal/devices", json={
            "slot": slot, "cfgMute": original_mute,
        })


# ===========================================================================
# NVS persistence
# ===========================================================================

@pytest.mark.reboot
@pytest.mark.slow
@pytest.mark.hal
class TestRebootPersistenceNvs:
    """Settings in NVS survive reboot."""

    def test_auto_discovery_survives_reboot(self, api, reboot_device):
        """halAutoDiscovery in NVS persists across reboot."""
        resp = api.get("/api/hal/settings")
        assert resp.status_code == 200
        original = resp.json().get("halAutoDiscovery", True)

        target = not original
        resp = api.put("/api/hal/settings", json={
            "halAutoDiscovery": target,
        })
        assert resp.status_code == 200

        reboot_device()

        resp = api.get("/api/hal/settings")
        assert resp.status_code == 200
        after = resp.json().get("halAutoDiscovery")
        assert after == target, (
            f"halAutoDiscovery did not survive reboot: expected {target}"
        )

        # Restore
        api.put("/api/hal/settings", json={
            "halAutoDiscovery": original,
        })


# ===========================================================================
# /inputnames.txt persistence
# ===========================================================================

@pytest.mark.reboot
@pytest.mark.slow
@pytest.mark.audio
class TestRebootPersistenceInputNames:
    """Input names in /inputnames.txt survive reboot."""

    def test_input_names_survive_reboot(self, api, reboot_device):
        """Custom input names persist across reboot."""
        resp = api.get("/api/inputnames")
        assert resp.status_code == 200
        data = resp.json()
        original_names = data.get("names", [])
        if not original_names:
            pytest.skip("No input names configured")

        # Set test names (same length as original)
        test_names = [f"TestCh{i}" for i in range(len(original_names))]
        resp = api.post("/api/inputnames", json={"names": test_names})
        assert resp.status_code == 200

        reboot_device()

        resp = api.get("/api/inputnames")
        assert resp.status_code == 200
        after_names = resp.json().get("names", [])
        assert after_names[:len(test_names)] == test_names, (
            f"Input names did not survive reboot"
        )

        # Restore
        api.post("/api/inputnames", json={"names": original_names})
