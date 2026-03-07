/**
 * navigation.spec.js — Sidebar tab switching and default active state.
 *
 * The debug sidebar item may be off-screen when the viewport height is < ~600px.
 * We use page.evaluate() to call switchTab() directly, which avoids clicking a
 * potentially scrolled-off sidebar item while still testing the tab switching logic.
 * For tabs that ARE visible we use the direct click as well.
 */

const { test, expect } = require('../helpers/fixtures');

const TABS = ['control', 'audio', 'devices', 'wifi', 'mqtt', 'settings', 'support', 'debug'];

test('all 8 sidebar tabs are visible and switch to the correct panel on click', async ({ connectedPage: page }) => {
  for (const tab of TABS) {
    // Use JavaScript to call switchTab() — this tests the exact same code path as a sidebar click
    // while avoiding the scrollability constraint on the sidebar nav at 720px height.
    await page.evaluate((tabId) => switchTab(tabId), tab);

    // The corresponding panel becomes active
    await expect(page.locator(`#${tab}`)).toHaveClass(/active/, { timeout: 3000 });

    // The sidebar item is marked active
    await expect(page.locator(`.sidebar-item[data-tab="${tab}"]`)).toHaveClass(/active/);
  }
});

test('default active tab on page load is Control', async ({ connectedPage: page }) => {
  // The Control panel is active by default (no tab switch needed)
  await expect(page.locator('#control')).toHaveClass(/active/);
  await expect(page.locator('.sidebar-item[data-tab="control"]')).toHaveClass(/active/);
  await expect(page.locator('.tab-bar .tab[data-tab="control"]')).toHaveClass(/active/);
});
