/**
 * auth-password.spec.js — Password change modal: opens, validates, submits to API.
 *
 * The modal is dynamically created by showPasswordChangeModal() and appended to <body>.
 * HTML5 minlength="8" on the inputs triggers browser-native validation before the form
 * submits — to test the JS mismatch error we use passwords >= 8 chars that don't match.
 */

const { test, expect } = require('../helpers/fixtures');

test('password change modal opens, validates min length, and submits to /api/auth/change', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  // Click the Change Password button
  const changePwdBtn = page.locator('button[onclick="showPasswordChangeModal()"]');
  await expect(changePwdBtn).toBeVisible();
  await changePwdBtn.click();

  // Modal appears
  const modal = page.locator('#passwordChangeModal');
  await expect(modal).toBeVisible({ timeout: 3000 });

  // New password and confirm fields are present
  const newPwdInput = page.locator('#newPassword');
  const confirmPwdInput = page.locator('#confirmPassword');
  await expect(newPwdInput).toBeVisible();
  await expect(confirmPwdInput).toBeVisible();

  // Both have minlength="8" attribute
  await expect(newPwdInput).toHaveAttribute('minlength', '8');
  await expect(confirmPwdInput).toHaveAttribute('minlength', '8');

  // Submit with mismatched passwords (both >= 8 chars to pass browser minlength validation)
  // JS changePassword() checks mismatch first and shows #passwordError
  await newPwdInput.fill('password123');
  await confirmPwdInput.fill('differentpassword');
  await modal.locator('button[type="submit"]').click();

  // Error message should be shown by JS validation
  const errorDiv = page.locator('#passwordError');
  await expect(errorDiv).toBeVisible({ timeout: 2000 });

  // Now enter a valid matching password
  await newPwdInput.fill('newpassword123');
  await confirmPwdInput.fill('newpassword123');

  // Intercept the API call
  let changeCalled = false;
  await page.route('/api/auth/change', async (route) => {
    changeCalled = true;
    await route.fulfill({
      status: 200,
      body: JSON.stringify({ success: true }),
    });
  });

  await modal.locator('button[type="submit"]').click();
  await page.waitForTimeout(500);
  expect(changeCalled).toBe(true);

  // Modal closes after success
  await expect(modal).not.toBeVisible({ timeout: 3000 });
});
