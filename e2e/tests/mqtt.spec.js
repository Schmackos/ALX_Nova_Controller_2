/**
 * mqtt.spec.js — MQTT tab: enable toggle reveals fields, WS fixture populates values.
 *
 * Note: Toggle inputs (#appState.mqttEnabled, #appState.mqttHADiscovery) are CSS-hidden
 * inside <label class="switch"> elements (opacity:0; width:0; height:0).
 * Use toBeChecked() for state assertions and click the parent label for interaction.
 *
 * Fixture values (mqtt-settings.json): enabled=true, broker=192.168.1.50, port=1883,
 * baseTopic="alx-nova", haDiscovery=true, useTls=false, verifyCert=false
 */

const { test, expect } = require('../helpers/fixtures');

test('MQTT enable toggle reveals config fields and WS broadcast populates values', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('mqtt'));

  // The mqttSettings fixture has enabled: true, so the checkbox is checked.
  // The input is CSS-hidden (opacity:0; width:0; height:0) — assert with toBeChecked().
  const mqttEnabledToggle = page.locator('#appState\\.mqttEnabled');
  await expect(mqttEnabledToggle).toBeChecked({ timeout: 3000 });

  const mqttFields = page.locator('#mqttFields');
  await expect(mqttFields).toBeVisible({ timeout: 3000 });

  // Values from mqttSettings.json fixture (baseTopic: "alx-nova")
  await expect(page.locator('#appState\\.mqttBroker')).toHaveValue('192.168.1.50', { timeout: 3000 });
  await expect(page.locator('#appState\\.mqttPort')).toHaveValue('1883');
  await expect(page.locator('#appState\\.mqttBaseTopic')).toHaveValue('alx-nova');

  // HA discovery is enabled in the fixture — also CSS-hidden, assert with toBeChecked()
  const haDiscoveryToggle = page.locator('#appState\\.mqttHADiscovery');
  await expect(haDiscoveryToggle).toBeChecked();

  // Disabling the toggle hides the fields — click the parent label (the input is CSS-hidden)
  const mqttEnabledLabel = page.locator('label.switch:has(#appState\\.mqttEnabled)');
  await mqttEnabledLabel.click();
  await expect(mqttFields).toBeHidden({ timeout: 2000 });

  // Re-enabling shows them again
  await mqttEnabledLabel.click();
  await expect(mqttFields).toBeVisible({ timeout: 2000 });
});

test('MQTT TLS toggle shows certificate verification option', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('mqtt'));

  // TLS should be off by default (from fixture)
  const tlsToggle = page.locator('#appState\\.mqttUseTls');
  await expect(tlsToggle).not.toBeChecked({ timeout: 3000 });

  // Cert verify fields should be hidden when TLS is off
  const tlsFields = page.locator('#mqttTlsFields');
  await expect(tlsFields).toBeHidden();

  // Enable TLS by clicking the parent label
  const tlsLabel = page.locator('label.switch:has(#appState\\.mqttUseTls)');
  await tlsLabel.click();
  await expect(tlsToggle).toBeChecked();
  await expect(tlsFields).toBeVisible({ timeout: 2000 });

  // Verify cert toggle should be visible and unchecked (default from fixture)
  const verifyCertToggle = page.locator('#appState\\.mqttVerifyCert');
  await expect(verifyCertToggle).not.toBeChecked();
});

test('MQTT TLS toggle auto-suggests port 8883', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('mqtt'));

  // Port should start as 1883 (from fixture)
  await expect(page.locator('#appState\\.mqttPort')).toHaveValue('1883', { timeout: 3000 });

  // Enable TLS
  const tlsLabel = page.locator('label.switch:has(#appState\\.mqttUseTls)');
  await tlsLabel.click();

  // Port should auto-change to 8883
  await expect(page.locator('#appState\\.mqttPort')).toHaveValue('8883', { timeout: 2000 });

  // Disable TLS
  await tlsLabel.click();

  // Port should revert to 1883
  await expect(page.locator('#appState\\.mqttPort')).toHaveValue('1883', { timeout: 2000 });
});
