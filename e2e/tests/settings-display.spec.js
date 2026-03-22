/**
 * settings-display.spec.js -- Display-related settings: brightness, screen timeout,
 * dim timeout, backlight toggle, dim enable toggle.
 *
 * All display controls send WebSocket commands when changed. The toggles also
 * POST to /api/settings but the primary assertion is the WS message shape.
 *
 * Fixture values (display-state.json):
 *   backlightOn=true, screenTimeout=30, backlightBrightness=200,
 *   dimEnabled=true, dimTimeout=60, dimBrightness=64
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@settings @ws Settings Display', () => {

  test('brightness select sends setBrightness WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Change brightness to 50%', async () => {
      const select = page.locator('#brightnessSelect');
      await expect(select).toBeVisible();
      page.wsCapture = [];
      await select.selectOption('50');
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setBrightness');
      expect(cmd).toBeTruthy();
      // 50% of 255 = 128 (rounded)
      expect(cmd.value).toBe(128);
    });
  });

  test('screen timeout select sends setScreenTimeout WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Change screen timeout to 60', async () => {
      const select = page.locator('#screenTimeoutSelect');
      await expect(select).toBeVisible();
      page.wsCapture = [];
      await select.selectOption('60');
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setScreenTimeout');
      expect(cmd).toBeTruthy();
      expect(cmd.value).toBe(60);
    });
  });

  test('dim timeout select sends setDimTimeout WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Change dim timeout to 30', async () => {
      // dimEnabled is true from fixture, so dimTimeoutRow should be visible
      const select = page.locator('#dimTimeoutSelect');
      await expect(select).toBeVisible();
      page.wsCapture = [];
      await select.selectOption('30');
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setDimTimeout');
      expect(cmd).toBeTruthy();
      expect(cmd.value).toBe(30);
    });
  });

  test('backlight toggle sends setBacklight WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Toggle backlight off', async () => {
      const toggle = page.locator('#backlightToggle');
      // Fixture has backlightOn=true, so it starts checked
      await expect(toggle).toBeChecked({ timeout: 3000 });
      page.wsCapture = [];
      // Click the parent label since the input is CSS-hidden
      const label = page.locator('label.switch:has(#backlightToggle)');
      await label.click();
    });

    await test.step('Verify WS command sent with enabled=false', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setBacklight');
      expect(cmd).toBeTruthy();
      expect(cmd.enabled).toBe(false);
    });
  });

  test('dim enable toggle sends setDimEnabled WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Toggle dim off', async () => {
      const toggle = page.locator('#dimToggle');
      // Fixture has dimEnabled=true
      await expect(toggle).toBeChecked({ timeout: 3000 });
      page.wsCapture = [];
      const label = page.locator('label.switch:has(#dimToggle)');
      await label.click();
    });

    await test.step('Verify WS command sent with enabled=false', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setDimEnabled');
      expect(cmd).toBeTruthy();
      expect(cmd.enabled).toBe(false);
    });
  });

});
