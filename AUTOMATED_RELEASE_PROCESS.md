# Automated Release Process

This project has a **fully automated** release workflow that handles version management, release notes, firmware building, and GitHub releases.

## 🔄 Complete Automation Flow

```
1. Developer makes commits
   └─> Post-commit hook updates RELEASE_NOTES.md
   └─> Developer amends commit to include release notes

2. Developer triggers GitHub Action
   └─> Bumps version in src/config.h
   └─> Creates/updates version section in RELEASE_NOTES.md
   └─> Builds firmware
   └─> Calculates SHA256 checksum
   └─> Extracts release notes for this version only
   └─> Creates GitHub release with:
       • Release notes from RELEASE_NOTES.md
       • Firmware details table
       • SHA256 verification instructions
       • firmware.bin file
   └─> Commits version bump back to repository
```

## 📝 Local Development: Automatic Release Notes

### How It Works

Every commit automatically updates `RELEASE_NOTES.md`:

```bash
# Make your changes
git add src/wifi_manager.cpp

# Commit with conventional commit prefix
git commit -m "feat: Add static IP configuration"

# Post-commit hook runs and updates RELEASE_NOTES.md
# You'll see a prompt to run:
git commit --amend --no-edit
```

### Conventional Commit Prefixes

Commits are automatically categorized:

| Prefix | Section | Example |
|--------|---------|---------|
| `feat:` | New Features | `feat: Add multi-network support` |
| `fix:` | Bug Fixes | `fix: Resolve connection timeout` |
| `perf:` | Improvements | `perf: Optimize memory usage` |
| `refactor:`, `chore:`, `style:` | Technical Details | `refactor: Clean up WiFi code` |
| `docs:` | Documentation | `docs: Update API documentation` |

**See `RELEASE_NOTES_AUTOMATION.md` for complete details.**

## 🚀 GitHub Release: Fully Automated

### Triggering a Release

1. **Go to GitHub Actions**
   - Navigate to your repository
   - Click "Actions" tab
   - Select "Release Firmware" workflow

2. **Run Workflow**
   - Click "Run workflow"
   - Select version bump type:
     - **patch**: 1.2.3 → 1.2.4 (bug fixes)
     - **minor**: 1.2.3 → 1.3.0 (new features)
     - **major**: 1.2.3 → 2.0.0 (breaking changes)
   - Click "Run workflow"

3. **Wait for Automation**
   - Tests run first (must pass)
   - Version is bumped
   - Firmware is built
   - Release notes are extracted
   - GitHub release is created
   - Changes are committed back

### What Gets Created

**GitHub Release includes:**

```markdown
## Version 1.2.12

## New Features
- [2026-02-03] feat: Add static IP configuration (`abc123`)

## Bug Fixes
- [2026-02-03] fix: Resolve authentication redirect (`def456`)

## Improvements
- None

## Technical Details
- [2026-02-03] chore: Update dependencies (`ghi789`)

## Breaking Changes
None

## Known Issues
- None

---

## 📦 Firmware Details

| Property | Value |
|----------|-------|
| **Version** | `1.2.12` |
| **SHA256** | `a1b2c3d4e5f6...` |
| **File Size** | 1234 KB |
| **Build Date** | 2026-02-03 14:30:00 UTC |
| **Platform** | ESP32-S3 DevKit M-1 |
| **Tests** | ✅ All tests passed |

### 🔐 Verification

To verify the firmware integrity:
```bash
sha256sum firmware.bin
# Should match: a1b2c3d4e5f6...
```
```

**Attached Files:**
- `firmware.bin` - Ready to flash

## 🔧 Version Management

### Current Version

The firmware version is defined in `src/config.h`:

```cpp
#define FIRMWARE_VERSION "1.2.11"
```

### Version Bump Process

When the GitHub Action runs:

1. **Reads current version** from `src/config.h`
2. **Calculates new version** based on bump type
3. **Updates `src/config.h`** with new version
4. **Creates/updates** version section in `RELEASE_NOTES.md`
5. **Commits changes** back to repository
6. **Creates Git tag** for the version
7. **Creates GitHub release** with notes and firmware

### Manual Version Bump

If you need to manually bump the version:

```bash
# Edit src/config.h
nano src/config.h
# Change: #define FIRMWARE_VERSION "1.2.12"

# Commit with special message
git add src/config.h
git commit -m "chore: Bump version to 1.2.12"
git commit --amend --no-edit  # Include release notes

# Push
git push
```

## 📊 Release Notes Structure

### Template-Based

Uses `RELEASE_NOTES_TEMPLATE.md` structure:

```markdown
## Version X.X.X

## New Features
- Feature descriptions

## Improvements
- Performance enhancements

## Bug Fixes
- Fixed issues

## Technical Details
- Under-the-hood changes

## Breaking Changes
- API changes that break compatibility

## Known Issues
- Current limitations
```

### Automatic Population

The post-commit hook populates sections based on commit prefixes.

### Manual Additions

You can manually edit `RELEASE_NOTES.md` to add:
- Detailed feature descriptions
- Migration guides
- Performance metrics
- Architecture diagrams
- Links to documentation

The automation will preserve your manual additions and add commit entries to the appropriate sections.

## 🎯 Best Practices

### Commit Messages

**Good:**
```bash
git commit -m "feat: Add static IP configuration for WiFi networks"
git commit -m "fix: Resolve authentication redirect loop on unauthorized access"
git commit -m "perf: Optimize main loop CPU usage by 60%"
```

**Bad:**
```bash
git commit -m "updated code"
git commit -m "fixes"
git commit -m "WIP"
```

### Release Cadence

**Recommended workflow:**

1. **Daily commits** with auto-updated release notes
2. **Weekly/bi-weekly patch releases** (bug fixes)
3. **Monthly minor releases** (new features)
4. **Quarterly major releases** (breaking changes)

### Testing Before Release

The GitHub Action runs tests automatically, but you should:

```bash
# Run tests locally before pushing
pio test -e native

# Build firmware to check for compilation errors
pio run -e esp32-s3-devkitm-1

# Test on actual hardware if possible
```

## 🛠 Troubleshooting

### Release Notes Not Generated

**Problem:** GitHub release has no release notes

**Solution:**
- Check that `RELEASE_NOTES.md` exists in repository
- Verify the version exists in `RELEASE_NOTES.md`
- Look at GitHub Actions logs for extraction errors

### Version Already Exists

**Problem:** Release fails because tag already exists

**Solution:**
```bash
# Delete the tag locally and remotely
git tag -d 1.2.11
git push origin :refs/tags/1.2.11

# Re-run the GitHub Action
```

### Build Fails

**Problem:** GitHub Action fails during firmware build

**Solution:**
- Check GitHub Actions logs for compilation errors
- Verify PlatformIO configuration in `platformio.ini`
- Test build locally: `pio run -e esp32-s3-devkitm-1`

### Tests Fail

**Problem:** Release is blocked by failing tests

**Solution:**
- Check test output in GitHub Actions
- Run tests locally: `pio test -e native -v`
- Fix failing tests before re-running action

## 📚 Related Documentation

- **RELEASE_NOTES_AUTOMATION.md** - Details on post-commit hook
- **RELEASE_NOTES_TEMPLATE.md** - Release notes template
- **RELEASE_NOTES.md** - Actual release notes
- **.github/workflows/release.yml** - GitHub Actions workflow

## 🎉 Summary

This project achieves **full automation** of the release process:

✅ **Automatic release notes** via post-commit hook
✅ **Template-based structure** for consistency
✅ **Conventional commits** for categorization
✅ **Automated version bumping** via GitHub Actions
✅ **Automated firmware building** with checksum
✅ **Automated GitHub releases** with notes and binary
✅ **Automatic git commits** back to repository

**Result:** One-click releases with complete documentation!
