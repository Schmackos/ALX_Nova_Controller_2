/**
 * WebSocket command capture and REST API capture helpers for Playwright E2E tests.
 *
 * Usage:
 *   const { expectWsCommand, clearWsCapture, captureApiCall } = require('./ws-assertions');
 *
 * Tests must populate page.wsCapture[] via their WS route handler:
 *   ws.onMessage(msg => { page.wsCapture.push(JSON.parse(msg)); server.send(msg); });
 */

const { expect } = require('@playwright/test');

/**
 * Wait for a specific WS command in page.wsCapture[].
 * Polls the array every 100ms until timeout.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} type           - expected command type
 * @param {object} expectedFields - subset of fields to match (shallow comparison)
 * @param {number} timeout        - max wait in ms (default 3000)
 * @returns {Promise<object>}     - the matching command object
 */
async function expectWsCommand(page, type, expectedFields = {}, timeout = 3000) {
  const deadline = Date.now() + timeout;
  const interval = 100;

  while (Date.now() < deadline) {
    const captures = page.wsCapture || [];
    const match = captures.find(cmd => {
      if (cmd.type !== type) return false;
      for (const [key, value] of Object.entries(expectedFields)) {
        if (typeof value === 'object' && value !== null) {
          // Deep-compare objects/arrays via JSON
          if (JSON.stringify(cmd[key]) !== JSON.stringify(value)) return false;
        } else if (cmd[key] !== value) {
          return false;
        }
      }
      return true;
    });

    if (match) return match;
    await new Promise(r => setTimeout(r, interval));
  }

  // Build a descriptive failure message
  const captures = page.wsCapture || [];
  const capturedTypes = captures.map(c => c.type);
  const matchingType = captures.filter(c => c.type === type);
  let detail = `Captured ${captures.length} command(s): [${capturedTypes.join(', ')}]`;
  if (matchingType.length > 0) {
    detail += `\nCommands with type "${type}":\n${JSON.stringify(matchingType, null, 2)}`;
  }
  if (Object.keys(expectedFields).length > 0) {
    detail += `\nExpected fields: ${JSON.stringify(expectedFields)}`;
  }

  expect(null, `Expected WS command "${type}" not found within ${timeout}ms.\n${detail}`).not.toBeNull();
}

/**
 * Assert that no WS command of a given type was sent.
 * Waits briefly to allow any pending messages to arrive.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} type    - command type that should NOT appear
 * @param {number} timeout - settling time in ms (default 500)
 */
async function expectNoWsCommand(page, type, timeout = 500) {
  await new Promise(r => setTimeout(r, timeout));
  const captures = page.wsCapture || [];
  const matches = captures.filter(c => c.type === type);
  if (matches.length > 0) {
    expect(matches.length,
      `Expected NO WS command "${type}" but found ${matches.length}:\n${JSON.stringify(matches, null, 2)}`
    ).toBe(0);
  }
}

/**
 * Return all WS commands of a specific type from wsCapture (synchronous).
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} type
 * @returns {object[]}
 */
function getWsCommands(page, type) {
  return (page.wsCapture || []).filter(c => c.type === type);
}

/**
 * Empty the page.wsCapture array.
 *
 * @param {import('@playwright/test').Page} page
 */
function clearWsCapture(page) {
  if (page.wsCapture) {
    page.wsCapture.length = 0;
  }
}

/**
 * Intercept a REST API call and capture the request body.
 *
 * Returns an object whose `expectCalled()` method polls for the captured
 * request and asserts on the body contents.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string|RegExp} urlPattern - URL pattern to intercept
 * @param {string} method            - HTTP method to match (default 'POST')
 * @returns {{ ready: Promise<void>, expectCalled: function }}
 */
function captureApiCall(page, urlPattern, method = 'POST') {
  let captured = null;

  const ready = page.route(urlPattern, async (route, request) => {
    if (request.method().toUpperCase() === method.toUpperCase()) {
      let body = null;
      try {
        body = request.postDataJSON();
      } catch {
        body = request.postData();
      }
      captured = {
        url: request.url(),
        method: request.method(),
        body,
        headers: request.headers()
      };
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"status":"ok"}' });
    } else {
      await route.continue();
    }
  });

  return {
    ready,

    /**
     * Assert the API call was made and optionally check body fields.
     *
     * @param {object} expectedBody - subset of body fields to match (optional)
     * @param {number} timeout      - max wait in ms (default 3000)
     * @returns {Promise<object>}   - the captured request
     */
    async expectCalled(expectedBody = null, timeout = 3000) {
      const deadline = Date.now() + timeout;
      const interval = 100;

      while (Date.now() < deadline) {
        if (captured) break;
        await new Promise(r => setTimeout(r, interval));
      }

      expect(captured,
        `Expected ${method} to ${urlPattern} to be called within ${timeout}ms, but it was not.`
      ).not.toBeNull();

      if (expectedBody !== null && typeof expectedBody === 'object') {
        for (const [key, value] of Object.entries(expectedBody)) {
          expect(captured.body[key],
            `API call body field "${key}" mismatch. Got: ${JSON.stringify(captured.body[key])}, expected: ${JSON.stringify(value)}`
          ).toEqual(value);
        }
      }

      return captured;
    }
  };
}

module.exports = {
  expectWsCommand,
  expectNoWsCommand,
  getWsCommands,
  clearWsCapture,
  captureApiCall
};
