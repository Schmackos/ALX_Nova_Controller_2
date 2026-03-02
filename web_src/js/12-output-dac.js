        // ===== DAC Output =====
        function updateDac() {
            var en = document.getElementById('dacEnable').checked;
            document.getElementById('dacFields').style.display = en ? '' : 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setDacEnabled', enabled: en }));
                ws.send(JSON.stringify({
                    type: 'setDacVolume',
                    volume: parseInt(document.getElementById('dacVolume').value)
                }));
                ws.send(JSON.stringify({
                    type: 'setDacMute',
                    mute: document.getElementById('dacMute').checked
                }));
            }
        }
        function updateDacFilter() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'setDacFilter',
                    filterMode: parseInt(document.getElementById('dacFilterMode').value)
                }));
            }
        }
        function changeDacDriver() {
            var id = parseInt(document.getElementById('dacDriverSelect').value);
            apiFetch('/api/dac', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ deviceId: id })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('DAC driver changed', 'success');
                else showToast(data.message || 'Failed', 'error');
            })
            .catch(() => showToast('Failed to change DAC driver', 'error'));
        }
        function handleDacState(d) {
            var enEl = document.getElementById('dacEnable');
            if (enEl) enEl.checked = d.enabled;
            var fields = document.getElementById('dacFields');
            if (fields) fields.style.display = d.enabled ? '' : 'none';
            var model = document.getElementById('dacModel');
            if (model) model.textContent = d.modelName || '—';
            var volSlider = document.getElementById('dacVolume');
            if (volSlider) { volSlider.value = d.volume; document.getElementById('dacVolVal').textContent = d.volume; }
            var muteEl = document.getElementById('dacMute');
            if (muteEl) muteEl.checked = d.mute;
            var badge = document.getElementById('dacReadyBadge');
            if (badge) {
                badge.style.display = d.enabled ? '' : 'none';
                badge.textContent = d.ready ? 'Ready' : 'Not Ready';
                badge.style.background = d.ready ? '#4CAF50' : '#F44336';
                badge.style.color = '#fff';
            }
            var und = document.getElementById('dacUnderruns');
            if (und) und.textContent = d.txUnderruns || 0;
            // Filter modes
            var fg = document.getElementById('dacFilterGroup');
            if (d.filterModes && d.filterModes.length > 0) {
                if (fg) fg.style.display = '';
                var sel = document.getElementById('dacFilterMode');
                if (sel) {
                    sel.innerHTML = '';
                    d.filterModes.forEach(function(name, i) {
                        var opt = document.createElement('option');
                        opt.value = i; opt.textContent = name;
                        sel.appendChild(opt);
                    });
                    sel.value = d.filterMode || 0;
                }
            } else if (fg) {
                fg.style.display = 'none';
            }
            // Driver select
            if (d.drivers) {
                var drvSel = document.getElementById('dacDriverSelect');
                if (drvSel) {
                    drvSel.innerHTML = '';
                    d.drivers.forEach(function(drv) {
                        var opt = document.createElement('option');
                        opt.value = drv.id; opt.textContent = drv.name;
                        drvSel.appendChild(opt);
                    });
                    drvSel.value = d.deviceId;
                }
            }
        }
