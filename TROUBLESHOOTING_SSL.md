# SSL Certificate Verification Troubleshooting

## Issue: "Certificate verification failed"

This error occurs when the ESP32 cannot validate GitHub's SSL certificate. This is **usually caused by incorrect system time**.

---

## Solution: NTP Time Synchronization (v1.1.6+)

### Automatic Fix (Implemented in v1.1.6)

The firmware now automatically synchronizes time with NTP servers when WiFi connects.

**What to look for in Serial Monitor:**

```
=== Synchronizing Time with NTP ===
Waiting for NTP time sync: ..........
✅ Time synchronized successfully
📅 Current time: 2026-01-15 14:30:45 UTC
```

If you see this, time sync worked and SSL should work fine.

---

## Problem: NTP Sync Fails

If you see:

```
⚠️  Failed to sync time with NTP server
⚠️  SSL certificate validation may fail!
```

### Causes:

1. **Network/Firewall blocking NTP** (UDP port 123)
2. **Router doesn't allow NTP traffic**
3. **NTP servers temporarily unavailable**

### Solutions:

#### Option 1: Check Network (Recommended)

1. Ensure your network allows NTP (UDP port 123)
2. Try different NTP servers
3. Restart ESP32 and router

#### Option 2: Manual Time Set

If NTP is blocked, you can manually set time in code:

```cpp
// In setup(), after WiFi connects:
struct tm timeinfo;
timeinfo.tm_year = 2026 - 1900;  // Year since 1900
timeinfo.tm_mon = 0;              // January (0-11)
timeinfo.tm_mday = 15;            // Day of month
timeinfo.tm_hour = 12;            // Hour (0-23)
timeinfo.tm_min = 0;
timeinfo.tm_sec = 0;
time_t t = mktime(&timeinfo);
struct timeval now = { .tv_sec = t };
settimeofday(&now, NULL);
```

#### Option 3: Disable Certificate Validation (NOT RECOMMENDED)

**⚠️ WARNING: Only use for testing/debugging! This removes security!**

Change this in `main.cpp`:

```cpp
// Certificate validation - set to false only for testing/debugging
const bool ENABLE_CERT_VALIDATION = false;  // Changed from true
```

Then in `getLatestReleaseInfo()` and `performOTAUpdate()`, add:

```cpp
WiFiClientSecure client;
if (!ENABLE_CERT_VALIDATION) {
    client.setInsecure();  // Skip certificate validation
} else {
    client.setCACert(github_root_ca);
}
```

---

## Diagnosing the Issue

### Check Serial Output

When the firmware tries to connect to GitHub, look for these messages:

**✅ Success:**
```
Fetching release info from: https://api.github.com/repos/...
🔐 Performing HTTPS request with certificate validation...
✅ HTTPS request successful
```

**❌ Failure:**
```
❌ Failed to get release info. HTTP code: -1
⚠️  Connection failed - possible causes:
   - SSL certificate validation failed (check NTP time sync)
   - Network/firewall blocking HTTPS
   - GitHub API temporarily unavailable
```

### Check Current Time

Add this to your code to check the current time:

```cpp
time_t now = time(nullptr);
Serial.printf("Current Unix timestamp: %ld\n", now);

if (now < 1000000000) {
    Serial.println("❌ Time not set! (before year 2000)");
} else {
    Serial.println("✅ Time looks valid");
}
```

---

## Understanding the Problem

### Why Time Matters for SSL

SSL certificates have validity periods:
- **Not Before:** Certificate start date
- **Not After:** Certificate expiration date

GitHub's certificate is valid from ~2024 to ~2025.

If your ESP32 thinks it's **January 1, 1970** (default):
- The certificate appears to be "not yet valid"
- SSL validation fails
- Connection rejected

### Default ESP32 Time

Without NTP, ESP32 starts at Unix epoch: **January 1, 1970, 00:00:00**

This is **before** GitHub's certificate validity period, causing validation to fail.

---

## Testing Your Fix

### 1. Upload Firmware with NTP Support

```bash
platformio run -t upload
```

### 2. Open Serial Monitor

```bash
platformio device monitor
```

### 3. Watch for Time Sync

Look for:
```
=== Synchronizing Time with NTP ===
✅ Time synchronized successfully
📅 Current time: 2026-01-15 14:30:45 UTC
```

### 4. Check Update

The firmware should now check for updates:
```
=== Checking for Firmware Update ===
Current firmware version installed: 1.1.6
Fetching release info from: https://api.github.com/repos/...
🔐 Performing HTTPS request with certificate validation...
✅ HTTPS request successful
Latest firmware version available: 1.1.6
Firmware is up to date!
```

---

## Common Errors and Solutions

### Error: HTTP Code -1

**Cause:** Certificate validation failed or connection error

**Solution:**
1. Check NTP time sync (most common)
2. Check internet connection
3. Verify GitHub is accessible

### Error: HTTP Code 403

**Cause:** GitHub API rate limit (60 requests/hour without auth)

**Solution:**
- Wait an hour
- Increase `OTA_CHECK_INTERVAL` to check less frequently

### Error: HTTP Code 404

**Cause:** No releases found in repository

**Solution:**
- Create a release on GitHub
- Ensure release has `firmware.bin` asset

### Error: "firmware.bin not found in release assets"

**Cause:** Release doesn't have firmware.bin file

**Solution:**
- Add `firmware.bin` to release assets
- Check file name is exactly `firmware.bin`

---

## Advanced: Custom NTP Servers

If default NTP servers don't work, modify `syncTimeWithNTP()`:

```cpp
void syncTimeWithNTP() {
  Serial.println("\n=== Synchronizing Time with NTP ===");
  
  // Use custom NTP servers (change to your local NTP server)
  configTime(0, 0, "time.google.com", "pool.ntp.org", "time.cloudflare.com");
  
  // Rest of function...
}
```

Common NTP servers:
- `pool.ntp.org` - Global pool (default)
- `time.google.com` - Google's NTP
- `time.cloudflare.com` - Cloudflare's NTP
- `time.windows.com` - Microsoft's NTP
- `time.apple.com` - Apple's NTP

---

## Verification Checklist

✅ **Before deploying v1.1.6:**

1. ☐ Serial monitor shows NTP sync success
2. ☐ Current time displays correctly (not 1970)
3. ☐ HTTPS request shows "✅ HTTPS request successful"
4. ☐ Update check completes without errors
5. ☐ Release notes can be fetched (tests HTTPS)

✅ **If any fail:**

1. Check network allows NTP (UDP 123)
2. Check network allows HTTPS (TCP 443)
3. Try different NTP servers
4. Consider manual time set if NTP blocked

---

## Summary

**The Fix:** Firmware v1.1.6 adds automatic NTP time synchronization

**Why:** SSL certificates require correct time to validate

**Result:** Certificate verification now works automatically

**Fallback:** If NTP fails, serial output will show warnings and suggest solutions

---

## Need More Help?

Check these in order:

1. **Serial Monitor:** Look for specific error messages
2. **Time Sync:** Ensure "✅ Time synchronized successfully"
3. **Network:** Test HTTPS on another device
4. **GitHub:** Verify releases exist and are accessible
5. **Documentation:** See `OTA_SECURITY_IMPROVEMENTS.md`

**Last updated:** January 15, 2026  
**Firmware version:** 1.1.6+
