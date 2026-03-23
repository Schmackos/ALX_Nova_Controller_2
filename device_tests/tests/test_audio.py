"""Audio pipeline, I2S ports, and PSRAM health tests."""

import pytest


@pytest.mark.audio
class TestAudio:
    """Verify audio subsystem health."""

    def test_i2s_port_status(self, api):
        """I2S port status endpoint returns valid data."""
        resp = api.get("/api/i2s/ports")
        assert resp.status_code == 200
        body = resp.json()
        # API returns {"ports": [...], "sampleRate": N}, not a bare list
        data = body.get("ports", body) if isinstance(body, dict) else body
        assert isinstance(data, list), f"I2S ports response is not a list: {type(data)}"
        assert len(data) == 3, f"Expected 3 I2S ports, got {len(data)}"

    def test_pipeline_matrix_accessible(self, api):
        """Pipeline matrix endpoint returns valid routing data."""
        resp = api.get("/api/pipeline/matrix")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, (list, dict)), (
            "Pipeline matrix response has unexpected type"
        )

    def test_dac_status(self, api):
        """DAC status endpoint returns valid data."""
        resp = api.get("/api/dac")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict), "DAC response is not a JSON object"
        # Should have at least enabled and volume fields
        assert "enabled" in data or "state" in data, (
            f"DAC response missing expected fields: {list(data.keys())}"
        )

    def test_psram_healthy(self, api):
        """PSRAM should not be in warning or critical state."""
        resp = api.get("/api/psram/status")
        assert resp.status_code == 200
        data = resp.json()

        assert not data.get("critical", False), (
            f"PSRAM is CRITICAL: {data.get('free', '?')} bytes free"
        )
        # Warning is acceptable but worth noting
        if data.get("warning", False):
            pytest.xfail(f"PSRAM warning: {data.get('free', '?')} bytes free")

    def test_no_dma_alloc_failures(self, api):
        """No DMA buffer allocation failures should be reported."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()

        # Check nested under system or at top level
        system = data.get("system", data)
        dma_failed = system.get("dmaAllocFailed", False)
        assert not dma_failed, "DMA buffer allocation failure reported"

    def test_audio_not_paused(self, api):
        """Audio pipeline should not be stuck in paused state."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()

        # Check nested under system or at top level
        system = data.get("system", data)
        paused = system.get("audioPaused", False)
        assert not paused, "Audio pipeline is paused — may indicate I2S issue"

    def test_heap_budget_reasonable(self, api):
        """Heap budget should show allocations without excessive SRAM fallback."""
        resp = api.get("/api/psram/status")
        assert resp.status_code == 200
        data = resp.json()

        fallback_count = data.get("fallbackCount", 0)
        # Some SRAM fallback is OK, but excessive indicates PSRAM issue
        assert fallback_count < 10, (
            f"Excessive PSRAM-to-SRAM fallbacks: {fallback_count}"
        )

    def test_i2s_port_details(self, api):
        """Individual I2S port query returns detailed config."""
        resp = api.get("/api/i2s/ports?id=0")
        assert resp.status_code == 200
        data = resp.json()
        # Single port query should return port-specific info
        assert isinstance(data, (dict, list)), (
            "I2S port detail response has unexpected type"
        )

    def test_smartsensing_state(self, api):
        """Smart sensing endpoint returns valid amplifier/ADC state."""
        resp = api.get("/api/smartsensing")
        assert resp.status_code == 200
        data = resp.json()
        assert data.get("success") is True, f"smartsensing success=false: {data}"
        assert "mode" in data, f"smartsensing missing 'mode': {data.keys()}"
        assert data["mode"] in ("always_on", "always_off", "smart_auto"), (
            f"Unexpected mode: {data['mode']}"
        )
        assert "amplifierState" in data, "smartsensing missing 'amplifierState'"
        assert "audioLevel" in data, "smartsensing missing 'audioLevel'"
        assert "numAdcsDetected" in data, "smartsensing missing 'numAdcsDetected'"

    def test_input_names_readable(self, api):
        """Pipeline input names endpoint returns valid channel name list."""
        resp = api.get("/api/inputnames")
        assert resp.status_code == 200
        data = resp.json()
        assert data.get("success") is True, f"inputnames success=false: {data}"
        assert "names" in data, f"inputnames missing 'names': {data.keys()}"
        assert isinstance(data["names"], list), "inputnames 'names' is not a list"
        assert len(data["names"]) > 0, "inputnames 'names' list is empty"
        assert "numAdcsDetected" in data, "inputnames missing 'numAdcsDetected'"
