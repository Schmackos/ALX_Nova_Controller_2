---
title: User Flows Overview
sidebar_position: 1
description: Catalog of documented user flows with sequence diagrams showing how the ALX Nova Controller 2 handles key interactions.
---

# User Flows

This section documents the 10 most important user flows for the ALX Nova Controller 2 carrier board. Each flow is presented as a **Mermaid sequence diagram** showing the precise message flow between system components, accompanied by a step-by-step walkthrough, preconditions, postconditions, and error scenarios.

These diagrams serve as the authoritative reference for how each flow works end-to-end — from physical hardware interaction or web UI click through REST API, HAL framework, audio pipeline, and back to the user via WebSocket broadcasts.

## Flow Catalog

| # | Flow | Trigger | Category |
|---|------|---------|----------|
| 1 | [Mezzanine ADC Card Insertion](mezzanine-adc-insert) | Physical card insertion + scan | Hardware |
| 2 | [Mezzanine DAC Card Insertion](mezzanine-dac-insert) | Physical card insertion + scan | Hardware |
| 3 | [Mezzanine Card Removal](mezzanine-removal) | Web UI delete action | Hardware |
| 4 | [Manual Device Configuration](manual-configuration) | Web UI config form | Configuration |
| 5 | [Custom Device Creation](custom-device-creation) | Web UI creator modal | Configuration |
| 6 | [Device Enable/Disable Toggle](device-toggle) | Web UI checkbox | Hardware |
| 7 | [Device Reinit / Error Recovery](device-reinit) | Web UI reinit button | Recovery |
| 8 | [Audio Matrix Routing](matrix-routing) | Web UI matrix grid | Audio |
| 9 | [PEQ / DSP Configuration](dsp-peq-config) | Web UI PEQ overlay | Audio |
| 10 | [First Boot Experience](first-boot) | Power on (new device) | System |

## How to Read the Diagrams

All diagrams use the **Mermaid sequence diagram** format with consistent participant naming:

### Participants

| Participant | Description |
|-------------|-------------|
| **User** | Physical person interacting with hardware or web UI |
| **Web UI** | Browser-based configuration interface (`web_src/`) |
| **REST API** | HTTP server on port 80 (`src/main.cpp`, `src/hal/hal_api.cpp`) |
| **WebSocket** | Real-time state broadcast server on port 81 |
| **HAL Manager** | Device lifecycle manager (`src/hal/hal_device_manager.cpp`) |
| **HAL Discovery** | I2C scan + EEPROM probe engine (`src/hal/hal_discovery.cpp`) |
| **EEPROM** | Mezzanine board identification memory (I2C Bus 2) |
| **Pipeline Bridge** | Connects HAL devices to audio pipeline (`src/hal/hal_pipeline_bridge.cpp`) |
| **Audio Pipeline** | 8-lane input, 32x32 matrix, 16-slot sink engine (`src/audio_pipeline.cpp`) |
| **LittleFS** | On-flash filesystem for configuration persistence |
| **Main Loop** | Arduino `loop()` on Core 1 (`src/main.cpp`) |
| **Toggle Queue** | Deferred device toggle queue (`src/state/hal_coord_state.h`) |

### Arrow Conventions

| Arrow | Meaning |
|-------|---------|
| `A ->> B` (solid) | Synchronous call / request |
| `A -->> B` (dashed) | Asynchronous broadcast / callback |
| `Note over A,B` | Important aside (e.g., SDIO conflict, semaphore handshake) |
| `alt` / `else` | Conditional branches (success vs error) |
| `rect` | Phase grouping for multi-step flows |

### Related Documentation

- [HAL Device Lifecycle](../hal/device-lifecycle) — State machine diagram for device states
- [REST API Reference (HAL)](../api/rest-hal) — All HAL REST endpoints
- [Audio Pipeline](../audio-pipeline) — Pipeline architecture and matrix routing
- [DSP System](../dsp-system) — DSP engine and coefficient computation
- [WebSocket Protocol](../websocket) — WebSocket message types and formats
