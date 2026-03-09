# Automatic Release Notes System

This project includes an automatic release notes update system that runs after every git commit.

## How It Works

1. **Post-Commit Hook**: After each commit, a git hook (`.git/hooks/post-commit`) automatically runs
2. **Version Detection**: Extracts the current firmware version from `src/config.h`
3. **Release Notes Update**: Updates `RELEASE_NOTES.md` with the commit message
4. **Staging**: Stages the updated release notes for inclusion in the commit

## Workflow

### Making a Commit

```bash
# Make your changes
git add <files>
git commit -m "feat: Add static IP configuration"

# The post-commit hook runs automatically and updates RELEASE_NOTES.md
# You'll see a message:
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#   Release notes have been updated and staged.
#   Run: git commit --amend --no-edit
#   To include them in this commit.
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Include the release notes in your commit
git commit --amend --no-edit

# Or use the helper script
./update-release-notes.sh
```

## Release Notes Format

The system uses the template from `RELEASE_NOTES_TEMPLATE.md` and maintains this structure in `RELEASE_NOTES.md`:

```markdown
# Release Notes

## Version 1.2.11

## New Features
- [2026-02-03] feat: Add static IP configuration (`abc123`)

## Improvements
- [2026-02-03] perf: Optimize WiFi connection speed (`def456`)

## Bug Fixes
- [2026-02-03] fix: Resolve authentication redirect issue (`ghi789`)

## Technical Details
- [2026-02-03] chore: Update dependencies (`jkl012`)

## Breaking Changes
None

## Known Issues
- None

## Version 1.2.10
...
```

**Automatic Section Assignment:**

Commits are automatically categorized based on their prefix:
- `feat:` or `feature:` → **New Features**
- `fix:` or `bugfix:` → **Bug Fixes**
- `perf:` or `performance:` → **Improvements**
- `refactor:`, `style:`, `chore:` → **Technical Details**
- `docs:` → **Documentation**
- Others → **Technical Details**

Each entry includes:
- **Date**: Commit date (YYYY-MM-DD)
- **Message**: Full commit message
- **Hash**: Short commit hash in code format for reference

## Version Management

### Creating a New Version

When you want to release a new version:

1. **Update the version in `src/config.h`**:
   ```cpp
   #define FIRMWARE_VERSION "1.2.12"  // Increment version
   ```

2. **Commit the version change**:
   ```bash
   git add src/config.h
   git commit -m "chore: Bump version to 1.2.12"
   git commit --amend --no-edit  # Include release notes
   ```

3. **The next commit will create a new version section** in the release notes

## Manual Release Notes

If you want to add more detailed release notes manually:

1. Edit `RELEASE_NOTES.md` directly
2. Add sections like:
   - `### New Features`
   - `### Bug Fixes`
   - `### Performance Improvements`
   - `### Breaking Changes`
   - `### Technical Details`

The automatic system will add commits under `### Changes`, so you can organize your release notes however you prefer.

## Commit Message Best Practices

For better automatic release notes, use conventional commit messages:

- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `style:` - Code style changes (formatting, etc.)
- `refactor:` - Code refactoring
- `perf:` - Performance improvements
- `test:` - Adding or updating tests
- `chore:` - Maintenance tasks

**Examples**:
```bash
git commit -m "feat: Add multi-network WiFi support"
git commit -m "fix: Resolve timer countdown bug in auto-sensing mode"
git commit -m "perf: Optimize CPU usage in main loop"
git commit -m "docs: Update README with new features"
```

## Disabling Auto-Update

If you want to disable automatic release notes updates:

```bash
# Rename or remove the hook
mv .git/hooks/post-commit .git/hooks/post-commit.disabled
```

## Re-enabling Auto-Update

```bash
# Restore the hook
mv .git/hooks/post-commit.disabled .git/hooks/post-commit
chmod +x .git/hooks/post-commit
```

## Troubleshooting

**Release notes not updating?**
- Ensure the hook is executable: `chmod +x .git/hooks/post-commit`
- Check that `src/config.h` has the `FIRMWARE_VERSION` defined
- Verify git hooks are not disabled in your git config

**Want to skip release notes for a commit?**
- Use `git commit --no-verify` to skip hooks
- Or temporarily rename the hook before committing

**Multiple commits before amending?**
- You can make several commits and then run `./update-release-notes.sh` once
- The release notes will include all commits since the last amend
