# Concept: Mezzanine Detection & Device Registry

| Field | Value |
|---|---|
| Workflow | `raw` |
| Priority | `---` |
| Effort | `---` |
| Sources | Mezzanine Detection Setup Methods.m4a, Carrier Board Mezzanine Setup online.m4a |
| Audio | [`inbox/processed/`](inbox/processed/) |
| Last updated | 2026-03-26 |

## Problem / Opportunity

The ALX Nova platform supports mezzanine expansion boards but lacks a unified detection-to-configuration pipeline and a central device registry. Without reliable auto-detection (EEPROM or resistor-based), users face complex manual setup that limits adoption. An online device registry hosted on the existing Docusaurus site would let manufacturers and community members publish board definitions, enabling the platform to fetch specs for newly detected boards automatically.

## Sub-topics

### Three-tier detection and firmware compilation model

#### What we know
- Three detection tiers exist: (1) EEPROM auto-detect (primary), (2) resistor-value identification using specific resistor combinations (fallback when no EEPROM), (3) manual configuration (expert users only)
- Auto-detection is critical for a frictionless setup experience
- All drivers (first-party and community) must be compiled into firmware — the ESP32-P4 does not support runtime driver loading
- Every mezzanine board needs a unique identification number for the system to work
- The existing Docusaurus site could serve as a device catalog displaying specs, components, and electrical diagrams
- Open question: whether device definitions (not drivers) could be fetched at runtime vs. always requiring firmware compilation

#### What needs research
- What data format should EEPROM identification use (manufacturer ID, board ID, revision, capability flags)?
- How many resistor-value combinations are feasible with the available ADC channels, and what resolution/tolerance is needed for reliable detection?
- What is the boundary between "device definition" (could be fetched online) and "device driver" (must be compiled)? Can runtime-fetched JSON definitions configure a generic driver without recompilation?
- How should unique board IDs be assigned and managed to prevent collisions between first-party and community boards?
- What EEPROM IC and capacity should be standardized for mezzanine boards (e.g., AT24C02, AT24C32)?

### Online registry and auto-detection flow

#### What we know
- Both EEPROM and resistor-value detection methods need a defined hardware-level and firmware-level specification
- An online component is needed — a registry/database hosted via the GitHub repository
- The ALX Nova platform should connect to the internet to fetch board data when it detects an unknown mezzanine
- An easy-to-use UI is needed for manufacturers and community members to register their boards
- The detection-to-configuration flow is: local detection (EEPROM/resistor) -> online lookup -> device configuration

#### What needs research
- What is the architecture for the online registry: static JSON files in GitHub (fetched raw), a GitHub Pages API, or a lightweight backend?
- How should the local-to-online lookup flow work: device detected -> ID extracted -> HTTP GET to registry -> receive board definition -> apply configuration?
- What data should the registry store per board (name, manufacturer, driver compatible string, pin mapping, I2C addresses, I2S config, power requirements, documentation URL)?
- How to handle offline scenarios: should the firmware cache previously fetched definitions in NVS/SPIFFS?
- What is the registration workflow for community members: GitHub PR to add a JSON entry, web form, or both?
- How to validate and prevent malicious or incorrect board definitions in the registry?
- What UI changes are needed on the ALX Nova web interface to show "new board detected, fetching info..." status and fallback to manual config?

## Action Items

- [ ] Design the EEPROM data format specification for mezzanine boards. Define the byte layout including manufacturer ID, board type ID, hardware revision, capability flags, and I2C address map. Consider compatibility with existing HAL discovery (`hal_discovery.h/.cpp`) and the `hal_eeprom_api.h/.cpp` EEPROM reading code already in the project. Document the spec at `docs-internal/backlog/research/mezzanine-eeprom-format.md`
- [ ] Design the resistor-value detection scheme for mezzanine boards without EEPROM. Determine which ESP32-P4 ADC channels are available on the mezzanine connector, calculate how many unique board IDs can be encoded with resistor divider combinations given ADC resolution and component tolerances (1%, 5%), and define the voltage-to-ID lookup table format. Document at `docs-internal/backlog/research/mezzanine-resistor-detection.md`
- [ ] Architect the online device registry system. Evaluate three approaches: (1) static JSON files in the GitHub repo served via GitHub Pages/raw URLs, (2) a structured JSON index auto-generated from individual board definition files, (3) a simple API endpoint. Define the board definition schema (JSON), the HTTP fetch flow from firmware, NVS caching for offline use, and the community submission workflow (GitHub PR template). Write the architecture document to `docs-internal/backlog/research/mezzanine-online-registry-architecture.md`
- [ ] Analyze the firmware boundary between compiled drivers and runtime-fetchable device definitions. Review the existing HAL device DB (`hal_device_db.h/.cpp`) and compatible-string driver registration (`HAL_REGISTER()` macro) to determine what device metadata could be separated into a runtime-loadable JSON definition (I2C addresses, pin mappings, display name, capability flags) vs. what must remain compiled (driver code, init sequences, register writes). Document findings and a recommended split at `docs-internal/backlog/research/mezzanine-driver-vs-definition-boundary.md`
- [ ] Design a unique board ID allocation scheme that prevents collisions between first-party ALX boards, third-party manufacturer boards, and community DIY boards. Consider a namespaced approach (e.g., 0x0001-0x00FF reserved for ALX, 0x0100-0x7FFF for registered manufacturers, 0x8000-0xFFFE for community). Define the registration process and write to `docs-internal/backlog/research/mezzanine-board-id-scheme.md`
- [ ] Create a Docusaurus device catalog page design for the docs site. Plan a `/devices` section that lists registered mezzanine boards with specs, component BOMs, electrical diagrams, compatible firmware versions, and driver installation status. Determine whether board definition JSON files can auto-generate catalog pages via a Docusaurus plugin or build script. Write the design to `docs-internal/backlog/research/mezzanine-device-catalog-design.md`

## Original Transcripts

<details>
<summary>Source: Mezzanine Detection Setup Methods.m4a</summary>

> We have two ways to auto detect add-on boards that are inserted into the mezzanine slots. The first is the EEPROM approach. The second one is using specific resistor values, a combination of the resistor values. When there is no EEPROM present. The third way is to just set it up manually but that requires more expert users in order to set all the values in the right way for the add-on to properly work. The auto detect is very important to make the set up basically a breeze. Now the drivers that are created either by the ALX platform team or the community. I believe they need to be integrated into the firmware otherwise these cannot be loaded and identified properly. So every add-on board that will ever be created in the future needs their unique identification number in order for the system to properly work. How should we design this in the right way and how can we link this to an online website in this case we could use maybe the docuSaurus that we already have to display the devices that are available with all their specifics components and electrical diagrams etc etc so that it's a full service platform that has everything in place. The other question that I have is is there a need to connect online to get this information or does it always needs to be compiled into the firmware when new devices become available.

</details>

<details>
<summary>Source: Carrier Board Mezzanine Setup online.m4a</summary>

> On the carrier board platform we use two ways for either manufacturers and community members when creating mezzanine plug-in boards to identify auto detection. The first way is using an EEPROM and EEPROM and the second way is using resistor values. Can you create a plan on how this should work and how should we design the online component to this? Because we have our get up repository where we can of course manage all the data but how do we create an easy to use user interface and have the ALX Nova platform connect through the internet to grab the required data when it detects a specific board.

</details>
