# GitHub Release Checklist for OTA Updates

## Quick Guide for Creating Releases

Follow these steps when creating a new firmware release to ensure proper OTA updates:

---

## Step 1: Build Firmware

```bash
platformio run
```

Your firmware will be at: `.pio/build/esp32-s3-devkitm-1/firmware.bin`

---

## Step 2: Calculate SHA256 Checksum

### Windows (PowerShell)
```powershell
Get-FileHash .pio/build/esp32-s3-devkitm-1/firmware.bin -Algorithm SHA256
```

### Linux/Mac
```bash
sha256sum .pio/build/esp32-s3-devkitm-1/firmware.bin
```

### Python (Cross-Platform)
```bash
python -c "import hashlib; print(hashlib.sha256(open('.pio/build/esp32-s3-devkitm-1/firmware.bin','rb').read()).hexdigest())"
```

**Copy the hash output** - you'll need it for the release notes.

---

## Step 3: Create GitHub Release

1. Go to your repository on GitHub
2. Click "Releases" → "Draft a new release"
3. **Tag version:** Use semantic versioning (e.g., `1.1.6`, `2.0.0`)
4. **Release title:** Same as tag or descriptive name
5. **Description:** Use this template:

```markdown
## What's New
- Feature A: Description
- Feature B: Description
- Bug fix: Description

## Changes
- List of changes
- Another change

## Firmware Details
**SHA256:** paste_your_checksum_here

## Installation
This firmware can be installed via OTA update on ESP32 devices running version 1.1.5 or later.

For manual installation:
1. Download `firmware.bin` below
2. Use esptool or PlatformIO to flash
```

6. **Upload Assets:** Attach `firmware.bin` from `.pio/build/esp32-s3-devkitm-1/`
7. Click "Publish release"

---

## Step 4: Verify

After publishing, verify your release has:
- ✅ Proper semantic version tag (e.g., `1.1.6`)
- ✅ `firmware.bin` in assets
- ✅ SHA256 checksum in description (starting with `SHA256:`)

---

## Example Release Notes

```markdown
## What's New in v1.1.6

### Features
- Added support for RGB LED control
- Implemented scheduling system for automatic on/off
- New REST API endpoints for advanced control

### Improvements
- Reduced memory usage by 10%
- Faster WiFi reconnection
- Better error messages in web UI

### Bug Fixes
- Fixed LED not turning off on some devices
- Resolved WebSocket disconnect issues
- Corrected timezone handling

## Firmware Details
**SHA256:** 1a2b3c4d5e6f7890abcdef1234567890abcdef1234567890abcdef1234567890

**File Size:** 1,033,977 bytes (1.0 MB)  
**Build Date:** 2026-01-15  
**Platform:** ESP32-S3

## Installation

### OTA Update (Recommended)
Devices running firmware 1.1.5 or later will automatically detect this update.
- Enable auto-update in settings, or
- Manually check for updates in the web interface

### Manual Installation
1. Download `firmware.bin` below
2. Use esptool: `esptool.py write_flash 0x10000 firmware.bin`
3. Or use PlatformIO: `platformio run -t upload`

## Compatibility
- Minimum previous version: 1.0.0
- ESP32-S3 DevKit M-1
- Arduino framework 3.20017+
```

---

## Automation Script (Optional)

Create a file `release.sh` to automate the process:

```bash
#!/bin/bash

# Build firmware
platformio run

# Calculate checksum
CHECKSUM=$(sha256sum .pio/build/esp32-s3-devkitm-1/firmware.bin | cut -d' ' -f1)

echo "================================"
echo "Firmware built successfully!"
echo "================================"
echo ""
echo "SHA256: $CHECKSUM"
echo ""
echo "Next steps:"
echo "1. Create new GitHub release"
echo "2. Upload: .pio/build/esp32-s3-devkitm-1/firmware.bin"
echo "3. Add SHA256 to release notes"
echo ""
echo "Copy this line to your release notes:"
echo "SHA256: $CHECKSUM"
```

Make it executable:
```bash
chmod +x release.sh
./release.sh
```

---

## Troubleshooting

### ESP32 doesn't detect update
- Check GitHub release has proper semantic version tag
- Ensure `firmware.bin` is in release assets
- Verify ESP32 has internet connection
- Check serial output for API errors

### Checksum verification fails
- Recalculate checksum from the **exact same file** you uploaded
- Ensure no extra spaces or characters in SHA256 line
- SHA256 must be exactly 64 hexadecimal characters

### Update fails with SSL error
- GitHub's SSL certificate chain is valid
- Check ESP32 has correct time (NTP sync)
- Verify github_root_ca certificate is not expired

---

## Version Numbering Guidelines

Follow semantic versioning: `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes, incompatible API changes
- **MINOR:** New features, backward-compatible
- **PATCH:** Bug fixes, backward-compatible

Examples:
- `1.1.5` → `1.1.6` : Bug fix release
- `1.1.6` → `1.2.0` : New feature added
- `1.2.0` → `2.0.0` : Breaking change

---

## Important Notes

⚠️ **Always test firmware locally before releasing**  
⚠️ **Keep a backup of working firmware**  
⚠️ **Never delete old releases** (users might need to rollback)  
⚠️ **Use pre-releases for beta testing**  
⚠️ **Document breaking changes clearly**

---

## Support

For issues with OTA updates:
1. Check ESP32 serial output for detailed error messages
2. Verify GitHub release format matches this checklist
3. Test with a single device before deploying widely
4. Keep previous firmware version available for rollback
