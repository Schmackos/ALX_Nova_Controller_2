/**
 * BasePage — abstract base Page Object Model class for ALX Nova E2E tests.
 *
 * Provides shared helpers for tab navigation, WebSocket interaction,
 * request interception, toast assertions, and screenshot comparison.
 */

const { expect } = require('@playwright/test');

class BasePage {
  /**
   * @param {import('@playwright/test').Page} page
   */
  constructor(page) {
    this.page = page;
  }

  /**
   * Switch to a tab by name using the frontend's switchTab() function.
   * Avoids scroll/click issues with sidebar navigation.
   * @param {string} tabName
   */
  async switchTab(tabName) {
    await this.page.evaluate((tab) => switchTab(tab), tabName);
    await this.page.waitForTimeout(200);
  }

  /**
   * Send a JSON object through the mocked WebSocket route.
   * @param {object} obj — JSON-serializable message
   */
  wsSend(obj) {
    this.page.wsRoute.send(obj);
  }

  /**
   * Send a binary buffer through the mocked WebSocket route.
   * @param {Buffer} buf
   */
  wsSendBinary(buf) {
    this.page.wsRoute.sendBinary(buf);
  }

  /**
   * Intercept a network request matching a URL pattern and HTTP method.
   * Returns a promise that resolves with the captured request data.
   * @param {string|RegExp} urlPattern
   * @param {string} [method='POST']
   * @returns {Promise<{url: string, method: string, postData: any, headers: Record<string,string>}>}
   */
  interceptRequest(urlPattern, method = 'POST') {
    return new Promise((resolve) => {
      this.page.on('request', function handler(request) {
        const matchesUrl = typeof urlPattern === 'string'
          ? request.url().includes(urlPattern)
          : urlPattern.test(request.url());
        if (matchesUrl && request.method() === method) {
          this.page.off('request', handler);
          let postData = request.postData();
          try {
            postData = JSON.parse(postData);
          } catch {
            // keep as string
          }
          resolve({
            url: request.url(),
            method: request.method(),
            postData,
            headers: request.headers(),
          });
        }
      }.bind(this));
    });
  }

  /**
   * Assert that a toast notification with the given text appears.
   * @param {string} text — expected text content (substring match)
   * @param {number} [timeout=5000]
   */
  async expectToast(text, timeout = 5000) {
    const toast = this.page.locator('.toast, .notification, [role="alert"]')
      .filter({ hasText: text });
    await expect(toast.first()).toBeVisible({ timeout });
  }

  /**
   * Take an element-level screenshot and compare against a stored baseline.
   * @param {string} selector — CSS selector for the element
   * @param {string} name — screenshot baseline name (e.g. 'status-bar-connected')
   */
  async screenshotElement(selector, name) {
    const element = this.page.locator(selector);
    await expect(element).toHaveScreenshot(`${name}.png`);
  }

  /**
   * Return the captured WebSocket messages sent by the frontend.
   * @returns {Array<object>}
   */
  getWsCapture() {
    return this.page.wsCapture || [];
  }

  /**
   * Wait for a matching WebSocket command to appear in the capture array.
   * Polls the capture array until a message with the given type (and optional
   * field values) is found, or the timeout expires.
   * @param {string} type — expected message type
   * @param {object} [fields={}] — additional fields to match
   * @param {number} [timeout=5000]
   * @returns {Promise<object>} the matched message
   */
  async expectWsCommand(type, fields = {}, timeout = 5000) {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      const capture = this.getWsCapture();
      const match = capture.find((msg) => {
        if (msg.type !== type) return false;
        for (const [key, value] of Object.entries(fields)) {
          if (msg[key] !== value) return false;
        }
        return true;
      });
      if (match) return match;
      await this.page.waitForTimeout(100);
    }
    throw new Error(
      `Expected WS command {type: "${type}", ${JSON.stringify(fields)}} not found within ${timeout}ms. ` +
      `Captured: ${JSON.stringify(this.getWsCapture().map((m) => m.type))}`
    );
  }

  /**
   * Assert that NO WebSocket command with the given type exists in the capture.
   * @param {string} type
   */
  expectNoWsCommand(type) {
    const capture = this.getWsCapture();
    const match = capture.find((msg) => msg.type === type);
    if (match) {
      throw new Error(
        `Unexpected WS command {type: "${type}"} found in capture: ${JSON.stringify(match)}`
      );
    }
  }
}

module.exports = BasePage;
