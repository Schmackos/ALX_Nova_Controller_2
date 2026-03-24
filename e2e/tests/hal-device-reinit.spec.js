/**
 * hal-device-reinit.spec.js — HAL Device Reinit and Remove: re-initialize button,
 * remove confirmation dialog, and state updates after reinit.
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
  await expect(page.locator('#hal-device-list')).toContainText('ES9822PRO', { timeout: 5000 });
}

test.describe('@hal @api HAL Device Reinit', () => {

  test('reinit button sends POST /api/hal/devices/reinit', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    let reinitPayload = null;
    await page.route('/api/hal/devices/reinit', async (route) => {
      reinitPayload = JSON.parse(route.request().postData());
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', state: 'AVAILABLE' })
      });
    });
    // Also intercept the GET /api/hal/devices that loadHalDeviceList() triggers after reinit
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'GET') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      } else {
        await route.continue();
      }
    });

    // Click the Re-initialize button on ES9822PRO (slot 7)
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('button[title="Re-initialize"]').click();

    await page.waitForTimeout(500);
    expect(reinitPayload).not.toBeNull();
    expect(reinitPayload.slot).toBe(7);
  });

  test('remove button shows confirmation dialog', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // ES9822PRO has discovery=1 (EEPROM), so remove button is available
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    const removeBtn = card.locator('.hal-icon-btn-danger[title="Remove device"]');
    await expect(removeBtn).toBeVisible();

    // Set up dialog handler to dismiss (cancel)
    let dialogMessage = '';
    page.once('dialog', async (dialog) => {
      dialogMessage = dialog.message();
      await dialog.dismiss();
    });

    await removeBtn.click();
    await page.waitForTimeout(300);
    expect(dialogMessage).toContain('Remove');
  });

  test('confirm remove sends DELETE /api/hal/devices', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    let deletePayload = null;
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'DELETE') {
        deletePayload = JSON.parse(route.request().postData());
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ status: 'ok' })
        });
      } else if (route.request().method() === 'GET') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      } else {
        await route.continue();
      }
    });

    // Accept the confirm dialog
    page.once('dialog', async (dialog) => {
      await dialog.accept();
    });

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('.hal-icon-btn-danger[title="Remove device"]').click();

    await page.waitForTimeout(500);
    expect(deletePayload).not.toBeNull();
    expect(deletePayload.slot).toBe(7);
  });

  test('cancel remove keeps device in list', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // Dismiss the confirm dialog
    page.once('dialog', async (dialog) => {
      await dialog.dismiss();
    });

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('.hal-icon-btn-danger[title="Remove device"]').click();
    await page.waitForTimeout(300);

    // Device should still be visible
    await expect(page.locator('#hal-device-list')).toContainText('ES9822PRO');
    await expect(page.locator('.hal-device-card')).toHaveCount(11);
  });

  test('reinit shows success toast after completion', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    await page.route('/api/hal/devices/reinit', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', state: 'AVAILABLE' })
      });
    });
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'GET') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      } else {
        await route.continue();
      }
    });

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('button[title="Re-initialize"]').click();

    // Should show a success toast
    const toast = page.locator('.toast, .notification, [role="alert"]')
      .filter({ hasText: /re-initialized/i });
    await expect(toast.first()).toBeVisible({ timeout: 5000 });
  });

});
