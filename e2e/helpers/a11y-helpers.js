/**
 * Accessibility scan helpers using @axe-core/playwright.
 *
 * Usage:
 *   const { expectNoViolations } = require('./a11y-helpers');
 *   await expectNoViolations(page, 'Settings');
 */

const AxeBuilder = require('@axe-core/playwright').default;
const { expect } = require('@playwright/test');

/**
 * Run an axe accessibility scan on the page.
 *
 * @param {import('@playwright/test').Page} page
 * @param {object} options
 * @param {string|string[]} [options.include] - CSS selectors to scope the scan to
 * @param {string|string[]} [options.exclude] - CSS selectors to exclude from scan
 * @param {string[]} [options.tags]           - WCAG tag filter (default: ['wcag2a', 'wcag2aa'])
 * @returns {Promise<import('axe-core').AxeResults>}
 */
async function scanPage(page, options = {}) {
  let builder = new AxeBuilder({ page });

  if (options.include) {
    const includes = Array.isArray(options.include) ? options.include : [options.include];
    for (const sel of includes) {
      builder = builder.include(sel);
    }
  }

  if (options.exclude) {
    const excludes = Array.isArray(options.exclude) ? options.exclude : [options.exclude];
    for (const sel of excludes) {
      builder = builder.exclude(sel);
    }
  }

  const tags = options.tags || ['wcag2a', 'wcag2aa'];
  builder = builder.withTags(tags);

  return builder.analyze();
}

/**
 * Assert that no critical or serious accessibility violations exist on the page.
 * Moderate violations are logged as console warnings.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} tabName   - human-readable label for error messages
 * @param {object} [options] - forwarded to scanPage()
 */
async function expectNoViolations(page, tabName, options = {}) {
  const results = await scanPage(page, options);

  // Filter for critical and serious violations only
  const severe = results.violations.filter(
    v => v.impact === 'critical' || v.impact === 'serious'
  );

  // Log moderate violations as warnings
  const moderate = results.violations.filter(v => v.impact === 'moderate');
  if (moderate.length > 0) {
    for (const v of moderate) {
      const nodes = v.nodes.map(n => n.html).join('\n  ');
      console.warn(`[a11y] Moderate: ${v.id} — ${v.description}\n  ${nodes}`);
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

module.exports = {
  scanPage,
  expectNoViolations
};
