# HAL Architecture — Mermaid Diagrams
_Extracted from hal-implementation.md — open in VS Code with Mermaid Preview extension_

---

## 1. HAL System Architecture

```mermaid
graph TB
    subgraph ESP32-P4["ESP32-P4 Motherboard"]
        direction TB

        subgraph HAL["HAL Device Manager (Singleton)"]
            direction LR
            DM["HalDeviceManager<br/>registerDevice()<br/>initAll() priority-sorted<br/>healthCheckAll() 30s<br/>forEach() iterator"]
            DR["HalDriverRegistry<br/>compatible → factory<br/>legacy DAC_ID → factory<br/>max 16 entries"]
            PC["Pin Claim Tracker<br/>claimPin() / releasePin()<br/>HAL_MAX_PINS=56"]
        end

        subgraph BUSES["Bus Abstraction (HalBusRef)"]
            I2C0["I2C Bus 0 (EXT)<br/>GPIO48 SDA / GPIO54 SCL<br/>⚠ SDIO conflict"]
            I2C1["I2C Bus 1 (ONBOARD)<br/>GPIO7 SDA / GPIO8 SCL<br/>ES8311 dedicated"]
            I2C2["I2C Bus 2 (EXPANSION)<br/>GPIO28 SDA / GPIO29 SCL<br/>✓ Always safe"]
            I2S0["I2S0 Full-Duplex<br/>BCK=20 WS=21 MCLK=22<br/>RX=23 TX=24"]
            I2S2["I2S2 (ES8311)<br/>MCLK=13 BCLK=12<br/>WS=10 DIN=9 DOUT=11"]
            GPIO_BUS["GPIO Bus<br/>Direct pin control"]
            INT_BUS["Internal Bus<br/>On-chip peripherals"]
        end

        subgraph DEVICES["Registered HAL Devices (max 8)"]
            DEV_PCM5102["Slot 0: PCM5102A<br/>HAL_DEV_DAC<br/>ti,pcm5102a<br/>I2S0 TX, no I2C<br/>BUILTIN"]
            DEV_ES8311["Slot 1: ES8311<br/>HAL_DEV_CODEC<br/>evergrande,es8311<br/>I2C 0x18 + I2S2<br/>BUILTIN"]
            DEV_ADC1["Slot 2: PCM1808 #1<br/>HAL_DEV_ADC<br/>ti,pcm1808<br/>I2S0 RX<br/>BUILTIN"]
            DEV_ADC2["Slot 3: PCM1808 #2<br/>HAL_DEV_ADC<br/>ti,pcm1808<br/>I2S1 RX<br/>BUILTIN"]
            DEV_NS4150["Slot 4: NS4150B<br/>HAL_DEV_AMP<br/>ns,ns4150b-amp<br/>GPIO53<br/>BUILTIN"]
            DEV_TEMP["Slot 5: Temp Sensor<br/>HAL_DEV_SENSOR<br/>espressif,esp32p4-temp<br/>Internal<br/>BUILTIN"]
            DEV_EXT["Slot 6-7: External<br/>HAL_DEV_???<br/>EEPROM / ONLINE / MANUAL"]
        end

        subgraph CONSUMERS["State Consumers"]
            PIPE["Audio Pipeline<br/>Core 1, priority 3<br/>reads _ready directly<br/>NO virtual dispatch"]
            GUI["GUI Task<br/>Core 0<br/>reads _state at 1s poll<br/>scr_devices + scr_home"]
            WS["WebSocket Handler<br/>sendHalDeviceState()<br/>halDeviceState JSON<br/>INIT_HAL_DEVICE bit 14"]
            MQTT["MQTT Handler<br/>HA discovery per device<br/>re-publish on HA restart"]
            REST["REST API<br/>/api/hal/devices<br/>/api/hal/scan<br/>/api/hal/db"]
        end
    end

    subgraph EXTERNAL["External Systems"]
        EEPROM["AT24C02 EEPROM<br/>I2C 0x50-0x57<br/>v1/v2/v3 format"]
        GITHUB["GitHub Raw YAML<br/>raw.githubusercontent.com<br/>ETag caching in NVS<br/>TLS + setInsecure()"]
        WEBUI["Web Browser<br/>Devices tab<br/>YAML import/export<br/>Manual config modal"]
        HA["Home Assistant<br/>MQTT auto-discovery<br/>homeassistant/status"]
        TFT["ST7735S TFT<br/>Devices carousel card<br/>Status dots"]
    end

    HAL --> BUSES
    HAL --> DEVICES
    DEVICES --> CONSUMERS

    I2C0 --> EEPROM
    I2C1 --> DEV_ES8311
    I2C2 -.->|"Future add-ons"| DEV_EXT
    I2S0 --> DEV_PCM5102
    I2S0 --> DEV_ADC1
    I2S2 --> DEV_ES8311
    GPIO_BUS --> DEV_NS4150
    INT_BUS --> DEV_TEMP

    WS --> WEBUI
    MQTT --> HA
    GUI --> TFT
    REST --> WEBUI
    GITHUB -.->|"Tier 2 fetch"| HAL

    style HAL fill:#1a1a2e,stroke:#e94560,color:#fff
    style BUSES fill:#16213e,stroke:#0f3460,color:#fff
    style DEVICES fill:#0f3460,stroke:#533483,color:#fff
    style CONSUMERS fill:#1a1a2e,stroke:#e94560,color:#fff
    style EXTERNAL fill:#16213e,stroke:#0f3460,color:#fff
```

---

## 2. Device Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> UNKNOWN : Device slot allocated

    UNKNOWN --> DETECTED : I2C ACK or\nEEPROM found
    UNKNOWN --> MANUAL : User adds via\nweb UI / YAML

    DETECTED --> CONFIGURING : Driver found\n(local DB or online)
    DETECTED --> MANUAL : No driver found\n(404 from GitHub)

    MANUAL --> CONFIGURING : User saves config\nor rescan finds driver

    CONFIGURING --> AVAILABLE : probe() + init()\nboth succeed\n_ready=true
    CONFIGURING --> ERROR : init() fails\n_ready=false

    AVAILABLE --> UNAVAILABLE : healthCheck() fails\n(I2C timeout)\n_ready=false
    UNAVAILABLE --> AVAILABLE : healthCheck() recovers\n_ready=true
    UNAVAILABLE --> ERROR : 3 consecutive\nhealthCheck failures

    ERROR --> CONFIGURING : User triggers rescan\nor manual reconfigure

    AVAILABLE --> REMOVED : User deletes or\ndevice absent on rescan\n_ready=false
    UNAVAILABLE --> REMOVED : User deletes
    ERROR --> REMOVED : User deletes
    MANUAL --> REMOVED : User deletes
    DETECTED --> REMOVED : User deletes

    REMOVED --> [*]

    note right of AVAILABLE
        Audio pipeline reads
        _ready directly (volatile bool)
        No virtual dispatch
    end note

    note right of CONFIGURING
        probe(): I2C ACK + chip ID
        init(): full HW setup
        dumpConfig(): LOG_I output
    end note
```

---

## 3. Three-Tier Device Discovery Flow

```mermaid
flowchart TD
    START([hal_discover_devices called]) --> BUILTINS

    subgraph BUILTINS["Phase 1: Builtin Registration"]
        B1[PCM5102A → Slot 0<br/>HAL_DISC_BUILTIN]
        B2[ES8311 → Slot 1<br/>HAL_DISC_BUILTIN, P4 only]
        B3[PCM1808 ADC1 → Slot 2]
        B4[PCM1808 ADC2 → Slot 3]
        B5[NS4150B Amp → Slot 4]
        B6[Temp Sensor → Slot 5]
    end

    BUILTINS --> PROBE_BUILTINS
    PROBE_BUILTINS["probe() each builtin<br/>ES8311: I2C ACK 0x18 + reg 0xFD=0x83<br/>PCM5102A: immediate true (no I2C)<br/>Temp: always true"] --> I2C_SCAN

    subgraph I2C_SCAN["Phase 2: I2C Bus Scan"]
        WIFI_CHECK{WiFi active?}
        WIFI_CHECK -->|Yes| SKIP_EXT[Skip HAL_I2C_BUS_EXT<br/>GPIO48/54 SDIO conflict]
        WIFI_CHECK -->|No| SCAN_EXT[Scan Bus 0<br/>GPIO48/54<br/>Addr 0x08-0x77]
        SKIP_EXT --> SCAN_EXP
        SCAN_EXT --> SCAN_EXP
        SCAN_EXP[Scan Bus 2<br/>GPIO28/29<br/>Addr 0x08-0x77<br/>Always safe]
    end

    I2C_SCAN --> EEPROM_CHECK

    subgraph EEPROM_CHECK["Phase 3: EEPROM Probe (0x50-0x57)"]
        EEPROM_FOUND{EEPROM ACK?}
        EEPROM_FOUND -->|Yes| READ_EEPROM[Read 256 bytes<br/>Parse v1/v2/v3]
        EEPROM_FOUND -->|No| NO_EEPROM[No external device<br/>builtins only]
        READ_EEPROM --> HAS_COMPAT{Has compatible\nstring? v3}
        HAS_COMPAT -->|Yes, v3| TIER1
        HAS_COMPAT -->|No, v1/v2| LEGACY[Match by legacy<br/>DAC_ID_* constant]
        LEGACY --> AVAILABLE_STATE
    end

    subgraph TIER1["Tier 1: Local DB (LittleFS)"]
        DB_LOOKUP{hal_db_lookup<br/>compatible?}
        DB_LOOKUP -->|Found| AVAILABLE_STATE[Create driver<br/>HAL_STATE_AVAILABLE<br/>_ready = true]
        DB_LOOKUP -->|Not found| TIER2
    end

    subgraph TIER2["Tier 2: GitHub Raw YAML"]
        AUDIO_IDLE{Audio pipeline\nidle? ~44KB SRAM\nfor TLS}
        AUDIO_IDLE -->|Yes + WiFi| FETCH["hal_online_fetch(compatible)<br/>GET raw.githubusercontent.com<br/>ETag: If-None-Match"]
        AUDIO_IDLE -->|No or no WiFi| DISCOVERED_STATE
        FETCH --> HTTP_CODE{HTTP Response}
        HTTP_CODE -->|200 OK| CACHE[Cache in hal_db<br/>+ NVS ETag]
        HTTP_CODE -->|304| USE_CACHED[Use NVS cached copy]
        HTTP_CODE -->|404| DISCOVERED_STATE[HAL_STATE_DISCOVERED<br/>Unknown device]
        HTTP_CODE -->|429| RATE_LIMIT[Log warning<br/>HAL_STATE_DISCOVERED]
        CACHE --> AVAILABLE_STATE
        USE_CACHED --> AVAILABLE_STATE
    end

    subgraph MANUAL_PATH["Manual Config (Web UI)"]
        DISCOVERED_STATE --> USER_CONFIG[User opens Devices tab<br/>Selects preset or imports YAML<br/>Configures bus/address]
        USER_CONFIG --> MANUAL_SAVE[POST /api/hal/devices/slot/configure<br/>Saved to /hal_config.json]
        MANUAL_SAVE --> AVAILABLE_STATE
    end

    AVAILABLE_STATE --> DONE([Discovery complete<br/>Devices registered in HalDeviceManager<br/>sendHalDeviceState via WS])

    style TIER1 fill:#1a472a,stroke:#2d6a4f,color:#fff
    style TIER2 fill:#1a3a5c,stroke:#2171b5,color:#fff
    style MANUAL_PATH fill:#5c3a1a,stroke:#b56721,color:#fff
    style AVAILABLE_STATE fill:#1a472a,stroke:#2d6a4f,color:#fff
```

---

## 4. Boot Sequence (Phase 4 — Final)

```mermaid
sequenceDiagram
    participant M as main.cpp setup()
    participant HAL as HalDeviceManager
    participant REG as HalDriverRegistry
    participant DB as hal_device_db
    participant DAC as dac_output
    participant SEC as dac_secondary
    participant DISC as hal_discovery
    participant IO as io_registry
    participant PIPE as hal_pipeline_bridge

    M->>REG: hal_register_builtins()
    Note over REG: PCM5102A, ES8311, PCM1808x2,<br/>NS4150B, TempSensor

    M->>DB: hal_db_init()
    Note over DB: Load /hal_devices.json<br/>from LittleFS

    M->>DB: hal_load_device_configs()
    Note over DB: Restore per-device settings<br/>from /hal_config.json

    M->>DAC: dac_output_init()
    DAC->>HAL: registerDevice(pcm5102a_adapter)
    Note over HAL: Slot 0, HAL_DISC_BUILTIN<br/>_ready=true, priority=800

    M->>SEC: dac_secondary_init()
    SEC->>HAL: registerDevice(es8311_adapter)
    Note over HAL: Slot 1, HAL_DISC_BUILTIN<br/>probe(): I2C 0x18 + reg 0xFD=0x83

    M->>DISC: hal_discover_devices()
    Note over DISC: I2C scan Bus 0 (if no WiFi)<br/>I2C scan Bus 2 (always)<br/>EEPROM probe 0x50-0x57<br/>3-tier lookup for unknowns
    DISC->>HAL: registerDevice(external_device)
    Note over HAL: Slot 6-7 for add-ons

    M->>IO: io_registry_init()
    IO->>HAL: forEach() -> sync outputs/inputs

    M->>PIPE: hal_pipeline_sync()
    Note over PIPE: Register AudioOutputSink<br/>per AVAILABLE device<br/>Fallback: legacy dac_output_write()

    M->>HAL: initAll() -- priority-sorted
    Note over HAL: 1000: Buses<br/>900: NS4150B (GPIO)<br/>800: Codecs/DACs/ADCs<br/>600: Pipeline bridge<br/>100: Temp sensor
```

---

## 5. Phase Dependency Graph

```mermaid
graph LR
    P0["Phase 0<br/>Core HAL Framework<br/>━━━━━━━━━━━━<br/>hal_types.h<br/>hal_device.h<br/>hal_audio_device.h<br/>hal_device_manager<br/>hal_driver_registry<br/>━━━━━━━━━━━━<br/>14 tests<br/>Zero files modified"]

    P1["Phase 1<br/>Driver Migration<br/>━━━━━━━━━━━━<br/>hal_dac_adapter<br/>hal_builtin_devices<br/>━━━━━━━━━━━━<br/>Wraps DacDriver<br/>Registers builtins<br/>EVT_HAL_DEVICE bit 14<br/>━━━━━━━━━━━━<br/>9 tests"]

    P2["Phase 2<br/>Discovery & DB<br/>━━━━━━━━━━━━<br/>hal_device_db<br/>hal_discovery<br/>hal_eeprom_v3<br/>hal_api (REST)<br/>hal_online_fetch<br/>━━━━━━━━━━━━<br/>3-tier lookup<br/>GitHub YAML + ETag<br/>━━━━━━━━━━━━<br/>33 tests"]

    P3A["Phase 3A<br/>Web UI<br/>━━━━━━━━━━━━<br/>15-hal-devices.js<br/>15a-yaml-parser.js<br/>Devices tab<br/>YAML import/export<br/>EEPROM write"]

    P3B["Phase 3B<br/>TFT + Immediate Wins<br/>━━━━━━━━━━━━<br/>scr_devices.cpp<br/>hal_temp_sensor.cpp<br/>hal_ns4150b.cpp<br/>scr_home I/O cell<br/>Desktop carousel card"]

    P4["Phase 4<br/>Pipeline + Settings<br/>━━━━━━━━━━━━<br/>hal_pipeline_bridge<br/>hal_settings<br/>HA re-enrollment<br/>io_registry <- HAL<br/>━━━━━━━━━━━━<br/>10 tests"]

    MULTI["plans/<br/>multi-dac-routing.md<br/>Phase 2+<br/>━━━━━━━━━━━━<br/>Builds on HAL"]

    P0 --> P1
    P1 --> P2
    P2 --> P3A
    P2 --> P3B
    P3A --> P4
    P3B --> P4
    P4 --> MULTI

    style P0 fill:#1a472a,stroke:#2d6a4f,color:#fff
    style P1 fill:#1a3a5c,stroke:#2171b5,color:#fff
    style P2 fill:#5c3a1a,stroke:#b56721,color:#fff
    style P3A fill:#3a1a5c,stroke:#7b21b5,color:#fff
    style P3B fill:#3a1a5c,stroke:#7b21b5,color:#fff
    style P4 fill:#5c1a1a,stroke:#b52121,color:#fff
    style MULTI fill:#1a1a1a,stroke:#666,color:#aaa
```
