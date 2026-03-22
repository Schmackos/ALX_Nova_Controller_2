const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class NetworkPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('wifi'));
    await this.page.locator(SELECTORS.panel('wifi')).waitFor({ state: 'visible' });
  }

  async expectWifiStatus(ssid, ip) {
    const statusBox = this.page.locator(SELECTORS.wifiStatusBox);
    if (ssid) {
      await expect(statusBox).toContainText(ssid);
    }
    if (ip) {
      await expect(statusBox).toContainText(ip);
    }
  }

  async expectEthStatus(ip) {
    const statusBox = this.page.locator(SELECTORS.ethStatusBox);
    if (ip) {
      await expect(statusBox).toContainText(ip);
    }
  }

  async expectEthLinkStatus(text) {
    await expect(this.page.locator(SELECTORS.ethLinkStatusText)).toHaveText(text);
  }

  async expectActiveInterface(text) {
    await expect(this.page.locator(SELECTORS.activeInterfaceText)).toHaveText(text);
  }

  async setHostname(name) {
    const input = this.page.locator(SELECTORS.ethHostnameInput);
    await input.fill(name);
    // Click the Save button next to the hostname input
    await this.page.locator('#ethConfigCard button:has-text("Save")').click();
  }

  async clickWifiScan() {
    await this.page.locator(SELECTORS.scanBtn).click();
  }

  async setWifiCredentials(ssid, password) {
    await this.page.locator(SELECTORS.wifiSsidInput).fill(ssid);
    await this.page.locator(SELECTORS.wifiPasswordInput).fill(password);
  }

  async setStaticIP(ip, subnet, gateway, dns) {
    await this.page.locator('#staticIP').fill(ip);
    if (subnet) {
      await this.page.locator('#subnet').fill(subnet);
    }
    if (gateway) {
      await this.page.locator('#gateway').fill(gateway);
    }
    if (dns) {
      await this.page.locator('#dns1').fill(dns);
    }
  }

  async toggleStaticIP() {
    const toggle = this.page.locator(SELECTORS.useStaticIPToggle);
    await toggle.click({ force: true });
  }

  async setEthStaticIP(ip, subnet, gateway, dns) {
    await this.page.locator(SELECTORS.ethStaticIP).fill(ip);
    if (subnet) {
      await this.page.locator(SELECTORS.ethSubnetInput).fill(subnet);
    }
    if (gateway) {
      await this.page.locator(SELECTORS.ethGatewayInput).fill(gateway);
    }
    if (dns) {
      await this.page.locator(SELECTORS.ethDns1Input).fill(dns);
    }
  }

  async toggleEthStaticIP() {
    const toggle = this.page.locator(SELECTORS.ethUseStaticIP);
    await toggle.click({ force: true });
  }

  async toggleAPMode() {
    const toggle = this.page.locator(SELECTORS.apToggle);
    await toggle.click({ force: true });
  }

  async expectStaticIPFieldsVisible(visible) {
    const locator = this.page.locator(SELECTORS.staticIPFields);
    if (visible) {
      await expect(locator).toBeVisible();
    } else {
      await expect(locator).toBeHidden();
    }
  }
}

module.exports = NetworkPage;
