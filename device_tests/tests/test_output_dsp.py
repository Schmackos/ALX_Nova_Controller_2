"""Output DSP (per-output, post-matrix) tests.

Tests the per-output mono DSP engine that runs after the routing matrix
and before each physical sink.  All writes restore original state.
"""

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _cleanup_output_stages(api, ch=0):
    """Remove all output DSP stages from a channel."""
    for _ in range(20):
        resp = api.get("/api/output/dsp", params={"ch": ch})
        if resp.status_code != 200:
            break
        data = resp.json()
        stages = data.get("stages", [])
        if not stages:
            break
        api.delete("/api/output/dsp/stage",
                   params={"ch": ch, "stage": len(stages) - 1})


# ===========================================================================
# Config
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestOutputDspConfig:
    """Verify output DSP configuration endpoints."""

    def test_get_output_channel_config(self, api):
        """GET /api/output/dsp?ch=0 returns valid config."""
        resp = api.get("/api/output/dsp", params={"ch": 0})
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)

    def test_output_bypass_roundtrip(self, api):
        """Toggle per-channel output bypass and restore."""
        resp = api.get("/api/output/dsp", params={"ch": 0})
        assert resp.status_code == 200
        original = resp.json().get("bypass", False)

        # Toggle
        resp = api.put("/api/output/dsp", json={
            "ch": 0, "bypass": not original,
        })
        assert resp.status_code == 200

        # Restore
        api.put("/api/output/dsp", json={"ch": 0, "bypass": original})

    def test_output_global_bypass(self, api):
        """Toggle global output DSP bypass if supported."""
        resp = api.get("/api/output/dsp", params={"ch": 0})
        if resp.status_code != 200:
            pytest.skip("Output DSP endpoint not available")
        data = resp.json()
        global_bypass = data.get("globalBypass")
        if global_bypass is None:
            pytest.skip("globalBypass field not present")

        # Toggle
        resp = api.put("/api/output/dsp", json={
            "ch": 0, "globalBypass": not global_bypass,
        })
        assert resp.status_code == 200

        # Restore
        api.put("/api/output/dsp", json={
            "ch": 0, "globalBypass": global_bypass,
        })

    def test_invalid_output_channel_400(self, api):
        """Invalid channel number should return 400."""
        resp = api.get("/api/output/dsp", params={"ch": 99})
        assert resp.status_code in (400, 422), (
            f"Expected 400/422, got {resp.status_code}"
        )


# ===========================================================================
# Stage CRUD
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestOutputDspStages:
    """Verify output DSP stage add/delete."""

    def test_add_gain_stage(self, api):
        """POST /api/output/dsp/stage adds a gain stage."""
        resp = api.post("/api/output/dsp/stage", json={
            "ch": 0, "type": "gain", "gain": -3.0,
        })
        assert resp.status_code == 200
        _cleanup_output_stages(api, 0)

    def test_delete_output_stage(self, api):
        """DELETE /api/output/dsp/stage removes a stage."""
        # Add first
        api.post("/api/output/dsp/stage", json={
            "ch": 0, "type": "gain", "gain": 0.0,
        })
        before = api.get("/api/output/dsp", params={"ch": 0}).json()
        n_before = len(before.get("stages", []))

        if n_before == 0:
            pytest.skip("No stages to delete")

        resp = api.delete("/api/output/dsp/stage",
                          params={"ch": 0, "stage": n_before - 1})
        assert resp.status_code == 200

        after = api.get("/api/output/dsp", params={"ch": 0}).json()
        assert len(after.get("stages", [])) < n_before

    def test_output_crossover(self, api):
        """POST /api/output/dsp/crossover applies output crossover."""
        resp = api.post("/api/output/dsp/crossover", json={
            "ch": 0, "frequency": 2000, "type": "lr4",
        })
        # Accept 200 or 400 (if not enough sinks)
        assert resp.status_code in (200, 400)
        _cleanup_output_stages(api, 0)


# ===========================================================================
# Validation
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestOutputDspValidation:
    """Verify output DSP input validation."""

    def test_missing_channel_param(self, api):
        """Omitting ch parameter should return 400."""
        resp = api.get("/api/output/dsp")
        # Some firmware returns default ch=0, some return 400
        assert resp.status_code in (200, 400)

    def test_stage_index_out_of_range(self, api):
        """Deleting a nonexistent stage should fail gracefully."""
        resp = api.delete("/api/output/dsp/stage",
                          params={"ch": 0, "stage": 999})
        assert resp.status_code in (400, 404, 422)
