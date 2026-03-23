/**
 * Programmatic fixture builders for Playwright E2E tests.
 *
 * Each builder returns a message object matching the corresponding
 * JSON fixture in e2e/fixtures/ws-messages/, with optional deep-merge
 * overrides for per-test customisation.
 */

/**
 * Deep-merge `overrides` into `defaults` (one level of nesting for objects,
 * arrays are replaced wholesale).
 */
function deepMerge(defaults, overrides) {
  const result = { ...defaults };
  for (const key of Object.keys(overrides)) {
    if (
      overrides[key] !== null &&
      typeof overrides[key] === 'object' &&
      !Array.isArray(overrides[key]) &&
      typeof result[key] === 'object' &&
      !Array.isArray(result[key]) &&
      result[key] !== null
    ) {
      result[key] = deepMerge(result[key], overrides[key]);
    } else {
      result[key] = overrides[key];
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Single HAL device entry (matches devices[0] in hal-device-state.json)
// ---------------------------------------------------------------------------
function buildHalDevice(overrides = {}) {
  return deepMerge({
    slot: 0,
    compatible: 'ti,pcm5102a',
    name: 'PCM5102A',
    type: 1,
    state: 3,
    discovery: 0,
    ready: true,
    i2cAddr: 0,
    channels: 2,
    capabilities: 16,
    manufacturer: 'Texas Instruments',
    busType: 2,
    busIndex: 0,
    pinA: 24,
    pinB: 0,
    busFreq: 0,
    sampleRates: 28,
    legacyId: 1,
    userLabel: 'PCM5102A DAC',
    cfgEnabled: true,
    cfgI2sPort: 0,
    cfgVolume: 100,
    cfgMute: false,
    cfgPinSda: 0,
    cfgPinScl: 0
  }, overrides);
}

// ---------------------------------------------------------------------------
// Full halDeviceState WS message (matches hal-device-state.json)
// ---------------------------------------------------------------------------
function buildHalDeviceState(overrides = {}) {
  const defaults = {
    type: 'halDeviceState',
    scanning: false,
    deviceCount: 16,
    deviceMax: 24,
    driverCount: 16,
    driverMax: 24,
    devices: [
      buildHalDevice(),
      buildHalDevice({
        slot: 1, compatible: 'everest-semi,es8311', name: 'ES8311',
        type: 3, i2cAddr: 24, capabilities: 199,
        manufacturer: 'Everest Semiconductor', busType: 1, busIndex: 1,
        pinA: 7, pinB: 8, busFreq: 400000, sampleRates: 27, legacyId: 4,
        userLabel: 'ES8311 Codec', cfgI2sPort: 2, cfgVolume: 80, cfgPinSda: 7, cfgPinScl: 8
      }),
      buildHalDevice({
        slot: 2, compatible: 'ti,pcm1808', name: 'PCM1808',
        type: 2, capabilities: 8, pinA: 23, sampleRates: 24, legacyId: 0,
        userLabel: 'PCM1808 ADC1'
      }),
      buildHalDevice({
        slot: 3, compatible: 'ti,pcm1808', name: 'PCM1808',
        type: 2, capabilities: 8, busIndex: 1, pinA: 25, sampleRates: 24, legacyId: 0,
        userLabel: 'PCM1808 ADC2', cfgI2sPort: 1
      }),
      buildHalDevice({
        slot: 4, compatible: 'ns,ns4150b-amp', name: 'NS4150B Amp',
        type: 4, channels: 1, capabilities: 0, manufacturer: 'Nsiway',
        busType: 4, pinA: 53, sampleRates: 0, legacyId: 0,
        userLabel: undefined, cfgEnabled: undefined, cfgI2sPort: undefined,
        cfgVolume: undefined, cfgMute: undefined, cfgPinSda: undefined, cfgPinScl: undefined
      }),
      buildHalDevice({
        slot: 5, compatible: 'espressif,esp32p4-temp', name: 'Chip Temperature',
        type: 6, channels: 1, capabilities: 0, manufacturer: 'Espressif',
        busType: 5, pinA: 0, sampleRates: 0, legacyId: 0, temperature: 42.5,
        userLabel: undefined, cfgEnabled: undefined, cfgI2sPort: undefined,
        cfgVolume: undefined, cfgMute: undefined, cfgPinSda: undefined, cfgPinScl: undefined
      }),
      buildHalDevice({
        slot: 7, compatible: 'ess,es9822pro', name: 'ES9822PRO',
        type: 2, discovery: 1, i2cAddr: 64, capabilities: 105,
        manufacturer: 'ESS Technology', busType: 1, busIndex: 2,
        pinA: 0, pinB: 0, busFreq: 400000, sampleRates: 28, legacyId: undefined,
        userLabel: '', cfgEnabled: true, cfgI2sPort: 2, cfgVolume: 80, cfgMute: false,
        cfgSampleRate: 48000, cfgBitDepth: 32, cfgPinSda: 28, cfgPinScl: 29,
        cfgPinData: 0, cfgPinMclk: 0, cfgMclkMultiple: 256, cfgI2sFormat: 0,
        cfgPgaGain: 6, cfgHpfEnabled: true, cfgPaControlPin: -1,
        cfgPinBck: 0, cfgPinLrc: 0, cfgPinFmt: -1, cfgFilterMode: 2
      }),
      buildHalDevice({
        slot: 8, compatible: 'ess,es9843pro', name: 'ES9843PRO',
        type: 2, discovery: 1, i2cAddr: 64, channels: 4, capabilities: 105,
        manufacturer: 'ESS Technology', busType: 1, busIndex: 2,
        pinA: 0, pinB: 0, busFreq: 400000, sampleRates: 28, legacyId: undefined,
        userLabel: '', cfgEnabled: true, cfgI2sPort: 2, cfgVolume: 100, cfgMute: false,
        cfgSampleRate: 48000, cfgBitDepth: 32, cfgPinSda: 28, cfgPinScl: 29,
        cfgPinData: 0, cfgPinMclk: 0, cfgMclkMultiple: 256, cfgI2sFormat: 0,
        cfgPgaGain: 0, cfgHpfEnabled: false, cfgPaControlPin: -1,
        cfgPinBck: 0, cfgPinLrc: 0, cfgPinFmt: -1, cfgFilterMode: 0
      })
    ]
  };

  // Clean up undefined values from device entries that lack config fields
  defaults.devices = defaults.devices.map(d => {
    const cleaned = {};
    for (const [k, v] of Object.entries(d)) {
      if (v !== undefined) cleaned[k] = v;
    }
    return cleaned;
  });

  return deepMerge(defaults, overrides);
}

// ---------------------------------------------------------------------------
// smartSensing WS message (matches smart-sensing.json)
// ---------------------------------------------------------------------------
function buildSmartSensing(overrides = {}) {
  return deepMerge({
    type: 'smartSensing',
    mode: 'smart_auto',
    timerDuration: 30,
    timerRemaining: 0,
    timerActive: false,
    amplifierState: false,
    audioThreshold: -40.0,
    audioLevel: -80.0,
    signalDetected: false,
    audioSampleRate: 48000
  }, overrides);
}

// ---------------------------------------------------------------------------
// hardware_stats WS message (matches hardware-stats.json)
// ---------------------------------------------------------------------------
function buildHardwareStats(overrides = {}) {
  return deepMerge({
    type: 'hardware_stats',
    cpu: {
      freqMHz: 360,
      model: 'ESP32-P4',
      revision: 1,
      cores: 2,
      usageCore0: 15.2,
      usageCore1: 52.7,
      usageTotal: 33.95,
      temperature: 42.5
    },
    memory: {
      heapTotal: 327680,
      heapFree: 182340,
      heapMinFree: 154880,
      heapMaxBlock: 131072,
      psramTotal: 8388608,
      psramFree: 4194304
    },
    storage: {
      flashSize: 16777216,
      sketchSize: 1245184,
      sketchFree: 2883584,
      LittleFSTotal: 1441792,
      LittleFSUsed: 32768
    },
    wifi: {
      rssi: -45,
      channel: 6,
      apClients: 0,
      connected: true
    },
    audio: {
      sampleRate: 48000,
      adcVref: 3.3,
      numAdcsDetected: 2,
      adcs: [
        {
          status: 'OK',
          noiseFloorDbfs: -72.5,
          i2sErrors: 0,
          consecutiveZeros: 0,
          totalBuffers: 14400,
          vrms: 0.0,
          snrDb: 72.5,
          sfdrDb: 68.0,
          i2sRecoveries: 0
        },
        {
          status: 'OK',
          noiseFloorDbfs: -71.8,
          i2sErrors: 0,
          consecutiveZeros: 0,
          totalBuffers: 14400,
          vrms: 0.0,
          snrDb: 71.8,
          sfdrDb: 67.2,
          i2sRecoveries: 0
        }
      ],
      fftWindowType: 2
    },
    uptime: 10000,
    resetReason: 'PowerOn',
    heapCritical: false,
    dmaAllocFailed: false,
    psramFallbackCount: 0,
    psramFailedCount: 0,
    psramAllocPsram: 155000,
    psramAllocSram: 0,
    psramWarning: false,
    psramCritical: false,
    crashHistory: [],
    tasks: {
      count: 6,
      loopUs: 1250,
      loopMaxUs: 8400,
      loopAvgUs: 1380,
      list: [
        { name: 'loopTask', stackFree: 4096, stackAlloc: 8192, pri: 1, state: 'Running', core: 1 },
        { name: 'audio_task', stackFree: 3072, stackAlloc: 6144, pri: 3, state: 'Blocked', core: 1 },
        { name: 'mqtt_task', stackFree: 2048, stackAlloc: 4096, pri: 2, state: 'Blocked', core: 0 },
        { name: 'gui_task', stackFree: 2560, stackAlloc: 5120, pri: 1, state: 'Blocked', core: 0 },
        { name: 'IDLE0', stackFree: 512, stackAlloc: 1024, pri: 0, state: 'Ready', core: 0 },
        { name: 'IDLE1', stackFree: 512, stackAlloc: 1024, pri: 0, state: 'Ready', core: 1 }
      ]
    },
    dac: {
      enabled: true,
      ready: true,
      detected: true,
      model: 'PCM5102A',
      deviceId: 1,
      volume: 100,
      mute: false,
      filterMode: 0,
      outputChannels: 2,
      txUnderruns: 0,
      manufacturer: 'Texas Instruments',
      hwVolume: false,
      i2cControl: false,
      independentClock: false,
      hasFilters: false,
      tx: {
        i2sTxEnabled: true,
        volumeGain: '1.0000',
        writeCount: 14400,
        bytesWritten: 921600,
        bytesExpected: 921600,
        peakSample: 0,
        zeroFrames: 14400
      },
      eeprom: {
        scanned: true,
        found: true,
        addr: 80,
        i2cMask: 1,
        i2cDevices: 1,
        readErrors: 0,
        writeErrors: 0,
        deviceName: 'PCM5102A',
        manufacturer: 'Texas Instruments',
        deviceId: 1,
        hwRevision: 1,
        maxChannels: 2,
        dacI2cAddress: 0,
        flags: 0,
        sampleRates: [44100, 48000, 96000]
      }
    }
  }, overrides);
}

// ---------------------------------------------------------------------------
// audioChannelMap WS message (matches audio-channel-map.json)
// ---------------------------------------------------------------------------
function buildAudioChannelMap(inputCount = 4, outputCount = 2) {
  const defaultInputs = [
    {
      lane: 0, name: 'ADC1', channels: 2, matrixCh: 0,
      deviceName: 'PCM1808', deviceType: 2, compatible: 'ti,pcm1808',
      manufacturer: 'Texas Instruments', capabilities: 8, ready: true
    },
    {
      lane: 1, name: 'ADC2', channels: 2, matrixCh: 2,
      deviceName: 'PCM1808', deviceType: 2, compatible: 'ti,pcm1808',
      manufacturer: 'Texas Instruments', capabilities: 8, ready: true
    },
    {
      lane: 2, name: 'SigGen', channels: 2, matrixCh: 4,
      deviceName: 'Signal Generator', deviceType: 2,
      manufacturer: '', capabilities: 0, ready: true
    },
    {
      lane: 3, name: 'USB Audio', channels: 2, matrixCh: 6,
      deviceName: 'USB Audio', deviceType: 2,
      manufacturer: '', capabilities: 0, ready: false
    }
  ];

  const defaultOutputs = [
    {
      index: 0, name: 'PCM5102A', firstChannel: 0, channels: 2,
      muted: false, compatible: 'ti,pcm5102a', manufacturer: 'Texas Instruments',
      capabilities: 16, ready: true, deviceType: 1, i2cAddr: 0
    },
    {
      index: 1, name: 'ES8311', firstChannel: 2, channels: 2,
      muted: false, compatible: 'everest-semi,es8311', manufacturer: 'Everest Semiconductor',
      capabilities: 199, ready: true, deviceType: 3, i2cAddr: 24
    }
  ];

  const inputs = defaultInputs.slice(0, inputCount);
  const outputs = defaultOutputs.slice(0, outputCount);

  // Build identity-like matrix rows (16x16)
  const matrixSize = 16;
  const zeroRow = new Array(matrixSize).fill('0.0000');
  const matrix = [];
  for (let r = 0; r < matrixSize; r++) {
    const row = [...zeroRow];
    if (r < 4) row[r] = '1.0000'; // first 4 rows are identity diagonal
    matrix.push(row);
  }

  return {
    type: 'audioChannelMap',
    inputs,
    outputs,
    matrixInputs: 16,
    matrixOutputs: 16,
    matrixBypass: false,
    matrix
  };
}

// ---------------------------------------------------------------------------
// debugLog WS message (matches debug-log.json)
// ---------------------------------------------------------------------------
function buildDebugLog(message, level = 'I', module = 'Test') {
  // Map short levels to fixture format
  const levelMap = { D: 'debug', I: 'info', W: 'warning', E: 'error' };
  return {
    type: 'debugLog',
    timestamp: 10000,
    level: levelMap[level] || level,
    module,
    message
  };
}

// ---------------------------------------------------------------------------
// wifiStatus WS message (matches wifi-status.json)
// ---------------------------------------------------------------------------
function buildWifiStatus(overrides = {}) {
  return deepMerge({
    type: 'wifiStatus',
    connected: true,
    ssid: 'TestNetwork',
    ip: '192.168.1.100',
    staIP: '192.168.1.100',
    rssi: -45,
    usingStaticIP: false,
    networkCount: 1,
    minSecurity: 0,
    'appState.wifiConnecting': false,
    'appState.wifiConnectSuccess': true,
    'appState.wifiNewIP': '192.168.1.100',
    'appState.apSSID': 'ALX-Nova-AP',
    ethLinkUp: false,
    ethConnected: false,
    ethIP: '',
    ethMAC: 'AA:BB:CC:DD:EE:FF',
    ethSpeed: 0,
    ethFullDuplex: false,
    ethGateway: '',
    ethSubnet: '',
    ethDns1: '',
    ethDns2: '',
    ethUseStaticIP: false,
    ethHostname: 'alx-nova',
    ethPendingConfirm: false,
    activeInterface: 'wifi',
    latestVersion: '1.12.1',
    'appState.updateAvailable': false
  }, overrides);
}

// ---------------------------------------------------------------------------
// mqttSettings WS message (matches mqtt-settings.json)
// ---------------------------------------------------------------------------
function buildMqttSettings(overrides = {}) {
  return deepMerge({
    type: 'mqttSettings',
    enabled: true,
    broker: '192.168.1.50',
    port: 1883,
    username: '',
    hasPassword: false,
    baseTopic: 'alx-nova',
    haDiscovery: true,
    connected: true
  }, overrides);
}

// ---------------------------------------------------------------------------
// displayState WS message (matches display-state.json)
// ---------------------------------------------------------------------------
function buildDisplayState(overrides = {}) {
  return deepMerge({
    type: 'displayState',
    backlightOn: true,
    screenTimeout: 30,
    backlightBrightness: 200,
    dimEnabled: true,
    dimTimeout: 60,
    dimBrightness: 64
  }, overrides);
}

// ---------------------------------------------------------------------------
// buzzerState WS message (matches buzzer-state.json)
// ---------------------------------------------------------------------------
function buildBuzzerState(overrides = {}) {
  return deepMerge({
    type: 'buzzerState',
    enabled: true,
    volume: 1
  }, overrides);
}

// ---------------------------------------------------------------------------
// dspState WS message (no fixture file yet — built from firmware broadcast shape)
// ---------------------------------------------------------------------------
function buildDspState(overrides = {}) {
  return deepMerge({
    type: 'dspState',
    dspEnabled: false,
    dspBypass: false,
    presetIndex: 0,
    presets: [
      { index: 0, name: 'Default', exists: true },
      { index: 1, name: '', exists: false },
      { index: 2, name: '', exists: false },
      { index: 3, name: '', exists: false }
    ],
    globalBypass: false,
    sampleRate: 48000,
    channels: [
      { bypass: false, stereoLink: false, stageCount: 0, stages: [] },
      { bypass: false, stereoLink: false, stageCount: 0, stages: [] },
      { bypass: false, stereoLink: false, stageCount: 0, stages: [] },
      { bypass: false, stereoLink: false, stageCount: 0, stages: [] }
    ]
  }, overrides);
}

// ---------------------------------------------------------------------------
// signalGenerator WS message (matches signal-generator.json)
// ---------------------------------------------------------------------------
function buildSignalGenerator(overrides = {}) {
  return deepMerge({
    type: 'signalGenerator',
    enabled: false,
    waveform: 0,
    frequency: 1000.0,
    amplitude: -20.0,
    channel: 0,
    outputMode: 0,
    sweepSpeed: 100.0
  }, overrides);
}

module.exports = {
  deepMerge,
  buildHalDevice,
  buildHalDeviceState,
  buildSmartSensing,
  buildHardwareStats,
  buildAudioChannelMap,
  buildDebugLog,
  buildWifiStatus,
  buildMqttSettings,
  buildDisplayState,
  buildBuzzerState,
  buildDspState,
  buildSignalGenerator
};
