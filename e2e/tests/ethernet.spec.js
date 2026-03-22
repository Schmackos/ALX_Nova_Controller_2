/**
 * ethernet.spec.js — Ethernet status, configuration, and backward compatibility.
 *
 * Tests the Network Overview card, Ethernet Status card, Ethernet Config card,
 * and verifies WiFi tab still works with Ethernet fields present.
 *
 * Note: Ethernet cards live inside the "wifi" tab (renamed to "Network" in sidebar).
 * #ethUseStaticIP is a <input type="checkbox"> inside <label class="switch">.
 * CSS hides the raw input — click the parent label or use page.evaluate to toggle.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('Ethernet Status', () => {
  test('Network Overview card shows WiFi as active when Ethernet disconnected', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const activeText = page.locator('#activeInterfaceText');
    await expect(activeText).toContainText('WiFi');
  });

  test('Network Overview card shows Ethernet as active when connected', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    // Send updated status with Ethernet connected
    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    const activeText = page.locator('#activeInterfaceText');
    await expect(activeText).toContainText('Ethernet', { timeout: 5000 });
  });

  test('Ethernet status card shows connected with IP and speed', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText('10.0.0.50', { timeout: 5000 });
    await expect(ethCard).toContainText('100');
  });

  test('Ethernet status shows no cable when link is down', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    // Default state has ethLinkUp: false
    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText(/No cable/i);
  });

  test('Ethernet status shows link up awaiting DHCP', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const linkOnly = require('../fixtures/ws-messages/wifi-status-eth-link-only.json');
    page.wsRoute.send(linkOnly);

    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText(/Awaiting DHCP/i, { timeout: 5000 });
  });

  test('MAC address is displayed in Ethernet status card', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    await expect(page.locator('#ethStatusCard')).toContainText('AA:BB:CC:DD:EE:FF', { timeout: 5000 });
  });

  test('Hostname displays correctly in Network Overview', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#networkHostnameDisplay')).toContainText('alx-nova');
  });

  test('Ethernet connected status shows gateway and subnet', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText('10.0.0.1', { timeout: 5000 });
    await expect(ethCard).toContainText('255.255.255.0');
  });

  test('Ethernet connected status shows DNS servers', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText('8.8.8.8', { timeout: 5000 });
    await expect(ethCard).toContainText('8.8.4.4');
  });

  test('Ethernet connected shows full duplex indicator', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    const ethStatus = require('../fixtures/ws-messages/wifi-status-eth-connected.json');
    page.wsRoute.send(ethStatus);

    const ethCard = page.locator('#ethStatusCard');
    await expect(ethCard).toContainText('Full Duplex', { timeout: 5000 });
  });
});

test.describe('Ethernet Configuration', () => {
  test('Static IP toggle shows and hides fields', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    // Fields should be hidden initially
    const fields = page.locator('#ethStaticIPFields');
    await expect(fields).not.toBeVisible();

    // Toggle checkbox via JS (CSS-hidden input, clicking label or using evaluate)
    await page.evaluate(() => {
      const cb = document.getElementById('ethUseStaticIP');
      cb.checked = true;
      cb.dispatchEvent(new Event('change'));
    });
    await expect(fields).toBeVisible();

    // Toggle back to hide
    await page.evaluate(() => {
      const cb = document.getElementById('ethUseStaticIP');
      cb.checked = false;
      cb.dispatchEvent(new Event('change'));
    });
    await expect(fields).not.toBeVisible();
  });

  test('Ethernet config card is present with hostname input', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#ethConfigCard')).toBeVisible();
    await expect(page.locator('#ethHostnameInput')).toBeVisible();
  });

  test('Static IP checkbox is attached in the DOM', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#ethUseStaticIP')).toBeAttached();
  });

  test('Hostname input shows default value', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#ethHostnameInput')).toHaveValue('alx-nova');
  });

  test('Static IP fields contain all required inputs when visible', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    // Enable static IP to reveal fields
    await page.evaluate(() => {
      const cb = document.getElementById('ethUseStaticIP');
      cb.checked = true;
      cb.dispatchEvent(new Event('change'));
    });

    await expect(page.locator('#ethStaticIP')).toBeVisible();
    await expect(page.locator('#ethSubnetInput')).toBeVisible();
    await expect(page.locator('#ethGatewayInput')).toBeVisible();
    await expect(page.locator('#ethDns1Input')).toBeVisible();
    await expect(page.locator('#ethDns2Input')).toBeVisible();
  });

  test('Subnet mask has default value', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await page.evaluate(() => {
      const cb = document.getElementById('ethUseStaticIP');
      cb.checked = true;
      cb.dispatchEvent(new Event('change'));
    });
    await expect(page.locator('#ethSubnetInput')).toHaveValue('255.255.255.0');
  });
});

test.describe('Ethernet WiFi backward compatibility', () => {
  test('WiFi status card still renders correctly with Ethernet fields present', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    // The regular WiFi status should still work
    const wifiStatus = page.locator('#wifiStatusBox');
    await expect(wifiStatus).toBeVisible();
  });

  test('Network Overview card is visible alongside WiFi status', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#networkOverviewCard')).toBeVisible();
    await expect(page.locator('#wifiStatusBox')).toBeVisible();
  });

  test('All three Ethernet cards are present on the Network tab', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));

    await expect(page.locator('#networkOverviewCard')).toBeVisible();
    await expect(page.locator('#ethStatusCard')).toBeVisible();
    await expect(page.locator('#ethConfigCard')).toBeVisible();
  });
});
