const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class MqttPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('mqtt'));
    await this.page.locator(SELECTORS.panel('mqtt')).waitFor({ state: 'visible' });
  }

  async toggleMqttEnabled() {
    const toggle = this.page.locator(SELECTORS.mqttEnabledToggle);
    await toggle.click({ force: true });
  }

  async setBrokerConfig(host, port, user, pass) {
    // Ensure MQTT is enabled and fields are visible
    const fields = this.page.locator(SELECTORS.mqttFields);

    if (host) {
      await this.page.locator(SELECTORS.mqttBrokerInput).fill(host);
    }
    if (port !== undefined && port !== null) {
      await this.page.locator(SELECTORS.mqttPortInput).fill(String(port));
    }
    if (user) {
      await this.page.locator(SELECTORS.mqttUsernameInput).fill(user);
    }
    if (pass) {
      await this.page.locator('#appState\\.mqttPassword').fill(pass);
    }
  }

  async setBaseTopic(topic) {
    await this.page.locator(SELECTORS.mqttBaseTopicInput).fill(topic);
  }

  async toggleHADiscovery() {
    const toggle = this.page.locator(SELECTORS.mqttHADiscovery);
    await toggle.click({ force: true });
  }

  async expectConnectionState(state) {
    const statusBox = this.page.locator(SELECTORS.mqttStatusBox);
    await expect(statusBox).toContainText(state);
  }

  async clickSave() {
    await this.page.locator('button:has-text("Save MQTT Settings")').click();
  }

  async expectMqttEnabled(enabled) {
    const toggle = this.page.locator(SELECTORS.mqttEnabledToggle);
    if (enabled) {
      await expect(toggle).toBeChecked();
    } else {
      await expect(toggle).not.toBeChecked();
    }
  }

  async expectMqttFieldsVisible(visible) {
    const locator = this.page.locator(SELECTORS.mqttFields);
    if (visible) {
      await expect(locator).toBeVisible();
    } else {
      await expect(locator).toBeHidden();
    }
  }

  async expectHADiscoveryChecked(checked) {
    const toggle = this.page.locator(SELECTORS.mqttHADiscovery);
    if (checked) {
      await expect(toggle).toBeChecked();
    } else {
      await expect(toggle).not.toBeChecked();
    }
  }
}

module.exports = MqttPage;
