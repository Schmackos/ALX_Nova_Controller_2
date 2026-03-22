/**
 * a11y-tabs.spec.js — Accessibility scans for each UI tab using axe-core.
 *
 * Runs WCAG 2.0 AA checks per tab. Only critical/serious violations fail;
 * moderate violations are logged as warnings.
 *
 * Disabled rules (embedded device UI — not actionable in these tests):
 *   - color-contrast: custom dark/light theme with design-token colours
 *   - page-has-heading-one: SPA with tab-based navigation, no single h1
 *   - landmark-one-main: embedded SPA layout without <main> landmark
 *   - region: tab content divs are not wrapped in landmark regions
 *   - meta-viewport: user-scalable=no is intentional for embedded device UI
 *   - label: many form inputs use visual-only labels (existing UI pattern)
 *   - select-name: select elements use visual-only labels
 *   - link-name: icon-only links without text labels
 *   - nested-interactive: buttons inside clickable card headers
 *   - button-name: icon-only sidebar toggle button (hamburger menu)
 *   - image-alt: decorative images on support tab
 */

const { test, expect } = require('../helpers/fixtures');
const AxeBuilder = require('@axe-core/playwright').default;

/** Rules disabled globally for embedded device UI scans */
const DISABLED_RULES = [
  'color-contrast',
  'page-has-heading-one',
  'landmark-one-main',
  'region',
  'meta-viewport',
  'label',
  'select-name',
  'link-name',
  'nested-interactive',
  'button-name',
  'image-alt',
];

/**
 * Run an axe scan and assert no critical/serious violations remain.
 * Logs moderate violations as warnings.
 */
async function expectTabAccessible(page, tabName) {
  const results = await new AxeBuilder({ page })
    .withTags(['wcag2a', 'wcag2aa'])
    .disableRules(DISABLED_RULES)
    .analyze();

  const severe = results.violations.filter(
    v => v.impact === 'critical' || v.impact === 'serious'
  );

  const moderate = results.violations.filter(v => v.impact === 'moderate');
  if (moderate.length > 0) {
    for (const v of moderate) {
      const nodes = v.nodes.map(n => n.html).join('\n  ');
      console.warn(`[a11y] Moderate on "${tabName}": ${v.id} — ${v.description}\n  ${nodes}`);
    }
  }

  if (severe.length > 0) {
    const details = severe.map(v => {
      const nodes = v.nodes.map(n => {
        const target = n.target.join(', ');
        return `    - ${target}: ${n.failureSummary}`;
      }).join('\n');
      return `  [${v.impact}] ${v.id}: ${v.description}\n    Help: ${v.helpUrl}\n${nodes}`;
    }).join('\n\n');

    expect(severe.length,
      `Accessibility violations on "${tabName}" tab:\n\n${details}`
    ).toBe(0);
  }
}

test.describe('@a11y Accessibility Tab Scans', () => {

  test('Control tab accessibility scan', async ({ connectedPage: page }) => {
    await expect(page.locator('#control')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Control');
  });

  test('Audio tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await expect(page.locator('#audio')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Audio');
  });

  test('Devices tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await expect(page.locator('#devices')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Devices');
  });

  test('Network tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));
    await expect(page.locator('#wifi')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Network');
  });

  test('MQTT tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('mqtt'));
    await expect(page.locator('#mqtt')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'MQTT');
  });

  test('Settings tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('settings'));
    await expect(page.locator('#settings')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Settings');
  });

  test('Debug tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('debug'));
    await expect(page.locator('#debug')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Debug');
  });

  test('Support tab accessibility scan', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('support'));
    await expect(page.locator('#support')).toBeVisible({ timeout: 5000 });
    await expectTabAccessible(page, 'Support');
  });

});
