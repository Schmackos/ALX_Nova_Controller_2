/**
 * a11y-modals.spec.js — Accessibility scans for modals and dialogs.
 *
 * Scopes axe-core to the visible modal/dialog element. Only critical/serious
 * violations fail; moderate violations are logged as warnings.
 *
 * Disabled rules (embedded device UI — not actionable in these tests):
 *   - color-contrast: custom dark/light theme with design-token colours
 *   - meta-viewport: user-scalable=no is intentional for embedded device UI
 *   - label: dynamically created modal forms use visual-only labels
 *   - select-name: select elements use visual-only labels
 *   - region: modals are overlays, not in landmark regions
 */

const { test, expect } = require('../helpers/fixtures');
const AxeBuilder = require('@axe-core/playwright').default;

/** Rules disabled globally for embedded device UI scans */
const DISABLED_RULES = [
  'color-contrast',
  'meta-viewport',
  'label',
  'select-name',
  'region',
];

/**
 * Run an axe scan scoped to a selector and assert no critical/serious violations.
 */
async function expectModalAccessible(page, selector, label) {
  const results = await new AxeBuilder({ page })
    .include(selector)
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
      console.warn(`[a11y] Moderate in "${label}": ${v.id} — ${v.description}\n  ${nodes}`);
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
      `Accessibility violations in "${label}":\n\n${details}`
    ).toBe(0);
  }
}

test.describe('@a11y Accessibility Modal Scans', () => {

  test('Password change modal accessibility', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('settings'));
    await expect(page.locator('#settings')).toBeVisible({ timeout: 5000 });

    // Open the password change modal
    await page.locator('button[onclick="showPasswordChangeModal()"]').click();
    await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });

    await expectModalAccessible(page, '#passwordChangeModal', 'Password Change Modal');
  });

  test('Ethernet confirmation modal accessibility', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('wifi'));
    await expect(page.locator('#wifi')).toBeVisible({ timeout: 5000 });

    // Force the Ethernet confirmation modal visible
    const modalExists = await page.evaluate(() => {
      var modal = document.getElementById('ethConfirmModal');
      if (modal) {
        modal.style.display = 'flex';
        modal.classList.add('active');
        return true;
      }
      return false;
    });

    if (modalExists) {
      await expect(page.locator('#ethConfirmModal')).toBeVisible({ timeout: 3000 });
      await expectModalAccessible(page, '#ethConfirmModal', 'Ethernet Confirmation Modal');
    }
  });

  test('Device edit form accessibility', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Expand the first device card then click Edit to open inline form
    await deviceList.locator('.hal-device-header').first().click();
    await page.waitForTimeout(300);

    const editBtn = deviceList.locator('.hal-device-card.expanded .hal-icon-btn[title="Edit"]').first();
    if (await editBtn.count() > 0) {
      await editBtn.click();
      await expect(deviceList.locator('.hal-edit-form').first()).toBeVisible({ timeout: 3000 });

      // Scan scoped to the first (expanded) edit form
      const results = await new AxeBuilder({ page })
        .include('.hal-device-card.expanded .hal-edit-form')
        .withTags(['wcag2a', 'wcag2aa'])
        .disableRules(DISABLED_RULES)
        .analyze();

      const severe = results.violations.filter(
        v => v.impact === 'critical' || v.impact === 'serious'
      );
      const moderate = results.violations.filter(v => v.impact === 'moderate');
      if (moderate.length > 0) {
        for (const v of moderate) {
          console.warn(`[a11y] Moderate in "Device Edit Form": ${v.id} — ${v.description}`);
        }
      }
      if (severe.length > 0) {
        const details = severe.map(v =>
          `  [${v.impact}] ${v.id}: ${v.description} (${v.nodes.length} occurrences)`
        ).join('\n');
        expect(severe.length,
          `Accessibility violations in "Device Edit Form":\n\n${details}`
        ).toBe(0);
      }
    }
  });

  test('Custom device create modal accessibility', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await expect(page.locator('#devices')).toBeVisible({ timeout: 5000 });

    // Open the custom device create modal
    const modalOpened = await page.evaluate(() => {
      if (typeof halShowCustomCreate === 'function') {
        halShowCustomCreate();
        return true;
      }
      var modal = document.getElementById('halCustomCreateModal');
      if (modal) {
        modal.style.display = 'flex';
        return true;
      }
      return false;
    });

    if (modalOpened) {
      await expect(page.locator('#halCustomCreateModal')).toBeVisible({ timeout: 3000 });
      await expectModalAccessible(page, '#halCustomCreateModal', 'Custom Device Create Modal');
    }
  });

});
