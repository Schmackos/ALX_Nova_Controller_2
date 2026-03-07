/**
 * responsive.spec.js — Mobile viewport: bottom bar visible, sidebar hidden/collapsed.
 *
 * The app has a mobile bottom tab-bar (.tab-bar) that is visible at narrow widths
 * and a sidebar that collapses on mobile.
 */

const { test, expect } = require('../helpers/fixtures');

test('at 375x667 viewport bottom tab bar is visible and sidebar is off-screen', async ({ connectedPage: page }) => {
  // Resize to a typical mobile viewport
  await page.setViewportSize({ width: 375, height: 667 });

  // Allow CSS transitions to settle
  await page.waitForTimeout(300);

  // The bottom tab bar must be visible
  const tabBar = page.locator('.tab-bar');
  await expect(tabBar).toBeVisible();

  // Sidebar should be collapsed or off-screen at mobile width.
  // The sidebar CSS hides/collapses it — check it is not in view.
  const sidebar = page.locator('#sidebar');
  // Either not visible or outside the viewport — bounding box x should be negative
  const box = await sidebar.boundingBox();
  // Accept either truly hidden or slid off-screen (x + width <= 0)
  if (box !== null) {
    const rightEdge = box.x + box.width;
    // Sidebar is collapsed if its right edge is at or left of 0, or width is zero
    const isOffScreen = rightEdge <= 0 || box.width === 0;
    const isHidden = await sidebar.isHidden();
    expect(isOffScreen || isHidden).toBe(true);
  }
  // If bounding box is null the element is hidden — that also passes

  // Tab bar items for main tabs should all be present
  const tabs = tabBar.locator('.tab');
  const count = await tabs.count();
  expect(count).toBeGreaterThanOrEqual(6);
});
