#include <pgmspace.h>
#include "web_pages.h"

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, viewport-fit=cover">
    <meta name="theme-color" content="#F5F5F5">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="default">
    <title>ALX Audio Controller</title>
    <style>
        /* ===== CSS Variables - Light Theme (Day Mode) ===== */
        :root {
            --bg-primary: #F5F5F5;
            --bg-surface: #FFFFFF;
            --bg-card: #EEEEEE;
            --bg-input: #E0E0E0;
            --accent: #FF9800;
            --accent-light: #FFB74D;
            --accent-dark: #F57C00;
            --text-primary: #212121;
            --text-secondary: #757575;
            --text-disabled: #9E9E9E;
            --success: #4CAF50;
            --error: #D32F2F;
            --warning: #FF9800;
            --info: #1976D2;
            --border: #E0E0E0;
            --shadow: rgba(0, 0, 0, 0.1);
            --tab-height: 56px;
            --safe-top: env(safe-area-inset-top, 0px);
            --safe-bottom: env(safe-area-inset-bottom, 0px);
        }

        /* ===== Dark Theme (Night Mode) ===== */
        body.night-mode {
            --bg-primary: #121212;
            --bg-surface: #1E1E1E;
            --bg-card: #252525;
            --bg-input: #2a2a2a;
            --text-primary: #FFFFFF;
            --text-secondary: #B3B3B3;
            --text-disabled: #666666;
            --error: #CF6679;
            --info: #2196F3;
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
            overflow-x: hidden;
            padding-top: calc(var(--tab-height) + var(--safe-top));
            padding-bottom: var(--safe-bottom);
        }

        /* ===== Tab Navigation Bar ===== */
        .tab-bar {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            height: calc(var(--tab-height) + var(--safe-top));
            padding-top: var(--safe-top);
            background: var(--bg-surface);
            display: flex;
            justify-content: space-around;
            align-items: center;
            border-bottom: 1px solid var(--border);
            z-index: 1000;
        }

        .tab {
            flex: 1;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: var(--tab-height);
            background: none;
            border: none;
            color: var(--text-disabled);
            cursor: pointer;
            transition: color 0.2s ease;
            position: relative;
            -webkit-tap-highlight-color: transparent;
        }

        .tab svg {
            width: 24px;
            height: 24px;
            fill: currentColor;
        }

        .tab.active {
            color: var(--accent);
        }

        .tab.active::after {
            content: '';
            position: absolute;
            bottom: 0;
            left: 50%;
            transform: translateX(-50%);
            width: 32px;
            height: 3px;
            background: var(--accent);
            border-radius: 3px 3px 0 0;
        }

        .tab:active {
            opacity: 0.7;
        }

        /* ===== Main Content ===== */
        .tab-content {
            padding: 16px;
            max-width: 480px;
            margin: 0 auto;
        }

        .panel {
            display: none;
        }

        .panel.active {
            display: block;
            animation: fadeIn 0.2s ease;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(8px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* ===== Card Component ===== */
        .card {
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 12px;
        }

        .card-title {
            font-size: 14px;
            font-weight: 600;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 12px;
        }

        /* ===== Skeleton Loading ===== */
        .skeleton {
            background: linear-gradient(90deg, var(--bg-card) 25%, #3a3a3a 50%, var(--bg-card) 75%);
            background-size: 200% 100%;
            animation: shimmer 1.5s infinite;
            border-radius: 4px;
        }

        .skeleton-text {
            height: 16px;
            width: 60%;
            margin-bottom: 8px;
        }

        .skeleton-text.short {
            width: 40%;
        }

        .skeleton-text.full {
            width: 100%;
        }

        .skeleton-button {
            height: 48px;
            width: 100%;
            border-radius: 8px;
        }

        .skeleton-circle {
            width: 48px;
            height: 48px;
            border-radius: 50%;
        }

        @keyframes shimmer {
            0% { background-position: 200% 0; }
            100% { background-position: -200% 0; }
        }

        /* ===== Buttons ===== */
        .btn {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            min-height: 48px;
            padding: 12px 24px;
            font-size: 16px;
            font-weight: 600;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s ease;
            -webkit-tap-highlight-color: transparent;
            width: 100%;
        }

        .btn-primary {
            background: var(--accent);
            color: #000;
        }

        .btn-primary:active {
            background: var(--accent-dark);
            transform: scale(0.98);
        }

        .btn-secondary {
            background: var(--bg-card);
            color: var(--text-primary);
            border: 1px solid var(--border);
        }

        .btn-secondary:active {
            background: var(--bg-input);
        }

        .btn-danger {
            background: var(--error);
            color: #fff;
        }

        .btn-danger:active {
            opacity: 0.8;
        }

        .btn-success {
            background: var(--success);
            color: #fff;
        }

        .btn + .btn {
            margin-top: 8px;
        }

        .btn-row {
            display: flex;
            gap: 8px;
        }

        .btn-row .btn {
            flex: 1;
        }

        .btn-row .btn + .btn {
            margin-top: 0;
        }

        /* ===== Form Elements ===== */
        .form-group {
            margin-bottom: 16px;
        }

        .form-label {
            display: block;
            font-size: 14px;
            color: var(--text-secondary);
            margin-bottom: 8px;
        }

        .form-input {
            width: 100%;
            height: 48px;
            padding: 12px 16px;
            font-size: 16px;
            color: var(--text-primary);
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 8px;
            outline: none;
            transition: border-color 0.2s ease;
        }

        .form-input:focus {
            border-color: var(--accent);
        }

        .form-input::placeholder {
            color: var(--text-disabled);
        }

        .input-with-btn {
            display: flex;
            gap: 8px;
        }

        .input-with-btn .form-input {
            flex: 1;
        }

        .input-with-btn .btn {
            width: auto;
            padding: 12px 16px;
        }

        /* ===== Toggle Switch ===== */
        .toggle-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            min-height: 48px;
            padding: 8px 0;
        }

        .toggle-label {
            font-size: 16px;
            color: var(--text-primary);
        }

        .toggle-sublabel {
            font-size: 13px;
            color: var(--text-secondary);
            margin-top: 2px;
        }

        .switch {
            position: relative;
            width: 52px;
            height: 32px;
            flex-shrink: 0;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: var(--bg-card);
            border: 2px solid var(--border);
            border-radius: 32px;
            transition: 0.2s;
        }

        .slider::before {
            content: '';
            position: absolute;
            height: 24px;
            width: 24px;
            left: 2px;
            bottom: 2px;
            background: var(--text-secondary);
            border-radius: 50%;
            transition: 0.2s;
        }

        input:checked + .slider {
            background: var(--accent);
            border-color: var(--accent);
        }

        input:checked + .slider::before {
            background: #fff;
            transform: translateX(20px);
        }

        /* ===== Radio Buttons ===== */
        .radio-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .radio-option {
            display: flex;
            align-items: center;
            min-height: 48px;
            padding: 12px;
            background: var(--bg-card);
            border-radius: 8px;
            cursor: pointer;
            transition: background 0.2s;
        }

        .radio-option:active {
            background: var(--bg-input);
        }

        .radio-option input[type="radio"] {
            width: 20px;
            height: 20px;
            margin-right: 12px;
            accent-color: var(--accent);
        }

        .radio-option span {
            font-size: 16px;
        }

        /* ===== Status Indicators ===== */
        .status-dot {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            margin-right: 8px;
        }

        .status-dot.success { background: var(--success); }
        .status-dot.error { background: var(--error); }
        .status-dot.warning { background: var(--warning); }
        .status-dot.info { background: var(--info); }

        .status-badge {
            display: inline-flex;
            align-items: center;
            padding: 6px 12px;
            font-size: 14px;
            font-weight: 600;
            border-radius: 16px;
            background: var(--bg-card);
        }

        .status-badge.on {
            background: rgba(76, 175, 80, 0.2);
            color: var(--success);
        }

        .status-badge.off {
            background: rgba(207, 102, 121, 0.2);
            color: var(--error);
        }

        /* ===== Progress Bar ===== */
        .progress-container {
            width: 100%;
            height: 8px;
            background: var(--bg-card);
            border-radius: 4px;
            overflow: hidden;
            margin: 12px 0;
        }

        .progress-bar {
            height: 100%;
            background: var(--accent);
            border-radius: 4px;
            transition: width 0.3s ease;
            width: 0%;
        }

        .progress-text {
            font-size: 14px;
            color: var(--text-secondary);
            text-align: center;
            margin-top: 4px;
        }

        /* ===== Info Box ===== */
        .info-box {
            padding: 12px;
            background: var(--bg-card);
            border-radius: 8px;
            font-size: 14px;
            color: var(--text-secondary);
            margin-bottom: 12px;
        }

        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid var(--border);
        }

        .info-row:last-child {
            border-bottom: none;
        }

        .info-label {
            color: var(--text-secondary);
        }

        .info-value {
            color: var(--text-primary);
            font-weight: 500;
            text-align: right;
        }

        /* ===== Connection Status ===== */
        .connection-bar {
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 8px;
            background: var(--bg-card);
            border-radius: 8px;
            font-size: 14px;
            margin-bottom: 12px;
        }

        .connection-bar.connected {
            background: rgba(76, 175, 80, 0.15);
            color: var(--success);
        }

        .connection-bar.disconnected {
            background: rgba(207, 102, 121, 0.15);
            color: var(--error);
        }

        /* ===== Stats Grid ===== */
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }

        .stat-card {
            background: var(--bg-card);
            border-radius: 8px;
            padding: 12px;
            text-align: center;
        }

        .stat-value {
            font-size: 24px;
            font-weight: 700;
            color: var(--accent);
        }

        .stat-label {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        /* ===== Timer Display ===== */
        .timer-display {
            text-align: center;
            padding: 20px;
            background: var(--bg-card);
            border-radius: 12px;
            margin: 12px 0;
        }

        .timer-value {
            font-size: 48px;
            font-weight: 700;
            font-family: 'SF Mono', 'Consolas', monospace;
            color: var(--accent);
        }

        .timer-label {
            font-size: 14px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        /* ===== LED Indicator ===== */
        .led-display {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 24px;
        }

        .led {
            width: 80px;
            height: 80px;
            border-radius: 50%;
            border: 4px solid var(--border);
            transition: all 0.3s ease;
        }

        .led.on {
            background: #ffeb3b;
            box-shadow: 0 0 40px #ffeb3b, 0 0 80px rgba(255, 235, 59, 0.5);
            border-color: #ffeb3b;
        }

        .led.off {
            background: var(--bg-card);
            box-shadow: inset 0 0 20px rgba(0, 0, 0, 0.5);
        }

        .led-status {
            margin-top: 12px;
            font-size: 14px;
            color: var(--text-secondary);
        }

        /* ===== Debug Console ===== */
        .debug-console {
            background: #0d0d0d;
            border-radius: 8px;
            font-family: 'SF Mono', 'Consolas', 'Monaco', monospace;
            font-size: 12px;
            max-height: 300px;
            overflow-y: auto;
            padding: 12px;
        }

        .log-entry {
            padding: 2px 0;
            border-bottom: 1px solid rgba(255,255,255,0.05);
        }

        .log-timestamp {
            color: var(--text-disabled);
            margin-right: 8px;
        }

        .log-message { color: var(--text-primary); }
        .log-message.info { color: var(--info); }
        .log-message.success { color: var(--success); }
        .log-message.warning { color: var(--warning); }
        .log-message.error { color: var(--error); }

        /* ===== Modal ===== */
        .modal-overlay {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            z-index: 2000;
            align-items: center;
            justify-content: center;
            padding: 16px;
        }

        .modal-overlay.active {
            display: flex;
        }

        .modal {
            background: var(--bg-surface);
            border-radius: 16px;
            padding: 24px;
            width: 100%;
            max-width: 400px;
            max-height: 80vh;
            overflow-y: auto;
        }

        .modal-title {
            font-size: 20px;
            font-weight: 600;
            margin-bottom: 16px;
        }

        .modal-close {
            position: absolute;
            top: 16px;
            right: 16px;
            width: 32px;
            height: 32px;
            border: none;
            background: var(--bg-card);
            border-radius: 50%;
            color: var(--text-secondary);
            cursor: pointer;
            font-size: 20px;
        }

        /* ===== Notification Toast ===== */
        .toast {
            position: fixed;
            bottom: calc(20px + var(--safe-bottom));
            left: 16px;
            right: 16px;
            max-width: 400px;
            margin: 0 auto;
            padding: 16px;
            background: var(--bg-surface);
            border-radius: 12px;
            box-shadow: 0 4px 20px var(--shadow);
            z-index: 3000;
            transform: translateY(100px);
            opacity: 0;
            transition: all 0.3s ease;
        }

        .toast.show {
            transform: translateY(0);
            opacity: 1;
        }

        .toast.success {
            border-left: 4px solid var(--success);
        }

        .toast.error {
            border-left: 4px solid var(--error);
        }

        /* ===== Collapsible Section ===== */
        .collapsible-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            min-height: 48px;
            cursor: pointer;
            -webkit-tap-highlight-color: transparent;
        }

        .collapsible-header svg {
            width: 20px;
            height: 20px;
            fill: var(--text-secondary);
            transition: transform 0.2s;
        }

        .collapsible-header.open svg {
            transform: rotate(180deg);
        }

        .collapsible-content {
            max-height: 0;
            overflow: hidden;
            transition: max-height 0.3s ease;
        }

        .collapsible-content.open {
            max-height: 2000px;
        }

        /* ===== Version Info ===== */
        .version-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 0;
            border-bottom: 1px solid var(--border);
        }

        .version-row:last-child {
            border-bottom: none;
        }

        .version-label {
            color: var(--text-secondary);
            font-size: 14px;
        }

        .version-value {
            font-weight: 600;
            font-family: 'SF Mono', 'Consolas', monospace;
        }

        .version-update {
            color: var(--success);
        }

        /* ===== Graph Canvas ===== */
        .graph-container {
            background: var(--bg-card);
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 12px;
        }

        .graph-canvas {
            width: 100%;
            height: 120px;
            display: block;
        }

        /* ===== Utility Classes ===== */
        .hidden { display: none !important; }
        .text-center { text-align: center; }
        .text-secondary { color: var(--text-secondary); }
        .text-success { color: var(--success); }
        .text-error { color: var(--error); }
        .text-warning { color: var(--warning); }
        .mb-8 { margin-bottom: 8px; }
        .mb-12 { margin-bottom: 12px; }
        .mb-16 { margin-bottom: 16px; }
        .mt-12 { margin-top: 12px; }
        .mt-16 { margin-top: 16px; }

        /* ===== Amplifier Status ===== */
        .amplifier-display {
            text-align: center;
            padding: 24px;
            background: var(--bg-card);
            border-radius: 12px;
            margin-bottom: 12px;
        }

        .amplifier-icon {
            width: 64px;
            height: 64px;
            margin: 0 auto 12px;
            fill: var(--text-disabled);
            transition: all 0.3s;
        }

        .amplifier-display.on .amplifier-icon {
            fill: var(--success);
            filter: drop-shadow(0 0 12px var(--success));
        }

        .amplifier-label {
            font-size: 18px;
            font-weight: 600;
        }

        /* ===== Select Dropdown ===== */
        .form-select {
            width: 100%;
            height: 48px;
            padding: 12px 16px;
            font-size: 16px;
            color: var(--text-primary);
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 8px;
            outline: none;
            appearance: none;
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' fill='%23B3B3B3' viewBox='0 0 16 16'%3E%3Cpath d='M8 11L3 6h10l-5 5z'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 16px center;
        }

        .form-select:focus {
            border-color: var(--accent);
        }

        /* ===== Divider ===== */
        .divider {
            height: 1px;
            background: var(--border);
            margin: 16px 0;
        }

        /* ===== Password Toggle ===== */
        .password-wrapper {
            position: relative;
        }

        .password-toggle {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            color: var(--text-secondary);
            cursor: pointer;
            padding: 8px;
            font-size: 18px;
        }

        .password-wrapper .form-input {
            padding-right: 48px;
        }

        /* ===== File Drop Zone ===== */
        .drop-zone {
            border: 2px dashed var(--border);
            border-radius: 12px;
            padding: 32px 16px;
            text-align: center;
            transition: all 0.2s;
            cursor: pointer;
        }

        .drop-zone.dragover {
            border-color: var(--accent);
            background: rgba(255, 152, 0, 0.1);
        }

        .drop-zone-icon {
            width: 48px;
            height: 48px;
            fill: var(--text-secondary);
            margin-bottom: 12px;
        }

        .drop-zone-text {
            color: var(--text-secondary);
            font-size: 14px;
        }

        .drop-zone-text strong {
            color: var(--accent);
        }
    </style>
</head>
<body>
    <!-- Tab Navigation Bar -->
    <nav class="tab-bar">
        <button class="tab active" data-tab="control" onclick="switchTab('control')">
            <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
        </button>
        <button class="tab" data-tab="wifi" onclick="switchTab('wifi')">
            <svg viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
        </button>
        <button class="tab" data-tab="mqtt" onclick="switchTab('mqtt')">
            <svg viewBox="0 0 24 24"><path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM19 18H6c-2.21 0-4-1.79-4-4s1.79-4 4-4h.71C7.37 7.69 9.48 6 12 6c3.04 0 5.5 2.46 5.5 5.5v.5H19c1.66 0 3 1.34 3 3s-1.34 3-3 3z"/></svg>
        </button>
        <button class="tab" data-tab="settings" onclick="switchTab('settings')">
            <svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
        </button>
        <button class="tab" data-tab="debug" onclick="switchTab('debug')">
            <svg viewBox="0 0 24 24"><path d="M20 19V7H4v12h16m0-16c1.11 0 2 .89 2 2v14c0 1.11-.89 2-2 2H4c-1.11 0-2-.89-2-2V5c0-1.11.89-2 2-2h16zM7 9h10v2H7V9zm0 4h7v2H7v-2z"/></svg>
        </button>
        <button class="tab" data-tab="test" onclick="switchTab('test')">
            <svg viewBox="0 0 24 24"><path d="M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7zm2.85 11.1l-.85.6V16h-4v-2.3l-.85-.6C7.8 12.16 7 10.63 7 9c0-2.76 2.24-5 5-5s5 2.24 5 5c0 1.63-.8 3.16-2.15 4.1z"/></svg>
        </button>
    </nav>

    <!-- Tab Content Panels -->
    <main class="tab-content">
        <!-- ===== CONTROL TAB ===== -->
        <section id="control" class="panel active">
            <div class="connection-bar" id="connectionStatus">
                <span class="status-dot info"></span> Connecting...
            </div>

            <!-- Amplifier Status -->
            <div class="amplifier-display" id="amplifierDisplay">
                <svg class="amplifier-icon" viewBox="0 0 24 24"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></svg>
                <div class="amplifier-label" id="amplifierStatus">OFF</div>
            </div>

            <!-- Smart Sensing Mode -->
            <div class="card">
                <div class="card-title">Smart Sensing Mode</div>
                <div class="radio-group">
                    <label class="radio-option">
                        <input type="radio" name="sensingMode" value="always_on" onchange="updateSensingMode()">
                        <span>Always On</span>
                    </label>
                    <label class="radio-option">
                        <input type="radio" name="sensingMode" value="always_off" onchange="updateSensingMode()">
                        <span>Always Off</span>
                    </label>
                    <label class="radio-option">
                        <input type="radio" name="sensingMode" value="smart_auto" onchange="updateSensingMode()">
                        <span>Smart Auto Sensing</span>
                    </label>
                </div>
            </div>

            <!-- Timer Display -->
            <div class="timer-display hidden" id="timerDisplay">
                <div class="timer-value" id="timerValue">--:--</div>
                <div class="timer-label">Time Remaining</div>
            </div>

            <!-- Timer & Voltage Settings -->
            <div class="card">
                <div class="card-title">Timer & Voltage Settings</div>
                <div class="form-group">
                    <label class="form-label">Auto-Off Timer (minutes)</label>
                    <input type="number" class="form-input" id="timerDuration" inputmode="numeric" min="1" max="60" value="15" onchange="updateTimerDuration()">
                </div>
                <div class="form-group">
                    <label class="form-label">Voltage Threshold (volts)</label>
                    <input type="number" class="form-input" id="voltageThreshold" inputmode="decimal" min="0.1" max="3.3" step="0.1" value="1.0" onchange="updateVoltageThreshold()">
                </div>
                <div class="info-box" id="voltageInfo">
                    <div class="info-row">
                        <span class="info-label">Voltage Detected</span>
                        <span class="info-value" id="voltageDetected">No</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Current Reading</span>
                        <span class="info-value" id="voltageReading">0.00V</span>
                    </div>
                </div>
            </div>

            <!-- Manual Controls -->
            <div class="card">
                <div class="card-title">Manual Control</div>
                <div class="btn-row">
                    <button class="btn btn-success" onclick="manualOverride(true)">Turn On</button>
                    <button class="btn btn-danger" onclick="manualOverride(false)">Turn Off</button>
                </div>
            </div>
        </section>

        <!-- ===== WIFI TAB ===== -->
        <section id="wifi" class="panel">
            <!-- WiFi Status -->
            <div class="card">
                <div class="card-title">Connection Status</div>
                <div class="info-box" id="wifiStatusBox">
                    <div class="skeleton skeleton-text"></div>
                    <div class="skeleton skeleton-text short"></div>
                    <div class="skeleton skeleton-text"></div>
                </div>
            </div>

            <!-- Connect to WiFi -->
            <div class="card">
                <div class="card-title">Connect to Network</div>
                <form onsubmit="submitWiFiConfig(event)">
                    <div class="form-group">
                        <label class="form-label">Available Networks</label>
                        <div style="display: flex; gap: 6px; align-items: center;">
                            <select class="form-input" id="wifiNetworkSelect" style="flex: 1; min-width: 0;" onchange="onNetworkSelect()">
                                <option value="">-- Select a network --</option>
                            </select>
                            <button type="button" class="btn btn-secondary" onclick="scanWiFiNetworks()" id="scanBtn" style="padding: 8px 10px; min-width: 40px; font-size: 16px;" title="Scan for networks">
                                🔍
                            </button>
                        </div>
                        <div id="scanStatus" style="font-size: 12px; color: rgba(255,255,255,0.6); margin-top: 4px;"></div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Network Name (SSID)</label>
                        <input type="text" class="form-input" id="wifiSSID" inputmode="text" placeholder="Enter WiFi SSID or select from above">
                    </div>
                    <div class="form-group">
                        <label class="form-label">Password</label>
                        <div class="password-wrapper">
                            <input type="password" class="form-input" id="wifiPassword" placeholder="Enter password">
                            <button type="button" class="password-toggle" onclick="togglePasswordVisibility('wifiPassword', this)">👁</button>
                        </div>
                    </div>
                    <button type="submit" class="btn btn-primary">Connect</button>
                </form>
            </div>

            <!-- Access Point -->
            <div class="card">
                <div class="card-title">Access Point</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Enable AP Mode</div>
                        <div class="toggle-sublabel">Allow direct connections</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="apToggle" onchange="toggleAP()">
                        <span class="slider"></span>
                    </label>
                </div>
                <button class="btn btn-secondary mt-12" onclick="showAPConfig()">Configure AP</button>
            </div>
        </section>

        <!-- ===== MQTT TAB ===== -->
        <section id="mqtt" class="panel">
            <div class="card">
                <div class="card-title">Connection Status</div>
                <div class="info-box" id="mqttStatusBox">
                    <div class="skeleton skeleton-text"></div>
                    <div class="skeleton skeleton-text short"></div>
                    <div class="skeleton skeleton-text"></div>
                </div>
            </div>

            <div class="card">
                <div class="card-title">MQTT Settings</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Enable MQTT</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="mqttEnabled">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="divider"></div>
                <div class="form-group">
                    <label class="form-label">Broker Address</label>
                    <input type="text" class="form-input" id="mqttBroker" inputmode="url" placeholder="mqtt.example.com">
                </div>
                <div class="form-group">
                    <label class="form-label">Port</label>
                    <input type="number" class="form-input" id="mqttPort" inputmode="numeric" placeholder="1883" value="1883">
                </div>
                <div class="form-group">
                    <label class="form-label">Username (optional)</label>
                    <input type="text" class="form-input" id="mqttUsername" placeholder="Username">
                </div>
                <div class="form-group">
                    <label class="form-label">Password (optional)</label>
                    <div class="password-wrapper">
                        <input type="password" class="form-input" id="mqttPassword" placeholder="Password">
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('mqttPassword', this)">👁</button>
                    </div>
                </div>
                <div class="form-group">
                    <label class="form-label">Base Topic</label>
                    <input type="text" class="form-input" id="mqttBaseTopic" placeholder="alx/audio">
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Home Assistant Discovery</div>
                        <div class="toggle-sublabel">Auto-configure in Home Assistant</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="mqttHADiscovery">
                        <span class="slider"></span>
                    </label>
                </div>
                <button class="btn btn-primary" onclick="saveMqttSettings()">Save MQTT Settings</button>
            </div>
        </section>

        <!-- ===== SETTINGS TAB ===== -->
        <section id="settings" class="panel">
            <!-- Timezone -->
            <div class="card">
                <div class="card-title">Timezone</div>
                <div class="form-group">
                    <select class="form-select" id="timezoneSelect" onchange="updateTimezone()">
                        <option value="-43200">UTC-12:00 (Baker Island)</option>
                        <option value="-39600">UTC-11:00 (Samoa)</option>
                        <option value="-36000">UTC-10:00 (Hawaii)</option>
                        <option value="-32400">UTC-09:00 (Alaska)</option>
                        <option value="-28800">UTC-08:00 (Pacific Time)</option>
                        <option value="-25200">UTC-07:00 (Mountain Time)</option>
                        <option value="-21600">UTC-06:00 (Central Time)</option>
                        <option value="-18000">UTC-05:00 (Eastern Time)</option>
                        <option value="-14400">UTC-04:00 (Atlantic Time)</option>
                        <option value="-10800">UTC-03:00 (Buenos Aires)</option>
                        <option value="-7200">UTC-02:00 (Mid-Atlantic)</option>
                        <option value="-3600">UTC-01:00 (Azores)</option>
                        <option value="0">UTC+00:00 (London, GMT)</option>
                        <option value="3600" selected>UTC+01:00 (Amsterdam, Paris)</option>
                        <option value="7200">UTC+02:00 (Cairo, Athens)</option>
                        <option value="10800">UTC+03:00 (Moscow, Nairobi)</option>
                        <option value="14400">UTC+04:00 (Dubai)</option>
                        <option value="18000">UTC+05:00 (Karachi)</option>
                        <option value="19800">UTC+05:30 (Mumbai)</option>
                        <option value="21600">UTC+06:00 (Dhaka)</option>
                        <option value="25200">UTC+07:00 (Bangkok)</option>
                        <option value="28800">UTC+08:00 (Singapore)</option>
                        <option value="32400">UTC+09:00 (Tokyo)</option>
                        <option value="36000">UTC+10:00 (Sydney)</option>
                        <option value="43200">UTC+12:00 (Auckland)</option>
                    </select>
                </div>
                <div class="info-box">
                    <span id="timezoneInfo">Current timezone offset will be shown here</span>
                </div>
            </div>

            <!-- Day/Night Mode -->
            <div class="card">
                <div class="card-title">Appearance</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Night Mode</div>
                        <div class="toggle-sublabel">Use darker theme</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="nightModeToggle" onchange="toggleTheme()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- Firmware Update -->
            <div class="card">
                <div class="card-title">Firmware Update</div>
                <div class="info-box">
                    <div class="version-row">
                        <span class="version-label">Current Version</span>
                        <span class="version-value" id="currentVersion">Loading...</span>
                    </div>
                    <div class="version-row" id="latestVersionRow" style="display: none;">
                        <span class="version-label">Latest Version</span>
                        <span class="version-value version-update" id="latestVersion">--</span>
                    </div>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Auto Update</div>
                        <div class="toggle-sublabel">Update on boot when available</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="autoUpdateToggle" onchange="toggleAutoUpdate()">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">SSL Validation</div>
                        <div class="toggle-sublabel">Verify update server certificate</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="certValidationToggle" onchange="toggleCertValidation()">
                        <span class="slider"></span>
                    </label>
                </div>
                <button class="btn btn-secondary mb-8" onclick="checkForUpdate()">Check for Updates</button>
                <button class="btn btn-primary hidden" id="updateBtn" onclick="startOTAUpdate()">Update Now</button>
                <div class="progress-container hidden" id="progressContainer">
                    <div class="progress-bar" id="progressBar"></div>
                </div>
                <div class="progress-text hidden" id="progressStatus"></div>

                <div class="divider"></div>
                <div class="text-secondary mb-8" style="font-size: 14px;">Or upload firmware manually:</div>
                <div class="drop-zone" id="firmwareDropZone" onclick="document.getElementById('firmwareFile').click()">
                    <svg class="drop-zone-icon" viewBox="0 0 24 24"><path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM14 13v4h-4v-4H7l5-5 5 5h-3z"/></svg>
                    <div class="drop-zone-text">Tap to select or <strong>drag & drop</strong> .bin file</div>
                </div>
                <input type="file" id="firmwareFile" accept=".bin" style="display: none;" onchange="handleFirmwareSelect(event)">
            </div>

            <!-- Export/Import Settings -->
            <div class="card">
                <div class="card-title">Backup & Restore</div>
                <div class="btn-row">
                    <button class="btn btn-secondary" onclick="exportSettings()">Export</button>
                    <button class="btn btn-secondary" onclick="document.getElementById('importFile').click()">Import</button>
                </div>
                <input type="file" id="importFile" accept=".json" style="display: none;" onchange="handleFileSelect(event)">
            </div>

            <!-- Reboot & Factory Reset -->
            <div class="card">
                <div class="card-title">Device Actions</div>
                <button class="btn btn-secondary mb-8" onclick="startReboot()">Reboot Device</button>
                <button class="btn btn-danger" onclick="startFactoryReset()">Factory Reset</button>
            </div>
        </section>

        <!-- ===== DEBUG TAB ===== -->
        <section id="debug" class="panel">
            <!-- Stats Refresh Interval -->
            <div class="card">
                <div class="card-title">Refresh Rate</div>
                <div class="input-group">
                    <label for="statsIntervalSelect" class="input-label">Update Interval</label>
                    <select id="statsIntervalSelect" class="select-input" onchange="setStatsInterval()">
                        <option value="1">1 second</option>
                        <option value="2" selected>2 seconds</option>
                        <option value="3">3 seconds</option>
                        <option value="5">5 seconds</option>
                        <option value="10">10 seconds</option>
                    </select>
                </div>
            </div>

            <!-- CPU Stats -->
            <div class="card">
                <div class="card-title">CPU</div>
                <div class="stats-grid">
                    <div class="stat-card">
                        <div class="stat-value" id="cpuTotal">--%</div>
                        <div class="stat-label">Total Load</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value" id="cpuTemp">--°C</div>
                        <div class="stat-label">Temperature</div>
                    </div>
                </div>
                <div class="info-box mt-12">
                    <div class="info-row">
                        <span class="info-label">Core 0 Load</span>
                        <span class="info-value" id="cpuCore0">--%</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Core 1 Load</span>
                        <span class="info-value" id="cpuCore1">--%</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Frequency</span>
                        <span class="info-value" id="cpuFreq">-- MHz</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Chip Model</span>
                        <span class="info-value" id="cpuModel">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Revision</span>
                        <span class="info-value" id="cpuRevision">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Cores</span>
                        <span class="info-value" id="cpuCores">--</span>
                    </div>
                </div>
            </div>

            <!-- Memory Stats -->
            <div class="card">
                <div class="card-title">Memory</div>
                <div class="stats-grid">
                    <div class="stat-card">
                        <div class="stat-value" id="heapPercent">--%</div>
                        <div class="stat-label">Heap Used</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value" id="psramPercent">--%</div>
                        <div class="stat-label">PSRAM Used</div>
                    </div>
                </div>
                <div class="info-box mt-12">
                    <div class="info-row">
                        <span class="info-label">Heap Free</span>
                        <span class="info-value" id="heapFree">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Heap Total</span>
                        <span class="info-value" id="heapTotal">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Heap Min Free</span>
                        <span class="info-value" id="heapMinFree">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Largest Block</span>
                        <span class="info-value" id="heapMaxBlock">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">PSRAM Free</span>
                        <span class="info-value" id="psramFree">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">PSRAM Total</span>
                        <span class="info-value" id="psramTotal">--</span>
                    </div>
                </div>
            </div>

            <!-- Storage Stats -->
            <div class="card">
                <div class="card-title">Storage</div>
                <div class="stats-grid">
                    <div class="stat-card">
                        <div class="stat-value" id="sketchPercent">--%</div>
                        <div class="stat-label">Sketch Used</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value" id="spiffsPercent">--%</div>
                        <div class="stat-label">SPIFFS Used</div>
                    </div>
                </div>
                <div class="info-box mt-12">
                    <div class="info-row">
                        <span class="info-label">Flash Size</span>
                        <span class="info-value" id="flashSize">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Sketch Size</span>
                        <span class="info-value" id="sketchSize">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Sketch Free</span>
                        <span class="info-value" id="sketchFree">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">SPIFFS Used</span>
                        <span class="info-value" id="spiffsUsed">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">SPIFFS Total</span>
                        <span class="info-value" id="spiffsTotal">--</span>
                    </div>
                </div>
            </div>

            <!-- WiFi & System Stats -->
            <div class="card">
                <div class="card-title">WiFi & System</div>
                <div class="info-box">
                    <div class="info-row">
                        <span class="info-label">Signal Strength</span>
                        <span class="info-value" id="wifiRssi">-- dBm</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Channel</span>
                        <span class="info-value" id="wifiChannel">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">AP Clients</span>
                        <span class="info-value" id="apClients">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Uptime</span>
                        <span class="info-value" id="uptime">--</span>
                    </div>
                </div>
            </div>

            <!-- Performance Graphs -->
            <div class="card">
                <div class="collapsible-header" onclick="toggleHistorySection()">
                    <span class="card-title" style="margin-bottom: 0;">Performance History</span>
                    <svg viewBox="0 0 24 24" id="historyChevron"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z"/></svg>
                </div>
                <div class="collapsible-content" id="historyContent">
                    <div class="graph-container">
                        <div class="text-secondary mb-8" style="font-size: 12px;">CPU Usage (Orange: Total, Light: Core 0, Dark: Core 1)</div>
                        <canvas class="graph-canvas" id="cpuGraph"></canvas>
                    </div>
                    <div class="graph-container">
                        <div class="text-secondary mb-8" style="font-size: 12px;">Memory Usage (%)</div>
                        <canvas class="graph-canvas" id="memoryGraph"></canvas>
                    </div>
                </div>
            </div>

            <!-- Debug Console -->
            <div class="card">
                <div class="card-title">Debug Console</div>
                <div class="debug-console" id="debugConsole">
                    <div class="log-entry"><span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Waiting for messages...</span></div>
                </div>
                <div class="btn-row mt-12">
                    <button class="btn btn-secondary" id="pauseBtn" onclick="toggleDebugPause()">Pause</button>
                    <button class="btn btn-secondary" onclick="clearDebugConsole()">Clear</button>
                </div>
            </div>
        </section>

        <!-- ===== TEST TAB ===== -->
        <section id="test" class="panel">
            <div class="card">
                <div class="card-title">LED Blink Test</div>
                <div class="led-display">
                    <div id="led" class="led off"></div>
                    <div class="led-status" id="ledStatus">LED is OFF</div>
                </div>
                <button class="btn btn-primary" id="toggleBtn" onclick="toggleBlinking()">Start Blinking</button>
                <div class="info-box mt-12" id="blinkStatus">
                    <div class="text-center">Blinking: <strong id="blinkingState">OFF</strong></div>
                </div>
            </div>
        </section>
    </main>

    <!-- AP Config Modal -->
    <div class="modal-overlay" id="apConfigModal">
        <div class="modal">
            <div class="modal-title">Configure Access Point</div>
            <form onsubmit="submitAPConfig(event)">
                <div class="form-group">
                    <label class="form-label">AP Network Name (SSID)</label>
                    <input type="text" class="form-input" id="apSSID" placeholder="ESP32-LED-Setup">
                </div>
                <div class="form-group">
                    <label class="form-label">AP Password</label>
                    <div class="password-wrapper">
                        <input type="password" class="form-input" id="apPassword" placeholder="Min 8 characters">
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('apPassword', this)">👁</button>
                    </div>
                </div>
                <button type="submit" class="btn btn-primary mb-8">Save AP Settings</button>
                <button type="button" class="btn btn-secondary" onclick="closeAPConfig()">Cancel</button>
            </form>
        </div>
    </div>

    <!-- Release Notes Modal -->
    <div class="modal-overlay" id="releaseNotesModal">
        <div class="modal">
            <div class="modal-title" id="releaseNotesTitle">Release Notes</div>
            <div id="releaseNotesContent" style="white-space: pre-wrap; font-size: 14px; color: var(--text-secondary);"></div>
            <button class="btn btn-secondary mt-16" onclick="closeReleaseNotes()">Close</button>
        </div>
    </div>

    <!-- Toast Notification -->
    <div class="toast" id="toast"></div>

    <script>
        // ===== State Variables =====
        let ws = null;
        let ledState = false;
        let blinkingEnabled = false;
        let autoUpdateEnabled = false;
        let nightMode = true;
        let enableCertValidation = true;
        let currentFirmwareVersion = '';
        let currentLatestVersion = '';
        let currentTimezoneOffset = 3600;
        let manualUploadInProgress = false;
        let debugPaused = false;
        let debugLogBuffer = [];
        const DEBUG_MAX_LINES = 500;

        // Input focus state to prevent overwrites during user input
        let inputFocusState = {
            timerDuration: false,
            voltageThreshold: false
        };

        // WebSocket reconnection
        let wsReconnectDelay = 2000;
        const WS_MIN_RECONNECT_DELAY = 2000;
        const WS_MAX_RECONNECT_DELAY = 30000;

        // Performance History Data
        let historyData = {
            timestamps: [],
            cpuTotal: [],
            cpuCore0: [],
            cpuCore1: [],
            memoryPercent: []
        };
        let maxHistoryPoints = 300;
        let historyCollapsed = true;

        // ===== Tab Switching =====
        function switchTab(tabId) {
            // Update tab buttons
            document.querySelectorAll('.tab').forEach(tab => {
                tab.classList.toggle('active', tab.dataset.tab === tabId);
            });
            // Update panels
            document.querySelectorAll('.panel').forEach(panel => {
                panel.classList.toggle('active', panel.id === tabId);
            });
        }

        // ===== WebSocket =====
        function initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.hostname;
            ws = new WebSocket(`${wsProtocol}//${wsHost}:81`);

            ws.onopen = function() {
                console.log('WebSocket connected');
                updateConnectionStatus(true);
                wsReconnectDelay = WS_MIN_RECONNECT_DELAY;
                fetchUpdateStatus();
            };

            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                console.log('Received:', data);
                
                if (data.type === 'ledState') {
                    ledState = data.state;
                    updateLED();
                } else if (data.type === 'blinkingEnabled') {
                    blinkingEnabled = data.enabled;
                    updateBlinkButton();
                } else if (data.type === 'wifiStatus') {
                    updateWiFiStatus(data);
                } else if (data.type === 'updateStatus') {
                    handleUpdateStatus(data);
                } else if (data.type === 'smartSensing') {
                    updateSmartSensingUI(data);
                } else if (data.type === 'factoryResetProgress') {
                    handlePhysicalResetProgress(data);
                } else if (data.type === 'rebootProgress') {
                    handlePhysicalRebootProgress(data);
                } else if (data.type === 'debugLog') {
                    appendDebugLog(data.timestamp, data.message);
                } else if (data.type === 'hardware_stats') {
                    updateHardwareStats(data);
                } else if (data.type === 'justUpdated') {
                    showUpdateSuccessNotification(data);
                }
            };

            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
                updateConnectionStatus(false);
            };

            ws.onclose = function() {
                console.log('WebSocket disconnected, reconnecting...');
                updateConnectionStatus(false);
                setTimeout(initWebSocket, wsReconnectDelay);
                wsReconnectDelay = Math.min(wsReconnectDelay * 2, WS_MAX_RECONNECT_DELAY);
            };
        }

        // ===== Connection Status =====
        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('connectionStatus');
            if (connected) {
                statusEl.className = 'connection-bar connected';
                statusEl.innerHTML = '<span class="status-dot success"></span> Connected';
            } else {
                statusEl.className = 'connection-bar disconnected';
                statusEl.innerHTML = '<span class="status-dot error"></span> Disconnected';
            }
        }

        // ===== LED Control =====
        function updateLED() {
            const led = document.getElementById('led');
            const status = document.getElementById('ledStatus');
            if (ledState) {
                led.classList.remove('off');
                led.classList.add('on');
                status.textContent = 'LED is ON';
            } else {
                led.classList.remove('on');
                led.classList.add('off');
                status.textContent = 'LED is OFF';
            }
        }

        function updateBlinkButton() {
            const btn = document.getElementById('toggleBtn');
            const state = document.getElementById('blinkingState');
            if (blinkingEnabled) {
                btn.textContent = 'Stop Blinking';
                btn.classList.remove('btn-primary');
                btn.classList.add('btn-danger');
                state.textContent = 'ON';
            } else {
                btn.textContent = 'Start Blinking';
                btn.classList.remove('btn-danger');
                btn.classList.add('btn-primary');
                state.textContent = 'OFF';
            }
        }

        function toggleBlinking() {
            blinkingEnabled = !blinkingEnabled;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'toggle', enabled: blinkingEnabled }));
            }
            updateBlinkButton();
        }

        // ===== WiFi Status =====
        function updateWiFiStatus(data) {
            const statusBox = document.getElementById('wifiStatusBox');
            const apToggle = document.getElementById('apToggle');
            const autoUpdateToggle = document.getElementById('autoUpdateToggle');
            
            let html = '';
            if (data.mode === 'ap') {
                html = `
                    <div class="info-row"><span class="info-label">Mode</span><span class="info-value text-warning">Access Point</span></div>
                    <div class="info-row"><span class="info-label">SSID</span><span class="info-value">${data.apSSID || 'ESP32-LED-Setup'}</span></div>
                    <div class="info-row"><span class="info-label">IP Address</span><span class="info-value">${data.ip || '192.168.4.1'}</span></div>
                    <div class="info-row"><span class="info-label">MAC</span><span class="info-value">${data.mac || 'Unknown'}</span></div>
                `;
                apToggle.checked = true;
            } else if (data.connected) {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Network</span><span class="info-value">${data.ssid || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">IP Address</span><span class="info-value">${data.ip || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Signal</span><span class="info-value">${data.rssi !== undefined ? data.rssi + ' dBm' : 'N/A'}</span></div>
                    <div class="info-row"><span class="info-label">MAC</span><span class="info-value">${data.mac || 'Unknown'}</span></div>
                `;
                apToggle.checked = data.apEnabled || false;
            } else {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-error">Not Connected</span></div>
                    <div class="info-row"><span class="info-label">MAC</span><span class="info-value">${data.mac || 'Unknown'}</span></div>
                `;
                apToggle.checked = data.apEnabled || false;
            }
            statusBox.innerHTML = html;

            if (typeof data.autoUpdateEnabled !== 'undefined') {
                autoUpdateEnabled = !!data.autoUpdateEnabled;
                autoUpdateToggle.checked = autoUpdateEnabled;
            }
            
            if (typeof data.timezoneOffset !== 'undefined') {
                currentTimezoneOffset = data.timezoneOffset;
                document.getElementById('timezoneSelect').value = data.timezoneOffset.toString();
                updateTimezoneDisplay(data.timezoneOffset);
            }
            
            if (typeof data.nightMode !== 'undefined') {
                nightMode = !!data.nightMode;
                document.getElementById('nightModeToggle').checked = nightMode;
                applyTheme(nightMode);
            }
            
            if (typeof data.enableCertValidation !== 'undefined') {
                enableCertValidation = !!data.enableCertValidation;
                document.getElementById('certValidationToggle').checked = enableCertValidation;
            }
            
            if (typeof data.hardwareStatsInterval !== 'undefined') {
                document.getElementById('statsIntervalSelect').value = data.hardwareStatsInterval.toString();
            }
            
            if (data.firmwareVersion) {
                currentFirmwareVersion = data.firmwareVersion;
                document.getElementById('currentVersion').textContent = data.firmwareVersion;
            }
            
            if (data.latestVersion && data.latestVersion !== 'Checking...' && data.latestVersion !== 'Unknown') {
                currentLatestVersion = data.latestVersion;
                document.getElementById('latestVersion').textContent = data.latestVersion;
                document.getElementById('latestVersionRow').style.display = 'flex';
                
                if (data.updateAvailable) {
                    document.getElementById('updateBtn').classList.remove('hidden');
                }
            }
        }

        // ===== Smart Sensing =====
        function updateSensingMode() {
            const selected = document.querySelector('input[name="sensingMode"]:checked');
            if (!selected) return;
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mode: selected.value })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Mode updated', 'success');
            })
            .catch(err => showToast('Failed to update mode', 'error'));
        }

        function updateTimerDuration() {
            const value = parseInt(document.getElementById('timerDuration').value);
            if (isNaN(value) || value < 1 || value > 60) return;
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timerDuration: value })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Timer updated', 'success');
            })
            .catch(err => showToast('Failed to update timer', 'error'));
        }

        function updateVoltageThreshold() {
            const value = parseFloat(document.getElementById('voltageThreshold').value);
            if (isNaN(value) || value < 0.1 || value > 3.3) return;
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ voltageThreshold: value })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Threshold updated', 'success');
            })
            .catch(err => showToast('Failed to update threshold', 'error'));
        }

        function manualOverride(state) {
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ manualOverride: state })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast(state ? 'Turned ON' : 'Turned OFF', 'success');
            })
            .catch(err => showToast('Failed to control amplifier', 'error'));
        }

        function updateSmartSensingUI(data) {
            // Update mode selection
            if (data.mode !== undefined) {
                // Mode can be a string ("always_on", "always_off", "smart_auto") or number (0, 1, 2)
                let modeValue = data.mode;
                if (typeof data.mode === 'number') {
                    const modeMap = { 0: 'always_on', 1: 'always_off', 2: 'smart_auto' };
                    modeValue = modeMap[data.mode] || 'smart_auto';
                }
                document.querySelectorAll('input[name="sensingMode"]').forEach(radio => {
                    radio.checked = (radio.value === modeValue);
                });
            }
            
            // Update timer duration (only if not focused)
            if (data.timerDuration !== undefined && !inputFocusState.timerDuration) {
                document.getElementById('timerDuration').value = data.timerDuration;
            }
            
            // Update voltage threshold (only if not focused)
            if (data.voltageThreshold !== undefined && !inputFocusState.voltageThreshold) {
                document.getElementById('voltageThreshold').value = data.voltageThreshold.toFixed(1);
            }
            
            // Update amplifier status
            if (data.amplifierState !== undefined) {
                const display = document.getElementById('amplifierDisplay');
                const status = document.getElementById('amplifierStatus');
                if (data.amplifierState) {
                    display.classList.add('on');
                    status.textContent = 'ON';
                } else {
                    display.classList.remove('on');
                    status.textContent = 'OFF';
                }
            }
            
            // Update voltage info
            if (data.voltageDetected !== undefined) {
                document.getElementById('voltageDetected').textContent = data.voltageDetected ? 'Yes' : 'No';
            }
            if (data.currentVoltage !== undefined) {
                document.getElementById('voltageReading').textContent = data.currentVoltage.toFixed(2) + 'V';
            }
            
            // Update timer display
            const timerDisplay = document.getElementById('timerDisplay');
            const timerValue = document.getElementById('timerValue');
            if (data.timerActive && data.timerRemaining !== undefined) {
                timerDisplay.classList.remove('hidden');
                const mins = Math.floor(data.timerRemaining / 60);
                const secs = data.timerRemaining % 60;
                timerValue.textContent = `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
            } else {
                timerDisplay.classList.add('hidden');
            }
        }

        // ===== WiFi Configuration =====
        let wifiScanInProgress = false;
        
        function submitWiFiConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('wifiSSID').value;
            const password = document.getElementById('wifiPassword').value;
            
            fetch('/api/wificonfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Connecting to WiFi...', 'success');
                } else {
                    showToast(data.message || 'Connection failed', 'error');
                }
            })
            .catch(err => showToast('Failed to connect', 'error'));
        }
        
        function scanWiFiNetworks() {
            const scanBtn = document.getElementById('scanBtn');
            const scanStatus = document.getElementById('scanStatus');
            const select = document.getElementById('wifiNetworkSelect');
            
            if (wifiScanInProgress) return;
            
            wifiScanInProgress = true;
            scanBtn.disabled = true;
            scanBtn.textContent = '⏳';
            scanStatus.textContent = 'Scanning for networks...';
            
            // Start scan and poll for results
            pollWiFiScan();
        }
        
        function pollWiFiScan() {
            fetch('/api/wifiscan')
                .then(res => res.json())
                .then(data => {
                    const scanBtn = document.getElementById('scanBtn');
                    const scanStatus = document.getElementById('scanStatus');
                    const select = document.getElementById('wifiNetworkSelect');
                    
                    if (data.scanning) {
                        // Still scanning, poll again
                        setTimeout(pollWiFiScan, 1000);
                        return;
                    }
                    
                    // Scan complete
                    wifiScanInProgress = false;
                    scanBtn.disabled = false;
                    scanBtn.textContent = '🔍';
                    
                    // Clear and populate dropdown
                    select.innerHTML = '<option value="">-- Select a network --</option>';
                    
                    if (data.networks && data.networks.length > 0) {
                        // Sort by signal strength (strongest first)
                        data.networks.sort((a, b) => b.rssi - a.rssi);
                        
                        data.networks.forEach(network => {
                            const option = document.createElement('option');
                            option.value = network.ssid;
                            // Show signal strength indicator
                            const signalIcon = network.rssi > -50 ? '📶' : network.rssi > -70 ? '📶' : '📶';
                            const lockIcon = network.encryption === 'secured' ? '🔒' : '🔓';
                            option.textContent = `${network.ssid} ${lockIcon} (${network.rssi} dBm)`;
                            select.appendChild(option);
                        });
                        scanStatus.textContent = `Found ${data.networks.length} network(s)`;
                    } else {
                        scanStatus.textContent = 'No networks found';
                    }
                })
                .catch(err => {
                    wifiScanInProgress = false;
                    const scanBtn = document.getElementById('scanBtn');
                    const scanStatus = document.getElementById('scanStatus');
                    scanBtn.disabled = false;
                    scanBtn.textContent = '🔍';
                    scanStatus.textContent = 'Scan failed';
                    showToast('Failed to scan networks', 'error');
                });
        }
        
        function onNetworkSelect() {
            const select = document.getElementById('wifiNetworkSelect');
            const ssidInput = document.getElementById('wifiSSID');
            
            if (select.value) {
                ssidInput.value = select.value;
            }
        }

        function toggleAP() {
            const enabled = document.getElementById('apToggle').checked;
            fetch('/api/toggleap', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast(enabled ? 'AP enabled' : 'AP disabled', 'success');
            })
            .catch(err => showToast('Failed to toggle AP', 'error'));
        }

        function showAPConfig() {
            document.getElementById('apConfigModal').classList.add('active');
        }

        function closeAPConfig() {
            document.getElementById('apConfigModal').classList.remove('active');
        }

        function submitAPConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('apSSID').value;
            const password = document.getElementById('apPassword').value;
            
            if (password.length > 0 && password.length < 8) {
                showToast('Password must be at least 8 characters', 'error');
                return;
            }
            
            fetch('/api/apconfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('AP settings saved', 'success');
                    closeAPConfig();
                } else {
                    showToast(data.message || 'Failed to save', 'error');
                }
            })
            .catch(err => showToast('Failed to save AP settings', 'error'));
        }

        // ===== MQTT =====
        function loadMqttSettings() {
            fetch('/api/mqtt')
            .then(res => res.json())
            .then(data => {
                document.getElementById('mqttEnabled').checked = data.enabled || false;
                document.getElementById('mqttBroker').value = data.broker || '';
                document.getElementById('mqttPort').value = data.port || 1883;
                document.getElementById('mqttUsername').value = data.username || '';
                document.getElementById('mqttBaseTopic').value = data.baseTopic || '';
                document.getElementById('mqttHADiscovery').checked = data.haDiscovery || false;
                updateMqttConnectionStatus(data.connected, data.deviceId, data.broker, data.port);
            })
            .catch(err => console.error('Failed to load MQTT settings:', err));
        }

        function updateMqttConnectionStatus(connected, deviceId, broker, port) {
            const statusBox = document.getElementById('mqttStatusBox');
            const enabled = document.getElementById('mqttEnabled').checked;
            
            let html = '';
            if (connected) {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                    ${deviceId ? `<div class="info-row"><span class="info-label">Device ID</span><span class="info-value">${deviceId}</span></div>` : ''}
                `;
            } else if (enabled) {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-error">Disconnected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Not configured'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                `;
            } else {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-secondary">Disabled</span></div>
                    <div class="info-row"><span class="info-label">MQTT</span><span class="info-value">Not enabled</span></div>
                `;
            }
            statusBox.innerHTML = html;
        }

        function saveMqttSettings() {
            const settings = {
                enabled: document.getElementById('mqttEnabled').checked,
                broker: document.getElementById('mqttBroker').value,
                port: parseInt(document.getElementById('mqttPort').value) || 1883,
                username: document.getElementById('mqttUsername').value,
                password: document.getElementById('mqttPassword').value,
                baseTopic: document.getElementById('mqttBaseTopic').value,
                haDiscovery: document.getElementById('mqttHADiscovery').checked
            };
            
            fetch('/api/mqtt', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(settings)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('MQTT settings saved', 'success');
                    setTimeout(loadMqttSettings, 2000);
                } else {
                    showToast(data.message || 'Failed to save', 'error');
                }
            })
            .catch(err => showToast('Failed to save MQTT settings', 'error'));
        }

        // ===== Settings =====
        function updateTimezone() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezoneOffset: offset })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Timezone updated', 'success');
                    updateTimezoneDisplay(offset);
                }
            })
            .catch(err => showToast('Failed to update timezone', 'error'));
        }

        function updateTimezoneDisplay(offset) {
            const hours = offset / 3600;
            const sign = hours >= 0 ? '+' : '';
            document.getElementById('timezoneInfo').textContent = `UTC${sign}${hours} hours`;
        }

        function toggleTheme() {
            nightMode = document.getElementById('nightModeToggle').checked;
            applyTheme(nightMode);
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ nightMode })
            });
        }

        function applyTheme(isNightMode) {
            if (isNightMode) {
                document.body.classList.add('night-mode');
                document.querySelector('meta[name="theme-color"]').setAttribute('content', '#121212');
            } else {
                document.body.classList.remove('night-mode');
                document.querySelector('meta[name="theme-color"]').setAttribute('content', '#F5F5F5');
            }
        }

        function toggleAutoUpdate() {
            autoUpdateEnabled = document.getElementById('autoUpdateToggle').checked;
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ autoUpdateEnabled: autoUpdateEnabled })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast(autoUpdateEnabled ? 'Auto-update enabled' : 'Auto-update disabled', 'success');
            })
            .catch(err => showToast('Failed to update setting', 'error'));
        }

        function toggleCertValidation() {
            enableCertValidation = document.getElementById('certValidationToggle').checked;
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enableCertValidation: enableCertValidation })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast(enableCertValidation ? 'SSL validation enabled' : 'SSL validation disabled', 'success');
            })
            .catch(err => showToast('Failed to update setting', 'error'));
        }

        function setStatsInterval() {
            const interval = parseInt(document.getElementById('statsIntervalSelect').value);
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ hardwareStatsInterval: interval })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Stats interval set to ' + interval + 's', 'success');
            })
            .catch(err => showToast('Failed to update interval', 'error'));
        }

        // ===== Firmware Update =====
        function checkForUpdate() {
            showToast('Checking for updates...', 'info');
            fetch('/api/checkupdate')
            .then(res => res.json())
            .then(data => {
                if (data.updateAvailable) {
                    document.getElementById('latestVersion').textContent = data.latestVersion;
                    document.getElementById('latestVersionRow').style.display = 'flex';
                    document.getElementById('updateBtn').classList.remove('hidden');
                    showToast(`Update available: ${data.latestVersion}`, 'success');
                } else {
                    showToast('Firmware is up to date', 'success');
                }
            })
            .catch(err => showToast('Failed to check for updates', 'error'));
        }

        function fetchUpdateStatus() {
            fetch('/api/updatestatus')
            .then(res => res.json())
            .then(data => handleUpdateStatus(data))
            .catch(err => console.error('Failed to fetch update status:', err));
        }

        function startOTAUpdate() {
            fetch('/api/startupdate', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    document.getElementById('progressContainer').classList.remove('hidden');
                    document.getElementById('progressStatus').classList.remove('hidden');
                    showToast('Update started', 'success');
                } else {
                    showToast(data.message || 'Failed to start update', 'error');
                }
            })
            .catch(err => showToast('Failed to start update', 'error'));
        }

        function handleUpdateStatus(data) {
            if (data.status === 'downloading' || data.status === 'uploading') {
                const container = document.getElementById('progressContainer');
                const bar = document.getElementById('progressBar');
                const status = document.getElementById('progressStatus');
                
                container.classList.remove('hidden');
                status.classList.remove('hidden');
                bar.style.width = data.progress + '%';
                status.textContent = data.message || `${data.progress}%`;
            } else if (data.status === 'complete') {
                showToast('Update complete! Rebooting...', 'success');
            } else if (data.status === 'error') {
                document.getElementById('progressContainer').classList.add('hidden');
                document.getElementById('progressStatus').classList.add('hidden');
                showToast(data.message || 'Update failed', 'error');
            }
        }

        function showUpdateSuccessNotification(data) {
            showToast(`Firmware updated: ${data.previousVersion} → ${data.currentVersion}`, 'success');
        }

        // ===== Manual Firmware Upload =====
        function handleFirmwareSelect(event) {
            const file = event.target.files[0];
            if (file) uploadFirmware(file);
        }

        function uploadFirmware(file) {
            if (!file.name.endsWith('.bin')) {
                showToast('Please select a .bin file', 'error');
                return;
            }
            
            manualUploadInProgress = true;
            const container = document.getElementById('progressContainer');
            const bar = document.getElementById('progressBar');
            const status = document.getElementById('progressStatus');
            
            container.classList.remove('hidden');
            status.classList.remove('hidden');
            status.textContent = 'Uploading...';
            
            const formData = new FormData();
            formData.append('firmware', file);
            
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/api/firmware/upload', true);
            
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    bar.style.width = percent + '%';
                    status.textContent = `Uploading: ${percent}%`;
                }
            };
            
            xhr.onload = function() {
                manualUploadInProgress = false;
                if (xhr.status === 200) {
                    const response = JSON.parse(xhr.responseText);
                    if (response.success) {
                        status.textContent = 'Upload complete! Rebooting...';
                        showToast('Firmware uploaded successfully', 'success');
                    } else {
                        container.classList.add('hidden');
                        status.classList.add('hidden');
                        showToast(response.message || 'Upload failed', 'error');
                    }
                } else {
                    container.classList.add('hidden');
                    status.classList.add('hidden');
                    showToast('Upload failed', 'error');
                }
            };
            
            xhr.onerror = function() {
                manualUploadInProgress = false;
                container.classList.add('hidden');
                status.classList.add('hidden');
                showToast('Upload failed', 'error');
            };
            
            xhr.send(formData);
        }

        // Drag and drop for firmware
        function initFirmwareDragDrop() {
            const dropZone = document.getElementById('firmwareDropZone');
            
            ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, e => {
                    e.preventDefault();
                    e.stopPropagation();
                });
            });
            
            ['dragenter', 'dragover'].forEach(eventName => {
                dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'));
            });
            
            ['dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'));
            });
            
            dropZone.addEventListener('drop', e => {
                const file = e.dataTransfer.files[0];
                if (file) uploadFirmware(file);
            });
        }

        // ===== Export/Import Settings =====
        function exportSettings() {
            fetch('/api/settings/export')
            .then(res => res.json())
            .then(data => {
                const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'alx-settings.json';
                a.click();
                URL.revokeObjectURL(url);
                showToast('Settings exported', 'success');
            })
            .catch(err => showToast('Failed to export settings', 'error'));
        }

        function handleFileSelect(event) {
            const file = event.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const settings = JSON.parse(e.target.result);
                    importSettings(settings);
                } catch (err) {
                    showToast('Invalid settings file', 'error');
                }
            };
            reader.readAsText(file);
        }

        function importSettings(settings) {
            fetch('/api/settings/import', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(settings)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Settings imported. Rebooting...', 'success');
                } else {
                    showToast(data.message || 'Import failed', 'error');
                }
            })
            .catch(err => showToast('Failed to import settings', 'error'));
        }

        // ===== Reboot & Factory Reset =====
        function startReboot() {
            if (confirm('Are you sure you want to reboot the device?')) {
                fetch('/api/reboot', { method: 'POST' })
                .then(res => res.json())
                .then(data => {
                    if (data.success) showToast('Rebooting...', 'success');
                })
                .catch(err => showToast('Failed to reboot', 'error'));
            }
        }

        function startFactoryReset() {
            if (confirm('Are you sure? This will erase all settings!')) {
                fetch('/api/factoryreset', { method: 'POST' })
                .then(res => res.json())
                .then(data => {
                    if (data.success) showToast('Factory reset in progress...', 'success');
                })
                .catch(err => showToast('Failed to reset', 'error'));
            }
        }

        function handlePhysicalResetProgress(data) {
            showToast(`Factory reset: ${data.progress}%`, 'info');
        }

        function handlePhysicalRebootProgress(data) {
            showToast(`Rebooting: ${data.progress}%`, 'info');
        }

        // ===== Hardware Stats =====
        function updateHardwareStats(data) {
            // CPU Stats
            if (data.cpu) {
                document.getElementById('cpuTotal').textContent = Math.round(data.cpu.usageTotal || 0) + '%';
                document.getElementById('cpuCore0').textContent = Math.round(data.cpu.usageCore0 || 0) + '%';
                document.getElementById('cpuCore1').textContent = Math.round(data.cpu.usageCore1 || 0) + '%';
                document.getElementById('cpuTemp').textContent = (data.cpu.temperature || 0).toFixed(1) + '°C';
                document.getElementById('cpuFreq').textContent = (data.cpu.freqMHz || 0) + ' MHz';
                document.getElementById('cpuModel').textContent = data.cpu.model || '--';
                document.getElementById('cpuRevision').textContent = data.cpu.revision || '--';
                document.getElementById('cpuCores').textContent = data.cpu.cores || '--';
            }
            
            // Memory Stats (Heap)
            if (data.memory) {
                const heapTotal = data.memory.heapTotal || 0;
                const heapFree = data.memory.heapFree || 0;
                const heapPercent = heapTotal > 0 ? Math.round((1 - heapFree / heapTotal) * 100) : 0;
                document.getElementById('heapPercent').textContent = heapPercent + '%';
                document.getElementById('heapFree').textContent = formatBytes(heapFree);
                document.getElementById('heapTotal').textContent = formatBytes(heapTotal);
                document.getElementById('heapMinFree').textContent = formatBytes(data.memory.heapMinFree || 0);
                document.getElementById('heapMaxBlock').textContent = formatBytes(data.memory.heapMaxBlock || 0);
                
                // PSRAM
                const psramTotal = data.memory.psramTotal || 0;
                const psramFree = data.memory.psramFree || 0;
                const psramPercent = psramTotal > 0 ? Math.round((1 - psramFree / psramTotal) * 100) : 0;
                document.getElementById('psramPercent').textContent = psramTotal > 0 ? psramPercent + '%' : 'N/A';
                document.getElementById('psramFree').textContent = psramTotal > 0 ? formatBytes(psramFree) : 'N/A';
                document.getElementById('psramTotal').textContent = psramTotal > 0 ? formatBytes(psramTotal) : 'N/A';
            }
            
            // Storage Stats
            if (data.storage) {
                document.getElementById('flashSize').textContent = formatBytes(data.storage.flashSize || 0);
                document.getElementById('sketchSize').textContent = formatBytes(data.storage.sketchSize || 0);
                document.getElementById('sketchFree').textContent = formatBytes(data.storage.sketchFree || 0);
                
                const sketchTotal = (data.storage.sketchSize || 0) + (data.storage.sketchFree || 0);
                const sketchPercent = sketchTotal > 0 ? Math.round((data.storage.sketchSize / sketchTotal) * 100) : 0;
                document.getElementById('sketchPercent').textContent = sketchPercent + '%';
                
                const spiffsTotal = data.storage.spiffsTotal || 0;
                const spiffsUsed = data.storage.spiffsUsed || 0;
                const spiffsPercent = spiffsTotal > 0 ? Math.round((spiffsUsed / spiffsTotal) * 100) : 0;
                document.getElementById('spiffsPercent').textContent = spiffsTotal > 0 ? spiffsPercent + '%' : 'N/A';
                document.getElementById('spiffsUsed').textContent = spiffsTotal > 0 ? formatBytes(spiffsUsed) : 'N/A';
                document.getElementById('spiffsTotal').textContent = spiffsTotal > 0 ? formatBytes(spiffsTotal) : 'N/A';
            }
            
            // WiFi Stats
            if (data.wifi) {
                document.getElementById('wifiRssi').textContent = (data.wifi.rssi || 0) + ' dBm';
                document.getElementById('wifiChannel').textContent = data.wifi.channel || '--';
                document.getElementById('apClients').textContent = data.wifi.apClients || 0;
            }
            
            // Uptime
            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
            }
            
            // Add to history
            addHistoryDataPoint(data);
        }

        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1048576).toFixed(1) + ' MB';
        }

        function formatUptime(ms) {
            const seconds = Math.floor(ms / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            const days = Math.floor(hours / 24);
            
            if (days > 0) return `${days}d ${hours % 24}h`;
            if (hours > 0) return `${hours}h ${minutes % 60}m`;
            if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
            return `${seconds}s`;
        }

        function addHistoryDataPoint(data) {
            historyData.timestamps.push(Date.now());
            historyData.cpuTotal.push(data.cpu ? (data.cpu.usageTotal || 0) : 0);
            historyData.cpuCore0.push(data.cpu ? (data.cpu.usageCore0 || 0) : 0);
            historyData.cpuCore1.push(data.cpu ? (data.cpu.usageCore1 || 0) : 0);
            
            if (data.memory && data.memory.heapTotal > 0) {
                const memPercent = (1 - data.memory.heapFree / data.memory.heapTotal) * 100;
                historyData.memoryPercent.push(memPercent);
            } else {
                historyData.memoryPercent.push(0);
            }
            
            // Trim to max points
            while (historyData.timestamps.length > maxHistoryPoints) {
                historyData.timestamps.shift();
                historyData.cpuTotal.shift();
                historyData.cpuCore0.shift();
                historyData.cpuCore1.shift();
                historyData.memoryPercent.shift();
            }
            
            // Redraw graphs if visible
            if (!historyCollapsed) {
                drawCpuGraph();
                drawMemoryGraph();
            }
        }

        function toggleHistorySection() {
            historyCollapsed = !historyCollapsed;
            const content = document.getElementById('historyContent');
            const chevron = document.getElementById('historyChevron');
            
            if (historyCollapsed) {
                content.classList.remove('open');
                chevron.parentElement.classList.remove('open');
            } else {
                content.classList.add('open');
                chevron.parentElement.classList.add('open');
                drawCpuGraph();
                drawMemoryGraph();
            }
        }

        function drawCpuGraph() {
            const canvas = document.getElementById('cpuGraph');
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * window.devicePixelRatio;
            canvas.height = rect.height * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
            
            const w = rect.width;
            const h = rect.height;
            
            // Background
            ctx.fillStyle = '#1E1E1E';
            ctx.fillRect(0, 0, w, h);
            
            // Draw grid lines
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 4; i++) {
                const y = (h / 4) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
            }
            
            if (historyData.cpuTotal.length < 2) return;
            
            const step = w / (historyData.cpuTotal.length - 1);
            
            // Draw Core 0 (light orange)
            ctx.strokeStyle = '#FFB74D';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            historyData.cpuCore0.forEach((val, i) => {
                const x = i * step;
                const y = h - (val / 100) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            });
            ctx.stroke();
            
            // Draw Core 1 (dark orange)
            ctx.strokeStyle = '#F57C00';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            historyData.cpuCore1.forEach((val, i) => {
                const x = i * step;
                const y = h - (val / 100) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            });
            ctx.stroke();
            
            // Draw Total (bright orange, on top)
            ctx.strokeStyle = '#FF9800';
            ctx.lineWidth = 2;
            ctx.beginPath();
            historyData.cpuTotal.forEach((val, i) => {
                const x = i * step;
                const y = h - (val / 100) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            });
            ctx.stroke();
        }

        function drawMemoryGraph() {
            const canvas = document.getElementById('memoryGraph');
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * window.devicePixelRatio;
            canvas.height = rect.height * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
            
            const w = rect.width;
            const h = rect.height;
            
            // Background
            ctx.fillStyle = '#1E1E1E';
            ctx.fillRect(0, 0, w, h);
            
            // Draw grid lines
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 4; i++) {
                const y = (h / 4) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
            }
            
            if (historyData.memoryPercent.length < 2) return;
            
            const step = w / (historyData.memoryPercent.length - 1);
            
            // Draw memory line
            ctx.strokeStyle = '#2196F3';
            ctx.lineWidth = 2;
            ctx.beginPath();
            historyData.memoryPercent.forEach((val, i) => {
                const x = i * step;
                const y = h - (val / 100) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            });
            ctx.stroke();
        }

        // ===== Debug Console =====
        function appendDebugLog(timestamp, message) {
            if (debugPaused) {
                debugLogBuffer.push({ timestamp, message });
                return;
            }
            
            const console = document.getElementById('debugConsole');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            
            const ts = formatDebugTimestamp(timestamp);
            let msgClass = 'log-message';
            if (message.includes('Error') || message.includes('❌')) msgClass += ' error';
            else if (message.includes('Warning') || message.includes('⚠')) msgClass += ' warning';
            else if (message.includes('✅') || message.includes('Success')) msgClass += ' success';
            else if (message.includes('ℹ') || message.includes('Info')) msgClass += ' info';
            
            entry.innerHTML = `<span class="log-timestamp">[${ts}]</span><span class="${msgClass}">${message}</span>`;
            console.appendChild(entry);
            
            // Limit entries
            while (console.children.length > DEBUG_MAX_LINES) {
                console.removeChild(console.firstChild);
            }
            
            console.scrollTop = console.scrollHeight;
        }

        function formatDebugTimestamp(millis) {
            const date = new Date(millis);
            const h = date.getHours().toString().padStart(2, '0');
            const m = date.getMinutes().toString().padStart(2, '0');
            const s = date.getSeconds().toString().padStart(2, '0');
            const ms = date.getMilliseconds().toString().padStart(3, '0');
            return `${h}:${m}:${s}.${ms}`;
        }

        function toggleDebugPause() {
            debugPaused = !debugPaused;
            const btn = document.getElementById('pauseBtn');
            if (debugPaused) {
                btn.textContent = 'Resume';
            } else {
                btn.textContent = 'Pause';
                // Flush buffer
                debugLogBuffer.forEach(log => appendDebugLog(log.timestamp, log.message));
                debugLogBuffer = [];
            }
        }

        function clearDebugConsole() {
            const console = document.getElementById('debugConsole');
            console.innerHTML = '<div class="log-entry"><span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span></div>';
            debugLogBuffer = [];
        }

        // ===== Release Notes =====
        function showReleaseNotes() {
            if (!currentLatestVersion) return;
            
            fetch(`/api/releasenotes?version=${currentLatestVersion}`)
            .then(res => res.json())
            .then(data => {
                document.getElementById('releaseNotesTitle').textContent = `Release Notes v${currentLatestVersion}`;
                document.getElementById('releaseNotesContent').textContent = data.notes || 'No release notes available.';
                document.getElementById('releaseNotesModal').classList.add('active');
            })
            .catch(err => showToast('Failed to load release notes', 'error'));
        }

        function closeReleaseNotes() {
            document.getElementById('releaseNotesModal').classList.remove('active');
        }

        // ===== Utilities =====
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

        function showToast(message, type = 'info') {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = 'toast show ' + type;
            
            setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }

        // ===== Initialization =====
        window.onload = function() {
            initWebSocket();
            loadMqttSettings();
            initFirmwareDragDrop();
            
            // Add input focus listeners
            document.getElementById('timerDuration').addEventListener('focus', () => inputFocusState.timerDuration = true);
            document.getElementById('timerDuration').addEventListener('blur', () => inputFocusState.timerDuration = false);
            document.getElementById('voltageThreshold').addEventListener('focus', () => inputFocusState.voltageThreshold = true);
            document.getElementById('voltageThreshold').addEventListener('blur', () => inputFocusState.voltageThreshold = false);
        };
    </script>
</body>
</html>
)rawliteral";

const char apHtmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, viewport-fit=cover">
    <meta name="theme-color" content="#121212">
    <title>ALX Audio - WiFi Setup</title>
    <style>
        :root {
            --bg-primary: #121212;
            --bg-surface: #1E1E1E;
            --bg-card: #252525;
            --bg-input: #2a2a2a;
            --accent: #FF9800;
            --accent-dark: #F57C00;
            --text-primary: #FFFFFF;
            --text-secondary: #B3B3B3;
            --text-disabled: #666666;
            --success: #4CAF50;
            --error: #CF6679;
            --border: #333333;
            --safe-top: env(safe-area-inset-top, 0px);
            --safe-bottom: env(safe-area-inset-bottom, 0px);
        }

        *, *::before, *::after {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        html {
            font-size: 16px;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            min-height: 100dvh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 16px;
            padding-top: calc(16px + var(--safe-top));
            padding-bottom: calc(16px + var(--safe-bottom));
        }

        .container {
            width: 100%;
            max-width: 400px;
        }

        .logo {
            text-align: center;
            margin-bottom: 32px;
        }

        .logo svg {
            width: 64px;
            height: 64px;
            fill: var(--accent);
        }

        .logo h1 {
            font-size: 24px;
            margin-top: 12px;
        }

        .logo p {
            font-size: 14px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .card {
            background: var(--bg-surface);
            border-radius: 16px;
            padding: 24px;
        }

        .form-group {
            margin-bottom: 16px;
        }

        .form-label {
            display: block;
            font-size: 14px;
            color: var(--text-secondary);
            margin-bottom: 8px;
        }

        .form-input {
            width: 100%;
            height: 48px;
            padding: 12px 16px;
            font-size: 16px;
            color: var(--text-primary);
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 8px;
            outline: none;
        }

        .form-input:focus {
            border-color: var(--accent);
        }

        .form-input::placeholder {
            color: var(--text-disabled);
        }

        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            width: 100%;
            height: 48px;
            padding: 12px 24px;
            font-size: 16px;
            font-weight: 600;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            background: var(--accent);
            color: #000;
            margin-top: 8px;
        }

        .btn:active {
            background: var(--accent-dark);
        }

        .status-message {
            margin-top: 16px;
            padding: 12px;
            border-radius: 8px;
            text-align: center;
            font-size: 14px;
            display: none;
        }

        .status-message.success {
            display: block;
            background: rgba(76, 175, 80, 0.2);
            color: var(--success);
        }

        .status-message.error {
            display: block;
            background: rgba(207, 102, 121, 0.2);
            color: var(--error);
        }

        .info-text {
            text-align: center;
            font-size: 13px;
            color: var(--text-secondary);
            margin-top: 16px;
        }

        .current-ap {
            text-align: center;
            font-size: 14px;
            color: var(--text-secondary);
            margin-bottom: 24px;
            padding: 12px;
            background: var(--bg-card);
            border-radius: 8px;
        }

        .password-wrapper {
            position: relative;
        }

        .password-toggle {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            color: var(--text-secondary);
            cursor: pointer;
            padding: 8px;
            font-size: 18px;
        }

        .password-wrapper .form-input {
            padding-right: 48px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <svg viewBox="0 0 24 24"><path d="M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"/></svg>
            <h1>ALX Audio</h1>
            <p>WiFi Configuration</p>
        </div>

        <div class="current-ap" id="currentAPInfo">Loading AP info...</div>

        <div class="card">
            <form onsubmit="submitWiFi(event)">
                <div class="form-group">
                    <label class="form-label">Network Name (SSID)</label>
                    <input type="text" class="form-input" id="ssid" inputmode="text" placeholder="Enter WiFi network name" required>
                </div>
                <div class="form-group">
                    <label class="form-label">Password</label>
                    <div class="password-wrapper">
                        <input type="password" class="form-input" id="password" placeholder="Enter WiFi password" required>
                        <button type="button" class="password-toggle" onclick="togglePassword()">👁</button>
                    </div>
                </div>
                <button type="submit" class="btn">Connect to WiFi</button>
            </form>
            <div class="status-message" id="statusMessage"></div>
        </div>

        <p class="info-text">Enter your WiFi credentials to connect the device to your network.</p>
    </div>

    <script>
        window.onload = function() {
            fetch('/api/wifistatus')
            .then(res => res.json())
            .then(data => {
                const apInfo = document.getElementById('currentAPInfo');
                if (data.apSSID) {
                    apInfo.textContent = 'Access Point: ' + data.apSSID;
                } else {
                    apInfo.textContent = 'Access Point: ESP32-Setup';
                }
            })
            .catch(err => {
                document.getElementById('currentAPInfo').textContent = 'Access Point Mode';
            });
        };

        function submitWiFi(event) {
            event.preventDefault();
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const statusMsg = document.getElementById('statusMessage');
            
            statusMsg.className = 'status-message';
            statusMsg.style.display = 'none';
            
            fetch('/api/wificonfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    statusMsg.className = 'status-message success';
                    statusMsg.textContent = 'Connecting to WiFi... Please wait.';
                    setTimeout(() => window.location.href = '/', 5000);
                } else {
                    statusMsg.className = 'status-message error';
                    statusMsg.textContent = data.message || 'Connection failed';
                }
            })
            .catch(err => {
                statusMsg.className = 'status-message error';
                statusMsg.textContent = 'Error: ' + err.message;
            });
        }

        function togglePassword() {
            const input = document.getElementById('password');
            const btn = event.target;
            if (input.type === 'password') {
                input.type = 'text';
                btn.textContent = '🙈';
            } else {
                input.type = 'password';
                btn.textContent = '👁';
            }
        }
    </script>
</body>
</html>
)rawliteral";
