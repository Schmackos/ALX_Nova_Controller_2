const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');
const halDeviceFixture = require('../fixtures/ws-messages/hal-device-state.json');

class DevicesPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Navigate to the Devices tab and push the HAL device fixture
   * so the device list renders with deterministic data.
   */
  async open() {
    await this.switchTab('devices');
    this.wsSend(halDeviceFixture);
    await this.page.locator(SELECTORS.halDeviceList).waitFor({ state: 'visible' });
    await this.page.waitForTimeout(200);
  }

  /** All device card locators. */
  get cards() {
    return this.page.locator(SELECTORS.halDeviceCards);
  }

  /**
   * Locate a device card by its display name (userLabel or device name).
   * @param {string} name
   */
  cardByName(name) {
    return this.page.locator(SELECTORS.halDeviceCards)
      .filter({ has: this.page.locator('.hal-device-name', { hasText: name }) });
  }

  /**
   * Locate a device card by its slot number.
   * The slot is shown in the expanded details row "Slot: N".
   * Since slot is embedded in onclick handlers, we match via data inspection.
   * @param {number} slot
   */
  cardBySlot(slot) {
    // Each card's header has onclick="halToggleExpand(slot)" — match that pattern
    return this.page.locator(SELECTORS.halDeviceCards)
      .filter({ has: this.page.locator(`.hal-device-header[onclick="halToggleExpand(${slot})"]`) });
  }

  /** Click the Rescan Devices button. */
  async clickRescan() {
    await this.page.locator(SELECTORS.halRescanBtn).click();
  }

  /**
   * Click the edit button on a device card to open its edit form.
   * First expands the card, then clicks edit.
   * @param {string} deviceName
   */
  async openEditForm(deviceName) {
    const card = this.cardByName(deviceName);
    const editBtn = card.locator('.hal-icon-btn[title="Edit"]');
    await editBtn.click();
    await this.page.locator('.hal-edit-form').waitFor({ state: 'visible' });
  }

  /**
   * Close (cancel) the edit form on a device card.
   * @param {string} deviceName
   */
  async closeEditForm(deviceName) {
    // The edit form has a Cancel button via halCancelEdit()
    const card = this.cardByName(deviceName);
    const cancelBtn = card.locator('button', { hasText: 'Cancel' });
    await cancelBtn.click();
  }

  /**
   * Enable a device by checking its enable toggle.
   * @param {string} deviceName
   */
  async enableDevice(deviceName) {
    const card = this.cardByName(deviceName);
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    if (!(await toggle.isChecked())) {
      await toggle.click({ force: true });
    }
  }

  /**
   * Disable a device by unchecking its enable toggle.
   * @param {string} deviceName
   */
  async disableDevice(deviceName) {
    const card = this.cardByName(deviceName);
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    if (await toggle.isChecked()) {
      await toggle.click({ force: true });
    }
  }

  /**
   * Delete a device card. Handles the browser confirm() dialog.
   * @param {string} deviceName
   */
  async deleteDevice(deviceName) {
    const card = this.cardByName(deviceName);
    const removeBtn = card.locator('.hal-icon-btn-danger[title="Remove device"]');
    // Set up dialog handler before clicking
    this.page.once('dialog', (dialog) => dialog.accept());
    await removeBtn.click();
  }

  /**
   * Click the re-initialize button on a device card.
   * @param {string} deviceName
   */
  async reinitDevice(deviceName) {
    const card = this.cardByName(deviceName);
    const reinitBtn = card.locator('.hal-icon-btn[title="Re-initialize"]');
    await reinitBtn.click();
  }

  /**
   * Assert the state badge text on a device card.
   * @param {string} deviceName
   * @param {string} stateLabel — e.g. 'Available', 'Error', 'Unavailable'
   */
  async expectDeviceState(deviceName, stateLabel) {
    const card = this.cardByName(deviceName);
    const badge = card.locator('.hal-device-info .badge').first();
    await expect(badge).toHaveText(stateLabel);
  }

  /**
   * Assert the status dot color class on a device card.
   * @param {string} deviceName
   * @param {string} colorClass — e.g. 'green', 'red', 'amber', 'blue', 'grey'
   */
  async expectStatusDotColor(deviceName, colorClass) {
    const card = this.cardByName(deviceName);
    const dot = card.locator('.status-dot');
    await expect(dot).toHaveClass(new RegExp(`status-${colorClass}`));
  }

  /**
   * Assert the capacity indicator text shows count/max.
   * @param {number} count
   * @param {number} max
   */
  async expectCapacity(count, max) {
    const indicator = this.page.locator('#hal-capacity-indicator');
    await expect(indicator).toContainText(`Devices: ${count}/${max}`);
  }

  /**
   * Assert the number of visible device cards.
   * @param {number} count
   */
  async expectDeviceCount(count) {
    await expect(this.cards).toHaveCount(count);
  }

  /**
   * Push a WebSocket update for a specific device with a new state.
   * Clones the fixture and modifies the target device's state/ready fields.
   * @param {number} deviceSlot — the slot number to modify
   * @param {number} stateValue — new state enum value (1=Detected, 3=Available, 4=Unavailable, 5=Error)
   * @param {boolean} ready — new ready flag
   */
  pushDeviceWithState(deviceSlot, stateValue, ready) {
    const msg = JSON.parse(JSON.stringify(halDeviceFixture));
    const device = msg.devices.find((d) => d.slot === deviceSlot);
    if (device) {
      device.state = stateValue;
      device.ready = ready;
    }
    this.wsSend(msg);
  }
}

module.exports = DevicesPage;
