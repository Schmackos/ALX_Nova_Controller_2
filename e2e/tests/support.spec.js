/**
 * support.spec.js — Support tab content renders with expected text and link.
 */

const { test, expect } = require('../helpers/fixtures');

test('support tab renders manual access card with QR code container and manual link', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="support"]').click();

  const supportPanel = page.locator('#support');
  await expect(supportPanel).toHaveClass(/active/);

  // Manual Access card is visible
  await expect(supportPanel.locator('.card').first()).toBeVisible();

  // QR code container is present (may be empty until loadManualContent() resolves)
  const qrContainer = page.locator('#manualQrCode');
  await expect(qrContainer).toBeVisible();

  // Manual link element exists
  const manualLink = page.locator('#manualLink');
  await expect(manualLink).toBeVisible();

  // Documentation card with search input
  const searchInput = page.locator('#manualSearchInput');
  await expect(searchInput).toBeVisible();
  await expect(searchInput).toHaveAttribute('placeholder', 'Search...');

  // Manual rendered container exists
  const manualRendered = page.locator('#manualRendered');
  await expect(manualRendered).toBeVisible();

  // Description text on the page
  await expect(supportPanel).toContainText('manual');
});
