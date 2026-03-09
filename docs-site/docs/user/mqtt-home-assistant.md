---
title: MQTT & Home Assistant
sidebar_position: 6
description: Integrating the ALX Nova with MQTT brokers and Home Assistant.
---

# MQTT & Home Assistant

ALX Nova publishes its state to any standard MQTT broker and includes full **Home Assistant auto-discovery** support. Once configured, all controllable features appear as native entities in Home Assistant — no manual YAML configuration needed.

---

## What You Will Need

- A running MQTT broker (e.g. Mosquitto, EMQX, or the Home Assistant MQTT add-on)
- The broker's IP address or hostname
- A username and password if your broker requires authentication
- The ALX Nova connected to the same network as your broker

:::tip
If you are using Home Assistant, the easiest path is to install the **Mosquitto broker** add-on from the HA Add-on Store and enable the MQTT integration. The broker then runs on the same machine as HA, so the broker address is simply your Home Assistant IP.
:::

---

## Configuring the MQTT Connection

1. Open the web interface and go to the **MQTT** tab.
2. Toggle **MQTT Enabled** to on.
3. Fill in the broker details:

| Field | Example | Notes |
|---|---|---|
| Broker address | `192.168.1.10` | IP or hostname of your MQTT broker |
| Port | `1883` | Default MQTT port (no TLS) |
| Username | `alxnova` | Leave empty if your broker allows anonymous access |
| Password | `••••••••` | Leave empty if not required |
| Base topic | `alx/nova` | Prefix for all MQTT topics (default is `ALX/{serial}`) |

4. Click **Save MQTT Settings**.
5. The connection status indicator turns green within a few seconds if the broker is reachable.

:::info
The controller attempts to reconnect to the broker every 5 seconds if the connection is lost. All state changes that occur while disconnected are published as a batch when the connection is restored.
:::

---

## Home Assistant Auto-Discovery

When auto-discovery is enabled, the ALX Nova announces itself to Home Assistant using the standard MQTT discovery protocol. Home Assistant picks up the announcement and creates entities automatically — no manual configuration file editing required.

### Enabling Auto-Discovery

1. On the MQTT tab, toggle **Home Assistant Discovery** to on.
2. Click **Save MQTT Settings**.
3. In Home Assistant, go to **Settings > Devices & Services > MQTT**.
4. The ALX Nova should appear as a new device within a few seconds.

:::tip
If the device does not appear, check that your HA MQTT integration is connected to the same broker. You can verify this in **Settings > Devices & Services > MQTT > Configure**.
:::

---

## Entities Created in Home Assistant

Once auto-discovery runs, the following entities are created under the ALX Nova device:

### Switches

| Entity | What It Controls |
|---|---|
| Amplifier | Turns the relay-controlled amplifier on or off |
| MQTT | Enable or disable MQTT entirely from HA |
| Access Point | Toggle the WiFi AP from HA |

### Select / Mode

| Entity | What It Controls |
|---|---|
| Sensing Mode | Choose between Always On, Always Off, and Smart Auto |

### Numbers / Sliders

| Entity | What It Controls |
|---|---|
| Timer Duration | Auto-off countdown in minutes (1–60) |
| Audio Threshold | Signal detection sensitivity in dBFS |
| Audio Update Rate | VU meter refresh rate in milliseconds |

### Sensors

| Entity | What It Reports |
|---|---|
| Amplifier State | Current on/off state |
| Timer Remaining | Seconds left on the auto-off countdown |
| Free Heap | Available internal RAM in bytes |
| Free PSRAM | Available PSRAM in bytes |
| Chip Temperature | ESP32-P4 internal temperature in °C |
| Uptime | Seconds since last boot |
| Firmware Version | Installed firmware version string |
| Audio Level (per lane) | Current dBFS reading per active ADC lane |
| Audio Health (per lane) | OK, WEAK, NO\_DATA, or CLIPPING per lane |
| MQTT Connection | Connected or disconnected |
| WiFi SSID | Currently connected network |
| IP Address | Current IP address |

### Buttons

| Entity | What It Does |
|---|---|
| Restart | Soft-reboots the controller |
| Check for OTA Update | Triggers an immediate firmware check |

---

## MQTT Topic Reference

All topics use the configured base topic as a prefix. With the default base topic `ALX/{serial}` the topics look like `ALX/A1B2C3D4E5F6/state`.

### Core State

| Topic | Direction | Payload |
|---|---|---|
| `{base}/state` | Publish | `ON` or `OFF` — amplifier relay state |
| `{base}/set` | Subscribe | `ON` or `OFF` — command to control the relay |
| `{base}/mode` | Publish | `always_on`, `always_off`, or `smart_auto` |
| `{base}/mode/set` | Subscribe | Set the sensing mode |
| `{base}/availability` | Publish | `online` or `offline` (LWT message) |

### Sensing and Timer

| Topic | Direction | Payload |
|---|---|---|
| `{base}/timer_duration` | Publish | Integer — timer duration in minutes |
| `{base}/timer_duration/set` | Subscribe | Set timer duration |
| `{base}/timer_remaining` | Publish | Integer — seconds remaining |
| `{base}/threshold` | Publish | Float — audio threshold in dBFS |
| `{base}/threshold/set` | Subscribe | Set threshold |

### Audio Levels (per lane)

Topics are published once per active ADC lane, numbered from 0:

| Topic | Payload |
|---|---|
| `{base}/audio/lane0/level` | Float — dBFS reading |
| `{base}/audio/lane0/health` | String — `OK`, `WEAK`, `NO_DATA`, `CLIPPING` |
| `{base}/audio/lane1/level` | Float — dBFS reading for lane 1 |
| ... | Additional lanes if more ADC devices are active |

### System

| Topic | Direction | Payload |
|---|---|---|
| `{base}/uptime` | Publish | Integer — seconds since boot |
| `{base}/free_heap` | Publish | Integer — bytes |
| `{base}/temperature` | Publish | Float — chip temperature °C |
| `{base}/firmware_version` | Publish | String — e.g. `1.12.1` |

:::info
A heartbeat publish occurs every 60 seconds even if no state has changed. This ensures HA does not mark entities as unavailable during quiet periods.
:::

---

## Example Home Assistant Automation

The entities work with standard HA automation triggers and conditions. Here is a plain-English example:

**"Turn on the amplifier when a Spotify media player starts playing, and turn it off 20 minutes after it pauses."**

You would create an automation with:
- Trigger: state of your Spotify media player changes to `playing`
- Action: turn on the ALX Nova Amplifier switch
- A second automation triggers when the player pauses, sets the Sensing Mode to Smart Auto, and lets the built-in timer handle the shutdown.

:::tip
Because ALX Nova's Smart Auto mode already handles the auto-off timer in firmware, you often do not need complex HA automations for power management — just turn it on from HA and let the controller handle the rest.
:::

---

## Troubleshooting MQTT

See the [Troubleshooting](./troubleshooting.md#mqtt-issues) page for a full FAQ. The most common causes of MQTT issues are:

- Wrong broker IP address or port
- Firewall blocking port 1883 between the controller and the broker host
- Incorrect username or password
- The MQTT integration in Home Assistant pointing at a different broker than the one the controller is using
