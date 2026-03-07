/**
 * control-tab.spec.js — Control tab: sensing mode radios and amp status display.
 */

const { test, expect } = require('../helpers/fixtures');

test('sensing mode radio buttons are visible and can be changed', async ({ connectedPage: page }) => {
  // The Control panel is the default active tab
  const radios = page.locator('input[name="sensingMode"]');
  await expect(radios).toHaveCount(3);

  // All three options exist
  await expect(page.locator('input[name="sensingMode"][value="always_on"]')).toBeVisible();
  await expect(page.locator('input[name="sensingMode"][value="always_off"]')).toBeVisible();
  await expect(page.locator('input[name="sensingMode"][value="smart_auto"]')).toBeVisible();

  // Clicking always_on selects it
  await page.locator('input[name="sensingMode"][value="always_on"]').click();
  await expect(page.locator('input[name="sensingMode"][value="always_on"]')).toBeChecked();
});

test('smartSensing WS update changes amp status display', async ({ connectedPage: page }) => {
  // Initial state: amp OFF
  await expect(page.locator('#amplifierStatus')).toHaveText('OFF');

  // Inject a WS message that indicates amp is now ON.
  // Uses the actual field names read by updateSmartSensingUI() in 22-settings.js:
  //   amplifierState (bool), signalDetected (bool), audioLevel (float), etc.
  page.wsRoute.send({
    type: 'smartSensing',
    amplifierState: true,
    signalDetected: true,
    audioLevel: -20.0,
    audioVrms: 0.5,
    mode: 'smart_auto',
    timerDuration: 15,
    audioThreshold: -60,
    timerActive: false,
    timerRemaining: 0,
  });

  // The amplifier display should update to ON
  await expect(page.locator('#amplifierStatus')).toHaveText('ON', { timeout: 3000 });

  // Signal detected row should update
  await expect(page.locator('#signalDetected')).toHaveText('Yes', { timeout: 3000 });
});
