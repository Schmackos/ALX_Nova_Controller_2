const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class DebugPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('debug'));
    await this.page.locator(SELECTORS.panel('debug')).waitFor({ state: 'visible' });
  }

  async filterByModule(moduleName) {
    const chip = this.page.locator(`${SELECTORS.moduleChips} .btn-chip:has-text("${moduleName}")`);
    await chip.click();
  }

  async clearModuleFilter() {
    await this.page.locator(`${SELECTORS.moduleChips} + .btn-chip-action, button:has-text("All")`).first().click();
  }

  async setLogLevel(level) {
    await this.page.locator(SELECTORS.logLevelFilter).selectOption(level);
  }

  async setSerialLogLevel(level) {
    await this.page.locator(SELECTORS.debugSerialLevel).selectOption(String(level));
  }

  async searchLogs(query) {
    await this.page.locator(SELECTORS.debugSearchInput).fill(query);
  }

  async clearSearch() {
    await this.page.locator(SELECTORS.debugSearchInput).fill('');
  }

  async clearLogs() {
    await this.page.locator('button:has-text("Clear")').click();
  }

  async togglePause() {
    await this.page.locator(SELECTORS.pauseBtn).click();
  }

  async toggleTimestampMode() {
    await this.page.locator(SELECTORS.timestampToggle).click();
  }

  async toggleHwStats() {
    const toggle = this.page.locator(SELECTORS.debugHwStatsToggle);
    await toggle.click({ force: true });
  }

  async toggleI2sMetrics() {
    const toggle = this.page.locator(SELECTORS.debugI2sMetricsToggle);
    await toggle.click({ force: true });
  }

  async toggleTaskMonitor() {
    const toggle = this.page.locator(SELECTORS.debugTaskMonitorToggle);
    await toggle.click({ force: true });
  }

  async expectLogEntry(text) {
    const console = this.page.locator(SELECTORS.debugConsole);
    await expect(console).toContainText(text);
  }

  async expectNoLogEntry(text) {
    const console = this.page.locator(SELECTORS.debugConsole);
    await expect(console).not.toContainText(text);
  }

  async expectEntryCount(count) {
    const entries = this.page.locator(`${SELECTORS.debugConsole} .log-entry`);
    await expect(entries).toHaveCount(count);
  }

  async expectCpuTotal(text) {
    await expect(this.page.locator(SELECTORS.cpuTotal)).toHaveText(text);
  }

  async expectCpuTemp(text) {
    await expect(this.page.locator(SELECTORS.cpuTemp)).toHaveText(text);
  }

  async expectHeapPercent(text) {
    await expect(this.page.locator(SELECTORS.heapPercent)).toHaveText(text);
  }
}

module.exports = DebugPage;
