# Architecture Diagrams

Open this file in VSCode and press `Ctrl+Shift+V` to preview all diagrams.

---

## System Architecture

```mermaid
graph TB
    classDef core1 fill:#1a3a5c,stroke:#2196F3,color:#fff
    classDef core0 fill:#1a4a3a,stroke:#26A69A,color:#fff
    classDef hal fill:#2d5016,stroke:#4CAF50,color:#fff
    classDef pipeline fill:#4a1a5c,stroke:#9C27B0,color:#fff
    classDef legacy fill:#5c4a00,stroke:#FF9800,color:#fff
    classDef web fill:#5c1a3a,stroke:#E91E63,color:#fff
    classDef state fill:#3a3a3a,stroke:#9E9E9E,color:#fff

    subgraph Core1["Core 1 (Audio-Only)"]
        AP["audio_pipeline_task\n(priority 3, DMA + DSP)"]
        ML["loopTask / main loop\n(priority 1, WS dispatch)"]
    end

    subgraph Core0["Core 0"]
        MQTT["mqtt_task\n(priority 2, 20Hz poll)"]
        GUI["gui_task\n(LVGL on TFT)"]
        USB["usb_audio_task\n(TinyUSB UAC2)"]
        OTA["OTA one-shot tasks"]
    end

    subgraph HAL["HAL Framework (src/hal/)"]
        HM["HalDeviceManager\nsingleton, 16 slots, 24-pin tracking"]
        HD["HalDiscovery\nI2C scan / EEPROM / manual"]
        HDB["HalDeviceDB\nbuiltin + LittleFS presets"]
        HR["HalDriverRegistry\ncompatible string -> factory"]
        HPB["HalPipelineBridge\nstate change callback\nslot-indexed sink API"]
        HS["HalSettings\nhal_apply_config()"]
    end

    subgraph AudioPipeline["Audio Pipeline"]
        INP["8-Lane Input\nHAL-managed dynamic sources\nADC | SigGen | USB via getInputSource()"]
        DSP["Per-Input DSP\nbiquad, FIR, limiter"]
        MTX["16x16 Routing Matrix\nfloat32 mix + gain"]
        ODSP["Per-Output DSP\n8 mono channels, 12 stages"]
        SINKS["Slot-Indexed Sink Dispatch\nset_sink(slot) / remove_sink(slot)\nhalSlot for O(1) HAL lookup"]
    end

    subgraph LegacyDAC["DAC Layer (dac_hal.cpp)"]
        style LegacyDAC stroke-dasharray: 5 5
        DH["dac_hal.cpp\nI2S TX driver, HalDacAdapter"]
        DR["DacRegistry\n(parallel to HalDriverRegistry)"]
    end

    subgraph WebGUI["Web Interface"]
        WS["WebSocket Server\nport 81, binary + JSON"]
        HTTP["HTTP Server\nport 80, REST API"]
        WEB["web_src/ frontend\nAudio Tab, HAL Devices"]
    end

    AS["AppState Singleton\ndirty flags + FreeRTOS event group\n24 usable bits (EVT_ANY = 0x00FFFFFF)"]

    HM -->|"registerDevice()"| HR
    HD -->|"probe results"| HM
    HDB -->|"preset lookup"| HD
    HS -->|"config update\nfires stateChangeCb"| HM

    HM -->|"stateChangeCb(slot,\noldState, newState)"| HPB
    HPB -->|"AVAILABLE:\nset_sink(slot, sink)"| SINKS
    HPB -->|"MANUAL/ERROR/REMOVED:\nremove_sink(slot)"| SINKS
    HPB -->|"ADC available:\nadcEnabled[lane]=true"| INP

    AP --> INP --> DSP --> MTX --> ODSP --> SINKS

    DH -->|"creates AudioOutputSink\nwith halSlot field"| SINKS
    DH -->|"wraps as\nHalDacAdapter"| HM

    AS -->|"dirty flags +\nEVT_XXX bits"| ML
    ML -->|"dispatch:\nsendHalDeviceState()\nsendAudioChannelMap()"| WS
    HPB -->|"markHalDeviceDirty()\nmarkChannelMapDirty()"| AS

    MQTT -->|"poll 20Hz\nindependent"| AS
    GUI -->|"reads state"| AS
    USB -->|"ring buffer\n-> HAL-assigned lane"| INP

    WEB -->|"WS frames"| WS
    HTTP -->|"/api/hal/*"| HS
    WS -->|"halSlot O(1)\ndevice lookup"| HM

    DH -.->|"audioPaused +\nbinary semaphore"| AP

    class AP,ML core1
    class MQTT,GUI,USB,OTA core0
    class HM,HD,HDB,HR,HPB,HS hal
    class INP,DSP,MTX,ODSP,SINKS pipeline
    class DH,DR legacy
    class WS,HTTP,WEB web
    class AS state
```

---

## HAL Device Lifecycle

```mermaid
stateDiagram-v2
    [*] --> UNKNOWN: registerDevice()

    UNKNOWN --> DETECTED: probe() ACK
    DETECTED --> CONFIGURING: init() called
    CONFIGURING --> AVAILABLE: init() success\n[bridge: set_sink(slot)]
    CONFIGURING --> ERROR: init() failure

    AVAILABLE --> UNAVAILABLE: healthCheck fail\n[bridge: no action, _ready=false]
    UNAVAILABLE --> AVAILABLE: healthCheck pass\n[bridge: no action, _ready=true]
    UNAVAILABLE --> ERROR: 3 consecutive fails\n[bridge: remove_sink(slot)]

    AVAILABLE --> MANUAL: user disable via Web UI\n[bridge: remove_sink(slot)]
    MANUAL --> CONFIGURING: user re-enable\n[deferred via _pendingDacToggle]

    AVAILABLE --> ERROR: fatal I2C/driver error\n[bridge: remove_sink(slot)]
    ERROR --> CONFIGURING: reinit requested\n[POST /api/hal/devices/reinit]

    AVAILABLE --> REMOVED: device absent on rescan\n[bridge: remove_sink(slot)]
    REMOVED --> [*]

    note right of AVAILABLE
        Pipeline active:
        - isReady() returns true
        - Sink dispatches audio
        - VU metering active
        - halSlot links to HAL device
    end note

    note right of UNAVAILABLE
        Hybrid transient policy:
        - volatile _ready = false
        - Sink STAYS registered
        - isReady() returns false
        - Pipeline skips (no dropout)
        - Auto-recovery when _ready=true
    end note

    note right of MANUAL
        Explicit teardown:
        - remove_sink(slot) called
        - DAC: deferred I2S deinit
          via _pendingDacToggle
        - ADC: adcEnabled[lane]=false
        - Config persisted to LittleFS
    end note

    note right of ERROR
        Explicit teardown:
        - remove_sink(slot) called
        - ADC lane disabled
        - Device stays in manager
        - Can be reinit'd via API
    end note
```

---

## HAL-Pipeline Bridge

```mermaid
flowchart TB
    subgraph triggers["State Change Triggers"]
        T1["Web UI: Disable device\nPUT /api/hal/devices"]
        T2["healthCheckAll()\n30s periodic timer"]
        T3["initAll()\nboot-time init"]
        T4["removeDevice()\nDELETE /api/hal/devices"]
        T5["hal_apply_config()\nenable/disable/reconfigure"]
    end

    subgraph manager["HalDeviceManager"]
        CB["_stateChangeCb(\n  slot,\n  oldState,\n  newState\n)"]
    end

    subgraph bridge["HalPipelineBridge"]
        SC["hal_pipeline_state_change()"]
        AVL["on_device_available(slot)"]
        UNV["on_device_unavailable(slot)"]
        REM["on_device_removed(slot)"]
    end

    subgraph tables["Mapping Tables"]
        SINK_MAP["_halSlotToSinkSlot[16]\nCapability-based ordinal counting\nDAC_PATH devices -> slot 0..7"]
        ADC_MAP["_halSlotToAdcLane[16]\nCapability-based ordinal counting\nADC_PATH devices -> lane 0..7"]
    end

    subgraph pipeline["Audio Pipeline"]
        SET["audio_pipeline_set_sink(\n  sinkSlot, &sink\n)\nvTaskSuspendAll() atomic"]
        RMV["audio_pipeline_remove_sink(\n  sinkSlot\n)\nzeroes slot, halSlot=0xFF"]
        ADC_EN["appState.adcEnabled[lane]\ntrue/false"]
    end

    subgraph output["Pipeline Effect"]
        FLOWS["Audio flows to device\nsink->write() called each DMA cycle"]
        SKIPS["Audio skipped\nisReady() returns false"]
        STOPS["Audio stopped\nsink slot empty, no dispatch"]
    end

    subgraph dirty["Dirty Flags -> Web UI"]
        DF["markHalDeviceDirty()\nmarkChannelMapDirty()\nmarkAdcEnabledDirty()"]
        WS["Main loop dispatches:\nsendHalDeviceState()\nsendAudioChannelMap()"]
    end

    T1 --> CB
    T2 --> CB
    T3 --> CB
    T4 --> CB
    T5 --> CB

    CB --> SC

    SC -->|"AVAILABLE"| AVL
    SC -->|"UNAVAILABLE"| UNV
    SC -->|"MANUAL / ERROR / REMOVED"| REM

    AVL --> SINK_MAP
    AVL --> ADC_MAP
    SINK_MAP -->|"DAC/CODEC"| SET
    ADC_MAP -->|"ADC"| ADC_EN
    SET --> FLOWS
    ADC_EN -->|"true"| FLOWS

    UNV -->|"volatile _ready=false\nno table changes"| SKIPS

    REM -->|"clear mapping"| SINK_MAP
    REM -->|"clear mapping"| ADC_MAP
    SINK_MAP -->|"DAC/CODEC"| RMV
    ADC_MAP -->|"ADC"| ADC_EN
    RMV --> STOPS
    ADC_EN -->|"false"| STOPS

    AVL --> DF
    REM --> DF
    DF --> WS
```

---

## Boot Sequence

```mermaid
sequenceDiagram
    participant setup as setup() [Core 1]
    participant HAL as HAL Framework
    participant I2S as I2S Audio
    participant DAC as DAC Layer
    participant Bridge as Pipeline Bridge
    participant Pipeline as Audio Pipeline

    Note over setup: ESP32-P4 boot on Core 1 (loopTask)

    setup->>HAL: hal_register_builtins()
    Note right of HAL: Register factory functions:<br/>PCM5102A, ES8311, PCM1808,<br/>NS4150B, TempSensor

    setup->>HAL: hal_db_init()
    Note right of HAL: Load builtin device descriptors +<br/>LittleFS custom presets

    setup->>HAL: hal_load_device_configs()
    Note right of HAL: /hal_config.json -> _configs[slot]<br/>(pre-populate before registration)

    setup->>HAL: hal_load_custom_devices()
    Note right of HAL: /hal/custom/*.json -> HalCustomDevice

    setup->>HAL: hal_provision_defaults()
    Note right of HAL: Write /hal_auto_devices.json<br/>on first boot only

    setup->>HAL: hal_load_auto_devices()
    Note right of HAL: PCM5102A (probeOnly=true),<br/>PCM1808 x2 (probe+init=AVAILABLE)

    setup->>I2S: i2s_audio_init()
    Note right of I2S: ADC2 first (data-in only, GPIO25),<br/>then ADC1 (BCK=20, LRC=21, MCLK=22, DOUT=23).<br/>Creates audio_pipeline_task on Core 1.<br/>Creates audioTaskPausedAck semaphore.

    setup->>DAC: dac_secondary_init()
    Note right of DAC: ES8311 codec on I2S2 TX.<br/>Registers as HAL_DISC_BUILTIN.<br/>Calls audio_pipeline_set_sink(<br/>  SINK_SLOT_ES8311, &sink).

    setup->>Bridge: hal_pipeline_sync()
    Note right of Bridge: 1. Initialize mapping tables to -1<br/>2. Register stateChangeCb with manager<br/>3. Scan AVAILABLE devices:<br/>   - DAC/CODEC -> _halSlotToSinkSlot[]<br/>   - ADC -> _halSlotToAdcLane[] + adcEnabled[]<br/>4. Log: "N output(s), M input(s)"

    setup->>HAL: Register BUILTIN peripherals
    Note right of HAL: NS4150B (GPIO53), TempSensor,<br/>Display, Encoder, Buzzer,<br/>LED, Relay, Button, SignalGen

    setup->>DAC: dac_output_init()
    Note right of DAC: PCM5102A I2S TX on I2S0.<br/>HalDacAdapter wraps DacDriver.<br/>Calls audio_pipeline_set_sink(<br/>  SINK_SLOT_PRIMARY, &sink).<br/>Bridge callback fires automatically.

    Note over setup: Main loop begins:<br/>app_events_wait(5) replaces delay(5)
```

---

## Event-Driven Architecture

```mermaid
sequenceDiagram
    participant Producer as Producer<br/>(any task/ISR)
    participant AS as AppState<br/>(dirty flags)
    participant EG as FreeRTOS<br/>Event Group<br/>(24 usable bits)
    participant Loop as Main Loop<br/>(Core 1)
    participant WS as WebSocket<br/>Server (port 81)
    participant MQTT as mqtt_task<br/>(Core 0, 20Hz)

    Note over EG: EVT_ANY = 0x00FFFFFF<br/>16 bits assigned, 8 spare<br/>Bits 24-31 reserved by FreeRTOS

    Producer->>AS: appState.markXxxDirty()
    AS->>AS: _xxxDirty = true (volatile)
    AS->>EG: app_events_signal(EVT_XXX)
    Note right of EG: xEventGroupSetBits()<br/>from any core

    Loop->>EG: app_events_wait(5ms)
    Note right of Loop: Wakes in <1us on any bit<br/>OR 5ms timeout if idle

    EG-->>Loop: returns set bits (pdTRUE clears)

    Loop->>AS: isHalDeviceDirty()?
    AS-->>Loop: true

    Loop->>WS: sendHalDeviceState()
    Loop->>WS: sendAudioChannelMap()
    Loop->>AS: clearHalDeviceDirty()

    Note over MQTT: Independent consumer:<br/>polls dirty flags at 20Hz<br/>does NOT use event group<br/>(avoids fan-out race)

    MQTT->>AS: check dirty flags
    AS-->>MQTT: publish if dirty
    MQTT->>AS: clear MQTT-specific flags
```

---

## Sink Dispatch

```mermaid
flowchart LR
    subgraph matrix["Post-Matrix Output"]
        OUT["_outCh[0..7]\n8 mono float32 channels\n256 frames each"]
    end

    subgraph dispatch["Slot-Indexed Dispatch Loop"]
        direction TB
        LOOP["for slot = 0..7"]
        CHK1{"_sinks[slot].write\n!= NULL?"}
        CHK2{"isReady()?"}
        CHK3{"!muted?"}
        GAIN{"gainLinear\n!= 1.0?"}
        APPLY["Apply gain +\nclamp to [-1,1]"]
        CONV["float32 -> int32\nleft-justified (<<8)"]
        WRITE["sink->write(\n  buf, frames\n)"]
        SKIP["Skip slot\n(no dispatch)"]

        LOOP --> CHK1
        CHK1 -->|"NULL"| SKIP
        CHK1 -->|"valid"| CHK2
        CHK2 -->|"false"| SKIP
        CHK2 -->|"true"| CHK3
        CHK3 -->|"muted"| SKIP
        CHK3 -->|"active"| GAIN
        GAIN -->|"yes"| APPLY --> CONV
        GAIN -->|"unity"| CONV
        CONV --> WRITE
    end

    subgraph sinks["Sink Slots (AudioOutputSink[8])"]
        S0["Slot 0: PCM5102A\nfirstChannel=0, chCount=2\nhalSlot=0"]
        S1["Slot 1: ES8311\nfirstChannel=2, chCount=2\nhalSlot=1"]
        S2["Slot 2..7: (empty)\nwrite=NULL"]
    end

    subgraph hw["Hardware"]
        I2S0["I2S0 TX\nGPIO 24"]
        I2S2["I2S2 TX\nES8311 codec"]
    end

    OUT -->|"channels 0,1"| S0
    OUT -->|"channels 2,3"| S1
    S0 --> LOOP
    S1 --> LOOP
    WRITE -->|"slot 0"| I2S0
    WRITE -->|"slot 1"| I2S2
```

---

## E2E Test Infrastructure

```mermaid
graph TB
    classDef playwright fill:#1a3a5c,stroke:#2196F3,color:#fff
    classDef mock fill:#2d5016,stroke:#4CAF50,color:#fff
    classDef frontend fill:#5c1a3a,stroke:#E91E63,color:#fff
    classDef fixture fill:#5c4a00,stroke:#FF9800,color:#fff
    classDef schema fill:#3a3a3a,stroke:#9E9E9E,color:#fff

    subgraph PlaywrightRunner["Playwright Test Runner (Chromium)"]
        SPECS["19 spec files\n26 tests total"]
        FIX["connectedPage fixture\nsession cookie + WS mock"]
        RWS["page.routeWebSocket(/.*:81/)\nintercepts browser WS"]
        SEL["selectors.js\nreusable DOM queries"]
    end

    subgraph MockServer["Express Mock Server (port 3000)"]
        SRV["server.js\nExpress app"]
        ASM["assembler.js\nHTML from web_src/\n(replicates build_web_assets.js)"]
        ROUTES["12 route files\nauth, hal, wifi, mqtt,\nsettings, ota, pipeline,\ndsp, sensing, siggen,\ndiagnostics, system"]
        STATE["ws-state.js\ndeterministic state singleton\nreset between tests"]
    end

    subgraph Fixtures["Test Fixtures (hand-crafted)"]
        WSF["ws-messages/\n~15 JSON files\none per broadcast type"]
        APIF["api-responses/\n~12 JSON files\none per REST endpoint"]
    end

    subgraph Frontend["Real Frontend (web_src/)"]
        HTML["index.html\nHTML shell"]
        CSS["css/01-05-*.css\n5 stylesheet files"]
        JS["js/01-28-*.js\n24 JS modules\n(concatenated, global scope)"]
    end

    subgraph Schemas["JSON Schema (docs/api/schemas/)"]
        SCH["~17 schema files\nJSON Schema 2020-12\nvalidated by ajv"]
    end

    SPECS -->|"uses"| FIX
    FIX -->|"intercepts WS\nat browser level"| RWS
    FIX -->|"navigates to\nlocalhost:3000"| SRV
    SPECS -->|"queries DOM"| SEL

    ASM -->|"reads & assembles"| HTML
    ASM -->|"concatenates"| CSS
    ASM -->|"concatenates"| JS
    SRV -->|"GET /"| ASM
    SRV -->|"/api/*"| ROUTES

    WSF -->|"initial state\nbroadcasts"| RWS
    APIF -->|"REST responses"| ROUTES
    STATE -->|"current state"| ROUTES

    SCH -->|"validates outbound"| WSF
    SCH -->|"validates responses"| APIF

    class SPECS,FIX,RWS,SEL playwright
    class SRV,ASM,ROUTES,STATE mock
    class HTML,CSS,JS frontend
    class WSF,APIF fixture
    class SCH schema
```

---

## CI Quality Gates

```mermaid
graph LR
    classDef trigger fill:#1a3a5c,stroke:#2196F3,color:#fff
    classDef gate fill:#2d5016,stroke:#4CAF50,color:#fff
    classDef build fill:#5c4a00,stroke:#FF9800,color:#fff
    classDef release fill:#5c1a3a,stroke:#E91E63,color:#fff

    PUSH["Push / PR\nto main or develop"]

    subgraph Gates["Quality Gates (parallel)"]
        direction TB
        CPP["cpp-tests\npio test -e native -v\n1,556 Unity tests"]
        CPPL["cpp-lint\ncppcheck src/\nwarning + style + performance"]
        JSL["js-lint\nfind_dups.js\ncheck_missing_fns.js\nESLint web_src/js/"]
        E2E["e2e-tests\nnpm ci\nplaywright install chromium\nplaywright test\n26 browser tests"]
    end

    BUILD["build\npio run -e esp32-p4\nfirmware.bin artifact"]

    REL["release\nversion bump\nSHA256 checksum\nGitHub Release"]

    PUSH --> CPP
    PUSH --> CPPL
    PUSH --> JSL
    PUSH --> E2E

    CPP --> BUILD
    CPPL --> BUILD
    JSL --> BUILD
    E2E --> BUILD

    BUILD --> REL

    class PUSH trigger
    class CPP,CPPL,JSL,E2E gate
    class BUILD build
    class REL release
```

---

## E2E Test Flow

```mermaid
sequenceDiagram
    participant PW as Playwright<br/>Test Runner
    participant FIX as connectedPage<br/>Fixture
    participant BR as Chromium<br/>Browser
    participant EXP as Express<br/>Mock Server<br/>(port 3000)
    participant ASM as assembler.js<br/>(web_src/)
    participant RWS as routeWebSocket<br/>(intercepts :81)

    Note over PW: Test spec starts

    PW->>FIX: setUp()
    FIX->>BR: context.addCookies()<br/>sessionId = test-session

    FIX->>RWS: page.routeWebSocket(/.*:81/)
    Note right of RWS: Intercepts WS<br/>at browser level<br/>(no port 81 binding)

    FIX->>BR: page.goto('http://localhost:3000/')
    BR->>EXP: GET /
    EXP->>ASM: assemble HTML
    ASM-->>EXP: index.html + CSS + JS
    EXP-->>BR: 200 OK (assembled page)

    Note over BR: Frontend JS executes<br/>initWebSocket() connects :81

    BR->>RWS: WebSocket connect :81
    RWS-->>BR: { type: "authRequired" }
    BR->>RWS: { type: "auth", sessionId: "test-session" }
    RWS-->>BR: { type: "authSuccess" }

    loop Initial State Broadcasts
        RWS-->>BR: wifiStatus, smartSensing,<br/>displayState, halDeviceState,<br/>audioChannelMap, dacState, ...
    end

    Note over BR: UI renders with mock state<br/>Status bar shows connected

    FIX->>PW: yield connectedPage

    Note over PW: Test assertions run

    PW->>BR: page.click('[data-tab="audio"]')
    BR->>EXP: GET /api/pipeline/matrix
    EXP-->>BR: 200 OK (fixture JSON)
    PW->>BR: expect(locator).toBeVisible()

    Note over PW: Test spec ends

    PW->>FIX: tearDown()
    FIX->>BR: close context
```

---

## Test Coverage Map

```mermaid
graph TB
    classDef unit fill:#2d5016,stroke:#4CAF50,color:#fff
    classDef e2e fill:#1a3a5c,stroke:#2196F3,color:#fff
    classDef static fill:#5c4a00,stroke:#FF9800,color:#fff
    classDef untested fill:#5c1a1a,stroke:#F44336,color:#fff
    classDef schema fill:#3a3a3a,stroke:#9E9E9E,color:#fff

    subgraph UnitTests["C++ Unit Tests (1,556 tests)"]
        UT_HAL["HAL Framework\n14 test modules\nlifecycle, bridge, drivers"]
        UT_PIPE["Audio Pipeline\n8 test modules\nsink dispatch, DSP, metering"]
        UT_NET["Networking\n5 test modules\nWiFi, MQTT, OTA, ETH"]
        UT_CORE["Core Handlers\n13 test modules\nauth, button, buzzer, settings"]
        UT_GUI["GUI\n3 test modules\nhome, input, navigation"]
    end

    subgraph E2ETests["Playwright E2E Tests (26 tests)"]
        E2E_AUTH["Auth Flow\nlogin, session, password"]
        E2E_NAV["Navigation\n8 tabs, responsive"]
        E2E_AUDIO["Audio Tab\ninputs, matrix, outputs, siggen, PEQ"]
        E2E_HAL["HAL Devices\nlist, toggle, delete, rescan"]
        E2E_CONFIG["Configuration\nwifi, mqtt, settings, OTA"]
        E2E_DEBUG["Debug & Stats\nconsole, filtering, hardware"]
    end

    subgraph StaticAnalysis["Static Analysis"]
        SA_ESLINT["ESLint\nno-undef, no-redeclare\neqeqeq on 24 JS files"]
        SA_CPPCHECK["cppcheck\nwarning, style, performance\non src/ (excl gui/)"]
        SA_DUPS["find_dups.js\nduplicate declarations"]
        SA_FNS["check_missing_fns.js\nundefined function refs"]
    end

    subgraph Schemas["Contract Testing"]
        CT_WS["WS JSON Schemas\n~17 message types\nvalidated by ajv"]
        CT_BIN["Binary Format Docs\nwaveform (0x01)\nspectrum (0x02)"]
    end

    subgraph NotTested["Not Covered (Hardware-Only)"]
        HW_I2S["I2S DMA streaming"]
        HW_GPIO["GPIO ISR / interrupts"]
        HW_RTOS["FreeRTOS multi-core\ntask scheduling"]
        HW_PSRAM["PSRAM allocation\nunder heap pressure"]
    end

    UT_HAL -.->|"tests"| HAL_B["HAL Device Manager\nlifecycle + callbacks"]
    UT_PIPE -.->|"tests"| PIPE_B["Audio Pipeline\nsink dispatch + DSP"]
    E2E_AUTH -.->|"tests"| WEB_B["Web UI\nDOM + REST + WS"]
    E2E_HAL -.->|"tests"| API_B["REST API contracts\nrequest/response format"]
    CT_WS -.->|"validates"| WS_B["WS Message Format\nJSON field contracts"]
    SA_ESLINT -.->|"checks"| JS_B["JavaScript\nsyntax + scope"]

    class UT_HAL,UT_PIPE,UT_NET,UT_CORE,UT_GUI unit
    class E2E_AUTH,E2E_NAV,E2E_AUDIO,E2E_HAL,E2E_CONFIG,E2E_DEBUG e2e
    class SA_ESLINT,SA_CPPCHECK,SA_DUPS,SA_FNS static
    class CT_WS,CT_BIN schema
    class HW_I2S,HW_GPIO,HW_RTOS,HW_PSRAM untested
```
