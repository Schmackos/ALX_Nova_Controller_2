# ALX Audio Controller - User Manual

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Web Interface Guide](#web-interface-guide)
4. [Smart Sensing Modes](#smart-sensing-modes)
5. [WiFi Configuration](#wifi-configuration)
6. [MQTT & Home Assistant](#mqtt--home-assistant)
7. [Firmware Updates](#firmware-updates)
8. [Button Controls](#button-controls)
9. [Settings & Preferences](#settings--preferences)
10. [Troubleshooting](#troubleshooting)
11. [Technical Specifications](#technical-specifications)

---

## Introduction

The ALX Audio Controller is an ESP32-based smart amplifier controller designed for automated audio system management. It features:

- **Smart Sensing**: Automatically detects audio signal and controls amplifier power
- **Web Interface**: Full control via mobile-friendly web dashboard
- **MQTT Support**: Integration with Home Assistant and other smart home platforms
- **OTA Updates**: Automatic firmware updates over WiFi
- **Access Point Mode**: Direct connection without existing WiFi network

---

## Getting Started

### Initial Setup

1. **Power On**: Connect the controller to power. The device will start in Access Point (AP) mode if no WiFi is configured.

2. **Connect to AP**: 
   - Look for a WiFi network named `ALX-XXXXXXXXXXXX` (where X is your device's unique ID)
   - Default password: `12345678`

3. **Open Web Interface**: 
   - Navigate to `http://192.168.4.1` in your browser
   - Configure your home WiFi network credentials

4. **Connect to Home Network**:
   - Enter your WiFi SSID and password
   - The device will restart and connect to your network
   - Find the new IP address in your router's DHCP client list

### Finding Your Device

After connecting to your home network, you can find the device by:
- Checking your router's connected devices list
- Using the serial monitor during boot (shows IP address)
- Accessing the AP mode again (double-click the button)

---

## Web Interface Guide

The web interface is organized into tabs for easy navigation:

### Control Tab
- **Amplifier Status**: Shows current ON/OFF state with visual indicator
- **Smart Sensing Mode**: Select between Always On, Always Off, or Smart Auto
- **Timer Display**: Shows countdown when auto-off timer is active
- **Timer Settings**: Configure auto-off duration (1-60 minutes)
- **Voltage Threshold**: Set detection sensitivity (0.1V - 3.3V)
- **Manual Controls**: Turn amplifier on/off manually

### WiFi Tab
- **Connection Status**: Current network info and signal strength
- **Network Selection**: Scan and connect to available networks
- **Access Point**: Enable/disable and configure the AP mode

### MQTT Tab
- **Connection Status**: MQTT broker connection state
- **Enable/Disable**: Toggle MQTT functionality
- **Broker Settings**: Configure server, port, and credentials
- **Home Assistant Discovery**: Enable auto-configuration for HA

### Settings Tab
- **Timezone**: Set your local timezone
- **Appearance**: Toggle between day/night mode
- **Firmware Update**: Check for and install updates
- **Backup/Restore**: Export and import device settings
- **Device Actions**: Reboot or factory reset

### Debug Tab
- **Hardware Stats**: CPU, memory, and storage usage
- **WiFi Info**: Signal strength, channel, connected clients
- **Performance Graphs**: Historical CPU and memory usage
- **Debug Console**: Real-time log messages

### Test Tab
- **LED Test**: Verify LED blinking functionality
- **System Status**: Check device operation

---

## Smart Sensing Modes

### Always On
The amplifier remains powered on continuously. Useful when:
- You want constant availability
- External control is preferred
- Power consumption is not a concern

### Always Off
The amplifier remains powered off. Useful for:
- Energy saving when not in use
- Manual-only control preference
- Temporary shutdown

### Smart Auto Sensing
The controller automatically manages amplifier power:

1. **Signal Detection**: Monitors the audio input voltage
2. **Auto Power On**: When voltage exceeds threshold, amplifier turns on
3. **Auto Power Off**: After signal drops below threshold for the timer duration, amplifier turns off
4. **Timer Reset**: Any new signal resets the auto-off countdown

**Configuration Tips**:
- **Voltage Threshold**: Start at 1.0V and adjust based on your audio source
  - Lower values = more sensitive (may trigger on noise)
  - Higher values = less sensitive (may miss quiet signals)
- **Timer Duration**: 15 minutes is a good default
  - Shorter for quick sessions
  - Longer for music with quiet passages

---

## WiFi Configuration

### Connecting to a Network

1. Go to the **WiFi** tab
2. Click the scan button (magnifying glass) to find networks
3. Select your network from the dropdown
4. Enter the password
5. Click **Connect**

### Access Point Mode

The device can create its own WiFi network for:
- Initial setup
- Direct connection without router
- Backup access if main WiFi fails

**Enable AP**: Toggle in WiFi tab or double-click the physical button

**Configure AP**:
- Custom SSID (default: ALX-{serial})
- Custom password (minimum 8 characters)

### Dual Mode
The device can simultaneously:
- Connect to your home WiFi (STA mode)
- Broadcast its own network (AP mode)

This allows access even if the main network details change.

---

## MQTT & Home Assistant

### MQTT Setup

1. Go to the **MQTT** tab
2. Enable MQTT toggle
3. Enter your broker details:
   - **Broker Address**: Your MQTT server (e.g., `192.168.1.100` or `mqtt.example.com`)
   - **Port**: Default is 1883 (or 8883 for TLS)
   - **Username/Password**: If authentication is required
   - **Base Topic**: Custom topic prefix (default: `ALX/{serial}`)
4. Click **Save MQTT Settings**

### Home Assistant Discovery

When enabled, the device automatically registers with Home Assistant:

1. Enable **Home Assistant Discovery** toggle
2. Save settings
3. The device appears in HA under **Settings > Devices & Services > MQTT**

### MQTT Topics

| Topic | Type | Description |
|-------|------|-------------|
| `{base}/state` | Publish | Current amplifier state (ON/OFF) |
| `{base}/set` | Subscribe | Control amplifier (ON/OFF) |
| `{base}/mode` | Publish | Current sensing mode |
| `{base}/mode/set` | Subscribe | Set sensing mode |
| `{base}/availability` | Publish | Online/offline status |

---

## Firmware Updates

### Automatic Updates

1. Go to **Settings** tab
2. Enable **Auto Update** toggle
3. Device checks for updates on boot and periodically
4. When update is available, it downloads and installs automatically

### Manual Updates

1. Click **Check for Updates**
2. If available, click **Update Now**
3. Watch the progress bar
4. Device reboots automatically after completion

### Manual Firmware Upload

For offline or custom firmware:
1. Drag and drop `.bin` file to the upload area
2. Or click to browse and select file
3. Upload progress shows in the progress bar

### SSL Validation

- **Enabled** (recommended): Verifies GitHub's SSL certificate
- **Disabled**: Skip certificate validation (use only if having connection issues)

---

## Button Controls

The physical button on the device provides quick access to common functions:

| Action | Duration | Function |
|--------|----------|----------|
| Short Press | < 0.5 seconds | Print status info to serial |
| Double Click | 2 quick presses | Toggle Access Point mode |
| Triple Click | 3 quick presses | Toggle LED blinking |
| Long Press | 2 seconds | Restart ESP32 |
| Very Long Press | 10 seconds | Reboot ESP32 |

**Visual Feedback**:
- LED blinks faster during long press to indicate progress
- LED pattern changes based on device state

---

## Settings & Preferences

### Timezone
Set your local timezone for accurate timestamps in:
- Debug console
- Log messages
- Scheduled operations

### Night Mode
Toggle between light and dark themes:
- **Day Mode**: Light background, easy visibility in bright conditions
- **Night Mode**: Dark background, reduces eye strain in low light

### Backup & Restore

**Export Settings**:
- Downloads a JSON file with all device settings
- Useful before firmware updates or factory reset
- Store safely for recovery

**Import Settings**:
- Upload previously exported settings file
- Device reboots to apply settings

### Factory Reset

**Warning**: This erases all settings!

Resets:
- WiFi credentials
- MQTT configuration
- Smart sensing settings
- All preferences

The device returns to initial AP mode after reset.

---

## Troubleshooting

### Cannot Connect to Device

1. **Check AP Mode**: Double-click button to enable AP
2. **Find IP Address**: Check router's DHCP list
3. **Same Network**: Ensure your phone/computer is on same network
4. **Firewall**: Allow connections on ports 80 and 81

### WiFi Connection Fails

1. **Correct Password**: Double-check WiFi password
2. **2.4GHz Network**: ESP32 only supports 2.4GHz WiFi
3. **Signal Strength**: Move device closer to router
4. **Router Settings**: Disable MAC filtering or add device

### Smart Sensing Not Working

1. **Check Mode**: Ensure "Smart Auto" is selected
2. **Voltage Threshold**: Adjust sensitivity
3. **Audio Signal**: Verify source is producing signal
4. **Wiring**: Check voltage sense pin connection

### MQTT Not Connecting

1. **Broker Address**: Verify IP/hostname is correct
2. **Port**: Confirm port number (default 1883)
3. **Credentials**: Check username/password
4. **Firewall**: Ensure MQTT port is open
5. **Broker Status**: Verify broker is running

### Firmware Update Fails

1. **Internet Connection**: Verify WiFi is connected
2. **SSL Issues**: Try disabling SSL validation temporarily
3. **Server Status**: Check GitHub is accessible
4. **Manual Upload**: Use local firmware file instead

### Device Keeps Rebooting

1. **Power Supply**: Ensure adequate power (5V, 1A minimum)
2. **Firmware Issue**: Flash known-good firmware via USB
3. **Factory Reset**: Very long button press (10 seconds)

---

## Technical Specifications

### Hardware

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3 |
| WiFi | 802.11 b/g/n (2.4GHz) |
| Flash | 4MB minimum |
| Power Input | 5V DC |
| Relay Control | GPIO 4 |
| Voltage Sense | GPIO 1 (ADC) |
| Button | GPIO 15 |
| LED | GPIO 2 |

### Software

| Feature | Details |
|---------|---------|
| Web Server | Port 80 |
| WebSocket | Port 81 |
| MQTT | Configurable port |
| OTA Updates | HTTPS from GitHub |
| File System | SPIFFS |

### Network

| Protocol | Purpose |
|----------|---------|
| HTTP | Web interface |
| WebSocket | Real-time updates |
| MQTT | Smart home integration |
| HTTPS | Firmware updates |
| NTP | Time synchronization |

---

## Support

For additional help:

- **GitHub Issues**: Report bugs and feature requests
- **Documentation**: Check the repository for latest guides
- **Firmware Updates**: Enable auto-update for latest features

---

*ALX Audio Controller - Making your audio smart*
