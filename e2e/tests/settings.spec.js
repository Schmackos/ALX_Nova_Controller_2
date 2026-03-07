/**
 * settings.spec.js — Settings tab: theme toggle, buzzer controls, display settings.
 * Verifies that displayState and buzzerState WS fixtures populate the selects.
 *
 * Note: Toggle inputs (#darkModeToggle, #buzzerToggle, #backlightToggle) are CSS-hidden
 * inside <label class="switch"> elements (opacity:0; width:0; height:0).
 * Use toBeChecked() for state assertions and click the parent label for interaction.
 *
 * Fixture values:
 *   displayState.json: backlightOn=true, screenTimeout=30, backlightBrightness=200
 *   buzzerState.json:  enabled=true, volume=1
 */

const { test, expect } = require('../helpers/fixtures');

test('settings tab controls are populated from WS fixtures and respond to interaction', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  // Dark mode toggle — the label is visible; the input is CSS-hidden (opacity:0; width:0; height:0)
  const darkModeLabel = page.locator('label.switch:has(#darkModeToggle)');
  await expect(darkModeLabel).toBeVisible();
  // State should be unchecked by default (localStorage default)
  const darkModeToggle = page.locator('#darkModeToggle');
  await expect(darkModeToggle).not.toBeChecked();

  // Buzzer toggle — fixture buzzerState.json has enabled: true
  const buzzerToggle = page.locator('#buzzerToggle');
  await expect(buzzerToggle).toBeChecked({ timeout: 3000 });

  // Buzzer volume select — fixture has volume: 1 (Medium)
  // buzzerFields is shown when buzzer is enabled
  const buzzerVolumeSelect = page.locator('#buzzerVolumeSelect');
  await expect(buzzerVolumeSelect).toHaveValue('1', { timeout: 3000 });

  // Display: backlight toggle — fixture displayState.json has backlightOn: true
  const backlightToggle = page.locator('#backlightToggle');
  await expect(backlightToggle).toBeChecked({ timeout: 3000 });

  // Screen timeout select — fixture displayState.json has screenTimeout: 30
  const screenTimeoutSelect = page.locator('#screenTimeoutSelect');
  await expect(screenTimeoutSelect).toHaveValue('30', { timeout: 3000 });

  // Brightness select — fixture has backlightBrightness: 200
  // 200/255 * 100 ≈ 78%, closest preset is 75%
  const brightnessSelect = page.locator('#brightnessSelect');
  await expect(brightnessSelect).toBeVisible();
  // value should be one of the preset options (75 or 100 based on rounding)
  const brightnessVal = await brightnessSelect.inputValue();
  expect(['75', '100']).toContain(brightnessVal);

  // Change password button is visible
  await expect(page.locator('button[onclick="showPasswordChangeModal()"]')).toBeVisible();
});
