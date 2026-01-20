# Version X.X.X Release Notes

## New Features
✨ Feature 1: Description of the new feature
✨ Feature 2: Another new feature added
✨ Feature 3: Yet another enhancement

## Improvements
🔧 Improvement 1: What was improved and why
🔧 Improvement 2: Performance or stability enhancement
🔧 Improvement 3: Better user experience change

## Bug Fixes
🐛 Bug 1: What bug was fixed
🐛 Bug 2: Another issue resolved
🐛 Bug 3: Stability issue corrected

## Technical Details
- Technical change 1
- Technical change 2
- Implementation details if relevant

## Breaking Changes
⚠️ List any breaking changes here, or write "None"

## Known Issues
- Known issue 1 (if any)
- Known issue 2 (if any)

Released: [Date]

---

## Example Template with Real Content:

# Version 1.1.7 Release Notes

## New Features
✨ **NTP Time Synchronization** - Automatic time sync for SSL certificate validation (fixes certificate verification errors!)
✨ **GitHub Releases API Integration** - Automatic version detection from GitHub releases (no more version.txt file!)
✨ **HTTPS Certificate Validation** - Secure firmware downloads with SSL certificate verification
✨ **SHA256 Checksum Verification** - Firmware integrity checking to prevent corrupted or tampered updates
✨ **Enhanced Security** - Industry-standard protection against man-in-the-middle attacks

## Improvements
🔧 Automatic NTP time synchronization on WiFi connection (critical for SSL)
🔧 More reliable version checking using GitHub's authoritative release API
🔧 Automatic extraction of firmware URLs from release assets
🔧 Real-time checksum calculation during firmware download
🔧 Better error detection and reporting for failed downloads
🔧 Improved serial debug output with security status indicators (🔐, ✅, ⚠️)
🔧 Clear diagnostic messages for SSL/certificate issues

## Bug Fixes
🐛 **Fixed certificate verification failure** - Added NTP sync to ensure correct system time
🐛 Fixed potential security vulnerability in unencrypted firmware downloads
🐛 Resolved issue where version.txt could be out of sync with actual releases
🐛 Corrected SSL timeout issues on slow networks

## Technical Details
- **New Function:** `syncTimeWithNTP()` - Synchronizes ESP32 time with NTP servers (pool.ntp.org, time.nist.gov)
- **New Function:** `getLatestReleaseInfo()` - Fetches release data from GitHub API
- **New Function:** `calculateSHA256()` - Real-time firmware hash calculation using mbedtls
- **Added:** DigiCert Global Root CA certificate for GitHub SSL validation
- **Added:** `#include <time.h>` for NTP time synchronization
- **Updated:** `performOTAUpdate()` to use WiFiClientSecure with certificate pinning
- **Updated:** `handleGetReleaseNotes()` to use secure HTTPS connections
- **Updated:** `connectToWiFi()` to call NTP sync before checking updates
- **New Variables:** `cachedFirmwareUrl`, `cachedChecksum` for release information
- **Memory Impact:** +8.2 KB flash (30.7% → 31.2%), +60 bytes RAM

## Security Enhancements
🔒 **Time Synchronization:** NTP ensures accurate time for SSL certificate validation
🔒 **SSL/TLS Certificate Validation:** All GitHub connections verify certificate chain
🔒 **Firmware Integrity:** SHA256 checksum prevents corrupted/tampered firmware
🔒 **MITM Protection:** Certificate pinning prevents man-in-the-middle attacks
🔒 **Secure Downloads:** HTTPS-only connections for all update operations

## How to Use SHA256 Checksums

When creating a release, include the firmware checksum in your release notes:

```markdown
## Firmware Details
SHA256: 1a2b3c4d5e6f7890abcdef1234567890abcdef1234567890abcdef1234567890
```

Generate checksum:
- **Windows:** `Get-FileHash firmware.bin -Algorithm SHA256`
- **Linux/Mac:** `sha256sum firmware.bin`

## Breaking Changes
⚠️ None - fully backward compatible with releases that don't include checksums

## Known Issues
- Checksum verification is skipped if SHA256 not found in release notes (warning logged)
- Requires GitHub releases to have `firmware.bin` in assets (previously only needed version.txt)
- NTP time sync may fail if UDP port 123 is blocked by firewall (warning logged, see TROUBLESHOOTING_SSL.md)

## Migration Notes
- **No action required** for existing devices - update works automatically
- **Time sync happens automatically** on WiFi connection (no configuration needed)
- **For new releases:** Follow the [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) guide
- **Optional:** Add SHA256 checksums to release notes for enhanced security
- **If SSL errors occur:** See [TROUBLESHOOTING_SSL.md](TROUBLESHOOTING_SSL.md) for solutions

## What You'll See in Serial Monitor

**Successful Operation:**
```
=== Synchronizing Time with NTP ===
Waiting for NTP time sync: ..........
✅ Time synchronized successfully
📅 Current time: 2026-01-15 14:30:45 UTC

=== Checking for Firmware Update ===
🔐 Performing HTTPS request with certificate validation...
✅ HTTPS request successful
Latest firmware version available: 1.1.7
```

**If NTP Sync Fails:**
```
⚠️  Failed to sync time with NTP server
⚠️  SSL certificate validation may fail!
```
See TROUBLESHOOTING_SSL.md for solutions if this occurs.

Released: January 15, 2026

---
