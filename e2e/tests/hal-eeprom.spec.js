/**
 * hal-eeprom.spec.js — HAL EEPROM: scan button sends WS command,
 * scan results display, program button, and status states.
 *
 * The EEPROM Programming card lives in the Devices tab.
 * eepromScan() sends a WS command {type: 'eepromScan'} and the
 * response comes as a hardware_stats message with EEPROM data nested at dac.eeprom.
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
  await expect(page.locator('#hal-device-list')).toContainText('PCM5102A', { timeout: 5000 });
}

test.describe('@hal @ws HAL EEPROM', () => {

  test('EEPROM scan button sends eepromScan WS command', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    const eepromCard = page.locator('#eepromCard');
    await expect(eepromCard).toBeVisible({ timeout: 5000 });

    // Click the "Scan I2C Bus" button
    const scanBtn = eepromCard.locator('button').filter({ hasText: /Scan/i });
    await scanBtn.click();
    await page.waitForTimeout(500);

    // Verify the WS command was sent
    const wsCapture = page.wsCapture || [];
    const eepromCmd = wsCapture.find(msg => msg.type === 'eepromScan');
    expect(eepromCmd).toBeDefined();
  });

  test('scan results display in UI via hardwareStats WS message', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    const eepromCard = page.locator('#eepromCard');
    await expect(eepromCard).toBeVisible({ timeout: 5000 });

    // Push a hardware_stats with eeprom data nested at dac.eeprom showing a found EEPROM
    page.wsRoute.send({
      type: 'hardware_stats',
      cpu: { usageTotal: 10, usageCore0: 5, usageCore1: 5, temperature: 40, freqMHz: 360, model: 'ESP32-P4', revision: '1', cores: 2 },
      dac: {
        eeprom: {
          scanned: true,
          found: true,
          addr: 80,
          i2cMask: 1,
          i2cDevices: 1,
          readErrors: 0,
          writeErrors: 0,
          deviceName: 'ES9822PRO',
          manufacturer: 'ESS Technology',
          deviceId: 7,
          hwRevision: 1,
          maxChannels: 2,
          dacI2cAddress: 64,
          flags: 0,
          sampleRates: [44100, 48000, 96000]
        }
      }
    });

    const status = page.locator('#eepromStatus');
    await expect(status).toContainText('Programmed', { timeout: 5000 });
  });

  test('EEPROM program button is visible', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    const eepromCard = page.locator('#eepromCard');
    await expect(eepromCard).toBeVisible({ timeout: 5000 });

    const programBtn = eepromCard.locator('button').filter({ hasText: /Program/i });
    await expect(programBtn).toBeVisible();
  });

  test('EEPROM status shows "Not scanned" before first scan', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    const eepromCard = page.locator('#eepromCard');
    await expect(eepromCard).toBeVisible({ timeout: 5000 });

    const status = page.locator('#eepromStatus');
    await expect(status).toContainText('Not scanned');
  });

  test('scan shows "No EEPROM detected" when nothing found', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    const eepromCard = page.locator('#eepromCard');
    await expect(eepromCard).toBeVisible({ timeout: 5000 });

    // Push scan result with nothing found
    page.wsRoute.send({
      type: 'hardware_stats',
      cpu: { usageTotal: 10, usageCore0: 5, usageCore1: 5, temperature: 40, freqMHz: 360, model: 'ESP32-P4', revision: '1', cores: 2 },
      dac: {
        eeprom: {
          scanned: true,
          found: false,
          i2cMask: 0,
          i2cDevices: 0,
          readErrors: 0,
          writeErrors: 0
        }
      }
    });

    const status = page.locator('#eepromStatus');
    await expect(status).toContainText('No EEPROM detected', { timeout: 5000 });
  });

});
