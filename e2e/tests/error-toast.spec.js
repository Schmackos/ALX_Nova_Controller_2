/**
 * error-toast.spec.js — Toast notification behavior tests.
 *
 * The frontend uses a single #toast element with classes "toast show <type>".
 * showToast(message, type) sets text, adds "show" class, then removes it
 * after 3000ms. Since there is only one toast element, multiple calls
 * replace the current toast content.
 */

const { test, expect } = require('../helpers/fixtures');
const BasePage = require('../pages/BasePage');

test.describe('@error Toast Notifications', () => {
  test('toast appears on error event with correct class', async ({ connectedPage }) => {
    const page = connectedPage;
    const toast = page.locator('#toast');

    await test.step('trigger an error toast via showToast', async () => {
      await page.evaluate(() => {
        showToast('Something went wrong', 'error');
      });
    });

    await test.step('verify toast is visible with error styling', async () => {
      await expect(toast).toHaveClass(/show/, { timeout: 3000 });
      await expect(toast).toHaveClass(/error/);
      await expect(toast).toHaveText('Something went wrong');
    });
  });

  test('toast auto-dismisses after timeout', async ({ connectedPage }) => {
    const page = connectedPage;
    const toast = page.locator('#toast');

    await test.step('show a toast', async () => {
      await page.evaluate(() => {
        showToast('Temporary message', 'info');
      });
      await expect(toast).toHaveClass(/show/, { timeout: 2000 });
    });

    await test.step('wait for auto-dismiss (3000ms timeout in showToast)', async () => {
      // showToast removes the "show" class after 3000ms
      // Wait slightly longer than 3s to account for timing
      await expect(toast).not.toHaveClass(/show/, { timeout: 5000 });
    });
  });

  test('successive toasts replace previous content', async ({ connectedPage }) => {
    const page = connectedPage;
    const toast = page.locator('#toast');

    // The frontend has a single #toast element — calling showToast again
    // replaces the text and resets the class. There is no stacking mechanism.

    await test.step('show first toast', async () => {
      await page.evaluate(() => {
        showToast('First message', 'error');
      });
      await expect(toast).toHaveClass(/show/, { timeout: 2000 });
      await expect(toast).toHaveText('First message');
    });

    await test.step('show second toast — replaces first', async () => {
      await page.evaluate(() => {
        showToast('Second message', 'success');
      });
      await expect(toast).toHaveClass(/show/, { timeout: 2000 });
      await expect(toast).toHaveClass(/success/);
      await expect(toast).toHaveText('Second message');
      // The first message text should no longer be visible
      await expect(toast).not.toHaveText('First message');
    });

    await test.step('show third toast — replaces second', async () => {
      await page.evaluate(() => {
        showToast('Third message', 'warning');
      });
      await expect(toast).toHaveClass(/show/, { timeout: 2000 });
      await expect(toast).toHaveClass(/warning/);
      await expect(toast).toHaveText('Third message');
    });
  });

  test('toast shows correct message text for different types', async ({ connectedPage }) => {
    const page = connectedPage;
    const toast = page.locator('#toast');

    const types = [
      { type: 'success', message: 'Settings saved successfully' },
      { type: 'error', message: 'Connection failed: auth error' },
      { type: 'warning', message: 'WebSocket client limit nearly reached' },
      { type: 'info', message: 'Device reconnected' },
    ];

    for (const { type, message } of types) {
      await test.step(`show "${type}" toast with correct message`, async () => {
        await page.evaluate(({ msg, t }) => {
          showToast(msg, t);
        }, { msg: message, t: type });

        await expect(toast).toHaveClass(/show/, { timeout: 2000 });
        await expect(toast).toHaveClass(new RegExp(type));
        await expect(toast).toHaveText(message);

        // Wait for dismiss before next iteration to avoid overlap
        await page.waitForTimeout(100);
      });
    }
  });
});
