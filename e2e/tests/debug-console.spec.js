/**
 * debug-console.spec.js — Debug console log rendering and module chip filtering.
 */

const { test, expect } = require('../helpers/fixtures');

test('debug log entries render in console when debugLog WS message is received', async ({ connectedPage: page }) => {
  // Use JavaScript to call switchTab() directly — the debug sidebar item may be off-screen
  // when the sidebar nav is clipped at 720px height, making scrollIntoViewIfNeeded unreliable.
  await page.evaluate(() => switchTab('debug'));

  const debugConsole = page.locator('#debugConsole');
  await expect(debugConsole).toBeVisible();

  // Send a debugLog WS message
  page.wsRoute.send({
    type: 'debugLog',
    timestamp: 5000,
    message: '[I] [Audio] I2S init complete',
    level: 'info',
    module: 'Audio',
  });

  // A log entry with the message should appear
  await expect(debugConsole).toContainText('I2S init complete', { timeout: 3000 });

  // The module chip "Audio" should be created
  await expect(page.locator('#moduleChips .btn-chip[data-module="Audio"]')).toBeVisible({ timeout: 3000 });
});

test('module chip filtering hides entries from other modules', async ({ connectedPage: page }) => {
  // Use JavaScript to call switchTab() directly — avoids sidebar scroll constraints at 720px height.
  await page.evaluate(() => switchTab('debug'));

  // Send two log entries from different modules
  page.wsRoute.send({
    type: 'debugLog',
    timestamp: 1000,
    message: '[I] [WiFi] Connected to TestNetwork',
    level: 'info',
    module: 'WiFi',
  });

  page.wsRoute.send({
    type: 'debugLog',
    timestamp: 2000,
    message: '[I] [MQTT] Broker connected',
    level: 'info',
    module: 'MQTT',
  });

  // Wait for both chips
  await expect(page.locator('#moduleChips .btn-chip[data-module="WiFi"]')).toBeVisible({ timeout: 3000 });
  await expect(page.locator('#moduleChips .btn-chip[data-module="MQTT"]')).toBeVisible({ timeout: 3000 });

  // Click the WiFi chip to filter to WiFi-only entries
  await page.locator('#moduleChips .btn-chip[data-module="WiFi"]').click();

  const debugConsole = page.locator('#debugConsole');

  // WiFi message is visible
  await expect(debugConsole).toContainText('TestNetwork');

  // MQTT message should be hidden (display:none on the entry)
  const mqttEntries = debugConsole.locator('.log-entry[data-module="MQTT"]');
  if (await mqttEntries.count() > 0) {
    await expect(mqttEntries.first()).toBeHidden();
  }
});
