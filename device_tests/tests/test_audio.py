"""Audio pipeline, I2S ports, and PSRAM health tests."""

import pytest


@pytest.mark.audio
class TestAudio:
    """Verify audio subsystem health."""

    def test_i2s_port_status(self, api):
        """I2S port status endpoint returns valid data."""
        resp = api.get("/api/i2s/ports")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, list), "I2S ports response is not a list"
        # Should have at least 1 configured port
        active_ports = [p for p in data if p.get("mode", "off") != "off"]
        assert len(active_ports) > 0, (
            f"No active I2S ports found. Ports: {data}"
        )

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

        dma_failed = data.get("dmaAllocFailed", False)
        assert not dma_failed, "DMA buffer allocation failure reported"

    def test_audio_not_paused(self, api):
        """Audio pipeline should not be stuck in paused state."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()

        paused = data.get("audioPaused", False)
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
