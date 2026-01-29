# GitHub Release Workflow

This project uses an automated GitHub Actions workflow to build and publish firmware releases.

## How to Use

### 1. Update Release Notes

Before triggering a release, edit `RELEASE_NOTES_TEMPLATE.md` with your actual release notes:

- Add new features, improvements, and bug fixes
- Update technical details
- Note any breaking changes or known issues

### 2. Commit and Push

Commit your changes (including code changes and updated release notes) and push to the repository:

```bash
git add .
git commit -m "Prepare release: description of changes"
git push
```

### 3. Trigger the Release Workflow

1. Go to your GitHub repository
2. Click **Actions** tab
3. Select **"Release Firmware"** from the left sidebar
4. Click **"Run workflow"** button
5. Select version bump type:
   - `patch`: Increments the last number (e.g., 1.1.9 → 1.1.10) - for bug fixes
   - `minor`: Increments the middle number (e.g., 1.1.9 → 1.2.0) - for new features
   - `major`: Increments the first number (e.g., 1.1.9 → 2.0.0) - for breaking changes
6. Click **"Run workflow"**

### 4. What Happens Automatically

The workflow will:

1. Read the current version from `src/config.h`
2. Calculate the new version based on your selection
3. Update `FIRMWARE_VERSION` in `src/config.h`
4. Build the firmware using PlatformIO
5. Calculate SHA256 checksum of the firmware
6. Read your release notes from `RELEASE_NOTES_TEMPLATE.md`
7. Append firmware details (version, SHA256, file size, build date)
8. Commit the version change back to the repository
9. Create a GitHub Release with `firmware.bin` attached

### 5. Verify the Release

After the workflow completes:

1. Go to **Releases** in your GitHub repository
2. Verify the new release has:
   - Correct version tag (e.g., `1.2.0`)
   - `firmware.bin` in the assets
   - SHA256 checksum in the release notes
   - Your release notes content

## Version Numbering

This project follows [Semantic Versioning](https://semver.org/):

| Bump Type | When to Use | Example |
|-----------|-------------|---------|
| `patch` | Bug fixes, minor corrections | 1.1.9 → 1.1.10 |
| `minor` | New features, backward-compatible | 1.1.9 → 1.2.0 |
| `major` | Breaking changes, major rewrites | 1.1.9 → 2.0.0 |

## Troubleshooting

### Workflow fails to run

- Ensure you have pushed all changes to the repository
- Check that the workflow file exists at `.github/workflows/release.yml`

### Build fails

- Check the Actions log for PlatformIO build errors
- Ensure `platformio.ini` is correctly configured

### Release notes are empty

- Make sure `RELEASE_NOTES_TEMPLATE.md` exists and has content
- The `X.X.X` placeholder will be replaced with the actual version

## Files

| File | Purpose |
|------|---------|
| `.github/workflows/release.yml` | GitHub Actions workflow definition |
| `RELEASE_NOTES_TEMPLATE.md` | Template for release notes (edit before each release) |
| `src/config.h` | Contains `FIRMWARE_VERSION` (updated automatically) |
