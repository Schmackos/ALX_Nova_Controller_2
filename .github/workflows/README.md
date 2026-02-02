# GitHub Actions CI/CD

## What This Does

Every time you push code to GitHub, this workflow automatically:

1. ✅ **Runs all tests** (23 tests in ~30 seconds)
   - Smart sensing logic tests
   - API endpoint tests

2. ✅ **Builds firmware** (only if tests pass)
   - Ensures code compiles
   - Generates firmware.bin artifact

3. ✅ **Prevents broken code** from being merged
   - Pull requests show test status
   - Can require passing tests before merge

## Workflow Details

### Trigger Events
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`

### Jobs

#### 1. Test Job
- Runs on: Ubuntu Linux (has gcc pre-installed)
- Uses: PlatformIO native environment
- Duration: ~30-60 seconds
- Outputs: Test results and pass/fail status

#### 2. Build Job
- Runs on: Ubuntu Linux
- Depends on: Test job passing
- Uses: ESP32-S3 environment
- Duration: ~20-40 seconds
- Outputs: firmware.bin artifact (kept for 30 days)

### Caching
- PlatformIO packages are cached
- Subsequent runs are faster (~15-20 seconds)

## Viewing Test Results

### On GitHub
1. Go to your repository
2. Click **Actions** tab
3. Click on any workflow run
4. See test results and logs

### Status Badge
Add to your README.md:
```markdown
![Tests](https://github.com/Schmackos/ALX_Nova_Controller_2/actions/workflows/tests.yml/badge.svg)
```

Shows: ![Tests](https://img.shields.io/badge/tests-passing-brightgreen) or ![Tests](https://img.shields.io/badge/tests-failing-red)

## Pull Request Protection

### Enable Required Status Checks
1. Go to **Settings** → **Branches**
2. Add branch protection rule for `main`
3. Check **Require status checks to pass before merging**
4. Select **test** job
5. Check **Require branches to be up to date before merging**

Now: Pull requests can't be merged if tests fail! ✅

## Example Workflow

```bash
# You make changes locally
vim src/smart_sensing.cpp

# Commit and push
git add src/smart_sensing.cpp
git commit -m "fix: Improve timer logic"
git push origin main

# GitHub automatically:
# 1. Runs tests (23 tests)
# 2. Builds firmware
# 3. Shows results in Actions tab
# 4. Sends email if tests fail

# You can see results at:
# https://github.com/Schmackos/ALX_Nova_Controller_2/actions
```

## Cost

- ✅ **FREE** for public repositories
- ✅ **FREE** for private repositories (2,000 minutes/month)
- Current usage: ~1 minute per push

## Troubleshooting

### Tests fail on GitHub but pass locally
→ Check test output in Actions tab for details
→ Ensure all dependencies are in platformio.ini

### Workflow doesn't run
→ Check it's pushed to main or develop branch
→ Check .github/workflows/tests.yml exists

### Want to run on every branch?
Change in tests.yml:
```yaml
on:
  push:
    branches: [ '*' ]  # All branches
```

### Want to skip CI on a commit?
```bash
git commit -m "docs: Update README [skip ci]"
```

## Notifications

### Email Notifications
- Enabled by default
- Sent when tests fail
- Configure in GitHub Settings → Notifications

### Slack/Discord Integration
Add webhook in tests.yml:
```yaml
- name: Notify Slack
  if: failure()
  uses: 8398a7/action-slack@v3
  with:
    status: ${{ job.status }}
    webhook_url: ${{ secrets.SLACK_WEBHOOK }}
```

## Next Steps

1. ✅ Push .github/workflows/tests.yml to GitHub
2. ✅ Check Actions tab for first run
3. ✅ Add status badge to README
4. ⚠️ Optional: Enable branch protection
5. ⚠️ Optional: Add more test tiers (WiFi, MQTT, etc.)
