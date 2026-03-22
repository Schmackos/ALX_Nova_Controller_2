        // ===== I2S Port Configuration =====
        var i2sPortData = null;

        function loadI2sPorts() {
            fetch('/api/i2s/ports')
                .then(function(r) { return r.ok ? r.json() : Promise.reject(r.status); })
                .then(function(data) {
                    i2sPortData = data;
                    renderI2sPorts();
                })
                .catch(function(e) { console.warn('[I2S] Load failed:', e); });
        }

        function renderI2sPorts() {
            var container = document.getElementById('i2s-ports-container');
            if (!container || !i2sPortData) return;

            var html = '<h3 style="margin:16px 0 8px">I2S Ports</h3>';

            var ports = i2sPortData.ports || [];
            for (var i = 0; i < ports.length; i++) {
                var port = ports[i];
                var txMode = port.tx.active ? (port.tx.mode === 'tdm' ? 'TDM' : 'STD') : 'Off';
                var rxMode = port.rx.active ? (port.rx.mode === 'tdm' ? 'TDM' : 'STD') : 'Off';
                var txBadgeCls = port.tx.active ? 'badge-green' : 'badge-grey';
                var rxBadgeCls = port.rx.active ? 'badge-green' : 'badge-grey';
                var masterCls = port.clocks.master ? 'badge-blue' : 'badge-grey';

                var txDetail = '';
                if (port.tx.active) {
                    txDetail = ' GPIO ' + port.tx.doutPin;
                    if (port.tx.mode === 'tdm' && port.tx.tdmSlots) txDetail += ' (' + port.tx.tdmSlots + 'ch)';
                }

                var rxDetail = '';
                if (port.rx.active) {
                    rxDetail = ' GPIO ' + port.rx.dinPin;
                    if (port.rx.mode === 'tdm' && port.rx.tdmSlots) rxDetail += ' (' + port.rx.tdmSlots + 'ch)';
                }

                var clockHtml = '';
                if (port.clocks.master) {
                    clockHtml = '<div style="margin-top:4px;opacity:0.7;font-size:0.8em">' +
                        'MCLK=' + port.clocks.mclk + ' BCK=' + port.clocks.bck + ' LRC=' + port.clocks.lrc +
                        '</div>';
                }

                html += '<div class="card" style="margin-bottom:8px">' +
                    '<div class="card-header" style="display:flex;justify-content:space-between;align-items:center;padding:8px 12px">' +
                        '<span style="font-weight:600">' +
                            '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true" style="vertical-align:middle;margin-right:4px">' +
                                '<path d="M12,3V13.55C11.41,13.21 10.73,13 10,13A4,4 0 0,0 6,17A4,4 0 0,0 10,21A4,4 0 0,0 14,17V7H18V3H12Z"/>' +
                            '</svg>' +
                            'Port ' + port.id +
                        '</span>' +
                        '<span class="badge ' + masterCls + '">' + (port.clocks.master ? 'Clock Master' : 'Data Only') + '</span>' +
                    '</div>' +
                    '<div class="card-body" style="font-size:0.85em;padding:8px 12px">' +
                        '<div style="display:flex;gap:16px;flex-wrap:wrap">' +
                            '<div><span class="badge ' + txBadgeCls + '">TX ' + txMode + '</span>' + txDetail + '</div>' +
                            '<div><span class="badge ' + rxBadgeCls + '">RX ' + rxMode + '</span>' + rxDetail + '</div>' +
                        '</div>' +
                        clockHtml +
                    '</div>' +
                '</div>';
            }

            if (i2sPortData.sampleRate) {
                html += '<div style="font-size:0.8em;opacity:0.6;margin-top:4px">Sample Rate: ' +
                    i2sPortData.sampleRate + ' Hz</div>';
            }

            container.innerHTML = html;
        }

        function handleI2sPortState(data) {
            if (data && data.ports) {
                i2sPortData = data;
                renderI2sPorts();
            }
        }
