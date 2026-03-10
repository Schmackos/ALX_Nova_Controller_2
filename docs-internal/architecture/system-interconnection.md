# ALX Nova Controller 2 -- System Interconnection

Architecture reference for the ESP32-P4 amplifier controller. Diagrams render in GitHub markdown via Mermaid.

---

## 1. System Architecture

All functional data paths shown. HAL Bridge is sole sink lifecycle owner.

```mermaid
graph TB
    subgraph Core1["Core 1 (Audio-Only)"]
        AP["audio_pipeline_task<br/>(priority 3)"]
        ML["loopTask / main loop<br/>(priority 1)"]
    end

    subgraph Core0["Core 0"]
        MQTT["mqtt_task<br/>(priority 2)"]
        GUI["gui_task<br/>(priority 1)"]
        USB["usb_audio_task<br/>(priority 1)"]
        OTA["OTA one-shot tasks"]
    end

    subgraph HAL["HAL Framework (src/hal/)"]
        HM["HalDeviceManager<br/>singleton, 24 slots, 56-pin tracking"]
        HD["HalDiscovery<br/>I2C scan / EEPROM / manual"]
        HDB["HalDeviceDB<br/>builtin + LittleFS presets"]
        HR["HalDriverRegistry<br/>compatible string -> factory"]
        HPB["HalPipelineBridge<br/>(state change callback)"]
    end

    subgraph AudioPipeline["Audio Pipeline"]
        INP["8-Lane Input<br/>ADC1 | ADC2 | SigGen | USB | ..."]
        DSP["Per-Input DSP<br/>biquad, FIR, limiter"]
        MTX["16x16 Routing Matrix"]
        ODSP["Per-Output DSP<br/>biquad, gain, compressor"]
        SINKS["AudioOutputSink dispatch<br/>write() per registered sink"]
    end

    subgraph DacUtil["DAC Utility (Bus Management)"]
        DH["dac_hal.cpp<br/>I2S TX mgmt, volume curves,<br/>periodic logging"]
    end

    subgraph WebGUI["Web Interface"]
        WS["WebSocket Server<br/>port 81"]
        HTTP["HTTP Server<br/>port 80, REST API"]
        WEB["web_src/ frontend<br/>Audio Tab, HAL Devices"]
    end

    AS["AppState Singleton<br/>dirty flags + event group"]

    %% HAL discovery & device registration
    HM -->|"registerDevice()"| HR
    HD -->|"probe results"| HM
    HDB -->|"preset lookup"| HD

    %% Audio pipeline core flow
    AP --> INP --> DSP --> MTX --> ODSP --> SINKS

    %% Bridge owns sink registration
    HM -->|"state changes"| HPB
    HPB -->|"register/remove sink"| SINKS

    %% DAC utilities (bus layer)
    SINKS -->|"write(L,R,frames)"| DH

    %% State & main loop
    AS -->|"dirty flags + EVT_XXX"| ML
    ML -->|"dispatches"| WS
    MQTT -->|"poll 20Hz, independent"| AS
    GUI -->|"reads state"| AS
    USB -->|"ring buffer -> lane"| INP
    WEB -->|"WS frames"| WS
    HTTP -->|"/api/hal/*"| HM
```

**All paths are functional** (post DEBT-6 completion).

---

## 2. HAL Device Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> UNKNOWN: device created
    UNKNOWN --> DETECTED: probe() succeeds
    DETECTED --> CONFIGURING: init() called
    CONFIGURING --> AVAILABLE: init() succeeds
    AVAILABLE --> UNAVAILABLE: transient disable (_ready=false only)
    UNAVAILABLE --> AVAILABLE: re-enable (_ready=true)
    AVAILABLE --> MANUAL: user disable via web UI
    AVAILABLE --> REMOVED: device physically removed
    AVAILABLE --> ERROR: healthCheck() fails
    UNAVAILABLE --> ERROR: repeated failures
    MANUAL --> AVAILABLE: user re-enables
    REMOVED --> [*]

    note right of UNAVAILABLE
        Hybrid transient policy:
        - _ready = false (lock-free, audio pipeline skips)
        - Sink NOT unregistered (fast recovery)
        - HAL state broadcast to web UI
    end note

    note right of MANUAL
        Explicit teardown:
        - Sink unregistered from pipeline
        - I2S driver may be uninstalled
        - Config persisted to LittleFS
    end note

    note right of REMOVED
        Explicit teardown:
        - Sink unregistered
        - hal_pipeline_on_device_removed()
        - Slot freed in HalDeviceManager
    end note
```

**State transition rules:**

| Transition | Who triggers | Sink action |
|---|---|---|
| AVAILABLE -> UNAVAILABLE | `_ready = false` (e.g., I2C error) | None (sink stays registered, `isReady()` returns false) |
| AVAILABLE -> MANUAL | `hal_apply_config()` DISABLE path | `audio_pipeline_clear_sinks()` via `dac_output_deinit()` |
| AVAILABLE -> REMOVED | `DELETE /api/hal/devices` | `hal_pipeline_on_device_removed(slot)` |
| AVAILABLE -> ERROR | `healthCheck()` returns false | `hal_pipeline_on_device_removed(slot)` |

---

## 3. Boot Sequence

```mermaid
sequenceDiagram
    participant setup as setup()
    participant HAL as HAL Framework
    participant I2S as I2S Audio
    participant DAC as DAC Layer
    participant Pipeline as Audio Pipeline

    Note over setup: Core 1 (loopTask)

    setup->>HAL: hal_register_builtins()
    Note right of HAL: Register PCM5102A, ES8311,<br/>PCM1808, NS4150B, TempSensor factories

    setup->>HAL: hal_db_init()
    Note right of HAL: Load builtin device DB +<br/>LittleFS custom presets

    setup->>HAL: hal_load_device_configs()
    setup->>HAL: hal_load_custom_devices()
    setup->>HAL: hal_provision_defaults()
    Note right of HAL: Write /hal_auto_devices.json<br/>on first boot only

    setup->>HAL: hal_load_auto_devices()
    Note right of HAL: Instantiate add-on devices<br/>via HAL factory registry

    setup->>I2S: i2s_audio_init()
    Note right of I2S: ADC2 first (data only),<br/>then ADC1 (all clock pins).<br/>Creates audio_pipeline_task on Core 1.

    setup->>DAC: dac_secondary_init()
    Note right of DAC: ES8311 codec on I2S2 TX<br/>(P4 only, no-op on S3)

    setup->>HAL: hal_pipeline_sync()
    Note right of HAL: Count AVAILABLE DAC/ADC/CODEC<br/>devices (metadata only, no sink registration)

    setup->>DAC: (NS4150B, TempSensor, peripherals registered)
    Note right of DAC: Remaining HAL devices:<br/>LED, Relay, Button, Buzzer,<br/>Encoder, Display, SignalGen
```

**Critical ordering constraints:**
- `hal_register_builtins()` before `hal_load_auto_devices()` -- factories must exist before instantiation
- `i2s_audio_init()` before `dac_secondary_init()` -- ADC1 I2S clocks must be running for shared bus
- `output_dsp_init()` before `audio_pipeline_init()` -- DSP config loaded before pipeline starts processing
- `hal_pipeline_sync()` after all device registration -- ensures accurate count

---

## 4. HAL Pipeline Bridge Detail (Current Architecture)

The data flow after DEBT-6 completion. Bridge is sole sink lifecycle owner.

```mermaid
graph LR
    subgraph HAL
        DEV["HalDevice<br/>(state change)"]
        CB["stateChangeCb()"]
    end

    subgraph Bridge["HalPipelineBridge"]
        SYNC["hal_pipeline_on_device_available()"]
        UNSYNC["hal_pipeline_on_device_removed()"]
    end

    subgraph Pipeline["Audio Pipeline"]
        REG["audio_pipeline_register_sink()"]
        UNREG["audio_pipeline_remove_sink()"]
        SINKS["AudioOutputSink[]"]
    end

    DEV -->|"state -> AVAILABLE"| CB
    CB -->|"slot ID"| SYNC
    SYNC -->|"creates AudioOutputSink,<br/>calls register_sink()"| REG
    REG --> SINKS

    DEV -->|"state -> REMOVED/ERROR"| CB
    CB -->|"slot ID"| UNSYNC
    UNSYNC -->|"calls remove_sink(slot)"| UNREG
    UNREG --> SINKS
```

**Implementation completed in DEBT-6:**
- Bridge owns all sink lifecycle via state change callbacks
- `dac_hal.cpp` reduced to I2S TX management and volume curves
- DacRegistry and HalDacAdapter deleted
- Per-sink removal via `audio_pipeline_remove_sink(slot)` replaces `clear_sinks()`

---

## 5. Event-Driven Architecture

```mermaid
sequenceDiagram
    participant Producer as Producer<br/>(any task/ISR)
    participant AS as AppState<br/>(dirty flags)
    participant EG as FreeRTOS<br/>Event Group
    participant Loop as Main Loop<br/>(Core 1)
    participant WS as WebSocket<br/>Server

    Note over EG: 24 usable bits (0-23)<br/>Bits 24-31 reserved by FreeRTOS

    Producer->>AS: appState.markXxxDirty()
    AS->>AS: _xxxDirty = true
    AS->>EG: app_events_signal(EVT_XXX)
    Note right of EG: xEventGroupSetBits()

    Loop->>EG: app_events_wait(5ms)
    Note right of Loop: Wakes in <1us on any bit,<br/>or 5ms timeout if idle

    EG-->>Loop: returns set bits (pdTRUE clears)

    Loop->>AS: isXxxDirty()?
    AS-->>Loop: true

    Loop->>WS: sendXxxState()
    Loop->>AS: clearXxxDirty()
```

**Active event bits (16 assigned, 8 spare):**

| Bit | Define | Dirty flag | WebSocket dispatch |
|---|---|---|---|
| 0 | `EVT_OTA` | `_otaDirty` | `sendOTAStatus()` |
| 1 | `EVT_DISPLAY` | `_displayDirty` | `sendDisplaySettings()` |
| 2 | `EVT_BUZZER` | `_buzzerDirty` | `sendBuzzerSettings()` |
| 3 | `EVT_SIGGEN` | `_siggenDirty` | `sendSignalGenState()` |
| 4 | `EVT_DSP_CONFIG` | `_dspConfigDirty` | `sendDspConfig()` |
| 5 | `EVT_DAC` | `_dacDirty` | `sendDacState()` |
| 6 | `EVT_EEPROM` | `_eepromDirty` | `sendDacState()` |
| 7 | `EVT_USB_AUDIO` | `_usbAudioDirty` | `sendUsbAudioState()` |
| 8 | `EVT_USB_VU` | `_usbVuDirty` | `sendUsbAudioLevels()` |
| 9 | `EVT_SETTINGS` | `_settingsDirty` | `sendSettings()` |
| 10 | `EVT_ADC_ENABLED` | `_adcEnabledDirty` | `sendAdcEnabled()` |
| 11 | `EVT_ETHERNET` | `_ethernetDirty` | `sendWiFiStatus()` |
| 12 | `EVT_DAC_SETTINGS` | `_dacSettingsDirty` | `sendDacSettings()` |
| 13 | `EVT_HAL_DEVICE` | `_halDeviceDirty` | `sendHalDeviceState()` + `sendAudioChannelMap()` |
| 14 | `EVT_AUDIO_UPDATE` | `_audioUpdateDirty` | `sendAudioUpdate()` |
| 15 | `EVT_CHANNEL_MAP` | `_channelMapDirty` | `sendAudioChannelMap()` |

**MQTT runs independently:** The `mqtt_task` on Core 0 polls at 20 Hz (50ms `vTaskDelay`). It does NOT consume from the event group -- it reads dirty flags directly and publishes independently of the main loop's WebSocket dispatch.
