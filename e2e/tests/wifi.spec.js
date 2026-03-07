/**
 * wifi.spec.js — WiFi tab: SSID/password form, scan button, static IP toggle.
 *
 * Note: #useStaticIP is a <input type="checkbox"> inside <label class="switch">.
 * CSS hides the raw input (opacity:0; width:0; height:0). Click the parent label to
 * toggle and assert state with toBeChecked().
 */

const { test, expect } = require('../helpers/fixtures');

test('WiFi tab shows SSID/password form, scan button, saved networks, and static IP toggle', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="wifi"]').click();

  // SSID input
  const ssidInput = page.locator('#appState\\.wifiSSID');
  await expect(ssidInput).toBeVisible();

  // Password input
  const pwdInput = page.locator('#appState\\.wifiPassword');
  await expect(pwdInput).toBeVisible();
  await expect(pwdInput).toHaveAttribute('type', 'password');

  // Network scan button
  const scanBtn = page.locator('#scanBtn');
  await expect(scanBtn).toBeVisible();

  // Saved networks dropdown (configNetworkSelect)
  const configSelect = page.locator('#configNetworkSelect');
  await expect(configSelect).toBeVisible();

  // Static IP toggle — the label is the visible element; the input is CSS-hidden
  const staticIPLabel = page.locator('label.switch:has(#useStaticIP)');
  await expect(staticIPLabel).toBeVisible();

  const staticIPToggle = page.locator('#useStaticIP');

  // Static IP fields are hidden before toggle is on
  await expect(page.locator('#staticIPFields')).toBeHidden();

  // Enabling the toggle (click label) reveals static IP fields
  await staticIPLabel.click();
  await expect(page.locator('#staticIPFields')).toBeVisible({ timeout: 2000 });

  // Static IP fields include IP, subnet, gateway inputs
  await expect(page.locator('#staticIP')).toBeVisible();
  await expect(page.locator('#subnet')).toBeVisible();
  await expect(page.locator('#gateway')).toBeVisible();
});
