const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class AudioMatrixPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Returns a locator for the matrix cell at the given input/output intersection.
   * Matrix cells use data-out (column = output channel) and data-in (row = input channel).
   * @param {number} inputCh — input channel (row index in matrix)
   * @param {number} outputCh — output channel (column index in matrix)
   * @returns {import('@playwright/test').Locator}
   */
  getCell(inputCh, outputCh) {
    return this.page.locator(`.matrix-cell[data-out="${outputCh}"][data-in="${inputCh}"]`);
  }

  /**
   * Click a matrix cell to toggle or open the gain popup.
   * @param {number} inputCh — input channel (row)
   * @param {number} outputCh — output channel (column)
   */
  async toggleCell(inputCh, outputCh) {
    await this.getCell(inputCh, outputCh).click();
  }

  /**
   * Apply a matrix preset by clicking the corresponding button.
   * @param {'1:1'|'clear'} presetName
   */
  async applyPreset(presetName) {
    const container = this.page.locator('.matrix-presets');
    switch (presetName) {
      case '1:1':
        await container.locator('button', { hasText: '1:1 Pass' }).click();
        break;
      case 'clear':
        await container.locator('button', { hasText: 'Clear All' }).click();
        break;
      default:
        throw new Error(`Unknown matrix preset: ${presetName}`);
    }
  }

  /**
   * Click the Save button in the matrix presets row.
   */
  async saveMatrix() {
    await this.page.locator('.matrix-presets button', { hasText: 'Save' }).click();
  }

  /**
   * Click the Load button in the matrix presets row.
   */
  async loadMatrix() {
    await this.page.locator('.matrix-presets button', { hasText: 'Load' }).click();
  }

  /**
   * Assert whether a matrix cell is active (has routing enabled).
   * Active cells have the 'matrix-active' CSS class.
   * @param {number} inputCh
   * @param {number} outputCh
   * @param {boolean} active
   */
  async expectCellActive(inputCh, outputCh, active) {
    const cell = this.getCell(inputCh, outputCh);
    if (active) {
      await expect(cell).toHaveClass(/matrix-active/);
    } else {
      await expect(cell).not.toHaveClass(/matrix-active/);
    }
  }

  /**
   * Assert the matrix table dimensions (rows x columns, excluding header row/column).
   * @param {number} inputs — expected number of input rows
   * @param {number} outputs — expected number of output columns
   */
  async expectMatrixSize(inputs, outputs) {
    // Rows = tbody tr count (each row is one input channel)
    const rows = this.page.locator('.matrix-table tbody tr');
    await expect(rows).toHaveCount(inputs);

    // Columns = th count in thead minus the empty corner th
    const cols = this.page.locator('.matrix-table thead th.matrix-col-hdr');
    await expect(cols).toHaveCount(outputs);
  }
}

module.exports = AudioMatrixPage;
