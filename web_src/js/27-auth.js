// ===== Authentication Helper Functions =====

        async function checkPasswordWarning() {
            try {
                const response = await apiFetch('/api/auth/status');
                const data = await response.json();

                if (data.success && data.isDefaultPassword) {
                    showDefaultPasswordWarning();
                }
            } catch (err) {
                console.error('Failed to check auth status:', err);
            }
        }

        function showDefaultPasswordWarning() {
            if (sessionStorage.getItem('passwordWarningDismissed') === 'true') {
                return;
            }

            const banner = document.createElement('div');
            banner.className = 'warning-banner';
            banner.id = 'passwordWarning';
            banner.innerHTML = `
                <div class="warning-icon">⚠️</div>
                <div class="warning-text">
                    <strong>Security Warning</strong>
                    You are using the default password. Anyone on your network can access this device.
                    <a href="#" onclick="openPasswordChangeSettings(); return false;">
                        Change password now →
                    </a>
                </div>
                <button class="warning-dismiss" onclick="dismissPasswordWarning()">
                    Dismiss
                </button>
            `;

            const container = document.querySelector('.main-content');
            if (container && container.firstChild) {
                container.insertBefore(banner, container.firstChild);
            }
        }

        function dismissPasswordWarning() {
            const banner = document.getElementById('passwordWarning');
            if (banner) {
                banner.remove();
            }
            sessionStorage.setItem('passwordWarningDismissed', 'true');
        }

        function openPasswordChangeSettings() {
            switchTab('settings');
            showPasswordChangeModal();
            dismissPasswordWarning();
        }

        function showPasswordChangeModal() {
            const modal = document.createElement('div');
            modal.className = 'modal-overlay';
            modal.id = 'passwordChangeModal';
            modal.innerHTML = `
                <div class="modal-content">
                    <h2>Change Password</h2>
                    <form id="passwordChangeForm">
                        <div class="form-group">
                            <label>New Password</label>
                            <input type="password" id="newPassword" required minlength="8">
                        </div>
                        <div class="form-group">
                            <label>Confirm New Password</label>
                            <input type="password" id="confirmPassword" required minlength="8">
                        </div>
                        <div id="passwordError" class="error-message" style="display:none;"></div>
                        <div class="modal-actions">
                            <button type="button" onclick="closePasswordChangeModal()">Cancel</button>
                            <button type="submit" class="primary">Change Password</button>
                        </div>
                    </form>
                </div>
            `;
            document.body.appendChild(modal);

            document.getElementById('passwordChangeForm').onsubmit = async function(e) {
                e.preventDefault();
                await changePassword();
            };
        }

        async function changePassword() {
            const newPassword = document.getElementById('newPassword').value;
            const confirmPassword = document.getElementById('confirmPassword').value;
            const errorDiv = document.getElementById('passwordError');

            if (newPassword !== confirmPassword) {
                errorDiv.textContent = 'Passwords do not match';
                errorDiv.style.display = 'block';
                return;
            }

            if (newPassword.length < 8) {
                errorDiv.textContent = 'Password must be at least 8 characters';
                errorDiv.style.display = 'block';
                return;
            }

            try {
                const response = await apiFetch('/api/auth/change', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        newPassword: newPassword
                    })
                });

                const data = await response.json();

                if (data.success) {
                    showToast('Password changed successfully', 'success');
                    closePasswordChangeModal();
                    sessionStorage.removeItem('passwordWarningDismissed');
                } else {
                    errorDiv.textContent = data.error || 'Failed to change password';
                    errorDiv.style.display = 'block';
                }
            } catch (err) {
                errorDiv.textContent = 'Network error';
                errorDiv.style.display = 'block';
            }
        }

        function closePasswordChangeModal() {
            const modal = document.getElementById('passwordChangeModal');
            if (modal) {
                modal.remove();
            }
        }

        function logout() {
            apiFetch('/api/auth/logout', { method: 'POST' })
                .then(() => {
                    sessionStorage.clear();
                    localStorage.clear();
                    window.location.href = '/login';
                });
        }

        function togglePasswordVisibility(inputId, button) {
            const input = document.getElementById(inputId);
            if (input.type === 'password') {
                input.type = 'text';
                button.textContent = '🙈';
            } else {
                input.type = 'password';
                button.textContent = '👁';
            }
        }
