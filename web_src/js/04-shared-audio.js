        // ===== Shared Audio State =====

        // Dynamic input lane count — set by audioChannelMap broadcast
        let numInputLanes = 0;
        let numAdcsDetected = 1;
        let inputNames = [];

        // Waveform and spectrum animation state — per-input, dynamically sized
        let waveformCurrent = [], waveformTarget = [];
        let spectrumCurrent = [], spectrumTarget = [];
        let currentDominantFreq = [], targetDominantFreq = [];
        let audioAnimFrameId = null;

        // Spectrum peak hold state — per-input
        let spectrumPeaks = [], spectrumPeakTimes = [];

        // VU meter animation state — per-input [lane][L=0,R=1]
        let vuCurrent = [], vuTargetArr = [];
        let peakCurrent = [], peakTargetArr = [];
        let vuDetected = false;
        let vuAnimFrameId = null;

        // Resize all per-input audio arrays when audioChannelMap reports new count
        function resizeAudioArrays(count) {
            if (count === numInputLanes) return;
            numInputLanes = count;
            while (waveformCurrent.length < count) {
                waveformCurrent.push(null);
                waveformTarget.push(null);
                spectrumCurrent.push(new Float32Array(16));
                spectrumTarget.push(new Float32Array(16));
                currentDominantFreq.push(0);
                targetDominantFreq.push(0);
                spectrumPeaks.push(new Float32Array(16));
                spectrumPeakTimes.push(new Float64Array(16));
                vuCurrent.push([0, 0]);
                vuTargetArr.push([0, 0]);
                peakCurrent.push([0, 0]);
                peakTargetArr.push([0, 0]);
            }
            waveformCurrent.length = count;
            waveformTarget.length = count;
            spectrumCurrent.length = count;
            spectrumTarget.length = count;
            currentDominantFreq.length = count;
            targetDominantFreq.length = count;
            spectrumPeaks.length = count;
            spectrumPeakTimes.length = count;
            vuCurrent.length = count;
            vuTargetArr.length = count;
            peakCurrent.length = count;
            peakTargetArr.length = count;
            while (inputNames.length < count * 2) inputNames.push('Input ' + (inputNames.length + 1));
            inputNames.length = count * 2;
        }
