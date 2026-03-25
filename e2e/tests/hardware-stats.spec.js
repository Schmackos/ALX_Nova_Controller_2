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

test('PSRAM fields in hardware_stats message are accepted without error', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('debug'));

  // Send hardware_stats with the new PSRAM allocation tracking fields.
  // These fields are present in the WS message even if the UI does not yet
  // render dedicated elements for all of them. The test verifies that the
  // frontend processes the message without throwing (no console errors).
  const consoleErrors = [];
  page.on('console', (msg) => {
    if (msg.type() === 'error') consoleErrors.push(msg.text());
  });

  page.wsRoute.send({
    type: 'hardware_stats',
    cpu: { usageTotal: 40, usageCore0: 25, usageCore1: 15, temperature: 45.0, freqMHz: 360, model: 'ESP32-P4', revision: 1, cores: 2 },
    memory: { heapTotal: 327680, heapFree: 182340, heapMinFree: 154880, heapMaxBlock: 131072, psramTotal: 8388608, psramFree: 7000000 },
    storage: { flashSize: 16777216, sketchSize: 1245184, sketchFree: 2883584, LittleFSTotal: 1441792, LittleFSUsed: 32768 },
    wifi: { rssi: -45, channel: 6, apClients: 0 },
    uptime: 60000,
    heapCritical: false,
    dmaAllocFailed: false,
    psramFallbackCount: 2,
    psramFailedCount: 0,
    psramAllocPsram: 155000,
    psramAllocSram: 8192,
    psramWarning: false,
    psramCritical: false,
  });

  // PSRAM percentage should still be rendered correctly
  await expect(page.locator('#psramPercent')).not.toHaveText('N/A', { timeout: 3000 });

  // PSRAM free should be populated
  await expect(page.locator('#psramFree')).not.toHaveText('--', { timeout: 3000 });

  // No console errors from processing the new fields
  // (small delay to let any async handlers run)
  await page.waitForTimeout(500);
  const psramRelatedErrors = consoleErrors.filter(e => e.toLowerCase().includes('psram'));
  expect(psramRelatedErrors).toHaveLength(0);
});

test('heap critical row becomes visible when heapCritical is true', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('debug'));

  // Enable debug mode and hardware stats so the stats section is visible
  page.wsRoute.send({ type: 'debugState', debugMode: true, debugHwStats: true, debugI2sMetrics: false, debugTaskMonitor: false, debugSerialLevel: 2 });
  await page.waitForTimeout(300);

  // Send hardware_stats with heapCritical = true
  page.wsRoute.send({
    type: 'hardware_stats',
    cpu: { usageTotal: 90, usageCore0: 85, usageCore1: 95, temperature: 65.0, freqMHz: 360, model: 'ESP32-P4', revision: 1, cores: 2 },
    memory: { heapTotal: 327680, heapFree: 30000, heapMinFree: 25000, heapMaxBlock: 20000, psramTotal: 8388608, psramFree: 2000000 },
    storage: { flashSize: 16777216, sketchSize: 1245184, sketchFree: 2883584, LittleFSTotal: 1441792, LittleFSUsed: 32768 },
    wifi: { rssi: -70, channel: 6, apClients: 0 },
    uptime: 300000,
    heapCritical: true,
    dmaAllocFailed: false,
    psramFallbackCount: 5,
    psramFailedCount: 1,
    psramAllocPsram: 200000,
    psramAllocSram: 32768,
    psramWarning: true,
    psramCritical: false,
  });

  // The heapCritical row should become visible
  await expect(page.locator('#heapCriticalRow')).toBeVisible({ timeout: 3000 });
  await expect(page.locator('#heapCriticalValue')).toHaveText('YES');
});

test('pipeline timing fields in hardware_stats populate DSP CPU section', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('debug'));

  // Enable debug mode so the DSP CPU section is visible
  page.wsRoute.send({ type: 'debugState', debugMode: true, debugHwStats: true, debugI2sMetrics: false, debugTaskMonitor: false, debugSerialLevel: 2 });
  await page.waitForTimeout(300);

  // Send hardware_stats with pipeline timing fields (inputReadUs, perInputDspUs,
  // sinkWriteUs added in foundation hardening — these come from PipelineTimingMetrics).
  // The frontend updateHardwareStats() handler processes pipelineCpu/pipelineFrameUs/
  // matrixUs/outputDspUs/firBypassCount when present on the data object.
  page.wsRoute.send({
    type: 'hardware_stats',
    cpu: { usageTotal: 40, usageCore0: 25, usageCore1: 15, temperature: 45.0, freqMHz: 360, model: 'ESP32-P4', revision: 1, cores: 2 },
    memory: { heapTotal: 327680, heapFree: 182340, heapMinFree: 154880, heapMaxBlock: 131072, psramTotal: 8388608, psramFree: 7000000 },
    storage: { flashSize: 16777216, sketchSize: 1245184, sketchFree: 2883584, LittleFSTotal: 1441792, LittleFSUsed: 32768 },
    wifi: { rssi: -45, channel: 6, apClients: 0 },
    uptime: 60000,
    pipelineCpu: 8.3,
    pipelineFrameUs: 1200,
    matrixUs: 180,
    outputDspUs: 320,
    inputReadUs: 210,
    perInputDspUs: 150,
    sinkWriteUs: 340,
    firBypassCount: 0,
  });

  // The DSP CPU section should become visible with pipeline timing values
  await expect(page.locator('#dsp-cpu-section')).toBeVisible({ timeout: 3000 });
  await expect(page.locator('#dsp-cpu-pipeline')).toHaveText(/8\.3/);
  await expect(page.locator('#dsp-frame-us')).toHaveText(/1200/);
  await expect(page.locator('#dsp-matrix-us')).toHaveText(/180/);
  await expect(page.locator('#dsp-output-us')).toHaveText(/320/);
});

test('DMA allocation failure row becomes visible with lane bitmask', async ({ connectedPage: page }) => {
  await page.evaluate(() => switchTab('debug'));

  // Enable debug mode and hardware stats so the stats section is visible
  page.wsRoute.send({ type: 'debugState', debugMode: true, debugHwStats: true, debugI2sMetrics: false, debugTaskMonitor: false, debugSerialLevel: 2 });
  await page.waitForTimeout(300);

  page.wsRoute.send({
    type: 'hardware_stats',
    cpu: { usageTotal: 50, usageCore0: 30, usageCore1: 70, temperature: 55.0, freqMHz: 360, model: 'ESP32-P4', revision: 1, cores: 2 },
    memory: { heapTotal: 327680, heapFree: 100000, heapMinFree: 80000, heapMaxBlock: 60000, psramTotal: 8388608, psramFree: 5000000 },
    storage: { flashSize: 16777216, sketchSize: 1245184, sketchFree: 2883584, LittleFSTotal: 1441792, LittleFSUsed: 32768 },
    wifi: { rssi: -55, channel: 6, apClients: 0 },
    uptime: 200000,
    heapCritical: false,
    dmaAllocFailed: true,
    dmaAllocFailMask: 0x0003,
    psramFallbackCount: 0,
    psramFailedCount: 0,
    psramAllocPsram: 155000,
    psramAllocSram: 0,
    psramWarning: false,
    psramCritical: false,
  });

  // DMA failure row should become visible with lane details
  await expect(page.locator('#dmaAllocFailRow')).toBeVisible({ timeout: 3000 });
  await expect(page.locator('#dmaAllocFailValue')).toHaveText(/Lane 0/);
  await expect(page.locator('#dmaAllocFailValue')).toHaveText(/Lane 1/);
});
