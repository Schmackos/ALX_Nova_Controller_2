#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <pgmspace.h>

const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, viewport-fit=cover">
    <meta name="theme-color" content="#F5F5F5">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="default">
    <title>Login - ALX Audio Controller</title>
    <style>
        /* ===== CSS Variables ===== */
        :root {
            --bg-primary: #F5F5F5;
            --bg-surface: #FFFFFF;
            --bg-card: #EEEEEE;
            --bg-input: #E0E0E0;
            --accent: #FF9800;
            --accent-light: #FFB74D;
            --accent-dark: #E68900;
            --text-primary: #212121;
            --text-secondary: #757575;
            --text-disabled: #9E9E9E;
            --error: #F44336;
            --border: #E0E0E0;
            --shadow: rgba(0, 0, 0, 0.1);
            --safe-top: env(safe-area-inset-top, 0px);
            --safe-bottom: env(safe-area-inset-bottom, 0px);
        }

        /* ===== Dark Theme ===== */
        body.night-mode {
            --bg-primary: #121212;
            --bg-surface: #1E1E1E;
            --bg-card: #252525;
            --bg-input: #2C2C2C;
            --text-primary: #FFFFFF;
            --text-secondary: #B0B0B0;
            --text-disabled: #666666;
            --error: #F44336;
            --border: #333333;
            --shadow: rgba(0, 0, 0, 0.4);
        }

        /* ===== Reset & Base Styles ===== */
        *, *::before, *::after {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        html {
            font-size: 16px;
            -webkit-text-size-adjust: 100%;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            min-height: 100dvh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
            padding-top: calc(20px + var(--safe-top));
            padding-bottom: calc(20px + var(--safe-bottom));
        }

        /* ===== Login Container ===== */
        .login-container {
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 40px 32px;
            max-width: 400px;
            width: 100%;
            box-shadow: 0 4px 20px var(--shadow);
        }

        .login-header {
            text-align: center;
            margin-bottom: 32px;
        }

        .login-logo {
            width: 80px;
            height: 80px;
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-dark) 100%);
            border-radius: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0 auto 16px;
            font-size: 40px;
        }

        .login-title {
            font-size: 24px;
            font-weight: 600;
            margin-bottom: 8px;
        }

        .login-subtitle {
            font-size: 14px;
            color: var(--text-secondary);
        }

        /* ===== Form Styles ===== */
        .form-group {
            margin-bottom: 24px;
        }

        .form-label {
            display: block;
            margin-bottom: 8px;
            font-size: 14px;
            font-weight: 500;
            color: var(--text-secondary);
        }

        .input-wrapper {
            position: relative;
        }

        .form-input {
            width: 100%;
            padding: 14px 48px 14px 16px;
            border: 2px solid var(--border);
            border-radius: 8px;
            background: var(--bg-input);
            color: var(--text-primary);
            font-size: 16px;
            transition: all 0.2s;
        }

        .form-input:focus {
            outline: none;
            border-color: var(--accent);
            background: var(--bg-surface);
        }

        .toggle-password {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            cursor: pointer;
            font-size: 20px;
            padding: 8px;
            color: var(--text-secondary);
            transition: color 0.2s;
        }

        .toggle-password:hover {
            color: var(--text-primary);
        }

        .error-message {
            margin-top: 16px;
            padding: 12px 16px;
            background: var(--error);
            color: white;
            border-radius: 8px;
            font-size: 14px;
            display: none;
        }

        .error-message.show {
            display: block;
            animation: slideIn 0.3s ease-out;
        }

        @keyframes slideIn {
            from {
                opacity: 0;
                transform: translateY(-10px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .login-button {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-dark) 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .login-button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(255, 152, 0, 0.4);
        }

        .login-button:active {
            transform: translateY(0);
        }

        .login-button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
            transform: none;
        }

        .loading-spinner {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid rgba(255, 255, 255, 0.3);
            border-radius: 50%;
            border-top-color: white;
            animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        /* ===== Mobile Responsive ===== */
        @media (max-width: 480px) {
            .login-container {
                padding: 32px 24px;
            }

            .login-logo {
                width: 64px;
                height: 64px;
                font-size: 32px;
            }

            .login-title {
                font-size: 20px;
            }
        }
    </style>
</head>
<body>
    <div class="login-container">
        <div class="login-header">
            <div class="login-logo">🔊</div>
            <h1 class="login-title">ALX Audio Controller</h1>
            <p class="login-subtitle">Enter your password to continue</p>
        </div>

        <form id="loginForm">
            <div class="form-group">
                <label for="password" class="form-label">Password</label>
                <div class="input-wrapper">
                    <input
                        type="password"
                        id="password"
                        name="password"
                        class="form-input"
                        placeholder="Enter password"
                        required
                        autofocus
                        autocomplete="current-password"
                    >
                    <button type="button" class="toggle-password" onclick="togglePassword()">👁️</button>
                </div>
            </div>

            <div id="errorMessage" class="error-message"></div>

            <button type="submit" class="login-button" id="loginButton">
                <span id="buttonText">Login</span>
                <span id="buttonSpinner" class="loading-spinner" style="display: none;"></span>
            </button>
        </form>
    </div>

    <script>
        // Apply dark mode if stored in localStorage
        if (localStorage.getItem('darkMode') === 'true' || localStorage.getItem('nightMode') === 'true') {
            document.body.classList.add('night-mode');
        }

        // Toggle password visibility
        function togglePassword() {
            const passwordInput = document.getElementById('password');
            const toggleButton = document.querySelector('.toggle-password');

            if (passwordInput.type === 'password') {
                passwordInput.type = 'text';
                toggleButton.textContent = '🙈';
            } else {
                passwordInput.type = 'password';
                toggleButton.textContent = '👁️';
            }
        }

        // Show error message
        function showError(message) {
            const errorDiv = document.getElementById('errorMessage');
            errorDiv.textContent = message;
            errorDiv.classList.add('show');

            // Auto-hide after 5 seconds
            setTimeout(() => {
                errorDiv.classList.remove('show');
            }, 5000);
        }

        // Hide error message
        function hideError() {
            const errorDiv = document.getElementById('errorMessage');
            errorDiv.classList.remove('show');
        }

        // Set loading state
        function setLoading(loading) {
            const button = document.getElementById('loginButton');
            const buttonText = document.getElementById('buttonText');
            const buttonSpinner = document.getElementById('buttonSpinner');
            const passwordInput = document.getElementById('password');

            if (loading) {
                button.disabled = true;
                buttonText.style.display = 'none';
                buttonSpinner.style.display = 'inline-block';
                passwordInput.disabled = true;
            } else {
                button.disabled = false;
                buttonText.style.display = 'inline';
                buttonSpinner.style.display = 'none';
                passwordInput.disabled = false;
            }
        }

        // Handle login form submission
        document.getElementById('loginForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const password = document.getElementById('password').value;

            if (password.length < 8) {
                showError('Password must be at least 8 characters');
                return;
            }

            hideError();
            setLoading(true);

            try {
                const response = await fetch('/api/auth/login', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ password: password })
                });

                const data = await response.json();

                if (data.success) {
                    // Check if using default password
                    if (data.isDefaultPassword) {
                        sessionStorage.setItem('showPasswordWarning', 'true');
                    }

                    // Redirect to main page
                    window.location.href = '/';
                } else {
                    showError(data.error || 'Login failed');
                    setLoading(false);
                    document.getElementById('password').value = '';
                    document.getElementById('password').focus();
                }
            } catch (err) {
                console.error('Login error:', err);
                showError('Network error. Please try again.');
                setLoading(false);
            }
        });

        // Auto-focus password field on page load
        window.addEventListener('load', function() {
            document.getElementById('password').focus();
        });
    </script>
</body>
</html>
)rawliteral";

#endif
