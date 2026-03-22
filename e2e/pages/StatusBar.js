/**
 * StatusBar — Page Object Model for the status bar component.
 *
 * Provides helpers to assert amp, WiFi, WS, and MQTT indicator states.
 */

const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class StatusBar extends BasePage {
  constructor(page) {
    super(page);
    this.ampIndicator = page.locator(SELECTORS.statusAmpIndicator);
    this.ampText = page.locator(SELECTORS.statusAmpText);
    this.wifiIndicator = page.locator(SELECTORS.statusWifiIndicator);
    this.wifiText = page.locator(SELECTORS.statusWifiText);
    this.wsIndicator = page.locator(SELECTORS.statusWsIndicator);
    this.mqttIndicator = page.locator(SELECTORS.statusMqttIndicator);
    this.mqttText = page.locator(SELECTORS.statusMqttText);
  }

  /**
   * Assert the amplifier indicator shows the expected state text.
   * @param {string} state — expected text, e.g. 'ON', 'OFF'
   */
  async expectAmpState(state) {
    await expect(this.ampText).toHaveText(state);
  }

  /**
   * Assert the WebSocket connection status indicator has the expected state.
   * Checks the indicator element's class list for the state value.
   * @param {string} state — expected state class, e.g. 'connected', 'disconnected'
   */
  async expectWsState(state) {
    await expect(this.wsIndicator).toHaveClass(new RegExp(state));
  }

  /**
   * Assert the WiFi indicator shows connected or disconnected state.
   * @param {boolean} connected — true for connected, false for disconnected
   */
  async expectWifiState(connected) {
    if (connected) {
      await expect(this.wifiText).not.toHaveText('Not Connected');
    } else {
      await expect(this.wifiText).toHaveText('Not Connected');
    }
  }

  /**
   * Assert the MQTT indicator shows the expected state text.
   * @param {string} state — expected text, e.g. 'Connected', 'Disconnected'
   */
  async expectMqttState(state) {
    await expect(this.mqttText).toHaveText(state);
  }
}

module.exports = StatusBar;
