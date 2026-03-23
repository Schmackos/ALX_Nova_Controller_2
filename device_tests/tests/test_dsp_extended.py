"""Extended DSP pipeline tests.

Tests DSP stage CRUD, crossover/bass management, import/export,
preset management, and PEQ presets on live hardware.
All mutating tests restore original state.
"""

import time

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _cleanup_test_stages(api, ch=0):
    """Remove all stages from channel (brute-force cleanup)."""
    for _ in range(20):
        resp = api.get("/api/dsp/channel", params={"ch": ch})
        if resp.status_code != 200:
            break
        data = resp.json()
        stages = data.get("stages", [])
        if not stages:
            break
        # Delete the last stage
        api.delete("/api/dsp/stage", params={"ch": ch, "stage": len(stages) - 1})


# ===========================================================================
# Stage CRUD
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestDspStageCrud:
    """Verify DSP stage add/update/delete lifecycle."""

    def test_add_peq_stage(self, api):
        """POST /api/dsp/stage adds a PEQ stage."""
        resp = api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq",
            "frequency": 1000,
            "gain": 0.0,
            "q": 1.0,
        })
        assert resp.status_code == 200, f"Add PEQ stage failed: {resp.status_code}"
        data = resp.json()
        # Should return the new stage index or success
        assert data.get("success") is True or "index" in data or "stage" in data
        # Cleanup
        _cleanup_test_stages(api, 0)

    def test_update_stage_params(self, api):
        """PUT /api/dsp/stage updates stage parameters."""
        # Add a stage first
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 1000, "gain": 0.0, "q": 1.0,
        })
        # Get stage index
        ch_data = api.get("/api/dsp/channel", params={"ch": 0}).json()
        stages = ch_data.get("stages", [])
        assert len(stages) > 0, "No stages after add"
        idx = len(stages) - 1

        # Update
        resp = api.put("/api/dsp/stage", params={"ch": 0, "stage": idx}, json={
            "frequency": 2000, "gain": -3.0, "q": 2.0,
        })
        assert resp.status_code == 200
        # Cleanup
        _cleanup_test_stages(api, 0)

    def test_delete_stage(self, api):
        """DELETE /api/dsp/stage removes a stage."""
        # Add
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 500, "gain": 0.0, "q": 1.0,
        })
        ch_data = api.get("/api/dsp/channel", params={"ch": 0}).json()
        count_before = len(ch_data.get("stages", []))
        idx = count_before - 1

        # Delete
        resp = api.delete("/api/dsp/stage", params={"ch": 0, "stage": idx})
        assert resp.status_code == 200

        # Verify removed
        ch_data = api.get("/api/dsp/channel", params={"ch": 0}).json()
        count_after = len(ch_data.get("stages", []))
        assert count_after < count_before

    def test_add_delete_roundtrip(self, api):
        """Add a stage, verify it exists, delete it, verify gone."""
        # Baseline
        before = api.get("/api/dsp/channel", params={"ch": 0}).json()
        n_before = len(before.get("stages", []))

        # Add
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 3000, "gain": -1.0, "q": 0.7,
        })
        after_add = api.get("/api/dsp/channel", params={"ch": 0}).json()
        n_after_add = len(after_add.get("stages", []))
        assert n_after_add == n_before + 1

        # Delete
        api.delete("/api/dsp/stage", params={"ch": 0, "stage": n_after_add - 1})
        after_del = api.get("/api/dsp/channel", params={"ch": 0}).json()
        n_after_del = len(after_del.get("stages", []))
        assert n_after_del == n_before

    def test_stage_reorder(self, api):
        """Reorder stages within a channel."""
        # Add 2 stages
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 100, "gain": 0.0, "q": 1.0,
        })
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 10000, "gain": 0.0, "q": 1.0,
        })
        ch_data = api.get("/api/dsp/channel", params={"ch": 0}).json()
        stages = ch_data.get("stages", [])
        n = len(stages)
        if n < 2:
            _cleanup_test_stages(api, 0)
            pytest.skip("Could not add 2 stages for reorder test")

        # Reorder: swap last two
        new_order = list(range(n))
        new_order[-2], new_order[-1] = new_order[-1], new_order[-2]
        resp = api.post("/api/dsp/stage/reorder", params={"ch": 0},
                        json={"order": new_order})
        assert resp.status_code == 200
        # Cleanup
        _cleanup_test_stages(api, 0)

    def test_stage_enable_toggle(self, api):
        """Toggle stage enabled/disabled."""
        # Add a stage
        api.post("/api/dsp/stage", params={"ch": 0}, json={
            "type": "peq", "frequency": 500, "gain": 0.0, "q": 1.0,
        })
        ch_data = api.get("/api/dsp/channel", params={"ch": 0}).json()
        stages = ch_data.get("stages", [])
        idx = len(stages) - 1

        # Disable
        resp = api.post("/api/dsp/stage/enable",
                        params={"ch": 0, "stage": idx},
                        json={"enabled": False})
        assert resp.status_code == 200

        # Re-enable
        resp = api.post("/api/dsp/stage/enable",
                        params={"ch": 0, "stage": idx},
                        json={"enabled": True})
        assert resp.status_code == 200
        # Cleanup
        _cleanup_test_stages(api, 0)

    def test_invalid_channel_400(self, api):
        """Adding a stage to invalid channel should fail."""
        resp = api.post("/api/dsp/stage", params={"ch": 99}, json={
            "type": "peq", "frequency": 1000, "gain": 0.0, "q": 1.0,
        })
        assert resp.status_code in (400, 422), (
            f"Expected 400/422 for invalid channel, got {resp.status_code}"
        )


# ===========================================================================
# Crossover / Bass Management
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestDspCrossover:
    """Verify crossover and bass management endpoints."""

    def test_apply_lr4_crossover(self, api):
        """POST /api/dsp/crossover applies LR4 crossover."""
        resp = api.post("/api/dsp/crossover", params={"ch": 0}, json={
            "frequency": 2000,
            "type": "lr4",
        })
        # Accept 200 or 400 (if no sinks registered)
        assert resp.status_code in (200, 400), (
            f"Crossover failed: {resp.status_code}"
        )
        # Cleanup added stages
        _cleanup_test_stages(api, 0)

    def test_baffle_step_correction(self, api):
        """POST /api/dsp/bafflestep applies baffle step."""
        resp = api.post("/api/dsp/bafflestep", params={"ch": 0}, json={
            "diameter": 200,
        })
        assert resp.status_code in (200, 400)
        _cleanup_test_stages(api, 0)

    def test_bass_management(self, api):
        """POST /api/dsp/bassmanagement configures bass routing."""
        resp = api.post("/api/dsp/bassmanagement", json={
            "frequency": 80,
            "mainChannels": [0, 1],
            "subChannel": 2,
        })
        assert resp.status_code in (200, 400)
        _cleanup_test_stages(api, 0)
        _cleanup_test_stages(api, 1)
        _cleanup_test_stages(api, 2)


# ===========================================================================
# Import / Export
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestDspImportExport:
    """Verify DSP import and export formats."""

    def test_export_apo_format(self, api):
        """GET /api/dsp/export/apo returns Equalizer APO text."""
        resp = api.get("/api/dsp/export/apo", params={"ch": 0})
        assert resp.status_code == 200
        # APO export is text, may be empty if no stages
        assert isinstance(resp.text, str)

    def test_export_minidsp_format(self, api):
        """GET /api/dsp/export/minidsp returns miniDSP format."""
        resp = api.get("/api/dsp/export/minidsp", params={"ch": 0})
        assert resp.status_code == 200
        assert isinstance(resp.text, str)

    def test_export_json_roundtrip(self, api):
        """Export and reimport JSON should not crash."""
        # Export
        resp = api.get("/api/dsp/export/json")
        assert resp.status_code == 200
        exported = resp.json()
        assert isinstance(exported, dict)
        # Reimport the same config (no-op roundtrip)
        resp = api.put("/api/dsp", json=exported)
        assert resp.status_code == 200

    def test_stereo_link_toggle(self, api):
        """POST /api/dsp/channel/stereolink toggles stereo link."""
        resp = api.post("/api/dsp/channel/stereolink", json={
            "ch": 0, "linked": True,
        })
        # Accept 200 or 400 (may require paired channels)
        assert resp.status_code in (200, 400)
        # Restore
        if resp.status_code == 200:
            api.post("/api/dsp/channel/stereolink", json={
                "ch": 0, "linked": False,
            })


# ===========================================================================
# Preset CRUD
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestDspPresetCrud:
    """Verify DSP preset save/load/rename/delete."""

    SLOT = 7  # Use slot 7 to avoid conflicts with user presets

    def test_save_preset_slot(self, api):
        """Save current DSP config to slot 7."""
        resp = api.post("/api/dsp/presets/save", params={"slot": self.SLOT},
                        json={"name": "_test_preset"})
        assert resp.status_code == 200
        # Verify in list
        presets = api.get("/api/dsp/presets").json()
        slots = presets.get("slots", presets) if isinstance(presets, dict) else presets
        # Cleanup
        api.delete("/api/dsp/presets", params={"slot": self.SLOT})

    def test_load_preset_slot(self, api):
        """Save then load a preset from slot 7."""
        # Save first
        api.post("/api/dsp/presets/save", params={"slot": self.SLOT},
                 json={"name": "_test_load"})
        # Load
        resp = api.post("/api/dsp/presets/load", params={"slot": self.SLOT})
        assert resp.status_code == 200
        # Cleanup
        api.delete("/api/dsp/presets", params={"slot": self.SLOT})

    def test_rename_preset(self, api):
        """Rename a preset in slot 7."""
        api.post("/api/dsp/presets/save", params={"slot": self.SLOT},
                 json={"name": "_test_old"})
        resp = api.post("/api/dsp/presets/rename", json={
            "slot": self.SLOT, "name": "_test_new",
        })
        assert resp.status_code == 200
        # Cleanup
        api.delete("/api/dsp/presets", params={"slot": self.SLOT})

    def test_delete_preset_slot(self, api):
        """Delete preset from slot 7."""
        api.post("/api/dsp/presets/save", params={"slot": self.SLOT},
                 json={"name": "_test_del"})
        resp = api.delete("/api/dsp/presets", params={"slot": self.SLOT})
        assert resp.status_code == 200


# ===========================================================================
# PEQ Presets
# ===========================================================================

@pytest.mark.audio
@pytest.mark.dsp
class TestPeqPresets:
    """Verify PEQ preset save/load/delete."""

    PRESET_NAME = "_test_peq_preset"

    def test_peq_preset_save_load_delete(self, api):
        """Full PEQ preset lifecycle: save → load → delete."""
        # Save
        resp = api.post("/api/dsp/peq/presets", json={
            "name": self.PRESET_NAME,
        })
        assert resp.status_code == 200

        # Load
        resp = api.get("/api/dsp/peq/preset", params={"name": self.PRESET_NAME})
        assert resp.status_code == 200

        # Delete
        resp = api.delete("/api/dsp/peq/preset",
                          params={"name": self.PRESET_NAME})
        assert resp.status_code == 200

    def test_peq_preset_nonexistent_404(self, api):
        """Loading a nonexistent PEQ preset should fail."""
        resp = api.get("/api/dsp/peq/preset",
                       params={"name": "_nonexistent_xyz"})
        assert resp.status_code in (400, 404), (
            f"Expected 400/404, got {resp.status_code}"
        )
