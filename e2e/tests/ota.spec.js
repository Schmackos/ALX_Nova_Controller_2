/**
 * ota.spec.js — Firmware update tab: version displayed, check button hits API.
 *
 * The OTA controls live inside the Settings tab section
 * (not a separate tab — see index.html structure).
 *
 * The current version is populated by fetchUpdateStatus() which is called on
 * authSuccess — the mock server's /api/updatestatus now returns currentVersion.
 */

const { test, expect } = require('../helpers/fixtures');

test('current firmware version is displayed and check-for-updates button calls the API', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  // Current version field is populated from /api/updatestatus (called after authSuccess)
  const currentVersion = page.locator('#currentVersion');
  await expect(currentVersion).toBeVisible();
  // The mock server returns firmwareVersion: '1.12.0' — wait for it to load
  await expect(currentVersion).not.toHaveText('Loading...', { timeout: 5000 });
  // Version should be non-empty and contain a version string
  await expect(currentVersion).not.toHaveText('', { timeout: 5000 });

  // "Check for Updates" button is present
  const checkBtn = page.locator('button[onclick="checkForUpdate()"]');
  await expect(checkBtn).toBeVisible();

  // Intercept the check request
  let checkedCalled = false;
  await page.route('/api/v1/checkupdate', async (route) => {
    checkedCalled = true;
    await route.fulfill({
      status: 200,
      body: JSON.stringify({
        available: false,
        currentVersion: '1.12.0',
        latestVersion: '1.12.0',
      }),
    });
  });

  await checkBtn.click();
  await page.waitForTimeout(500);
  expect(checkedCalled).toBe(true);
});
