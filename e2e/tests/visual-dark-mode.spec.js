/**
 * visual-dark-mode.spec.js — Visual regression tests for dark/light mode
 * across different tabs.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

test.describe('@visual Visual Dark Mode', () => {

  test('control tab in light mode', async ({ connectedPage: page }) => {
    // Switch to light mode
    await page.evaluate(() => {
      document.body.classList.remove('night-mode');
      if (typeof applyTheme === 'function') applyTheme(false);
    });
    await page.waitForTimeout(300);

    // Ensure we are on the control tab
    await page.evaluate(() => switchTab('control'));
    await page.waitForTimeout(300);

    const panel = page.locator('#control');
    await expect(panel).toBeVisible({ timeout: 5000 });
    await expect(panel).toHaveScreenshot('control-tab-light-mode.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('control tab in dark mode', async ({ connectedPage: page }) => {
    // Switch to dark mode
    await page.evaluate(() => {
      document.body.classList.add('night-mode');
      if (typeof applyTheme === 'function') applyTheme(true);
    });
    await page.waitForTimeout(300);

    await page.evaluate(() => switchTab('control'));
    await page.waitForTimeout(300);

    const panel = page.locator('#control');
    await expect(panel).toBeVisible({ timeout: 5000 });
    await expect(panel).toHaveScreenshot('control-tab-dark-mode.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('settings tab in dark mode', async ({ connectedPage: page }) => {
    await page.evaluate(() => {
      document.body.classList.add('night-mode');
      if (typeof applyTheme === 'function') applyTheme(true);
    });
    await page.waitForTimeout(300);

    await page.evaluate(() => switchTab('settings'));
    await page.waitForTimeout(300);

    const panel = page.locator('#settings');
    await expect(panel).toBeVisible({ timeout: 5000 });
    await expect(panel).toHaveScreenshot('settings-tab-dark-mode.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('device cards in dark mode', async ({ connectedPage: page }) => {
    await page.evaluate(() => {
      document.body.classList.add('night-mode');
      if (typeof applyTheme === 'function') applyTheme(true);
    });
    await page.waitForTimeout(300);

    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);
    page.wsRoute.send(HAL_FIXTURE);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    await expect(deviceList).toHaveScreenshot('device-cards-dark-mode.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

});
