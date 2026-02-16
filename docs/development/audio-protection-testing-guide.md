# Audio Protection & Diagnostics Testing Guide

This guide covers testing for all three phases of the audio protection system implemented in firmware v1.8.3+.

---

## Prerequisites

### Hardware Setup
- ESP32-S3 board with ALX Nova Controller firmware v1.8.3+
- PCM1808 I2S ADC connected (at least ADC1 on GPIO 16/17/18/3)
- DAC output on GPIO 40 (optional but recommended)
- Signal generator or audio source
- Speakers/headphones connected to DAC output
- Serial monitor at 115200 baud

### Software Tools
- PlatformIO CLI or VSCode with PlatformIO extension
- Web browser for Web UI access
- MQTT client (optional - e.g., MQTT Explorer, mosquitto_sub)
- Audio analysis tool (optional - e.g., Audacity, REW)

---

## Phase 1: Emergency Safety Limiter

**Purpose**: Prevent speaker damage by hard-limiting audio peaks above a configurable threshold.

### Test 1.1: Basic Limiter Functionality

**Setup**:
1. Build and upload firmware: `pio run --target upload`
2. Connect to Web UI: `http://alx-nova.local` (or IP address)
3. Navigate to Audio tab → Emergency Protection card
4. Set threshold to **-3.0 dBFS**
5. Enable Emergency Limiter (toggle ON)

**Test Procedure**:
```bash
# Enable signal generator via REST API
curl -X POST http://alx-nova.local/api/signalgenerator \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "waveform": 1,
    "frequency": 1000,
    "amplitude": 1.0,
    "channel": 2,
    "outputMode": 0,
    "targetAdc": 0
  }'
```

**Expected Results**:
- ✅ Web UI "Emergency Protection" badge turns **RED** ("ACTIVE")
- ✅ Trigger counter increments
- ✅ Gain Reduction shows ~3 dB
- ✅ DAC output clamps at -3 dBFS (verify with scope or audio analyzer)
- ✅ No overshoot or glitches audible

**Serial Log Check**:
```
[DSP] Emergency limiter active: GR=-3.2 dB, triggers=1
```

### Test 1.2: Limiter Attack/Release

**Test Procedure**:
1. Set amplitude to 0.5 (below threshold)
2. Wait 5 seconds
3. Quickly increase amplitude to 1.0 (above threshold)
4. Watch Web UI Gain Reduction meter

**Expected Results**:
- ✅ Attack time < 0.2 ms (instant response)
- ✅ Release time ~100 ms (smooth ramp down)
- ✅ No audible "pumping" or distortion

### Test 1.3: Disable Limiter

**Test Procedure**:
1. Toggle Emergency Limiter OFF in Web UI
2. Keep signal generator at 1.0 amplitude

**Expected Results**:
- ✅ Badge turns **GREEN** ("IDLE")
- ✅ No gain reduction applied
- ✅ DAC output passes through unprocessed
- ⚠️ **Warning**: Peaks may clip at 0 dBFS (hard clamp still active)

---

## Phase 2: DSP Swap Synchronization Fixes

**Purpose**: Eliminate audio pops/crackles when changing DSP configuration during playback.

### Test 2.1: DSP Config Change Under Load

**Setup**:
1. Enable DSP: Web UI → DSP tab → Enable DSP
2. Add 12 PEQ stages (any filter type)
3. Enable signal generator: 1 kHz sine @ -6 dBFS
4. Monitor serial output for swap failures

**Test Procedure**:
```bash
# Trigger 20 rapid DSP config changes
for i in {1..20}; do
  curl -X POST http://alx-nova.local/api/dsp/stages \
    -H "Content-Type: application/json" \
    -d '{"channel":0,"type":0,"enabled":true,"freq":1000,"gain":3.0,"q":1.0}' &
  sleep 0.1
done
wait
```

**Expected Results**:
- ✅ No audible pops or crackles
- ✅ Serial log shows: `[DSP] Config swapped (active=X)` for each change
- ✅ Swap success rate >99% (check `dspSwapSuccesses` vs `dspSwapFailures` in `/api/diagnostics`)
- ✅ No `[DSP] Swap timeout` errors
- ✅ Swap latency < 10 ms

**Check Swap Stats**:
```bash
curl http://alx-nova.local/api/diagnostics | grep -E "dspSwap"
```

**Expected Output**:
```json
"dspSwapSuccesses": 127,
"dspSwapFailures": 0
```

### Test 2.2: Multi-ADC Race Condition

**Setup**:
1. Connect both ADC1 and ADC2 (or leave ADC2 disconnected for NO_DATA)
2. Enable signal generator targeting both ADCs: `"targetAdc": 2`

**Test Procedure**:
1. Trigger DSP config change while both ADCs are actively processing
2. Repeat 10 times with different timing offsets

**Expected Results**:
- ✅ Swap always waits until both ADC buffers finish processing
- ✅ No partial swap between ADC1 and ADC2
- ✅ No clicks/pops even with dual ADC active

### Test 2.3: Swap Failure Retry Logic

**Setup**:
1. Set `TASK_STACK_SIZE_AUDIO` very low (e.g., 4096) to cause artificial pressure
2. Trigger swap during high CPU load

**Test Procedure**:
```bash
# Stress test with 100 rapid changes
for i in {1..100}; do
  curl -X POST http://alx-nova.local/api/dsp/stages \
    -H "Content-Type: application/json" \
    -d '{"channel":0,"type":0,"enabled":true,"freq":'$((1000 + RANDOM % 10000))',"gain":'$((RANDOM % 10))',"q":1.0}' &
  sleep 0.01
done
```

**Expected Results**:
- ✅ If timeout occurs, `dspSwapFailures` increments
- ✅ Web UI shows warning: "Last swap failure: X ms ago"
- ✅ No crashes or watchdog resets
- ✅ Failed swaps are staged for retry (check serial logs)

---

## Phase 3: Audio Quality Diagnostics

**Purpose**: Monitor audio pipeline health, detect glitches, and correlate with system events.

### Test 3.1: Enable Diagnostics

**Setup**:
1. Web UI → Debug tab → Audio Quality Diagnostics card
2. Toggle "Enable Diagnostics" **ON**
3. Set Glitch Threshold to **0.5** (default)

**Expected Results**:
- ✅ Diagnostics section appears on TFT Debug screen (if GUI enabled)
- ✅ Web UI shows live counters: Glitches Total, Last Minute, Last Glitch Type
- ✅ Correlation badges: DSP Swap, WiFi, MQTT (initially grey)

### Test 3.2: Glitch Detection - Discontinuity

**Test Procedure**:
1. Set signal generator to square wave @ 10 kHz (high slew rate)
2. Amplitude: 0.8 (to avoid overload)
3. Watch Web UI Last Glitch Type

**Expected Results**:
- ✅ Web UI shows "Last Glitch: **Discontinuity**"
- ✅ Glitches Total increments
- ✅ Serial log: `[AudioQuality] Glitch detected: Discontinuity on ADC1 CH0 (mag: 0.7, sample: 42)`

**Adjust Threshold**:
1. Increase threshold to **0.9** → glitches should stop
2. Decrease to **0.3** → glitches should increase
3. Verify threshold adjustment is live (no reboot needed)

### Test 3.3: Glitch Detection - DC Offset

**Test Procedure**:
1. Manually inject DC offset in signal generator (if available)
2. OR: Modify `audio_quality.cpp` line 129: change `0.7f` to `0.1f` temporarily

**Expected Results**:
- ✅ Web UI shows "Last Glitch: **DC Offset**"
- ✅ Serial log: `[AudioQuality] Glitch detected: DC Offset on ADC1 CH1 (mag: 0.85, sample: 0)`

### Test 3.4: Glitch Detection - Dropout

**Test Procedure**:
1. Disable signal generator (silence)
2. Wait for ADC to show NO_DATA status
3. Re-enable signal at very low amplitude (< -60 dBFS)

**Expected Results**:
- ✅ Web UI shows "Last Glitch: **Dropout**"
- ✅ Serial log: `[AudioQuality] Glitch detected: Dropout on ADC1 CH0 (mag: 0.92, sample: 0)`

### Test 3.5: Glitch Detection - Overload

**Test Procedure**:
1. Set signal generator amplitude to **1.0** (full scale)
2. Disable emergency limiter (to allow clipping)

**Expected Results**:
- ✅ Web UI shows "Last Glitch: **Overload**"
- ✅ Serial log: `[AudioQuality] Glitch detected: Overload on ADC1 CH0 (mag: 0.98, sample: 0)`
- ✅ ADC status shows "CLIPPING" in Debug tab

### Test 3.6: Event Correlation - DSP Swap

**Test Procedure**:
1. Enable signal generator (clean sine @ -6 dBFS)
2. Trigger DSP config change
3. Immediately check Web UI correlation badges (within 100ms)

**Expected Results**:
- ✅ "DSP Swap" badge turns **ORANGE** (active)
- ✅ If glitch occurred, correlation is logged
- ✅ Serial log: `[AudioQuality] DSP swap event marked`
- ✅ Badge clears after 100ms if no new glitches

### Test 3.7: Event Correlation - WiFi

**Test Procedure**:
1. Disconnect WiFi: Settings → WiFi → Disconnect
2. Watch for glitches during disconnect
3. Reconnect and watch for glitches

**Expected Results**:
- ✅ "WiFi" badge turns **ORANGE** if glitch occurred within 100ms of WiFi event
- ✅ Serial log: `[AudioQuality] WiFi event marked: wifi_disconnected`
- ✅ Serial log: `[AudioQuality] WiFi event marked: wifi_connected`

### Test 3.8: Timing Histogram

**Test Procedure**:
1. Let system run for 5 minutes with signal generator active
2. Check timing stats via REST API:

```bash
curl http://alx-nova.local/api/diagnostics | jq '.audioQuality.timingHist'
```

**Expected Output**:
```json
{
  "avgLatencyMs": 2.3,
  "maxLatencyMs": 4.8,
  "buckets": [0, 50, 120, 80, 10, 2, 0, 0, ...],
  "overflows": 0
}
```

**Expected Results**:
- ✅ Average latency: 1-5 ms (typical for 256-frame buffers @ 48kHz)
- ✅ Max latency: < 10 ms
- ✅ Overflows: 0 (no buffers exceeded 20ms)
- ✅ Histogram shows Gaussian distribution around 2-3ms

### Test 3.9: Memory Snapshot

**Test Procedure**:
1. Monitor memory over 1 minute
2. Query via REST API:

```bash
curl http://alx-nova.local/api/diagnostics | jq '.audioQuality.memoryHist'
```

**Expected Results**:
- ✅ 60 snapshots captured (1 per second)
- ✅ Free heap stable (no leaks)
- ✅ Max alloc heap > 40KB (not critical)

### Test 3.10: WebSocket Live Updates

**Setup**:
1. Open browser DevTools → Network → WS tab
2. Filter for `audioQualityDiag` messages

**Test Procedure**:
1. Enable diagnostics
2. Trigger glitches (square wave, DC offset, etc.)
3. Watch WebSocket messages every 5 seconds

**Expected Message**:
```json
{
  "type": "audioQualityDiag",
  "glitchesTotal": 42,
  "glitchesLastMinute": 5,
  "lastGlitchType": 1,
  "lastGlitchMs": 123456,
  "avgLatencyMs": "2.34",
  "maxLatencyMs": "4.56",
  "correlationDsp": false,
  "correlationWifi": false,
  "correlationMqtt": false
}
```

### Test 3.11: MQTT Integration

**Setup**:
1. Configure MQTT broker in Settings
2. Enable Home Assistant discovery

**Test Procedure**:
```bash
# Subscribe to audio quality topics
mosquitto_sub -h <broker> -t "homeassistant/sensor/alx_nova/audio_quality/#" -v
```

**Expected Topics**:
- `audio_quality/enabled` → "ON" / "OFF"
- `audio_quality/glitch_threshold` → "0.50"
- `audio_quality/glitches_total` → "42"
- `audio_quality/glitches_last_minute` → "5"
- `audio_quality/buffer_latency_avg` → "2.34"
- `audio_quality/correlation_dsp_swap` → "ON" / "OFF"
- `audio_quality/correlation_wifi` → "ON" / "OFF"

**Expected Results**:
- ✅ All topics publish correctly
- ✅ Home Assistant auto-discovers entities
- ✅ Entities update every 5 seconds (when enabled)

### Test 3.12: Reset Statistics

**Test Procedure**:
1. Accumulate some glitches (run square wave for 1 minute)
2. Click "Reset Stats" button in Web UI
3. Check counters

**Expected Results**:
- ✅ Glitches Total: **0**
- ✅ Glitches Last Minute: **0**
- ✅ Last Glitch Type: **None**
- ✅ Timing histogram cleared
- ✅ Settings preserved (enabled state, threshold)

---

## Integration Test: All Three Phases Together

### Test INT.1: Full Protection Under Stress

**Setup**:
1. Enable Emergency Limiter (threshold: -3 dBFS)
2. Enable Audio Quality Diagnostics (threshold: 0.5)
3. Load full DSP config (24 stages)
4. Enable signal generator (sine @ 1 kHz, -6 dBFS)

**Test Procedure**:
1. Run for 5 minutes
2. Trigger 50 DSP config changes (random timing)
3. Disconnect/reconnect WiFi 5 times
4. Ramp signal amplitude from 0.5 to 1.0 and back

**Expected Results**:
- ✅ No audible pops or crackles
- ✅ Emergency limiter activates when amplitude > -3 dBFS
- ✅ DSP swap success rate >99%
- ✅ Audio quality glitches correlate with DSP swaps (if any)
- ✅ No watchdog resets or crashes
- ✅ Heap stable (check `/api/diagnostics` → `heapCritical: false`)

**Performance Check**:
```bash
curl http://alx-nova.local/api/diagnostics | jq '{
  cpu: .tasks.loopAvgUs,
  heap: .freeHeap,
  swaps: {success: .dspSwapSuccesses, failures: .dspSwapFailures},
  glitches: .audioQuality.glitchHistory.totalGlitches,
  limiter: .emergencyLimiter.triggerCount
}'
```

**Expected Metrics**:
- CPU loop: < 3000 µs average (< 15% load @ 5ms loop)
- Free heap: > 80 KB (not critical)
- Swap failures: 0-2 (< 1% failure rate acceptable)
- Glitches: < 10 total (mostly during amplitude ramps)
- Limiter triggers: > 0 (only when amplitude > threshold)

---

## Troubleshooting

### Emergency Limiter Not Activating
**Symptoms**: Signal peaks above threshold, but no gain reduction

**Checks**:
1. Verify DSP is enabled: `/api/dsp` → `enabled: true`
2. Check limiter is enabled: `/api/diagnostics` → `emergencyLimiterEnabled: true`
3. Verify signal actually exceeds threshold:
   - REST API: `/api/smartsensing` → `audioLevel_dBFS > threshold`
4. Check DSP metrics: `/api/dsp/metrics` → `emergencyLimiterActive: true`

**Fix**: If DSP disabled, limiter is bypassed (guarded by `#ifdef DSP_ENABLED`)

### DSP Swap Failures
**Symptoms**: `dspSwapFailures` counter increasing

**Checks**:
1. Check audio task stack usage: `/api/diagnostics` → `tasks.list[audio_cap].stackFree`
2. Verify I2S read timeout not firing:
   - Serial log: Look for `[Audio] I2S read timeout (500ms)`
3. Check CPU load: `/api/diagnostics` → `tasks.loopAvgUs < 5000`

**Fix**:
- Increase `TASK_STACK_SIZE_AUDIO` in `src/config.h` (current: 12288)
- Reduce DSP stage count (target < 20 stages)
- Disable GUI if enabled (saves ~5% CPU)

### Audio Quality False Positives
**Symptoms**: Glitches detected on clean audio

**Checks**:
1. Verify ADC health: Debug tab → Audio ADC Status should be "OK"
2. Check noise floor: REST API → `audioNoiseFloor` should be < -60 dBFS
3. Verify waveform is clean: Web UI → Audio tab → Waveform graph

**Fix**:
- Increase glitch threshold (Web UI → 0.7 or 0.9)
- Check PCM1808 power supply (3.3V stable?)
- Verify ground connections (no ground loops)

### WebSocket Disconnects
**Symptoms**: Web UI doesn't update, diagnostics stop

**Checks**:
1. Browser DevTools → Console: Look for WebSocket errors
2. Serial log: `[WebSocket] Client X disconnected`
3. Check WiFi signal strength: Settings → WiFi → RSSI

**Fix**:
- Reduce WebSocket broadcast frequency (edit `main.cpp` → increase 5s interval)
- Check network MTU (1500 bytes typical)
- Disable audio waveform/spectrum if not needed (saves bandwidth)

---

## Performance Baselines

### Expected CPU Usage (ESP32-S3 @ 240 MHz)
- Idle: 5-10%
- DSP 24 stages: 30-40%
- DSP + Emergency Limiter: 35-45%
- DSP + Audio Quality Diagnostics: 40-50%
- All features + GUI: 50-60%

### Expected Memory Usage
- RAM: 44-48% (144-157 KB / 327 KB)
- Flash: 77-80% (2.6-2.7 MB / 3.3 MB)
- PSRAM: 10-20% (0.8-1.6 MB / 8 MB)

### Expected Latency
- I2S buffer read: 1-5 ms
- DSP processing: 0.5-2 ms per stage
- Total audio pipeline: 5-15 ms (imperceptible)

---

## Unit Tests

### Run All Tests
```bash
# Run native tests (no hardware needed)
pio test -e native -v

# Run specific test modules
pio test -e native -f test_emergency_limiter  # 10 tests
pio test -e native -f test_dsp_swap           # 9 tests
pio test -e native -f test_audio_quality      # 33 tests
```

**Expected Results**:
- ✅ All 52 new tests pass (10 + 9 + 33)
- ✅ Total test count: 842 tests (790 existing + 52 new)
- ✅ No memory leaks detected
- ✅ All edge cases covered

---

## Validation Checklist

Before releasing v1.8.3 to production:

- [ ] Phase 1: Emergency limiter prevents DAC clipping at configured threshold
- [ ] Phase 1: Attack time < 0.2 ms, release time ~100 ms
- [ ] Phase 1: Web UI, REST API, MQTT, and TFT GUI all control limiter correctly
- [ ] Phase 2: DSP config swaps produce zero audible pops/crackles
- [ ] Phase 2: Swap success rate >99% under normal load
- [ ] Phase 2: Multi-ADC race condition eliminated
- [ ] Phase 3: All 4 glitch types detected correctly
- [ ] Phase 3: Event correlation works (DSP swap, WiFi, MQTT)
- [ ] Phase 3: Timing histogram shows realistic latency distribution
- [ ] Phase 3: Memory snapshots show no leaks over 1 hour
- [ ] Integration: 24-hour soak test passes (no crashes, heap stable)
- [ ] Integration: All 842 unit tests pass
- [ ] Performance: CPU usage < 60%, heap > 40 KB free

---

## Further Reading

- **Implementation Plan**: `.claude/plans/lucky-strolling-breeze.md`
- **DSP Pipeline**: `memory/dsp_details.md`
- **Audio ADC**: `docs/hardware/pcm1808-integration-plan.md`
- **Release Notes**: `RELEASE_NOTES.md` (v1.8.3 section)
