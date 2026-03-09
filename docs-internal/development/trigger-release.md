# How to Trigger Release Workflow

## Option 1: Via GitHub Web Interface (Easiest)

1. **Go to your repository:**
   https://github.com/Schmackos/ALX_Nova_Controller_2

2. **Click the "Actions" tab** at the top

3. **Find "Release Firmware" workflow** in the left sidebar

4. **Click "Run workflow" button** (top right)

5. **Select parameters:**
   - Branch: `main`
   - Version bump type: `patch` (for 1.2.5 → 1.2.6)

6. **Click green "Run workflow" button**

7. **Watch it run:**
   - Test job will run first (~30-60 seconds)
   - Release job will run if tests pass (~30-60 seconds)
   - Total: ~1-2 minutes

## Option 2: Via Browser (Direct Link)

Open this URL:
https://github.com/Schmackos/ALX_Nova_Controller_2/actions/workflows/release.yml

Then follow steps 4-7 above.

## What to Look For

### Step 1: Test Job
```
✓ Checkout repository
✓ Set up Python
✓ Install PlatformIO
✓ Run Tests
  → test_smart_sensing: 10/10 passed
  → test_api: 13/13 passed
✓ Upload test results
```

### Step 2: Release Job (only if tests pass)
```
✓ Checkout repository
✓ Get current version (1.2.5)
✓ Calculate new version (1.2.6)
✓ Update version in config.h
✓ Build firmware
✓ Calculate SHA256 checksum
✓ Prepare release notes
✓ Commit version change
✓ Create GitHub Release
```

## Expected Results

### Success ✅
- New release appears at: https://github.com/Schmackos/ALX_Nova_Controller_2/releases
- Release tagged: `1.2.6`
- Firmware file attached: `firmware.bin`
- Release notes include test status
- Version bumped in `src/config.h`

### Failure ❌
- If tests fail: Release job won't run
- Error logs will show which test failed
- No release created
- Version not bumped

## After Triggering

Check results at:
https://github.com/Schmackos/ALX_Nova_Controller_2/actions

Look for the running workflow with a yellow dot (in progress) or green check (success).
