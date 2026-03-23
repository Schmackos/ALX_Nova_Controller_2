/**
 * settings-password.spec.js -- Password change modal: open, validation, submit, cancel.
 *
 * The modal is dynamically created by showPasswordChangeModal() and appended to <body>.
 * Password change calls POST /api/auth/change with { currentPassword, newPassword }.
 * HTML5 minlength="8" on new/confirm inputs triggers browser-native validation.
 * currentPassword field is hidden (and not required) when _isDefaultPassword is true.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@settings @api Settings Password', () => {

  test('password modal opens from settings tab', async ({ connectedPage: page }) => {
    await test.step('Navigate to settings and click Change Password', async () => {
      await page.evaluate(() => switchTab('settings'));
      const btn = page.locator('button[onclick="showPasswordChangeModal()"]');
      await expect(btn).toBeVisible();
      await btn.click();
    });

    await test.step('Modal is visible with expected fields', async () => {
      const modal = page.locator('#passwordChangeModal');
      await expect(modal).toBeVisible({ timeout: 3000 });
      await expect(page.locator('#newPassword')).toBeVisible();
      await expect(page.locator('#confirmPassword')).toBeVisible();
    });
  });

  test('mismatched passwords show error', async ({ connectedPage: page }) => {
    await test.step('Open modal', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Fill mismatched passwords (>= 8 chars)', async () => {
      await page.locator('#newPassword').fill('password123');
      await page.locator('#confirmPassword').fill('differentpassword');
    });

    await test.step('Submit and verify error', async () => {
      await page.locator('#passwordChangeModal button[type="submit"]').click();
      const errorDiv = page.locator('#passwordError');
      await expect(errorDiv).toBeVisible({ timeout: 2000 });
      await expect(errorDiv).toContainText('do not match');
    });
  });

  test('short password shows error via JS validation', async ({ connectedPage: page }) => {
    await test.step('Open modal', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Fill short matching passwords and submit via JS', async () => {
      // Fill both fields with a short password that matches
      // Bypass browser HTML5 validation by calling changePassword() directly
      await page.locator('#newPassword').fill('short');
      await page.locator('#confirmPassword').fill('short');
      // Call the JS function directly to bypass HTML5 minlength validation
      await page.evaluate(() => changePassword());
    });

    await test.step('Verify error message about minimum length', async () => {
      const errorDiv = page.locator('#passwordError');
      await expect(errorDiv).toBeVisible({ timeout: 2000 });
      await expect(errorDiv).toContainText('at least 8');
    });
  });

  test('missing current password shows error', async ({ connectedPage: page }) => {
    await test.step('Open modal and force non-default-password state', async () => {
      await page.evaluate(() => { _isDefaultPassword = false; });
      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Fill new passwords without current password and submit via JS', async () => {
      // Leave currentPassword empty, fill new and confirm
      const currentPwdInput = page.locator('#currentPassword');
      if (await currentPwdInput.count() > 0) {
        await currentPwdInput.fill('');
      }
      await page.locator('#newPassword').fill('newpassword123');
      await page.locator('#confirmPassword').fill('newpassword123');
      await page.evaluate(() => changePassword());
    });

    await test.step('Verify current password required error', async () => {
      const errorDiv = page.locator('#passwordError');
      await expect(errorDiv).toBeVisible({ timeout: 2000 });
      await expect(errorDiv).toContainText('Current password is required');
    });
  });

  test('matching valid password sends POST /api/auth/change', async ({ connectedPage: page }) => {
    await test.step('Open modal', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });
    });

    let changeCalled = false;
    let requestBody = null;

    await test.step('Intercept /api/auth/change and fill valid passwords', async () => {
      await page.route('**/api/auth/change', async (route) => {
        changeCalled = true;
        const req = route.request();
        requestBody = JSON.parse(req.postData());
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ success: true }),
        });
      });

      // Fill current password (required when not on default password)
      const currentPwdInput = page.locator('#currentPassword');
      if (await currentPwdInput.count() > 0 && await currentPwdInput.isVisible()) {
        await currentPwdInput.fill('currentpassword');
      }
      await page.locator('#newPassword').fill('newpassword123');
      await page.locator('#confirmPassword').fill('newpassword123');
    });

    await test.step('Submit and verify API was called with newPassword', async () => {
      await page.locator('#passwordChangeModal button[type="submit"]').click();
      await page.waitForTimeout(500);
      expect(changeCalled).toBe(true);
      expect(requestBody.newPassword).toBe('newpassword123');
    });
  });

  test('cancel button closes modal without submitting', async ({ connectedPage: page }) => {
    let changeCalled = false;

    await test.step('Set up route intercept and open modal', async () => {
      await page.route('**/api/auth/change', async (route) => {
        changeCalled = true;
        await route.fulfill({ status: 200, body: JSON.stringify({ success: true }) });
      });

      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });
    });

    await test.step('Fill passwords and click cancel', async () => {
      await page.locator('#newPassword').fill('newpassword123');
      await page.locator('#confirmPassword').fill('newpassword123');
      // Click the Cancel button (type="button", not submit)
      await page.locator('#passwordChangeModal button[type="button"]').click();
    });

    await test.step('Modal is gone and API was not called', async () => {
      await expect(page.locator('#passwordChangeModal')).toHaveCount(0, { timeout: 3000 });
      expect(changeCalled).toBe(false);
    });
  });

  test('modal closes after successful password change', async ({ connectedPage: page }) => {
    await test.step('Open modal and intercept API', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.locator('button[onclick="showPasswordChangeModal()"]').click();
      await expect(page.locator('#passwordChangeModal')).toBeVisible({ timeout: 3000 });

      await page.route('**/api/auth/change', async (route) => {
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ success: true }),
        });
      });
    });

    await test.step('Fill current and new passwords then submit', async () => {
      // Fill current password if the field is visible (non-default-password state)
      const currentPwdInput = page.locator('#currentPassword');
      if (await currentPwdInput.count() > 0 && await currentPwdInput.isVisible()) {
        await currentPwdInput.fill('currentpassword');
      }
      await page.locator('#newPassword').fill('validpassword123');
      await page.locator('#confirmPassword').fill('validpassword123');
      await page.locator('#passwordChangeModal button[type="submit"]').click();
      await expect(page.locator('#passwordChangeModal')).toHaveCount(0, { timeout: 3000 });
    });
  });

});
