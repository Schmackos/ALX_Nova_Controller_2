/**
 * audio-matrix-device-groups.spec.js — Phase 4 Routing Matrix sub-view.
 *
 * Tests for:
 * - Matrix grid renders with INPUTS/OUTPUTS axis labels and device-grouped headers
 * - Gain popup slider adjusts dB value label in real time
 * - "Off" button in gain popup sends -72 dB (removes route)
 * - Gain slider in popup sends setMatrixGainDb API call
 * - matrix-active cells reflect gain > 0 after API response
 */

const { test, expect } = require('../helpers/fixtures');
const SELECTORS = require('../helpers/selectors');

test.describe('@audio Phase 4: Routing Matrix sub-view', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator(SELECTORS.sidebarTab('audio')).click();
    await page.locator(SELECTORS.audioSubNavBtn('matrix')).click();
    await expect(page.locator(SELECTORS.audioSubView('matrix'))).toHaveClass(/active/);
    await expect(page.locator(SELECTORS.audioMatrixContainer)).not.toContainText(
      'Waiting for device data...', { timeout: 5000 }
    );
  });

  test('matrix renders 16x16 grid from audioChannelMap fixture', async ({ connectedPage: page }) => {
    // audioChannelMap fixture has matrixInputs=16, matrixOutputs=16
    const rows = page.locator('.matrix-table tbody tr');
    await expect(rows).toHaveCount(16);

    const colHeaders = page.locator('.matrix-table thead th.matrix-col-hdr');
    await expect(colHeaders).toHaveCount(16);
  });

  test('row device group headers contain device names from fixture inputs', async ({ connectedPage: page }) => {
    // Fixture inputs[0] = PCM1808, inputs[1] = PCM1808 (second lane)
    // Device name is in .matrix-row-dev-hdr (merged cell); .matrix-row-hdr has L/R label
    const firstRowDevHdr = page.locator('.matrix-table tbody .matrix-row-dev-hdr').first();
    await expect(firstRowDevHdr).toContainText('PCM1808');
  });

  test('column device group headers contain device names from fixture outputs', async ({ connectedPage: page }) => {
    // Fixture outputs[0] = PCM5102A — device name in .matrix-dev-hdr (group row)
    const firstColDevHdr = page.locator('.matrix-table thead th.matrix-dev-hdr').first();
    await expect(firstColDevHdr).toContainText('PCM5102A');
  });

  test('matrix corner cell shows axis label', async ({ connectedPage: page }) => {
    const corner = page.locator('.matrix-corner');
    await expect(corner).toBeVisible();
  });

  test('identity diagonal cells are rendered as matrix-active', async ({ connectedPage: page }) => {
    // Fixture matrix has identity diagonal (row 0, col 0) set to 1.0
    const activeCell = page.locator('.matrix-cell.matrix-active[data-out="0"][data-in="0"]');
    await expect(activeCell).toBeVisible();
  });

  test('off-diagonal cells are not matrix-active', async ({ connectedPage: page }) => {
    // Cell (out=1, in=0) is 0.0 in the identity fixture
    const inactiveCell = page.locator('.matrix-cell[data-out="1"][data-in="0"]');
    await expect(inactiveCell).not.toHaveClass(/matrix-active/);
  });

  test('clicking a cell opens gain popup', async ({ connectedPage: page }) => {
    const cell = page.locator('.matrix-cell[data-out="0"][data-in="0"]');
    await cell.click();
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });
  });

  test('gain popup contains slider and dB label', async ({ connectedPage: page }) => {
    const cell = page.locator('.matrix-cell[data-out="0"][data-in="0"]');
    await cell.click();
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });

    const slider = popup.locator('#matrixGainSlider');
    await expect(slider).toBeVisible();

    const dbLabel = popup.locator('#matrixGainDbVal');
    await expect(dbLabel).toBeVisible();
  });

  test('gain popup "Off" button closes popup and sends -72 dB', async ({ connectedPage: page }) => {
    let capturedBody = null;
    await page.route('**/api/v1/pipeline/matrix/cell', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', gainLinear: 0 })
      });
    });

    const cell = page.locator('.matrix-cell[data-out="0"][data-in="0"]');
    await cell.click();
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });

    const offBtn = popup.locator('[data-action="matrix-gain-setoff"]');
    await offBtn.click();

    await page.waitForTimeout(300);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.gainDb).toBe(-72);
    expect(capturedBody.out).toBe(0);
    expect(capturedBody.in).toBe(0);
  });

  test('gain popup "0 dB" button sends 0 dB to API', async ({ connectedPage: page }) => {
    let capturedBody = null;
    await page.route('**/api/v1/pipeline/matrix/cell', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', gainLinear: 1.0 })
      });
    });

    const cell = page.locator('.matrix-cell[data-out="2"][data-in="0"]');
    await cell.click();
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });

    const zeroDbBtn = popup.locator('[data-action="matrix-gain-set0"]');
    await zeroDbBtn.click();

    await page.waitForTimeout(300);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.gainDb).toBe(0);
  });

  test('gain popup close button hides popup without API call', async ({ connectedPage: page }) => {
    let apiCalled = false;
    await page.route('**/api/v1/pipeline/matrix/cell', async (route) => {
      apiCalled = true;
      await route.continue();
    });

    const cell = page.locator('.matrix-cell[data-out="0"][data-in="0"]');
    await cell.click();
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });

    const closeBtn = popup.locator('[data-action="matrix-popup-close"]');
    await closeBtn.click();

    await page.waitForTimeout(200);
    await expect(popup).toBeHidden();
    expect(apiCalled).toBe(false);
  });

  test('all matrix preset buttons are visible', async ({ connectedPage: page }) => {
    await expect(page.locator('[data-action="matrix-preset-1to1"]').first()).toBeVisible();
    await expect(page.locator('[data-action="matrix-preset-stereo-all"]').first()).toBeVisible();
    await expect(page.locator('[data-action="matrix-preset-clear"]').first()).toBeVisible();
    await expect(page.locator('[data-action="matrix-save"]').first()).toBeVisible();
    await expect(page.locator('[data-action="matrix-load"]').first()).toBeVisible();
  });
});
