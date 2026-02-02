# ALX Nova Controller

![Tests](https://github.com/Schmackos/ALX_Nova_Controller_2/actions/workflows/tests.yml/badge.svg)
![Version](https://img.shields.io/badge/version-1.2.4-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green)

ESP32-S3 based intelligent amplifier controller with smart auto-sensing, WiFi management, MQTT integration, and OTA updates.

## Features

### Smart Auto Sensing
- Automatic voltage detection and amplifier control
- Configurable auto-off timer (1-60 minutes)
- Adjustable voltage threshold (0.1-3.3V)
- Three modes: Always On, Always Off, Smart Auto

### Connectivity
- WiFi client and AP modes
- Web-based configuration interface
- WebSocket real-time updates
- MQTT integration with Home Assistant discovery
- OTA firmware updates

### Management
- Settings persistence and export/import
- Factory reset capability
- Hardware statistics monitoring
- Release notes for updates

## Quick Start

### Hardware Requirements
- ESP32-S3 DevKitM-1
- Power supply (5V USB)
- Amplifier control circuit (relay on GPIO 4)
- Voltage sensing circuit (GPIO 1)

### Building and Uploading

```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### First-Time Setup

1. Connect ESP32 to power
2. Device creates WiFi AP: `ALX-XXXXXXXXXXXX`
3. Connect to AP (password: `alxaudio2024`)
4. Navigate to `http://192.168.4.1`
5. Configure WiFi credentials
6. Device connects to your network

## Testing

![Test Coverage](https://img.shields.io/badge/tests-23%20passing-brightgreen)

### Run Tests

```bash
# Requires MinGW/gcc on Windows
pio test -e native

# Or use GitHub Actions (automatic on push)
```

### Test Coverage
- ✅ Smart sensing timer logic (10 tests)
- ✅ HTTP API endpoints (13 tests)
- ⚠️ WiFi/MQTT/OTA (planned)

See [test/README.md](test/README.md) for details.

## API Endpoints

### Smart Sensing
- `GET /api/smartsensing` - Get current state
- `POST /api/smartsensing` - Update settings

### WiFi
- `GET /api/wifistatus` - Connection status
- `GET /api/wifiscan` - Available networks
- `POST /api/wificonfig` - Configure WiFi

### System
- `GET /api/settings` - Get all settings
- `POST /api/settings` - Update settings
- `GET /api/settings/export` - Export config
- `POST /api/settings/import` - Import config
- `POST /api/factoryreset` - Reset to defaults
- `POST /api/reboot` - Restart device

### OTA Updates
- `GET /api/checkupdate` - Check for updates
- `POST /api/startupdate` - Begin OTA update
- `GET /api/updatestatus` - Update progress

## Configuration

### Pin Assignments
- LED: GPIO 2 (internal)
- Reset Button: GPIO 15
- Amplifier Control: GPIO 4
- Voltage Sensing: GPIO 1 (ADC)

### Default Settings
- AP SSID: Device serial number
- AP Password: `alxaudio2024`
- Auto-off Timer: 5 minutes
- Voltage Threshold: 0.5V

## Development

### Project Structure
```
src/
├── main.cpp              # Main application
├── app_state.h/cpp       # Global state management
├── smart_sensing.h/cpp   # Smart sensing logic
├── wifi_manager.h/cpp    # WiFi functionality
├── mqtt_handler.h/cpp    # MQTT integration
├── ota_updater.h/cpp     # OTA updates
├── websocket_handler.h   # WebSocket server
├── settings_manager.h    # Settings persistence
└── web_pages.h           # Web interface

test/
├── test_smart_sensing/   # Smart sensing tests
├── test_api/             # API endpoint tests
└── test_mocks/           # Arduino mocks
```

### Contributing

1. Create feature branch
2. Make changes
3. Ensure tests pass: `pio test -e native`
4. Build firmware: `pio run`
5. Create pull request

Tests run automatically on push via GitHub Actions.

### Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

## Version History

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for detailed changelog.

### Latest: v1.2.4
- Fixed smart auto sensing timer countdown logic
- Timer now correctly stays at full value when voltage detected
- Timer only counts down when no voltage present

## License

Copyright © 2024-2026 ALX Audio

## Support

- Issues: https://github.com/Schmackos/ALX_Nova_Controller_2/issues
- Documentation: See [docs/](docs/) (if available)

## Acknowledgments

Built with:
- [PlatformIO](https://platformio.org/)
- [Arduino Framework](https://www.arduino.cc/)
- [ArduinoJson](https://arduinojson.org/)
- [WebSockets](https://github.com/Links2004/arduinoWebSockets)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
