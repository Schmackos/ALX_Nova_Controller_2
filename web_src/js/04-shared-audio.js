        // ===== Shared Audio State =====

        // Dual ADC count
        const NUM_ADCS = 2;
        let numAdcsDetected = 1;
        let inputNames = ['Subwoofer 1','Subwoofer 2','Subwoofer 3','Subwoofer 4'];

        // Waveform and spectrum animation state — per-ADC
        let waveformCurrent = [null, null], waveformTarget = [null, null];
        let spectrumCurrent = [new Float32Array(16), new Float32Array(16)];
        let spectrumTarget = [new Float32Array(16), new Float32Array(16)];
        let currentDominantFreq = [0, 0], targetDominantFreq = [0, 0];
        let audioAnimFrameId = null;

        // Spectrum peak hold state — per-ADC
        let spectrumPeaks = [new Float32Array(16), new Float32Array(16)];
        let spectrumPeakTimes = [new Float64Array(16), new Float64Array(16)];

        // VU meter animation state — per-ADC [adc][L=0,R=1]
        let vuCurrent = [[0,0],[0,0]], vuTargetArr = [[0,0],[0,0]];
        let peakCurrent = [[0,0],[0,0]], peakTargetArr = [[0,0],[0,0]];
        let vuDetected = false;
        let vuAnimFrameId = null;
