/**
 * Deterministic mock state singleton.
 * Reset between tests via POST /api/__test__/reset.
 */

function createDefaultState() {
  return {
    authenticated: false,
    wifiConnected: true,
    wifiSSID: 'TestNetwork',
    wifiIP: '192.168.1.100',
    wifiRSSI: -45,
    mqttConnected: true,
    mqttBroker: '192.168.1.50',
    mqttPort: 1883,
    ampOn: false,
    sensingMode: 1,
    sensingThreshold: 400,
    autoOffTimer: 30,
    buzzerEnabled: true,
    buzzerVolume: 50,
    backlightOn: true,
    screenTimeout: 30,
    backlightBrightness: 200,
    dimEnabled: true,
    dimTimeout: 60,
    dimBrightness: 50,
    darkMode: false,
    sigGenEnabled: false,
    sigGenWaveform: 0,
    sigGenFrequency: 1000,
    sigGenAmplitude: 50,
    debugLevel: 2,
    debugTaskMonitor: false,
    audioSubscribed: false,
    vuMeterEnabled: true,
    waveformEnabled: true,
    spectrumEnabled: true,
    fftWindowType: 2,
    scanning: false,
    firmwareVersion: '1.12.0',
    halDevices: [
      { id: 0, name: 'PCM5102A', compatible: 'analog,pcm5102a', type: 'DAC', state: 'AVAILABLE', enabled: true, bus: 'I2S', address: '0x00' },
      { id: 1, name: 'ES8311', compatible: 'ti,es8311', type: 'CODEC', state: 'AVAILABLE', enabled: true, bus: 'I2C', address: '0x18' },
      { id: 2, name: 'PCM1808-1', compatible: 'ti,pcm1808', type: 'ADC', state: 'AVAILABLE', enabled: true, bus: 'I2S', address: '0x00' },
      { id: 3, name: 'PCM1808-2', compatible: 'ti,pcm1808', type: 'ADC', state: 'AVAILABLE', enabled: true, bus: 'I2S', address: '0x00' },
      { id: 4, name: 'NS4150B', compatible: 'ns,ns4150b', type: 'AMP', state: 'AVAILABLE', enabled: true, bus: 'GPIO', address: '53' },
      { id: 5, name: 'Chip Temp', compatible: 'espressif,temp-sensor', type: 'SENSOR', state: 'AVAILABLE', enabled: true, bus: 'Internal', address: '' }
    ],
    audioChannelMap: {
      inputs: [
        { id: 0, name: 'ADC1-L', type: 'ADC', halSlot: 2 },
        { id: 1, name: 'ADC1-R', type: 'ADC', halSlot: 2 },
        { id: 2, name: 'ADC2-L', type: 'ADC', halSlot: 3 },
        { id: 3, name: 'ADC2-R', type: 'ADC', halSlot: 3 },
        { id: 4, name: 'SigGen-L', type: 'SIGGEN', halSlot: 255 },
        { id: 5, name: 'SigGen-R', type: 'SIGGEN', halSlot: 255 },
        { id: 6, name: 'USB-L', type: 'USB', halSlot: 255 },
        { id: 7, name: 'USB-R', type: 'USB', halSlot: 255 }
      ],
      outputs: [
        { id: 0, name: 'PCM5102A-L', type: 'DAC', halSlot: 0 },
        { id: 1, name: 'PCM5102A-R', type: 'DAC', halSlot: 0 },
        { id: 2, name: 'ES8311-L', type: 'CODEC', halSlot: 1 },
        { id: 3, name: 'ES8311-R', type: 'CODEC', halSlot: 1 },
        { id: 4, name: 'NS4150B-L', type: 'AMP', halSlot: 4 },
        { id: 5, name: 'NS4150B-R', type: 'AMP', halSlot: 4 },
        { id: 6, name: 'Out7', type: 'NONE', halSlot: 255 },
        { id: 7, name: 'Out8', type: 'NONE', halSlot: 255 }
      ]
    },
    matrix: Array.from({ length: 8 }, () => Array(8).fill(0.0)),
  };
}

let _state = createDefaultState();
// Set default diagonal routing (input 0→output 0, 1→1, etc.)
_state.matrix[0][0] = 1.0;
_state.matrix[1][1] = 1.0;
_state.matrix[2][2] = 1.0;
_state.matrix[3][3] = 1.0;

function getState() { return _state; }

function resetState() { _state = createDefaultState(); _state.matrix[0][0] = 1.0; _state.matrix[1][1] = 1.0; _state.matrix[2][2] = 1.0; _state.matrix[3][3] = 1.0; }

module.exports = { getState, resetState };
