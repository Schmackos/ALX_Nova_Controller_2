const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');

class PeqOverlay extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Open the PEQ overlay for a given output channel index.
   * Calls the frontend openOutputPeq() function.
   * @param {number} channel — the firstChannel value of the output
   */
  async open(channel) {
    await this.page.evaluate((ch) => openOutputPeq(ch), channel);
    await this.page.locator('#peqOverlay').waitFor({ state: 'visible' });
  }

  /**
   * Open the PEQ overlay for an input lane.
   * @param {number} lane — the input lane index
   */
  async openForInput(lane) {
    await this.page.evaluate((l) => openInputPeq(l), lane);
    await this.page.locator('#peqOverlay').waitFor({ state: 'visible' });
  }

  /**
   * Close the PEQ overlay.
   */
  async close() {
    await this.page.locator('.peq-overlay-close').click();
    await this.page.locator('#peqOverlay').waitFor({ state: 'hidden' });
  }

  /**
   * Click the "Add Band" button to add a new EQ band.
   */
  async addBand() {
    await this.page.locator('.peq-overlay-actions button', { hasText: 'Add Band' }).click();
  }

  /**
   * Remove a band by clicking its delete button.
   * @param {number} index — zero-based band index
   */
  async removeBand(index) {
    const row = this.page.locator(`#peqBandRows tr[data-band="${index}"]`);
    await row.locator('button').click();
  }

  /**
   * Set the filter type for a band via its select dropdown.
   * @param {number} index — zero-based band index
   * @param {string|number} type — filter type ID (0=LPF, 1=HPF, 2=BPF, 3=Notch, 4=Peak, 5=Lo Shelf, 6=Hi Shelf)
   */
  async setBandType(index, type) {
    const row = this.page.locator(`#peqBandRows tr[data-band="${index}"]`);
    await row.locator('.peq-type-sel').selectOption(String(type));
  }

  /**
   * Set the frequency for a band.
   * @param {number} index — zero-based band index
   * @param {number} hz — frequency in Hz (20-20000)
   */
  async setBandFreq(index, hz) {
    const row = this.page.locator(`#peqBandRows tr[data-band="${index}"]`);
    // Frequency is the first number input (after the type select)
    const freqInput = row.locator('input[type="number"]').nth(0);
    await freqInput.fill(String(hz));
    await freqInput.dispatchEvent('change');
  }

  /**
   * Set the gain for a band.
   * @param {number} index — zero-based band index
   * @param {number} db — gain in dB (-24 to +24)
   */
  async setBandGain(index, db) {
    const row = this.page.locator(`#peqBandRows tr[data-band="${index}"]`);
    // Gain is the second number input
    const gainInput = row.locator('input[type="number"]').nth(1);
    await gainInput.fill(String(db));
    await gainInput.dispatchEvent('change');
  }

  /**
   * Set the Q factor for a band.
   * @param {number} index — zero-based band index
   * @param {number} q — Q factor (0.1 to 30)
   */
  async setBandQ(index, q) {
    const row = this.page.locator(`#peqBandRows tr[data-band="${index}"]`);
    // Q is the third number input
    const qInput = row.locator('input[type="number"]').nth(2);
    await qInput.fill(String(q));
    await qInput.dispatchEvent('change');
  }

  /**
   * Click the Apply button to send PEQ changes to the firmware.
   */
  async apply() {
    await this.page.locator('.peq-overlay-actions button', { hasText: 'Apply' }).click();
  }

  /**
   * Click the Reset All button to clear all bands.
   */
  async resetAll() {
    await this.page.locator('.peq-overlay-actions button', { hasText: 'Reset All' }).click();
  }

  /**
   * Save the current PEQ config as a preset.
   * Note: Preset save/load is handled via the DSP preset system, not within the PEQ overlay itself.
   * This method triggers the Apply action which persists the config to firmware.
   * @param {string} _name — reserved for future preset naming support
   */
  async savePreset(_name) {
    await this.apply();
  }

  /**
   * Load a preset.
   * Note: Preset loading is handled via the DSP system outside the PEQ overlay.
   * This is a placeholder that opens the overlay fresh (presets auto-load from firmware state).
   * @param {string} _name — reserved for future preset selection support
   */
  async loadPreset(_name) {
    // Presets are loaded from firmware when the overlay opens;
    // re-opening fetches the current config from /api/output/dsp
  }

  /**
   * Assert the number of EQ bands currently displayed.
   * @param {number} count
   */
  async expectBandCount(count) {
    const rows = this.page.locator('#peqBandRows tr');
    await expect(rows).toHaveCount(count);
  }
}

module.exports = PeqOverlay;
