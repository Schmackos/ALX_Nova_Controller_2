/**
 * auth-password.spec.js — Password change modal: opens, validates, submits to API.
 *
 * The modal is dynamically created by showPasswordChangeModal() and appended to <body>.
 * HTML5 minlength="8" on the inputs triggers browser-native validation before the form
 * submits — to test the JS mismatch error we use passwords >= 8 chars that don't match.
 *
 * The modal includes a "Current Password" field (hidden when isDefaultPassword === true).
 */

const { test, expect } = require('../helpers/fixtures');

test('password change modal shows current password field and validates matching passwords', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  // Click the Change Password button
  const changePwdBtn = page.locator('button[onclick="showPasswordChangeModal()"]');
  await expect(changePwdBtn).toBeVisible();
  await changePwdBtn.click();

  // Modal appears
  const modal = page.locator('#passwordChangeModal');
  await expect(modal).toBeVisible({ timeout: 3000 });

  // Current, new and confirm fields are all present
  const currentPwdInput = page.locator('#currentPassword');
  const newPwdInput = page.locator('#newPassword');
  const confirmPwdInput = page.locator('#confirmPassword');
  await expect(currentPwdInput).toBeVisible();
  await expect(newPwdInput).toBeVisible();
  await expect(confirmPwdInput).toBeVisible();

  // New and confirm have minlength="8"
  await expect(newPwdInput).toHaveAttribute('minlength', '8');
  await expect(confirmPwdInput).toHaveAttribute('minlength', '8');

  // Submit with mismatched passwords (both >= 8 chars to pass browser minlength validation)
  // JS changePassword() checks mismatch first and shows #passwordError
  await currentPwdInput.fill('currentpass');
  await newPwdInput.fill('password123');
  await confirmPwdInput.fill('differentpassword');
  await modal.locator('button[type="submit"]').click();

  // Error message should be shown by JS validation
  const errorDiv = page.locator('#passwordError');
  await expect(errorDiv).toBeVisible({ timeout: 2000 });
  await expect(errorDiv).toContainText('do not match');

  // Now enter a valid matching password
  await currentPwdInput.fill('currentpass');
  await newPwdInput.fill('newpassword123');
  await confirmPwdInput.fill('newpassword123');

  // Intercept the API call
  let requestBody = null;
  await page.route('/api/auth/change', async (route) => {
    requestBody = JSON.parse(route.request().postData() || '{}');
    await route.fulfill({
      status: 200,
      body: JSON.stringify({ success: true }),
    });
  });

  await modal.locator('button[type="submit"]').click();
  await page.waitForTimeout(500);

  // currentPassword was included in the request
  expect(requestBody).not.toBeNull();
  expect(requestBody.currentPassword).toBe('currentpass');
  expect(requestBody.newPassword).toBe('newpassword123');

  // Modal closes after success
  await expect(modal).not.toBeVisible({ timeout: 3000 });
});

test('password change modal hides current password field when using default password', async ({ connectedPage: page }) => {
  // Set isDefaultPassword on the mock server state so /api/auth/status returns it
  await page.route('/api/auth/status', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ success: true, authenticated: true, isDefaultPassword: true }),
    });
  });

  // Trigger checkPasswordWarning() by navigating — simulate it via page.evaluate
  await page.evaluate(() => checkPasswordWarning());
  await page.waitForTimeout(300);

  await page.locator('.sidebar-item[data-tab="settings"]').click();
  const changePwdBtn = page.locator('button[onclick="showPasswordChangeModal()"]');
  await changePwdBtn.click();

  const modal = page.locator('#passwordChangeModal');
  await expect(modal).toBeVisible({ timeout: 3000 });

  // Current password group should be hidden for first-boot default password
  const currentPwGroup = page.locator('#currentPasswordGroup');
  await expect(currentPwGroup).toHaveCSS('display', 'none');

  // Submit with only new + confirm (no current password needed)
  await page.locator('#newPassword').fill('mynewpassword');
  await page.locator('#confirmPassword').fill('mynewpassword');

  let requestBody = null;
  await page.route('/api/auth/change', async (route) => {
    requestBody = JSON.parse(route.request().postData() || '{}');
    await route.fulfill({
      status: 200,
      body: JSON.stringify({ success: true }),
    });
  });

  await modal.locator('button[type="submit"]').click();
  await page.waitForTimeout(500);

  // currentPassword sent as empty string (default password exemption)
  expect(requestBody).not.toBeNull();
  expect(requestBody.currentPassword).toBe('');
  expect(requestBody.newPassword).toBe('mynewpassword');

  await expect(modal).not.toBeVisible({ timeout: 3000 });
});

test('incorrect current password shows error from API', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="settings"]').click();

  const changePwdBtn = page.locator('button[onclick="showPasswordChangeModal()"]');
  await changePwdBtn.click();

  const modal = page.locator('#passwordChangeModal');
  await expect(modal).toBeVisible({ timeout: 3000 });

  await page.locator('#currentPassword').fill('wrongpassword');
  await page.locator('#newPassword').fill('newpassword123');
  await page.locator('#confirmPassword').fill('newpassword123');

  // Intercept with 401 response (wrong current password)
  await page.route('/api/auth/change', async (route) => {
    await route.fulfill({
      status: 401,
      contentType: 'application/json',
      body: JSON.stringify({ success: false, error: 'Current password is incorrect' }),
    });
  });

  await modal.locator('button[type="submit"]').click();
  await page.waitForTimeout(500);

  // Error from API shown inside modal
  const errorDiv = page.locator('#passwordError');
  await expect(errorDiv).toBeVisible({ timeout: 2000 });
  await expect(errorDiv).toContainText('incorrect');

  // Modal remains open
  await expect(modal).toBeVisible();
});
