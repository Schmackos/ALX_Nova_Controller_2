/**
 * settings-debug.spec.js -- Debug and advanced settings controls.
 *
 * Debug serial level and hardware stats toggle live in the Debug tab (visible
 * only when debugMode is enabled). FFT window type is in the Audio tab.
 * Auto-update and SSL cert validation use POST /api/settings (not WS).
 * Debug mode toggle is in the Settings tab and sends WS command.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@settings @ws Settings Debug', () => {

  test('debug serial level dropdown sends setDebugSerialLevel WS command', async ({ connectedPage: page }) => {
    await test.step('Enable debug mode so debug tab appears', async () => {
      // debugMode starts as false in debug-state.json fixture.
      // We need to enable it first so the debug tab becomes visible.
      await page.evaluate(() => switchTab('settings'));
      page.wsCapture = [];
      const label = page.locator('label.switch:has(#debugModeToggle)');
      await label.click();
      // Wait for the debug tab to appear
      await expect(page.locator('.sidebar-item[data-tab="debug"]')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Navigate to debug tab and change serial level', async () => {
      await page.evaluate(() => switchTab('debug'));
      page.wsCapture = [];
      const select = page.locator('#debugSerialLevel');
      await expect(select).toBeVisible({ timeout: 3000 });
      await select.selectOption('3');
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setDebugSerialLevel');
      expect(cmd).toBeTruthy();
      expect(cmd.level).toBe(3);
    });
  });

  test('FFT window type selector sends setFftWindowType WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to audio tab', async () => {
      await page.evaluate(() => switchTab('audio'));
    });

    await test.step('Change FFT window type', async () => {
      const select = page.locator('#fftWindowSelect');
      await expect(select).toBeVisible({ timeout: 3000 });
      page.wsCapture = [];
      await select.selectOption('1'); // Blackman
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setFftWindowType');
      expect(cmd).toBeTruthy();
      expect(cmd.value).toBe(1);
    });
  });

  test('auto-update toggle sends POST /api/settings', async ({ connectedPage: page }) => {
    let settingsPosted = false;
    let postBody = null;

    await test.step('Set up route intercept', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          settingsPosted = true;
          postBody = JSON.parse(route.request().postData());
          await route.fulfill({
            status: 200,
            contentType: 'application/json',
            body: JSON.stringify({ success: true }),
          });
        } else {
          await route.continue();
        }
      });
    });

    await test.step('Navigate to settings and toggle auto-update', async () => {
      await page.evaluate(() => switchTab('settings'));
      const label = page.locator('label.switch:has(#autoUpdateToggle)');
      await expect(label).toBeVisible();
      await label.click();
    });

    await test.step('Verify POST sent with autoUpdateEnabled', async () => {
      await page.waitForTimeout(500);
      expect(settingsPosted).toBe(true);
      expect(postBody).toHaveProperty('autoUpdateEnabled');
    });
  });

  test('SSL cert validation toggle sends POST /api/settings', async ({ connectedPage: page }) => {
    let settingsPosted = false;
    let postBody = null;

    await test.step('Set up route intercept', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          settingsPosted = true;
          postBody = JSON.parse(route.request().postData());
          await route.fulfill({
            status: 200,
            contentType: 'application/json',
            body: JSON.stringify({ success: true }),
          });
        } else {
          await route.continue();
        }
      });
    });

    await test.step('Navigate to settings and toggle cert validation', async () => {
      await page.evaluate(() => switchTab('settings'));
      const label = page.locator('label.switch:has(#certValidationToggle)');
      await expect(label).toBeVisible();
      await label.click();
    });

    await test.step('Verify POST sent with enableCertValidation', async () => {
      await page.waitForTimeout(500);
      expect(settingsPosted).toBe(true);
      expect(postBody['appState.enableCertValidation']).toBeDefined();
    });
  });

  test('debug mode toggle sends setDebugMode WS command', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings tab', async () => {
      await page.evaluate(() => switchTab('settings'));
    });

    await test.step('Toggle debug mode on', async () => {
      const toggle = page.locator('#debugModeToggle');
      // Fixture has debugMode=false
      await expect(toggle).not.toBeChecked();
      page.wsCapture = [];
      const label = page.locator('label.switch:has(#debugModeToggle)');
      await label.click();
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setDebugMode');
      expect(cmd).toBeTruthy();
      expect(cmd.enabled).toBe(true);
    });
  });

  test('hardware stats toggle sends setDebugHwStats WS command', async ({ connectedPage: page }) => {
    await test.step('Enable debug mode first', async () => {
      await page.evaluate(() => switchTab('settings'));
      const label = page.locator('label.switch:has(#debugModeToggle)');
      await label.click();
      await expect(page.locator('.sidebar-item[data-tab="debug"]')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Navigate to debug tab and toggle HW stats', async () => {
      await page.evaluate(() => switchTab('debug'));
      page.wsCapture = [];
      const label = page.locator('label.switch:has(#debugHwStatsToggle)');
      await expect(label).toBeVisible({ timeout: 3000 });
      await label.click();
    });

    await test.step('Verify WS command sent', async () => {
      const cmd = page.wsCapture.find(m => m.type === 'setDebugHwStats');
      expect(cmd).toBeTruthy();
      expect(cmd.enabled).toBe(true);
    });
  });

});
