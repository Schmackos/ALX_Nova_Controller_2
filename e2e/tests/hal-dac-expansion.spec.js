/**
 * hal-dac-expansion.spec.js — Expansion DAC capability badges and sample rate display.
 *
 * Verifies that new capability bits (MQA, Line Driver, APLL, DSD, HP Amp) render
 * as badges and appear in expanded detail text, and that 384k/768k sample rates
 * are displayed when the corresponding bits are set.
 *
 * Fixture devices under test:
 *   - ES9069Q  (slot 9):  capabilities=279  (DAC+Vol+Filters+Mute+MQA),        sampleRates=252 (44.1k-768k)
 *   - CS43131  (slot 10): capabilities=6167 (DAC+Vol+Filters+Mute+DSD+HP_AMP),  sampleRates=124 (44.1k-384k)
 *   - ES9033Q  (slot 11): capabilities=535  (DAC+Vol+Filters+Mute+Line Driver),  sampleRates=252
 *   - PCM5102A (slot 0):  capabilities=16   (DAC only),                          sampleRates=28
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

async function openDevicesTab(page) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
  page.wsRoute.send(HAL_FIXTURE);
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });
}

function cardByName(page, name) {
  return page.locator('.hal-device-card').filter({ hasText: name });
}

async function expandCard(page, name) {
  const card = cardByName(page, name);
  await card.locator('.hal-device-header').click();
  await expect(card.locator('.hal-device-details')).toBeVisible({ timeout: 5000 });
  return card;
}

test.describe('@hal HAL Expansion DAC Badges', () => {

  // --- MQA badge ---
  test('ES9069Q shows MQA capability badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = cardByName(page, 'ES9069Q');
    await expect(card.locator('.hal-cap-badge', { hasText: 'MQA' })).toBeVisible();
  });

  // --- DSD badge ---
  test('CS43131 shows DSD capability badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = cardByName(page, 'CS43131');
    await expect(card.locator('.hal-cap-badge', { hasText: 'DSD' })).toBeVisible();
  });

  // --- HP Amp badge ---
  test('CS43131 shows HP Amp capability badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = cardByName(page, 'CS43131');
    await expect(card.locator('.hal-cap-badge', { hasText: 'HP Amp' })).toBeVisible();
  });

  // --- Line Driver badge ---
  test('ES9033Q shows Line Driver capability badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = cardByName(page, 'ES9033Q');
    await expect(card.locator('.hal-cap-badge', { hasText: 'Line Driver' })).toBeVisible();
  });

  // --- 384k sample rate in expanded details ---
  test('ES9069Q expanded details show 384k sample rate', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'ES9069Q');
    const details = card.locator('.hal-device-details');
    const ratesRow = details.locator('.hal-detail-row').filter({ hasText: 'Sample Rates:' });
    await expect(ratesRow).toContainText('384k');
  });

  // --- 768k sample rate in expanded details ---
  test('ES9069Q expanded details show 768k sample rate', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'ES9069Q');
    const details = card.locator('.hal-device-details');
    const ratesRow = details.locator('.hal-detail-row').filter({ hasText: 'Sample Rates:' });
    await expect(ratesRow).toContainText('768k');
  });

  // --- CS43131 shows 384k but not 768k ---
  test('CS43131 expanded details show 384k but not 768k', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'CS43131');
    const details = card.locator('.hal-device-details');
    const ratesRow = details.locator('.hal-detail-row').filter({ hasText: 'Sample Rates:' });
    await expect(ratesRow).toContainText('384k');
    const ratesText = await ratesRow.textContent();
    expect(ratesText).not.toContain('768k');
  });

  // --- Capability detail text includes new capabilities ---
  test('ES9069Q expanded details list MQA in capabilities row', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'ES9069Q');
    const details = card.locator('.hal-device-details');
    const capsRow = details.locator('.hal-detail-row').filter({ hasText: 'Capabilities:' });
    await expect(capsRow).toContainText('MQA');
  });

  test('CS43131 expanded details list DSD and HP Amp in capabilities row', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'CS43131');
    const details = card.locator('.hal-device-details');
    const capsRow = details.locator('.hal-detail-row').filter({ hasText: 'Capabilities:' });
    await expect(capsRow).toContainText('DSD');
    await expect(capsRow).toContainText('HP Amp');
  });

  test('ES9033Q expanded details list Line Driver in capabilities row', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'ES9033Q');
    const details = card.locator('.hal-device-details');
    const capsRow = details.locator('.hal-detail-row').filter({ hasText: 'Capabilities:' });
    await expect(capsRow).toContainText('Line Driver');
  });

  // --- Negative: PCM5102A has no new-capability badges ---
  test('PCM5102A does not show MQA, DSD, HP Amp, Line Driver, or APLL badges', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = cardByName(page, 'PCM5102A');
    // PCM5102A has capabilities=16 (DAC only) — none of the new badges should appear
    await expect(card.locator('.hal-cap-badge', { hasText: 'MQA' })).toHaveCount(0);
    await expect(card.locator('.hal-cap-badge', { hasText: 'DSD' })).toHaveCount(0);
    await expect(card.locator('.hal-cap-badge', { hasText: 'HP Amp' })).toHaveCount(0);
    await expect(card.locator('.hal-cap-badge', { hasText: 'Line Driver' })).toHaveCount(0);
    await expect(card.locator('.hal-cap-badge', { hasText: 'APLL' })).toHaveCount(0);
  });

  // --- Negative: PCM5102A does not show 384k or 768k ---
  test('PCM5102A expanded details do not show 384k or 768k', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = await expandCard(page, 'PCM5102A');
    const details = card.locator('.hal-device-details');
    const ratesRow = details.locator('.hal-detail-row').filter({ hasText: 'Sample Rates:' });
    const ratesText = await ratesRow.textContent();
    expect(ratesText).not.toContain('384k');
    expect(ratesText).not.toContain('768k');
  });

  // --- Dynamic update: pushing a device with APLL bit shows APLL badge ---
  test('APLL badge renders when device has APLL capability bit set', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    // Push a custom fixture with a device that has APLL (bit 10 = 1024)
    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    // Give ES9069Q the APLL bit too: 279 | 1024 = 1303
    const es9069q = modified.devices.find(d => d.slot === 9);
    es9069q.capabilities = 1303;
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('ES9069Q', { timeout: 5000 });

    const card = cardByName(page, 'ES9069Q');
    await expect(card.locator('.hal-cap-badge', { hasText: 'APLL' })).toBeVisible();
    // MQA should still be present
    await expect(card.locator('.hal-cap-badge', { hasText: 'MQA' })).toBeVisible();
  });

});
