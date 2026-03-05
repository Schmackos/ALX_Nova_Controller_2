        // ===== I/O Registry State =====
        var ioOutputs = [];
        var ioInputs = [];

        function handleIoRegistryState(d) {
            if (d.outputs) ioOutputs = d.outputs;
            if (d.inputs) ioInputs = d.inputs;
            renderIoRegistry();
        }

        function renderIoRegistry() {
            var outList = document.getElementById('ioOutputList');
            var inList = document.getElementById('ioInputList');
            if (!outList || !inList) return;

            // Render outputs
            var outHtml = '';
            for (var i = 0; i < ioOutputs.length; i++) {
                var o = ioOutputs[i];
                var disc = ['Builtin', 'EEPROM', 'Manual'][o.discovery] || 'Unknown';
                var readyClass = o.ready ? 'badge-success' : 'badge-secondary';
                var readyText = o.ready ? 'Ready' : 'Idle';
                outHtml += '<div class="info-row" style="display:flex;align-items:center;justify-content:space-between">';
                outHtml += '<span><strong>' + escapeHtml(o.name) + '</strong> <span style="opacity:0.6;font-size:11px">(I2S' + o.i2sPort + ', ch ' + o.firstChannel + '-' + (o.firstChannel + o.channelCount - 1) + ')</span></span>';
                outHtml += '<span>';
                outHtml += '<span class="badge ' + readyClass + '" style="font-size:10px;padding:2px 6px;margin-right:4px">' + readyText + '</span>';
                outHtml += '<span class="badge" style="font-size:10px;padding:2px 6px">' + disc + '</span>';
                if (o.discovery === 2) {
                    outHtml += ' <button class="btn btn-secondary" style="padding:1px 6px;font-size:10px;margin-left:4px" onclick="ioRemoveOutput(' + o.index + ')">Remove</button>';
                }
                outHtml += '</span></div>';
            }
            outList.innerHTML = outHtml || '<div class="info-row" style="opacity:0.5">No outputs registered</div>';

            // Render inputs
            var inHtml = '';
            for (var i = 0; i < ioInputs.length; i++) {
                var inp = ioInputs[i];
                var disc = ['Builtin', 'EEPROM', 'Manual'][inp.discovery] || 'Unknown';
                inHtml += '<div class="info-row" style="display:flex;align-items:center;justify-content:space-between">';
                inHtml += '<span><strong>' + escapeHtml(inp.name) + '</strong> <span style="opacity:0.6;font-size:11px">(I2S' + inp.i2sPort + ', ch ' + inp.firstChannel + '-' + (inp.firstChannel + inp.channelCount - 1) + ')</span></span>';
                inHtml += '<span class="badge" style="font-size:10px;padding:2px 6px">' + disc + '</span>';
                inHtml += '</div>';
            }
            inList.innerHTML = inHtml || '<div class="info-row" style="opacity:0.5">No inputs registered</div>';

            // Update counts
            var outCount = document.getElementById('ioOutputCount');
            var inCount = document.getElementById('ioInputCount');
            if (outCount) outCount.textContent = ioOutputs.length;
            if (inCount) inCount.textContent = ioInputs.length;
        }

        function escapeHtml(str) {
            var div = document.createElement('div');
            div.appendChild(document.createTextNode(str));
            return div.innerHTML;
        }

        async function ioAddManualOutput() {
            var name = document.getElementById('ioManualName').value.trim();
            var i2sPort = parseInt(document.getElementById('ioManualI2s').value) || 0;
            var channelCount = parseInt(document.getElementById('ioManualChannels').value) || 2;
            if (!name) { showToast('Name is required', 'error'); return; }

            try {
                var resp = await apiFetch('/api/io/output', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name: name, i2sPort: i2sPort, channelCount: channelCount })
                });
                var data = await resp.json();
                if (data.success) {
                    showToast('Output added to slot ' + data.slot, 'success');
                    document.getElementById('ioManualName').value = '';
                } else {
                    showToast(data.message || 'Failed to add output', 'error');
                }
            } catch (e) {
                showToast('Error: ' + e.message, 'error');
            }
        }

        async function ioRemoveOutput(idx) {
            try {
                var resp = await apiFetch('/api/io/output/remove', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ index: idx })
                });
                var data = await resp.json();
                if (data.success) {
                    showToast('Output removed', 'success');
                } else {
                    showToast(data.message || 'Failed to remove output', 'error');
                }
            } catch (e) {
                showToast('Error: ' + e.message, 'error');
            }
        }
