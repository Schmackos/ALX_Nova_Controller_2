/**
 * hardware-stats.spec.js — CPU, heap, PSRAM, temperature fields populated
 * from a hardware_stats WS message.
 *
 * The hardware_stats message can be received on any tab; the elements it
 * populates (#cpuTotal, #heapPercent, etc.) live in the debug tab panel.
 */

const { test, expect } = require('../helpers/fixtures');

test('hardware stats elements are populated from hardware_stats WS message', async ({ connectedPage: page }) => {
  // Switch to the Debug tab where the hardware stats elements are rendered.
  // Use JavaScript to call switchTab() directly — the debug sidebar item may be off-screen
  // when the sidebar nav is clipped at 720px height.
  await page.evaluate(() => switchTab('debug'));

  // Inject a hardware_stats message
  page.wsRoute.send({
    type: 'hardware_stats',
    cpu: {
      usageTotal: 35,
      usageCore0: 20,
      usageCore1: 15,
      temperature: 52.3,
      freqMHz: 240,
      model: 'ESP32-P4',
      revision: 'v1.0',
      cores: 2,
    },
    memory: {
      heapTotal: 524288,
      heapFree: 340000,
      heapMinFree: 200000,
      heapMaxBlock: 150000,
      psramTotal: 8388608,
      psramFree: 7000000,
    },
    storage: {
      flashSize: 8388608,
      sketchSize: 1572864,
      sketchFree: 2621440,
      LittleFSTotal: 1048576,
      LittleFSUsed: 32768,
    },
    wifi: { rssi: -50, channel: 6, apClients: 0 },
    uptime: 120000,
  });

  // CPU total load
  await expect(page.locator('#cpuTotal')).toHaveText('35%', { timeout: 3000 });

  // CPU temperature
  await expect(page.locator('#cpuTemp')).toHaveText(/52\.3/, { timeout: 3000 });

  // Heap percentage: (1 - 340000/524288) * 100 ≈ 35%
  const heapPercentEl = page.locator('#heapPercent');
  await expect(heapPercentEl).not.toHaveText('--%', { timeout: 3000 });

  // Heap free should show a human-readable value
  const heapFreeEl = page.locator('#heapFree');
  await expect(heapFreeEl).not.toHaveText('', { timeout: 3000 });

  // PSRAM percentage should be populated (psramTotal > 0)
  const psramEl = page.locator('#psramPercent');
  await expect(psramEl).not.toHaveText('N/A', { timeout: 3000 });
  await expect(psramEl).not.toHaveText('', { timeout: 3000 });

  // Uptime should be populated
  await expect(page.locator('#uptime')).not.toHaveText('', { timeout: 3000 });
});
