        // ===== DSP / PEQ Preset Manager Overlay =====
        // Provides a two-tab overlay:
        //   Tab 1 — DSP Config Presets (32 named slots, backed by /api/dsp/presets)
        //   Tab 2 — PEQ Presets (10 named, backed by /api/dsp/peq/presets)

        var _dspPresetOverlayOpen = false;
        var _dspPresetTab = 'dsp';  // 'dsp' | 'peq'

        // ===== Open / Close =====

        function openDspPresetOverlay() {
            _dspPresetOverlayOpen = true;
            var overlay = document.getElementById('dspPresetOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'dspPresetOverlay';
                overlay.className = 'peq-overlay dsp-preset-overlay';
                document.body.appendChild(overlay);
                overlay.addEventListener('click', function(e) {
                    if (e.target === overlay) closeDspPresetOverlay();
                });
            }
            _renderDspPresetOverlay(overlay);
            overlay.style.display = 'flex';
            _loadDspPresets();
        }

        function closeDspPresetOverlay() {
            _dspPresetOverlayOpen = false;
            var overlay = document.getElementById('dspPresetOverlay');
            if (overlay) overlay.style.display = 'none';
        }

        function _renderDspPresetOverlay(overlay) {
            var html = '<div class="peq-overlay-inner dsp-preset-overlay-inner">';
            html += '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">';
            html += '    <svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M17,3H7A2,2 0 0,0 5,5V21L12,18L19,21V5C19,3.89 18.1,3 17,3Z"/></svg>';
            html += '    DSP Presets';
            html += '  </span>';
            html += '  <button class="peq-overlay-close" id="dspPresetClose" aria-label="Close presets">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';

            // Tab strip
            html += '<div class="dsp-preset-tabs">';
            html += '  <button class="dsp-preset-tab' + (_dspPresetTab === 'dsp' ? ' active' : '') + '" id="dspPresetTabDsp" onclick="switchDspPresetTab(\'dsp\')">DSP Configs (32 slots)</button>';
            html += '  <button class="dsp-preset-tab' + (_dspPresetTab === 'peq' ? ' active' : '') + '" id="dspPresetTabPeq" onclick="switchDspPresetTab(\'peq\')">PEQ Presets (10 named)</button>';
            html += '</div>';

            // Body
            html += '<div class="dsp-preset-body" id="dspPresetBody">';
            html += '  <div class="dsp-preset-loading">Loading...</div>';
            html += '</div>';

            // Footer
            html += '<div class="dsp-preset-footer">';
            html += '  <button class="btn btn-secondary btn-sm" onclick="closeDspPresetOverlay()">Close</button>';
            html += '</div>';
            html += '</div>';

            overlay.innerHTML = html;
            document.getElementById('dspPresetClose').addEventListener('click', closeDspPresetOverlay);
        }

        function switchDspPresetTab(tab) {
            _dspPresetTab = tab;
            var tabDsp = document.getElementById('dspPresetTabDsp');
            var tabPeq = document.getElementById('dspPresetTabPeq');
            if (tabDsp) tabDsp.classList.toggle('active', tab === 'dsp');
            if (tabPeq) tabPeq.classList.toggle('active', tab === 'peq');
            _loadDspPresets();
        }

        // ===== Load and Render =====

        function _loadDspPresets() {
            var body = document.getElementById('dspPresetBody');
            if (!body) return;
            body.innerHTML = '<div class="dsp-preset-loading">Loading...</div>';

            if (_dspPresetTab === 'dsp') {
                apiFetch('/api/dsp/presets')
                    .then(function(r) { return r.json(); })
                    .then(function(data) { _renderDspConfigPresets(data); })
                    .catch(function() {
                        if (body) body.innerHTML = '<div class="dsp-preset-error">Failed to load presets</div>';
                    });
            } else {
                apiFetch('/api/dsp/peq/presets')
                    .then(function(r) { return r.json(); })
                    .then(function(data) { _renderPeqPresets(data); })
                    .catch(function() {
                        if (body) body.innerHTML = '<div class="dsp-preset-error">Failed to load PEQ presets</div>';
                    });
            }
        }

        function _renderDspConfigPresets(data) {
            var body = document.getElementById('dspPresetBody');
            if (!body) return;

            var slots = data.slots || [];
            var activeIndex = data.activeIndex !== undefined ? data.activeIndex : -1;

            // Save-to-new button
            var html = '<div class="dsp-preset-actions">';
            html += '  <button class="btn btn-primary btn-sm" onclick="dspPresetSaveNew()">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M17,13H13V17H11V13H7V11H11V7H13V11H17M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg>';
            html += '    Save Current Config';
            html += '  </button>';
            html += '</div>';

            if (slots.length === 0) {
                html += '<div class="dsp-preset-empty">No presets saved. Save your current DSP configuration to create one.</div>';
            } else {
                html += '<div class="dsp-preset-list" id="dspConfigPresetList">';
                for (var i = 0; i < slots.length; i++) {
                    var slot = slots[i];
                    if (!slot.exists) continue;
                    var isActive = slot.index === activeIndex;
                    html += _dspPresetSlotHtml(slot, isActive);
                }
                html += '</div>';
            }

            body.innerHTML = html;
        }

        function _dspPresetSlotHtml(slot, isActive) {
            var html = '<div class="dsp-preset-item' + (isActive ? ' active' : '') + '" data-slot="' + slot.index + '">';
            html += '  <div class="dsp-preset-item-info">';
            html += '    <span class="dsp-preset-item-name">' + escapeHtml(slot.name || ('Preset ' + slot.index)) + '</span>';
            if (isActive) html += '    <span class="dsp-preset-item-badge">Active</span>';
            html += '  </div>';
            html += '  <div class="dsp-preset-item-actions">';
            html += '    <button class="btn btn-sm btn-primary" onclick="dspPresetLoad(' + slot.index + ')" title="Load preset">Load</button>';
            html += '    <button class="btn btn-sm btn-secondary" onclick="dspPresetRename(' + slot.index + ', ' + JSON.stringify(slot.name || '') + ')" title="Rename preset">';
            html += '      <svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg>';
            html += '    </button>';
            html += '    <button class="btn btn-sm btn-danger" onclick="dspPresetDelete(' + slot.index + ')" title="Delete preset">';
            html += '      <svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg>';
            html += '    </button>';
            html += '  </div>';
            html += '</div>';
            return html;
        }

        function _renderPeqPresets(data) {
            var body = document.getElementById('dspPresetBody');
            if (!body) return;

            var presets = data.presets || [];

            var html = '<div class="dsp-preset-actions">';
            html += '  <button class="btn btn-primary btn-sm" onclick="peqPresetSaveFromOverlay()">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M17,13H13V17H11V13H7V11H11V7H13V11H17M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg>';
            html += '    Save Current PEQ';
            html += '  </button>';
            html += '</div>';

            if (presets.length === 0) {
                html += '<div class="dsp-preset-empty">No PEQ presets saved. Open a PEQ overlay and save your EQ curve.</div>';
            } else {
                html += '<div class="dsp-preset-list" id="peqPresetList">';
                for (var i = 0; i < presets.length; i++) {
                    var p = presets[i];
                    html += '<div class="dsp-preset-item" data-name="' + escapeHtml(p.name) + '">';
                    html += '  <div class="dsp-preset-item-info">';
                    html += '    <span class="dsp-preset-item-name">' + escapeHtml(p.name) + '</span>';
                    if (p.channel !== undefined) html += '    <span class="dsp-preset-item-sub">Ch ' + p.channel + '</span>';
                    html += '  </div>';
                    html += '  <div class="dsp-preset-item-actions">';
                    html += '    <button class="btn btn-sm btn-primary" onclick="peqPresetLoad(' + JSON.stringify(p.name) + ')">Load</button>';
                    html += '    <button class="btn btn-sm btn-danger" onclick="peqPresetDelete(' + JSON.stringify(p.name) + ')">';
                    html += '      <svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg>';
                    html += '    </button>';
                    html += '  </div>';
                    html += '</div>';
                }
                html += '</div>';
            }

            body.innerHTML = html;
        }

        // ===== DSP Config Preset Actions =====

        function dspPresetSaveNew() {
            var name = prompt('Preset name (max 20 chars):');
            if (!name || name.trim() === '') return;
            name = name.trim().slice(0, 20);
            // Find first empty slot
            apiFetch('/api/dsp/presets')
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    var slots = data.slots || [];
                    var slot = -1;
                    for (var i = 0; i < 32; i++) {
                        var s = slots.find(function(x) { return x.index === i; });
                        if (!s || !s.exists) { slot = i; break; }
                    }
                    if (slot < 0) { showToast('All 32 preset slots are full', 'error'); return; }
                    return apiFetch('/api/dsp/presets/save?slot=' + slot, {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ name: name })
                    });
                })
                .then(function(r) { if (r) return r.json(); })
                .then(function(d) {
                    if (d && d.success) {
                        showToast('Preset saved', 'success');
                        _loadDspPresets();
                    } else {
                        showToast('Save failed', 'error');
                    }
                })
                .catch(function() { showToast('Save failed', 'error'); });
        }

        function dspPresetLoad(slot) {
            apiFetch('/api/dsp/presets/load?slot=' + slot, { method: 'POST' })
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success) {
                        showToast('Preset loaded', 'success');
                        closeDspPresetOverlay();
                    } else {
                        showToast('Load failed', 'error');
                    }
                })
                .catch(function() { showToast('Load failed', 'error'); });
        }

        function dspPresetDelete(slot) {
            if (!confirm('Delete this preset?')) return;
            apiFetch('/api/dsp/presets?slot=' + slot, { method: 'DELETE' })
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success) {
                        showToast('Preset deleted', 'success');
                        _loadDspPresets();
                    } else {
                        showToast('Delete failed', 'error');
                    }
                })
                .catch(function() { showToast('Delete failed', 'error'); });
        }

        function dspPresetRename(slot, currentName) {
            var name = prompt('New name:', currentName);
            if (!name || name.trim() === '' || name.trim() === currentName) return;
            name = name.trim().slice(0, 20);
            apiFetch('/api/dsp/presets/rename', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slot, name: name })
            })
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success) {
                        showToast('Preset renamed', 'success');
                        _loadDspPresets();
                    } else {
                        showToast('Rename failed', 'error');
                    }
                })
                .catch(function() { showToast('Rename failed', 'error'); });
        }

        function handleDspPresetList(data) {
            // Called when firmware broadcasts dspPresetList — refresh overlay if open
            if (_dspPresetOverlayOpen) _loadDspPresets();
        }

        // ===== PEQ Preset Actions =====

        function peqPresetSaveFromOverlay() {
            // If a PEQ overlay is open, save its current bands; otherwise use current channel
            var name = prompt('PEQ preset name (max 40 chars):');
            if (!name || name.trim() === '') return;
            name = name.trim().slice(0, 40);

            var bands = peqOverlayActive ? peqOverlayBands.slice() : [];
            var channel = peqOverlayActive && peqOverlayTarget ? peqOverlayTarget.channel : 0;

            apiFetch('/api/dsp/peq/presets', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name: name, channel: channel, stages: bands })
            })
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success) {
                        showToast('PEQ preset saved', 'success');
                        _loadDspPresets();
                    } else {
                        showToast('Save failed', 'error');
                    }
                })
                .catch(function() { showToast('Save failed', 'error'); });
        }

        function peqPresetLoad(name) {
            apiFetch('/api/dsp/peq/preset?name=' + encodeURIComponent(name))
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success && d.preset) {
                        if (peqOverlayActive && d.preset.stages) {
                            // Load bands into open PEQ overlay
                            peqOverlayBands = d.preset.stages.slice();
                            var tbody = document.getElementById('peqBandRows');
                            if (tbody) {
                                var html = '';
                                for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                                tbody.innerHTML = html;
                            }
                            peqDrawGraph();
                            showToast('PEQ preset loaded', 'success');
                        } else {
                            showToast('Open a PEQ editor first to apply this preset', 'info');
                        }
                        closeDspPresetOverlay();
                    } else {
                        showToast('Load failed', 'error');
                    }
                })
                .catch(function() { showToast('Load failed', 'error'); });
        }

        function peqPresetDelete(name) {
            if (!confirm('Delete PEQ preset "' + name + '"?')) return;
            apiFetch('/api/dsp/peq/preset', {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name: name })
            })
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.success) {
                        showToast('PEQ preset deleted', 'success');
                        _loadDspPresets();
                    } else {
                        showToast('Delete failed', 'error');
                    }
                })
                .catch(function() { showToast('Delete failed', 'error'); });
        }

        // ===== PEQ Overlay Quick-Save / Quick-Load =====
        // Called from PEQ overlay toolbar (defined in 06-peq-overlay.js openPeqOverlay)

        function peqOverlayQuickSave() {
            openDspPresetOverlay();
            // Switch to PEQ tab so user can save
            switchDspPresetTab('peq');
        }

        function peqOverlayQuickLoad() {
            openDspPresetOverlay();
            switchDspPresetTab('peq');
        }
