/**
 * dark-mode.spec.js — Night mode toggle adds class to body and persists to localStorage.
 *
 * Note: The #darkModeToggle is a <input type="checkbox"> inside a <label class="switch">.
 * CSS hides the raw input (opacity:0; width:0; height:0) — the visual toggle is the .slider span.
 * We click the parent label to activate the toggle, and assert state with toBeChecked().
 */

const { test, expect } = require('../helpers/fixtures');

test('dark mode toggle adds night-mode class to body and persists to localStorage', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  // The toggle input itself is CSS-hidden; locate it and its parent label
  const darkModeToggle = page.locator('#darkModeToggle');
  // The label wrapping the toggle is the clickable element
  const darkModeLabel = page.locator('label.switch:has(#darkModeToggle)');
  await expect(darkModeLabel).toBeVisible();

  // Initial state: dark mode is off (no night-mode class)
  await expect(page.locator('body')).not.toHaveClass(/night-mode/);
  await expect(darkModeToggle).not.toBeChecked();

  // Enable dark mode by clicking the label (the visible toggle knob)
  await darkModeLabel.click();

  // body must have night-mode class
  await expect(page.locator('body')).toHaveClass(/night-mode/, { timeout: 2000 });
  await expect(darkModeToggle).toBeChecked();

  // localStorage should persist the preference
  const stored = await page.evaluate(() => localStorage.getItem('darkMode'));
  expect(stored).toBe('true');

  // Disable dark mode again
  await darkModeLabel.click();
  await expect(page.locator('body')).not.toHaveClass(/night-mode/, { timeout: 2000 });

  const storedAfter = await page.evaluate(() => localStorage.getItem('darkMode'));
  expect(storedAfter).toBe('false');
});
