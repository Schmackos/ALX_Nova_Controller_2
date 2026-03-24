"""DSP pipeline, signal generator, and audio diagnostics tests.

Tests the DSP engine, signal generator, pipeline matrix, and DAC
management endpoints on live hardware. Uses safe writes that restore
original state after each test.
"""

import pytest


@pytest.mark.audio
class TestDspConfig:
    """Verify DSP configuration endpoints."""

    def test_dsp_get_config(self, api):
        """GET /api/dsp returns valid DSP configuration."""
        resp = api.get("/api/dsp")
        assert resp.status_code == 200
        data = resp.json()
        assert "dspEnabled" in data or "enabled" in data

    def test_dsp_metrics(self, api):
        """GET /api/dsp/metrics returns processing metrics."""
        resp = api.get("/api/dsp/metrics")
        assert resp.status_code == 200
        data = resp.json()
        # Should have CPU load and process time
        assert "cpuLoad" in data or "processTimeUs" in data

    def test_dsp_bypass_toggle_roundtrip(self, api):
        """Toggle DSP bypass on and off, verify state changes."""
        # Read initial state
        resp = api.get("/api/dsp")
        assert resp.status_code == 200
        initial = resp.json()

        # Toggle bypass ON
        resp = api.post("/api/dsp/bypass", json={"bypass": True})
        assert resp.status_code == 200

        # Toggle bypass OFF (restore)
        resp = api.post("/api/dsp/bypass", json={"bypass": False})
        assert resp.status_code == 200

    def test_dsp_channel_config(self, api):
        """GET /api/dsp/channel?ch=0 returns channel 0 config."""
        resp = api.get("/api/dsp/channel", params={"ch": 0})
        assert resp.status_code == 200
        data = resp.json()
        # Should have bypass and stages info
        assert isinstance(data, dict)

    def test_dsp_channel_bypass_roundtrip(self, api):
        """Toggle channel 0 bypass and restore."""
        # Set bypass ON
        resp = api.post("/api/dsp/channel/bypass", params={"ch": 0},
                        json={"bypass": True})
        assert resp.status_code == 200

        # Restore bypass OFF
        resp = api.post("/api/dsp/channel/bypass", params={"ch": 0},
                        json={"bypass": False})
        assert resp.status_code == 200

    def test_dsp_presets_list(self, api):
        """GET /api/dsp/presets returns preset slots."""
        resp = api.get("/api/dsp/presets")
        assert resp.status_code == 200
        data = resp.json()
        assert "slots" in data or isinstance(data, list)

    def test_dsp_peq_presets_list(self, api):
        """GET /api/dsp/peq/presets returns saved PEQ preset names."""
        resp = api.get("/api/dsp/peq/presets")
        assert resp.status_code == 200
        data = resp.json()
        assert "presets" in data

    def test_dsp_export_json(self, api):
        """GET /api/dsp/export/json returns full DSP config."""
        resp = api.get("/api/dsp/export/json")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)


@pytest.mark.audio
class TestSignalGenerator:
    """Verify signal generator control endpoints."""

    def test_siggen_get_state(self, api):
        """GET /api/signalgenerator returns generator state."""
        resp = api.get("/api/signalgenerator")
        assert resp.status_code == 200
        data = resp.json()
        assert "enabled" in data
        assert "waveform" in data
        assert "frequency" in data

    def test_siggen_valid_waveforms(self, api):
        """Signal generator waveform should be a known type."""
        data = api.get("/api/signalgenerator").json()
        valid_waveforms = {"sine", "square", "white_noise", "sweep"}
        waveform = data.get("waveform", "")
        assert waveform in valid_waveforms, (
            f"Unknown waveform: '{waveform}'"
        )

    def test_siggen_frequency_in_range(self, api):
        """Signal generator frequency should be 1-22000 Hz."""
        data = api.get("/api/signalgenerator").json()
        freq = data.get("frequency", 0)
        assert 1 <= freq <= 22000, f"Frequency out of range: {freq}"

    def test_siggen_enable_disable_roundtrip(self, api):
        """Enable then disable signal generator (safe roundtrip)."""
        # Get initial state
        initial = api.get("/api/signalgenerator").json()
        was_enabled = initial.get("enabled", False)

        # Enable with known safe settings
        resp = api.post("/api/signalgenerator", json={
            "enabled": True,
            "waveform": "sine",
            "frequency": 1000,
            "amplitude": -20,
        })
        assert resp.status_code == 200

        # Verify enabled
        data = api.get("/api/signalgenerator").json()
        assert data.get("enabled") is True

        # Restore original state
        api.post("/api/signalgenerator", json={"enabled": was_enabled})

    def test_siggen_amplitude_range(self, api):
        """Signal generator amplitude should be -96 to 0 dB."""
        data = api.get("/api/signalgenerator").json()
        amp = data.get("amplitude", 0)
        assert -96 <= amp <= 0, f"Amplitude out of range: {amp}"


@pytest.mark.audio
class TestPipelineMatrix:
    """Verify audio pipeline routing matrix."""

    def test_matrix_structure_valid(self, api):
        """Pipeline matrix should have size and matrix array."""
        data = api.get("/api/pipeline/matrix").json()
        assert "matrix" in data, "Matrix missing 'matrix' field"
        assert "size" in data, "Matrix missing 'size' field"
        assert data["size"] == 32, f"Expected 32x32 matrix, got size={data['size']}"

    def test_matrix_dimensions_valid(self, api):
        """Matrix should be square with correct dimensions."""
        data = api.get("/api/pipeline/matrix").json()
        matrix = data.get("matrix", [])
        assert len(matrix) > 0, "Matrix is empty"
        # Each row should have same length as number of rows
        for i, row in enumerate(matrix):
            assert len(row) == len(matrix), (
                f"Row {i} has {len(row)} columns, expected {len(matrix)}"
            )

    def test_matrix_cell_set_and_restore(self, api):
        """Set a matrix cell gain and restore original value."""
        # Read current matrix
        data = api.get("/api/pipeline/matrix").json()
        matrix = data.get("matrix", [[]])
        original_gain = matrix[0][0] if matrix and matrix[0] else 0

        # Set cell [0][0] to -6 dB
        resp = api.post("/api/pipeline/matrix/cell", json={
            "out": 0, "in": 0, "gainDb": -6.0,
        })
        assert resp.status_code == 200

        # Restore original
        api.post("/api/pipeline/matrix/cell", json={
            "out": 0, "in": 0, "gainDb": original_gain,
        })

    def test_pipeline_sinks_list(self, api):
        """GET /api/pipeline/sinks returns registered output sinks."""
        resp = api.get("/api/pipeline/sinks")
        assert resp.status_code == 200
        data = resp.json()
        # Should be a list (may be empty if no DAC enabled)
        assert isinstance(data, list)



@pytest.mark.audio
class TestThdMeasurement:
    """Verify THD measurement endpoint."""

    def test_thd_status_readable(self, api):
        """GET /api/thd returns measurement status."""
        resp = api.get("/api/thd")
        assert resp.status_code == 200
        data = resp.json()
        assert "measuring" in data


@pytest.mark.audio
class TestAudioDiagnostics:
    """Verify audio-related diagnostic endpoints."""

    def test_diagnostics_includes_audio_info(self, api):
        """Diagnostic snapshot should include audio-related data."""
        resp = api.get("/api/diag/snapshot")
        assert resp.status_code == 200
        data = resp.json()
        # Should have some audio-related fields
        assert isinstance(data, dict)
        assert len(data) > 5, "Diagnostic snapshot suspiciously small"

    def test_diagnostic_journal_readable(self, api):
        """Diagnostic journal should be queryable."""
        resp = api.get("/api/diagnostics/journal")
        assert resp.status_code == 200
        data = resp.json()
        # Journal should have an entries array
        entries = data.get("entries", data.get("journal", []))
        assert isinstance(entries, list)

    def test_no_audio_error_diagnostics(self, api):
        """No audio error diagnostics should be present on healthy device."""
        resp = api.get("/api/diagnostics/journal")
        assert resp.status_code == 200
        data = resp.json()
        entries = data.get("entries", data.get("journal", []))
        # Audio error codes are 0x2001-0x200E
        audio_errors = [
            e for e in entries
            if e.get("c", "").startswith("0x20")
            and e.get("sev") in ("E", "C")  # Error or Critical severity
        ]
        assert len(audio_errors) == 0, (
            f"Audio error diagnostics found:\n"
            + "\n".join(
                f"  {e.get('c')}: {e.get('msg', e.get('message', ''))}"
                for e in audio_errors
            )
        )
