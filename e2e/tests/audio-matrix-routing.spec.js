/**
 * audio-matrix-routing.spec.js — Audio routing matrix grid and presets.
 *
 * Tests for matrix rendering, cell interaction, presets, and save/load.
 */

const { test, expect } = require('../helpers/fixtures');
const { captureApiCall } = require('../helpers/ws-assertions');

test.describe('@audio @api Audio Matrix Routing', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await page.locator('.audio-subnav-btn[data-view="matrix"]').click();
    await expect(page.locator('#audio-sv-matrix')).toHaveClass(/active/);
    await expect(page.locator('#audio-matrix-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('matrix grid renders with correct dimensions', async ({ connectedPage: page }) => {
    // audioChannelMap has matrixInputs=16, matrixOutputs=16
    const rows = page.locator('.matrix-table tbody tr');
    await expect(rows).toHaveCount(16);

    const colHeaders = page.locator('.matrix-table thead th.matrix-col-hdr');
    await expect(colHeaders).toHaveCount(16);
  });

  test('matrix cell click sends API call', async ({ connectedPage: page }) => {
    const apiCapture = captureApiCall(page, '**/api/pipeline/matrix/cell', 'POST');
    await apiCapture.ready;

    // Click cell at out=2, in=0
    const cell = page.locator('.matrix-cell[data-out="2"][data-in="0"]');
    await cell.click();

    // The click opens a popup with gain slider; click "0 dB" button to set gain
    const popup = page.locator('#matrixGainPopup');
    await expect(popup).toBeVisible({ timeout: 2000 });
    const zeroDbBtn = popup.locator('button').filter({ hasText: '0 dB' });
    await zeroDbBtn.click();

    await apiCapture.expectCalled({ out: 2, in: 0, gainDb: 0 });
  });

  test('1:1 preset button sends correct matrix API calls', async ({ connectedPage: page }) => {
    let apiCalls = [];
    await page.route('**/api/pipeline/matrix/cell', async (route, request) => {
      let body = null;
      try { body = request.postDataJSON(); } catch { body = request.postData(); }
      apiCalls.push(body);
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'ok', gainLinear: 1.0 }) });
    });

    await page.locator('button[onclick="matrixPreset1to1()"]').first().click();

    // Wait for API calls to settle
    await page.waitForTimeout(500);
    // 1:1 preset sets diagonal cells to 0dB — should have 16 calls (one per diagonal)
    expect(apiCalls.length).toBe(16);
    // Verify at least the first diagonal cell
    const diag0 = apiCalls.find(c => c.out === 0 && c.in === 0);
    expect(diag0).toBeTruthy();
    expect(diag0.gainDb).toBe(0);
  });

  test('clear preset sends correct matrix API calls', async ({ connectedPage: page }) => {
    let apiCallCount = 0;
    await page.route('**/api/pipeline/matrix/cell', async (route) => {
      apiCallCount++;
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'ok', gainLinear: 0 }) });
    });

    await page.locator('button[onclick="matrixPresetClear()"]').first().click();

    // Wait for API calls to settle
    await page.waitForTimeout(500);
    // Clear sets all 16x16=256 cells to -96 dB
    expect(apiCallCount).toBe(256);
  });

  test('matrix save sends POST request', async ({ connectedPage: page }) => {
    const apiCapture = captureApiCall(page, '**/api/pipeline/matrix/save', 'POST');
    await apiCapture.ready;

    await page.locator('.matrix-presets button').filter({ hasText: 'Save' }).click();
    await apiCapture.expectCalled();
  });

  test('matrix load sends POST request', async ({ connectedPage: page }) => {
    // Intercept both the load POST and the subsequent GET for matrix data
    let loadCalled = false;
    await page.route('**/api/pipeline/matrix/load', async (route) => {
      loadCalled = true;
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"status":"ok"}' });
    });
    await page.route('**/api/pipeline/matrix', async (route, request) => {
      if (request.method() === 'GET') {
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ matrix: [] })
        });
      } else {
        await route.continue();
      }
    });

    await page.locator('.matrix-presets button').filter({ hasText: 'Load' }).click();
    await page.waitForTimeout(500);
    expect(loadCalled).toBe(true);
  });

  test('active cells visually highlighted with matrix-active class', async ({ connectedPage: page }) => {
    // From the fixture matrix, cells (0,0), (1,1), (2,2), (3,3) are 1.0 (active)
    const activeCell = page.locator('.matrix-cell.matrix-active[data-out="0"][data-in="0"]');
    await expect(activeCell).toBeVisible();

    // Cell (0,1) should NOT be active
    const inactiveCell = page.locator('.matrix-cell[data-out="0"][data-in="1"]');
    await expect(inactiveCell).not.toHaveClass(/matrix-active/);
  });

  test('input and output headers show correct names', async ({ connectedPage: page }) => {
    // Row headers include input device names (PCM1808 L, PCM1808 R, etc.)
    const firstRowHeader = page.locator('.matrix-table tbody tr').first().locator('.matrix-row-hdr');
    await expect(firstRowHeader).toContainText('PCM1808');

    // Column headers include output names (PCM5102A L, PCM5102A R, etc.)
    const firstColHeader = page.locator('.matrix-table thead th.matrix-col-hdr').first();
    await expect(firstColHeader).toContainText('PCM5102A');
  });

  test('matrix presets buttons are visible', async ({ connectedPage: page }) => {
    await expect(page.locator('button[onclick="matrixPreset1to1()"]').first()).toBeVisible();
    await expect(page.locator('button[onclick="matrixPresetClear()"]').first()).toBeVisible();
    await expect(page.locator('.matrix-presets button').filter({ hasText: 'Save' })).toBeVisible();
    await expect(page.locator('.matrix-presets button').filter({ hasText: 'Load' })).toBeVisible();
  });
});
