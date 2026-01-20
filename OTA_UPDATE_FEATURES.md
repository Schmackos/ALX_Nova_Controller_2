# Enhanced OTA Update Features - Implementation Guide

## New Features Implemented

### 1. Visual Progress Bar
- Real-time progress bar showing download percentage
- Displays bytes downloaded vs total size (e.g., "150 / 300 KB")
- Smooth animations and visual feedback
- Shows different states: preparing, connecting, downloading, verifying

### 2. Detailed Status Messages
Both in the web UI and Serial Monitor, you'll see:
- ✅ "Preparing for update..."
- ✅ "Connecting to server..."
- 📦 "Firmware size: X bytes (X.X KB)"
- 📥 "Downloading: X / Y KB"
- 📊 "Progress: X% (X KB / Y KB)"
- ✅ "Download complete. Verifying..."
- 🔄 "Rebooting device in 3 seconds..."
- ❌ Error messages with specific details

### 3. Release Notes Viewer
- "View Release Notes" button appears when update is available
- Beautiful modal popup to display changelog
- Fetches release notes from GitHub repository
- User can read what's new before deciding to update

### 4. Real-time WebSocket Updates
- Progress broadcasts every 5% or every 2 seconds
- No need to refresh the page
- Live status updates for all connected clients

## GitHub Repository Setup

To use the release notes feature, structure your firmware repository like this:

```
Esp32_firmware/
├── README.md
├── version.txt                     # Contains: 1.2.0
├── releases/
│   ├── 1.0.0.md                   # Release notes for v1.0.0
│   ├── 1.1.0.md                   # Release notes for v1.1.0
│   ├── 1.1.1.md                   # Release notes for v1.1.1
│   └── 1.2.0.md                   # Release notes for v1.2.0 (latest)
└── firmware.bin                    # Latest firmware binary
```

### Example Release Notes File (1.2.0.md)

```markdown
# Version 1.2.0 Release Notes

## New Features
✨ Added visual progress bar for OTA updates
✨ Release notes viewer with modal popup
✨ Real-time download progress with KB counter
✨ Enhanced status messages in Serial Monitor

## Improvements
🔧 Enhanced WebSocket stability
🔧 Better error messages during updates
🔧 Improved UI responsiveness
🔧 Broadcast progress every 5% for smoother updates

## Bug Fixes
🐛 Fixed LED timing issues during downloads
🐛 Resolved WiFi reconnection problems
🐛 Fixed memory leak in update process
🐛 Corrected progress calculation for large files

## Technical Details
- Total download size now displayed
- Emoji indicators in Serial Monitor for better readability
- Verification step added after download
- Graceful error handling with detailed messages

## Breaking Changes
⚠️ None

Released: January 15, 2026
```

## Configuration

Update the following URL in your `main.cpp` if needed:

```cpp
const char* releaseNotesUrlBase = "https://raw.githubusercontent.com/YOUR_USERNAME/YOUR_REPO/main/releases/";
```

Replace `YOUR_USERNAME` and `YOUR_REPO` with your actual GitHub details.

## How It Works

### Update Flow:

1. **User clicks "Check for Updates"**
   - ESP32 fetches `version.txt` from GitHub
   - Compares with current firmware version
   - If newer version available, shows "Update Available" message

2. **User clicks "View Release Notes"** (optional)
   - Fetches release notes from GitHub (`releases/X.X.X.md`)
   - Displays in modal popup
   - User reads changelog to decide whether to update

3. **User clicks "Update to Latest Version"**
   - Shows "Preparing for update..." (0%)
   - Shows "Connecting to server..."
   - Begins download with real-time progress
   - Updates every 5% or every 2 seconds
   - Shows "Downloading: X / Y KB" with percentage
   - After download: "Verifying firmware..." (100%)
   - On success: "Update complete! Rebooting..."
   - Device automatically restarts

### Progress Broadcasting:

The ESP32 broadcasts progress via WebSocket:
```json
{
  "type": "updateStatus",
  "status": "downloading",
  "progress": 45,
  "message": "Downloading: 150 / 330 KB",
  "bytesDownloaded": 153600,
  "totalBytes": 337920,
  "updateAvailable": true,
  "latestVersion": "1.2.0",
  "currentVersion": "1.1.1"
}
```

## Serial Monitor Output

Enhanced terminal output with emojis for better readability:

```
=== Starting OTA Update ===
Downloading from: https://github.com/.../firmware.bin
📦 Firmware size: 337920 bytes (330.00 KB)
📥 Downloading firmware to flash...
📊 Progress: 5% (16 KB / 330 KB)
📊 Progress: 10% (33 KB / 330 KB)
📊 Progress: 15% (49 KB / 330 KB)
...
📊 Progress: 95% (313 KB / 330 KB)
📊 Progress: 100% (330 KB / 330 KB)
✅ Download complete. Verifying...
✅ OTA update completed successfully!
🔄 Rebooting device in 3 seconds...
```

## Error Handling

Enhanced error messages help diagnose issues:

- ❌ "Failed to connect to server" (HTTP error)
- ❌ "Invalid firmware file" (file size issue)
- ❌ "Not enough storage space" (insufficient memory)
- ❌ "Failed to initialize update" (Update.begin() failed)
- ❌ "Write error during download" (flash write failed)
- ❌ "Update verification failed" (Update.isFinished() false)

## Testing

1. Upload the firmware to your ESP32
2. Connect to the web interface
3. Click "Check for Updates"
4. If an update is available:
   - Click "View Release Notes" to see the changelog
   - Click "Update to Latest Version" to start
5. Watch the progress bar and status messages
6. Monitor Serial output for detailed logging

## Auto-Update Feature

The existing auto-update feature still works:
- Toggle "Auto update on boot" in the web interface
- When enabled, ESP32 checks for updates every 5 minutes
- If found, shows 30-second countdown
- Can be cancelled by clicking "Cancel Auto-Update"

## Benefits

✅ **Better User Experience**: Visual feedback keeps users informed
✅ **Transparency**: Users know exactly what's happening at each stage
✅ **Informed Decisions**: Release notes help users decide when to update
✅ **Professional**: Emojis and detailed messages look polished
✅ **Debugging**: Detailed Serial output helps troubleshoot issues
✅ **Confidence**: Progress bar reduces anxiety during updates

## Customization

### Change Progress Bar Color:
Edit `.progress-bar` CSS in `web_pages.cpp`:
```css
background: linear-gradient(90deg, #4CAF50, #45a049); /* Green */
/* Change to: */
background: linear-gradient(90deg, #2196F3, #0b7dda); /* Blue */
```

### Adjust Broadcast Frequency:
In `performOTAUpdate()` function:
```cpp
if (otaProgress % 5 == 0 || (now - lastBroadcast) >= 2000) {
    // Broadcast every 5% or every 2 seconds
    // Change 5 to 10 for every 10%
    // Change 2000 to 1000 for every 1 second
}
```

## Troubleshooting

**Progress bar not showing:**
- Check browser console for JavaScript errors
- Verify WebSocket connection is active
- Ensure `broadcastUpdateStatus()` is being called

**Release notes not loading:**
- Verify `releaseNotesUrlBase` URL is correct
- Check that release notes file exists on GitHub
- Ensure file is named exactly as the version (e.g., `1.2.0.md`)
- Check GitHub raw URL is accessible

**Serial monitor shows no emojis:**
- Some terminals don't support emojis
- This is cosmetic only; functionality is not affected
- Use a modern terminal like Windows Terminal or iTerm2

## Future Enhancements

Possible improvements:
- [ ] Add download speed indicator (KB/s)
- [ ] Show estimated time remaining
- [ ] Support rollback to previous version
- [ ] Compare release notes between versions
- [ ] Download firmware in background using FreeRTOS task
- [ ] Add integrity verification (checksum/signature)

---

**Version:** 1.2.0
**Date:** January 15, 2026
**Compatibility:** ESP32-S3, Arduino Framework
