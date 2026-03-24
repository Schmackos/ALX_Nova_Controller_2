/**
 * WebSocket mock helper functions for Playwright E2E tests.
 *
 * buildInitialState()   — ordered array of all initial-state WS messages
 * handleCommand()       — routes inbound WS commands, returns response array
 * buildWaveformFrame()  — binary 0x01 frame (258 bytes)
 * buildSpectrumFrame()  — binary 0x02 frame (70 bytes)
 */

const path = require('path');
const fs   = require('fs');

const FIXTURE_DIR = path.join(__dirname, '..', 'fixtures', 'ws-messages');

/** Read a fixture JSON file and return parsed object. */
function loadFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}

/**
 * Returns an ordered array of all initial-state messages sent after authSuccess.
 * Order matches the requirement: wifiStatus, smartSensing, displayState,
 * buzzerState, mqttSettings, halDeviceState, audioChannelMap, audioGraphState,
 * signalGenerator, debugState.
 *
 * Fixture filenames use kebab-case matching the existing fixtures directory.
 */
function buildInitialState() {
  return [
    'wifi-status',
    'smart-sensing',
    'display-state',
    'buzzer-state',
    'mqtt-settings',
    'hal-device-state',
    'audio-channel-map',
    'audio-graph-state',
    'signal-generator',
    'debug-state',
  ].map(loadFixture);
}

/**
 * Routes an inbound WS command from the frontend and returns an array of
 * JSON response messages to send back (may be empty).
 *
 * @param {string} type   - data.type from the frontend message
 * @param {object} data   - full parsed message object
 * @returns {object[]}    - array of response message objects
 */
function handleCommand(type, data) {
  switch (type) {
    case 'subscribeAudio':
      // Acknowledge but send no state data in tests — tests use explicit sends
      return [];

    case 'setDebugHwStats':
    case 'setDebugI2sMetrics':
    case 'setDebugTaskMonitor':
    case 'setDebugMode':
      return [{ type: 'debugState', [type.replace('set', '').charAt(0).toLowerCase() + type.replace('set', '').slice(1)]: data.enabled }];

    case 'setSerialLogLevel':
    case 'setDebugSerialLevel':
      return [];

    case 'setStatsInterval':
    case 'setAudioUpdateRate':
      return [];

    case 'manualOverride':
      return [{ type: 'smartSensing', ampOn: !!data.on, signalDetected: false, audioLevel: -96.0, sensingMode: 2, timerDuration: 15, audioThreshold: -60, timerActive: false, timerRemaining: 0 }];

    case 'updateSensingMode':
      return [{ type: 'smartSensing', sensingMode: data.mode, ampOn: false, signalDetected: false, audioLevel: -96.0, audioVrms: 0.0, timerDuration: 15, audioThreshold: -60, timerActive: false, timerRemaining: 0 }];

    case 'eepromScan':
      return [{ type: 'hardware_stats', cpu: { usage: 0 }, dac: { eeprom: { scanned: true, found: false, i2cMask: 0, i2cDevices: 0, readErrors: 0, writeErrors: 0 } } }];

    case 'setEthConfig':
      return [{ type: 'wifiStatus', ethHostname: data.hostname || 'alx-nova', ethUseStaticIP: !!data.useStaticIP, ethPendingConfirm: !!data.useStaticIP }];

    case 'confirmEthConfig':
      return [{ type: 'wifiStatus', ethPendingConfirm: false }];

    case 'setHostname':
      return [{ type: 'wifiStatus', ethHostname: data.hostname || 'alx-nova' }];

    // ===== HAL device commands =====
    case 'setDeviceEnabled':
      return [loadFixture('hal-device-state')];

    // ===== Input pipeline commands =====
    case 'setInputGain':
      return [];

    case 'setInputMute':
      return [];

    case 'setInputPhase':
      return [];

    // ===== Output pipeline commands =====
    case 'setOutputGain':
      return [];

    case 'setOutputHwVolume':
      return [];

    case 'setOutputMute':
      return [];

    case 'setOutputPhase':
      return [];

    case 'setOutputDelay':
      return [];

    // ===== Signal generator =====
    case 'setSignalGen':
      return [Object.assign({}, loadFixture('signal-generator'), {
        enabled: data.enabled !== undefined ? data.enabled : false,
        waveform: data.waveform !== undefined ? data.waveform : 0,
        frequency: data.frequency !== undefined ? data.frequency : 1000.0,
        amplitude: data.amplitude !== undefined ? data.amplitude : -20.0,
      })];

    // ===== Display / brightness commands =====
    case 'setBrightness':
      return [{ type: 'displayState', backlightBrightness: data.value }];

    case 'setScreenTimeout':
      return [{ type: 'displayState', screenTimeout: data.value }];

    case 'setDimTimeout':
      return [{ type: 'displayState', dimTimeout: data.value }];

    case 'setDimBrightness':
      return [{ type: 'displayState', dimBrightness: data.value }];

    // ===== Buzzer commands =====
    case 'setBuzzerEnabled':
      return [{ type: 'buzzerState', enabled: !!data.enabled, volume: 1 }];

    case 'setBuzzerVolume':
      return [{ type: 'buzzerState', enabled: true, volume: data.value }];

    // ===== Audio / FFT settings =====
    case 'setFftWindowType':
      return [];

    default:
      return [];
  }
}

/**
 * Builds a binary waveform frame (type 0x01).
 * Format: [type:u8][adc:u8][samples:256xu8]  = 258 bytes total.
 *
 * @param {number}   adc     - ADC index (0 or 1)
 * @param {number[]} samples - 256 uint8 amplitude values (0-255)
 * @returns {Buffer}
 */
function buildWaveformFrame(adc, samples) {
  if (!samples || samples.length !== 256) {
    samples = new Array(256).fill(128); // silence
  }
  const buf = Buffer.alloc(258);
  buf.writeUInt8(0x01, 0);
  buf.writeUInt8(adc & 0xff, 1);
  for (let i = 0; i < 256; i++) {
    buf.writeUInt8(samples[i] & 0xff, 2 + i);
  }
  return buf;
}

/**
 * Builds a binary spectrum frame (type 0x02).
 * Format: [type:u8][adc:u8][dominantFreq:f32LE][bands:16xf32LE] = 70 bytes total.
 *
 * @param {number}   adc   - ADC index (0 or 1)
 * @param {number}   freq  - dominant frequency in Hz (float32)
 * @param {number[]} bands - 16 band magnitudes (0.0-1.0)
 * @returns {Buffer}
 */
function buildSpectrumFrame(adc, freq, bands) {
  if (!bands || bands.length !== 16) {
    bands = new Array(16).fill(0.0);
  }
  const buf = Buffer.alloc(70);
  buf.writeUInt8(0x02, 0);
  buf.writeUInt8(adc & 0xff, 1);
  buf.writeFloatLE(freq || 0.0, 2);
  for (let i = 0; i < 16; i++) {
    buf.writeFloatLE(bands[i] || 0.0, 6 + i * 4);
  }
  return buf;
}

module.exports = { buildInitialState, handleCommand, buildWaveformFrame, buildSpectrumFrame };
