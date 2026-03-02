        // ===== Canvas Helpers =====

        // Canvas dimension cache — avoid GPU texture realloc every frame
        let canvasDims = {};
        function resizeCanvasIfNeeded(canvas) {
            const id = canvas.id;
            const rect = canvas.getBoundingClientRect();
            const dpr = window.devicePixelRatio;
            const tw = Math.round(rect.width * dpr);
            const th = Math.round(rect.height * dpr);
            if (tw === 0 || th === 0) return -1; // not laid out yet
            const cached = canvasDims[id];
            if (cached && cached.tw === tw && cached.th === th) return false;
            canvas.width = tw;
            canvas.height = th;
            canvasDims[id] = { tw, th, w: rect.width, h: rect.height };
            return true;
        }

        // Offscreen canvas background cache — static grids/labels drawn once
        let bgCache = {};
        function invalidateBgCache() { bgCache = {}; }

        // Spectrum color LUT — 256 entries, avoids template literal per-bar per-frame
        const spectrumColorLUT = new Array(256);
        for (let i = 0; i < 256; i++) {
            const val = i / 255;
            const r = 255;
            const g = Math.round(152 - val * 109);
            const b = Math.round(val * 54);
            spectrumColorLUT[i] = 'rgb(' + r + ',' + g + ',' + b + ')';
        }

        // DOM element cache for VU meters — avoid getElementById per rAF frame
        let vuDomRefs = null;
        function cacheVuDomRefs() {
            vuDomRefs = {};
            for (let a = 0; a < NUM_ADCS; a++) {
                vuDomRefs['fillL' + a] = document.getElementById('vuFill' + a + 'L');
                vuDomRefs['fillR' + a] = document.getElementById('vuFill' + a + 'R');
                vuDomRefs['pkL' + a] = document.getElementById('vuPeak' + a + 'L');
                vuDomRefs['pkR' + a] = document.getElementById('vuPeak' + a + 'R');
                vuDomRefs['dbL' + a] = document.getElementById('vuDb' + a + 'L');
                vuDomRefs['dbR' + a] = document.getElementById('vuDb' + a + 'R');
                vuDomRefs['dbSegL' + a] = document.getElementById('vuDbSeg' + a + 'L');
                vuDomRefs['dbSegR' + a] = document.getElementById('vuDbSeg' + a + 'R');
            }
            vuDomRefs['dot'] = document.getElementById('audioSignalDot');
            vuDomRefs['txt'] = document.getElementById('audioSignalText');
        }
