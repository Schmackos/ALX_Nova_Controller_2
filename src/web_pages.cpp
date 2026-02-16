#include "web_pages.h"
#include <pgmspace.h>

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
            --accent-dark: #E68900;
            --text-primary: #212121;
            --text-secondary: #757575;
            --text-disabled: #9E9E9E;
            --success: #4CAF50;
            --error: #F44336;
            --warning: #FFC107;
            --info: #2196F3;
            --border: #E0E0E0;
            --shadow: rgba(0, 0, 0, 0.1);
            --tab-height: 56px;
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
            padding-top: var(--safe-top);
            padding-bottom: calc(var(--tab-height) + var(--safe-bottom));
        }

        /* ===== Tab Navigation Bar ===== */
        .tab-bar {
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
            height: calc(var(--tab-height) + var(--safe-bottom));
            padding-bottom: var(--safe-bottom);
            background: var(--bg-surface);
            display: flex;
            justify-content: space-around;
            align-items: center;
            border-top: 1px solid var(--border);
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
            top: 0;
            left: 50%;
            transform: translateX(-50%);
            width: 32px;
            height: 3px;
            background: var(--accent);
            border-radius: 0 0 3px 3px;
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

        .graph-disabled {
            opacity: 0.3;
            pointer-events: none;
        }

        /* ===== Skeleton Loading ===== */
        .skeleton {
            background: linear-gradient(90deg, var(--bg-card) 25%, var(--border) 50%, var(--bg-card) 75%);
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

        .btn-small {
            padding: 6px 12px;
            font-size: 13px;
            min-width: auto;
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

        .info-box-compact {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 0 12px;
            padding: 8px;
            font-size: 13px;
            background: var(--bg-card);
            border-radius: 8px;
            color: var(--text-secondary);
            margin-bottom: 12px;
        }

        .info-box-compact .info-row {
            padding: 6px 8px;
            font-size: 12px;
        }

        /* Mobile: single column */
        @media (max-width: 767px) {
            .info-box-compact {
                grid-template-columns: 1fr;
            }
        }

        /* ===== Task Monitor Table ===== */
        .task-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
            font-family: 'Courier New', monospace;
        }
        .task-table th {
            text-align: left;
            padding: 4px 6px;
            border-bottom: 2px solid var(--accent);
            color: var(--accent);
            font-weight: 600;
            font-size: 10px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .task-table td {
            padding: 3px 6px;
            border-bottom: 1px solid var(--border);
        }
        .task-table tr:last-child td {
            border-bottom: none;
        }
        .task-stack-bar {
            display: inline-block;
            height: 8px;
            border-radius: 2px;
            min-width: 4px;
        }
        .task-table th.sortable {
            cursor: pointer;
            user-select: none;
            white-space: nowrap;
        }
        .task-table th.sortable:hover {
            color: var(--accent-light);
        }
        .task-table th .sort-arrow {
            font-size: 8px;
            opacity: 0.3;
        }
        .task-table th .sort-arrow.asc::after { content: '\25B2'; opacity: 1; }
        .task-table th .sort-arrow.desc::after { content: '\25BC'; opacity: 1; }
        .task-stack-ok { background: var(--success-color); }
        .task-stack-warn { background: #f0ad4e; }
        .task-stack-crit { background: var(--error-color); }
        .task-pct-text { font-size: 10px; }
        .task-pct-ok { color: var(--success); }
        .task-pct-warn { color: #f0ad4e; }
        .task-pct-crit { color: var(--error); }

        /* ===== Pin Configuration Table ===== */
        .pin-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 13px;
        }

        .pin-table th {
            text-align: left;
            padding: 6px 8px;
            border-bottom: 2px solid var(--accent);
            color: var(--accent);
            font-weight: 600;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            cursor: pointer;
            user-select: none;
            white-space: nowrap;
        }

        .pin-table th:hover {
            color: var(--accent-light);
        }

        .pin-table th .sort-arrow {
            margin-left: 4px;
            font-size: 10px;
            opacity: 0.4;
        }

        .pin-table th.sorted .sort-arrow {
            opacity: 1;
        }

        .pin-table td {
            padding: 5px 8px;
            border-bottom: 1px solid var(--border);
        }

        .pin-table tr:last-child td {
            border-bottom: none;
        }

        .pin-cat {
            font-size: 10px;
            font-weight: 600;
            text-transform: uppercase;
            padding: 2px 6px;
            border-radius: 3px;
            display: inline-block;
        }

        .pin-cat-core { background: var(--accent); color: #fff; }
        .pin-cat-input { background: var(--success); color: #fff; }
        .pin-cat-display { background: var(--info); color: #fff; }
        .pin-cat-audio { background: #9C27B0; color: #fff; }

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
        .log-message.debug { color: #9E9E9E; }
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
            bottom: calc(20px + var(--tab-height) + var(--safe-bottom));
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

        .graph-embedded {
            background: #1A1A1A;
            border-radius: 8px;
            padding: 12px;
            padding-left: 40px;
            padding-bottom: 28px;
            margin-top: 12px;
        }

        .graph-legend {
            font-size: 11px;
            color: var(--text-secondary);
            text-align: center;
            margin-bottom: 8px;
        }

        .graph-embedded .graph-canvas {
            width: 100%;
            height: 140px;
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

        .select-sm {
            padding: 4px 8px;
            font-size: 12px;
            color: var(--text-primary);
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 4px;
            outline: none;
        }
        .select-sm:focus { border-color: var(--accent); }

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

        /* ===== Release Notes Link ===== */
        .release-notes-link {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 18px;
            height: 18px;
            margin-left: 6px;
            font-size: 12px;
            font-weight: 600;
            color: var(--text-primary);
            background: var(--accent);
            border-radius: 50%;
            text-decoration: none;
            vertical-align: middle;
        }

        .release-notes-link:hover {
            background: var(--accent-dark);
        }

        /* ===== User Manual QR Code Section ===== */
        .qr-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 16px;
            gap: 12px;
        }

        .qr-code {
            background: #fff;
            padding: 12px;
            border-radius: 8px;
            display: inline-block;
        }

        .qr-code canvas {
            display: block;
        }

        .manual-link {
            color: var(--accent);
            text-decoration: none;
            font-size: 14px;
            word-break: break-all;
            text-align: center;
        }

        .manual-link:hover {
            text-decoration: underline;
        }

        .manual-description {
            font-size: 13px;
            color: var(--text-secondary);
            text-align: center;
            margin-top: 4px;
        }

        /* ===== Manual Rendered Content ===== */
        .search-input {
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 8px;
            color: var(--text-primary);
            padding: 6px 12px;
            font-size: 13px;
            outline: none;
            width: 200px;
        }
        .search-input:focus {
            border-color: var(--accent);
        }
        .manual-rendered {
            max-height: 60vh;
            overflow-y: auto;
            padding: 12px;
            font-size: 14px;
            line-height: 1.6;
            color: var(--text-primary);
        }
        .manual-rendered h1 { font-size: 20px; margin: 16px 0 8px; color: var(--accent); }
        .manual-rendered h2 { font-size: 17px; margin: 14px 0 6px; color: var(--accent); border-bottom: 1px solid var(--border); padding-bottom: 4px; }
        .manual-rendered h3 { font-size: 15px; margin: 12px 0 4px; color: var(--text-primary); }
        .manual-rendered p { margin: 6px 0; }
        .manual-rendered ul, .manual-rendered ol { padding-left: 20px; margin: 6px 0; }
        .manual-rendered li { margin: 3px 0; }
        .manual-rendered code { background: var(--bg-input); padding: 2px 6px; border-radius: 4px; font-size: 13px; }
        .manual-rendered pre { background: var(--bg-input); padding: 12px; border-radius: 8px; overflow-x: auto; margin: 8px 0; }
        .manual-rendered pre code { padding: 0; background: none; }
        .manual-rendered a { color: var(--accent); }
        .search-highlight { background: rgba(255, 152, 0, 0.3); border-radius: 2px; }
        .manual-loading { color: var(--text-secondary); font-size: 13px; padding: 16px; text-align: center; }
        .manual-search-status { color: var(--text-secondary); font-size: 12px; padding: 4px 12px; }

        /* ===== ADAPTIVE DESIGN ===== */
        
        /* --- Persistent Status Bar --- */
        .status-bar {
            position: fixed;
            top: var(--safe-top);
            left: 0;
            right: 0;
            height: 36px;
            background: var(--bg-surface);
            border-bottom: 1px solid var(--border);
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 16px;
            padding: 0 16px;
            z-index: 999;
            font-size: 12px;
            transition: top 0.3s ease;
        }

        .status-item {
            display: flex;
            align-items: center;
            gap: 6px;
            color: var(--text-secondary);
        }

        .status-item .status-indicator {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--text-disabled);
            transition: background 0.3s ease, box-shadow 0.3s ease;
        }

        .status-item .status-indicator.online {
            background: var(--success);
            box-shadow: 0 0 8px var(--success);
        }

        .status-item .status-indicator.offline {
            background: var(--error);
        }

        .status-item .status-indicator.warning {
            background: var(--warning);
        }

        .status-item .status-indicator.active {
            background: var(--accent);
            box-shadow: 0 0 8px var(--accent);
        }

        body.has-status-bar {
            padding-top: calc(var(--safe-top) + 36px);
        }

        /* --- Sidebar Navigation (Desktop) --- */
        .sidebar {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            bottom: 0;
            width: 240px;
            background: var(--bg-surface);
            border-right: 1px solid var(--border);
            z-index: 1001;
            flex-direction: column;
            padding-top: var(--safe-top);
            transition: transform 0.3s ease, width 0.3s ease;
        }

        .sidebar-header {
            padding: 20px 16px;
            border-bottom: 1px solid var(--border);
        }

        .sidebar-logo {
            display: flex;
            align-items: center;
            gap: 12px;
            font-size: 18px;
            font-weight: 600;
            color: var(--text-primary);
        }

        .sidebar-logo svg {
            width: 32px;
            height: 32px;
            fill: var(--accent);
        }

        .sidebar-nav {
            flex: 1;
            padding: 16px 0;
            overflow-y: auto;
        }

        .sidebar-item {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 14px 20px;
            color: var(--text-secondary);
            text-decoration: none;
            transition: all 0.2s ease;
            cursor: pointer;
            border: none;
            background: none;
            width: 100%;
            text-align: left;
            font-size: 15px;
        }

        .sidebar-item svg {
            width: 22px;
            height: 22px;
            fill: currentColor;
            flex-shrink: 0;
        }

        .sidebar-item:hover {
            background: var(--bg-card);
            color: var(--text-primary);
        }

        .sidebar-item.active {
            color: var(--accent);
            background: rgba(255, 152, 0, 0.1);
            border-right: 3px solid var(--accent);
        }

        .sidebar-item span {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .sidebar-footer {
            padding: 16px;
            border-top: 1px solid var(--border);
            font-size: 12px;
            color: var(--text-secondary);
        }

        /* Sidebar collapsed state */
        .sidebar.collapsed {
            width: 64px;
        }

        .sidebar.collapsed .sidebar-item span,
        .sidebar.collapsed .sidebar-logo span,
        .sidebar.collapsed .sidebar-footer {
            display: none;
        }

        .sidebar.collapsed .sidebar-item {
            justify-content: center;
            padding: 14px;
        }

        .sidebar-toggle {
            position: absolute;
            bottom: 60px;
            right: -12px;
            width: 24px;
            height: 24px;
            background: var(--bg-surface);
            border: 1px solid var(--border);
            border-radius: 50%;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: transform 0.3s ease;
        }

        .sidebar-toggle svg {
            width: 14px;
            height: 14px;
            fill: var(--text-secondary);
            transition: transform 0.3s ease;
        }

        .sidebar.collapsed .sidebar-toggle svg {
            transform: rotate(180deg);
        }

        .release-notes-inline {
            max-height: 0;
            overflow: hidden;
            transition: max-height 0.4s cubic-bezier(0.4, 0, 0.2, 1), margin 0.3s ease, opacity 0.3s ease;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 8px;
            margin-top: 0;
            opacity: 0;
        }

        .release-notes-inline.open {
            max-height: 2000px;
            margin-top: 12px;
            margin-bottom: 12px;
            opacity: 1;
            padding: 16px;
            border: 1px solid var(--border);
        }

        .release-notes-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 12px;
            border-bottom: 1px solid var(--border);
            padding-bottom: 8px;
        }

        .release-notes-title {
            font-size: 14px;
            font-weight: 600;
            color: var(--accent);
        }

        .release-notes-body {
            font-size: 13px;
            line-height: 1.6;
            color: var(--text-secondary);
            white-space: pre-wrap;
            word-break: break-word;
        }

        .release-notes-close {
            background: none;
            border: none;
            color: var(--text-secondary);
            font-size: 18px;
            cursor: pointer;
            padding: 4px 8px;
        }

        .release-notes-close:hover {
            color: var(--text-primary);
        }

        /* --- Dashboard Stats Grid --- */
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }

        .dashboard-card {
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 16px;
            display: flex;
            flex-direction: column;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }

        .dashboard-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px var(--shadow);
        }

        .dashboard-card-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 12px;
        }

        .dashboard-card-title {
            font-size: 12px;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .dashboard-card-icon {
            width: 20px;
            height: 20px;
            fill: var(--text-disabled);
        }

        .dashboard-value {
            font-size: 28px;
            font-weight: 700;
            color: var(--accent);
            line-height: 1;
        }

        .dashboard-unit {
            font-size: 14px;
            font-weight: 400;
            color: var(--text-secondary);
            margin-left: 4px;
        }

        .dashboard-subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .dashboard-sparkline {
            height: 32px;
            margin-top: 12px;
        }

        .dashboard-progress {
            height: 4px;
            background: var(--bg-card);
            border-radius: 2px;
            margin-top: 8px;
            overflow: hidden;
        }

        .dashboard-progress-bar {
            height: 100%;
            background: var(--accent);
            border-radius: 2px;
            transition: width 0.5s ease;
        }

        .dashboard-card.full-width {
            grid-column: span 2;
        }

        /* --- Enhanced Animations --- */
        @keyframes slideIn {
            from { opacity: 0; transform: translateX(-20px); }
            to { opacity: 1; transform: translateX(0); }
        }

        @keyframes slideUp {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        @keyframes scaleIn {
            from { opacity: 0; transform: scale(0.95); }
            to { opacity: 1; transform: scale(1); }
        }

        .animate-slide-in {
            animation: slideIn 0.3s ease forwards;
        }

        .animate-slide-up {
            animation: slideUp 0.3s ease forwards;
        }

        .animate-pulse {
            animation: pulse 2s ease-in-out infinite;
        }

        .animate-scale-in {
            animation: scaleIn 0.2s ease forwards;
        }

        .hidden {
            display: none !important;
        }

        /* Staggered animation for cards */
        .panel.active .card:nth-child(1) { animation-delay: 0ms; }
        .panel.active .card:nth-child(2) { animation-delay: 50ms; }
        .panel.active .card:nth-child(3) { animation-delay: 100ms; }
        .panel.active .card:nth-child(4) { animation-delay: 150ms; }
        .panel.active .card:nth-child(5) { animation-delay: 200ms; }
        .panel.active .card:nth-child(6) { animation-delay: 250ms; }

        .panel.active .card {
            animation: slideUp 0.3s ease forwards;
            opacity: 0;
        }

        /* Button hover effects for desktop */
        @media (hover: hover) {
            .btn:hover {
                transform: translateY(-1px);
                box-shadow: 0 4px 12px var(--shadow);
            }

            .btn-primary:hover {
                background: var(--accent-light);
            }

            .btn-secondary:hover {
                background: var(--bg-input);
            }

            .card:hover {
                box-shadow: 0 2px 8px var(--shadow);
            }
        }

        /* --- Responsive Breakpoints --- */

        /* Mobile (max 480px) - Stack button rows vertically */
        @media (max-width: 480px) {
            .btn-row {
                flex-direction: column;
                gap: 8px;
            }

            .btn-row .btn {
                width: 100%;
            }
        }

        /* Tablet (768px+) */
        @media (min-width: 768px) {
            :root {
                --content-padding: 24px;
            }

            .tab-content {
                max-width: 720px;
                padding: 24px;
            }

            .card {
                padding: 20px;
                margin-bottom: 16px;
            }

            .stats-grid {
                grid-template-columns: repeat(2, 1fr);
                gap: 16px;
            }

            .dashboard-grid {
                grid-template-columns: repeat(2, 1fr);
                gap: 16px;
            }

            .btn-row {
                gap: 12px;
            }

            .modal {
                max-width: 500px;
                padding: 32px;
            }

            .status-bar {
                gap: 24px;
            }

            .dsp-freq-canvas {
                height: 280px;
            }
        }

        /* Desktop (1024px+) - Enable sidebar */
        @media (min-width: 1024px) {
            .tab-bar {
                display: none;
            }

            .toast {
                bottom: calc(20px + var(--safe-bottom));
            }

            .sidebar {
                display: flex;
            }

            body {
                padding-left: 240px;
                padding-top: calc(var(--safe-top) + 36px);
                padding-bottom: var(--safe-bottom);
            }

            body.sidebar-collapsed {
                padding-left: 64px;
            }

            body.has-status-bar {
                padding-top: calc(var(--safe-top) + 36px);
            }

            .status-bar {
                top: 0;
                left: 240px;
                padding-top: var(--safe-top);
                height: calc(36px + var(--safe-top));
            }

            body.sidebar-collapsed .status-bar {
                left: 64px;
            }

            .tab-content {
                max-width: 900px;
                padding: 32px;
            }

            .card {
                padding: 24px;
            }

            .stats-grid {
                grid-template-columns: repeat(3, 1fr);
            }

            .dashboard-grid {
                grid-template-columns: repeat(3, 1fr);
            }

            .dashboard-card.full-width {
                grid-column: span 3;
            }

            .form-group {
                margin-bottom: 20px;
            }
        }

        /* Large Desktop (1280px+) */
        @media (min-width: 1280px) {
            .tab-content {
                max-width: 1100px;
            }

            .dashboard-grid {
                grid-template-columns: repeat(4, 1fr);
            }

            .dashboard-card.full-width {
                grid-column: span 4;
            }

            .stats-grid {
                grid-template-columns: repeat(4, 1fr);
            }
        }

        /* Reduce motion preference */
        @media (prefers-reduced-motion: reduce) {
            *, *::before, *::after {
                animation-duration: 0.01ms !important;
                animation-iteration-count: 1 !important;
                transition-duration: 0.01ms !important;
            }
        }

        /* ===== Password Warning Banner ===== */
        .warning-banner {
            background: linear-gradient(135deg, #FF6B00 0%, #FF9800 100%);
            color: white;
            padding: 16px 20px;
            display: flex;
            align-items: center;
            gap: 12px;
            border-radius: 8px;
            margin: 16px;
            box-shadow: 0 4px 12px rgba(255, 152, 0, 0.3);
            animation: slideDown 0.3s ease-out;
            position: relative;
            z-index: 100;
        }

        .warning-icon {
            font-size: 24px;
            flex-shrink: 0;
        }

        .warning-text {
            flex: 1;
            line-height: 1.5;
        }

        .warning-text strong {
            display: block;
            margin-bottom: 4px;
            font-size: 16px;
        }

        .warning-text a {
            color: white;
            text-decoration: underline;
            font-weight: 600;
            cursor: pointer;
        }

        .warning-text a:hover {
            opacity: 0.8;
        }

        .warning-dismiss {
            background: rgba(255, 255, 255, 0.2);
            border: 1px solid rgba(255, 255, 255, 0.3);
            color: white;
            padding: 8px 16px;
            border-radius: 4px;
            cursor: pointer;
            flex-shrink: 0;
            transition: background 0.2s;
        }

        .warning-dismiss:hover {
            background: rgba(255, 255, 255, 0.3);
        }

        @keyframes slideDown {
            from {
                opacity: 0;
                transform: translateY(-20px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        /* ===== Modal Styles ===== */
        .modal-overlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.5);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 2000;
            padding: 20px;
        }
        
        #apConfigModal {
            display: none;
        }

        .modal-content {
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 32px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 8px 32px var(--shadow);
        }

        .modal-content h2 {
            margin-bottom: 24px;
            font-size: 24px;
        }

        .form-group {
            margin-bottom: 20px;
        }

        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-size: 14px;
            font-weight: 500;
            color: var(--text-secondary);
        }

        .form-group input {
            width: 100%;
            padding: 12px 16px;
            border: 2px solid var(--border);
            border-radius: 8px;
            background: var(--bg-input);
            color: var(--text-primary);
            font-size: 16px;
            transition: all 0.2s;
        }

        .form-group input:focus {
            outline: none;
            border-color: var(--accent);
            background: var(--bg-surface);
        }

        .error-message {
            margin-top: 16px;
            padding: 12px 16px;
            background: var(--error);
            color: white;
            border-radius: 8px;
            font-size: 14px;
        }

        .modal-actions {
            display: flex;
            gap: 12px;
            margin-top: 24px;
        }

        .modal-actions button {
            flex: 1;
            padding: 12px 24px;
            border-radius: 8px;
            border: none;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }

        .modal-actions button.primary {
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-dark) 100%);
            color: white;
        }

        .modal-actions button.primary:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(255, 152, 0, 0.4);
        }

        .modal-actions button:not(.primary) {
            background: var(--bg-card);
            color: var(--text-primary);
        }

        .modal-actions button:not(.primary):hover {
            background: var(--border);
        }

        /* ===== Audio Tab Styles ===== */
        .audio-canvas-wrap {
            background: var(--bg-card);
            border-radius: 8px;
            padding: 8px;
            margin-top: 8px;
        }

        .audio-canvas-wrap canvas {
            width: 100%;
            display: block;
            border-radius: 4px;
        }

        .audio-canvas-wrap canvas.waveform-canvas {
            height: 160px;
        }

        .audio-canvas-wrap canvas.spectrum-canvas {
            height: 140px;
        }

        .vu-meter-row {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-bottom: 10px;
        }

        .vu-meter-label {
            font-size: 13px;
            font-weight: 600;
            color: var(--text-secondary);
            width: 16px;
            text-align: right;
        }

        .vu-meter-track {
            flex: 1;
            height: 18px;
            background: var(--bg-input);
            border-radius: 4px;
            position: relative;
            overflow: hidden;
        }

        .vu-meter-fill {
            height: 100%;
            border-radius: 4px;
            background: linear-gradient(to right, #4CAF50 0%, #4CAF50 50%, #FFC107 70%, #FF9800 85%, #F44336 100%);
            width: 0%;
        }

        .vu-meter-peak {
            position: absolute;
            top: 0;
            width: 2px;
            height: 100%;
            background: #FFFFFF;
            left: 0%;
        }

        .vu-meter-db {
            font-size: 12px;
            font-weight: 500;
            color: var(--text-secondary);
            width: 72px;
            text-align: right;
            font-variant-numeric: tabular-nums;
        }

        .vu-scale {
            flex: 1;
            position: relative;
            height: 14px;
            margin-top: 2px;
        }
        .vu-tick {
            position: absolute;
            font-size: 9px;
            color: var(--text-secondary);
            opacity: 0.6;
            transform: translateX(-50%);
        }
        .vu-tick::before {
            content: '';
            position: absolute;
            top: -3px;
            left: 50%;
            width: 1px;
            height: 3px;
            background: var(--text-secondary);
            opacity: 0.4;
        }

        .ppm-canvas {
            flex: 1;
            height: 22px;
            border-radius: 4px;
        }

        .signal-dot {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #666;
            vertical-align: middle;
            margin-right: 6px;
        }

        .signal-dot.active {
            background: #4CAF50;
            box-shadow: 0 0 6px #4CAF50;
        }

        .dominant-freq-readout {
            font-size: 13px;
            color: var(--text-secondary);
            text-align: center;
            margin-top: 6px;
        }

        .dominant-freq-readout span {
            color: var(--accent);
            font-weight: 600;
        }

        /* Dual ADC responsive grid */
        .dual-canvas-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        @media (max-width: 768px) {
            .dual-canvas-grid {
                grid-template-columns: 1fr;
            }
        }
        .dual-canvas-grid .canvas-panel {
            min-width: 0;
        }
        .canvas-panel-title {
            font-size: 12px;
            font-weight: 600;
            color: var(--text-secondary);
            margin-bottom: 4px;
        }

        /* ADC section headers in VU meters */
        .adc-section-header {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-top: 12px;
            margin-bottom: 6px;
        }
        .adc-section-header:first-child {
            margin-top: 0;
        }
        .adc-section-title {
            font-size: 13px;
            font-weight: 600;
            color: var(--text-primary);
        }
        .adc-status-badge {
            font-size: 10px;
            font-weight: 600;
            padding: 2px 8px;
            border-radius: 10px;
            text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .adc-status-badge.ok {
            background: rgba(76,175,80,0.15);
            color: #4CAF50;
        }
        .adc-status-badge.no-data {
            background: rgba(158,158,158,0.15);
            color: #9E9E9E;
        }
        .adc-status-badge.clipping {
            background: rgba(244,67,54,0.15);
            color: #F44336;
        }
        .adc-status-badge.noise-only {
            background: rgba(255,193,7,0.15);
            color: #FFC107;
        }
        .adc-status-badge.i2s-error {
            background: rgba(244,67,54,0.15);
            color: #F44336;
        }
        .adc-status-badge.hw-fault {
            background: rgba(156,39,176,0.15);
            color: #9C27B0;
        }
        .clip-indicator {
            display: none;
            font-size: 10px;
            font-weight: 700;
            padding: 2px 8px;
            border-radius: 10px;
            background: #F44336;
            color: #FFF;
            letter-spacing: 0.5px;
        }
        .clip-indicator.active {
            display: inline-block;
            animation: pulse 1s cubic-bezier(0.4, 0, 0.6, 1) infinite;
        }
        .adc-readout {
            font-size: 11px;
            color: var(--text-secondary);
            margin-left: auto;
            font-variant-numeric: tabular-nums;
        }

        /* VU meter channel name labels */
        .vu-meter-label.ch-name {
            width: auto;
            min-width: 24px;
            max-width: 90px;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            font-size: 11px;
        }

        /* ===== DSP Tab Styles ===== */
        .dsp-ch-tabs {
            display: flex;
            gap: 4px;
            margin-bottom: 12px;
            flex-wrap: wrap;
        }
        .dsp-ch-tab {
            padding: 7px 16px;
            border-radius: 6px;
            border: 1px solid var(--border);
            background: var(--bg-card);
            color: var(--text-secondary);
            font-size: 13px;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.18s ease;
            letter-spacing: 0.5px;
            text-transform: uppercase;
            position: relative;
        }
        .dsp-ch-tab:hover { background: var(--bg-input); }
        .dsp-ch-tab.active {
            background: linear-gradient(135deg, var(--accent), var(--accent-dark, #E68900));
            color: #fff;
            border-color: var(--accent);
            box-shadow: 0 2px 8px rgba(255,152,0,0.3);
        }
        .dsp-ch-tab .badge {
            display: inline-block;
            background: var(--bg-surface);
            color: var(--text-secondary);
            font-size: 9px;
            padding: 1px 5px;
            border-radius: 4px;
            margin-left: 4px;
            font-weight: 600;
        }
        .dsp-ch-tab.active .badge {
            background: rgba(255,255,255,0.2);
            color: #fff;
        }
        .dsp-stage-card {
            background: var(--bg-card);
            border-radius: 10px;
            margin-bottom: 8px;
            overflow: hidden;
            border: 1px solid var(--border);
        }
        .dsp-stage-card.disabled {
            opacity: 0.5;
        }
        .dsp-stage-header {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 10px 12px;
            cursor: pointer;
        }
        .dsp-stage-type {
            font-size: 11px;
            font-weight: 700;
            padding: 2px 8px;
            border-radius: 4px;
            color: #fff;
            letter-spacing: 0.3px;
            flex-shrink: 0;
        }
        .dsp-stage-name {
            flex: 1;
            font-size: 13px;
            font-weight: 500;
            color: var(--text-primary);
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .dsp-stage-info {
            font-size: 11px;
            color: var(--text-secondary);
            flex-shrink: 0;
        }
        .dsp-stage-actions {
            display: flex;
            gap: 4px;
            flex-shrink: 0;
        }
        .dsp-stage-actions button {
            width: 28px;
            height: 28px;
            border: none;
            border-radius: 6px;
            background: var(--bg-surface);
            color: var(--text-secondary);
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 14px;
            transition: background 0.15s;
        }
        .dsp-stage-actions button:hover {
            background: var(--border);
        }
        .dsp-stage-actions button.del:hover {
            background: var(--error);
            color: #fff;
        }
        .dsp-stage-body {
            display: none;
            padding: 0 12px 12px;
        }
        .dsp-stage-body.open {
            display: block;
        }
        .dsp-param {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-top: 8px;
        }
        .dsp-param label {
            font-size: 12px;
            color: var(--text-secondary);
            width: 70px;
            flex-shrink: 0;
        }
        .dsp-param input[type="range"] {
            flex: 1;
            accent-color: var(--accent);
        }
        .dsp-param .dsp-val {
            font-size: 12px;
            font-weight: 600;
            color: var(--text-primary);
            min-width: 55px;
            text-align: right;
            font-variant-numeric: tabular-nums;
        }
        .dsp-step-btn {
            width: 24px;
            height: 24px;
            padding: 0;
            border: 1px solid var(--border);
            border-radius: 4px;
            background: var(--bg-input);
            color: var(--text-primary);
            font-size: 14px;
            line-height: 22px;
            text-align: center;
            cursor: pointer;
            flex-shrink: 0;
            user-select: none;
        }
        .dsp-step-btn:hover { background: var(--accent); color: #fff; }
        .dsp-step-btn:active { transform: scale(0.92); }
        .dsp-num-input {
            width: 58px;
            padding: 2px 4px;
            border: 1px solid var(--border);
            border-radius: 4px;
            background: var(--bg-input);
            color: var(--text-primary);
            font-size: 12px;
            font-weight: 600;
            text-align: right;
            font-variant-numeric: tabular-nums;
            flex-shrink: 0;
        }
        .dsp-num-input:focus { outline: 1px solid var(--accent); border-color: var(--accent); }
        .dsp-unit {
            font-size: 11px;
            color: var(--text-secondary);
            min-width: 28px;
            flex-shrink: 0;
        }
        .comp-graph-wrap {
            margin-bottom: 8px;
            border-radius: 6px;
            overflow: hidden;
            background: rgba(0,0,0,0.25);
        }
        .comp-graph-wrap canvas {
            display: block;
            width: 100%;
        }
        .comp-gr-wrap {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-top: 10px;
            font-size: 12px;
        }
        .comp-gr-wrap label {
            color: var(--text-secondary);
            width: 70px;
            flex-shrink: 0;
        }
        .comp-gr-track {
            flex: 1;
            height: 10px;
            background: var(--bg-input);
            border-radius: 5px;
            overflow: hidden;
        }
        .comp-gr-fill {
            height: 100%;
            background: linear-gradient(90deg, var(--error), #ff7043);
            border-radius: 5px;
            transition: width 0.15s;
        }
        .comp-gr-val {
            font-size: 12px;
            font-weight: 600;
            color: var(--error);
            min-width: 60px;
            text-align: right;
            font-variant-numeric: tabular-nums;
        }
        .dsp-add-btn {
            display: block;
            width: 100%;
            padding: 10px;
            border: 2px dashed var(--border);
            border-radius: 10px;
            background: transparent;
            color: var(--accent);
            font-size: 13px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 8px;
            transition: border-color 0.15s, background 0.15s;
        }
        .dsp-add-btn:hover {
            border-color: var(--accent);
            background: rgba(255, 152, 0, 0.05);
        }
        .dsp-add-menu {
            display: none;
            background: var(--bg-surface);
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 8px 0;
            margin-top: 4px;
            box-shadow: 0 4px 16px var(--shadow);
        }
        .dsp-add-menu.open { display: block; }
        .dsp-add-menu .menu-cat {
            font-size: 10px;
            font-weight: 700;
            text-transform: uppercase;
            color: var(--text-disabled);
            padding: 8px 16px 4px;
            letter-spacing: 0.5px;
        }
        .dsp-add-menu .menu-item {
            padding: 8px 16px;
            font-size: 13px;
            cursor: pointer;
            color: var(--text-primary);
            transition: background 0.1s;
        }
        .dsp-add-menu .menu-item:hover {
            background: var(--bg-card);
        }
        .dsp-cpu-bar {
            height: 6px;
            background: var(--bg-card);
            border-radius: 3px;
            overflow: hidden;
            margin-top: 8px;
        }
        .dsp-cpu-bar .fill {
            height: 100%;
            background: var(--accent);
            border-radius: 3px;
            transition: width 0.3s;
        }
        .dsp-freq-canvas {
            display: block;
            width: 100%;
            height: 220px;
            border-radius: 8px;
            background: var(--bg-card);
        }
        @media (max-width: 767px) {
            .dsp-freq-canvas { height: 180px; }
        }
        /* ===== PEQ Band Styles ===== */
        .peq-band-strip {
            display: flex;
            gap: 4px;
            margin-bottom: 10px;
            flex-wrap: wrap;
        }
        .peq-band-pill {
            width: 32px;
            height: 28px;
            border-radius: 6px;
            border: 1.5px solid var(--border);
            background: var(--bg-surface);
            color: var(--text-secondary);
            font-size: 12px;
            font-weight: 700;
            cursor: pointer;
            border-bottom: 3px solid var(--band-color, var(--border));
            transition: all 0.15s;
            padding: 0;
        }
        .peq-band-pill.active {
            border-color: var(--band-color, var(--accent));
            background: var(--bg-card);
            color: var(--text-primary);
            box-shadow: 0 0 0 2px rgba(255,255,255,0.15);
        }
        .peq-band-pill.enabled {
            background: color-mix(in srgb, var(--band-color, var(--accent)) 20%, var(--bg-surface));
            color: var(--text-primary);
        }
        .peq-detail-panel {
            padding: 8px 12px;
            background: var(--bg-card);
            border-radius: 8px;
        }
        .peq-graph-tog {
            opacity: 0.5;
            transition: opacity 0.15s;
        }
        .peq-graph-tog.active {
            opacity: 1;
            background: var(--accent) !important;
            color: #fff !important;
        }
    </style>
</head>
<body class="has-status-bar">
    <script>if(localStorage.getItem('darkMode')==='true'){document.body.classList.add('night-mode');document.querySelector('meta[name="theme-color"]').setAttribute('content','#121212');}</script>
    <!-- Sidebar Navigation (Desktop) -->
    <aside class="sidebar" id="sidebar">
        <div class="sidebar-header">
            <div class="sidebar-logo">
                <svg viewBox="0 0 24 24"><path d="M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"/></svg>
                <span>ALX Audio</span>
            </div>
        </div>
        <nav class="sidebar-nav">
            <button class="sidebar-item active" data-tab="control" onclick="switchTab('control')">
                <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
                <span>Control</span>
            </button>
            <button class="sidebar-item" data-tab="audio" onclick="switchTab('audio')">
                <svg viewBox="0 0 24 24"><path d="M12 3v9.28c-.47-.17-.97-.28-1.5-.28C8.01 12 6 14.01 6 16.5S8.01 21 10.5 21c2.31 0 4.2-1.75 4.45-4H15V6h4V3h-7z"/></svg>
                <span>Audio</span>
            </button>
            <button class="sidebar-item" data-tab="dsp" onclick="switchTab('dsp')">
                <svg viewBox="0 0 24 24"><path d="M7 18h2V6H7v12zm4-12v12h2V6h-2zm-8 8h2v-4H3v4zm12-6v8h2V8h-2zm4 2v4h2v-4h-2z"/></svg>
                <span>DSP</span>
            </button>
            <button class="sidebar-item" data-tab="wifi" onclick="switchTab('wifi')">
                <svg viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
                <span>WiFi</span>
            </button>
            <button class="sidebar-item" data-tab="mqtt" onclick="switchTab('mqtt')">
                <svg viewBox="0 0 24 24"><path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM19 18H6c-2.21 0-4-1.79-4-4s1.79-4 4-4h.71C7.37 7.69 9.48 6 12 6c3.04 0 5.5 2.46 5.5 5.5v.5H19c1.66 0 3 1.34 3 3s-1.34 3-3 3z"/></svg>
                <span>MQTT</span>
            </button>
            <button class="sidebar-item" data-tab="settings" onclick="switchTab('settings')">
                <svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
                <span>Settings</span>
            </button>
            <button class="sidebar-item" data-tab="support" onclick="switchTab('support')">
                <svg viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>
                <span>Support</span>
            </button>
            <button class="sidebar-item" data-tab="debug" onclick="switchTab('debug')">
                <svg viewBox="0 0 24 24"><path d="M20 8h-2.81c-.45-.78-1.07-1.45-1.82-1.96L17 4.41 15.59 3l-2.17 2.17C12.96 5.06 12.49 5 12 5c-.49 0-.96.06-1.41.17L8.41 3 7 4.41l1.62 1.63C7.88 6.55 7.26 7.22 6.81 8H4v2h2.09c-.05.33-.09.66-.09 1v1H4v2h2v1c0 .34.04.67.09 1H4v2h2.81c1.04 1.79 2.97 3 5.19 3s4.15-1.21 5.19-3H20v-2h-2.09c.05-.33.09-.66.09-1v-1h2v-2h-2v-1c0-.34-.04-.67-.09-1H20V8zm-6 8h-4v-2h4v2zm0-4h-4v-2h4v2z"/></svg>
                <span>Debug</span>
            </button>
            <button class="sidebar-item" data-tab="test" onclick="switchTab('test')">
                <svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-7 14l-5-5 1.41-1.41L12 14.17l4.59-4.58L18 11l-6 6z"/></svg>
                <span>Test</span>
            </button>
        </nav>
        <div class="sidebar-footer">
            <span id="sidebarVersion">v--</span>
        </div>
        <button class="sidebar-toggle" onclick="toggleSidebar()">
            <svg viewBox="0 0 24 24"><path d="M15.41 16.59L10.83 12l4.58-4.59L14 6l-6 6 6 6 1.41-1.41z"/></svg>
        </button>
    </aside>

    <!-- Persistent Status Bar -->
    <div class="status-bar" id="statusBar">
        <div class="status-item" title="Amplifier State">
            <div class="status-indicator" id="statusAmp"></div>
            <span id="statusAmpText">Amp</span>
        </div>
        <div class="status-item" title="WiFi Connection">
            <div class="status-indicator" id="statusWifi"></div>
            <span id="statusWifiText">WiFi</span>
        </div>
        <div class="status-item" title="MQTT Connection">
            <div class="status-indicator" id="statusMqtt"></div>
            <span id="statusMqttText">MQTT</span>
        </div>
        <div class="status-item" title="WebSocket">
            <div class="status-indicator" id="statusWs"></div>
            <span>WS</span>
        </div>
    </div>

    <!-- Tab Navigation Bar (Mobile) -->
    <nav class="tab-bar">
        <button class="tab active" data-tab="control" onclick="switchTab('control')">
            <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
        </button>
        <button class="tab" data-tab="audio" onclick="switchTab('audio')">
            <svg viewBox="0 0 24 24"><path d="M12 3v9.28c-.47-.17-.97-.28-1.5-.28C8.01 12 6 14.01 6 16.5S8.01 21 10.5 21c2.31 0 4.2-1.75 4.45-4H15V6h4V3h-7z"/></svg>
        </button>
        <button class="tab" data-tab="dsp" onclick="switchTab('dsp')">
            <svg viewBox="0 0 24 24"><path d="M7 18h2V6H7v12zm4-12v12h2V6h-2zm-8 8h2v-4H3v4zm12-6v8h2V8h-2zm4 2v4h2v-4h-2z"/></svg>
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
        <button class="tab" data-tab="support" onclick="switchTab('support')">
            <svg viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>
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
            <!-- Control Status -->
            <div class="card">
                <div class="card-title">Control Status</div>
                <div class="info-box" id="controlStatusBox">
                    <div class="info-row">
                        <span class="info-label">WebSocket</span>
                        <span class="info-value" id="wsConnectionStatus">Connecting...</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Signal Detected</span>
                        <span class="info-value" id="signalDetected">No</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Audio Level</span>
                        <span class="info-value" id="audioLevel">-96.0 dBFS</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Input Voltage</span>
                        <span class="info-value" id="audioVrms">0.000 V</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Sensing Mode</span>
                        <span class="info-value" id="infoSensingMode">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Auto-Off Timer</span>
                        <span class="info-value" id="infoTimerDuration">-- min</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Audio Threshold (dBFS)</span>
                        <span class="info-value" id="infoAudioThreshold">-- dBFS</span>
                    </div>
                </div>
            </div>

            <!-- Amplifier Status -->
            <div class="amplifier-display" id="amplifierDisplay">
                <svg class="amplifier-icon" viewBox="0 0 24 24"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></svg>
                <div class="amplifier-label" id="amplifierStatus">OFF</div>
            </div>

            <!-- Timer Display -->
            <div class="timer-display hidden" id="timerDisplay">
                <div class="timer-value" id="timerValue">--:--</div>
                <div class="timer-label">Time Remaining</div>
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

            <!-- Smart Auto Settings (collapsible, only shown when Smart Auto is selected) -->
            <div class="card" id="smartAutoSettingsCard" style="display: none;">
                <div class="collapsible-header" onclick="toggleSmartAutoSettings()">
                    <span class="card-title" style="margin-bottom: 0;">Smart Auto Settings</span>
                    <svg viewBox="0 0 24 24" id="smartAutoChevron"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z"/></svg>
                </div>
                <div class="collapsible-content" id="smartAutoContent">
                    <div class="form-group" style="margin-top: 12px;">
                        <label class="form-label">Auto-Off Timer (minutes)</label>
                        <input type="number" class="form-input" id="appState.timerDuration" inputmode="numeric" min="1" max="60" value="15" onchange="updateTimerDuration()">
                    </div>
                    <div class="form-group">
                        <label class="form-label">Audio Threshold (dBFS)</label>
                        <input type="number" class="form-input" id="audioThreshold" inputmode="decimal" min="-96" max="0" step="1" value="-60" onchange="updateAudioThreshold()">
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

        <!-- ===== AUDIO TAB ===== -->
        <section id="audio" class="panel">
            <!-- Audio Waveform -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    Audio Waveform
                    <div style="display:flex;align-items:center;gap:8px;">
                        <span style="font-size:11px;color:var(--text-secondary);">Enable</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="waveformEnabledToggle" checked onchange="setGraphEnabled('waveform',this.checked)">
                            <span class="slider round"></span>
                        </label>
                        <span style="font-size:11px;color:var(--text-secondary);">Auto-scale</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="waveformAutoScale" onchange="waveformAutoScaleEnabled=this.checked">
                            <span class="slider round"></span>
                        </label>
                    </div>
                </div>
                <div id="waveformContent">
                    <div class="dual-canvas-grid">
                        <div class="canvas-panel">
                            <div class="canvas-panel-title">ADC 1</div>
                            <div class="audio-canvas-wrap">
                                <canvas class="waveform-canvas" id="audioWaveformCanvas0"></canvas>
                            </div>
                        </div>
                        <div class="canvas-panel" id="waveformPanel1">
                            <div class="canvas-panel-title">ADC 2</div>
                            <div class="audio-canvas-wrap">
                                <canvas class="waveform-canvas" id="audioWaveformCanvas1"></canvas>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Frequency Spectrum -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    Frequency Spectrum
                    <div style="display:flex;align-items:center;gap:6px;">
                        <span style="font-size:11px;color:var(--text-secondary);">Enable</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="spectrumEnabledToggle" checked onchange="setGraphEnabled('spectrum',this.checked)">
                            <span class="slider round"></span>
                        </label>
                        <span style="font-size:11px;color:var(--text-secondary);">LED</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="ledModeToggle" onchange="toggleLedMode()">
                            <span class="slider round"></span>
                        </label>
                    </div>
                </div>
                <div id="spectrumContent">
                    <div class="dual-canvas-grid">
                        <div class="canvas-panel">
                            <div class="canvas-panel-title">ADC 1</div>
                            <div class="audio-canvas-wrap">
                                <canvas class="spectrum-canvas" id="audioSpectrumCanvas0"></canvas>
                            </div>
                            <div class="dominant-freq-readout">Dominant: <span id="dominantFreq0">-- Hz</span></div>
                        </div>
                        <div class="canvas-panel" id="spectrumPanel1">
                            <div class="canvas-panel-title">ADC 2</div>
                            <div class="audio-canvas-wrap">
                                <canvas class="spectrum-canvas" id="audioSpectrumCanvas1"></canvas>
                            </div>
                            <div class="dominant-freq-readout">Dominant: <span id="dominantFreq1">-- Hz</span></div>
                        </div>
                    </div>
                    <div style="display:flex;align-items:center;gap:8px;margin-top:6px;flex-wrap:wrap;">
                        <span style="font-size:12px;color:var(--text-secondary);">Window:</span>
                        <select id="fftWindowSelect" class="select-sm" onchange="setFftWindow(this.value)">
                            <option value="0">Hann</option>
                            <option value="1">Blackman</option>
                            <option value="2">Blackman-Harris</option>
                            <option value="3">Blackman-Nuttall</option>
                            <option value="4">Nuttall</option>
                            <option value="5">Flat-Top</option>
                        </select>
                        <span style="font-size:11px;color:var(--text-secondary);margin-left:8px;">SNR: <span id="audioSnr0">--</span> dB</span>
                        <span style="font-size:11px;color:var(--text-secondary);">SFDR: <span id="audioSfdr0">--</span> dB</span>
                    </div>
                </div>
            </div>

            <!-- Audio Levels -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    Audio Levels
                    <div style="display:flex;align-items:center;gap:6px;">
                        <span style="font-size:11px;color:var(--text-secondary);">Enable</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="vuMeterEnabledToggle" checked onchange="setGraphEnabled('vuMeter',this.checked)">
                            <span class="slider round"></span>
                        </label>
                        <span style="font-size:11px;color:var(--text-secondary);">Segmented</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="vuSegmented" onchange="toggleVuMode(this.checked)">
                            <span class="slider round"></span>
                        </label>
                    </div>
                </div>
                <div id="vuMeterContent">
                <!-- ADC 0 (ADC 1) -->
                <div class="adc-section-header">
                    <span class="adc-section-title">ADC 1</span>
                    <span class="adc-status-badge ok" id="adcStatusBadge0">OK</span>
                    <span class="clip-indicator" id="clipIndicator0">CLIP</span>
                    <span class="adc-readout" id="adcReadout0">-- dBFS | -- Vrms</span>
                </div>
                <!-- Continuous bars ADC 0 -->
                <div id="vuContinuous0">
                    <div class="vu-meter-row">
                        <span class="vu-meter-label ch-name" id="vuChName0">Ch 1</span>
                        <div class="vu-meter-track">
                            <div class="vu-meter-fill" id="vuFill0L"></div>
                            <div class="vu-meter-peak" id="vuPeak0L"></div>
                        </div>
                        <span class="vu-meter-db" id="vuDb0L">-inf dBFS</span>
                    </div>
                    <div class="vu-meter-row" style="margin-bottom:0">
                        <span class="vu-meter-label ch-name" id="vuChName1">Ch 2</span>
                        <div class="vu-meter-track">
                            <div class="vu-meter-fill" id="vuFill0R"></div>
                            <div class="vu-meter-peak" id="vuPeak0R"></div>
                        </div>
                        <span class="vu-meter-db" id="vuDb0R">-inf dBFS</span>
                    </div>
                </div>
                <!-- Segmented PPM ADC 0 -->
                <div id="vuSegmented0" style="display:none">
                    <div class="vu-meter-row">
                        <span class="vu-meter-label ch-name" id="vuChNameSeg0">Ch 1</span>
                        <canvas id="ppmCanvas0L" class="ppm-canvas"></canvas>
                        <span class="vu-meter-db" id="vuDbSeg0L">-inf dBFS</span>
                    </div>
                    <div class="vu-meter-row" style="margin-bottom:0">
                        <span class="vu-meter-label ch-name" id="vuChNameSeg1">Ch 2</span>
                        <canvas id="ppmCanvas0R" class="ppm-canvas"></canvas>
                        <span class="vu-meter-db" id="vuDbSeg0R">-inf dBFS</span>
                    </div>
                </div>

                <!-- ADC 1 (ADC 2) -->
                <div id="adcSection1">
                <div class="adc-section-header">
                    <span class="adc-section-title">ADC 2</span>
                    <span class="adc-status-badge no-data" id="adcStatusBadge1">NO_DATA</span>
                    <span class="clip-indicator" id="clipIndicator1">CLIP</span>
                    <span class="adc-readout" id="adcReadout1">-- dBFS | -- Vrms</span>
                </div>
                <!-- Continuous bars ADC 1 -->
                <div id="vuContinuous1">
                    <div class="vu-meter-row">
                        <span class="vu-meter-label ch-name" id="vuChName2">Ch 3</span>
                        <div class="vu-meter-track">
                            <div class="vu-meter-fill" id="vuFill1L"></div>
                            <div class="vu-meter-peak" id="vuPeak1L"></div>
                        </div>
                        <span class="vu-meter-db" id="vuDb1L">-inf dBFS</span>
                    </div>
                    <div class="vu-meter-row" style="margin-bottom:0">
                        <span class="vu-meter-label ch-name" id="vuChName3">Ch 4</span>
                        <div class="vu-meter-track">
                            <div class="vu-meter-fill" id="vuFill1R"></div>
                            <div class="vu-meter-peak" id="vuPeak1R"></div>
                        </div>
                        <span class="vu-meter-db" id="vuDb1R">-inf dBFS</span>
                    </div>
                </div>
                <!-- Segmented PPM ADC 1 -->
                <div id="vuSegmented1" style="display:none">
                    <div class="vu-meter-row">
                        <span class="vu-meter-label ch-name" id="vuChNameSeg2">Ch 3</span>
                        <canvas id="ppmCanvas1L" class="ppm-canvas"></canvas>
                        <span class="vu-meter-db" id="vuDbSeg1L">-inf dBFS</span>
                    </div>
                    <div class="vu-meter-row" style="margin-bottom:0">
                        <span class="vu-meter-label ch-name" id="vuChNameSeg3">Ch 4</span>
                        <canvas id="ppmCanvas1R" class="ppm-canvas"></canvas>
                        <span class="vu-meter-db" id="vuDbSeg1R">-inf dBFS</span>
                    </div>
                </div>
                </div>

                <!-- Shared scale row -->
                <div class="vu-meter-row" style="margin-bottom:8px;margin-top:10px">
                    <span class="vu-meter-label"></span>
                    <div class="vu-scale">
                        <span class="vu-tick" style="left:0%">-60</span>
                        <span class="vu-tick" style="left:16.7%">-50</span>
                        <span class="vu-tick" style="left:33.3%">-40</span>
                        <span class="vu-tick" style="left:50%">-30</span>
                        <span class="vu-tick" style="left:66.7%">-20</span>
                        <span class="vu-tick" style="left:83.3%">-10</span>
                        <span class="vu-tick" style="left:100%">0</span>
                    </div>
                    <span class="vu-meter-db"></span>
                </div>
                <div class="info-row" style="border-bottom: none; padding: 4px 0;">
                    <span class="info-label"><span class="signal-dot" id="audioSignalDot"></span>Signal</span>
                    <span class="info-value" id="audioSignalText">Not detected</span>
                </div>
                </div>
            </div>

            <!-- Input Names -->
            <div class="card">
                <div class="card-title">Input Names</div>
                <div class="form-group">
                    <label class="form-label">ADC 1 - Input 1</label>
                    <input type="text" class="form-input" id="inputName0" placeholder="Subwoofer 1" maxlength="20">
                </div>
                <div class="form-group">
                    <label class="form-label">ADC 1 - Input 2</label>
                    <input type="text" class="form-input" id="inputName1" placeholder="Subwoofer 2" maxlength="20">
                </div>
                <div id="inputNamesAdc1">
                <div class="form-group">
                    <label class="form-label">ADC 2 - Input 3</label>
                    <input type="text" class="form-input" id="inputName2" placeholder="Subwoofer 3" maxlength="20">
                </div>
                <div class="form-group">
                    <label class="form-label">ADC 2 - Input 4</label>
                    <input type="text" class="form-input" id="inputName3" placeholder="Subwoofer 4" maxlength="20">
                </div>
                </div>
                <button class="btn btn-primary" onclick="saveInputNames()">Save Names</button>
            </div>

            <!-- Audio Settings -->
            <div class="card">
                <div class="card-title">Audio Settings</div>
                <div class="form-group" style="display:flex;align-items:center;justify-content:space-between">
                    <label class="form-label" style="margin:0">ADC Input 1</label>
                    <label class="switch"><input type="checkbox" id="adcEnable0" checked onchange="setAdcEnabled(0,this.checked)"><span class="slider round"></span></label>
                </div>
                <div class="form-group" style="display:flex;align-items:center;justify-content:space-between">
                    <label class="form-label" style="margin:0">ADC Input 2</label>
                    <label class="switch"><input type="checkbox" id="adcEnable1" checked onchange="setAdcEnabled(1,this.checked)"><span class="slider round"></span></label>
                </div>
                <div class="form-group">
                    <label class="form-label">Update Rate</label>
                    <select class="form-input" id="audioUpdateRateSelect" onchange="setAudioUpdateRate()">
                        <option value="100">100 ms</option>
                        <option value="50" selected>50 ms</option>
                        <option value="33">33 ms</option>
                        <option value="20">20 ms</option>
                    </select>
                </div>
                <div class="form-group">
                    <label class="form-label">Sample Rate</label>
                    <select class="form-input" id="audioSampleRateSelect">
                        <option value="16000">16000 Hz</option>
                        <option value="44100">44100 Hz</option>
                        <option value="48000" selected>48000 Hz</option>
                    </select>
                </div>
                <button class="btn btn-primary" onclick="updateAudioSettings()">Update Sample Rate</button>
            </div>

            <!-- USB Audio Input -->
            <div class="card" id="usbAudioCard">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    USB Audio Input
                    <span id="usbAudioBadge" class="badge" style="font-size:10px;padding:2px 6px;background:#9E9E9E;color:#fff">Disabled</span>
                </div>
                <div class="form-group">
                    <label class="form-label">Enable</label>
                    <label class="switch" style="float:right"><input type="checkbox" id="usbAudioEnable" onchange="setUsbAudioEnabled(this.checked)"><span class="slider round"></span></label>
                    <div style="clear:both"></div>
                </div>
                <div id="usbAudioFields" style="display:none">
                <div class="info-row"><span class="info-label">Status</span><span class="info-value" id="usbAudioStatus">Disconnected</span></div>
                <div class="info-row"><span class="info-label">Format</span><span class="info-value" id="usbAudioFormat">—</span></div>
                <div class="info-row"><span class="info-label">Host Volume</span><span class="info-value" id="usbAudioVolume">—</span></div>
                <div id="usbAudioDetails" style="display:none">
                    <div class="info-row"><span class="info-label">Buffer Overruns</span><span class="info-value" id="usbAudioOverruns">0</span></div>
                    <div class="info-row"><span class="info-label">Buffer Underruns</span><span class="info-value" id="usbAudioUnderruns">0</span></div>
                </div>
                </div>
            </div>

            <!-- DAC Output -->
            <div class="card" id="dacCard">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    DAC Output
                    <span id="dacReadyBadge" class="badge" style="font-size:10px;padding:2px 6px;display:none">Ready</span>
                </div>
                <div class="form-group">
                    <label class="form-label">Enable</label>
                    <label class="switch" style="float:right"><input type="checkbox" id="dacEnable" onchange="updateDac()"><span class="slider round"></span></label>
                    <div style="clear:both"></div>
                </div>
                <div id="dacFields" style="display:none">
                <div class="info-row"><span class="info-label">Model</span><span class="info-value" id="dacModel">—</span></div>
                <div class="form-group">
                    <label class="form-label">Volume: <span id="dacVolVal">80</span>%</label>
                    <input type="range" class="form-input" id="dacVolume" min="0" max="100" value="80" step="1" oninput="document.getElementById('dacVolVal').textContent=this.value" onchange="updateDac()">
                </div>
                <div class="form-group">
                    <label class="form-label">Mute</label>
                    <label class="switch" style="float:right"><input type="checkbox" id="dacMute" onchange="updateDac()"><span class="slider round"></span></label>
                    <div style="clear:both"></div>
                </div>
                <div class="form-group" id="dacFilterGroup" style="display:none">
                    <label class="form-label">Filter Mode</label>
                    <select class="form-input" id="dacFilterMode" onchange="updateDacFilter()"></select>
                </div>
                <div class="info-row"><span class="info-label">TX Underruns</span><span class="info-value" id="dacUnderruns">0</span></div>
                <div class="form-group">
                    <label class="form-label">DAC Driver</label>
                    <select class="form-input" id="dacDriverSelect" onchange="changeDacDriver()"></select>
                </div>
                </div>
            </div>

            <!-- EEPROM Programming -->
            <div class="card" id="eepromCard">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    EEPROM Programming
                    <span id="eepromFoundBadge" class="badge" style="font-size:10px;padding:2px 6px;display:none">Found</span>
                </div>
                <div class="info-row"><span class="info-label">Status</span><span class="info-value" id="eepromStatus">Not scanned</span></div>
                <div class="info-row"><span class="info-label">I2C Address</span><span class="info-value" id="eepromI2cAddr">—</span></div>
                <div class="info-row"><span class="info-label">I2C Devices</span><span class="info-value" id="eepromI2cCount">—</span></div>
                <div style="margin:8px 0">
                    <button class="btn btn-secondary" onclick="eepromScan()">Scan I2C Bus</button>
                </div>
                <div class="form-group">
                    <label class="form-label">Driver Preset</label>
                    <select class="form-input" id="eepromPreset" onchange="eepromFillPreset()">
                        <option value="">— Select preset —</option>
                    </select>
                </div>
                <div class="form-group">
                    <label class="form-label">Device ID (hex)</label>
                    <input type="text" class="form-input" id="eepromDeviceId" placeholder="0x0001">
                </div>
                <div class="form-group">
                    <label class="form-label">Device Name</label>
                    <input type="text" class="form-input" id="eepromDeviceName" maxlength="32" placeholder="PCM5102A">
                </div>
                <div class="form-group">
                    <label class="form-label">Manufacturer</label>
                    <input type="text" class="form-input" id="eepromManufacturer" maxlength="32" placeholder="Texas Instruments">
                </div>
                <div class="form-group">
                    <label class="form-label">HW Revision</label>
                    <input type="number" class="form-input" id="eepromHwRev" min="0" max="255" value="1">
                </div>
                <div class="form-group">
                    <label class="form-label">Max Channels</label>
                    <input type="number" class="form-input" id="eepromMaxCh" min="1" max="8" value="2">
                </div>
                <div class="form-group">
                    <label class="form-label">DAC I2C Address (hex, 0=none)</label>
                    <input type="text" class="form-input" id="eepromDacAddr" placeholder="0x00">
                </div>
                <div class="form-group">
                    <label class="form-label">Flags</label>
                    <div style="display:flex;flex-wrap:wrap;gap:12px;margin-top:4px">
                        <label><input type="checkbox" id="eepromFlagClock"> Indep. Clock</label>
                        <label><input type="checkbox" id="eepromFlagVol"> HW Volume</label>
                        <label><input type="checkbox" id="eepromFlagFilter"> Filters</label>
                    </div>
                </div>
                <div class="form-group">
                    <label class="form-label">Sample Rates (comma-separated)</label>
                    <input type="text" class="form-input" id="eepromRates" placeholder="44100,48000,96000">
                </div>
                <div class="form-group">
                    <label class="form-label">Target EEPROM Address</label>
                    <select class="form-input" id="eepromTargetAddr">
                        <option value="80">0x50</option>
                        <option value="81">0x51</option>
                        <option value="82">0x52</option>
                        <option value="83">0x53</option>
                        <option value="84">0x54</option>
                        <option value="85">0x55</option>
                        <option value="86">0x56</option>
                        <option value="87">0x57</option>
                    </select>
                </div>
                <div class="btn-row" style="margin-top:8px">
                    <button class="btn btn-primary" onclick="eepromProgram()">Program</button>
                    <button class="btn btn-danger" onclick="eepromErase()">Erase</button>
                </div>
            </div>

            <!-- Emergency Protection -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    <span>Emergency Protection</span>
                    <span id="emergencyLimiterStatusBadge" class="badge" style="background:#4CAF50">Idle</span>
                </div>
                <div class="form-group">
                    <label class="form-label">Enable Limiter</label>
                    <label class="switch" style="float:right"><input type="checkbox" id="emergencyLimiterEnable" checked onchange="updateEmergencyLimiter('enabled')"><span class="slider round"></span></label>
                    <div style="clear:both"></div>
                    <small style="color:#888">Brick-wall limiter prevents speaker damage from audio peaks</small>
                </div>
                <div class="form-group">
                    <label class="form-label">Threshold: <span id="emergencyLimiterThresholdVal">-0.5</span> dBFS</label>
                    <input type="range" class="form-input" id="emergencyLimiterThreshold" min="-6" max="0" value="-0.5" step="0.1" oninput="document.getElementById('emergencyLimiterThresholdVal').textContent=parseFloat(this.value).toFixed(1)" onchange="updateEmergencyLimiter('threshold')">
                    <small style="color:#888">Recommended: -0.5 to -1.0 dBFS for safety headroom</small>
                </div>
                <div class="info-row" style="margin-top:8px">
                    <span>Status:</span>
                    <span id="emergencyLimiterStatus">Idle</span>
                </div>
                <div class="info-row">
                    <span>Gain Reduction:</span>
                    <span id="emergencyLimiterGR">0.0 dB</span>
                </div>
                <div class="info-row">
                    <span>Lifetime Triggers:</span>
                    <span id="emergencyLimiterTriggers">0</span>
                </div>
            </div>

            <!-- Test Signal Generator -->
            <div class="card">
                <div class="card-title">Test Signal Generator</div>
                <div class="form-group">
                    <label class="form-label">Enable</label>
                    <label class="switch" style="float:right"><input type="checkbox" id="siggenEnable" onchange="updateSigGen()"><span class="slider round"></span></label>
                    <div style="clear:both"></div>
                </div>
                <div id="siggenFields" style="display:none">
                <div class="form-group">
                    <label class="form-label">Output Mode</label>
                    <select class="form-input" id="siggenOutputMode" onchange="updateSigGen()">
                        <option value="0">Software Injection</option>
                        <option value="1">PWM Output (GPIO 38)</option>
                    </select>
                </div>
                <div id="siggenTargetAdcGroup">
                <div class="form-group">
                    <label class="form-label">Target ADC</label>
                    <select class="form-input" id="siggenTargetAdc" onchange="updateSigGen()">
                        <option value="0">ADC 1</option>
                        <option value="1">ADC 2</option>
                        <option value="2" selected>Both</option>
                    </select>
                </div>
                </div>
                <div class="form-group">
                    <label class="form-label">Waveform</label>
                    <select class="form-input" id="siggenWaveform" onchange="updateSigGen()">
                        <option value="0">Sine</option>
                        <option value="1">Square</option>
                        <option value="2">White Noise</option>
                        <option value="3">Frequency Sweep</option>
                    </select>
                </div>
                <div class="form-group">
                    <label class="form-label">Frequency: <span id="siggenFreqVal">1000</span> Hz</label>
                    <input type="range" class="form-input" id="siggenFreq" min="1" max="22000" value="1000" step="1" oninput="document.getElementById('siggenFreqVal').textContent=this.value" onchange="updateSigGen()">
                </div>
                <div class="form-group">
                    <label class="form-label">Amplitude: <span id="siggenAmpVal">-6</span> dBFS</label>
                    <input type="range" class="form-input" id="siggenAmp" min="-96" max="0" value="-6" step="1" oninput="document.getElementById('siggenAmpVal').textContent=this.value" onchange="updateSigGen()">
                </div>
                <div class="form-group">
                    <label class="form-label">Channel</label>
                    <select class="form-input" id="siggenChannel" onchange="updateSigGen()">
                        <option value="0">Ch 1</option>
                        <option value="1">Ch 2</option>
                        <option value="2" selected>Both</option>
                    </select>
                </div>
                <div class="form-group" id="siggenSweepGroup" style="display:none">
                    <label class="form-label">Sweep Speed (Hz/s)</label>
                    <input type="number" class="form-input" id="siggenSweepSpeed" min="1" max="22000" value="1000" onchange="updateSigGen()">
                </div>
                <div style="display:flex;flex-wrap:wrap;gap:4px;margin-top:8px">
                    <button class="btn btn-outline" onclick="siggenPreset(0,440,-6)">440 Hz</button>
                    <button class="btn btn-outline" onclick="siggenPreset(0,1000,-6)">1 kHz</button>
                    <button class="btn btn-outline" onclick="siggenPreset(0,100,-6)">100 Hz</button>
                    <button class="btn btn-outline" onclick="siggenPreset(0,10000,-6)">10 kHz</button>
                    <button class="btn btn-outline" onclick="siggenPreset(2,1000,-6)">Noise</button>
                    <button class="btn btn-outline" onclick="siggenPreset(3,20000,-6)">Sweep</button>
                </div>
                <p id="siggenPwmNote" style="display:none;font-size:11px;color:#FFA726;margin-top:8px">PWM output requires external RC low-pass filter for sine approximation.</p>
                </div>
            </div>
        </section>

        <!-- ===== DSP TAB ===== -->
        <section id="dsp" class="panel">
            <!-- DSP Control -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    DSP Control
                    <div style="display:flex;align-items:center;gap:8px;">
                        <span style="font-size:11px;color:var(--text-secondary);">Enable</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="dspEnableToggle" onchange="dspSetEnabled(this.checked)">
                            <span class="slider round"></span>
                        </label>
                    </div>
                </div>
                <div class="info-box">
                    <div class="info-row">
                        <span class="info-label">Global Bypass</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="dspBypassToggle" onchange="dspSetBypass(this.checked)">
                            <span class="slider round"></span>
                        </label>
                    </div>
                    <div class="info-row">
                        <span class="info-label">DSP CPU</span>
                        <span class="info-value" id="dspCpuText">0%</span>
                    </div>
                    <div class="dsp-cpu-bar"><div class="fill" id="dspCpuBar" style="width:0%"></div></div>
                    <div class="info-row" style="margin-top:8px;">
                        <span class="info-label">Sample Rate</span>
                        <span class="info-value" id="dspSampleRate">48000 Hz</span>
                    </div>
                </div>
            </div>

            <!-- DSP Presets -->
            <div class="card">
                <div class="card-title">Presets (<span id="dspPresetCount">0</span>)</div>
                <div id="dspPresetList" style="margin-top:8px;"></div>
                <button class="dsp-add-btn" id="dspAddPresetBtn" onclick="dspShowAddPresetDialog()">+ Add Preset</button>
                <span id="dspPresetModified" style="font-size:10px;color:var(--accent);display:none;margin-top:4px;">Modified</span>
            </div>

            <!-- Channel Selector -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    Channel
                    <div style="display:flex;align-items:center;gap:6px;">
                        <button class="btn btn-secondary" id="peqLinkBtn" onclick="peqToggleLink()" style="padding:2px 8px;font-size:11px;" title="Link L/R channels">Link L/R</button>
                    </div>
                </div>
                <div class="dsp-ch-tabs" id="dspChTabs"></div>
                <div class="info-box">
                    <div class="info-row">
                        <span class="info-label">Channel Bypass</span>
                        <label class="switch" style="transform:scale(0.75);">
                            <input type="checkbox" id="dspChBypassToggle" onchange="dspSetChBypass(this.checked)">
                            <span class="slider round"></span>
                        </label>
                    </div>
                </div>
            </div>

            <!-- Frequency Response Graph -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    Frequency Response
                    <div style="display:flex;gap:8px;" id="peqGraphToggles">
                        <button class="btn btn-secondary btn-small peq-graph-tog active" id="togIndividual" onclick="peqToggleGraphLayer('individual')">Individual</button>
                        <button class="btn btn-secondary btn-small peq-graph-tog" id="togRta" onclick="peqToggleGraphLayer('rta')">RTA</button>
                        <button class="btn btn-secondary btn-small peq-graph-tog active" id="togChain" onclick="peqToggleGraphLayer('chain')">Chain</button>
                    </div>
                </div>
                <canvas class="dsp-freq-canvas" id="dspFreqCanvas" style="cursor:crosshair;"></canvas>
            </div>

            <!-- PEQ Bands -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    EQ Bands
                    <div style="display:flex;gap:4px;">
                        <button id="peqToggleAllBtn" class="btn btn-secondary" onclick="peqToggleAll()" style="padding:2px 8px;font-size:11px;">Enable All</button>
                        <select id="peqCopyTo" class="select-sm" style="font-size:11px;" onchange="peqCopyChannel(this.value);this.value=''">
                            <option value="">Copy to...</option>
                            <option value="0">L1</option>
                            <option value="1">R1</option>
                            <option value="2">L2</option>
                            <option value="3">R2</option>
                            <option value="all">All Channels</option>
                        </select>
                        <select id="peqPresetSel" class="select-sm" style="font-size:11px;" onchange="peqPresetAction(this.value);this.value=''">
                            <option value="">Presets...</option>
                            <option value="_save">Save Preset...</option>
                            <option value="_load">Load Preset...</option>
                        </select>
                    </div>
                </div>
                <div class="peq-band-strip" id="peqBandStrip"></div>
                <div id="peqBandDetail"></div>
            </div>

            <!-- Additional Processing (chain stages) -->
            <div class="card">
                <div class="card-title" id="dspStageTitle" style="display:flex;align-items:center;justify-content:space-between;">
                    <span id="dspStageTitleText">Additional Processing (0)</span>
                    <select id="chainCopyTo" class="select-sm" style="font-size:11px;" onchange="dspCopyChainChannel(this.value);this.value=''">
                        <option value="">Copy to...</option>
                        <option value="0">L1</option>
                        <option value="1">R1</option>
                        <option value="2">L2</option>
                        <option value="3">R2</option>
                        <option value="all">All Channels</option>
                    </select>
                </div>
                <div style="margin-top:8px;">
                    <div id="dspStageList"></div>
                    <button class="dsp-add-btn" id="dspAddBtn" onclick="dspToggleAddMenu()">+ Add Stage</button>
                    <div class="dsp-add-menu" id="dspAddMenu">
                        <div class="menu-cat" style="color:#e6a817">Dynamics</div>
                        <div class="menu-item" onclick="dspAddStage(12)">Limiter</div>
                        <div class="menu-item" onclick="dspAddStage(18)">Compressor</div>
                        <div class="menu-item" onclick="dspAddStage(24)">Noise Gate</div>
                        <div class="menu-item" onclick="dspAddStage(30)">Multiband Comp</div>
                        <div class="menu-cat" style="color:#43a047">Tone Shaping</div>
                        <div class="menu-item" onclick="dspAddStage(25)">Tone Controls</div>
                        <div class="menu-item" onclick="dspAddStage(28)">Loudness Comp</div>
                        <div class="menu-item" onclick="dspAddStage(29)">Bass Enhance</div>
                        <div class="menu-cat" style="color:#8e24aa">Stereo / Protection</div>
                        <div class="menu-item" onclick="dspAddStage(27)">Stereo Width</div>
                        <div class="menu-item" onclick="dspAddStage(26)">Speaker Protection</div>
                        <div class="menu-cat" style="color:#757575">Utility</div>
                        <div class="menu-item" onclick="dspAddStage(14)">Gain</div>
                        <div class="menu-item" onclick="dspAddStage(15)">Delay</div>
                        <div class="menu-item" onclick="dspAddStage(16)">Polarity Invert</div>
                        <div class="menu-item" onclick="dspAddStage(17)">Mute</div>
                        <div class="menu-item" onclick="dspAddStage(13)">FIR Filter</div>
                        <div class="menu-item" onclick="dspAddDCBlock()">DC Block</div>
                        <div class="menu-item" onclick="dspShowBaffleModal()">Baffle Step...</div>
                        <div class="menu-cat" style="color:#1e88e5">Crossover</div>
                        <div class="menu-item" onclick="dspShowCrossoverModal()">Crossover Preset...</div>
                        <div class="menu-cat" style="color:#ef6c00">Analysis</div>
                        <div class="menu-item" onclick="dspShowThdModal()">THD+N Measurement...</div>
                    </div>
                </div>
            </div>

            <!-- Import / Export -->
            <div class="card">
                <div class="card-title">Import / Export</div>
                <div class="btn-row" style="flex-wrap:wrap;gap:8px;">
                    <button class="btn btn-secondary" onclick="dspImportApo()">Import REW</button>
                    <button class="btn btn-secondary" onclick="dspExportApo()">Export REW</button>
                    <button class="btn btn-secondary" onclick="dspImportJson()">Import JSON</button>
                    <button class="btn btn-secondary" onclick="dspExportJson()">Export JSON</button>
                </div>
                <input type="file" id="dspFileInput" accept=".txt,.json" style="display:none" onchange="dspHandleFileImport(event)">
            </div>

            <!-- Quick Setup: Routing Matrix -->
            <div class="card">
                <div class="collapsible-header" onclick="this.classList.toggle('open');this.nextElementSibling.classList.toggle('open')">
                    <span class="card-title" style="margin-bottom:0;">Routing Matrix</span>
                    <svg viewBox="0 0 24 24" style="width:20px;height:20px;fill:var(--text-secondary);"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z"/></svg>
                </div>
                <div class="collapsible-content" style="margin-top:8px;">
                    <div class="btn-row" style="flex-wrap:wrap;gap:6px;margin-bottom:12px;">
                        <button class="btn btn-secondary" onclick="dspRoutingPreset('identity')">1:1</button>
                        <button class="btn btn-secondary" onclick="dspRoutingPreset('mono_sum')">Mono Sum</button>
                        <button class="btn btn-secondary" onclick="dspRoutingPreset('swap_lr')">Swap L/R</button>
                        <button class="btn btn-secondary" onclick="dspRoutingPreset('sub_sum')">Sub Sum</button>
                    </div>
                    <div id="dspRoutingGrid" style="overflow-x:auto;"></div>
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
                        <div style="display: flex; gap: 8px; align-items: center;">
                            <select class="form-input" id="wifiNetworkSelect" style="flex: 1; min-width: 0;" onchange="onNetworkSelect()">
                                <option value="">-- Select a network --</option>
                            </select>
                            <button type="button" class="btn btn-secondary" onclick="scanWiFiNetworks()" id="scanBtn" style="padding: 0; width: 48px; min-height: 48px; min-width: 48px; font-size: 18px; flex-shrink: 0;" title="Scan for networks">
                                🔍
                            </button>
                        </div>
                        <div id="scanStatus" style="font-size: 12px; color: rgba(255,255,255,0.6); margin-top: 4px;"></div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Network Name (SSID)</label>
                        <input type="text" class="form-input" id="appState.wifiSSID" inputmode="text" placeholder="Enter WiFi SSID or select from above">
                    </div>
                    <div class="form-group">
                        <label class="form-label">Password</label>
                        <div class="password-wrapper">
                            <input type="password" class="form-input" id="appState.wifiPassword" placeholder="Enter password">
                            <button type="button" class="password-toggle" onclick="togglePasswordVisibility('appState.wifiPassword', this)">👁</button>
                        </div>
                    </div>
                    <div class="toggle-row" style="margin: 16px 0;">
                        <div>
                            <div class="toggle-label">Static IP</div>
                            <div class="toggle-sublabel">Use static IP instead of DHCP</div>
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="useStaticIP" onchange="toggleStaticIPFields()">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div id="staticIPFields" style="display: none;">
                        <div class="form-group">
                            <label class="form-label">IPv4 Address</label>
                            <input type="text" class="form-input" id="staticIP" inputmode="decimal" placeholder="e.g., 192.168.1.100" onchange="updateStaticIPDefaults()">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Network Mask</label>
                            <input type="text" class="form-input" id="subnet" inputmode="decimal" placeholder="e.g., 255.255.255.0" value="255.255.255.0">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Gateway</label>
                            <input type="text" class="form-input" id="gateway" inputmode="decimal" placeholder="e.g., 192.168.1.1">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Primary DNS</label>
                            <input type="text" class="form-input" id="dns1" inputmode="decimal" placeholder="e.g., 192.168.1.1">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Secondary DNS (Optional)</label>
                            <input type="text" class="form-input" id="dns2" inputmode="decimal" placeholder="e.g., 8.8.8.8">
                        </div>
                    </div>
                    <div style="display: flex; gap: 8px;">
                        <button type="submit" class="btn btn-primary" style="flex: 1;">Connect</button>
                        <button type="button" class="btn btn-secondary" style="flex: 1;" onclick="saveNetworkSettings(event)">Save Settings</button>
                    </div>
                </form>
            </div>

            <!-- Network Configuration -->
            <div class="card">
                <div class="card-title">Network Configuration</div>
                <div class="form-group">
                    <label class="form-label">Select Network</label>
                    <select class="form-input" id="configNetworkSelect" onchange="loadNetworkConfig()">
                        <option value="">-- Select a saved network --</option>
                    </select>
                </div>
                <div id="networkConfigFields" style="display: none;">
                    <div class="form-group">
                        <label class="form-label">Password</label>
                        <div class="password-wrapper">
                            <input type="password" class="form-input" id="configPassword" placeholder="Enter network password">
                            <button type="button" class="password-toggle" onclick="togglePasswordVisibility('configPassword', this)">👁</button>
                        </div>
                    </div>
                    <div class="toggle-row" style="margin: 16px 0;">
                        <div>
                            <div class="toggle-label">Static IP</div>
                            <div class="toggle-sublabel">Use static IP for this network</div>
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="configUseStaticIP" onchange="toggleConfigStaticIPFields()">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div id="configStaticIPFields" style="display: none;">
                        <div class="form-group">
                            <label class="form-label">IPv4 Address</label>
                            <input type="text" class="form-input" id="configStaticIP" inputmode="decimal" placeholder="e.g., 192.168.1.100">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Network Mask</label>
                            <input type="text" class="form-input" id="configSubnet" inputmode="decimal" placeholder="e.g., 255.255.255.0" value="255.255.255.0">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Gateway</label>
                            <input type="text" class="form-input" id="configGateway" inputmode="decimal" placeholder="e.g., 192.168.1.1">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Primary DNS</label>
                            <input type="text" class="form-input" id="configDns1" inputmode="decimal" placeholder="e.g., 192.168.1.1">
                        </div>
                        <div class="form-group">
                            <label class="form-label">Secondary DNS (Optional)</label>
                            <input type="text" class="form-input" id="configDns2" inputmode="decimal" placeholder="e.g., 8.8.8.8">
                        </div>
                    </div>
                    <div style="display: flex; gap: 8px; flex-wrap: wrap;">
                        <button id="connectUpdateBtn" class="btn btn-primary" style="flex: 1; min-width: 120px;" onclick="updateNetworkConfig(true)">Connect</button>
                        <button class="btn btn-secondary" style="flex: 1; min-width: 120px;" onclick="updateNetworkConfig(false)">Update Network</button>
                        <button class="btn btn-danger" style="flex: 1; min-width: 120px;" onclick="removeSelectedNetworkConfig()">Remove Network</button>
                    </div>
                </div>
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
                <div id="apFields" style="display:none">
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Auto AP Mode</div>
                        <div class="toggle-sublabel">Enable AP if connection fails</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="autoAPToggle" onchange="toggleAutoAP()">
                        <span class="slider"></span>
                    </label>
                </div>
                <button class="btn btn-secondary mt-12" onclick="openAPConfig()">Configure AP</button>
                </div>
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
                        <div class="toggle-sublabel">Toggle to connect/disconnect</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="appState.mqttEnabled" onchange="toggleMqttEnabled()">
                        <span class="slider"></span>
                    </label>
                </div>
                <div id="mqttFields" style="display:none">
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Home Assistant Discovery</div>
                        <div class="toggle-sublabel">Auto-configure in Home Assistant</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="appState.mqttHADiscovery">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="divider"></div>
                <div class="form-group">
                    <label class="form-label">Broker Address</label>
                    <input type="text" class="form-input" id="appState.mqttBroker" inputmode="url" placeholder="mqtt.example.com">
                </div>
                <div class="form-group">
                    <label class="form-label">Port</label>
                    <input type="number" class="form-input" id="appState.mqttPort" inputmode="numeric" placeholder="1883" value="1883">
                </div>
                <div class="form-group">
                    <label class="form-label">Username (optional)</label>
                    <input type="text" class="form-input" id="appState.mqttUsername" placeholder="Username">
                </div>
                <div class="form-group">
                    <label class="form-label">Password (optional)</label>
                    <div class="password-wrapper">
                        <input type="password" class="form-input" id="appState.mqttPassword" placeholder="Password">
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('appState.mqttPassword', this)">👁</button>
                    </div>
                </div>
                <div class="form-group">
                    <label class="form-label">Base Topic</label>
                    <input type="text" class="form-input" id="appState.mqttBaseTopic" placeholder="ALX/device-serial">
                    <div class="text-secondary" style="font-size: 11px; margin-top: 4px;">Leave empty to use: <span id="mqttDefaultTopic">ALX/{serial}</span></div>
                </div>
                <button class="btn btn-primary" onclick="saveMqttSettings()">Save MQTT Settings</button>
                </div>
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
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Daylight Saving Time (DST)</div>
                        <div class="toggle-sublabel">Add 1 hour for DST</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="dstToggle" onchange="updateDST()">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="info-box">
                    <div><span id="timezoneInfo">Current timezone offset will be shown here</span></div>
                    <div style="margin-top: 8px;"><strong>Device time:</strong> <span id="currentTimeDisplay">Loading...</span></div>
                </div>
            </div>

            <!-- General -->
            <div class="card">
                <div class="card-title">General</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Dark Mode</div>
                        <div class="toggle-sublabel">Use darker theme</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="darkModeToggle" onchange="toggleTheme()">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Debug Mode</div>
                        <div class="toggle-sublabel">Enable debug tools</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="debugModeToggle" onchange="setDebugToggle('setDebugMode',this.checked)">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- Display -->
            <div class="card">
                <div class="card-title">Display</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Backlight</div>
                        <div class="toggle-sublabel">Turn screen on/off</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="backlightToggle" onchange="toggleBacklight()" checked>
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Brightness</div>
                        <div class="toggle-sublabel">Display brightness level</div>
                    </div>
                    <select id="brightnessSelect" onchange="setBrightness()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="10">10%</option>
                        <option value="25">25%</option>
                        <option value="50">50%</option>
                        <option value="75">75%</option>
                        <option value="100" selected>100%</option>
                    </select>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Screen Timeout</div>
                        <div class="toggle-sublabel">Auto-sleep delay</div>
                    </div>
                    <select id="screenTimeoutSelect" onchange="setScreenTimeout()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="30">30 sec</option>
                        <option value="60" selected>1 min</option>
                        <option value="300">5 min</option>
                        <option value="600">10 min</option>
                        <option value="0">Never</option>
                    </select>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Dim Display</div>
                        <div class="toggle-sublabel">Dim before sleep</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="dimToggle" onchange="toggleDim()">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="toggle-row" id="dimTimeoutRow" style="display:none;">
                    <div>
                        <div class="toggle-label">Dim Timeout</div>
                        <div class="toggle-sublabel">Delay before dimming</div>
                    </div>
                    <select id="dimTimeoutSelect" onchange="setDimTimeout()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="5">5 sec</option>
                        <option value="10" selected>10 sec</option>
                        <option value="15">15 sec</option>
                        <option value="30">30 sec</option>
                        <option value="60">1 min</option>
                    </select>
                </div>
                <div class="toggle-row" id="dimBrightnessRow" style="display:none;">
                    <div>
                        <div class="toggle-label">Dim Brightness</div>
                        <div class="toggle-sublabel">Brightness when dimmed</div>
                    </div>
                    <select id="dimBrightnessSelect" onchange="setDimBrightness()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="26" selected>10%</option>
                        <option value="64">25%</option>
                        <option value="128">50%</option>
                        <option value="191">75%</option>
                    </select>
                </div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Boot Animation</div>
                        <div class="toggle-sublabel">Animation on startup</div>
                    </div>
                    <select id="bootAnimSelect" onchange="setBootAnimation()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="-1">None</option>
                        <option value="0" selected>Wave Pulse</option>
                        <option value="1">Speaker</option>
                        <option value="2">Waveform</option>
                        <option value="3">Beat Bounce</option>
                        <option value="4">Freq Bars</option>
                        <option value="5">Heartbeat</option>
                    </select>
                </div>
            </div>

            <!-- Sound -->
            <div class="card">
                <div class="card-title">Sound</div>
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Buzzer</div>
                        <div class="toggle-sublabel">Audio feedback sounds</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="buzzerToggle" onchange="toggleBuzzer()" checked>
                        <span class="slider"></span>
                    </label>
                </div>
                <div id="buzzerFields" style="display:none">
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Buzzer Volume</div>
                        <div class="toggle-sublabel">Feedback sound level</div>
                    </div>
                    <select id="buzzerVolumeSelect" onchange="setBuzzerVolume()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                        <option value="0">Low</option>
                        <option value="1" selected>Medium</option>
                        <option value="2">High</option>
                    </select>
                </div>
                </div>
            </div>

            <!-- Security -->
            <div class="card">
                <div class="card-title">Security</div>
                <div class="button-row">
                    <button class="btn btn-secondary" onclick="showPasswordChangeModal()">Change Password</button>
                    <button class="btn btn-secondary" onclick="logout()">Logout</button>
                </div>
                <div class="info-box">
                    <div class="toggle-sublabel">Change your web interface password or logout from the current session.</div>
                </div>
            </div>

            <!-- Firmware Update -->
            <div class="card">
                <div class="card-title">Firmware Update</div>
                <div class="info-box">
                    <div class="version-row">
                        <span class="version-label">Current Version</span>
                        <span class="version-value">
                            <span id="currentVersion">Loading...</span>
                            <a href="#" id="currentVersionNotes" class="release-notes-link" onclick="showReleaseNotesFor('current'); return false;" title="View release notes">?</a>
                        </span>
                    </div>
                    <div class="version-row" id="latestVersionRow">
                        <span class="version-label">Latest Version</span>
                        <span class="version-value version-update">
                            <span id="latestVersion" style="opacity: 0.6; font-style: italic;">Checking...</span>
                            <a href="#" id="latestVersionNotes" class="release-notes-link" onclick="showReleaseNotesFor('latest'); return false;" title="View release notes">?</a>
                        </span>
                    </div>
                </div>
                
                <!-- Inline Release Notes Accordion -->
                <div id="inlineReleaseNotes" class="release-notes-inline">
                    <div class="release-notes-header">
                        <span id="inlineReleaseNotesTitle" class="release-notes-title">Release Notes</span>
                        <button type="button" class="release-notes-close" onclick="toggleInlineReleaseNotes(false)">×</button>
                    </div>
                    <div id="inlineReleaseNotesContent" class="release-notes-body"></div>
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

        <!-- ===== SUPPORT TAB ===== -->
        <section id="support" class="panel">
            <!-- Manual Access -->
            <div class="card">
                <div class="card-title">Manual Access</div>
                <div class="qr-container">
                    <div class="manual-description">Scan the QR code or click the link below to access the full user manual:</div>
                    <div class="qr-code" id="manualQrCode"></div>
                    <a href="#" id="manualLink" class="manual-link" target="_blank" rel="noopener noreferrer">Loading...</a>
                    <div class="manual-description">The manual includes setup instructions, troubleshooting tips, and feature documentation.</div>
                </div>
            </div>

            <!-- Documentation -->
            <div class="card">
                <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
                    <div class="card-title" style="margin-bottom: 0;">Documentation</div>
                    <input type="text" class="search-input" id="manualSearchInput" placeholder="Search..." oninput="searchManual(this.value)">
                </div>
                <div class="manual-search-status" id="manualSearchStatus"></div>
                <div class="manual-rendered" id="manualRendered">
                    <div class="manual-loading">Loading manual...</div>
                </div>
            </div>
        </section>

        <!-- ===== DEBUG TAB ===== -->
        <section id="debug" class="panel">
            <!-- Debug Controls -->
            <div class="card">
                <div class="card-title">Debug Controls</div>
                <div class="info-box">
                    <div class="toggle-row">
                        <span>Hardware Stats</span>
                        <label class="switch"><input type="checkbox" id="debugHwStatsToggle" onchange="setDebugToggle('setDebugHwStats',this.checked)"><span class="slider round"></span></label>
                    </div>
                    <div class="toggle-row">
                        <span>I2S Metrics</span>
                        <label class="switch"><input type="checkbox" id="debugI2sMetricsToggle" onchange="setDebugToggle('setDebugI2sMetrics',this.checked)"><span class="slider round"></span></label>
                    </div>
                    <div class="toggle-row">
                        <span>Task Monitor</span>
                        <label class="switch"><input type="checkbox" id="debugTaskMonitorToggle" onchange="setDebugToggle('setDebugTaskMonitor',this.checked)"><span class="slider round"></span></label>
                    </div>
                    <div class="toggle-row">
                        <div>
                            <div class="toggle-label">Serial Log Level</div>
                            <div class="toggle-sublabel">Debug output verbosity</div>
                        </div>
                        <select id="debugSerialLevel" onchange="setDebugSerialLevel(this.value)" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                            <option value="0">Off</option>
                            <option value="1">Errors Only</option>
                            <option value="2">Info (Normal)</option>
                            <option value="3">Debug (Verbose)</option>
                        </select>
                    </div>
                    <div class="toggle-row">
                        <div>
                            <div class="toggle-label">Refresh Rate</div>
                            <div class="toggle-sublabel">Stats update interval</div>
                        </div>
                        <select id="statsIntervalSelect" onchange="setStatsInterval()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                            <option value="1">1 second</option>
                            <option value="2" selected>2 seconds</option>
                            <option value="3">3 seconds</option>
                            <option value="5">5 seconds</option>
                            <option value="10">10 seconds</option>
                        </select>
                    </div>
                </div>
            </div>

            <!-- FreeRTOS Tasks -->
            <div id="taskMonitorSection">
            <div class="card">
                <div class="card-title">FreeRTOS Tasks</div>
                <div class="info-box">
                    <div class="stats-grid" style="margin-bottom:8px">
                        <div class="stat-card">
                            <div class="stat-value" id="tmCpuTotal">--%</div>
                            <div class="stat-label">CPU Load</div>
                        </div>
                        <div class="stat-card">
                            <div class="stat-value" id="tmLoopFreq">-- Hz</div>
                            <div class="stat-label">Loop Freq</div>
                        </div>
                    </div>
                    <div class="info-row"><span class="info-label">Core 0 Load</span><span class="info-value" id="tmCpuCore0">--</span></div>
                    <div class="info-row"><span class="info-label">Core 1 Load</span><span class="info-value" id="tmCpuCore1">--</span></div>
                    <div class="info-row"><span class="info-label">Task Count</span><span class="info-value" id="taskCount">--</span></div>
                    <div class="info-row"><span class="info-label">Loop Time (avg)</span><span class="info-value" id="loopTimeAvg">--</span></div>
                    <div class="info-row"><span class="info-label">Loop Time (max)</span><span class="info-value" id="loopTimeMax">--</span></div>
                    <div class="divider"></div>
                    <table class="task-table" id="taskTable">
                        <thead>
                            <tr>
                                <th onclick="sortTaskTable(0)" data-col="0" class="sortable">Name <span class="sort-arrow"></span></th>
                                <th onclick="sortTaskTable(1)" data-col="1" class="sortable">Stack <span class="sort-arrow"></span></th>
                                <th onclick="sortTaskTable(2)" data-col="2" class="sortable">Used% <span class="sort-arrow"></span></th>
                                <th onclick="sortTaskTable(3)" data-col="3" class="sortable">Pri <span class="sort-arrow"></span></th>
                                <th onclick="sortTaskTable(4)" data-col="4" class="sortable">State <span class="sort-arrow"></span></th>
                                <th onclick="sortTaskTable(5)" data-col="5" class="sortable">Core <span class="sort-arrow"></span></th>
                            </tr>
                        </thead>
                        <tbody id="taskTableBody">
                        </tbody>
                    </table>
                </div>
            </div>
            </div>

            <!-- CPU Stats (always visible when debug tab is shown) -->
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
                <div class="graph-embedded">
                    <div class="graph-legend">CPU Usage (Orange: Total, Light: Core 0, Dark: Core 1)</div>
                    <canvas class="graph-canvas" id="cpuGraph"></canvas>
                </div>
                <div class="info-box-compact mt-12">
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

            <div id="hwStatsSection">
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
                <div class="graph-embedded">
                    <div class="graph-legend">Heap Memory Usage (%)</div>
                    <canvas class="graph-canvas" id="memoryGraph"></canvas>
                </div>
                <div class="graph-embedded" id="psramGraphContainer" style="display: none;">
                    <div class="graph-legend">PSRAM Usage (%)</div>
                    <canvas class="graph-canvas" id="psramGraph"></canvas>
                </div>
                <div class="info-box-compact mt-12">
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
                        <div class="stat-value" id="LittleFSPercent">--%</div>
                        <div class="stat-label">LittleFS Used</div>
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
                        <span class="info-label">LittleFS Used</span>
                        <span class="info-value" id="LittleFSUsed">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">LittleFS Total</span>
                        <span class="info-value" id="LittleFSTotal">--</span>
                    </div>
                </div>
            </div>

            <!-- WiFi & System Stats -->
            <div class="card">
                <div class="card-title">WiFi & System</div>
                <div class="info-box">
                    <div class="info-row">
                        <span class="info-label">Reset Reason</span>
                        <span class="info-value" id="resetReason">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Serial Number</span>
                        <span class="info-value" id="deviceSerial">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">MAC Address</span>
                        <span class="info-value" id="deviceMac">--</span>
                    </div>
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

            <!-- Audio ADC Diagnostics -->
            <div class="card">
                <div class="card-title">Audio ADC</div>
                <div class="info-box">
                    <div class="dual-canvas-grid">
                        <div>
                            <div style="font-size:12px;font-weight:600;color:var(--text-secondary);margin-bottom:4px;">ADC 1</div>
                            <div class="info-row">
                                <span class="info-label">ADC Status</span>
                                <span class="info-value" id="adcStatus">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Noise Floor</span>
                                <span class="info-value" id="adcNoiseFloor">-- dBFS</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">I2S Errors</span>
                                <span class="info-value" id="adcI2sErrors">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Consecutive Zeros</span>
                                <span class="info-value" id="adcConsecutiveZeros">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Total Buffers</span>
                                <span class="info-value" id="adcTotalBuffers">--</span>
                            </div>
                        </div>
                        <div>
                            <div style="font-size:12px;font-weight:600;color:var(--text-secondary);margin-bottom:4px;">ADC 2</div>
                            <div class="info-row">
                                <span class="info-label">ADC Status</span>
                                <span class="info-value" id="adcStatus1">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Noise Floor</span>
                                <span class="info-value" id="adcNoiseFloor1">-- dBFS</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">I2S Errors</span>
                                <span class="info-value" id="adcI2sErrors1">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Consecutive Zeros</span>
                                <span class="info-value" id="adcConsecutiveZeros1">--</span>
                            </div>
                            <div class="info-row">
                                <span class="info-label">Total Buffers</span>
                                <span class="info-value" id="adcTotalBuffers1">--</span>
                            </div>
                        </div>
                    </div>
                    <div class="divider"></div>
                    <div class="info-row">
                        <span class="info-label">Sample Rate</span>
                        <span class="info-value" id="adcSampleRate">--</span>
                    </div>
                </div>
            </div>

            </div>

            <!-- Audio DAC Diagnostics -->
            <div class="card">
                <div class="card-title">Audio DAC</div>
                <div class="info-box">
                    <div class="info-row">
                        <span class="info-label">Status</span>
                        <span class="info-value" id="dacStatus">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Model</span>
                        <span class="info-value" id="dacModel">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Manufacturer</span>
                        <span class="info-value" id="dacManufacturer">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Device ID</span>
                        <span class="info-value" id="dacDeviceId">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Detection</span>
                        <span class="info-value" id="dacDetection">--</span>
                    </div>
                    <div class="divider"></div>
                    <div class="info-row">
                        <span class="info-label">Enabled</span>
                        <span class="info-value" id="dacDbgEnabled">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Volume</span>
                        <span class="info-value" id="dacDbgVolume">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Mute</span>
                        <span class="info-value" id="dacDbgMute">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Output Channels</span>
                        <span class="info-value" id="dacDbgChannels">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Filter Mode</span>
                        <span class="info-value" id="dacDbgFilter">--</span>
                    </div>
                    <div class="divider"></div>
                    <div class="info-row">
                        <span class="info-label">HW Volume</span>
                        <span class="info-value" id="dacHwVolume">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">I2C Control</span>
                        <span class="info-value" id="dacI2cControl">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Independent Clock</span>
                        <span class="info-value" id="dacIndepClock">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Filter Support</span>
                        <span class="info-value" id="dacHasFilters">--</span>
                    </div>
                    <div class="divider"></div>
                    <div class="info-row">
                        <span class="info-label">I2S TX Active</span>
                        <span class="info-value" id="dacI2sTxEnabled">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Volume Gain</span>
                        <span class="info-value" id="dacVolumeGain">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">TX Writes</span>
                        <span class="info-value" id="dacTxWrites">0</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">TX Data</span>
                        <span class="info-value" id="dacTxData">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">TX Peak Sample</span>
                        <span class="info-value" id="dacTxPeak">0</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">TX Zero Frames</span>
                        <span class="info-value" id="dacTxZeroFrames">0</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">TX Underruns</span>
                        <span class="info-value" id="dacTxUnderruns">0</span>
                    </div>
                </div>
            </div>

            <!-- EEPROM / I2C Diagnostics -->
            <div class="card">
                <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                    EEPROM / I2C
                    <button class="btn btn-secondary btn-small" onclick="eepromScan()">Re-Scan</button>
                </div>
                <div class="info-box">
                    <div class="info-row"><span class="info-label">EEPROM Found</span><span class="info-value" id="dbgEepromFound">--</span></div>
                    <div class="info-row"><span class="info-label">I2C Address</span><span class="info-value" id="dbgEepromAddr">--</span></div>
                    <div class="info-row"><span class="info-label">Device ID</span><span class="info-value" id="dbgEepromDeviceId">--</span></div>
                    <div class="info-row"><span class="info-label">Device Name</span><span class="info-value" id="dbgEepromName">--</span></div>
                    <div class="info-row"><span class="info-label">Manufacturer</span><span class="info-value" id="dbgEepromMfr">--</span></div>
                    <div class="info-row"><span class="info-label">HW Revision</span><span class="info-value" id="dbgEepromRev">--</span></div>
                    <div class="info-row"><span class="info-label">Channels</span><span class="info-value" id="dbgEepromCh">--</span></div>
                    <div class="info-row"><span class="info-label">DAC I2C Addr</span><span class="info-value" id="dbgEepromDacAddr">--</span></div>
                    <div class="info-row"><span class="info-label">Flags</span><span class="info-value" id="dbgEepromFlags">--</span></div>
                    <div class="info-row"><span class="info-label">Sample Rates</span><span class="info-value" id="dbgEepromRates">--</span></div>
                    <div class="divider"></div>
                    <div class="info-row"><span class="info-label">I2C Devices</span><span class="info-value" id="dbgI2cCount">--</span></div>
                    <div class="info-row"><span class="info-label">Read Errors</span><span class="info-value" id="dbgEepromRdErr">0</span></div>
                    <div class="info-row"><span class="info-label">Write Errors</span><span class="info-value" id="dbgEepromWrErr">0</span></div>
                    <div class="divider"></div>
                    <div style="margin-top:4px">
                        <button class="btn btn-secondary btn-small" onclick="eepromLoadHex()">Load Hex Dump</button>
                    </div>
                    <pre id="dbgEepromHex" style="display:none;font-size:10px;overflow-x:auto;max-height:200px;background:var(--bg-secondary);padding:6px;border-radius:4px;margin-top:4px;word-break:break-all"></pre>
                </div>
            </div>

            <!-- I2S Configuration -->
            <div id="i2sMetricsSection">
            <div class="card">
                <div class="card-title">I2S Configuration</div>
                <div class="info-box">
                    <div class="dual-canvas-grid">
                        <div>
                            <div style="font-size:12px;font-weight:600;color:var(--text-secondary);margin-bottom:4px;">ADC 1 (Master)</div>
                            <div class="info-row"><span class="info-label">Mode</span><span class="info-value" id="i2sMode0">--</span></div>
                            <div class="info-row"><span class="info-label">Sample Rate</span><span class="info-value" id="i2sSampleRate0">--</span></div>
                            <div class="info-row"><span class="info-label">Bits</span><span class="info-value" id="i2sBits0">--</span></div>
                            <div class="info-row"><span class="info-label">Channels</span><span class="info-value" id="i2sChannels0">--</span></div>
                            <div class="info-row"><span class="info-label">DMA Buffers</span><span class="info-value" id="i2sDma0">--</span></div>
                            <div class="info-row"><span class="info-label">APLL</span><span class="info-value" id="i2sApll0">--</span></div>
                            <div class="info-row"><span class="info-label">MCLK</span><span class="info-value" id="i2sMclk0">--</span></div>
                            <div class="info-row"><span class="info-label">Format</span><span class="info-value" id="i2sFormat0">--</span></div>
                        </div>
                        <div>
                            <div style="font-size:12px;font-weight:600;color:var(--text-secondary);margin-bottom:4px;">ADC 2 (Master)</div>
                            <div class="info-row"><span class="info-label">Mode</span><span class="info-value" id="i2sMode1">--</span></div>
                            <div class="info-row"><span class="info-label">Sample Rate</span><span class="info-value" id="i2sSampleRate1">--</span></div>
                            <div class="info-row"><span class="info-label">Bits</span><span class="info-value" id="i2sBits1">--</span></div>
                            <div class="info-row"><span class="info-label">Channels</span><span class="info-value" id="i2sChannels1">--</span></div>
                            <div class="info-row"><span class="info-label">DMA Buffers</span><span class="info-value" id="i2sDma1">--</span></div>
                            <div class="info-row"><span class="info-label">APLL</span><span class="info-value" id="i2sApll1">--</span></div>
                            <div class="info-row"><span class="info-label">MCLK</span><span class="info-value" id="i2sMclk1">--</span></div>
                            <div class="info-row"><span class="info-label">Format</span><span class="info-value" id="i2sFormat1">--</span></div>
                        </div>
                    </div>
                    <div class="divider"></div>
                    <div style="font-size:12px;font-weight:600;color:var(--text-secondary);margin-bottom:4px;">Runtime Performance</div>
                    <div class="info-row"><span class="info-label">Audio Task Stack Free</span><span class="info-value" id="i2sStackFree">--</span></div>
                    <div class="dual-canvas-grid">
                        <div>
                            <div class="info-row"><span class="info-label">ADC1 Throughput</span><span class="info-value" id="i2sThroughput0">--</span></div>
                            <div class="info-row"><span class="info-label">ADC1 Read Latency</span><span class="info-value" id="i2sLatency0">--</span></div>
                        </div>
                        <div>
                            <div class="info-row"><span class="info-label">ADC2 Throughput</span><span class="info-value" id="i2sThroughput1">--</span></div>
                            <div class="info-row"><span class="info-label">ADC2 Read Latency</span><span class="info-value" id="i2sLatency1">--</span></div>
                        </div>
                    </div>
                </div>
            </div>
            </div>


            <!-- Pin Configuration -->
            <div class="card">
                <div class="card-title">Pin Configuration</div>
                <table class="pin-table" id="pinTable">
                    <thead>
                        <tr>
                            <th onclick="sortPinTable(0)" data-col="0">GPIO <span class="sort-arrow">&#9650;</span></th>
                            <th onclick="sortPinTable(1)" data-col="1">Function <span class="sort-arrow">&#9650;</span></th>
                            <th onclick="sortPinTable(2)" data-col="2">Device <span class="sort-arrow">&#9650;</span></th>
                            <th onclick="sortPinTable(3)" data-col="3">Category <span class="sort-arrow">&#9650;</span></th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr><td>2</td><td>LED</td><td>Status LED</td><td><span class="pin-cat pin-cat-core">Core</span></td></tr>
                        <tr><td>4</td><td>Amplifier Relay</td><td>Relay Module</td><td><span class="pin-cat pin-cat-core">Core</span></td></tr>
                        <tr><td>8</td><td>Buzzer (PWM)</td><td>Piezo Buzzer</td><td><span class="pin-cat pin-cat-core">Core</span></td></tr>
                        <tr><td>15</td><td>Reset Button</td><td>Tactile Switch</td><td><span class="pin-cat pin-cat-core">Core</span></td></tr>
                        <tr><td>38</td><td>Signal Gen (PWM)</td><td>Signal Generator</td><td><span class="pin-cat pin-cat-core">Core</span></td></tr>
                        <tr><td>3</td><td>I2S MCLK</td><td>PCM1808 ADC 1 &amp; 2</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>16</td><td>I2S BCK</td><td>PCM1808 ADC 1 &amp; 2</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>17</td><td>I2S DOUT</td><td>PCM1808 ADC 1</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>18</td><td>I2S LRC</td><td>PCM1808 ADC 1 &amp; 2</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>9</td><td>I2S DOUT2</td><td>PCM1808 ADC 2</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>40</td><td>I2S TX Data</td><td>DAC Output</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>41</td><td>I2C SDA</td><td>DAC EEPROM / I2C</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>42</td><td>I2C SCL</td><td>DAC EEPROM / I2C</td><td><span class="pin-cat pin-cat-audio">Audio</span></td></tr>
                        <tr><td>5</td><td>Encoder A</td><td>Rotary Encoder</td><td><span class="pin-cat pin-cat-input">Input</span></td></tr>
                        <tr><td>6</td><td>Encoder B</td><td>Rotary Encoder</td><td><span class="pin-cat pin-cat-input">Input</span></td></tr>
                        <tr><td>7</td><td>Encoder SW</td><td>Rotary Encoder</td><td><span class="pin-cat pin-cat-input">Input</span></td></tr>
                        <tr><td>10</td><td>TFT CS</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                        <tr><td>11</td><td>TFT MOSI</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                        <tr><td>12</td><td>TFT SCLK</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                        <tr><td>13</td><td>TFT DC</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                        <tr><td>14</td><td>TFT RST</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                        <tr><td>21</td><td>TFT Backlight</td><td>ST7735S TFT</td><td><span class="pin-cat pin-cat-display">Display</span></td></tr>
                    </tbody>
                </table>
            </div>

            <!-- Audio Quality Diagnostics -->
            <div class="card">
                <div class="card-title">Audio Quality Diagnostics</div>
                <div class="info-box">
                    <div class="toggle-row">
                        <span>Enable Diagnostics</span>
                        <label class="switch"><input type="checkbox" id="audioQualityEnabled" onchange="updateAudioQuality('enabled')"><span class="slider round"></span></label>
                    </div>
                    <div class="form-row">
                        <label for="audioQualityThreshold">Glitch Threshold (0.1-1.0):</label>
                        <input type="number" id="audioQualityThreshold" min="0.1" max="1.0" step="0.1" value="0.5" onchange="updateAudioQuality('threshold')">
                    </div>
                </div>
                <div class="stats-grid" id="audioQualityStats">
                    <div class="stat-card">
                        <div class="stat-value" id="aqGlitchesTotal">0</div>
                        <div class="stat-label">Total Glitches</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value" id="aqGlitchesMinute">0</div>
                        <div class="stat-label">Last Minute</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value" id="aqLatencyAvg">--</div>
                        <div class="stat-label">Avg Latency</div>
                    </div>
                </div>
                <div class="info-box-compact mt-12">
                    <div class="info-row">
                        <span class="info-label">Last Glitch Type</span>
                        <span class="info-value" id="aqLastGlitchType">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">DSP Swap Correlation</span>
                        <span class="info-value badge" id="aqCorrelationDsp">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">WiFi Correlation</span>
                        <span class="info-value badge" id="aqCorrelationWifi">--</span>
                    </div>
                </div>
                <div class="btn-row mt-12">
                    <button class="btn btn-secondary" onclick="resetAudioQualityStats()">Reset Statistics</button>
                </div>
            </div>

            <!-- Debug Console -->
            <div class="card">
                <div class="card-title">Debug Console</div>
                <div class="form-row mb-12">
                    <label for="logLevelFilter" style="margin-right: 8px; font-weight: 500;">Log Level:</label>
                    <select id="logLevelFilter" class="select-sm" onchange="setLogFilter(this.value)" style="min-width:120px;">
                        <option value="all">All Levels</option>
                        <option value="debug">Debug</option>
                        <option value="info">Info</option>
                        <option value="warn">Warning</option>
                        <option value="error">Error</option>
                    </select>
                </div>
                <div class="debug-console" id="debugConsole">
                    <div class="log-entry" data-level="info"><span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Waiting for messages...</span></div>
                </div>
                <div class="btn-row mt-12">
                    <button class="btn btn-secondary" id="pauseBtn" onclick="toggleDebugPause()">Pause</button>
                    <button class="btn btn-secondary" onclick="clearDebugConsole()">Clear</button>
                    <button class="btn btn-primary" onclick="downloadDebugLog()">Download Logs</button>
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
                    <input type="text" class="form-input" id="appState.apSSID" placeholder="ALX-XXXXXXXXXXXX">
                </div>
                <div class="form-group">
                    <label class="form-label">AP Password</label>
                    <div class="password-wrapper">
                        <input type="password" class="form-input" id="appState.apPassword" placeholder="Min 8 characters (leave empty to keep current)">
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('appState.apPassword', this)">👁</button>
                    </div>
                </div>
                <button type="submit" class="btn btn-primary mb-8">Save AP Settings</button>
                <button type="button" class="btn btn-secondary" onclick="closeAPConfig()">Cancel</button>
            </form>
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
        let darkMode = true;
        let waveformAutoScaleEnabled = false;
        let backlightOn = true;
        let backlightBrightness = 255;
        let screenTimeoutSec = 60;
        let dimEnabled = false;
        let dimTimeoutSec = 10;
        let dimBrightnessPwm = 26;
        let enableCertValidation = true;
        let currentFirmwareVersion = '';
        let currentLatestVersion = '';
        let currentTimezoneOffset = 3600;
        let currentAPSSID = '';
        let manualUploadInProgress = false;
        let debugPaused = false;
        let debugLogBuffer = [];
        const DEBUG_MAX_LINES = 1000;
        let currentLogFilter = 'all'; // all, debug, info, warn, error
        let audioSubscribed = false;
        let currentActiveTab = 'control';

        // Dual ADC count
        const NUM_ADCS = 2;
        let numAdcsDetected = 1;
        let inputNames = ['Subwoofer 1','Subwoofer 2','Subwoofer 3','Subwoofer 4'];

        // Audio animation state (rAF interpolation) — per-ADC
        let waveformCurrent = [null, null], waveformTarget = [null, null];
        let spectrumCurrent = [new Float32Array(16), new Float32Array(16)];
        let spectrumTarget = [new Float32Array(16), new Float32Array(16)];
        let currentDominantFreq = [0, 0], targetDominantFreq = [0, 0];
        let audioAnimFrameId = null;
        let LERP_SPEED = 0.25;

        // Spectrum peak hold state — per-ADC
        let spectrumPeaks = [new Float32Array(16), new Float32Array(16)];
        let spectrumPeakTimes = [new Float64Array(16), new Float64Array(16)];

        // VU meter animation state (rAF LERP interpolation) — per-ADC [adc][L=0,R=1]
        let vuCurrent = [[0,0],[0,0]], vuTargetArr = [[0,0],[0,0]];
        let peakCurrent = [[0,0],[0,0]], peakTargetArr = [[0,0],[0,0]];
        let vuDetected = false;
        let vuAnimFrameId = null;
        let VU_LERP = 0.3;

        // Canvas dimension cache — avoid GPU texture realloc every frame
        let canvasDims = {};
        function resizeCanvasIfNeeded(canvas) {
            const id = canvas.id;
            const rect = canvas.getBoundingClientRect();
            const dpr = window.devicePixelRatio;
            const tw = Math.round(rect.width * dpr);
            const th = Math.round(rect.height * dpr);
            if (tw === 0 || th === 0) return -1; // not laid out yet
            const cached = canvasDims[id];
            if (cached && cached.tw === tw && cached.th === th) return false;
            canvas.width = tw;
            canvas.height = th;
            canvasDims[id] = { tw, th, w: rect.width, h: rect.height };
            return true;
        }

        // Offscreen canvas background cache — static grids/labels
        let bgCache = {};
        function invalidateBgCache() { bgCache = {}; }

        // Spectrum color LUT — 256 entries, avoids template literal per-bar per-frame
        const spectrumColorLUT = new Array(256);
        for (let i = 0; i < 256; i++) {
            const val = i / 255;
            const r = 255;
            const g = Math.round(152 - val * 109);
            const b = Math.round(val * 54);
            spectrumColorLUT[i] = 'rgb(' + r + ',' + g + ',' + b + ')';
        }

        // DOM element cache for VU meters — avoid getElementById per rAF frame
        let vuDomRefs = null;
        function cacheVuDomRefs() {
            vuDomRefs = {};
            for (let a = 0; a < NUM_ADCS; a++) {
                vuDomRefs['fillL' + a] = document.getElementById('vuFill' + a + 'L');
                vuDomRefs['fillR' + a] = document.getElementById('vuFill' + a + 'R');
                vuDomRefs['pkL' + a] = document.getElementById('vuPeak' + a + 'L');
                vuDomRefs['pkR' + a] = document.getElementById('vuPeak' + a + 'R');
                vuDomRefs['dbL' + a] = document.getElementById('vuDb' + a + 'L');
                vuDomRefs['dbR' + a] = document.getElementById('vuDb' + a + 'R');
                vuDomRefs['dbSegL' + a] = document.getElementById('vuDbSeg' + a + 'L');
                vuDomRefs['dbSegR' + a] = document.getElementById('vuDbSeg' + a + 'R');
            }
            vuDomRefs['dot'] = document.getElementById('audioSignalDot');
            vuDomRefs['txt'] = document.getElementById('audioSignalText');
        }

        // Adaptive LERP — scale with update rate so convergence feels consistent
        function updateLerpFactors(rateMs) {
            LERP_SPEED = Math.min(0.25 * (50 / rateMs), 0.7);
            VU_LERP = Math.min(0.3 * (50 / rateMs), 0.7);
        }

        // Binary WS message handler (waveform + spectrum)
        function handleBinaryMessage(buf) {
            const dv = new DataView(buf);
            const type = dv.getUint8(0);
            const adc = dv.getUint8(1);
            if (type === 0x01 && currentActiveTab === 'audio') {
                // Waveform: [type:1][adc:1][samples:256]
                if (adc < NUM_ADCS && buf.byteLength >= 258) {
                    const samples = new Uint8Array(buf, 2, 256);
                    waveformTarget[adc] = samples;
                    if (!waveformCurrent[adc]) waveformCurrent[adc] = new Uint8Array(samples);
                    startAudioAnimation();
                }
            } else if (type === 0x02) {
                // Spectrum: [type:1][adc:1][freq:f32LE][bands:16xf32LE]
                if (adc < NUM_ADCS && buf.byteLength >= 70) {
                    const freq = dv.getFloat32(2, true);
                    for (let i = 0; i < 16; i++) spectrumTarget[adc][i] = dv.getFloat32(6 + i * 4, true);
                    targetDominantFreq[adc] = freq;
                    if (currentActiveTab === 'audio') startAudioAnimation();
                    // Feed RTA overlay for DSP tab
                    if (currentActiveTab === 'dsp' && adc === 0) {
                        peqRtaData = new Float32Array(16);
                        for (let i = 0; i < 16; i++) peqRtaData[i] = spectrumTarget[0][i];
                        if (peqGraphLayers.rta) dspDrawFreqResponse();
                    }
                }
            }
        }
        let vuSegmentedMode = localStorage.getItem('vuSegmented') === 'true';

        // LED bar mode
        let ledBarMode = localStorage.getItem('ledBarMode') === 'true';

        // Input focus state to prevent overwrites during user input
        let inputFocusState = {
            timerDuration: false,
            audioThreshold: false
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
            memoryPercent: [],
            psramPercent: []
        };
        let maxHistoryPoints = 300;

        // ===== Tab Switching =====
        function switchTab(tabId) {
            // Update mobile tab buttons
            document.querySelectorAll('.tab').forEach(tab => {
                tab.classList.toggle('active', tab.dataset.tab === tabId);
            });
            // Update sidebar items
            document.querySelectorAll('.sidebar-item').forEach(item => {
                item.classList.toggle('active', item.dataset.tab === tabId);
            });
            // Update panels
            document.querySelectorAll('.panel').forEach(panel => {
                panel.classList.toggle('active', panel.id === tabId);
            });

            // Start/stop time updates when switching to/from settings tab
            if (tabId === 'settings') {
                startTimeUpdates();
            } else {
                stopTimeUpdates();
            }

            // Load support content when switching to support tab
            if (tabId === 'support') {
                generateManualQRCode();
                loadManualContent();
            }

            // DSP tab: redraw frequency response, load routing, subscribe audio for RTA
            if (tabId === 'dsp') {
                canvasDims = {};
                setTimeout(dspDrawFreqResponse, 50);
                dspLoadRouting();
                if (typeof updatePeqCopyToDropdown === 'function') updatePeqCopyToDropdown();
                if (typeof updateChainCopyToDropdown === 'function') updateChainCopyToDropdown();
                if (peqGraphLayers.rta && ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                    ws.send(JSON.stringify({ type: 'setSpectrumEnabled', enabled: true }));
                }
            } else if (currentActiveTab === 'dsp' && peqGraphLayers.rta && tabId !== 'audio') {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
            }

            // Audio tab subscription management
            if (tabId === 'audio' && !audioSubscribed) {
                audioSubscribed = true;
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                }
                // Sync LED toggle
                const ledToggle = document.getElementById('ledModeToggle');
                if (ledToggle) ledToggle.checked = ledBarMode;
                // Cache VU DOM refs + invalidate canvas caches for fresh tab
                cacheVuDomRefs();
                canvasDims = {};
                invalidateBgCache();
                // Draw initial empty canvases for each ADC
                for (let a = 0; a < NUM_ADCS; a++) {
                    drawAudioWaveform(null, a);
                    drawSpectrumBars(null, 0, a);
                }
                // Update ADC2 panel visibility
                updateAdc2Visibility();
                // Load input names into fields
                loadInputNameFields();
            } else if (tabId !== 'audio' && audioSubscribed) {
                audioSubscribed = false;
                var dspRtaTakeover = (tabId === 'dsp' && peqGraphLayers.rta);
                if (!dspRtaTakeover && ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
                // Stop animation and reset state
                if (audioAnimFrameId) { cancelAnimationFrame(audioAnimFrameId); audioAnimFrameId = null; }
                if (vuAnimFrameId) { cancelAnimationFrame(vuAnimFrameId); vuAnimFrameId = null; }
                for (let a = 0; a < NUM_ADCS; a++) {
                    waveformCurrent[a] = null; waveformTarget[a] = null;
                    if (!dspRtaTakeover) { spectrumTarget[a].fill(0); }
                    spectrumCurrent[a].fill(0);
                    spectrumPeaks[a].fill(0); spectrumPeakTimes[a].fill(0);
                }
                vuDomRefs = null;
            }

            currentActiveTab = tabId;
        }

        // ===== Sidebar Toggle =====
        function toggleSidebar() {
            const sidebar = document.getElementById('sidebar');
            const body = document.body;
            sidebar.classList.toggle('collapsed');
            body.classList.toggle('sidebar-collapsed');
            // Save preference
            localStorage.setItem('sidebarCollapsed', sidebar.classList.contains('collapsed'));
        }

        let wifiConnectionPollTimer = null;


        function initSidebar() {
            const collapsed = localStorage.getItem('sidebarCollapsed') === 'true';
            if (collapsed) {
                document.getElementById('sidebar').classList.add('collapsed');
                document.body.classList.add('sidebar-collapsed');
            }
        }

        // ===== Status Bar Updates =====
        function updateStatusBar(wifiConnected, mqttConnected, ampState, wsConnected) {
            // WiFi status
            const wifiIndicator = document.getElementById('statusWifi');
            const wifiText = document.getElementById('statusWifiText');
            if (wifiConnected) {
                wifiIndicator.className = 'status-indicator online';
                wifiText.textContent = 'WiFi';
            } else {
                wifiIndicator.className = 'status-indicator offline';
                wifiText.textContent = 'WiFi';
            }

            // MQTT status
            const mqttIndicator = document.getElementById('statusMqtt');
            const mqttText = document.getElementById('statusMqttText');
            if (mqttConnected) {
                mqttIndicator.className = 'status-indicator online';
                mqttText.textContent = 'MQTT';
            } else if (mqttConnected === false) {
                mqttIndicator.className = 'status-indicator offline';
                mqttText.textContent = 'MQTT';
            } else {
                mqttIndicator.className = 'status-indicator';
                mqttText.textContent = 'MQTT';
            }

            // Amplifier status
            const ampIndicator = document.getElementById('statusAmp');
            const ampText = document.getElementById('statusAmpText');
            if (ampState) {
                ampIndicator.className = 'status-indicator online';
                ampText.textContent = 'Amp ON';
            } else {
                ampIndicator.className = 'status-indicator';
                ampText.textContent = 'Amp OFF';
            }

            // WebSocket status
            const wsIndicator = document.getElementById('statusWs');
            if (wsConnected) {
                wsIndicator.className = 'status-indicator online';
            } else {
                wsIndicator.className = 'status-indicator offline';
            }
        }

        // ===== WebSocket =====
        let wasDisconnectedDuringUpdate = false;
        let hadPreviousConnection = false;
        
        function getSessionIdFromCookie() {
            const name = "sessionId=";
            const decodedCookie = decodeURIComponent(document.cookie);
            const ca = decodedCookie.split(';');
            for(let i = 0; i < ca.length; i++) {
                let c = ca[i];
                while (c.charAt(0) == ' ') {
                    c = c.substring(1);
                }
                if (c.indexOf(name) === 0) {
                    return c.substring(name.length, c.length);
                }
            }
            return "";
        }

        // Global fetch wrapper for API calls (handles 401 Unauthorized)
        async function apiFetch(url, options = {}) {
            // Auto-include credentials and session header for all API calls
            const sessionId = getSessionIdFromCookie();
            const defaultOptions = {
                credentials: 'include',
                headers: {
                    'X-Session-ID': sessionId
                }
            };

            // Merge options with defaults, ensuring headers are properly combined
            const mergedOptions = {
                ...defaultOptions,
                ...options,
                headers: {
                    ...defaultOptions.headers,
                    ...(options.headers || {})
                }
            };

            try {
                const response = await fetch(url, mergedOptions);

                if (response.status === 401) {
                    console.warn(`Unauthorized (401) on ${url}. Redirecting...`);
                    // Try to parse JSON to see if there's a redirect provided
                    try {
                        const data = await response.clone().json();
                        if (data.redirect) {
                            window.location.href = data.redirect;
                        } else {
                            window.location.href = '/login';
                        }
                    } catch(e) {
                        window.location.href = '/login';
                    }
                    // Return a never-resolving promise to stop further .then() calls
                    return new Promise(() => {});
                }

                return response;
            } catch (error) {
                console.error(`API Fetch Error [${url}]:`, error);
                throw error;
            }
        }
        
        function initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.hostname;
            ws = new WebSocket(`${wsProtocol}//${wsHost}:81`);
            ws.binaryType = 'arraybuffer';

            ws.onopen = function() {
                console.log('WebSocket connected');

                // Send authentication immediately
                const sessionId = getSessionIdFromCookie();
                if (sessionId) {
                    ws.send(JSON.stringify({
                        type: 'auth',
                        sessionId: sessionId
                    }));
                } else {
                    console.error('No session ID for WebSocket auth');
                    window.location.href = '/login';
                }
            };

            ws.onmessage = function(event) {
                if (event.data instanceof ArrayBuffer) { handleBinaryMessage(event.data); return; }
                const data = JSON.parse(event.data);

                if (data.type === 'authRequired') {
                    // Server requesting authentication
                    const sessionId = getSessionIdFromCookie();
                    if (sessionId) {
                        ws.send(JSON.stringify({
                            type: 'auth',
                            sessionId: sessionId
                        }));
                    } else {
                        console.error('No session ID for WebSocket auth');
                        // Do not redirect automatically to avoid loops
                        showToast('Connection failed: No Session ID', 'error');
                    }
                }
                else if (data.type === 'authSuccess') {
                    console.log('WebSocket authenticated');
                    updateConnectionStatus(true);
                    wsReconnectDelay = WS_MIN_RECONNECT_DELAY;
                    fetchUpdateStatus();

                    // Draw initial graphs
                    drawCpuGraph();
                    drawMemoryGraph();
                    drawPsramGraph();

                    // Re-subscribe to audio stream if audio tab or DSP RTA is active
                    if (audioSubscribed && currentActiveTab === 'audio') {
                        ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                    } else if (currentActiveTab === 'dsp' && peqGraphLayers.rta) {
                        ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                    }

                    // Show reconnection notification if we were disconnected during an update
                    if (wasDisconnectedDuringUpdate) {
                        showToast('Device is back online after update!', 'success');
                        wasDisconnectedDuringUpdate = false;
                    } else if (hadPreviousConnection) {
                        // Show general reconnection notification
                        showToast('Device reconnected', 'success');
                    }
                    hadPreviousConnection = true;
                }
                else if (data.type === 'authFailed') {
                    console.error('WebSocket auth failed:', data.error);
                    ws.close();
                    showToast('Session invalid. Redirecting to login...', 'error');
                    setTimeout(() => {
                        window.location.href = '/login';
                    }, 2000);
                }
                else if (data.type === 'ledState') {
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
                } else if (data.type === 'factoryResetStatus') {
                    // Handle factory reset status messages
                    if (data.status === 'complete') {
                        showToast(data.message || 'Factory reset complete', 'success');
                    } else if (data.status === 'rebooting') {
                        showToast(data.message || 'Rebooting device...', 'info');
                    }
                } else if (data.type === 'rebootProgress') {
                    handlePhysicalRebootProgress(data);
                } else if (data.type === 'debugLog') {
                    appendDebugLog(data.timestamp, data.message, data.level);
                } else if (data.type === 'hardware_stats') {
                    updateHardwareStats(data);
                } else if (data.type === 'justUpdated') {
                    showUpdateSuccessNotification(data);
                } else if (data.type === 'displayState') {
                    if (typeof data.backlightOn !== 'undefined') {
                        backlightOn = !!data.backlightOn;
                        document.getElementById('backlightToggle').checked = backlightOn;
                    }
                    if (typeof data.screenTimeout !== 'undefined') {
                        screenTimeoutSec = data.screenTimeout;
                        document.getElementById('screenTimeoutSelect').value = screenTimeoutSec.toString();
                    }
                    if (typeof data.backlightBrightness !== 'undefined') {
                        backlightBrightness = data.backlightBrightness;
                        var pct = Math.round(backlightBrightness * 100 / 255);
                        var options = [10, 25, 50, 75, 100];
                        var closest = options.reduce(function(a, b) { return Math.abs(b - pct) < Math.abs(a - pct) ? b : a; });
                        document.getElementById('brightnessSelect').value = closest;
                    }
                    if (typeof data.dimEnabled !== 'undefined') {
                        dimEnabled = !!data.dimEnabled;
                        document.getElementById('dimToggle').checked = dimEnabled;
                        updateDimVisibility();
                    }
                    if (typeof data.dimTimeout !== 'undefined') {
                        dimTimeoutSec = data.dimTimeout;
                        document.getElementById('dimTimeoutSelect').value = dimTimeoutSec.toString();
                    }
                    if (typeof data.dimBrightness !== 'undefined') {
                        dimBrightnessPwm = data.dimBrightness;
                        document.getElementById('dimBrightnessSelect').value = dimBrightnessPwm.toString();
                    }
                } else if (data.type === 'buzzerState') {
                    if (typeof data.enabled !== 'undefined') {
                        document.getElementById('buzzerToggle').checked = !!data.enabled;
                        document.getElementById('buzzerFields').style.display = data.enabled ? '' : 'none';
                    }
                    if (typeof data.volume !== 'undefined') {
                        document.getElementById('buzzerVolumeSelect').value = data.volume.toString();
                    }
                } else if (data.type === 'mqttSettings') {
                    document.getElementById('appState.mqttEnabled').checked = data.enabled || false;
                    document.getElementById('mqttFields').style.display = (data.enabled || false) ? '' : 'none';
                    document.getElementById('appState.mqttBroker').value = data.broker || '';
                    document.getElementById('appState.mqttPort').value = data.port || 1883;
                    document.getElementById('appState.mqttUsername').value = data.username || '';
                    document.getElementById('appState.mqttBaseTopic').value = data.baseTopic || '';
                    document.getElementById('appState.mqttHADiscovery').checked = data.haDiscovery || false;
                    updateMqttConnectionStatus(data.connected, data.broker, data.port, data.baseTopic);
                } else if (data.type === 'audioLevels') {
                    if (currentActiveTab === 'audio') {
                        if (data.numAdcsDetected !== undefined) {
                            numAdcsDetected = data.numAdcsDetected;
                            updateAdc2Visibility();
                        }
                        // Per-ADC VU/peak data
                        if (data.adc && Array.isArray(data.adc)) {
                            for (let a = 0; a < data.adc.length && a < NUM_ADCS; a++) {
                                const ad = data.adc[a];
                                vuTargetArr[a][0] = ad.vu1 !== undefined ? ad.vu1 : 0;
                                vuTargetArr[a][1] = ad.vu2 !== undefined ? ad.vu2 : 0;
                                peakTargetArr[a][0] = ad.peak1 !== undefined ? ad.peak1 : 0;
                                peakTargetArr[a][1] = ad.peak2 !== undefined ? ad.peak2 : 0;
                            }
                        }
                        // Per-ADC status badges and readouts
                        if (data.adcStatus && Array.isArray(data.adcStatus)) {
                            for (let a = 0; a < data.adcStatus.length && a < NUM_ADCS; a++) {
                                updateAdcStatusBadge(a, data.adcStatus[a]);
                            }
                        }
                        if (data.adc && Array.isArray(data.adc)) {
                            for (let a = 0; a < data.adc.length && a < NUM_ADCS; a++) {
                                const ad = data.adc[a];
                                updateAdcReadout(a, ad.dBFS, ad.vrms1, ad.vrms2);
                            }
                        }
                        vuDetected = data.signalDetected !== undefined ? data.signalDetected : false;
                        startVuAnimation();
                    }
                } else if (data.type === 'audioWaveform') {
                    if (currentActiveTab === 'audio' && data.w) {
                        const a = data.adc || 0;
                        if (a < NUM_ADCS) {
                            waveformTarget[a] = data.w;
                            if (!waveformCurrent[a]) waveformCurrent[a] = data.w.slice();
                            startAudioAnimation();
                        }
                    }
                } else if (data.type === 'audioSpectrum') {
                    if (currentActiveTab === 'audio' && data.bands) {
                        const a = data.adc || 0;
                        if (a < NUM_ADCS) {
                            for (let i = 0; i < data.bands.length && i < 16; i++) spectrumTarget[a][i] = data.bands[i];
                            targetDominantFreq[a] = data.freq || 0;
                            startAudioAnimation();
                        }
                    }
                } else if (data.type === 'inputNames') {
                    if (data.names && Array.isArray(data.names)) {
                        for (let i = 0; i < data.names.length && i < NUM_ADCS * 2; i++) {
                            inputNames[i] = data.names[i];
                        }
                        applyInputNames();
                        loadInputNameFields();
                    }
                } else if (data.type === 'audioGraphState') {
                    var vuT = document.getElementById('vuMeterEnabledToggle');
                    var wfT = document.getElementById('waveformEnabledToggle');
                    var spT = document.getElementById('spectrumEnabledToggle');
                    if (vuT) vuT.checked = data.vuMeterEnabled;
                    if (wfT) wfT.checked = data.waveformEnabled;
                    if (spT) spT.checked = data.spectrumEnabled;
                    var fftSel = document.getElementById('fftWindowSelect');
                    if (fftSel && data.fftWindowType !== undefined) fftSel.value = data.fftWindowType;
                    toggleGraphDisabled('vuMeterContent', !data.vuMeterEnabled);
                    toggleGraphDisabled('waveformContent', !data.waveformEnabled);
                    toggleGraphDisabled('spectrumContent', !data.spectrumEnabled);
                } else if (data.type === 'debugState') {
                    applyDebugState(data);
                } else if (data.type === 'signalGenerator') {
                    applySigGenState(data);
                } else if (data.type === 'emergencyLimiterState') {
                    applyEmergencyLimiterState(data);
                } else if (data.type === 'audioQualityState') {
                    applyAudioQualityState(data);
                } else if (data.type === 'audioQualityDiag') {
                    applyAudioQualityDiag(data);
                } else if (data.type === 'adcState') {
                    if (Array.isArray(data.enabled)) {
                        for (var ai = 0; ai < data.enabled.length; ai++) {
                            var cb = document.getElementById('adcEnable' + ai);
                            if (cb) cb.checked = !!data.enabled[ai];
                        }
                    }
                } else if (data.type === 'usbAudioState') {
                    handleUsbAudioState(data);
                } else if (data.type === 'dacState') {
                    handleDacState(data);
                    if (data.eeprom) handleEepromDiag(data.eeprom);
                } else if (data.type === 'eepromProgramResult') {
                    showToast(data.success ? 'EEPROM programmed' : 'EEPROM program failed', data.success ? 'success' : 'error');
                } else if (data.type === 'eepromEraseResult') {
                    showToast(data.success ? 'EEPROM erased' : 'EEPROM erase failed', data.success ? 'success' : 'error');
                } else if (data.type === 'dspState') {
                    dspHandleState(data);
                } else if (data.type === 'dspMetrics') {
                    dspHandleMetrics(data);
                } else if (data.type === 'peqPresets') {
                    peqHandlePresetsList(data.presets);
                } else if (data.type === 'thdResult') {
                    thdUpdateResult(data);
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
        let currentWifiConnected = false;
        let currentWifiSSID = '';
        let currentMqttConnected = null;
        let currentAmpState = false;

        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('wsConnectionStatus');
            if (statusEl) {
                if (connected) {
                    statusEl.textContent = 'Connected';
                    statusEl.className = 'info-value text-success';
                } else {
                    statusEl.textContent = 'Disconnected';
                    statusEl.className = 'info-value text-error';
                }
            }
            // Update status bar
            updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, connected);
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

            // Store AP SSID for pre-filling the config modal
            if (data.apSSID) {
                currentAPSSID = data.apSSID;
            } else if (data.serialNumber) {
                // Fallback to serial number if appState.apSSID not provided
                currentAPSSID = data.serialNumber;
            }

            // Track current WiFi connection state and SSID
            currentWifiConnected = data.connected || false;
            currentWifiSSID = data.connected ? (data.ssid || '') : '';

            let html = '';

            // Client (STA) Status
            if (data.connected) {
                const ipType = data.usingStaticIP ? 'Static IP' : 'DHCP';
                html += `
                    <div class="info-row"><span class="info-label">Client Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Network</span><span class="info-value">${data.ssid || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Client IP</span><span class="info-value">${data.staIP || data.ip || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">IP Configuration</span><span class="info-value">${ipType}</span></div>
                    <div class="info-row"><span class="info-label">Signal</span><span class="info-value">${formatRssi(data.rssi)}</span></div>
                    <div class="info-row"><span class="info-label">Saved Networks</span><span class="info-value">${data.networkCount || 0}</span></div>
                `;
            } else {
                html += `
                    <div class="info-row"><span class="info-label">Client Status</span><span class="info-value text-error">Not Connected</span></div>
                `;
            }
            
            // AP Details separator if both are relevant
            let apContentAdded = false;
            if (data.mode === 'ap' || data.apEnabled) {
                if (html !== '') html += '<div class="divider"></div>';

                html += `
                    <div class="info-row"><span class="info-label">AP Mode</span><span class="info-value text-warning">Active</span></div>
                    <div class="info-row"><span class="info-label">AP SSID</span><span class="info-value">${data.apSSID || 'ALX-Device'}</span></div>
                    <div class="info-row"><span class="info-label">AP IP</span><span class="info-value">${data.apIP || data.ip || '192.168.4.1'}</span></div>
                `;

                if (data.apClients !== undefined) {
                    html += `<div class="info-row"><span class="info-label">Clients Connected</span><span class="info-value">${data.apClients}</span></div>`;
                }
                apContentAdded = true;
            }

            // Common details - only add divider if AP content was shown or if we have any content
            if (html !== '' && apContentAdded) {
                html += `<div class="divider"></div>`;
            }
            html += `<div class="info-row"><span class="info-label">MAC Address</span><span class="info-value">${data.mac || 'Unknown'}</span></div>`;

            apToggle.checked = data.apEnabled || (data.mode === 'ap');
            document.getElementById('apFields').style.display = apToggle.checked ? '' : 'none';
            statusBox.innerHTML = html;

            if (typeof data.autoUpdateEnabled !== 'undefined') {
                autoUpdateEnabled = !!data.autoUpdateEnabled;
                autoUpdateToggle.checked = autoUpdateEnabled;
            }

            if (typeof data.autoAPEnabled !== 'undefined') {
                document.getElementById('autoAPToggle').checked = !!data.autoAPEnabled;
            }
            
            if (typeof data.timezoneOffset !== 'undefined') {
                currentTimezoneOffset = data.timezoneOffset;
                document.getElementById('timezoneSelect').value = data.timezoneOffset.toString();
                updateTimezoneDisplay(data.timezoneOffset, data.dstOffset || 0);
            }

            if (typeof data.dstOffset !== 'undefined') {
                currentDstOffset = data.dstOffset;
                document.getElementById('dstToggle').checked = (data.dstOffset === 3600);
            }
            
            if (typeof data.darkMode !== 'undefined') {
                darkMode = !!data.darkMode;
                document.getElementById('darkModeToggle').checked = darkMode;
                applyTheme(darkMode);
            }

            if (typeof data.backlightOn !== 'undefined') {
                backlightOn = !!data.backlightOn;
                document.getElementById('backlightToggle').checked = backlightOn;
            }

            if (typeof data.screenTimeout !== 'undefined') {
                screenTimeoutSec = data.screenTimeout;
                document.getElementById('screenTimeoutSelect').value = screenTimeoutSec.toString();
            }

            if (typeof data.backlightBrightness !== 'undefined') {
                backlightBrightness = data.backlightBrightness;
                var pct = Math.round(backlightBrightness * 100 / 255);
                var options = [10, 25, 50, 75, 100];
                var closest = options.reduce(function(a, b) { return Math.abs(b - pct) < Math.abs(a - pct) ? b : a; });
                document.getElementById('brightnessSelect').value = closest;
            }

            if (typeof data.dimEnabled !== 'undefined') {
                dimEnabled = !!data.dimEnabled;
                document.getElementById('dimToggle').checked = dimEnabled;
                updateDimVisibility();
            }

            if (typeof data.dimTimeout !== 'undefined') {
                dimTimeoutSec = data.dimTimeout;
                document.getElementById('dimTimeoutSelect').value = dimTimeoutSec.toString();
            }

            if (typeof data.dimBrightness !== 'undefined') {
                dimBrightnessPwm = data.dimBrightness;
                document.getElementById('dimBrightnessSelect').value = dimBrightnessPwm.toString();
            }

            if (typeof data.bootAnimEnabled !== 'undefined') {
                if (!data.bootAnimEnabled) {
                    document.getElementById('bootAnimSelect').value = '-1';
                } else if (typeof data.bootAnimStyle !== 'undefined') {
                    document.getElementById('bootAnimSelect').value = data.bootAnimStyle.toString();
                }
            }

            if (typeof data.buzzerEnabled !== 'undefined') {
                document.getElementById('buzzerToggle').checked = !!data.buzzerEnabled;
                document.getElementById('buzzerFields').style.display = data.buzzerEnabled ? '' : 'none';
            }
            if (typeof data.buzzerVolume !== 'undefined') {
                document.getElementById('buzzerVolumeSelect').value = data.buzzerVolume.toString();
            }

            if (typeof data.enableCertValidation !== 'undefined') {
                enableCertValidation = !!data.enableCertValidation;
                document.getElementById('certValidationToggle').checked = enableCertValidation;
            }
            
            if (typeof data.hardwareStatsInterval !== 'undefined') {
                document.getElementById('statsIntervalSelect').value = data.hardwareStatsInterval.toString();
            }

            if (typeof data.audioUpdateRate !== 'undefined') {
                document.getElementById('audioUpdateRateSelect').value = data.audioUpdateRate.toString();
                updateLerpFactors(data.audioUpdateRate);
            }

            if (data.firmwareVersion) {
                currentFirmwareVersion = data.firmwareVersion;
                document.getElementById('currentVersion').textContent = data.firmwareVersion;
            }
            
            // Always show latest version row if we have any version info
            if (data.latestVersion) {
                currentLatestVersion = data.latestVersion;
                const latestVersionEl = document.getElementById('latestVersion');
                const latestVersionRow = document.getElementById('latestVersionRow');
                const latestVersionNotes = document.getElementById('latestVersionNotes');

                latestVersionRow.style.display = 'flex';

                // If up-to-date, show green "Up-To-Date" text and hide release notes link
                if (!data.updateAvailable && data.latestVersion !== 'Checking...' && data.latestVersion !== 'Unknown') {
                    latestVersionEl.textContent = 'Up-To-Date, no newer version available';
                    latestVersionEl.style.opacity = '1';
                    latestVersionEl.style.fontStyle = 'normal';
                    latestVersionEl.style.color = 'var(--success)';
                    latestVersionNotes.style.display = 'none';
                } else {
                    latestVersionEl.textContent = data.latestVersion;
                    latestVersionNotes.style.display = '';

                    // Style based on status
                    if (data.latestVersion === 'Checking...') {
                        latestVersionEl.style.opacity = '0.6';
                        latestVersionEl.style.fontStyle = 'italic';
                        latestVersionEl.style.color = '';
                    } else if (data.latestVersion === 'Unknown') {
                        latestVersionEl.style.opacity = '0.6';
                        latestVersionEl.style.fontStyle = 'italic';
                        latestVersionEl.style.color = 'var(--text-secondary)';
                    } else {
                        latestVersionEl.style.opacity = '1';
                        latestVersionEl.style.fontStyle = 'normal';
                        latestVersionEl.style.color = '';
                    }
                }

                if (data.updateAvailable) {
                    document.getElementById('updateBtn').classList.remove('hidden');
                } else {
                    document.getElementById('updateBtn').classList.add('hidden');
                }
            }
            
            // Update device serial number in Debug tab
            if (data.serialNumber) {
                document.getElementById('deviceSerial').textContent = data.serialNumber;
                // Also update sidebar version
                const sidebarVer = document.getElementById('sidebarVersion');
                if (sidebarVer && data.firmwareVersion) {
                    sidebarVer.textContent = 'v' + data.firmwareVersion;
                }
            }
            
            // Update MAC address in Debug tab
            if (data.mac) {
                document.getElementById('deviceMac').textContent = data.mac;
            }
            
            // Pre-fill WiFi SSID with currently connected network
            if (data.ssid && data.connected) {
                document.getElementById('appState.wifiSSID').value = data.ssid;
            }

            // Update global WiFi status for status bar
            currentWifiConnected = data.connected || data.mode === 'ap';
            updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);

            // Refresh saved networks list
            loadSavedNetworks();
        }

        // ===== Smart Sensing =====
        let smartAutoSettingsCollapsed = true;
        
        function updateSensingMode() {
            const selected = document.querySelector('input[name="sensingMode"]:checked');
            if (!selected) return;
            
            // Show/hide Smart Auto Settings card based on mode
            updateSmartAutoSettingsVisibility(selected.value);
            
            apiFetch('/api/smartsensing', {
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
        
        function updateSmartAutoSettingsVisibility(mode) {
            const settingsCard = document.getElementById('smartAutoSettingsCard');
            if (mode === 'smart_auto') {
                settingsCard.style.display = 'block';
            } else {
                settingsCard.style.display = 'none';
            }
        }
        
        function toggleSmartAutoSettings() {
            smartAutoSettingsCollapsed = !smartAutoSettingsCollapsed;
            const content = document.getElementById('smartAutoContent');
            const chevron = document.getElementById('smartAutoChevron');
            const header = chevron.parentElement;
            
            if (smartAutoSettingsCollapsed) {
                content.classList.remove('open');
                header.classList.remove('open');
            } else {
                content.classList.add('open');
                header.classList.add('open');
            }
        }

        function updateTimerDuration() {
            const value = parseInt(document.getElementById('appState.timerDuration').value);
            if (isNaN(value) || value < 1 || value > 60) return;
            
            apiFetch('/api/smartsensing', {
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

        function updateAudioThreshold() {
            const value = parseFloat(document.getElementById('audioThreshold').value);
            if (isNaN(value) || value < -96 || value > 0) return;

            apiFetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ audioThreshold: value })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Threshold updated', 'success');
            })
            .catch(err => showToast('Failed to update threshold', 'error'));
        }

        function manualOverride(state) {
            apiFetch('/api/smartsensing', {
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
                // Show/hide Smart Auto Settings based on mode
                updateSmartAutoSettingsVisibility(modeValue);
                const modeLabels = { 'always_on': 'Always On', 'always_off': 'Always Off', 'smart_auto': 'Smart Auto' };
                document.getElementById('infoSensingMode').textContent = modeLabels[modeValue] || modeValue;
            }
            
            // Update timer duration (only if not focused)
            if (data.timerDuration !== undefined && !inputFocusState.timerDuration) {
                document.getElementById('appState.timerDuration').value = data.timerDuration;
            }
            if (data.timerDuration !== undefined) {
                document.getElementById('infoTimerDuration').textContent = data.timerDuration + ' min';
            }
            
            // Update audio threshold (only if not focused)
            if (data.audioThreshold !== undefined && !inputFocusState.audioThreshold) {
                document.getElementById('audioThreshold').value = Math.round(data.audioThreshold);
            }
            if (data.audioThreshold !== undefined) {
                document.getElementById('infoAudioThreshold').textContent = Math.round(data.audioThreshold) + ' dBFS';
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
                // Update status bar
                currentAmpState = data.amplifierState;
                updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);
            }
            
            // Update audio info
            if (data.signalDetected !== undefined) {
                document.getElementById('signalDetected').textContent = data.signalDetected ? 'Yes' : 'No';
            }
            if (data.audioLevel !== undefined) {
                document.getElementById('audioLevel').textContent = data.audioLevel.toFixed(1) + ' dBFS';
            }
            if (data.audioVrms !== undefined) {
                document.getElementById('audioVrms').textContent = data.audioVrms.toFixed(3) + ' V';
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

            // Sync audio sample rate to Audio Settings card
            if (data.audioSampleRate !== undefined) {
                const sel = document.getElementById('audioSampleRateSelect');
                if (sel) sel.value = data.audioSampleRate.toString();
            }
        }

        // ===== Audio Tab Functions =====
        function updateAdc2Visibility() {
            var show = numAdcsDetected > 1;
            var ids = ['adcSection1', 'waveformPanel1', 'spectrumPanel1', 'inputNamesAdc1', 'siggenTargetAdcGroup'];
            for (var i = 0; i < ids.length; i++) {
                var el = document.getElementById(ids[i]);
                if (el) el.style.display = show ? '' : 'none';
            }
            var grids = document.querySelectorAll('#audio .dual-canvas-grid');
            for (var i = 0; i < grids.length; i++) {
                grids[i].style.gridTemplateColumns = show ? '1fr 1fr' : '1fr';
            }
        }

        function updateAdcStatusBadge(adcIdx, status) {
            var el = document.getElementById('adcStatusBadge' + adcIdx);
            if (!el) return;
            el.textContent = status || 'OK';
            el.className = 'adc-status-badge';
            var s = (status || 'OK').toUpperCase();
            if (s === 'OK') el.classList.add('ok');
            else if (s === 'NO_DATA') el.classList.add('no-data');
            else if (s === 'CLIPPING') el.classList.add('clipping');
            else if (s === 'NOISE_ONLY') el.classList.add('noise-only');
            else if (s === 'I2S_ERROR') el.classList.add('i2s-error');
            else if (s === 'HW_FAULT') el.classList.add('hw-fault');
            var clip = document.getElementById('clipIndicator' + adcIdx);
            if (clip) {
                if (s === 'CLIPPING' || s === 'HW_FAULT') clip.classList.add('active');
                else clip.classList.remove('active');
            }
        }

        function updateAdcReadout(adcIdx, dBFS, vrms1, vrms2) {
            var el = document.getElementById('adcReadout' + adcIdx);
            if (!el) return;
            var dbStr = (dBFS !== undefined && dBFS > -95) ? dBFS.toFixed(1) + ' dBFS' : '-inf dBFS';
            var vrms = 0;
            if (vrms1 !== undefined && vrms2 !== undefined) vrms = Math.max(vrms1, vrms2);
            else if (vrms1 !== undefined) vrms = vrms1;
            var vStr = vrms > 0.001 ? vrms.toFixed(3) + ' Vrms' : '-- Vrms';
            el.textContent = dbStr + ' | ' + vStr;
        }

        function applyInputNames() {
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('vuChName' + i);
                if (el) el.textContent = inputNames[i] || ('Ch ' + (i + 1));
                var segEl = document.getElementById('vuChNameSeg' + i);
                if (segEl) segEl.textContent = inputNames[i] || ('Ch ' + (i + 1));
            }
            if (typeof dspRenderChannelTabs === 'function') dspRenderChannelTabs();
            if (typeof dspRenderRouting === 'function') dspRenderRouting();
            if (typeof updatePeqCopyToDropdown === 'function') updatePeqCopyToDropdown();
            if (typeof updateChainCopyToDropdown === 'function') updateChainCopyToDropdown();
        }

        function updatePeqCopyToDropdown() {
            var sel = document.getElementById('peqCopyTo');
            if (!sel) return;
            // Preserve first option and rebuild channel options
            var html = '<option value="">Copy to...</option>';
            for (var i = 0; i < DSP_MAX_CH; i++) {
                var name = inputNames[i] || DSP_CH_NAMES[i];
                html += '<option value="' + i + '">' + name + '</option>';
            }
            html += '<option value="all">All Channels</option>';
            sel.innerHTML = html;
        }

        function loadInputNameFields() {
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('inputName' + i);
                if (el) el.value = inputNames[i] || '';
            }
        }

        function saveInputNames() {
            var names = [];
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('inputName' + i);
                names.push(el ? el.value : '');
            }
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputNames', names: names }));
                showToast('Input names saved', 'success');
            }
        }

        function toggleLedMode() {
            ledBarMode = document.getElementById('ledModeToggle').checked;
            localStorage.setItem('ledBarMode', ledBarMode.toString());
        }

        function formatFreq(f) {
            return f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 0 : 1) + 'k' : f.toFixed(0);
        }

        function drawRoundedBar(ctx, x, y, w, h, radius) {
            if (h < 1) return;
            const r = Math.min(radius, w / 2, h);
            ctx.beginPath();
            ctx.moveTo(x, y + h);
            ctx.lineTo(x, y + r);
            ctx.arcTo(x, y, x + r, y, r);
            ctx.arcTo(x + w, y, x + w, y + r, r);
            ctx.lineTo(x + w, y + h);
            ctx.closePath();
            ctx.fill();
        }

        function startAudioAnimation() {
            if (!audioAnimFrameId) audioAnimFrameId = requestAnimationFrame(audioAnimLoop);
        }

        function audioAnimLoop() {
            audioAnimFrameId = null;
            let needsMore = false;

            for (let a = 0; a < NUM_ADCS; a++) {
                // Lerp waveform
                if (waveformCurrent[a] && waveformTarget[a]) {
                    for (let i = 0; i < waveformCurrent[a].length && i < waveformTarget[a].length; i++) {
                        const diff = waveformTarget[a][i] - waveformCurrent[a][i];
                        if (Math.abs(diff) > 0.5) { waveformCurrent[a][i] += diff * LERP_SPEED; needsMore = true; }
                        else waveformCurrent[a][i] = waveformTarget[a][i];
                    }
                }

                // Lerp spectrum
                for (let i = 0; i < 16; i++) {
                    const diff = spectrumTarget[a][i] - spectrumCurrent[a][i];
                    if (Math.abs(diff) > 0.001) { spectrumCurrent[a][i] += diff * LERP_SPEED; needsMore = true; }
                    else spectrumCurrent[a][i] = spectrumTarget[a][i];
                }

                // Lerp dominant freq
                const fDiff = targetDominantFreq[a] - currentDominantFreq[a];
                if (Math.abs(fDiff) > 1) { currentDominantFreq[a] += fDiff * LERP_SPEED; needsMore = true; }
                else currentDominantFreq[a] = targetDominantFreq[a];

                drawAudioWaveform(waveformCurrent[a], a);
                drawSpectrumBars(spectrumCurrent[a], currentDominantFreq[a], a);
            }

            if (needsMore) audioAnimFrameId = requestAnimationFrame(audioAnimLoop);
        }

        function drawAudioWaveform(data, adcIndex) {
            adcIndex = adcIndex || 0;
            const canvas = document.getElementById('audioWaveformCanvas' + adcIndex);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const dpr = window.devicePixelRatio;
            const resized = resizeCanvasIfNeeded(canvas);
            if (resized === -1) return; // canvas not laid out yet (0x0)
            const dims = canvasDims[canvas.id];
            const w = dims.w, h = dims.h;

            const isNight = document.body.classList.contains('night-mode');
            const plotX = 36, plotY = 4;
            const plotW = w - 40, plotH = h - 22;

            // Offscreen background cache — grid, labels, axes drawn once
            const bgKey = 'wf' + adcIndex;
            if (resized || !bgCache[bgKey]) {
                const offscreen = document.createElement('canvas');
                offscreen.width = dims.tw;
                offscreen.height = dims.th;
                const bgCtx = offscreen.getContext('2d');
                bgCtx.scale(dpr, dpr);
                const bgColor = isNight ? '#1E1E1E' : '#F5F5F5';
                const gridColor = isNight ? '#333333' : '#D0D0D0';
                const labelColor = isNight ? '#999999' : '#757575';
                bgCtx.fillStyle = bgColor;
                bgCtx.fillRect(0, 0, w, h);
                bgCtx.font = '10px -apple-system, sans-serif';
                bgCtx.textAlign = 'right';
                bgCtx.textBaseline = 'middle';
                const yLabels = ['+1.0', '+0.5', '0', '-0.5', '-1.0'];
                const yValues = [1.0, 0.5, 0, -0.5, -1.0];
                for (let i = 0; i < yLabels.length; i++) {
                    const yPos = plotY + plotH * (1 - (yValues[i] + 1) / 2);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(yLabels[i], plotX - 4, yPos);
                    bgCtx.strokeStyle = gridColor;
                    bgCtx.lineWidth = 0.5;
                    bgCtx.beginPath();
                    bgCtx.moveTo(plotX, yPos);
                    bgCtx.lineTo(plotX + plotW, yPos);
                    bgCtx.stroke();
                }
                const sampleRate = 48000;
                const sel = document.getElementById('audioSampleRateSelect');
                const sr = sel ? parseInt(sel.value) || sampleRate : sampleRate;
                const numSamples = 256;
                const totalTimeMs = (numSamples / sr) * 1000;
                bgCtx.textAlign = 'center';
                bgCtx.textBaseline = 'top';
                const numXLabels = 5;
                for (let i = 0; i <= numXLabels; i++) {
                    const xFrac = i / numXLabels;
                    const xPos = plotX + xFrac * plotW;
                    const timeVal = (xFrac * totalTimeMs).toFixed(1);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(timeVal + 'ms', xPos, plotY + plotH + 4);
                    if (i > 0 && i < numXLabels) {
                        bgCtx.strokeStyle = gridColor;
                        bgCtx.lineWidth = 0.5;
                        bgCtx.beginPath();
                        bgCtx.moveTo(xPos, plotY);
                        bgCtx.lineTo(xPos, plotY + plotH);
                        bgCtx.stroke();
                    }
                }
                bgCache[bgKey] = offscreen;
            }

            // Blit cached background
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(bgCache[bgKey], 0, 0);
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

            if (!data || data.length === 0) return;

            const len = data.length;
            let scale = 1;
            if (waveformAutoScaleEnabled) {
                let maxDev = 0;
                for (let i = 0; i < len; i++) {
                    const dev = Math.abs(data[i] - 128);
                    if (dev > maxDev) maxDev = dev;
                }
                scale = maxDev > 2 ? (0.45 * 255) / maxDev : 1;
            }

            // Draw waveform — no shadow blur (saves ~2-3ms GPU convolution per frame)
            ctx.strokeStyle = '#FF9800';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            const step = plotW / (len - 1);
            for (let i = 0; i < len; i++) {
                const x = plotX + i * step;
                const centered = (data[i] - 128) * scale + 128;
                const y = plotY + (1 - centered / 255) * plotH;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function drawSpectrumBars(bands, freq, adcIndex) {
            adcIndex = adcIndex || 0;
            const canvas = document.getElementById('audioSpectrumCanvas' + adcIndex);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const dpr = window.devicePixelRatio;
            const resized = resizeCanvasIfNeeded(canvas);
            if (resized === -1) return; // canvas not laid out yet (0x0)
            const dims = canvasDims[canvas.id];
            const w = dims.w, h = dims.h;

            const isNight = document.body.classList.contains('night-mode');
            const plotX = 32, plotY = 4;
            const plotW = w - 36, plotH = h - 22;

            // Compute bar geometry (needed for both bg cache and bar drawing)
            const numBands = (bands && bands.length) ? Math.min(bands.length, 16) : 16;
            const gap = 2;
            const barWidth = (plotW - gap * (numBands - 1)) / numBands;
            const bandEdges = [0, 40, 80, 160, 315, 630, 1250, 2500, 5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000];

            // Offscreen background cache — grid, labels, axes drawn once
            const bgKey = 'sp' + adcIndex;
            if (resized || !bgCache[bgKey]) {
                const offscreen = document.createElement('canvas');
                offscreen.width = dims.tw;
                offscreen.height = dims.th;
                const bgCtx = offscreen.getContext('2d');
                bgCtx.scale(dpr, dpr);
                const bgColor = isNight ? '#1E1E1E' : '#F5F5F5';
                const gridColor = isNight ? '#333333' : '#D0D0D0';
                const labelColor = isNight ? '#999999' : '#757575';
                bgCtx.fillStyle = bgColor;
                bgCtx.fillRect(0, 0, w, h);
                bgCtx.font = '10px -apple-system, sans-serif';
                bgCtx.textAlign = 'right';
                bgCtx.textBaseline = 'middle';
                const dbLevels = [0, -12, -24, -36];
                for (let i = 0; i < dbLevels.length; i++) {
                    const db = dbLevels[i];
                    const linearVal = Math.pow(10, db / 20);
                    const yPos = plotY + plotH * (1 - linearVal);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(db + 'dB', plotX - 4, yPos);
                    bgCtx.strokeStyle = gridColor;
                    bgCtx.lineWidth = 0.5;
                    bgCtx.beginPath();
                    bgCtx.moveTo(plotX, yPos);
                    bgCtx.lineTo(plotX + plotW, yPos);
                    bgCtx.stroke();
                }
                bgCtx.textAlign = 'center';
                bgCtx.textBaseline = 'top';
                for (let i = 0; i < numBands; i += 2) {
                    const centerFreq = Math.sqrt(bandEdges[i] * bandEdges[i + 1]);
                    const xCenter = plotX + i * (barWidth + gap) + barWidth / 2;
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(formatFreq(centerFreq), xCenter, plotY + plotH + 4);
                }
                bgCache[bgKey] = offscreen;
            }

            // Blit cached background
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(bgCache[bgKey], 0, 0);
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

            // Update dominant frequency readout
            const freqEl = document.getElementById('dominantFreq' + adcIndex);
            if (freqEl) {
                freqEl.textContent = freq > 0 ? freq.toFixed(0) + ' Hz' : '-- Hz';
            }

            if (!bands || bands.length === 0) return;

            const now = performance.now();

            // Draw bars
            for (let i = 0; i < numBands; i++) {
                const val = Math.min(Math.max(bands[i], 0), 1);
                const barHeight = val * plotH;
                const x = plotX + i * (barWidth + gap);
                const y = plotY + plotH - barHeight;

                if (ledBarMode) {
                    // LED segmented bar mode
                    const segH = 4, segGap = 1.5;
                    const totalSegs = Math.floor(plotH / (segH + segGap));
                    const litSegs = Math.round(val * totalSegs);
                    for (let s = 0; s < totalSegs; s++) {
                        const segY = plotY + plotH - (s + 1) * (segH + segGap);
                        const frac = s / totalSegs;
                        let r, g, b;
                        if (frac < 0.6) { r = 76; g = 175; b = 80; }          // green
                        else if (frac < 0.8) { r = 255; g = 193; b = 7; }      // yellow
                        else { r = 244; g = 67; b = 54; }                        // red
                        if (s < litSegs) {
                            ctx.fillStyle = `rgb(${r},${g},${b})`;
                        } else {
                            ctx.fillStyle = `rgba(${r},${g},${b},0.05)`;
                        }
                        drawRoundedBar(ctx, x, segY, barWidth, segH, 1);
                    }
                } else {
                    // Standard smooth bars with rounded tops — use pre-computed color LUT
                    ctx.fillStyle = spectrumColorLUT[Math.round(val * 255)];
                    drawRoundedBar(ctx, x, y, barWidth, barHeight, 3);
                }

                // Peak hold indicator
                if (val > spectrumPeaks[adcIndex][i]) {
                    spectrumPeaks[adcIndex][i] = val;
                    spectrumPeakTimes[adcIndex][i] = now;
                }
                const elapsed = now - spectrumPeakTimes[adcIndex][i];
                if (elapsed > 1500) {
                    spectrumPeaks[adcIndex][i] -= 0.002 * (elapsed - 1500) / 16.67;
                    if (spectrumPeaks[adcIndex][i] < val) spectrumPeaks[adcIndex][i] = val;
                }
                if (spectrumPeaks[adcIndex][i] > 0.01) {
                    const peakY = plotY + plotH - spectrumPeaks[adcIndex][i] * plotH;
                    ctx.fillStyle = isNight ? 'rgba(255,255,255,0.85)' : 'rgba(0,0,0,0.6)';
                    ctx.fillRect(x, peakY, barWidth, 2);
                }
            }
        }

        function linearToDbPercent(val) {
            if (val <= 0) return 0;
            const db = 20 * Math.log10(val);
            const clamped = Math.max(db, -60);
            return ((clamped + 60) / 60) * 100;
        }

        function formatDbFS(val) {
            if (val <= 0) return '-inf dBFS';
            const db = 20 * Math.log10(val);
            return db.toFixed(1) + ' dBFS';
        }

        function startVuAnimation() {
            if (!vuAnimFrameId) vuAnimFrameId = requestAnimationFrame(vuAnimLoop);
        }

        function vuAnimLoop() {
            vuAnimFrameId = null;
            for (let a = 0; a < NUM_ADCS; a++) {
                for (let ch = 0; ch < 2; ch++) {
                    vuCurrent[a][ch] += (vuTargetArr[a][ch] - vuCurrent[a][ch]) * VU_LERP;
                    peakCurrent[a][ch] += (peakTargetArr[a][ch] - peakCurrent[a][ch]) * VU_LERP;
                }
                updateLevelMeters(a, vuCurrent[a][0], vuCurrent[a][1], peakCurrent[a][0], peakCurrent[a][1]);
            }
            // Update signal detection indicator — use cached refs if available
            const refs = vuDomRefs || {};
            const dot = refs['dot'] || document.getElementById('audioSignalDot');
            const txt = refs['txt'] || document.getElementById('audioSignalText');
            if (dot) dot.classList.toggle('active', vuDetected);
            if (txt) txt.textContent = vuDetected ? 'Detected' : 'Not detected';
            if (audioSubscribed) vuAnimFrameId = requestAnimationFrame(vuAnimLoop);
        }

        function drawPPM(canvasId, level, peak) {
            const canvas = document.getElementById(canvasId);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const w = canvas.offsetWidth;
            const h = canvas.offsetHeight;
            if (canvas.width !== w || canvas.height !== h) {
                canvas.width = w;
                canvas.height = h;
            }
            ctx.clearRect(0, 0, w, h);

            const segments = 48;
            const gap = 1;
            const segW = (w - (segments - 1) * gap) / segments;
            const pct = linearToDbPercent(level) / 100;
            const litCount = Math.round(pct * segments);
            const greenEnd = Math.round(segments * (40 / 60));
            const yellowEnd = Math.round(segments * (54 / 60));

            for (let i = 0; i < segments; i++) {
                const x = i * (segW + gap);
                if (i < litCount) {
                    if (i < greenEnd) ctx.fillStyle = '#4CAF50';
                    else if (i < yellowEnd) ctx.fillStyle = '#FFC107';
                    else ctx.fillStyle = '#F44336';
                } else {
                    ctx.fillStyle = 'rgba(255,255,255,0.06)';
                }
                ctx.fillRect(x, 0, segW, h);
            }

            // Peak marker
            const peakPct = linearToDbPercent(peak) / 100;
            const peakSeg = Math.min(Math.round(peakPct * segments), segments - 1);
            if (peak > 0) {
                const px = peakSeg * (segW + gap);
                ctx.fillStyle = '#FFFFFF';
                ctx.fillRect(px, 0, segW, h);
            }
        }

        function updateLevelMeters(adcIdx, vu1, vu2, peak1, peak2) {
            vu1 = Math.min(Math.max(vu1, 0), 1);
            vu2 = Math.min(Math.max(vu2, 0), 1);
            peak1 = Math.min(Math.max(peak1, 0), 1);
            peak2 = Math.min(Math.max(peak2, 0), 1);

            // Use cached DOM refs if available, else fallback to getElementById
            const refs = vuDomRefs || {};

            if (!vuSegmentedMode) {
                // Continuous dB-scaled bars
                const pct1 = linearToDbPercent(vu1);
                const pct2 = linearToDbPercent(vu2);
                const fillL = refs['fillL' + adcIdx] || document.getElementById('vuFill' + adcIdx + 'L');
                const fillR = refs['fillR' + adcIdx] || document.getElementById('vuFill' + adcIdx + 'R');
                if (fillL) fillL.style.width = pct1 + '%';
                if (fillR) fillR.style.width = pct2 + '%';

                const pkPct1 = linearToDbPercent(peak1);
                const pkPct2 = linearToDbPercent(peak2);
                const pkL = refs['pkL' + adcIdx] || document.getElementById('vuPeak' + adcIdx + 'L');
                const pkR = refs['pkR' + adcIdx] || document.getElementById('vuPeak' + adcIdx + 'R');
                if (pkL) pkL.style.left = pkPct1 + '%';
                if (pkR) pkR.style.left = pkPct2 + '%';

                const dbL = refs['dbL' + adcIdx] || document.getElementById('vuDb' + adcIdx + 'L');
                const dbR = refs['dbR' + adcIdx] || document.getElementById('vuDb' + adcIdx + 'R');
                if (dbL) dbL.textContent = formatDbFS(vu1);
                if (dbR) dbR.textContent = formatDbFS(vu2);
            } else {
                // Segmented PPM canvas
                drawPPM('ppmCanvas' + adcIdx + 'L', vu1, peak1);
                drawPPM('ppmCanvas' + adcIdx + 'R', vu2, peak2);

                const dbSegL = refs['dbSegL' + adcIdx] || document.getElementById('vuDbSeg' + adcIdx + 'L');
                const dbSegR = refs['dbSegR' + adcIdx] || document.getElementById('vuDbSeg' + adcIdx + 'R');
                if (dbSegL) dbSegL.textContent = formatDbFS(vu1);
                if (dbSegR) dbSegR.textContent = formatDbFS(vu2);
            }
        }

        function toggleVuMode(seg) {
            vuSegmentedMode = seg;
            localStorage.setItem('vuSegmented', seg);
            for (let a = 0; a < NUM_ADCS; a++) {
                var cont = document.getElementById('vuContinuous' + a);
                var segDiv = document.getElementById('vuSegmented' + a);
                if (cont) cont.style.display = seg ? 'none' : '';
                if (segDiv) segDiv.style.display = seg ? '' : 'none';
            }
        }

        function updateAudioSettings() {
            const sampleRate = parseInt(document.getElementById('audioSampleRateSelect').value);

            apiFetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ audioSampleRate: sampleRate })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Sample rate updated', 'success');
                else showToast('Failed to update sample rate', 'error');
            })
            .catch(err => showToast('Failed to update sample rate', 'error'));
        }

        // ===== Per-ADC Input Enable/Disable =====
        function setAdcEnabled(adc, en) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'setAdcEnabled',adc:adc,enabled:en}));
        }

        // ===== USB Audio Input =====
        function setUsbAudioEnabled(en) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'setUsbAudioEnabled',enabled:en}));
        }

        function handleUsbAudioState(d) {
            var enableCb = document.getElementById('usbAudioEnable');
            var fields = document.getElementById('usbAudioFields');
            if (enableCb) enableCb.checked = !!d.enabled;
            if (fields) fields.style.display = d.enabled ? '' : 'none';

            var badge = document.getElementById('usbAudioBadge');
            var statusEl = document.getElementById('usbAudioStatus');
            var formatEl = document.getElementById('usbAudioFormat');
            var volEl = document.getElementById('usbAudioVolume');
            var details = document.getElementById('usbAudioDetails');
            if (!d.enabled) {
                if (badge) { badge.textContent = 'Disabled'; badge.style.background = '#9E9E9E'; }
            } else if (d.streaming) {
                if (badge) { badge.textContent = 'Streaming'; badge.style.background = '#4CAF50'; }
                if (statusEl) statusEl.textContent = 'Streaming';
                if (details) details.style.display = '';
            } else if (d.connected) {
                if (badge) { badge.textContent = 'Connected'; badge.style.background = '#FF9800'; }
                if (statusEl) statusEl.textContent = 'Connected (idle)';
                if (details) details.style.display = '';
            } else {
                if (badge) { badge.textContent = 'Disconnected'; badge.style.background = '#9E9E9E'; }
                if (statusEl) statusEl.textContent = 'Disconnected';
                if (details) details.style.display = 'none';
            }
            if (formatEl) {
                if (d.connected) {
                    formatEl.textContent = (d.sampleRate/1000) + ' kHz / ' + d.bitDepth + '-bit ' + (d.channels === 1 ? 'mono' : 'stereo');
                } else {
                    formatEl.textContent = '\u2014';
                }
            }
            if (volEl) {
                if (d.connected) {
                    if (d.mute) {
                        volEl.textContent = 'Muted';
                    } else {
                        var dbVal = (d.volume / 256).toFixed(1);
                        var pct = Math.round(d.volumeLinear * 100);
                        volEl.textContent = dbVal + ' dB (' + pct + '%)';
                    }
                } else {
                    volEl.textContent = '\u2014';
                }
            }
            var ovr = document.getElementById('usbAudioOverruns');
            if (ovr) ovr.textContent = d.overruns || 0;
            var udr = document.getElementById('usbAudioUnderruns');
            if (udr) udr.textContent = d.underruns || 0;
        }

        // ===== DAC Output =====
        function updateDac() {
            var en = document.getElementById('dacEnable').checked;
            document.getElementById('dacFields').style.display = en ? '' : 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setDacEnabled', enabled: en }));
                ws.send(JSON.stringify({
                    type: 'setDacVolume',
                    volume: parseInt(document.getElementById('dacVolume').value)
                }));
                ws.send(JSON.stringify({
                    type: 'setDacMute',
                    mute: document.getElementById('dacMute').checked
                }));
            }
        }
        function updateDacFilter() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'setDacFilter',
                    filterMode: parseInt(document.getElementById('dacFilterMode').value)
                }));
            }
        }
        function changeDacDriver() {
            var id = parseInt(document.getElementById('dacDriverSelect').value);
            apiFetch('/api/dac', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ deviceId: id })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('DAC driver changed', 'success');
                else showToast(data.message || 'Failed', 'error');
            })
            .catch(() => showToast('Failed to change DAC driver', 'error'));
        }
        function handleDacState(d) {
            var enEl = document.getElementById('dacEnable');
            if (enEl) enEl.checked = d.enabled;
            var fields = document.getElementById('dacFields');
            if (fields) fields.style.display = d.enabled ? '' : 'none';
            var model = document.getElementById('dacModel');
            if (model) model.textContent = d.modelName || '—';
            var volSlider = document.getElementById('dacVolume');
            if (volSlider) { volSlider.value = d.volume; document.getElementById('dacVolVal').textContent = d.volume; }
            var muteEl = document.getElementById('dacMute');
            if (muteEl) muteEl.checked = d.mute;
            var badge = document.getElementById('dacReadyBadge');
            if (badge) {
                badge.style.display = d.enabled ? '' : 'none';
                badge.textContent = d.ready ? 'Ready' : 'Not Ready';
                badge.style.background = d.ready ? '#4CAF50' : '#F44336';
                badge.style.color = '#fff';
            }
            var und = document.getElementById('dacUnderruns');
            if (und) und.textContent = d.txUnderruns || 0;
            // Filter modes
            var fg = document.getElementById('dacFilterGroup');
            if (d.filterModes && d.filterModes.length > 0) {
                if (fg) fg.style.display = '';
                var sel = document.getElementById('dacFilterMode');
                if (sel) {
                    sel.innerHTML = '';
                    d.filterModes.forEach(function(name, i) {
                        var opt = document.createElement('option');
                        opt.value = i; opt.textContent = name;
                        sel.appendChild(opt);
                    });
                    sel.value = d.filterMode || 0;
                }
            } else if (fg) {
                fg.style.display = 'none';
            }
            // Driver select
            if (d.drivers) {
                var drvSel = document.getElementById('dacDriverSelect');
                if (drvSel) {
                    drvSel.innerHTML = '';
                    d.drivers.forEach(function(drv) {
                        var opt = document.createElement('option');
                        opt.value = drv.id; opt.textContent = drv.name;
                        drvSel.appendChild(opt);
                    });
                    drvSel.value = d.deviceId;
                }
            }
        }

        // ===== EEPROM Programming =====
        var _eepromPresetsLoaded = false;
        function eepromScan() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'eepromScan' }));
                showToast('Scanning I2C bus...', 'info');
            }
        }
        function eepromLoadPresets() {
            if (_eepromPresetsLoaded) return;
            apiFetch('/api/dac/eeprom/presets')
            .then(r => r.json())
            .then(d => {
                if (!d.success) return;
                var sel = document.getElementById('eepromPreset');
                if (!sel) return;
                d.presets.forEach(function(p) {
                    var opt = document.createElement('option');
                    opt.value = JSON.stringify(p);
                    opt.textContent = p.deviceName + ' (' + p.manufacturer + ')';
                    sel.appendChild(opt);
                });
                _eepromPresetsLoaded = true;
            }).catch(function(){});
        }
        function eepromFillPreset() {
            var sel = document.getElementById('eepromPreset');
            if (!sel || !sel.value) return;
            var p = JSON.parse(sel.value);
            document.getElementById('eepromDeviceId').value = '0x' + p.deviceId.toString(16).padStart(4,'0');
            document.getElementById('eepromDeviceName').value = p.deviceName || '';
            document.getElementById('eepromManufacturer').value = p.manufacturer || '';
            document.getElementById('eepromMaxCh').value = p.maxChannels || 2;
            document.getElementById('eepromDacAddr').value = p.dacI2cAddress ? '0x' + p.dacI2cAddress.toString(16).padStart(2,'0') : '0x00';
            document.getElementById('eepromFlagClock').checked = !!(p.flags & 1);
            document.getElementById('eepromFlagVol').checked = !!(p.flags & 2);
            document.getElementById('eepromFlagFilter').checked = !!(p.flags & 4);
            document.getElementById('eepromRates').value = (p.sampleRates || []).join(',');
        }
        function eepromProgram() {
            var rates = document.getElementById('eepromRates').value.split(',').map(Number).filter(function(n){return n>0;});
            var flags = {};
            flags.independentClock = document.getElementById('eepromFlagClock').checked;
            flags.hwVolume = document.getElementById('eepromFlagVol').checked;
            flags.filters = document.getElementById('eepromFlagFilter').checked;
            var payload = {
                address: parseInt(document.getElementById('eepromTargetAddr').value),
                deviceId: parseInt(document.getElementById('eepromDeviceId').value),
                deviceName: document.getElementById('eepromDeviceName').value,
                manufacturer: document.getElementById('eepromManufacturer').value,
                hwRevision: parseInt(document.getElementById('eepromHwRev').value) || 1,
                maxChannels: parseInt(document.getElementById('eepromMaxCh').value) || 2,
                dacI2cAddress: parseInt(document.getElementById('eepromDacAddr').value),
                flags: flags,
                sampleRates: rates
            };
            apiFetch('/api/dac/eeprom', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify(payload)
            })
            .then(r => r.json())
            .then(d => {
                if (d.success) showToast('EEPROM programmed successfully','success');
                else showToast(d.message || 'Program failed','error');
            })
            .catch(function(){ showToast('EEPROM program failed','error'); });
        }
        function eepromErase() {
            if (!confirm('Erase EEPROM? This will clear all stored DAC identification data.')) return;
            var addr = parseInt(document.getElementById('eepromTargetAddr').value);
            apiFetch('/api/dac/eeprom/erase', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({ address: addr })
            })
            .then(r => r.json())
            .then(d => {
                if (d.success) showToast('EEPROM erased','success');
                else showToast(d.message || 'Erase failed','error');
            })
            .catch(function(){ showToast('EEPROM erase failed','error'); });
        }
        function eepromLoadHex() {
            apiFetch('/api/dac/eeprom')
            .then(r => r.json())
            .then(d => {
                var el = document.getElementById('dbgEepromHex');
                if (!el) return;
                if (d.rawHex) {
                    var hex = d.rawHex;
                    var lines = [];
                    for (var i = 0; i < hex.length; i += 32) {
                        var addr = (i/2).toString(16).padStart(4,'0').toUpperCase();
                        var row = hex.substring(i, i+32).match(/.{2}/g).join(' ');
                        lines.push(addr + ': ' + row);
                    }
                    el.textContent = lines.join('\n');
                    el.style.display = '';
                } else {
                    el.textContent = 'No EEPROM data available';
                    el.style.display = '';
                }
            })
            .catch(function(){
                var el = document.getElementById('dbgEepromHex');
                if (el) { el.textContent = 'Failed to load'; el.style.display = ''; }
            });
        }
        function handleEepromDiag(eep) {
            if (!eep) return;
            // Determine EEPROM state: chip detected on I2C but no ALXD = "empty"
            var chipDetected = eep.scanned && eep.i2cMask > 0;
            var chipEmpty = chipDetected && !eep.found;
            // Find first EEPROM address from mask
            var chipAddr = 0;
            if (eep.i2cMask > 0) {
                for (var b = 0; b < 8; b++) { if (eep.i2cMask & (1 << b)) { chipAddr = 0x50 + b; break; } }
            }
            // Audio tab status
            var st = document.getElementById('eepromStatus');
            if (st) {
                if (eep.found) st.textContent = 'Programmed';
                else if (chipEmpty) st.textContent = 'Empty (blank)';
                else if (eep.scanned) st.textContent = 'No EEPROM detected';
                else st.textContent = 'Not scanned';
            }
            var addr = document.getElementById('eepromI2cAddr');
            if (addr) addr.textContent = (eep.found || chipDetected) ? '0x' + (eep.found ? eep.addr : chipAddr).toString(16).padStart(2,'0').toUpperCase() : '—';
            var cnt = document.getElementById('eepromI2cCount');
            if (cnt) cnt.textContent = eep.scanned ? eep.i2cDevices : '—';
            var badge = document.getElementById('eepromFoundBadge');
            if (badge) {
                badge.style.display = eep.scanned ? '' : 'none';
                if (eep.found) { badge.textContent = 'Programmed'; badge.style.background = '#4CAF50'; }
                else if (chipEmpty) { badge.textContent = 'Empty'; badge.style.background = '#FF9800'; }
                else { badge.textContent = 'Not Found'; badge.style.background = '#F44336'; }
                badge.style.color = '#fff';
            }
            // Debug tab
            var el;
            el = document.getElementById('dbgEepromFound');
            if (el) {
                if (eep.found) el.textContent = 'Yes @ 0x' + eep.addr.toString(16).padStart(2,'0').toUpperCase();
                else if (chipEmpty) el.textContent = 'Empty (blank) @ 0x' + chipAddr.toString(16).padStart(2,'0').toUpperCase();
                else if (eep.scanned) el.textContent = 'No';
                else el.textContent = '—';
            }
            el = document.getElementById('dbgEepromAddr');
            if (el) el.textContent = (eep.found || chipDetected) ? '0x' + (eep.found ? eep.addr : chipAddr).toString(16).padStart(2,'0').toUpperCase() : '—';
            el = document.getElementById('dbgI2cCount');
            if (el) el.textContent = eep.i2cDevices != null ? eep.i2cDevices : '—';
            el = document.getElementById('dbgEepromRdErr');
            if (el) el.textContent = eep.readErrors || 0;
            el = document.getElementById('dbgEepromWrErr');
            if (el) el.textContent = eep.writeErrors || 0;
            if (eep.found) {
                el = document.getElementById('dbgEepromDeviceId');
                if (el) el.textContent = '0x' + (eep.deviceId||0).toString(16).padStart(4,'0').toUpperCase();
                el = document.getElementById('dbgEepromName');
                if (el) el.textContent = eep.deviceName || '—';
                el = document.getElementById('dbgEepromMfr');
                if (el) el.textContent = eep.manufacturer || '—';
                el = document.getElementById('dbgEepromRev');
                if (el) el.textContent = eep.hwRevision != null ? eep.hwRevision : '—';
                el = document.getElementById('dbgEepromCh');
                if (el) el.textContent = eep.maxChannels || '—';
                el = document.getElementById('dbgEepromDacAddr');
                if (el) el.textContent = eep.dacI2cAddress ? '0x' + eep.dacI2cAddress.toString(16).padStart(2,'0') : 'None';
                var flagStrs = [];
                if (eep.flags & 1) flagStrs.push('IndepClk');
                if (eep.flags & 2) flagStrs.push('HW Vol');
                if (eep.flags & 4) flagStrs.push('Filters');
                el = document.getElementById('dbgEepromFlags');
                if (el) el.textContent = flagStrs.length ? flagStrs.join(', ') : 'None';
                el = document.getElementById('dbgEepromRates');
                if (el) el.textContent = (eep.sampleRates || []).join(', ') || '—';
            } else {
                ['dbgEepromDeviceId','dbgEepromName','dbgEepromMfr','dbgEepromRev','dbgEepromCh','dbgEepromDacAddr','dbgEepromFlags','dbgEepromRates'].forEach(function(id){
                    el = document.getElementById(id);
                    if (el) el.textContent = '—';
                });
            }
            // Lazy-load presets on first eeprom data
            eepromLoadPresets();
        }

        // ===== Signal Generator =====
        // ===== Emergency Limiter =====
        function updateEmergencyLimiter(field) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            if (field === 'enabled') {
                ws.send(JSON.stringify({
                    type: 'setEmergencyLimiterEnabled',
                    enabled: document.getElementById('emergencyLimiterEnable').checked
                }));
            } else if (field === 'threshold') {
                ws.send(JSON.stringify({
                    type: 'setEmergencyLimiterThreshold',
                    threshold: parseFloat(document.getElementById('emergencyLimiterThreshold').value)
                }));
            }
        }
        function applyEmergencyLimiterState(d) {
            document.getElementById('emergencyLimiterEnable').checked = d.enabled;
            document.getElementById('emergencyLimiterThreshold').value = d.threshold;
            document.getElementById('emergencyLimiterThresholdVal').textContent = parseFloat(d.threshold).toFixed(1);

            // Update status badge
            var statusBadge = document.getElementById('emergencyLimiterStatusBadge');
            var statusText = document.getElementById('emergencyLimiterStatus');
            if (d.active) {
                statusBadge.textContent = 'ACTIVE';
                statusBadge.style.background = '#F44336';
                statusText.textContent = 'Limiting';
                statusText.style.color = '#F44336';
            } else {
                statusBadge.textContent = 'Idle';
                statusBadge.style.background = '#4CAF50';
                statusText.textContent = 'Idle';
                statusText.style.color = '';
            }

            // Update metrics
            document.getElementById('emergencyLimiterGR').textContent = parseFloat(d.gainReductionDb).toFixed(1) + ' dB';
            document.getElementById('emergencyLimiterTriggers').textContent = d.triggerCount || 0;
        }

        function updateAudioQuality(field) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            if (field === 'enabled') {
                ws.send(JSON.stringify({
                    type: 'setAudioQualityEnabled',
                    enabled: document.getElementById('audioQualityEnabled').checked
                }));
            } else if (field === 'threshold') {
                ws.send(JSON.stringify({
                    type: 'setAudioQualityThreshold',
                    threshold: parseFloat(document.getElementById('audioQualityThreshold').value)
                }));
            }
        }

        function applyAudioQualityState(d) {
            document.getElementById('audioQualityEnabled').checked = d.enabled;
            document.getElementById('audioQualityThreshold').value = d.threshold;
        }

        function applyAudioQualityDiag(d) {
            document.getElementById('aqGlitchesTotal').textContent = d.glitchesTotal || 0;
            document.getElementById('aqGlitchesMinute').textContent = d.glitchesLastMinute || 0;
            document.getElementById('aqLatencyAvg').textContent = d.timingAvgMs ? parseFloat(d.timingAvgMs).toFixed(2) + ' ms' : '--';
            document.getElementById('aqLastGlitchType').textContent = d.lastGlitchTypeStr || '--';

            // Correlation badges
            var dspBadge = document.getElementById('aqCorrelationDsp');
            var wifiBadge = document.getElementById('aqCorrelationWifi');

            if (d.correlationDspSwap) {
                dspBadge.textContent = 'YES';
                dspBadge.style.background = '#F44336';
            } else {
                dspBadge.textContent = 'No';
                dspBadge.style.background = '#4CAF50';
            }

            if (d.correlationWifi) {
                wifiBadge.textContent = 'YES';
                wifiBadge.style.background = '#F44336';
            } else {
                wifiBadge.textContent = 'No';
                wifiBadge.style.background = '#4CAF50';
            }
        }

        function resetAudioQualityStats() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            ws.send(JSON.stringify({ type: 'resetAudioQualityStats' }));
        }

        function updateSigGen() {
            document.getElementById('siggenFields').style.display = document.getElementById('siggenEnable').checked ? '' : 'none';
            var wf = parseInt(document.getElementById('siggenWaveform').value);
            document.getElementById('siggenSweepGroup').style.display = wf === 3 ? '' : 'none';
            var mode = parseInt(document.getElementById('siggenOutputMode').value);
            document.getElementById('siggenPwmNote').style.display = mode === 1 ? '' : 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'setSignalGen',
                    enabled: document.getElementById('siggenEnable').checked,
                    waveform: wf,
                    frequency: parseFloat(document.getElementById('siggenFreq').value),
                    amplitude: parseFloat(document.getElementById('siggenAmp').value),
                    channel: parseInt(document.getElementById('siggenChannel').value),
                    outputMode: mode,
                    sweepSpeed: parseFloat(document.getElementById('siggenSweepSpeed').value),
                    targetAdc: parseInt(document.getElementById('siggenTargetAdc').value)
                }));
            }
        }
        function siggenPreset(wf, freq, amp) {
            document.getElementById('siggenWaveform').value = wf;
            document.getElementById('siggenFreq').value = freq;
            document.getElementById('siggenFreqVal').textContent = freq;
            document.getElementById('siggenAmp').value = amp;
            document.getElementById('siggenAmpVal').textContent = amp;
            if (wf === 3) document.getElementById('siggenSweepSpeed').value = 1000;
            document.getElementById('siggenEnable').checked = true;
            updateSigGen();
        }
        function applySigGenState(d) {
            document.getElementById('siggenEnable').checked = d.enabled;
            document.getElementById('siggenFields').style.display = d.enabled ? '' : 'none';
            document.getElementById('siggenWaveform').value = d.waveform;
            document.getElementById('siggenFreq').value = d.frequency;
            document.getElementById('siggenFreqVal').textContent = Math.round(d.frequency);
            document.getElementById('siggenAmp').value = d.amplitude;
            document.getElementById('siggenAmpVal').textContent = Math.round(d.amplitude);
            document.getElementById('siggenChannel').value = d.channel;
            document.getElementById('siggenOutputMode').value = d.outputMode;
            document.getElementById('siggenSweepSpeed').value = d.sweepSpeed;
            if (d.targetAdc !== undefined) document.getElementById('siggenTargetAdc').value = d.targetAdc;
            document.getElementById('siggenSweepGroup').style.display = d.waveform === 3 ? '' : 'none';
            document.getElementById('siggenPwmNote').style.display = d.outputMode === 1 ? '' : 'none';
        }

        // ===== WiFi Configuration =====
        let wifiScanInProgress = false;
        
        

        function submitWiFiConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('appState.wifiSSID').value;
            const password = document.getElementById('appState.wifiPassword').value;
            const useStaticIP = document.getElementById('useStaticIP').checked;

            // Build request body
            const requestBody = { ssid, password, useStaticIP };

            // Add static IP configuration if enabled
            if (useStaticIP) {
                requestBody.staticIP = document.getElementById('staticIP').value;
                requestBody.subnet = document.getElementById('subnet').value;
                requestBody.gateway = document.getElementById('gateway').value;
                requestBody.dns1 = document.getElementById('dns1').value;
                requestBody.dns2 = document.getElementById('dns2').value;

                // Validate IP addresses
                if (!isValidIP(requestBody.staticIP)) {
                    showToast('Invalid IPv4 address', 'error');
                    return;
                }
                if (!isValidIP(requestBody.subnet)) {
                    showToast('Invalid network mask', 'error');
                    return;
                }
                if (!isValidIP(requestBody.gateway)) {
                    showToast('Invalid gateway address', 'error');
                    return;
                }
                if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
                    showToast('Invalid primary DNS address', 'error');
                    return;
                }
                if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
                    showToast('Invalid secondary DNS address', 'error');
                    return;
                }
            }

            showWiFiModal(ssid);

            apiFetch('/api/wificonfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(requestBody)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    // Start polling for connection status
                    if (wifiConnectionPollTimer) clearInterval(wifiConnectionPollTimer);
                    wifiConnectionPollTimer = setInterval(pollWiFiConnection, 2000);
                } else {
                    updateWiFiConnectionStatus('error', data.message || 'Failed to initiate connection');
                }
            })
            .catch(err => updateWiFiConnectionStatus('error', 'Network error: ' + err.message));
        }

        function saveNetworkSettings(event) {
            event.preventDefault();
            const ssid = document.getElementById('appState.wifiSSID').value;
            const password = document.getElementById('appState.wifiPassword').value;
            const useStaticIP = document.getElementById('useStaticIP').checked;

            if (!ssid) {
                showToast('Please enter network SSID', 'error');
                return;
            }

            if (!password) {
                showToast('Please enter network password', 'error');
                return;
            }

            // Build request body
            const requestBody = { ssid, password, useStaticIP };

            // Add static IP configuration if enabled
            if (useStaticIP) {
                requestBody.staticIP = document.getElementById('staticIP').value;
                requestBody.subnet = document.getElementById('subnet').value;
                requestBody.gateway = document.getElementById('gateway').value;
                requestBody.dns1 = document.getElementById('dns1').value;
                requestBody.dns2 = document.getElementById('dns2').value;

                // Validate IP addresses
                if (!isValidIP(requestBody.staticIP)) {
                    showToast('Invalid IPv4 address', 'error');
                    return;
                }
                if (!isValidIP(requestBody.subnet)) {
                    showToast('Invalid network mask', 'error');
                    return;
                }
                if (!isValidIP(requestBody.gateway)) {
                    showToast('Invalid gateway address', 'error');
                    return;
                }
                if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
                    showToast('Invalid primary DNS address', 'error');
                    return;
                }
                if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
                    showToast('Invalid secondary DNS address', 'error');
                    return;
                }
            }

            // Call save-only endpoint
            apiFetch('/api/wifisave', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(requestBody)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Network saved successfully', 'success');

                    // Clear the form
                    document.getElementById('appState.wifiSSID').value = '';
                    document.getElementById('appState.wifiPassword').value = '';
                    document.getElementById('useStaticIP').checked = false;
                    toggleStaticIPFields(); // Hide static IP fields

                    // Clear static IP fields
                    document.getElementById('staticIP').value = '';
                    document.getElementById('subnet').value = '255.255.255.0';
                    document.getElementById('gateway').value = '';
                    document.getElementById('dns1').value = '';
                    document.getElementById('dns2').value = '';

                    // Reload saved networks list
                    loadSavedNetworks();
                } else {
                    showToast(data.message || 'Failed to save network', 'error');
                }
            })
            .catch(err => showToast('Network error: ' + err.message, 'error'));
        }

        function showWiFiModal(ssid) {
            // Remove existing modal if any
            const existing = document.getElementById('wifiConnectionModal');
            if (existing) existing.remove();

            const modal = document.createElement('div');
            modal.id = 'wifiConnectionModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title">Connecting to WiFi</div>
                    <div class="info-box">
                        <div style="text-align: center; padding: 20px;">
                            <div id="wifiLoader" class="animate-pulse" style="font-size: 40px; margin-bottom: 16px;">📶</div>
                            <div id="wifiStatusText">Connecting to <strong>${ssid}</strong>...</div>
                            <div id="wifiIPInfo" class="hidden" style="margin-top: 16px; font-family: monospace; font-size: 18px; color: var(--success);"></div>
                        </div>
                    </div>
                    <div id="wifiModalActions" class="modal-actions" style="margin-top: 16px;">
                        <button type="button" class="btn btn-secondary" onclick="closeWiFiModal()">Cancel</button>
                    </div>
                </div>
            `;
            document.body.appendChild(modal);
        }

        function updateWiFiConnectionStatus(type, message, ip) {
            const statusText = document.getElementById('wifiStatusText');
            const loader = document.getElementById('wifiLoader');
            const ipInfo = document.getElementById('wifiIPInfo');
            const actions = document.getElementById('wifiModalActions');
            
            if (!statusText) return; // Modal might be closed
            
            statusText.innerHTML = message;
            
            if (type === 'success') {
                loader.textContent = '✅';
                loader.classList.remove('animate-pulse');
                
                if (ip) {
                    ipInfo.textContent = 'IP: ' + ip;
                    ipInfo.classList.remove('hidden');
                    
                    actions.innerHTML = `
                        <button class="btn btn-success" onclick="window.location.href='http://${ip}'">Go to Dashboard</button>
                    `;
                } else {
                     actions.innerHTML = `<button class="btn btn-secondary" onclick="closeWiFiModal()">Close</button>`;
                }
            } else if (type === 'error') {
                loader.textContent = '❌';
                loader.classList.remove('animate-pulse');
                actions.innerHTML = `<button class="btn btn-secondary" onclick="closeWiFiModal()">Close</button>`;
            }
        }


        function closeWiFiModal() {
            const modal = document.getElementById('wifiConnectionModal');
            if (modal) modal.remove();
            if (wifiConnectionPollTimer) {
                clearInterval(wifiConnectionPollTimer);
                wifiConnectionPollTimer = null;
            }
        }

        // Track connection attempts for network change detection
        let connectionPollAttempts = 0;
        let lastKnownNewIP = '';

        function pollWiFiConnection() {
            connectionPollAttempts++;

            apiFetch('/api/wifistatus')
                .then(res => res.json())
                .then(data => {
                    // Reset attempts on successful response
                    connectionPollAttempts = 0;

                    if (data.wifiConnecting) {
                        // Still connecting, keep polling
                        return;
                    }

                    // Stop polling
                    if (wifiConnectionPollTimer) {
                        clearInterval(wifiConnectionPollTimer);
                        wifiConnectionPollTimer = null;
                    }

                    if (data.wifiConnectSuccess) {
                        lastKnownNewIP = data.wifiNewIP || data.staIP || '';
                        updateWiFiConnectionStatus('success', 'Connected successfully!', lastKnownNewIP);
                    } else {
                        const errorMsg = data.wifiConnectError || 'Failed to connect. Check credentials.';
                        updateWiFiConnectionStatus('error', errorMsg);
                    }
                })
                .catch(err => {
                    console.log('Poll attempt ' + connectionPollAttempts + ' failed:', err.message);

                    // If we've had multiple failed fetch attempts, the device likely changed networks
                    if (connectionPollAttempts >= 3) {
                        if (wifiConnectionPollTimer) {
                            clearInterval(wifiConnectionPollTimer);
                            wifiConnectionPollTimer = null;
                        }

                        // Check if we're on AP mode IP - device may have connected to WiFi
                        const currentHost = window.location.hostname;
                        if (currentHost === '192.168.4.1' || currentHost.startsWith('192.168.4.')) {
                            // We were on AP mode, device likely connected to new network
                            updateWiFiConnectionStatus('success',
                                'Connection successful! The device has connected to WiFi and is no longer reachable at this address. Please connect to your WiFi network and access the device at its new IP address.',
                                '');
                        } else {
                            // Generic network error
                            updateWiFiConnectionStatus('error', 'Network error: Lost connection to device. The device may have changed networks.');
                        }
                        connectionPollAttempts = 0;
                    }
                    // Otherwise keep trying (device might be temporarily unreachable during network switch)
                });
        }

        function showAPModeModal(apIP) {
            const modal = document.createElement('div');
            modal.id = 'apModeModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title">AP Mode Activated</div>
                    <div class="info-box">
                        <div style="text-align: center; padding: 20px;">
                            <div style="font-size: 40px; margin-bottom: 16px;">📶</div>
                            <div style="margin-bottom: 8px;">No saved networks available.</div>
                            <div style="margin-bottom: 16px;">Access Point mode has been started.</div>
                            <div style="margin-top: 16px; font-family: monospace; font-size: 18px; color: var(--accent);">${apIP}</div>
                        </div>
                    </div>
                    <div class="modal-actions" style="margin-top: 16px;">
                        <button class="primary" onclick="window.location.href='http://${apIP}'">Go to Dashboard</button>
                    </div>
                </div>
            `;
            document.body.appendChild(modal);
        }

        function closeAPModeModal() {
            const modal = document.getElementById('apModeModal');
            if (modal) modal.remove();
        }

        // Toggle static IP fields visibility
        function toggleStaticIPFields() {
            const useStaticIP = document.getElementById('useStaticIP').checked;
            const fields = document.getElementById('staticIPFields');
            fields.style.display = useStaticIP ? 'block' : 'none';
        }

        // Auto-populate gateway and DNS based on static IP
        function updateStaticIPDefaults() {
            const staticIP = document.getElementById('staticIP').value;
            const gatewayField = document.getElementById('gateway');
            const dns1Field = document.getElementById('dns1');

            if (!staticIP || !isValidIP(staticIP)) return;

            // Extract network portion and suggest gateway (.1)
            const parts = staticIP.split('.');
            if (parts.length === 4) {
                const suggestedGateway = `${parts[0]}.${parts[1]}.${parts[2]}.1`;

                // Only auto-fill if fields are empty
                if (!gatewayField.value) {
                    gatewayField.value = suggestedGateway;
                }
                if (!dns1Field.value) {
                    dns1Field.value = suggestedGateway;
                }
            }
        }

        // Validate IP address format
        function isValidIP(ip) {
            if (!ip) return false;
            const parts = ip.split('.');
            if (parts.length !== 4) return false;
            return parts.every(part => {
                const num = parseInt(part, 10);
                return num >= 0 && num <= 255 && part === num.toString();
            });
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
            apiFetch('/api/wifiscan')
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
            const ssidInput = document.getElementById('appState.wifiSSID');
            const passwordInput = document.getElementById('appState.wifiPassword');
            const useStaticIPCheckbox = document.getElementById('useStaticIP');
            const staticIPFields = document.getElementById('staticIPFields');

            if (select.value) {
                ssidInput.value = select.value;

                // Check if this is a saved network
                const savedNetwork = savedNetworksData.find(net => net.ssid === select.value);
                if (savedNetwork) {
                    // Populate password field with placeholder
                    passwordInput.value = '';
                    passwordInput.placeholder = '••••••••';

                    // Populate Static IP fields if configured
                    if (savedNetwork.useStaticIP) {
                        useStaticIPCheckbox.checked = true;
                        staticIPFields.style.display = 'block';
                        document.getElementById('staticIP').value = savedNetwork.staticIP || '';
                        document.getElementById('subnet').value = savedNetwork.subnet || '255.255.255.0';
                        document.getElementById('gateway').value = savedNetwork.gateway || '';
                        document.getElementById('dns1').value = savedNetwork.dns1 || '';
                        document.getElementById('dns2').value = savedNetwork.dns2 || '';
                    } else {
                        useStaticIPCheckbox.checked = false;
                        staticIPFields.style.display = 'none';
                        // Clear Static IP fields
                        document.getElementById('staticIP').value = '';
                        document.getElementById('subnet').value = '255.255.255.0';
                        document.getElementById('gateway').value = '';
                        document.getElementById('dns1').value = '';
                        document.getElementById('dns2').value = '';
                    }
                } else {
                    // Not a saved network - clear password and Static IP fields
                    passwordInput.value = '';
                    passwordInput.placeholder = 'Enter password';
                    useStaticIPCheckbox.checked = false;
                    staticIPFields.style.display = 'none';
                    document.getElementById('staticIP').value = '';
                    document.getElementById('subnet').value = '255.255.255.0';
                    document.getElementById('gateway').value = '';
                    document.getElementById('dns1').value = '';
                    document.getElementById('dns2').value = '';
                }
            }
        }

        // Store saved networks data globally for config management
        let savedNetworksData = [];

        // Load and display saved networks
        function loadSavedNetworks() {
            const configSelect = document.getElementById('configNetworkSelect');

            apiFetch('/api/wifilist')
            .then(res => res.json())
            .then(data => {
                if (data.success && data.networks) { // Check success flag specifically
                    // Store networks data globally
                    savedNetworksData = data.networks;

                    if (data.networks.length > 0) {
                        // Populate config network select dropdown
                        configSelect.innerHTML = '<option value="">-- Select a saved network --</option>';
                        data.networks.forEach(net => {
                            const option = document.createElement('option');
                            option.value = net.index;
                            option.textContent = net.ssid + (net.useStaticIP ? ' (Static IP)' : '');
                            configSelect.appendChild(option);
                        });
                    } else {
                        configSelect.innerHTML = '<option value="">-- No saved networks --</option>';
                    }
                } else {
                    // Show error from API if available
                    const errorMsg = data.error || 'Unknown error';
                    console.error('API Error loading networks:', errorMsg);
                }
            })
            .catch(err => {
                console.error('Failed to load saved networks:', err);
            });
        }

        // Remove network by index
        // Load network configuration for selected network
        // Store original network config to detect changes
        let originalNetworkConfig = {
            useStaticIP: false,
            staticIP: '',
            subnet: '',
            gateway: '',
            dns1: '',
            dns2: ''
        };

        function loadNetworkConfig() {
            const select = document.getElementById('configNetworkSelect');
            const fields = document.getElementById('networkConfigFields');
            const staticIPFields = document.getElementById('configStaticIPFields');
            const useStaticIP = document.getElementById('configUseStaticIP');
            const passwordField = document.getElementById('configPassword');

            const selectedIndex = parseInt(select.value);
            if (isNaN(selectedIndex)) {
                fields.style.display = 'none';
                return;
            }

            // Find the network data
            const network = savedNetworksData.find(net => net.index === selectedIndex);
            if (!network) {
                fields.style.display = 'none';
                return;
            }

            // Show fields and populate data
            fields.style.display = 'block';

            // Clear password field (we don't store passwords on frontend for security)
            passwordField.value = '';
            passwordField.placeholder = 'Enter password (leave empty to keep current)';

            useStaticIP.checked = network.useStaticIP || false;

            // Store original values for change detection
            originalNetworkConfig = {
                useStaticIP: network.useStaticIP || false,
                staticIP: network.staticIP || '',
                subnet: network.subnet || '255.255.255.0',
                gateway: network.gateway || '',
                dns1: network.dns1 || '',
                dns2: network.dns2 || ''
            };

            if (network.useStaticIP) {
                staticIPFields.style.display = 'block';
                document.getElementById('configStaticIP').value = network.staticIP || '';
                document.getElementById('configSubnet').value = network.subnet || '255.255.255.0';
                document.getElementById('configGateway').value = network.gateway || '';
                document.getElementById('configDns1').value = network.dns1 || '';
                document.getElementById('configDns2').value = network.dns2 || '';
            } else {
                staticIPFields.style.display = 'none';
                // Clear fields
                document.getElementById('configStaticIP').value = '';
                document.getElementById('configSubnet').value = '255.255.255.0';
                document.getElementById('configGateway').value = '';
                document.getElementById('configDns1').value = '';
                document.getElementById('configDns2').value = '';
            }

            // Update button label
            updateConnectButtonLabel();

            // Add change listeners to update button label
            const fieldsToWatch = ['configPassword', 'configUseStaticIP', 'configStaticIP', 'configSubnet', 'configGateway', 'configDns1', 'configDns2'];
            fieldsToWatch.forEach(fieldId => {
                const field = document.getElementById(fieldId);
                if (field) {
                    field.removeEventListener('input', updateConnectButtonLabel);
                    field.removeEventListener('change', updateConnectButtonLabel);
                    field.addEventListener('input', updateConnectButtonLabel);
                    field.addEventListener('change', updateConnectButtonLabel);
                }
            });
        }

        function updateConnectButtonLabel() {
            const btn = document.getElementById('connectUpdateBtn');
            if (!btn) return;

            const passwordField = document.getElementById('configPassword');
            const useStaticIP = document.getElementById('configUseStaticIP').checked;

            // Check if password was entered
            const hasPasswordChange = passwordField.value.trim() !== '';

            // Check if static IP settings changed
            const hasStaticIPChange =
                useStaticIP !== originalNetworkConfig.useStaticIP ||
                (useStaticIP && (
                    document.getElementById('configStaticIP').value !== originalNetworkConfig.staticIP ||
                    document.getElementById('configSubnet').value !== originalNetworkConfig.subnet ||
                    document.getElementById('configGateway').value !== originalNetworkConfig.gateway ||
                    document.getElementById('configDns1').value !== originalNetworkConfig.dns1 ||
                    document.getElementById('configDns2').value !== originalNetworkConfig.dns2
                ));

            // Update button text based on whether changes were made
            if (hasPasswordChange || hasStaticIPChange) {
                btn.textContent = 'Connect & Update';
            } else {
                btn.textContent = 'Connect';
            }
        }

        // Toggle config static IP fields visibility
        function toggleConfigStaticIPFields() {
            const useStaticIP = document.getElementById('configUseStaticIP').checked;
            const fields = document.getElementById('configStaticIPFields');
            fields.style.display = useStaticIP ? 'block' : 'none';
            updateConnectButtonLabel();
        }

        // Update network configuration
        function updateNetworkConfig(connect) {
            const select = document.getElementById('configNetworkSelect');
            const selectedIndex = parseInt(select.value);

            if (isNaN(selectedIndex)) {
                showToast('Please select a network', 'error');
                return;
            }

            const network = savedNetworksData.find(net => net.index === selectedIndex);
            if (!network) {
                showToast('Network not found', 'error');
                return;
            }

            const passwordField = document.getElementById('configPassword');
            const password = passwordField.value.trim();

            // If password is empty, we'll send empty string and backend will keep existing password
            const useStaticIP = document.getElementById('configUseStaticIP').checked;
            const requestBody = {
                ssid: network.ssid,
                password: password, // Empty string keeps existing password
                useStaticIP: useStaticIP
            };

            if (useStaticIP) {
                requestBody.staticIP = document.getElementById('configStaticIP').value;
                requestBody.subnet = document.getElementById('configSubnet').value;
                requestBody.gateway = document.getElementById('configGateway').value;
                requestBody.dns1 = document.getElementById('configDns1').value;
                requestBody.dns2 = document.getElementById('configDns2').value;

                // Validate IP addresses
                if (!isValidIP(requestBody.staticIP)) {
                    showToast('Invalid IPv4 address', 'error');
                    return;
                }
                if (!isValidIP(requestBody.subnet)) {
                    showToast('Invalid network mask', 'error');
                    return;
                }
                if (!isValidIP(requestBody.gateway)) {
                    showToast('Invalid gateway address', 'error');
                    return;
                }
                if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
                    showToast('Invalid primary DNS address', 'error');
                    return;
                }
                if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
                    showToast('Invalid secondary DNS address', 'error');
                    return;
                }
            }

            // Choose endpoint based on connect parameter
            const endpoint = connect ? '/api/wificonfig' : '/api/wifisave';

            // Show connection modal if connecting
            if (connect) {
                showWiFiModal(network.ssid);
            }

            apiFetch(endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(requestBody)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    if (connect) {
                        // Start polling for connection status
                        if (wifiConnectionPollTimer) clearInterval(wifiConnectionPollTimer);
                        wifiConnectionPollTimer = setInterval(pollWiFiConnection, 2000);
                    } else {
                        showToast('Network settings updated', 'success');
                        loadSavedNetworks(); // Reload the list
                        // Clear password field after successful save
                        document.getElementById('configPassword').value = '';
                    }
                } else {
                    if (connect) {
                        updateWiFiConnectionStatus('error', data.message || 'Failed to connect');
                    } else {
                        showToast(data.message || 'Failed to update settings', 'error');
                    }
                }
            })
            .catch(err => {
                if (connect) {
                    updateWiFiConnectionStatus('error', 'Network error: ' + err.message);
                } else {
                    showToast('Network error: ' + err.message, 'error');
                }
            });
        }

        let networkRemovalPollTimer = null;

        function removeSelectedNetworkConfig() {
            const select = document.getElementById('configNetworkSelect');
            const selectedIndex = parseInt(select.value);

            if (isNaN(selectedIndex)) {
                showToast('Please select a network to remove', 'error');
                return;
            }

            const network = savedNetworksData.find(net => net.index === selectedIndex);
            if (!network) {
                showToast('Network not found', 'error');
                return;
            }

            // Check if this is the currently connected network
            const isCurrentNetwork = currentWifiConnected && currentWifiSSID === network.ssid;

            if (isCurrentNetwork) {
                // Show warning modal for currently connected network
                showRemoveCurrentNetworkModal(network, selectedIndex);
            } else {
                // Show simple confirmation for other networks
                if (!confirm(`Are you sure you want to remove "${network.ssid}"?`)) {
                    return;
                }
                performNetworkRemoval(selectedIndex, false);
            }
        }

        function showRemoveCurrentNetworkModal(network, selectedIndex) {
            const modal = document.createElement('div');
            modal.id = 'removeNetworkModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title">⚠️ Remove Current Network</div>
                    <div class="info-box" style="background: var(--error-bg); border-color: var(--error);">
                        <div style="padding: 20px;">
                            <div style="font-size: 16px; margin-bottom: 16px; font-weight: bold; color: var(--error);">
                                Warning: You are currently connected to this network
                            </div>
                            <div style="margin-bottom: 12px;">
                                Network: <strong>${network.ssid}</strong>
                            </div>
                            <div style="margin-bottom: 16px; line-height: 1.5;">
                                If you remove this network, the device will:
                                <ul style="margin: 8px 0; padding-left: 20px;">
                                    <li>Disconnect from this network</li>
                                    <li>Try to connect to other saved networks</li>
                                    <li>Start AP Mode if no networks connect successfully</li>
                                </ul>
                            </div>
                            <div style="font-weight: bold;">
                                Do you want to continue?
                            </div>
                        </div>
                    </div>
                    <div class="modal-actions">
                        <button class="secondary" onclick="closeRemoveNetworkModal()">Cancel</button>
                        <button class="primary" style="background: var(--error);" onclick="confirmNetworkRemoval(${selectedIndex})">Remove Network</button>
                    </div>
                </div>
            `;
            document.body.appendChild(modal);
        }

        function closeRemoveNetworkModal() {
            const modal = document.getElementById('removeNetworkModal');
            if (modal) modal.remove();
        }

        function confirmNetworkRemoval(selectedIndex) {
            closeRemoveNetworkModal();
            performNetworkRemoval(selectedIndex, true);
        }

        function performNetworkRemoval(selectedIndex, wasCurrentNetwork) {
            apiFetch('/api/wifiremove', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ index: selectedIndex })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Network removed successfully', 'success');

                    // Reload the network list
                    apiFetch('/api/wifilist')
                    .then(res => res.json())
                    .then(listData => {
                        if (listData.success && listData.networks) {
                            // Store networks data globally
                            savedNetworksData = listData.networks;

                            const configSelect = document.getElementById('configNetworkSelect');
                            if (listData.networks.length > 0) {
                                // Populate config network select dropdown
                                configSelect.innerHTML = '<option value="">-- Select a saved network --</option>';
                                listData.networks.forEach(net => {
                                    const option = document.createElement('option');
                                    option.value = net.index;
                                    option.textContent = net.ssid + (net.useStaticIP ? ' (Static IP)' : '');
                                    configSelect.appendChild(option);
                                });
                            } else {
                                configSelect.innerHTML = '<option value="">-- No saved networks --</option>';
                            }

                            // Reset the select dropdown and hide fields
                            configSelect.value = '';
                            document.getElementById('networkConfigFields').style.display = 'none';
                        }
                    })
                    .catch(err => {
                        showToast('Failed to reload network list', 'error');
                    });

                    // If this was the current network, monitor for reconnection or AP mode
                    if (wasCurrentNetwork) {
                        showToast('Attempting to connect to other saved networks...', 'info');
                        monitorNetworkRemoval();
                    }
                } else {
                    showToast(data.message || 'Failed to remove network', 'error');
                }
            })
            .catch(err => {
                showToast('Network error: ' + err.message, 'error');
            });
        }

        function monitorNetworkRemoval() {
            let pollCount = 0;
            const maxPolls = 30; // Poll for up to 30 seconds

            if (networkRemovalPollTimer) {
                clearInterval(networkRemovalPollTimer);
            }

            networkRemovalPollTimer = setInterval(() => {
                pollCount++;

                apiFetch('/api/wifistatus')
                    .then(res => res.json())
                    .then(data => {
                        // Check if AP mode is now active and we're not connected to WiFi
                        if (data.mode === 'ap' && !data.connected && data.apIP) {
                            clearInterval(networkRemovalPollTimer);
                            networkRemovalPollTimer = null;
                            showAPModeModal(data.apIP);
                        }
                        // Check if successfully reconnected to another network
                        else if (data.connected) {
                            clearInterval(networkRemovalPollTimer);
                            networkRemovalPollTimer = null;
                            showWiFiModal(data.ssid);
                            updateWiFiConnectionStatus('success', 'Network removed. Reconnected successfully!', data.ip);
                        }
                        // Timeout after max polls
                        else if (pollCount >= maxPolls) {
                            clearInterval(networkRemovalPollTimer);
                            networkRemovalPollTimer = null;
                            showWiFiModal('');
                            updateWiFiConnectionStatus('error', 'Failed to connect to any saved network');
                        }
                    })
                    .catch(err => {
                        console.error('Error polling WiFi status:', err);
                    });
            }, 1000); // Poll every second
        }

        function toggleAP() {
            const enabled = document.getElementById('apToggle').checked;
            document.getElementById('apFields').style.display = enabled ? '' : 'none';
            apiFetch('/api/toggleap', {
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
            // Pre-fill with current AP SSID from stored data
            if (currentAPSSID) {
                document.getElementById('appState.apSSID').value = currentAPSSID;
            }
            document.getElementById('apConfigModal').classList.add('active');
        }

        function closeAPConfig() {
            document.getElementById('apConfigModal').classList.remove('active');
        }

        function submitAPConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('appState.apSSID').value;
            const password = document.getElementById('appState.apPassword').value;
            
            if (password.length > 0 && password.length < 8) {
                showToast('Password must be at least 8 characters', 'error');
                return;
            }
            
            apiFetch('/api/apconfig', {
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
            apiFetch('/api/mqtt')
            .then(res => res.json())
            .then(data => {
                document.getElementById('appState.mqttEnabled').checked = data.enabled || false;
                document.getElementById('mqttFields').style.display = (data.enabled || false) ? '' : 'none';
                document.getElementById('appState.mqttBroker').value = data.broker || '';
                document.getElementById('appState.mqttPort').value = data.port || 1883;
                document.getElementById('appState.mqttUsername').value = data.username || '';
                document.getElementById('appState.mqttPassword').value = '';
                document.getElementById('appState.mqttPassword').placeholder = data.hasPassword
                    ? 'Enter password (leave empty to keep current)'
                    : 'Password';
                document.getElementById('appState.mqttBaseTopic').value = data.baseTopic || '';
                document.getElementById('appState.mqttBaseTopic').placeholder = data.defaultBaseTopic || 'ALX/device-serial';
                document.getElementById('mqttDefaultTopic').textContent = data.defaultBaseTopic || 'ALX/{serial}';
                document.getElementById('appState.mqttHADiscovery').checked = data.haDiscovery || false;
                updateMqttConnectionStatus(data.connected, data.broker, data.port, data.effectiveBaseTopic);
            })
            .catch(err => console.error('Failed to load MQTT settings:', err));
        }

        function updateMqttConnectionStatus(connected, broker, port, baseTopic) {
            const statusBox = document.getElementById('mqttStatusBox');
            const enabled = document.getElementById('appState.mqttEnabled').checked;
            
            let html = '';
            if (connected) {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                `;
                currentMqttConnected = true;
            } else if (enabled) {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-error">Disconnected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Not configured'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                `;
                currentMqttConnected = false;
            } else {
                html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-secondary">Disabled</span></div>
                    <div class="info-row"><span class="info-label">MQTT</span><span class="info-value">Not enabled</span></div>
                `;
                currentMqttConnected = null;
            }
            statusBox.innerHTML = html;
            // Update status bar
            updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);
        }

        function toggleMqttEnabled() {
            const enabled = document.getElementById('appState.mqttEnabled').checked;
            document.getElementById('mqttFields').style.display = enabled ? '' : 'none';
            apiFetch('/api/mqtt', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: enabled })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast(enabled ? 'MQTT enabled' : 'MQTT disabled', 'success');
                    setTimeout(loadMqttSettings, 1000);
                } else {
                    showToast(data.message || 'Failed to toggle MQTT', 'error');
                    // Revert toggle on failure
                    document.getElementById('appState.mqttEnabled').checked = !enabled;
                }
            })
            .catch(err => {
                showToast('Failed to toggle MQTT', 'error');
                document.getElementById('appState.mqttEnabled').checked = !enabled;
            });
        }

        function saveMqttSettings() {
            const settings = {
                broker: document.getElementById('appState.mqttBroker').value,
                port: parseInt(document.getElementById('appState.mqttPort').value) || 1883,
                username: document.getElementById('appState.mqttUsername').value,
                password: document.getElementById('appState.mqttPassword').value,
                baseTopic: document.getElementById('appState.mqttBaseTopic').value,
                haDiscovery: document.getElementById('appState.mqttHADiscovery').checked
            };
            
            apiFetch('/api/mqtt', {
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

        // ===== WiFi Management Functions =====
        function toggleAutoAP() {
            const enabled = document.getElementById('autoAPToggle').checked;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ autoAPEnabled: enabled })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast(enabled ? 'Auto AP enabled' : 'Auto AP disabled', 'success');
            })
            .catch(err => showToast('Failed to update setting', 'error'));
        }

        // ===== Settings =====
        let currentDstOffset = 0;
        let timeUpdateInterval = null;

        function updateTimezone() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            const dstOffset = document.getElementById('dstToggle').checked ? 3600 : 0;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezoneOffset: offset, dstOffset: dstOffset })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Timezone updated', 'success');
                    currentTimezoneOffset = offset;
                    currentDstOffset = dstOffset;
                    updateTimezoneDisplay(offset, dstOffset);
                    // Wait a moment for NTP sync then refresh time
                    setTimeout(updateCurrentTime, 2000);
                }
            })
            .catch(err => showToast('Failed to update timezone', 'error'));
        }

        function updateDST() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            const dstOffset = document.getElementById('dstToggle').checked ? 3600 : 0;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezoneOffset: offset, dstOffset: dstOffset })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('DST setting updated', 'success');
                    currentTimezoneOffset = offset;
                    currentDstOffset = dstOffset;
                    updateTimezoneDisplay(offset, dstOffset);
                    // Wait a moment for NTP sync then refresh time
                    setTimeout(updateCurrentTime, 2000);
                }
            })
            .catch(err => showToast('Failed to update DST setting', 'error'));
        }

        function updateTimezoneDisplay(offset, dstOffset = 0) {
            const totalOffset = offset + dstOffset;
            const hours = totalOffset / 3600;
            const sign = hours >= 0 ? '+' : '';
            const baseHours = offset / 3600;
            const baseSign = baseHours >= 0 ? '+' : '';

            let displayText = `UTC${sign}${hours} hours (GMT${baseSign}${baseHours}`;
            if (dstOffset !== 0) {
                displayText += ' + DST)';
            } else {
                displayText += ')';
            }

            document.getElementById('timezoneInfo').textContent = displayText;
            updateCurrentTime();
        }

        function updateCurrentTime() {
            apiFetch('/api/settings')
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    // Create date object with current UTC time
                    const now = new Date();
                    // Apply timezone and DST offsets
                    const offset = (data.timezoneOffset || 0) + (data.dstOffset || 0);
                    const localTime = new Date(now.getTime() + offset * 1000);

                    // Format time
                    const year = localTime.getUTCFullYear();
                    const month = String(localTime.getUTCMonth() + 1).padStart(2, '0');
                    const day = String(localTime.getUTCDate()).padStart(2, '0');
                    const hours = String(localTime.getUTCHours()).padStart(2, '0');
                    const minutes = String(localTime.getUTCMinutes()).padStart(2, '0');
                    const seconds = String(localTime.getUTCSeconds()).padStart(2, '0');

                    document.getElementById('currentTimeDisplay').textContent =
                        `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
                }
            })
            .catch(err => {
                document.getElementById('currentTimeDisplay').textContent = 'Error loading time';
            });
        }

        // Update time every second when on settings tab
        function startTimeUpdates() {
            if (!timeUpdateInterval) {
                updateCurrentTime();
                timeUpdateInterval = setInterval(updateCurrentTime, 1000);
            }
        }

        function stopTimeUpdates() {
            if (timeUpdateInterval) {
                clearInterval(timeUpdateInterval);
                timeUpdateInterval = null;
            }
        }

        function toggleTheme() {
            darkMode = document.getElementById('darkModeToggle').checked;
            applyTheme(darkMode);
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ darkMode: darkMode })
            });
        }

        function applyTheme(isDarkMode) {
            if (isDarkMode) {
                document.body.classList.add('night-mode');
                document.querySelector('meta[name="theme-color"]').setAttribute('content', '#121212');
            } else {
                document.body.classList.remove('night-mode');
                document.querySelector('meta[name="theme-color"]').setAttribute('content', '#F5F5F5');
            }
            localStorage.setItem('darkMode', isDarkMode ? 'true' : 'false');
            invalidateBgCache();
        }

        function toggleBacklight() {
            backlightOn = document.getElementById('backlightToggle').checked;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setBacklight', enabled: backlightOn }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ backlightOn })
            });
        }

        function setBrightness() {
            var pct = parseInt(document.getElementById('brightnessSelect').value);
            var pwm = Math.round(pct * 255 / 100);
            backlightBrightness = pwm;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setBrightness', value: pwm }));
            }
        }

        function setScreenTimeout() {
            screenTimeoutSec = parseInt(document.getElementById('screenTimeoutSelect').value);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setScreenTimeout', value: screenTimeoutSec }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ screenTimeout: screenTimeoutSec })
            });
        }

        function updateDimVisibility() {
            var show = dimEnabled ? '' : 'none';
            document.getElementById('dimTimeoutRow').style.display = show;
            document.getElementById('dimBrightnessRow').style.display = show;
        }

        function toggleDim() {
            dimEnabled = document.getElementById('dimToggle').checked;
            updateDimVisibility();
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setDimEnabled', enabled: dimEnabled }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ dimEnabled: dimEnabled })
            });
        }

        function setDimTimeout() {
            dimTimeoutSec = parseInt(document.getElementById('dimTimeoutSelect').value);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setDimTimeout', value: dimTimeoutSec }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ dimTimeout: dimTimeoutSec })
            });
        }

        function setDimBrightness() {
            dimBrightnessPwm = parseInt(document.getElementById('dimBrightnessSelect').value);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setDimBrightness', value: dimBrightnessPwm }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ dimBrightness: dimBrightnessPwm })
            });
        }

        function setBootAnimation() {
            var val = parseInt(document.getElementById('bootAnimSelect').value);
            var payload = {};
            if (val < 0) {
                payload.bootAnimEnabled = false;
            } else {
                payload.bootAnimEnabled = true;
                payload.bootAnimStyle = val;
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        }

        function toggleBuzzer() {
            var enabled = document.getElementById('buzzerToggle').checked;
            document.getElementById('buzzerFields').style.display = enabled ? '' : 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setBuzzerEnabled', enabled: enabled }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ buzzerEnabled: enabled })
            });
        }

        function setBuzzerVolume() {
            var vol = parseInt(document.getElementById('buzzerVolumeSelect').value);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setBuzzerVolume', value: vol }));
            }
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ buzzerVolume: vol })
            });
        }

        function toggleAutoUpdate() {
            autoUpdateEnabled = document.getElementById('autoUpdateToggle').checked;
            apiFetch('/api/settings', {
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
            apiFetch('/api/settings', {
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
            apiFetch('/api/settings', {
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

        function setGraphEnabled(graph, enabled) {
            var map = {vuMeter:'setVuMeterEnabled', waveform:'setWaveformEnabled', spectrum:'setSpectrumEnabled'};
            var contentMap = {vuMeter:'vuMeterContent', waveform:'waveformContent', spectrum:'spectrumContent'};
            toggleGraphDisabled(contentMap[graph], !enabled);
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:map[graph], enabled:enabled}));
        }
        function setFftWindow(val) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'setFftWindowType', value:parseInt(val)}));
        }
        function toggleGraphDisabled(id, disabled) {
            var el = document.getElementById(id);
            if (el) { if (disabled) el.classList.add('graph-disabled'); else el.classList.remove('graph-disabled'); }
        }

        function setDebugToggle(type, enabled) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({type: type, enabled: enabled}));
            }
        }

        function setDebugSerialLevel(level) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({type: 'setDebugSerialLevel', level: parseInt(level)}));
            }
        }

        function applyDebugState(d) {
            var modeT = document.getElementById('debugModeToggle');
            var hwT = document.getElementById('debugHwStatsToggle');
            var i2sT = document.getElementById('debugI2sMetricsToggle');
            var tmT = document.getElementById('debugTaskMonitorToggle');
            var lvl = document.getElementById('debugSerialLevel');
            if (modeT) modeT.checked = d.debugMode;
            if (hwT) hwT.checked = d.debugHwStats;
            if (i2sT) i2sT.checked = d.debugI2sMetrics;
            if (tmT) tmT.checked = d.debugTaskMonitor;
            if (lvl) lvl.value = d.debugSerialLevel;

            // Hide/show I2S and Task sections when disabled
            var i2sSec = document.getElementById('i2sMetricsSection');
            if (i2sSec) i2sSec.style.display = (d.debugMode && d.debugI2sMetrics) ? '' : 'none';
            var tmSec = document.getElementById('taskMonitorSection');
            if (tmSec) tmSec.style.display = (d.debugMode && d.debugTaskMonitor) ? '' : 'none';

            // Hide/show hardware stats sections when HW Stats disabled
            var hwSec = document.getElementById('hwStatsSection');
            if (hwSec) hwSec.style.display = (d.debugMode && d.debugHwStats) ? '' : 'none';

            // Hide/show debug tab based on debugMode only
            updateDebugTabVisibility(d.debugMode);
        }

        function updateDebugTabVisibility(visible) {
            // Sidebar item
            var sideItem = document.querySelector('.sidebar-item[data-tab="debug"]');
            if (sideItem) sideItem.style.display = visible ? '' : 'none';
            // Mobile tab button
            var tabBtn = document.querySelector('.tab[data-tab="debug"]');
            if (tabBtn) tabBtn.style.display = visible ? '' : 'none';
            // If currently on debug tab and hiding, switch to settings
            if (!visible) {
                var debugPanel = document.getElementById('debug');
                if (debugPanel && debugPanel.classList.contains('active')) {
                    switchTab('settings');
                }
            }
        }

        function setAudioUpdateRate() {
            const rate = parseInt(document.getElementById('audioUpdateRateSelect').value);
            updateLerpFactors(rate);
            const labels = {100:'100 ms',50:'50 ms',33:'33 ms',20:'20 ms'};
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ audioUpdateRate: rate })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Audio rate set to ' + (labels[rate]||rate+'ms'), 'success');
            })
            .catch(err => showToast('Failed to update audio rate', 'error'));
        }

        // ===== Firmware Update =====
        function checkForUpdate() {
            showToast('Checking for updates...', 'info');
            apiFetch('/api/checkupdate')
            .then(res => res.json())
            .then(data => {
                // Update current version display
                if (data.currentVersion) {
                    currentFirmwareVersion = data.currentVersion;
                    document.getElementById('currentVersion').textContent = data.currentVersion;
                }
                
                // Update latest version display
                if (data.latestVersion) {
                    currentLatestVersion = data.latestVersion;
                    const latestVersionEl = document.getElementById('latestVersion');
                    const latestVersionNotes = document.getElementById('latestVersionNotes');
                    document.getElementById('latestVersionRow').style.display = 'flex';

                    // If up-to-date, show green "Up-To-Date" text and hide release notes link
                    if (!data.updateAvailable && data.latestVersion !== 'Unknown') {
                        latestVersionEl.textContent = 'Up-To-Date, no newer version available';
                        latestVersionEl.style.opacity = '1';
                        latestVersionEl.style.fontStyle = 'normal';
                        latestVersionEl.style.color = 'var(--success)';
                        latestVersionNotes.style.display = 'none';
                    } else {
                        latestVersionEl.textContent = data.latestVersion;
                        latestVersionNotes.style.display = '';

                        // Style based on status
                        if (data.latestVersion === 'Unknown') {
                            latestVersionEl.style.opacity = '0.6';
                            latestVersionEl.style.fontStyle = 'italic';
                            latestVersionEl.style.color = 'var(--text-secondary)';
                        } else {
                            latestVersionEl.style.opacity = '1';
                            latestVersionEl.style.fontStyle = 'normal';
                            latestVersionEl.style.color = '';
                        }
                    }
                }

                // Show/hide update button based on update availability
                if (data.updateAvailable) {
                    document.getElementById('updateBtn').classList.remove('hidden');
                    showToast(`Update available: ${data.latestVersion}`, 'success');
                } else {
                    document.getElementById('updateBtn').classList.add('hidden');
                    showToast('Firmware is up to date', 'success');
                }
                
                // Reset progress UI when checking
                document.getElementById('progressContainer').classList.add('hidden');
                document.getElementById('progressStatus').classList.add('hidden');
            })
            .catch(err => showToast('Failed to check for updates', 'error'));
        }

        function fetchUpdateStatus() {
            apiFetch('/api/updatestatus')
            .then(res => res.json())
            .then(data => handleUpdateStatus(data))
            .catch(err => console.error('Failed to fetch update status:', err));
        }

        function startOTAUpdate() {
            apiFetch('/api/startupdate', { method: 'POST' })
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
            // Skip progress bar updates if manual upload is in progress
            // Manual upload manages its own progress bar
            if (manualUploadInProgress) {
                return;
            }
            
            const container = document.getElementById('progressContainer');
            const bar = document.getElementById('progressBar');
            const status = document.getElementById('progressStatus');
            const updateBtn = document.getElementById('updateBtn');
            
            // Update version info if available
            if (data.currentVersion) {
                currentFirmwareVersion = data.currentVersion;
                document.getElementById('currentVersion').textContent = data.currentVersion;
            }
            if (data.latestVersion) {
                currentLatestVersion = data.latestVersion;
                const latestVersionEl = document.getElementById('latestVersion');
                latestVersionEl.textContent = data.latestVersion;
                document.getElementById('latestVersionRow').style.display = 'flex';

                // Style based on status
                if (data.latestVersion === 'Unknown') {
                    latestVersionEl.style.opacity = '0.6';
                    latestVersionEl.style.fontStyle = 'italic';
                    latestVersionEl.style.color = 'var(--text-secondary)';
                } else {
                    latestVersionEl.style.opacity = '1';
                    latestVersionEl.style.fontStyle = 'normal';
                    latestVersionEl.style.color = '';
                }
            }
            
            // Handle different status states
            if (data.status === 'preparing') {
                container.classList.remove('hidden');
                status.classList.remove('hidden');
                bar.style.width = '0%';
                status.textContent = data.message || 'Preparing for update...';
                updateBtn.classList.add('hidden');
            } else if (data.status === 'downloading' || data.status === 'uploading') {
                container.classList.remove('hidden');
                status.classList.remove('hidden');
                bar.style.width = data.progress + '%';
                // Show percentage and downloaded size for OTA
                let statusText = `${data.progress}%`;
                if (data.bytesDownloaded !== undefined && data.totalBytes !== undefined && data.totalBytes > 0) {
                    const downloadedKB = (data.bytesDownloaded / 1024).toFixed(0);
                    const totalKB = (data.totalBytes / 1024).toFixed(0);
                    statusText = `Downloading: ${data.progress}% (${downloadedKB} / ${totalKB} KB)`;
                }
                status.textContent = data.message || statusText;
                updateBtn.classList.add('hidden');
            } else if (data.status === 'complete') {
                bar.style.width = '100%';
                status.textContent = 'Update complete! Rebooting...';
                showToast('Update complete! Rebooting...', 'success');
                updateBtn.classList.add('hidden');
                wasDisconnectedDuringUpdate = true;  // Flag for reconnection notification
            } else if (data.status === 'error') {
                container.classList.add('hidden');
                status.classList.add('hidden');
                showToast(data.message || 'Update failed', 'error');
                // Re-show update button if update is still available
                if (data.updateAvailable) {
                    updateBtn.classList.remove('hidden');
                }
            } else {
                // Idle state or unknown - reset UI
                container.classList.add('hidden');
                status.classList.add('hidden');
                
                // Show/hide update button based on update availability
                if (data.updateAvailable) {
                    updateBtn.classList.remove('hidden');
                } else {
                    updateBtn.classList.add('hidden');
                }
            }
        }

        function showUpdateSuccessNotification(data) {
            // Clear the flag so we don't show duplicate notification
            wasDisconnectedDuringUpdate = false;
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
                        wasDisconnectedDuringUpdate = true;  // Flag for reconnection notification
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
            apiFetch('/api/settings/export')
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
            apiFetch('/api/settings/import', {
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
                apiFetch('/api/reboot', { method: 'POST' })
                .then(res => res.json())
                .then(data => {
                    if (data.success) showToast('Rebooting...', 'success');
                })
                .catch(err => showToast('Failed to reboot', 'error'));
            }
        }

        function startFactoryReset() {
            if (confirm('Are you sure? This will erase all settings!')) {
                apiFetch('/api/factoryreset', { method: 'POST' })
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
            // Update ADC count from hardware_stats (fires on all tabs)
            if (data.audio && data.audio.numAdcsDetected !== undefined) {
                numAdcsDetected = data.audio.numAdcsDetected;
                updateAdc2Visibility();
            }
            // CPU Stats
            if (data.cpu) {
                var cpuCalibrating = (data.cpu.usageCore0 < 0 || data.cpu.usageCore1 < 0);
                document.getElementById('cpuTotal').textContent = cpuCalibrating ? 'Calibrating...' : Math.round(data.cpu.usageTotal || 0) + '%';
                document.getElementById('cpuCore0').textContent = cpuCalibrating ? '...' : Math.round(data.cpu.usageCore0 || 0) + '%';
                document.getElementById('cpuCore1').textContent = cpuCalibrating ? '...' : Math.round(data.cpu.usageCore1 || 0) + '%';
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
                
                const LittleFSTotal = data.storage.LittleFSTotal || 0;
                const LittleFSUsed = data.storage.LittleFSUsed || 0;
                const LittleFSPercent = LittleFSTotal > 0 ? Math.round((LittleFSUsed / LittleFSTotal) * 100) : 0;
                document.getElementById('LittleFSPercent').textContent = LittleFSTotal > 0 ? LittleFSPercent + '%' : 'N/A';
                document.getElementById('LittleFSUsed').textContent = LittleFSTotal > 0 ? formatBytes(LittleFSUsed) : 'N/A';
                document.getElementById('LittleFSTotal').textContent = LittleFSTotal > 0 ? formatBytes(LittleFSTotal) : 'N/A';
            }
            
            // WiFi Stats
            if (data.wifi) {
                document.getElementById('wifiRssi').innerHTML = formatRssi(data.wifi.rssi);
                document.getElementById('wifiChannel').textContent = data.wifi.channel || '--';
                document.getElementById('apClients').textContent = data.wifi.apClients || 0;
            }
            
            // Audio ADC — per-ADC diagnostics from adcs array
            if (data.audio) {
                if (data.audio.adcs && Array.isArray(data.audio.adcs)) {
                    var adcs = data.audio.adcs;
                    // Show ADC 0 in the existing fields (legacy)
                    if (adcs.length > 0) {
                        document.getElementById('adcStatus').textContent = adcs[0].status || '--';
                        document.getElementById('adcNoiseFloor').textContent = adcs[0].noiseFloorDbfs !== undefined ? adcs[0].noiseFloorDbfs.toFixed(1) + ' dBFS' : '--';
                        document.getElementById('adcI2sErrors').textContent = adcs[0].i2sErrors !== undefined ? adcs[0].i2sErrors : '--';
                        document.getElementById('adcConsecutiveZeros').textContent = adcs[0].consecutiveZeros !== undefined ? adcs[0].consecutiveZeros : '--';
                        document.getElementById('adcTotalBuffers').textContent = adcs[0].totalBuffers !== undefined ? adcs[0].totalBuffers : '--';
                        var snr0 = document.getElementById('audioSnr0');
                        if (snr0 && adcs[0].snrDb !== undefined) snr0.textContent = adcs[0].snrDb.toFixed(1);
                        var sfdr0 = document.getElementById('audioSfdr0');
                        if (sfdr0 && adcs[0].sfdrDb !== undefined) sfdr0.textContent = adcs[0].sfdrDb.toFixed(1);
                    }
                    // Show ADC 1 fields if present
                    var el2 = document.getElementById('adcStatus1');
                    if (el2 && adcs.length > 1) {
                        el2.textContent = adcs[1].status || '--';
                        var nf2 = document.getElementById('adcNoiseFloor1');
                        if (nf2) nf2.textContent = adcs[1].noiseFloorDbfs !== undefined ? adcs[1].noiseFloorDbfs.toFixed(1) + ' dBFS' : '--';
                        var ie2 = document.getElementById('adcI2sErrors1');
                        if (ie2) ie2.textContent = adcs[1].i2sErrors !== undefined ? adcs[1].i2sErrors : '--';
                        var cz2 = document.getElementById('adcConsecutiveZeros1');
                        if (cz2) cz2.textContent = adcs[1].consecutiveZeros !== undefined ? adcs[1].consecutiveZeros : '--';
                        var tb2 = document.getElementById('adcTotalBuffers1');
                        if (tb2) tb2.textContent = adcs[1].totalBuffers !== undefined ? adcs[1].totalBuffers : '--';
                    }
                } else {
                    // Legacy flat format fallback
                    document.getElementById('adcStatus').textContent = data.audio.adcStatus || '--';
                    document.getElementById('adcNoiseFloor').textContent = (data.audio.noiseFloorDbfs !== undefined ? data.audio.noiseFloorDbfs.toFixed(1) + ' dBFS' : '--');
                    document.getElementById('adcI2sErrors').textContent = data.audio.i2sErrors !== undefined ? data.audio.i2sErrors : '--';
                    document.getElementById('adcConsecutiveZeros').textContent = data.audio.consecutiveZeros !== undefined ? data.audio.consecutiveZeros : '--';
                    document.getElementById('adcTotalBuffers').textContent = data.audio.totalBuffers !== undefined ? data.audio.totalBuffers : '--';
                }
                document.getElementById('adcSampleRate').textContent = data.audio.sampleRate ? (data.audio.sampleRate / 1000).toFixed(1) + ' kHz' : '--';
            }

            // Audio DAC diagnostics
            if (data.dac) {
                var d = data.dac;
                var statusEl = document.getElementById('dacStatus');
                if (statusEl) {
                    var statusText = d.ready ? 'Ready' : (d.enabled ? 'Not Ready' : 'Disabled');
                    statusEl.textContent = statusText;
                    statusEl.style.color = d.ready ? 'var(--success-color)' : (d.enabled ? 'var(--error-color)' : '');
                }
                var el;
                el = document.getElementById('dacModel'); if (el) el.textContent = d.model || '--';
                el = document.getElementById('dacManufacturer'); if (el) el.textContent = d.manufacturer || '--';
                el = document.getElementById('dacDeviceId'); if (el) el.textContent = d.deviceId !== undefined ? '0x' + ('0000' + d.deviceId.toString(16)).slice(-4).toUpperCase() : '--';
                el = document.getElementById('dacDetection'); if (el) el.textContent = d.detected ? 'EEPROM (Auto)' : 'Manual';
                el = document.getElementById('dacDbgEnabled'); if (el) el.textContent = d.enabled ? 'Yes' : 'No';
                el = document.getElementById('dacDbgVolume'); if (el) el.textContent = d.volume !== undefined ? d.volume + '%' : '--';
                el = document.getElementById('dacDbgMute'); if (el) el.textContent = d.mute ? 'Yes' : 'No';
                el = document.getElementById('dacDbgChannels'); if (el) el.textContent = d.outputChannels || '--';
                el = document.getElementById('dacDbgFilter'); if (el) el.textContent = d.filterMode !== undefined ? d.filterMode : '--';
                el = document.getElementById('dacHwVolume'); if (el) el.textContent = d.hwVolume ? 'Yes' : 'No';
                el = document.getElementById('dacI2cControl'); if (el) el.textContent = d.i2cControl ? 'Yes' : 'No';
                el = document.getElementById('dacIndepClock'); if (el) el.textContent = d.independentClock ? 'Yes' : 'No';
                el = document.getElementById('dacHasFilters'); if (el) el.textContent = d.hasFilters ? 'Yes' : 'No';
                // TX diagnostics
                if (d.tx) {
                    el = document.getElementById('dacI2sTxEnabled'); if (el) {
                        el.textContent = d.tx.i2sTxEnabled ? 'Yes' : 'No';
                        el.style.color = d.tx.i2sTxEnabled ? 'var(--success-color)' : 'var(--error-color)';
                    }
                    el = document.getElementById('dacVolumeGain'); if (el) el.textContent = d.tx.volumeGain !== undefined ? parseFloat(d.tx.volumeGain).toFixed(4) : '--';
                    el = document.getElementById('dacTxWrites'); if (el) el.textContent = d.tx.writeCount || 0;
                    el = document.getElementById('dacTxData'); if (el) {
                        var written = d.tx.bytesWritten || 0;
                        var expected = d.tx.bytesExpected || 0;
                        var wKB = (written / 1024).toFixed(0);
                        var eKB = (expected / 1024).toFixed(0);
                        el.textContent = wKB + 'KB / ' + eKB + 'KB';
                        el.style.color = (expected > 0 && written < expected) ? 'var(--warning-color)' : '';
                    }
                    el = document.getElementById('dacTxPeak'); if (el) el.textContent = d.tx.peakSample || 0;
                    el = document.getElementById('dacTxZeroFrames'); if (el) el.textContent = d.tx.zeroFrames || 0;
                }
                el = document.getElementById('dacTxUnderruns'); if (el) {
                    el.textContent = d.txUnderruns || 0;
                    el.style.color = d.txUnderruns > 0 ? 'var(--warning-color)' : '';
                }
                // EEPROM diagnostics from hardware_stats
                if (d.eeprom) handleEepromDiag(d.eeprom);
            }

            if (data.audio) {
                // I2S Static Config
                if (data.audio.i2sConfig && Array.isArray(data.audio.i2sConfig)) {
                    for (var i = 0; i < data.audio.i2sConfig.length && i < 2; i++) {
                        var c = data.audio.i2sConfig[i];
                        var el;
                        el = document.getElementById('i2sMode' + i); if (el) el.textContent = c.mode || '--';
                        el = document.getElementById('i2sSampleRate' + i); if (el) el.textContent = c.sampleRate ? (c.sampleRate / 1000) + ' kHz' : '--';
                        el = document.getElementById('i2sBits' + i); if (el) el.textContent = c.bitsPerSample ? c.bitsPerSample + '-bit (24-bit payload)' : '--';
                        el = document.getElementById('i2sChannels' + i); if (el) el.textContent = c.channelFormat || '--';
                        el = document.getElementById('i2sDma' + i); if (el) el.textContent = c.dmaBufCount && c.dmaBufLen ? c.dmaBufCount + ' x ' + c.dmaBufLen : '--';
                        el = document.getElementById('i2sApll' + i); if (el) el.textContent = c.apll ? 'On' : 'Off';
                        el = document.getElementById('i2sMclk' + i); if (el) el.textContent = c.mclkHz ? (c.mclkHz / 1e6).toFixed(3) + ' MHz' : 'N/A';
                        el = document.getElementById('i2sFormat' + i); if (el) el.textContent = c.commFormat || '--';
                    }
                }

                // I2S Runtime Metrics
                if (data.audio.i2sRuntime) {
                    var rt = data.audio.i2sRuntime;
                    var stackEl = document.getElementById('i2sStackFree');
                    if (stackEl) {
                        var stackFree = rt.stackFree || 0;
                        var stackTotal = 10240;
                        var stackUsed = stackTotal - stackFree;
                        var stackPct = stackTotal > 0 ? Math.round(stackUsed / stackTotal * 100) : 0;
                        stackEl.textContent = stackFree + ' bytes (' + stackPct + '% used)';
                        stackEl.style.color = stackFree < 1024 ? 'var(--error-color)' : '';
                    }
                    var expectedBps = 187.5;
                    if (rt.buffersPerSec) {
                        for (var i = 0; i < rt.buffersPerSec.length && i < 2; i++) {
                            var tEl = document.getElementById('i2sThroughput' + i);
                            if (tEl) {
                                var bps = parseFloat(rt.buffersPerSec[i]) || 0;
                                tEl.textContent = bps.toFixed(1) + ' buf/s';
                                tEl.style.color = (bps > 0 && bps < expectedBps * 0.9) ? 'var(--error-color)' : '';
                            }
                        }
                    }
                    if (rt.avgReadLatencyUs) {
                        for (var i = 0; i < rt.avgReadLatencyUs.length && i < 2; i++) {
                            var lEl = document.getElementById('i2sLatency' + i);
                            if (lEl) {
                                var lat = parseFloat(rt.avgReadLatencyUs[i]) || 0;
                                lEl.textContent = (lat / 1000).toFixed(2) + ' ms';
                                lEl.style.color = lat > 10000 ? 'var(--error-color)' : '';
                            }
                        }
                    }
                }
            }

            // FreeRTOS Tasks — CPU load from hardware_stats
            if (data.cpu) {
                var tmCal = (data.cpu.usageCore0 < 0 || data.cpu.usageCore1 < 0);
                var el0 = document.getElementById('tmCpuCore0');
                if (el0) el0.textContent = tmCal ? '...' : Math.round(data.cpu.usageCore0 || 0) + '%';
                var el1 = document.getElementById('tmCpuCore1');
                if (el1) el1.textContent = tmCal ? '...' : Math.round(data.cpu.usageCore1 || 0) + '%';
                var elt = document.getElementById('tmCpuTotal');
                if (elt) elt.textContent = tmCal ? '...' : Math.round(data.cpu.usageTotal || 0) + '%';
            }
            if (data.tasks) {
                var tc = document.getElementById('taskCount');
                if (tc) tc.textContent = data.tasks.count || 0;
                var avgUs = data.tasks.loopAvgUs || 0;
                var la = document.getElementById('loopTimeAvg');
                if (la) la.textContent = avgUs + ' us';
                var lm = document.getElementById('loopTimeMax');
                if (lm) lm.textContent = (data.tasks.loopMaxUs || 0) + ' us';
                var lf = document.getElementById('tmLoopFreq');
                if (lf) lf.textContent = avgUs > 0 ? Math.round(1000000 / avgUs) + ' Hz' : '-- Hz';

                if (data.tasks.list) {
                    _taskData = data.tasks.list;
                    renderTaskTable();
                }
            } else {
                var tbody = document.getElementById('taskTableBody');
                if (tbody && !tbody.dataset.populated) {
                    var tc = document.getElementById('taskCount');
                    if (tc) tc.textContent = 'Disabled';
                    var la = document.getElementById('loopTimeAvg');
                    if (la) la.textContent = '-';
                    var lm = document.getElementById('loopTimeMax');
                    if (lm) lm.textContent = '-';
                    var lf = document.getElementById('tmLoopFreq');
                    if (lf) lf.textContent = '-';
                    var el0 = document.getElementById('tmCpuCore0');
                    if (el0) el0.textContent = '-';
                    var el1 = document.getElementById('tmCpuCore1');
                    if (el1) el1.textContent = '-';
                    var elt = document.getElementById('tmCpuTotal');
                    if (elt) elt.textContent = '--%';
                    tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;opacity:0.5">Task monitor disabled</td></tr>';
                }
            }

            // Uptime
            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
            }

            // Reset Reason
            if (data.resetReason) {
                document.getElementById('resetReason').textContent = formatResetReason(data.resetReason);
            }

            // Add to history
            addHistoryDataPoint(data);
        }

        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1048576).toFixed(1) + ' MB';
        }

        // ===== Task Table Sorting =====
        var _taskData = [];
        var _taskSortCol = 3; // default sort by priority
        var _taskSortAsc = false; // descending by default

        function sortTaskTable(col) {
            if (_taskSortCol === col) {
                _taskSortAsc = !_taskSortAsc;
            } else {
                _taskSortCol = col;
                _taskSortAsc = (col === 0); // name ascending by default, others descending
            }
            renderTaskTable();
        }

        function renderTaskTable() {
            var tbody = document.getElementById('taskTableBody');
            if (!tbody || !_taskData.length) return;
            tbody.dataset.populated = '1';

            var stateNames = ['Run', 'Rdy', 'Blk', 'Sus', 'Del'];
            // Build enriched rows for sorting
            var rows = [];
            for (var i = 0; i < _taskData.length; i++) {
                var t = _taskData[i];
                var stackFree = t.stackFree || 0;
                var stackAlloc = t.stackAlloc || 0;
                var usedPct = (stackAlloc > 0) ? Math.round((1 - stackFree / stackAlloc) * 100) : -1;
                rows.push({ name: t.name, stackFree: stackFree, stackAlloc: stackAlloc, usedPct: usedPct, pri: t.pri, state: t.state, core: t.core });
            }

            // Sort
            var col = _taskSortCol;
            var asc = _taskSortAsc;
            rows.sort(function(a, b) {
                var va, vb;
                switch (col) {
                    case 0: va = a.name.toLowerCase(); vb = b.name.toLowerCase(); return asc ? (va < vb ? -1 : va > vb ? 1 : 0) : (vb < va ? -1 : vb > va ? 1 : 0);
                    case 1: va = a.stackAlloc > 0 ? a.stackFree : 0; vb = b.stackAlloc > 0 ? b.stackFree : 0; break;
                    case 2: va = a.usedPct; vb = b.usedPct; break;
                    case 3: va = a.pri; vb = b.pri; break;
                    case 4: va = a.state; vb = b.state; break;
                    case 5: va = a.core; vb = b.core; break;
                    default: va = 0; vb = 0;
                }
                return asc ? va - vb : vb - va;
            });

            // Update sort arrows
            var ths = document.querySelectorAll('#taskTable thead th .sort-arrow');
            for (var i = 0; i < ths.length; i++) {
                ths[i].className = 'sort-arrow' + (i === col ? (asc ? ' asc' : ' desc') : '');
            }

            // Render rows
            var html = '';
            for (var i = 0; i < rows.length; i++) {
                var r = rows[i];
                var stackStr = '', barHtml = '', pctHtml = '';
                if (r.stackAlloc > 0) {
                    var freePct = 100 - r.usedPct;
                    var barClass = freePct > 50 ? 'task-stack-ok' : freePct > 25 ? 'task-stack-warn' : 'task-stack-crit';
                    var pctClass = freePct > 50 ? 'task-pct-ok' : freePct > 25 ? 'task-pct-warn' : 'task-pct-crit';
                    stackStr = formatBytes(r.stackFree) + '/' + formatBytes(r.stackAlloc);
                    barHtml = ' <span class="task-stack-bar ' + barClass + '" style="width:' + Math.max(r.usedPct, 4) + 'px" title="' + r.usedPct + '% used"></span>';
                    pctHtml = '<span class="task-pct-text ' + pctClass + '">' + r.usedPct + '%</span>';
                } else {
                    stackStr = formatBytes(r.stackFree);
                    pctHtml = '<span class="task-pct-text" style="opacity:0.4">--</span>';
                }
                var stateName = r.state < stateNames.length ? stateNames[r.state] : '?';
                html += '<tr><td>' + r.name + '</td><td>' + stackStr + barHtml + '</td><td>' + pctHtml + '</td><td>' + r.pri + '</td><td>' + stateName + '</td><td>' + r.core + '</td></tr>';
            }
            tbody.innerHTML = html;
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

        function formatResetReason(reason) {
            if (!reason) return '--';
            // The reset reason from the backend is already formatted as a readable string
            return reason;
        }

        function formatRssi(rssi) {
            if (rssi === undefined || rssi === null) return 'N/A';
            rssi = parseInt(rssi);
            let text, cls;
            if (rssi >= -50) { text = 'Excellent (90-100%)'; cls = 'text-success'; }
            else if (rssi >= -60) { text = 'Very Good (70-90%)'; cls = 'text-success'; }
            else if (rssi >= -70) { text = 'Fair (50-70%)'; cls = 'text-warning'; }
            else if (rssi >= -80) { text = 'Weak (30-50%)'; cls = 'text-error'; }
            else { text = 'Very Weak (0-30%)'; cls = 'text-error'; }
            return '<span class="' + cls + '">' + rssi + ' dBm - ' + text + '</span>';
        }

        function addHistoryDataPoint(data) {
            historyData.timestamps.push(Date.now());
            var c0 = data.cpu ? data.cpu.usageCore0 : 0;
            var c1 = data.cpu ? data.cpu.usageCore1 : 0;
            var ct = data.cpu ? data.cpu.usageTotal : 0;
            historyData.cpuTotal.push(ct >= 0 ? ct : 0);
            historyData.cpuCore0.push(c0 >= 0 ? c0 : 0);
            historyData.cpuCore1.push(c1 >= 0 ? c1 : 0);

            if (data.memory && data.memory.heapTotal > 0) {
                const memPercent = (1 - data.memory.heapFree / data.memory.heapTotal) * 100;
                historyData.memoryPercent.push(memPercent);
            } else {
                historyData.memoryPercent.push(0);
            }

            if (data.memory && data.memory.psramTotal > 0) {
                const psramPercent = (1 - data.memory.psramFree / data.memory.psramTotal) * 100;
                historyData.psramPercent.push(psramPercent);
            } else {
                historyData.psramPercent.push(0);
            }

            // Trim to max points
            while (historyData.timestamps.length > maxHistoryPoints) {
                historyData.timestamps.shift();
                historyData.cpuTotal.shift();
                historyData.cpuCore0.shift();
                historyData.cpuCore1.shift();
                historyData.memoryPercent.shift();
                historyData.psramPercent.shift();
            }

            // Always redraw graphs (they're always visible now)
            drawCpuGraph();
            drawMemoryGraph();
            drawPsramGraph();
        }

        // ===== Support / User Manual Section =====
        let manualQrGenerated = false;
        let manualContentLoaded = false;
        let manualRawMarkdown = '';
        const GITHUB_REPO_OWNER = 'Schmackos';
        const GITHUB_REPO_NAME = 'ALX_Nova_Controller_2';
        const MANUAL_URL = `https://github.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}/blob/main/USER_MANUAL.md`;
        const MANUAL_RAW_URL = `https://raw.githubusercontent.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}/main/USER_MANUAL.md`;

        function generateManualQRCode() {
            if (manualQrGenerated) return;

            const qrContainer = document.getElementById('manualQrCode');
            const manualLink = document.getElementById('manualLink');

            manualLink.href = MANUAL_URL;
            manualLink.textContent = MANUAL_URL;

            if (typeof QRCode !== 'undefined') {
                renderQR();
                return;
            }

            const script = document.createElement('script');
            script.src = 'https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js';
            script.onload = () => renderQR();
            script.onerror = () => {
                qrContainer.innerHTML = '<div style="color: var(--text-secondary); padding: 10px; font-size: 13px;">QR code library unavailable (offline)</div>';
            };
            document.head.appendChild(script);

            function renderQR() {
                try {
                    new QRCode(qrContainer, {
                        text: MANUAL_URL,
                        width: 180,
                        height: 180,
                        colorDark: '#000000',
                        colorLight: '#ffffff',
                        correctLevel: QRCode.CorrectLevel.M
                    });
                    manualQrGenerated = true;
                } catch (e) {
                    console.error('QR Generator Error:', e);
                }
            }
        }

        function loadManualContent() {
            if (manualContentLoaded) return;
            manualContentLoaded = true;

            const container = document.getElementById('manualRendered');
            container.innerHTML = '<div class="manual-loading">Loading manual...</div>';

            fetch(MANUAL_RAW_URL)
                .then(r => { if (!r.ok) throw new Error(r.status); return r.text(); })
                .then(md => {
                    manualRawMarkdown = md;
                    loadMarkedAndRender(md);
                })
                .catch(() => {
                    container.innerHTML = '<div class="manual-loading">Manual unavailable offline. Use the QR code above.</div>';
                    manualContentLoaded = false;
                });
        }

        function loadMarkedAndRender(md) {
            const container = document.getElementById('manualRendered');

            if (typeof marked !== 'undefined') {
                container.innerHTML = marked.parse(md);
                return;
            }

            const script = document.createElement('script');
            script.src = 'https://cdn.jsdelivr.net/npm/marked/marked.min.js';
            script.onload = () => {
                container.innerHTML = marked.parse(md);
            };
            script.onerror = () => {
                container.innerHTML = '<pre style="white-space: pre-wrap;">' + md.replace(/</g, '&lt;') + '</pre>';
            };
            document.head.appendChild(script);
        }

        function searchManual(query) {
            const container = document.getElementById('manualRendered');
            const status = document.getElementById('manualSearchStatus');

            if (!manualRawMarkdown || typeof marked === 'undefined') {
                status.textContent = '';
                return;
            }

            container.innerHTML = marked.parse(manualRawMarkdown);

            if (!query || query.length < 2) {
                status.textContent = '';
                return;
            }

            let count = 0;
            const regex = new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');

            function highlightNode(node) {
                if (node.nodeType === 3) {
                    const text = node.textContent;
                    if (regex.test(text)) {
                        regex.lastIndex = 0;
                        const span = document.createElement('span');
                        span.innerHTML = text.replace(regex, m => { count++; return '<span class="search-highlight">' + m + '</span>'; });
                        node.parentNode.replaceChild(span, node);
                    }
                } else if (node.nodeType === 1 && node.childNodes.length && !/(script|style)/i.test(node.tagName)) {
                    Array.from(node.childNodes).forEach(highlightNode);
                }
            }

            highlightNode(container);
            status.textContent = count > 0 ? count + ' match' + (count !== 1 ? 'es' : '') + ' found' : 'No matches';

            if (count > 0) {
                const first = container.querySelector('.search-highlight');
                if (first) first.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
        }

        function drawLineGraph(canvasId, lines, containerId) {
            const canvas = document.getElementById(canvasId);
            if (!canvas) return;

            if (containerId) {
                const container = document.getElementById(containerId);
                if (!container) return;
                const psramTotal = document.getElementById('psramTotal');
                if (!psramTotal || psramTotal.textContent === 'N/A') {
                    container.style.display = 'none';
                    return;
                }
                container.style.display = 'block';
            }

            const ctx = canvas.getContext('2d');
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * window.devicePixelRatio;
            canvas.height = rect.height * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

            const leftMargin = 35;
            const bottomMargin = 20;
            const w = rect.width - leftMargin;
            const h = rect.height - bottomMargin;

            ctx.fillStyle = '#1A1A1A';
            ctx.fillRect(0, 0, rect.width, rect.height);

            ctx.save();
            ctx.translate(leftMargin, 0);

            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            ctx.fillStyle = '#999';
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';

            for (let i = 0; i <= 4; i++) {
                const y = (h / 4) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
                ctx.fillText((100 - i * 25) + '%', -5, y);
            }

            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            [0, 0.5, 1].forEach((point, idx) => {
                ctx.fillText(['-60s', '-30s', 'now'][idx], w * point, h + 4);
            });

            if (lines[0].data.length < 2) {
                ctx.restore();
                return;
            }

            const step = w / (lines[0].data.length - 1);
            lines.forEach(line => {
                ctx.strokeStyle = line.color;
                ctx.lineWidth = line.width || 2;
                ctx.beginPath();
                line.data.forEach((val, i) => {
                    const x = i * step;
                    const y = h - (val / 100) * h;
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                });
                ctx.stroke();
            });

            ctx.restore();
        }

        function drawCpuGraph() {
            drawLineGraph('cpuGraph', [
                { data: historyData.cpuCore0, color: '#FFB74D', width: 1.5 },
                { data: historyData.cpuCore1, color: '#F57C00', width: 1.5 },
                { data: historyData.cpuTotal, color: '#FF9800', width: 2 }
            ]);
        }

        function drawMemoryGraph() {
            drawLineGraph('memoryGraph', [
                { data: historyData.memoryPercent, color: '#2196F3' }
            ]);
        }

        function drawPsramGraph() {
            drawLineGraph('psramGraph', [
                { data: historyData.psramPercent, color: '#9C27B0' }
            ], 'psramGraphContainer');
        }

        // ===== Debug Console =====
        function appendDebugLog(timestamp, message, level = 'info') {
            if (debugPaused) {
                debugLogBuffer.push({ timestamp, message, level });
                return;
            }

            // Determine log level from message if not provided
            let detectedLevel = level;
            if (message.includes('[E]') || message.includes('Error') || message.includes('❌')) {
                detectedLevel = 'error';
            } else if (message.includes('[W]') || message.includes('Warning') || message.includes('⚠')) {
                detectedLevel = 'warn';
            } else if (message.includes('[D]')) {
                detectedLevel = 'debug';
            } else if (message.includes('[I]') || message.includes('Info') || message.includes('ℹ')) {
                detectedLevel = 'info';
            }

            const console = document.getElementById('debugConsole');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = detectedLevel; // Store level for filtering

            const ts = formatDebugTimestamp(timestamp);
            let msgClass = 'log-message';
            if (detectedLevel === 'error') msgClass += ' error';
            else if (detectedLevel === 'warn') msgClass += ' warning';
            else if (detectedLevel === 'debug') msgClass += ' debug';
            else if (message.includes('✅') || message.includes('Success')) msgClass += ' success';
            else msgClass += ' info';

            entry.innerHTML = `<span class="log-timestamp">[${ts}]</span><span class="${msgClass}">${message}</span>`;

            // Apply filter visibility (but always add to DOM)
            if (currentLogFilter !== 'all' && detectedLevel !== currentLogFilter) {
                entry.style.display = 'none';
            }

            // Check if user is near the bottom before adding (within 40px)
            const wasAtBottom = (console.scrollHeight - console.scrollTop - console.clientHeight) < 40;

            console.appendChild(entry);

            // Limit entries
            while (console.children.length > DEBUG_MAX_LINES) {
                console.removeChild(console.firstChild);
            }

            // Only auto-scroll if user was already at the bottom
            if (wasAtBottom && entry.style.display !== 'none') {
                console.scrollTop = console.scrollHeight;
            }
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
                debugLogBuffer.forEach(log => appendDebugLog(log.timestamp, log.message, log.level));
                debugLogBuffer = [];
            }
        }

        // ===== Pin Table Sorting =====
        let pinSortCol = 0;
        let pinSortAsc = true;
        function sortPinTable(col) {
            const table = document.getElementById('pinTable');
            const tbody = table.querySelector('tbody');
            const rows = Array.from(tbody.querySelectorAll('tr'));
            if (col === pinSortCol) { pinSortAsc = !pinSortAsc; } else { pinSortCol = col; pinSortAsc = true; }
            rows.sort((a, b) => {
                let aVal = a.cells[col].textContent.trim();
                let bVal = b.cells[col].textContent.trim();
                if (col === 0) { return pinSortAsc ? parseInt(aVal) - parseInt(bVal) : parseInt(bVal) - parseInt(aVal); }
                return pinSortAsc ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
            });
            rows.forEach(r => tbody.appendChild(r));
            table.querySelectorAll('th').forEach(th => {
                th.classList.remove('sorted');
                th.querySelector('.sort-arrow').innerHTML = '&#9650;';
            });
            const th = table.querySelectorAll('th')[col];
            th.classList.add('sorted');
            th.querySelector('.sort-arrow').innerHTML = pinSortAsc ? '&#9650;' : '&#9660;';
        }

        function clearDebugConsole() {
            const console = document.getElementById('debugConsole');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = 'info';
            entry.innerHTML = '<span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span>';

            console.innerHTML = '';
            console.appendChild(entry);
            debugLogBuffer = [];
        }

        function setLogFilter(level) {
            currentLogFilter = level;

            // Apply filter to all console entries
            const console = document.getElementById('debugConsole');
            const allEntries = Array.from(console.children);

            allEntries.forEach(entry => {
                const entryLevel = entry.dataset.level;
                if (level === 'all' || entryLevel === level) {
                    entry.style.display = '';
                } else {
                    entry.style.display = 'none';
                }
            });

            // Auto-scroll to bottom after filtering
            console.scrollTop = console.scrollHeight;
        }

        function downloadDebugLog() {
            const console = document.getElementById('debugConsole');
            const entries = Array.from(console.children);

            if (entries.length === 0) {
                showToast('No logs to download', 'warning');
                return;
            }

            // Build log content
            let logContent = '=== Debug Log Export ===\n';
            logContent += `Exported: ${new Date().toISOString()}\n`;
            logContent += `Device: ${currentAPSSID || 'Unknown'}\n`;
            logContent += `Firmware: ${currentFirmwareVersion || 'Unknown'}\n`;
            logContent += `Total Entries: ${entries.length}\n`;
            logContent += `Filter: ${currentLogFilter}\n`;
            logContent += '========================\n\n';

            entries.forEach(entry => {
                // Only include visible entries (respects current filter)
                if (entry.style.display !== 'none') {
                    const text = entry.textContent || entry.innerText;
                    logContent += text + '\n';
                }
            });

            // Create blob and download
            const blob = new Blob([logContent], { type: 'text/plain' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.style.display = 'none';
            a.href = url;

            // Generate filename with timestamp
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
            const deviceName = currentAPSSID.replace(/[^a-zA-Z0-9]/g, '_') || 'device';
            a.download = `${deviceName}_debug_log_${timestamp}.txt`;

            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);

            showToast(`Log downloaded (${entries.filter(e => e.style.display !== 'none').length} entries)`, 'success');
        }

        function downloadDiagnostics() {
            showToast('Generating diagnostics...', 'info');
            apiFetch('/api/diagnostics')
                .then(res => {
                    if (!res.ok) throw new Error('Failed to generate diagnostics');
                    return res.blob();
                })
                .then(blob => {
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.style.display = 'none';
                    a.href = url;
                    // Generate filename with timestamp
                    const now = new Date();
                    const timestamp = now.toISOString().replace(/[:.]/g, '-').slice(0, -5);
                    a.download = `diagnostics-${timestamp}.json`;
                    document.body.appendChild(a);
                    a.click();
                    window.URL.revokeObjectURL(url);
                    document.body.removeChild(a);
                    showToast('Diagnostics downloaded', 'success');
                })
                .catch(err => {
                    console.error('Download error:', err);
                    showToast('Failed to download diagnostics', 'error');
                });
        }

        // ===== Release Notes =====
        function showReleaseNotes() {
            showReleaseNotesFor('latest');
        }

        function showReleaseNotesFor(which) {
            let version = which === 'current' ? currentFirmwareVersion : currentLatestVersion;
            
            if (!version) {
                showToast('Version information not available', 'error');
                return;
            }
            
            // If already open with the same version, toggle it closed
            const container = document.getElementById('inlineReleaseNotes');
            const currentShownVersion = container.dataset.version;
            
            if (container.classList.contains('open') && currentShownVersion === version) {
                toggleInlineReleaseNotes(false);
                return;
            }

            // Fetch and show
            apiFetch(`/api/releasenotes?version=${version}`)
            .then(res => res.json())
            .then(data => {
                const label = which === 'current' ? 'Current' : 'Latest';
                document.getElementById('inlineReleaseNotesTitle').textContent = `Release Notes v${version} (${label})`;
                document.getElementById('inlineReleaseNotesContent').textContent = data.notes || 'No release notes available for this version.';
                
                container.dataset.version = version;
                toggleInlineReleaseNotes(true);
            })
            .catch(err => showToast('Failed to load release notes', 'error'));
        }

        function toggleInlineReleaseNotes(show) {
            const container = document.getElementById('inlineReleaseNotes');
            if (show) {
                container.classList.add('open');
            } else {
                container.classList.remove('open');
            }
        }
        
        // Remove old modal functions if needed, but keeping utilities below

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

        // ===== Window Resize Handler =====
        let resizeTimeout;
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(function() {
                canvasDims = {};
                invalidateBgCache();
                drawCpuGraph();
                drawMemoryGraph();
                drawPsramGraph();
                dspDrawFreqResponse();
            }, 250);
        });

        // ===== Initialization =====
        window.onload = function() {
            initWebSocket();
            loadMqttSettings();
            initFirmwareDragDrop();
            initSidebar();
            loadSavedNetworks();

            // Add input focus listeners
            document.getElementById('appState.timerDuration').addEventListener('focus', () => inputFocusState.timerDuration = true);
            document.getElementById('appState.timerDuration').addEventListener('blur', () => inputFocusState.timerDuration = false);
            document.getElementById('audioThreshold').addEventListener('focus', () => inputFocusState.audioThreshold = true);
            document.getElementById('audioThreshold').addEventListener('blur', () => inputFocusState.audioThreshold = false);

            // Restore VU meter mode from localStorage
            if (vuSegmentedMode) {
                document.getElementById('vuSegmented').checked = true;
                toggleVuMode(true);
            }

            // Fetch input names
            apiFetch('/api/inputnames')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.names && Array.isArray(d.names)) {
                        for (var i = 0; i < d.names.length && i < NUM_ADCS * 2; i++) inputNames[i] = d.names[i];
                        applyInputNames();
                    }
                })
                .catch(function() {});

            // Initial status bar update
            updateStatusBar(false, null, false, false);

            // Check if settings tab is active and start time updates
            const activePanel = document.querySelector('.panel.active');
            if (activePanel && activePanel.id === 'settings') {
                startTimeUpdates();
            }

            // Check for default password warning
            checkPasswordWarning();
        };

        // Authentication Helper Functions moved/unified at top of script

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

        
        // ===== AP Configuration Modal =====
        function openAPConfig() {
            document.getElementById('apConfigModal').style.display = 'flex';
            // Load current AP settings if available (optional, requires API support)
        }

        function closeAPConfig() {
            document.getElementById('apConfigModal').style.display = 'none';
        }

        function submitAPConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('appState.apSSID').value;
            const password = document.getElementById('appState.apPassword').value;
            
            if (password && password.length < 8) {
                showToast('Password must be at least 8 characters', 'error');
                return;
            }

            apiFetch('/api/ap/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid: ssid, password: password })
            })
            .then(res => {
                if (res.ok) {
                    showToast('AP settings saved. Reboot required.', 'success');
                    closeAPConfig();
                } else {
                    showToast('Failed to save AP settings', 'error');
                }
            })
            .catch(err => showToast('Error saving AP settings', 'error'));
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

        // ===== DSP Tab =====
        const DSP_TYPES = ['LPF','HPF','BPF','Notch','PEQ','Low Shelf','High Shelf','Allpass','AP360','AP180','BPF0dB','Custom','Limiter','FIR','Gain','Delay','Polarity','Mute','Compressor','LPF 1st','HPF 1st','Linkwitz','Decimator','Convolution','Noise Gate','Tone Controls','Speaker Prot','Stereo Width','Loudness','Bass Enhance','Multiband Comp'];
        const DSP_MAX_CH = 4;
        const DSP_CH_NAMES = ['L1','R1','L2','R2'];
        function dspChLabel(c) { return inputNames[c] || DSP_CH_NAMES[c]; }
        let dspState = null;
        let dspCh = 0; // selected channel
        let dspOpenStage = -1; // expanded stage index
        let dspImportMode = ''; // 'apo' or 'json'

        // ===== PEQ State =====
        const DSP_PEQ_BANDS = 10;
        const PEQ_COLORS = ['#F44336','#E91E63','#9C27B0','#3F51B5','#2196F3','#00BCD4','#4CAF50','#8BC34A','#FFC107','#FF5722'];
        const PEQ_FILTER_TYPES = [
            {value:4,label:'PEQ'},{value:5,label:'Low Shelf'},{value:6,label:'High Shelf'},
            {value:3,label:'Notch'},{value:2,label:'BPF'},{value:0,label:'LPF'},
            {value:1,label:'HPF'},{value:7,label:'Allpass'}
        ];
        let peqSelectedBand = 0;
        let peqLinked = false;
        let peqGraphLayers = { individual: true, rta: false, chain: true };
        let peqRtaData = null;
        let peqDragging = null;
        let peqCanvasInited = false;

        function peqGetBands() {
            if (!dspState || !dspState.channels[dspCh]) return [];
            return (dspState.channels[dspCh].stages || []).slice(0, DSP_PEQ_BANDS);
        }
        function peqRenderBandStrip() {
            var el = document.getElementById('peqBandStrip');
            if (!el) return;
            var bands = peqGetBands();
            var html = '';
            for (var i = 0; i < DSP_PEQ_BANDS; i++) {
                var b = bands[i];
                var active = (i === peqSelectedBand);
                var enabled = b && b.enabled;
                html += '<button class="peq-band-pill' + (active ? ' active' : '') + (enabled ? ' enabled' : '') + '" onclick="peqSelectBand(' + i + ')" style="--band-color:' + PEQ_COLORS[i] + ';">' + (i + 1) + '</button>';
            }
            el.innerHTML = html;
        }
        function peqSelectBand(band) {
            peqSelectedBand = band;
            peqRenderBandStrip();
            peqRenderBandDetail();
            dspDrawFreqResponse();
        }
        function peqRenderBandDetail() {
            var el = document.getElementById('peqBandDetail');
            if (!el || !dspState || !dspState.channels[dspCh]) return;
            var bands = peqGetBands();
            var b = bands[peqSelectedBand];
            if (!b) { el.innerHTML = ''; return; }
            var t = b.type;
            var hasGain = (t === 4 || t === 5 || t === 6);
            var html = '<div class="peq-detail-panel" style="border-left:3px solid ' + PEQ_COLORS[peqSelectedBand] + ';">';
            html += '<div style="display:flex;align-items:center;gap:8px;margin-bottom:8px;">';
            html += '<label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (b.enabled ? 'checked' : '') + ' onchange="peqSetBandEnabled(' + peqSelectedBand + ',this.checked)"><span class="slider round"></span></label>';
            html += '<select class="select-sm" onchange="peqSetBandType(' + peqSelectedBand + ',parseInt(this.value))">';
            for (var fi = 0; fi < PEQ_FILTER_TYPES.length; fi++) {
                var ft = PEQ_FILTER_TYPES[fi];
                html += '<option value="' + ft.value + '"' + (t === ft.value ? ' selected' : '') + '>' + ft.label + '</option>';
            }
            html += '</select>';
            html += '<button class="btn btn-secondary" style="padding:2px 8px;font-size:11px;margin-left:auto;" onclick="peqResetBand(' + peqSelectedBand + ')">Reset</button>';
            html += '<span style="font-size:11px;color:var(--text-secondary);">Band ' + (peqSelectedBand + 1) + '</span>';
            html += '</div>';
            html += peqSlider('freq', 'Frequency', b.freq || 1000, 5, 20000, 1, 'Hz');
            if (hasGain) html += peqSlider('gain', 'Gain', b.gain || 0, -24, 24, 0.5, 'dB');
            html += peqSlider('Q', 'Q Factor', b.Q || 0.707, 0.1, 25, 0.01, '');
            html += '</div>';
            el.innerHTML = html;
        }
        function peqSlider(key, label, val, min, max, step, unit) {
            var numVal = parseFloat(val) || 0;
            var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
            var id = 'peq_' + peqSelectedBand + '_' + key;
            return '<div class="dsp-param"><label>' + label + '</label>' +
                '<button class="dsp-step-btn" onclick="peqParamStep(\'' + key + '\',' + (-step) + ',' + min + ',' + max + ',' + step + ')">&lsaquo;</button>' +
                '<input type="range" id="' + id + '_s" min="' + min + '" max="' + max + '" step="' + step + '" value="' + numVal + '" ' +
                'oninput="document.getElementById(\'' + id + '_n\').value=parseFloat(this.value).toFixed(' + dec + ')" ' +
                'onchange="peqParamSync(\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
                '<button class="dsp-step-btn" onclick="peqParamStep(\'' + key + '\',' + step + ',' + min + ',' + max + ',' + step + ')">&rsaquo;</button>' +
                '<input type="number" class="dsp-num-input" id="' + id + '_n" value="' + numVal.toFixed(dec) + '" min="' + min + '" max="' + max + '" step="' + step + '" ' +
                'onchange="peqParamSync(\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
                '<span class="dsp-unit">' + unit + '</span></div>';
        }
        function peqParamSync(key, val, min, max, step) {
            val = Math.min(max, Math.max(min, parseFloat(val) || 0));
            peqUpdateBandParam(peqSelectedBand, key, val);
        }
        function peqParamStep(key, delta, min, max, step) {
            var id = 'peq_' + peqSelectedBand + '_' + key;
            var sl = document.getElementById(id + '_s');
            var cur = sl ? parseFloat(sl.value) : 0;
            var newVal = Math.min(max, Math.max(min, cur + delta));
            var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
            if (sl) sl.value = newVal;
            var ni = document.getElementById(id + '_n');
            if (ni) ni.value = newVal.toFixed(dec);
            peqUpdateBandParam(peqSelectedBand, key, newVal);
        }
        function peqUpdateBandParam(band, key, val) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            var bands = peqGetBands();
            var b = bands[band];
            if (!b) return;
            var msg = { type: 'updatePeqBand', ch: dspCh, band: band,
                freq: b.freq || 1000, gain: b.gain || 0, Q: b.Q || 0.707,
                filterType: b.type, enabled: b.enabled };
            if (key === 'filterType') msg.filterType = val;
            else msg[key] = val;
            ws.send(JSON.stringify(msg));
            if (peqLinked) {
                ws.send(JSON.stringify(Object.assign({}, msg, { ch: dspCh ^ 1 })));
            }
        }
        function peqSetBandEnabled(band, en) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            ws.send(JSON.stringify({ type: 'setPeqBandEnabled', ch: dspCh, band: band, enabled: en }));
            if (peqLinked) ws.send(JSON.stringify({ type: 'setPeqBandEnabled', ch: dspCh ^ 1, band: band, enabled: en }));
        }
        function peqSetBandType(band, typeInt) {
            peqUpdateBandParam(band, 'filterType', typeInt);
        }
        // Equal logarithmic spacing from 20 Hz to 20 kHz (10 bands)
        const PEQ_DEFAULT_FREQS = [20,43,93,200,430,930,2000,4300,9300,20000];
        function peqResetBand(band) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            var msg = { type: 'updatePeqBand', ch: dspCh, band: band,
                freq: PEQ_DEFAULT_FREQS[band] || 1000, gain: 0, Q: 1.0, filterType: 4, enabled: true };
            ws.send(JSON.stringify(msg));
            if (peqLinked) ws.send(JSON.stringify(Object.assign({}, msg, { ch: dspCh ^ 1 })));
        }
        function peqToggleLink() {
            peqLinked = !peqLinked;
            var btn = document.getElementById('peqLinkBtn');
            if (btn) {
                btn.classList.toggle('active', peqLinked);
                btn.style.background = peqLinked ? 'var(--accent)' : '';
                btn.style.color = peqLinked ? '#fff' : '';
            }
        }
        function peqToggleAll() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            var bands = peqGetBands();
            var allEnabled = bands.length > 0 && bands.every(function(b) { return b && b.enabled; });
            var en = !allEnabled;
            ws.send(JSON.stringify({ type: 'setPeqAllEnabled', ch: dspCh, enabled: en }));
            if (peqLinked) ws.send(JSON.stringify({ type: 'setPeqAllEnabled', ch: dspCh ^ 1, enabled: en }));
        }
        function peqUpdateToggleAllBtn() {
            var btn = document.getElementById('peqToggleAllBtn');
            if (!btn) return;
            var bands = peqGetBands();
            var allEnabled = bands.length > 0 && bands.every(function(b) { return b && b.enabled; });
            btn.textContent = allEnabled ? 'Disable All' : 'Enable All';
        }
        function peqCopyChannel(target) {
            if (!target || !ws || ws.readyState !== WebSocket.OPEN) return;
            if (target === 'all') {
                for (var c = 0; c < DSP_MAX_CH; c++) {
                    if (c !== dspCh) ws.send(JSON.stringify({ type: 'copyPeqChannel', from: dspCh, to: c }));
                }
            } else {
                ws.send(JSON.stringify({ type: 'copyPeqChannel', from: dspCh, to: parseInt(target) }));
            }
        }
        function dspCopyChainChannel(target) {
            if (!target || !ws || ws.readyState !== WebSocket.OPEN) return;
            if (target === 'all') {
                for (var c = 0; c < DSP_MAX_CH; c++) {
                    if (c !== dspCh) ws.send(JSON.stringify({ type: 'copyChainStages', from: dspCh, to: c }));
                }
            } else {
                ws.send(JSON.stringify({ type: 'copyChainStages', from: dspCh, to: parseInt(target) }));
            }
        }
        function updateChainCopyToDropdown() {
            var sel = document.getElementById('chainCopyTo');
            if (!sel) return;
            var html = '<option value="">Copy to...</option>';
            for (var i = 0; i < DSP_MAX_CH; i++) {
                var name = inputNames[i] || DSP_CH_NAMES[i];
                html += '<option value="' + i + '">' + name + '</option>';
            }
            html += '<option value="all">All Channels</option>';
            sel.innerHTML = html;
        }
        function peqPresetAction(val) {
            if (!val) return;
            if (val === '_save') {
                var name = prompt('Preset name (max 20 chars):');
                if (!name) return;
                if (ws && ws.readyState === WebSocket.OPEN)
                    ws.send(JSON.stringify({ type: 'savePeqPreset', ch: dspCh, name: name.substring(0, 20) }));
            } else if (val === '_load') {
                if (ws && ws.readyState === WebSocket.OPEN)
                    ws.send(JSON.stringify({ type: 'listPeqPresets' }));
            } else {
                if (ws && ws.readyState === WebSocket.OPEN)
                    ws.send(JSON.stringify({ type: 'loadPeqPreset', ch: dspCh, name: val }));
            }
        }
        function peqToggleGraphLayer(layer) {
            peqGraphLayers[layer] = !peqGraphLayers[layer];
            var btn = document.getElementById('tog' + layer.charAt(0).toUpperCase() + layer.slice(1));
            if (btn) btn.classList.toggle('active', peqGraphLayers[layer]);
            if (layer === 'rta' && ws && ws.readyState === WebSocket.OPEN) {
                if (peqGraphLayers.rta) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                    ws.send(JSON.stringify({ type: 'setSpectrumEnabled', enabled: true }));
                } else if (!audioSubscribed) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
            }
            dspDrawFreqResponse();
        }
        function peqHandlePresetsList(presets) {
            var sel = document.getElementById('peqPresetSel');
            if (!sel) return;
            var html = '<option value="">Presets...</option><option value="_save">Save Preset...</option><option value="_load">Refresh List</option>';
            if (presets && presets.length > 0) {
                html += '<option disabled>──────────</option>';
                for (var i = 0; i < presets.length; i++) {
                    html += '<option value="' + presets[i] + '">' + presets[i] + '</option>';
                }
            }
            sel.innerHTML = html;
            showToast('Found ' + (presets ? presets.length : 0) + ' presets');
        }

        // ===== PEQ Canvas Interaction (drag control points) =====
        function peqInitCanvas() {
            if (peqCanvasInited) return;
            var canvas = document.getElementById('dspFreqCanvas');
            if (!canvas) return;
            peqCanvasInited = true;
            canvas.addEventListener('mousedown', peqCanvasMouseDown);
            canvas.addEventListener('mousemove', peqCanvasMouseMove);
            canvas.addEventListener('mouseup', peqCanvasMouseUp);
            canvas.addEventListener('mouseleave', peqCanvasMouseUp);
            canvas.addEventListener('touchstart', peqCanvasTouchStart, { passive: false });
            canvas.addEventListener('touchmove', peqCanvasTouchMove, { passive: false });
            canvas.addEventListener('touchend', peqCanvasMouseUp);
        }
        function peqCanvasCoords(canvas, clientX, clientY) {
            var rect = canvas.getBoundingClientRect();
            return { x: clientX - rect.left, y: clientY - rect.top };
        }
        function peqCanvasToFreqGain(canvas, x, y) {
            var dpr = window.devicePixelRatio || 1;
            var dims = canvasDims[canvas.id];
            if (!dims) return null;
            var w = dims.tw / dpr, h = dims.th / dpr;
            var padL = 35, padR = 10, padT = 10, padB = 20;
            var gw = w - padL - padR, gh = h - padT - padB;
            var logMin = Math.log10(5), logRange = Math.log10(24000) - logMin;
            var normX = (x - padL) / gw;
            var normY = (y - padT) / gh;
            var freq = Math.pow(10, logMin + logRange * normX);
            var gain = 24 - normY * 48;
            return { freq: Math.max(5, Math.min(20000, Math.round(freq))), gain: Math.max(-24, Math.min(24, Math.round(gain * 2) / 2)) };
        }
        function peqFreqGainToCanvas(canvas, freq, gain) {
            var dpr = window.devicePixelRatio || 1;
            var dims = canvasDims[canvas.id];
            if (!dims) return null;
            var w = dims.tw / dpr, h = dims.th / dpr;
            var padL = 35, padR = 10, padT = 10, padB = 20;
            var gw = w - padL - padR, gh = h - padT - padB;
            var logMin = Math.log10(5), logRange = Math.log10(24000) - logMin;
            return {
                x: padL + gw * (Math.log10(freq) - logMin) / logRange,
                y: padT + gh * (1 - (gain + 24) / 48)
            };
        }
        function peqFindNearestBand(canvas, x, y, maxDist) {
            var bands = peqGetBands();
            var closest = -1, closestDist = maxDist || 25;
            for (var i = 0; i < bands.length; i++) {
                var b = bands[i];
                var pos = peqFreqGainToCanvas(canvas, b.freq || 1000, b.gain || 0);
                if (!pos) continue;
                var dist = Math.sqrt(Math.pow(x - pos.x, 2) + Math.pow(y - pos.y, 2));
                if (dist < closestDist) { closestDist = dist; closest = i; }
            }
            return closest;
        }
        function peqCanvasMouseDown(e) {
            var canvas = e.target;
            var pos = peqCanvasCoords(canvas, e.clientX, e.clientY);
            var band = peqFindNearestBand(canvas, pos.x, pos.y, 20);
            if (band >= 0) {
                peqSelectBand(band);
                peqDragging = { band: band };
                canvas.style.cursor = 'grabbing';
                e.preventDefault();
            } else {
                var nearest = peqFindNearestBand(canvas, pos.x, pos.y, Infinity);
                if (nearest >= 0) peqSelectBand(nearest);
            }
        }
        function peqCanvasMouseMove(e) {
            var canvas = e.target;
            var pos = peqCanvasCoords(canvas, e.clientX, e.clientY);
            if (!peqDragging) {
                var hover = peqFindNearestBand(canvas, pos.x, pos.y, 20);
                canvas.style.cursor = hover >= 0 ? 'grab' : 'crosshair';
                return;
            }
            var fg = peqCanvasToFreqGain(canvas, pos.x, pos.y);
            if (!fg) return;
            var b = peqDragging.band;
            var bands = peqGetBands();
            if (bands[b]) {
                bands[b].freq = fg.freq;
                bands[b].gain = fg.gain;
            }
            var freqSl = document.getElementById('peq_' + b + '_freq_s');
            var freqNi = document.getElementById('peq_' + b + '_freq_n');
            var gainSl = document.getElementById('peq_' + b + '_gain_s');
            var gainNi = document.getElementById('peq_' + b + '_gain_n');
            if (freqSl) freqSl.value = fg.freq;
            if (freqNi) freqNi.value = fg.freq;
            if (gainSl) gainSl.value = fg.gain.toFixed(1);
            if (gainNi) gainNi.value = fg.gain.toFixed(1);
            dspDrawFreqResponse();
            e.preventDefault();
        }
        function peqCanvasMouseUp(e) {
            if (!peqDragging) return;
            var canvas = e.target || document.getElementById('dspFreqCanvas');
            canvas.style.cursor = 'crosshair';
            var b = peqDragging.band;
            peqDragging = null;
            var bands = peqGetBands();
            if (bands[b]) {
                peqUpdateBandParam(b, 'freq', bands[b].freq);
                setTimeout(function() { peqUpdateBandParam(b, 'gain', bands[b].gain); }, 10);
            }
        }
        function peqCanvasTouchStart(e) {
            if (e.touches.length !== 1) return;
            var canvas = e.target;
            var touch = e.touches[0];
            var pos = peqCanvasCoords(canvas, touch.clientX, touch.clientY);
            var band = peqFindNearestBand(canvas, pos.x, pos.y, 30);
            if (band >= 0) {
                peqSelectBand(band);
                peqDragging = { band: band };
                e.preventDefault();
            }
        }
        function peqCanvasTouchMove(e) {
            if (!peqDragging || e.touches.length !== 1) return;
            var canvas = e.target;
            var touch = e.touches[0];
            var pos = peqCanvasCoords(canvas, touch.clientX, touch.clientY);
            var fg = peqCanvasToFreqGain(canvas, pos.x, pos.y);
            if (!fg) return;
            var b = peqDragging.band;
            var bands = peqGetBands();
            if (bands[b]) {
                bands[b].freq = fg.freq;
                bands[b].gain = fg.gain;
            }
            var freqSl = document.getElementById('peq_' + b + '_freq_s');
            var freqNi = document.getElementById('peq_' + b + '_freq_n');
            var gainSl = document.getElementById('peq_' + b + '_gain_s');
            var gainNi = document.getElementById('peq_' + b + '_gain_n');
            if (freqSl) freqSl.value = fg.freq;
            if (freqNi) freqNi.value = fg.freq;
            if (gainSl) gainSl.value = fg.gain.toFixed(1);
            if (gainNi) gainNi.value = fg.gain.toFixed(1);
            dspDrawFreqResponse();
            e.preventDefault();
        }

        function dspSetEnabled(en) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'setDspBypass', enabled: en, bypass: dspState ? dspState.dspBypass : false }));
        }
        function dspSetBypass(bp) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'setDspBypass', enabled: dspState ? dspState.dspEnabled : false, bypass: bp }));
        }
        function dspSetChBypass(bp) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'setDspChannelBypass', ch: dspCh, bypass: bp }));
        }
        function dspAddStage(typeInt) {
            document.getElementById('dspAddMenu').classList.remove('open');
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'addDspStage', ch: dspCh, stageType: typeInt }));
        }
        function dspAddDCBlock() {
            dspToggleAddMenu();
            fetch('/api/dsp/crossover?ch=' + dspCh, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ freq: 10, type: 'bw2', role: 1 })
            })
            .then(r => r.json())
            .then(d => { if (d.success) showToast('DC Block added (10 Hz HPF)'); else showToast('Failed: ' + (d.message || ''), true); })
            .catch(err => showToast('Error: ' + err, true));
        }
        function dspRemoveStage(idx) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'removeDspStage', ch: dspCh, stage: idx }));
        }
        function dspMoveStage(from, to) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'reorderDspStage', ch: dspCh, from: from, to: to }));
        }
        function dspToggleAddMenu() {
            document.getElementById('dspAddMenu').classList.toggle('open');
        }
        function dspUpdateParam(idx, key, val) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                var msg = { type: 'updateDspStage', ch: dspCh, stage: idx };
                msg[key] = val;
                ws.send(JSON.stringify(msg));
            }
        }
        function dspToggleStageEnabled(idx, en) {
            dspUpdateParam(idx, 'enabled', en);
        }

        function dspSelectChannel(ch) {
            dspCh = ch;
            dspOpenStage = -1;
            peqSelectedBand = 0;
            dspRenderChannelTabs();
            peqRenderBandStrip();
            peqRenderBandDetail();
            peqUpdateToggleAllBtn();
            dspRenderStages();
            dspDrawFreqResponse();
        }

        function dspRenderChannelTabs() {
            var el = document.getElementById('dspChTabs');
            if (!el || !dspState) return;
            var html = '';
            for (var c = 0; c < DSP_MAX_CH; c++) {
                var ch = dspState.channels[c];
                var peqActive = 0, chainCount = 0;
                if (ch && ch.stages) {
                    for (var i = 0; i < Math.min(DSP_PEQ_BANDS, ch.stages.length); i++) {
                        if (ch.stages[i] && ch.stages[i].enabled) peqActive++;
                    }
                    chainCount = Math.max(0, (ch.stageCount || 0) - DSP_PEQ_BANDS);
                }
                var badge = peqActive + 'P' + (chainCount > 0 ? ' ' + chainCount + 'C' : '');
                html += '<button class="dsp-ch-tab' + (c === dspCh ? ' active' : '') + '" onclick="dspSelectChannel(' + c + ')">' + dspChLabel(c) + '<span class="badge">' + badge + '</span></button>';
            }
            el.innerHTML = html;
            var byp = document.getElementById('dspChBypassToggle');
            if (byp && dspState.channels[dspCh]) byp.checked = dspState.channels[dspCh].bypass;
        }

        function dspIsBiquad(t) { return t <= 11 || t === 19 || t === 20 || t === 21; }
        function dspStageSummary(s) {
            var t = s.type;
            if (t === 21) return 'F0=' + (s.freq || 50).toFixed(0) + ' Q0=' + (s.Q || 0.707).toFixed(2) + ' Fp=' + (s.gain || 25).toFixed(0) + ' Qp=' + (s.Q2 || 0.5).toFixed(2);
            if (dspIsBiquad(t)) return (s.freq || 1000).toFixed(0) + ' Hz' + (s.gain ? ' ' + (s.gain > 0 ? '+' : '') + s.gain.toFixed(1) + ' dB' : '') + ' Q=' + (s.Q || 0.707).toFixed(2);
            if (t === 12) return s.thresholdDb.toFixed(1) + ' dBFS ' + s.ratio.toFixed(0) + ':1';
            if (t === 13) return s.numTaps + ' taps';
            if (t === 14) return (s.gainDb > 0 ? '+' : '') + s.gainDb.toFixed(1) + ' dB';
            if (t === 15) return s.delaySamples + ' smp';
            if (t === 16) return s.inverted ? 'Inverted' : 'Normal';
            if (t === 17) return s.muted ? 'Muted' : 'Active';
            if (t === 18) return s.thresholdDb.toFixed(1) + ' dBFS ' + s.ratio.toFixed(1) + ':1';
            if (t === 24) return (s.thresholdDb || -40).toFixed(0) + ' dB ' + (s.ratio || 1).toFixed(0) + ':1';
            if (t === 25) return 'B' + (s.bassGain || 0).toFixed(0) + ' M' + (s.midGain || 0).toFixed(0) + ' T' + (s.trebleGain || 0).toFixed(0);
            if (t === 26) return (s.currentTempC || 25).toFixed(0) + '\u00B0C GR=' + (s.gr || 0).toFixed(1);
            if (t === 27) return 'W=' + (s.width || 100).toFixed(0) + '%';
            if (t === 28) return 'Ref=' + (s.referenceLevelDb || 85).toFixed(0) + ' Cur=' + (s.currentLevelDb || 75).toFixed(0);
            if (t === 29) return (s.frequency || 80).toFixed(0) + ' Hz mix=' + (s.mix || 50).toFixed(0) + '%';
            if (t === 30) return (s.numBands || 3) + ' bands';
            return '';
        }

        function dspParamSliders(idx, s) {
            var t = s.type;
            var h = '';
            if (t === 21) {
                h += dspSlider(idx, 'freq', 'F0 (Speaker Fs)', s.freq || 50, 20, 200, 1, 'Hz');
                h += dspSlider(idx, 'Q', 'Q0 (Speaker Qts)', s.Q || 0.707, 0.1, 2.0, 0.01, '');
                h += dspSlider(idx, 'gain', 'Fp (Target Fs)', s.gain || 25, 10, 100, 1, 'Hz');
                h += dspSlider(idx, 'Q2', 'Qp (Target Qts)', s.Q2 || 0.5, 0.1, 2.0, 0.01, '');
            } else if (t <= 11 || t === 19 || t === 20) {
                h += dspSlider(idx, 'freq', 'Frequency', s.freq || 1000, 5, 20000, 1, 'Hz');
                if (t === 4 || t === 5 || t === 6) h += dspSlider(idx, 'gain', 'Gain', s.gain || 0, -24, 24, 0.5, 'dB');
                if (t !== 19 && t !== 20) h += dspSlider(idx, 'Q', 'Q Factor', s.Q || 0.707, 0.1, 20, 0.01, '');
            } else if (t === 18) {
                var cHook = ';dspDrawCompressorGraph(' + idx + ')';
                h += '<div class="comp-graph-wrap"><canvas id="compCanvas_' + idx + '" height="180"></canvas></div>';
                h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -60, 0, 0.5, 'dBFS', cHook);
                h += dspSlider(idx, 'ratio', 'Ratio', s.ratio, 1, 100, 0.5, ':1', cHook);
                h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
                h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 1000, 1, 'ms');
                h += dspSlider(idx, 'kneeDb', 'Knee', s.kneeDb, 0, 24, 0.5, 'dB', cHook);
                h += dspSlider(idx, 'makeupGainDb', 'Makeup', s.makeupGainDb, 0, 24, 0.5, 'dB', cHook);
                var gr = s.gr !== undefined ? s.gr : 0;
                h += '<div class="comp-gr-wrap"><label>GR</label><div class="comp-gr-track"><div class="comp-gr-fill" id="compGr_' + idx + '" style="width:' + Math.min(100, Math.abs(gr) / 24 * 100).toFixed(1) + '%"></div></div><span class="comp-gr-val" id="compGrVal_' + idx + '">' + gr.toFixed(1) + ' dB</span></div>';
            } else if (t === 12) {
                h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -60, 0, 0.5, 'dBFS');
                h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
                h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 1000, 1, 'ms');
                h += dspSlider(idx, 'ratio', 'Ratio', s.ratio, 1, 100, 0.5, ':1');
                if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
            } else if (t === 14) {
                h += dspSlider(idx, 'gainDb', 'Gain', s.gainDb, -60, 24, 0.5, 'dB');
            } else if (t === 15) {
                h += dspSlider(idx, 'delaySamples', 'Delay', s.delaySamples, 0, 4800, 1, 'smp');
                var ms = (s.delaySamples / (dspState.sampleRate || 48000) * 1000).toFixed(2);
                h += '<div class="dsp-param"><label>Time</label><span class="dsp-val">' + ms + ' ms</span></div>';
            } else if (t === 16) {
                h += '<div class="dsp-param"><label>Invert</label><label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (s.inverted ? 'checked' : '') + ' onchange="dspUpdateParam(' + idx + ',\'inverted\',this.checked)"><span class="slider round"></span></label></div>';
            } else if (t === 17) {
                h += '<div class="dsp-param"><label>Mute</label><label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (s.muted ? 'checked' : '') + ' onchange="dspUpdateParam(' + idx + ',\'muted\',this.checked)"><span class="slider round"></span></label></div>';
            } else if (t === 13) {
                h += '<div class="dsp-param"><label>Taps</label><span class="dsp-val">' + (s.numTaps || 0) + '</span></div>';
            } else if (t === 24) {
                h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -80, 0, 0.5, 'dB');
                h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
                h += dspSlider(idx, 'holdMs', 'Hold', s.holdMs, 0, 500, 1, 'ms');
                h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 2000, 1, 'ms');
                h += dspSlider(idx, 'ratio', 'Ratio (1=gate)', s.ratio, 1, 20, 0.5, ':1');
                h += dspSlider(idx, 'rangeDb', 'Range', s.rangeDb, -80, 0, 0.5, 'dB');
                if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
            } else if (t === 25) {
                h += dspSlider(idx, 'bassGain', 'Bass (100 Hz)', s.bassGain, -12, 12, 0.5, 'dB');
                h += dspSlider(idx, 'midGain', 'Mid (1 kHz)', s.midGain, -12, 12, 0.5, 'dB');
                h += dspSlider(idx, 'trebleGain', 'Treble (10 kHz)', s.trebleGain, -12, 12, 0.5, 'dB');
            } else if (t === 26) {
                h += dspSlider(idx, 'powerRatingW', 'Power Rating', s.powerRatingW, 1, 1000, 1, 'W');
                h += dspSlider(idx, 'impedanceOhms', 'Impedance', s.impedanceOhms, 2, 32, 0.5, '\u03A9');
                h += dspSlider(idx, 'thermalTauMs', 'Thermal \u03C4', s.thermalTauMs, 100, 10000, 100, 'ms');
                h += dspSlider(idx, 'excursionLimitMm', 'Xmax', s.excursionLimitMm, 0.5, 30, 0.5, 'mm');
                h += dspSlider(idx, 'driverDiameterMm', 'Driver \u00D8', s.driverDiameterMm, 25, 460, 1, 'mm');
                h += dspSlider(idx, 'maxTempC', 'Max Temp', s.maxTempC, 50, 300, 5, '\u00B0C');
                h += '<div class="dsp-param"><label>Temp</label><span class="dsp-val">' + (s.currentTempC || 25).toFixed(1) + ' \u00B0C</span></div>';
                if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
            } else if (t === 27) {
                h += dspSlider(idx, 'width', 'Width', s.width, 0, 200, 1, '%');
                h += dspSlider(idx, 'centerGainDb', 'Center Gain', s.centerGainDb, -12, 12, 0.5, 'dB');
            } else if (t === 28) {
                h += dspSlider(idx, 'referenceLevelDb', 'Reference Level', s.referenceLevelDb, 60, 100, 1, 'dB SPL');
                h += dspSlider(idx, 'currentLevelDb', 'Current Level', s.currentLevelDb, 20, 100, 1, 'dB SPL');
                h += dspSlider(idx, 'amount', 'Amount', s.amount, 0, 100, 1, '%');
            } else if (t === 29) {
                h += dspSlider(idx, 'frequency', 'Crossover Freq', s.frequency, 20, 200, 1, 'Hz');
                h += dspSlider(idx, 'harmonicGainDb', 'Harmonic Gain', s.harmonicGainDb, -12, 12, 0.5, 'dB');
                h += dspSlider(idx, 'mix', 'Mix', s.mix, 0, 100, 1, '%');
                h += '<div class="dsp-param"><label>Harmonics</label><select class="form-input" style="width:auto;padding:2px 6px;" onchange="dspUpdateParam(' + idx + ',\'order\',parseInt(this.value))">';
                h += '<option value="0"' + (s.order===0?' selected':'') + '>2nd</option>';
                h += '<option value="1"' + (s.order===1?' selected':'') + '>3rd</option>';
                h += '<option value="2"' + (s.order===2?' selected':'') + '>Both</option>';
                h += '</select></div>';
            } else if (t === 30) {
                h += '<div class="dsp-param"><label>Bands</label><select class="form-input" style="width:auto;padding:2px 6px;" onchange="dspUpdateParam(' + idx + ',\'numBands\',parseInt(this.value))">';
                h += '<option value="2"' + (s.numBands===2?' selected':'') + '>2</option>';
                h += '<option value="3"' + ((s.numBands||3)===3?' selected':'') + '>3</option>';
                h += '<option value="4"' + (s.numBands===4?' selected':'') + '>4</option>';
                h += '</select></div>';
            }
            return h;
        }

        function dspParamSync(idx, key, val, min, max, step) {
            val = Math.min(max, Math.max(min, parseFloat(val) || 0));
            var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
            var id = 'dsp_' + idx + '_' + key;
            var sl = document.getElementById(id + '_s');
            var ni = document.getElementById(id + '_n');
            if (sl) sl.value = val;
            dspDrawCompressorGraph(idx);
            if (ni) ni.value = val.toFixed(dec);
            dspUpdateParam(idx, key, val);
        }
        function dspParamStep(idx, key, delta, min, max, step) {
            var id = 'dsp_' + idx + '_' + key;
            var sl = document.getElementById(id + '_s');
            var cur = sl ? parseFloat(sl.value) : 0;
            dspParamSync(idx, key, cur + delta, min, max, step);
        }
        function dspSlider(idx, key, label, val, min, max, step, unit, extraOninput) {
            var numVal = parseFloat(val) || 0;
            var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
            var id = 'dsp_' + idx + '_' + key;
            var oninp = 'document.getElementById(\'' + id + '_n\').value=parseFloat(this.value).toFixed(' + dec + ')' + (extraOninput || '');
            return '<div class="dsp-param"><label>' + label + '</label>' +
                '<button class="dsp-step-btn" onclick="dspParamStep(' + idx + ',\'' + key + '\',' + (-step) + ',' + min + ',' + max + ',' + step + ')" title="Decrease">&lsaquo;</button>' +
                '<input type="range" id="' + id + '_s" min="' + min + '" max="' + max + '" step="' + step + '" value="' + numVal + '" ' +
                'oninput="' + oninp + '" ' +
                'onchange="dspParamSync(' + idx + ',\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
                '<button class="dsp-step-btn" onclick="dspParamStep(' + idx + ',\'' + key + '\',' + step + ',' + min + ',' + max + ',' + step + ')" title="Increase">&rsaquo;</button>' +
                '<input type="number" class="dsp-num-input" id="' + id + '_n" value="' + numVal.toFixed(dec) + '" min="' + min + '" max="' + max + '" step="' + step + '" ' +
                'onchange="dspParamSync(' + idx + ',\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
                '<span class="dsp-unit">' + unit + '</span></div>';
        }

        function dspDrawCompressorGraph(idx) {
            var canvas = document.getElementById('compCanvas_' + idx);
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var getVal = function(key, def) {
                var el = document.getElementById('dsp_' + idx + '_' + key + '_s');
                return el ? parseFloat(el.value) : def;
            };
            var threshold = getVal('thresholdDb', -20);
            var ratio = getVal('ratio', 4);
            var knee = getVal('kneeDb', 6);
            var makeup = getVal('makeupGainDb', 0);
            var dpr = window.devicePixelRatio || 1;
            var rect = canvas.getBoundingClientRect();
            var w = rect.width || 300;
            var h = 180;
            canvas.width = w * dpr;
            canvas.height = h * dpr;
            canvas.style.height = h + 'px';
            ctx.scale(dpr, dpr);
            var pad = { l: 34, r: 8, t: 10, b: 22 };
            var gw = w - pad.l - pad.r;
            var gh = h - pad.t - pad.b;
            var dbMin = -60, dbMax = 0;
            var xOf = function(db) { return pad.l + (db - dbMin) / (dbMax - dbMin) * gw; };
            var yOf = function(db) { return pad.t + (1 - (db - dbMin) / (dbMax - dbMin)) * gh; };
            ctx.clearRect(0, 0, w, h);
            ctx.fillStyle = 'rgba(0,0,0,0.35)';
            ctx.fillRect(pad.l, pad.t, gw, gh);
            ctx.strokeStyle = 'rgba(255,255,255,0.08)';
            ctx.lineWidth = 0.5;
            var grid = [-48, -36, -24, -12];
            for (var g = 0; g < grid.length; g++) {
                ctx.beginPath(); ctx.moveTo(xOf(grid[g]), pad.t); ctx.lineTo(xOf(grid[g]), pad.t + gh); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(pad.l, yOf(grid[g])); ctx.lineTo(pad.l + gw, yOf(grid[g])); ctx.stroke();
            }
            ctx.fillStyle = 'rgba(255,255,255,0.35)';
            ctx.font = '9px sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            for (var g = 0; g < grid.length; g++) ctx.fillText(grid[g], xOf(grid[g]), pad.t + gh + 4);
            ctx.fillText('0', xOf(0), pad.t + gh + 4);
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';
            for (var g = 0; g < grid.length; g++) ctx.fillText(grid[g], pad.l - 4, yOf(grid[g]));
            ctx.fillText('0', pad.l - 4, yOf(0));
            ctx.setLineDash([4, 4]);
            ctx.strokeStyle = 'rgba(255,255,255,0.2)';
            ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(xOf(dbMin), yOf(dbMin)); ctx.lineTo(xOf(dbMax), yOf(dbMax)); ctx.stroke();
            ctx.setLineDash([]);
            ctx.setLineDash([3, 3]);
            ctx.strokeStyle = 'rgba(255,255,255,0.3)';
            ctx.lineWidth = 1;
            if (threshold >= dbMin && threshold <= dbMax) {
                ctx.beginPath(); ctx.moveTo(xOf(threshold), pad.t); ctx.lineTo(xOf(threshold), pad.t + gh); ctx.stroke();
            }
            ctx.setLineDash([]);
            ctx.strokeStyle = '#FF9800';
            ctx.lineWidth = 2.5;
            ctx.beginPath();
            var first = true;
            for (var inDb = dbMin; inDb <= dbMax; inDb += 0.5) {
                var overDb = inDb - threshold;
                var halfK = knee / 2;
                var grDb = 0;
                if (knee > 0 && overDb > -halfK && overDb < halfK) {
                    grDb = (1 - 1 / ratio) * (overDb + halfK) * (overDb + halfK) / (2 * knee);
                } else if (overDb >= halfK) {
                    grDb = overDb * (1 - 1 / ratio);
                }
                var outDb = inDb - grDb + makeup;
                var py = yOf(Math.max(dbMin, Math.min(dbMax, outDb)));
                if (first) { ctx.moveTo(xOf(inDb), py); first = false; } else ctx.lineTo(xOf(inDb), py);
            }
            ctx.stroke();
            ctx.fillStyle = 'rgba(255,255,255,0.4)';
            ctx.font = '9px sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            ctx.fillText('Input (dBFS)', pad.l + gw / 2, h - 1);
            ctx.save();
            ctx.translate(9, pad.t + gh / 2);
            ctx.rotate(-Math.PI / 2);
            ctx.textBaseline = 'bottom';
            ctx.fillText('Output', 0, 0);
            ctx.restore();
        }

        function dspStageColor(t) {
            // Dynamics: Limiter(12), Compressor(18), Noise Gate(24), Multiband Comp(30)
            if (t===12||t===18||t===24||t===30) return '#e6a817';
            // Tone Shaping: Tone Controls(25), Loudness Comp(28), Bass Enhance(29)
            if (t===25||t===28||t===29) return '#43a047';
            // Stereo / Protection: Stereo Width(27), Speaker Protection(26)
            if (t===26||t===27) return '#8e24aa';
            // Utility: FIR(13), Gain(14), Delay(15), Polarity(16), Mute(17)
            if (t>=13&&t<=17) return '#757575';
            // Analysis / Other: Decimator(22), Convolution(23)
            if (t===22||t===23) return '#ef6c00';
            // Crossover / Filters: all EQ/filter types (0-11, 19-21)
            return '#1e88e5';
        }

        function dspRenderStages() {
            if (!dspState || !dspState.channels[dspCh]) return;
            var ch = dspState.channels[dspCh];
            var list = document.getElementById('dspStageList');
            var title = document.getElementById('dspStageTitleText');
            var chainCount = Math.max(0, (ch.stageCount || 0) - DSP_PEQ_BANDS);
            if (title) title.textContent = 'Additional Processing (' + chainCount + ')';
            if (!list) return;
            var html = '';
            var stages = ch.stages || [];
            for (var i = DSP_PEQ_BANDS; i < stages.length; i++) {
                var s = stages[i];
                var typeName = DSP_TYPES[s.type] || 'Unknown';
                var label = s.label || typeName;
                var open = (i === dspOpenStage);
                html += '<div class="dsp-stage-card' + (!s.enabled ? ' disabled' : '') + '">';
                html += '<div class="dsp-stage-header" onclick="dspOpenStage=' + (open ? -1 : i) + ';dspRenderStages();dspDrawFreqResponse();">';
                html += '<span class="dsp-stage-type" style="background:' + dspStageColor(s.type) + '">' + typeName + '</span>';
                html += '<span class="dsp-stage-name">' + label + '</span>';
                html += '<span class="dsp-stage-info">' + dspStageSummary(s) + '</span>';
                html += '<div class="dsp-stage-actions" onclick="event.stopPropagation()">';
                html += '<label class="switch" style="transform:scale(0.6);margin:0;"><input type="checkbox" ' + (s.enabled ? 'checked' : '') + ' onchange="dspToggleStageEnabled(' + i + ',this.checked)"><span class="slider round"></span></label>';
                if (i > DSP_PEQ_BANDS) html += '<button onclick="dspMoveStage(' + i + ',' + (i-1) + ')" title="Move up">&#9650;</button>';
                if (i < stages.length - 1) html += '<button onclick="dspMoveStage(' + i + ',' + (i+1) + ')" title="Move down">&#9660;</button>';
                html += '<button class="del" onclick="dspRemoveStage(' + i + ')" title="Delete">&times;</button>';
                html += '</div></div>';
                html += '<div class="dsp-stage-body' + (open ? ' open' : '') + '">' + (open ? dspParamSliders(i, s) : '') + '</div>';
                html += '</div>';
            }
            list.innerHTML = html;
            if (dspOpenStage >= DSP_PEQ_BANDS && stages[dspOpenStage] && stages[dspOpenStage].type === 18) {
                requestAnimationFrame(function() { dspDrawCompressorGraph(dspOpenStage); });
            }
        }

        function dspHandleState(d) {
            dspState = d;
            var enTgl = document.getElementById('dspEnableToggle');
            var bpTgl = document.getElementById('dspBypassToggle');
            if (enTgl) enTgl.checked = d.dspEnabled;
            if (bpTgl) bpTgl.checked = d.globalBypass;
            var sr = document.getElementById('dspSampleRate');
            if (sr) sr.textContent = (d.sampleRate || 48000) + ' Hz';
            dspRenderPresetList(d.presets || [], d.presetIndex != null ? d.presetIndex : -1);
            dspRenderChannelTabs();
            peqRenderBandStrip();
            peqRenderBandDetail();
            peqUpdateToggleAllBtn();
            dspRenderStages();
            dspDrawFreqResponse();
        }

        // ===== DSP Config Presets =====
        function dspRenderPresetList(presets, activeIndex) {
            var list = document.getElementById('dspPresetList');
            var count = document.getElementById('dspPresetCount');
            var mod = document.getElementById('dspPresetModified');
            if (!list) return;

            var html = '';
            var anyExists = false;
            for (var i = 0; i < presets.length; i++) {
                var p = presets[i];
                if (!p.exists) continue;
                anyExists = true;
                var isActive = (activeIndex === p.index);
                html += '<div class="dsp-stage-item' + (isActive ? ' active' : '') + '">';
                html += '<div class="stage-header">';
                html += '<span class="stage-name">' + escapeHtml(p.name || ('Slot ' + (p.index + 1))) + '</span>';
                html += '<div class="stage-controls">';
                html += '<button class="btn btn-small" onclick="dspLoadPreset(' + p.index + ')">Load</button>';
                html += '<button class="btn btn-small" onclick="dspRenamePresetDialog(' + p.index + ')">Rename</button>';
                html += '<button class="btn btn-small btn-danger" onclick="dspDeletePresetConfirm(' + p.index + ')">Delete</button>';
                html += '</div></div></div>';
            }
            list.innerHTML = html;
            if (count) count.textContent = presets.filter(function(p){ return p.exists; }).length;
            if (mod) mod.style.display = (activeIndex === -1 && anyExists) ? 'inline' : 'none';
        }
        function dspLoadPreset(slot) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'loadDspPreset', slot: slot }));
        }
        function dspShowAddPresetDialog() {
            var name = prompt('Preset name (max 20 chars):', '');
            if (!name) return;
            name = name.substring(0, 20);
            // Backend will auto-assign next available slot
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'saveDspPreset', slot: -1, name: name }));
        }
        function dspRenamePresetDialog(slot) {
            var presets = dspState && dspState.presets ? dspState.presets : [];
            var current = presets.find(function(p) { return p.index === slot; });
            var oldName = current ? current.name : '';
            var name = prompt('Rename preset:', oldName);
            if (!name || name === oldName) return;
            name = name.substring(0, 20);
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'renameDspPreset', slot: slot, name: name }));
        }
        function dspDeletePresetConfirm(slot) {
            var presets = dspState && dspState.presets ? dspState.presets : [];
            var preset = presets.find(function(p) { return p.index === slot; });
            if (!preset) return;
            if (!confirm('Delete preset "' + preset.name + '"?')) return;
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({ type: 'deleteDspPreset', slot: slot }));
        }

        function dspHandleMetrics(d) {
            var cpuText = document.getElementById('dspCpuText');
            var cpuBar = document.getElementById('dspCpuBar');
            if (cpuText) cpuText.textContent = (d.cpuLoad || 0).toFixed(1) + '%';
            if (cpuBar) cpuBar.style.width = Math.min(d.cpuLoad || 0, 100) + '%';
        }

        // ===== Frequency Response Graph (PEQ-aware) =====
        function dspBiquadMagDb(coeffs, f, fs) {
            var b0=coeffs[0], b1=coeffs[1], b2=coeffs[2], a1=coeffs[3], a2=coeffs[4];
            var omega = 2 * Math.PI * f / fs;
            var cosW = Math.cos(omega), sinW = Math.sin(omega);
            var cos2W = Math.cos(2*omega), sin2W = Math.sin(2*omega);
            var numR = b0 + b1*cosW + b2*cos2W, numI = -(b1*sinW + b2*sin2W);
            var denR = 1 + a1*cosW + a2*cos2W, denI = -(a1*sinW + a2*sin2W);
            return 10 * Math.log10(Math.max((numR*numR + numI*numI) / (denR*denR + denI*denI), 1e-20));
        }

        // Client-side biquad coefficient computation (RBJ Audio EQ Cookbook).
        // Returns [b0, b1, b2, a1, a2] normalized so a0=1.
        // Uses ESP-DSP sign convention: denominator is 1 + a1*z^-1 + a2*z^-2,
        // processing uses y = b0*x + d0; d0 = b1*x - a1*y + d1; d1 = b2*x - a2*y.
        function dspComputeCoeffs(type, freq, gain, Q, fs) {
            var fn = freq / fs;
            if (fn < 0.0001) fn = 0.0001;
            if (fn > 0.4999) fn = 0.4999;
            var w0 = 2 * Math.PI * fn;
            var cosW = Math.cos(w0), sinW = Math.sin(w0);
            if (Q <= 0) Q = 0.707;
            var alpha = sinW / (2 * Q);
            var A, b0, b1, b2, a0, a1, a2;

            switch (type) {
                case 0: // LPF
                    b1 = 1 - cosW; b0 = b1 / 2; b2 = b0;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 1: // HPF
                    b1 = -(1 + cosW); b0 = -b1 / 2; b2 = b0;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 2: // BPF
                    b0 = sinW / 2; b1 = 0; b2 = -b0;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 3: // Notch
                    b0 = 1; b1 = -2 * cosW; b2 = 1;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 4: // PEQ
                    A = Math.pow(10, gain / 40);
                    b0 = 1 + alpha * A; b1 = -2 * cosW; b2 = 1 - alpha * A;
                    a0 = 1 + alpha / A; a1 = -2 * cosW; a2 = 1 - alpha / A;
                    break;
                case 5: // Low Shelf
                    A = Math.pow(10, gain / 40);
                    var sq = 2 * Math.sqrt(A) * alpha;
                    b0 = A * ((A + 1) - (A - 1) * cosW + sq);
                    b1 = 2 * A * ((A - 1) - (A + 1) * cosW);
                    b2 = A * ((A + 1) - (A - 1) * cosW - sq);
                    a0 = (A + 1) + (A - 1) * cosW + sq;
                    a1 = -2 * ((A - 1) + (A + 1) * cosW);
                    a2 = (A + 1) + (A - 1) * cosW - sq;
                    break;
                case 6: // High Shelf
                    A = Math.pow(10, gain / 40);
                    var sq = 2 * Math.sqrt(A) * alpha;
                    b0 = A * ((A + 1) + (A - 1) * cosW + sq);
                    b1 = -2 * A * ((A - 1) + (A + 1) * cosW);
                    b2 = A * ((A + 1) + (A - 1) * cosW - sq);
                    a0 = (A + 1) - (A - 1) * cosW + sq;
                    a1 = 2 * ((A - 1) - (A + 1) * cosW);
                    a2 = (A + 1) - (A - 1) * cosW - sq;
                    break;
                case 7: case 8: // Allpass / AP360
                    b0 = 1 - alpha; b1 = -2 * cosW; b2 = 1 + alpha;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 9: // AP180
                    b0 = -(1 - alpha); b1 = 2 * cosW; b2 = -(1 + alpha);
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 10: // BPF 0dB
                    b0 = alpha; b1 = 0; b2 = -alpha;
                    a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
                    break;
                case 19: // LPF 1st order
                    var wt = Math.tan(Math.PI * fn);
                    var n = 1 / (1 + wt);
                    return [wt * n, wt * n, 0, (wt - 1) * n, 0];
                case 20: // HPF 1st order
                    var wt = Math.tan(Math.PI * fn);
                    var n = 1 / (1 + wt);
                    return [n, -n, 0, (wt - 1) * n, 0];
                default: // passthrough
                    return [1, 0, 0, 0, 0];
            }
            // Normalize by a0
            var inv = 1 / a0;
            return [b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv];
        }

        // Compute magnitude in dB from stage parameters (no server coefficients needed)
        function dspStageMagDb(s, f, fs) {
            var coeffs = dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
            return dspBiquadMagDb(coeffs, f, fs);
        }

        function dspDrawFreqResponse() {
            var canvas = document.getElementById('dspFreqCanvas');
            if (!canvas || !dspState || currentActiveTab !== 'dsp') return;
            peqInitCanvas();
            var ctx = canvas.getContext('2d');
            var resized = resizeCanvasIfNeeded(canvas);
            if (resized === -1) return;
            var dims = canvasDims[canvas.id];
            var w = dims.tw, h = dims.th;
            var dpr = window.devicePixelRatio;

            ctx.clearRect(0, 0, w, h);
            ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-card').trim();
            ctx.fillRect(0, 0, w, h);

            var padL = 35 * dpr, padR = 10 * dpr, padT = 10 * dpr, padB = 20 * dpr;
            var gw = w - padL - padR, gh = h - padT - padB;
            var yMin = -24, yMax = 24;
            var fMin = 5, fMax = 24000;
            var logMin = Math.log10(fMin), logRange = Math.log10(fMax) - logMin;

            // Grid
            ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--border').trim();
            ctx.lineWidth = 0.5 * dpr;
            ctx.font = (9 * dpr) + 'px sans-serif';
            ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--text-disabled').trim();
            for (var db = yMin; db <= yMax; db += 6) {
                var y = padT + gh * (1 - (db - yMin) / (yMax - yMin));
                ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(w - padR, y); ctx.stroke();
                ctx.textAlign = 'right';
                ctx.fillText(db + '', padL - 4 * dpr, y + 3 * dpr);
            }
            ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--text-secondary').trim();
            ctx.lineWidth = 1 * dpr;
            var y0 = padT + gh * (1 - (0 - yMin) / (yMax - yMin));
            ctx.beginPath(); ctx.moveTo(padL, y0); ctx.lineTo(w - padR, y0); ctx.stroke();
            ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--border').trim();
            ctx.lineWidth = 0.5 * dpr;
            ctx.textAlign = 'center';
            var freqs = [5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
            var labels = ['5','10','20','50','100','200','500','1k','2k','5k','10k','20k'];
            for (var fi = 0; fi < freqs.length; fi++) {
                var x = padL + gw * (Math.log10(freqs[fi]) - logMin) / logRange;
                ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, padT + gh); ctx.stroke();
                ctx.fillText(labels[fi], x, h - 2 * dpr);
            }

            var ch = dspState.channels[dspCh];
            if (!ch || !ch.stages) return;
            var fs = dspState.sampleRate || 48000;
            var nPts = 256;
            var stages = ch.stages || [];

            // Layer 1: RTA overlay (dBFS spectrum mapped to full graph height)
            if (peqGraphLayers.rta && peqRtaData && peqRtaData.length >= 16) {
                var BAND_EDGES = [0, 60, 150, 250, 400, 600, 1000, 2500, 4000, 6000, 8000, 10000, 12000, 14000, 16000, 20000, 24000];
                var rtaFloor = -96, rtaCeil = 0;
                ctx.beginPath();
                ctx.moveTo(padL, padT + gh);
                for (var bi = 0; bi < 16; bi++) {
                    var midF = Math.max(fMin, (BAND_EDGES[bi] + BAND_EDGES[bi+1]) / 2);
                    var xb = padL + gw * (Math.log10(midF) - logMin) / logRange;
                    var rtaNorm = Math.max(0, Math.min(1, (peqRtaData[bi] - rtaFloor) / (rtaCeil - rtaFloor)));
                    var rtaY = padT + gh * (1 - rtaNorm);
                    ctx.lineTo(xb, rtaY);
                }
                ctx.lineTo(w - padR, padT + gh);
                ctx.closePath();
                ctx.fillStyle = 'rgba(76,175,80,0.12)';
                ctx.fill();
                // RTA line stroke
                ctx.beginPath();
                for (var bi = 0; bi < 16; bi++) {
                    var midF = Math.max(fMin, (BAND_EDGES[bi] + BAND_EDGES[bi+1]) / 2);
                    var xb = padL + gw * (Math.log10(midF) - logMin) / logRange;
                    var rtaNorm = Math.max(0, Math.min(1, (peqRtaData[bi] - rtaFloor) / (rtaCeil - rtaFloor)));
                    var rtaY = padT + gh * (1 - rtaNorm);
                    if (bi === 0) ctx.moveTo(xb, rtaY); else ctx.lineTo(xb, rtaY);
                }
                ctx.strokeStyle = 'rgba(76,175,80,0.5)';
                ctx.lineWidth = 1.5 * dpr;
                ctx.stroke();
            }

            // Separate PEQ bands (0-9) from chain stages (10+)
            var peqBands = stages.slice(0, DSP_PEQ_BANDS);
            var chainStages = stages.slice(DSP_PEQ_BANDS);
            var peqCombined = new Float32Array(nPts);
            var chainCombined = new Float32Array(nPts);
            var hasPeq = false, hasChain = false;

            // Layer 2: Individual PEQ band curves (client-side coefficient computation)
            for (var si = 0; si < peqBands.length; si++) {
                var s = peqBands[si];
                if (!dspIsBiquad(s.type) || !s.enabled) continue;
                var peqCoeffs = dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
                hasPeq = true;
                if (peqGraphLayers.individual) {
                    ctx.beginPath();
                    ctx.strokeStyle = PEQ_COLORS[si] + '40';
                    ctx.lineWidth = 1 * dpr;
                }
                for (var p = 0; p < nPts; p++) {
                    var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
                    var magDb = dspBiquadMagDb(peqCoeffs, f, fs);
                    peqCombined[p] += magDb;
                    if (peqGraphLayers.individual) {
                        var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                        var xp = padL + gw * p / (nPts - 1);
                        if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
                    }
                }
                if (peqGraphLayers.individual) ctx.stroke();
            }

            // Layer 3: Chain biquad curves (use server coefficients, fallback to client-side)
            for (var si = 0; si < chainStages.length; si++) {
                var s = chainStages[si];
                if (!dspIsBiquad(s.type) || !s.enabled) continue;
                var chainCoeffs = s.coeffs || dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
                hasChain = true;
                if (peqGraphLayers.chain) {
                    ctx.beginPath();
                    ctx.strokeStyle = 'rgba(180,180,180,0.15)';
                    ctx.lineWidth = 1 * dpr;
                }
                for (var p = 0; p < nPts; p++) {
                    var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
                    var magDb = dspBiquadMagDb(chainCoeffs, f, fs);
                    chainCombined[p] += magDb;
                    if (peqGraphLayers.chain) {
                        var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                        var xp = padL + gw * p / (nPts - 1);
                        if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
                    }
                }
                if (peqGraphLayers.chain) ctx.stroke();
            }

            // Layer 4: Combined PEQ response (orange dashed)
            if (hasPeq) {
                ctx.beginPath();
                ctx.strokeStyle = '#FF9800';
                ctx.lineWidth = 1.5 * dpr;
                ctx.setLineDash([4*dpr, 4*dpr]);
                for (var p = 0; p < nPts; p++) {
                    var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, peqCombined[p])) - yMin) / (yMax - yMin));
                    var xp = padL + gw * p / (nPts - 1);
                    if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
                }
                ctx.stroke();
                ctx.setLineDash([]);
            }

            // Layer 5: Combined total response (PEQ + chain, orange solid)
            if (hasPeq || hasChain) {
                ctx.beginPath();
                ctx.strokeStyle = '#FF9800';
                ctx.lineWidth = 2.5 * dpr;
                for (var p = 0; p < nPts; p++) {
                    var total = peqCombined[p] + chainCombined[p];
                    var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, total)) - yMin) / (yMax - yMin));
                    var xp = padL + gw * p / (nPts - 1);
                    if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
                }
                ctx.stroke();
            }

            // Highlight selected chain stage (if expanded)
            if (dspOpenStage >= DSP_PEQ_BANDS && dspOpenStage < stages.length) {
                var ss = stages[dspOpenStage];
                if (dspIsBiquad(ss.type)) {
                    var hlCoeffs = ss.coeffs || dspComputeCoeffs(ss.type, ss.freq || 1000, ss.gain || 0, ss.Q || 0.707, fs);
                    ctx.beginPath();
                    ctx.strokeStyle = '#FFFFFF';
                    ctx.lineWidth = 2 * dpr;
                    ctx.setLineDash([4*dpr, 4*dpr]);
                    for (var p = 0; p < nPts; p++) {
                        var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
                        var magDb = dspBiquadMagDb(hlCoeffs, f, fs);
                        var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                        var xp = padL + gw * p / (nPts - 1);
                        if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
                    }
                    ctx.stroke();
                    ctx.setLineDash([]);
                }
            }

            // Layer 6: PEQ control point circles with band numbers
            for (var i = 0; i < peqBands.length; i++) {
                var b = peqBands[i];
                var freq = b.freq || 1000, gain = b.gain || 0;
                var cx = padL + gw * (Math.log10(freq) - logMin) / logRange;
                var cy = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, gain)) - yMin) / (yMax - yMin));
                var isSelected = (i === peqSelectedBand);
                var radius = isSelected ? 11 * dpr : 9 * dpr;
                ctx.beginPath();
                ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
                if (b.enabled) {
                    ctx.fillStyle = PEQ_COLORS[i];
                    ctx.fill();
                } else {
                    ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-card').trim();
                    ctx.fill();
                    ctx.strokeStyle = PEQ_COLORS[i];
                    ctx.lineWidth = 1.5 * dpr;
                    ctx.stroke();
                }
                // Band number label inside dot
                ctx.font = 'bold ' + (isSelected ? 11 * dpr : 10 * dpr) + 'px sans-serif';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillStyle = b.enabled ? '#fff' : PEQ_COLORS[i];
                ctx.fillText('' + (i + 1), cx, cy + 0.5 * dpr);
                if (isSelected) {
                    ctx.beginPath();
                    ctx.arc(cx, cy, radius + 3 * dpr, 0, 2 * Math.PI);
                    ctx.strokeStyle = '#fff';
                    ctx.lineWidth = 1.5 * dpr;
                    ctx.stroke();
                }
            }
        }

        // ===== DSP Import/Export =====
        function dspImportApo() {
            dspImportMode = 'apo';
            document.getElementById('dspFileInput').accept = '.txt';
            document.getElementById('dspFileInput').click();
        }
        function dspImportJson() {
            dspImportMode = 'json';
            document.getElementById('dspFileInput').accept = '.json';
            document.getElementById('dspFileInput').click();
        }
        function dspHandleFileImport(ev) {
            var file = ev.target.files[0];
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function(e) {
                var url = dspImportMode === 'apo' ? '/api/dsp/import/apo?ch=' + dspCh : '/api/dsp';
                var method = dspImportMode === 'apo' ? 'POST' : 'PUT';
                fetch(url, { method: method, headers: { 'Content-Type': dspImportMode === 'json' ? 'application/json' : 'text/plain' }, body: e.target.result })
                    .then(r => r.json())
                    .then(d => { if (d.success) showToast('Import successful'); else showToast('Import failed: ' + (d.message || ''), true); })
                    .catch(err => showToast('Import error: ' + err, true));
            };
            reader.readAsText(file);
            ev.target.value = '';
        }
        function dspExportApo() {
            window.open('/api/dsp/export/apo?ch=' + dspCh, '_blank');
        }
        function dspExportJson() {
            window.open('/api/dsp/export/json', '_blank');
        }

        // ===== DSP Crossover Presets =====
        function dspShowCrossoverModal() {
            dspToggleAddMenu();
            var existing = document.getElementById('crossoverModal');
            if (existing) existing.remove();
            var modal = document.createElement('div');
            modal.id = 'crossoverModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = '<div class="modal">' +
                '<div class="modal-title">Crossover Preset</div>' +
                '<div class="form-group"><label class="form-label">Crossover Frequency (Hz)</label>' +
                '<input type="number" class="form-input" id="modalXoverFreq" value="2000" min="20" max="20000"></div>' +
                '<div class="form-group"><label class="form-label">Slope</label>' +
                '<select class="form-input" id="modalXoverType">' +
                '<optgroup label="Butterworth">' +
                '<option value="bw1">BW1 — 1st order (6 dB/oct)</option>' +
                '<option value="bw2">BW2 — 2nd order (12 dB/oct)</option>' +
                '<option value="bw3">BW3 — 3rd order (18 dB/oct)</option>' +
                '<option value="bw4">BW4 — 4th order (24 dB/oct)</option>' +
                '<option value="bw5">BW5 — 5th order (30 dB/oct)</option>' +
                '<option value="bw6">BW6 — 6th order (36 dB/oct)</option>' +
                '<option value="bw7">BW7 — 7th order (42 dB/oct)</option>' +
                '<option value="bw8">BW8 — 8th order (48 dB/oct)</option>' +
                '<option value="bw9">BW9 — 9th order (54 dB/oct)</option>' +
                '<option value="bw10">BW10 — 10th order (60 dB/oct)</option>' +
                '<option value="bw11">BW11 — 11th order (66 dB/oct)</option>' +
                '<option value="bw12">BW12 — 12th order (72 dB/oct)</option>' +
                '</optgroup>' +
                '<optgroup label="Linkwitz-Riley">' +
                '<option value="lr2">LR2 — 2nd order (12 dB/oct)</option>' +
                '<option value="lr4" selected>LR4 — 4th order (24 dB/oct)</option>' +
                '<option value="lr6">LR6 — 6th order (36 dB/oct)</option>' +
                '<option value="lr8">LR8 — 8th order (48 dB/oct)</option>' +
                '<option value="lr12">LR12 — 12th order (72 dB/oct)</option>' +
                '<option value="lr16">LR16 — 16th order (96 dB/oct)</option>' +
                '<option value="lr24">LR24 — 24th order (144 dB/oct)</option>' +
                '</optgroup>' +
                '<optgroup label="Bessel (flat group delay)">' +
                '<option value="bessel2">Bessel 2nd order (12 dB/oct)</option>' +
                '<option value="bessel4">Bessel 4th order (24 dB/oct)</option>' +
                '<option value="bessel6">Bessel 6th order (36 dB/oct)</option>' +
                '<option value="bessel8">Bessel 8th order (48 dB/oct)</option>' +
                '</optgroup>' +
                '</select></div>' +
                '<div class="form-group"><label class="form-label">Role</label>' +
                '<select class="form-input" id="modalXoverRole">' +
                '<option value="0">Low Pass (LPF)</option>' +
                '<option value="1">High Pass (HPF)</option>' +
                '</select></div>' +
                '<div class="modal-actions"><button class="btn btn-secondary" onclick="closeCrossoverModal()">Cancel</button>' +
                '<button class="btn btn-primary" onclick="dspApplyCrossover()">Apply to Channel</button></div></div>';
            modal.addEventListener('click', function(e) { if (e.target === modal) closeCrossoverModal(); });
            document.body.appendChild(modal);
        }
        function closeCrossoverModal() {
            var m = document.getElementById('crossoverModal');
            if (m) m.remove();
        }
        function dspApplyCrossover() {
            var freq = parseInt(document.getElementById('modalXoverFreq').value) || 2000;
            var type = document.getElementById('modalXoverType').value;
            var role = parseInt(document.getElementById('modalXoverRole').value);
            fetch('/api/dsp/crossover?ch=' + dspCh, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ freq: freq, type: type, role: role })
            })
            .then(r => r.json())
            .then(d => { if (d.success) showToast('Crossover applied'); else showToast('Failed: ' + (d.message || ''), true); })
            .catch(err => showToast('Error: ' + err, true));
            closeCrossoverModal();
        }

        // ===== Baffle Step =====
        function dspShowBaffleModal() {
            dspToggleAddMenu();
            var existing = document.getElementById('baffleModal');
            if (existing) existing.remove();
            var modal = document.createElement('div');
            modal.id = 'baffleModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = '<div class="modal">' +
                '<div class="modal-title">Baffle Step Correction</div>' +
                '<div class="form-group"><label class="form-label">Baffle Width (mm)</label>' +
                '<input type="number" class="form-input" id="modalBaffleWidth" value="250" min="50" max="600" step="1"></div>' +
                '<div id="modalBafflePreview" style="font-size:12px;color:var(--text-secondary);margin-bottom:12px;">Estimated: ~437 Hz, +6.0 dB high shelf</div>' +
                '<div class="modal-actions"><button class="btn btn-secondary" onclick="closeBaffleModal()">Cancel</button>' +
                '<button class="btn btn-primary" onclick="applyBaffleStep()">Apply to Channel</button></div></div>';
            modal.addEventListener('click', function(e) { if (e.target === modal) closeBaffleModal(); });
            document.body.appendChild(modal);
            document.getElementById('modalBaffleWidth').addEventListener('input', function() {
                var w = parseInt(this.value) || 250;
                var f = 343000 / (3.14159 * w);
                document.getElementById('modalBafflePreview').textContent = 'Estimated: ~' + f.toFixed(0) + ' Hz, +6.0 dB high shelf';
            });
        }
        function closeBaffleModal() {
            var m = document.getElementById('baffleModal');
            if (m) m.remove();
        }
        function applyBaffleStep() {
            var w = parseInt(document.getElementById('modalBaffleWidth').value) || 250;
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'applyBaffleStep', ch:dspCh, baffleWidthMm:w}));
            showToast('Baffle step applied');
            closeBaffleModal();
        }

        // ===== THD Measurement =====
        function dspShowThdModal() {
            dspToggleAddMenu();
            var existing = document.getElementById('thdModal');
            if (existing) existing.remove();
            var modal = document.createElement('div');
            modal.id = 'thdModal';
            modal.className = 'modal-overlay active';
            modal.innerHTML = '<div class="modal">' +
                '<div class="modal-title">THD+N Measurement</div>' +
                '<div class="form-group"><label class="form-label">Test Frequency</label>' +
                '<select class="form-input" id="thdFreq"><option value="100">100 Hz</option>' +
                '<option value="1000" selected>1 kHz</option><option value="10000">10 kHz</option></select></div>' +
                '<div class="form-group"><label class="form-label">Averages</label>' +
                '<select class="form-input" id="thdAverages"><option value="4">4 frames</option>' +
                '<option value="8" selected>8 frames</option><option value="16">16 frames</option></select></div>' +
                '<div style="display:flex;gap:8px;margin-bottom:8px;">' +
                '<button class="btn btn-primary" id="thdStartBtn" onclick="thdStart()">Start</button>' +
                '<button class="btn btn-outline" id="thdStopBtn" onclick="thdStop()" style="display:none">Stop</button></div>' +
                '<div id="thdResult" style="display:none;font-size:13px;">' +
                '<div class="info-row"><span class="info-label">THD+N</span><span class="info-value" id="thdPercent">\u2014</span></div>' +
                '<div class="info-row"><span class="info-label">THD+N (dB)</span><span class="info-value" id="thdDb">\u2014</span></div>' +
                '<div class="info-row"><span class="info-label">Fundamental</span><span class="info-value" id="thdFund">\u2014</span></div>' +
                '<div class="info-row"><span class="info-label">Progress</span><span class="info-value" id="thdProgress">\u2014</span></div>' +
                '<div id="thdHarmonics" style="margin-top:8px;">' +
                '<div style="font-weight:600;margin-bottom:4px;font-size:12px;">Harmonics (dB rel. fundamental)</div>' +
                '<div id="thdHarmBars" style="display:flex;gap:2px;height:60px;align-items:flex-end;"></div></div></div>' +
                '<div class="modal-actions" style="margin-top:12px;"><button class="btn btn-secondary" onclick="closeThdModal()">Close</button></div></div>';
            modal.addEventListener('click', function(e) { if (e.target === modal) closeThdModal(); });
            document.body.appendChild(modal);
        }
        function closeThdModal() {
            thdStop();
            var m = document.getElementById('thdModal');
            if (m) m.remove();
        }
        function thdStart() {
            var freq = parseInt(document.getElementById('thdFreq').value) || 1000;
            var avg = parseInt(document.getElementById('thdAverages').value) || 8;
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'startThdMeasurement', freq:freq, averages:avg}));
            var sb = document.getElementById('thdStartBtn');
            var stb = document.getElementById('thdStopBtn');
            var tr = document.getElementById('thdResult');
            if (sb) sb.style.display = 'none';
            if (stb) stb.style.display = '';
            if (tr) tr.style.display = '';
        }
        function thdStop() {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'stopThdMeasurement'}));
            var sb = document.getElementById('thdStartBtn');
            var stb = document.getElementById('thdStopBtn');
            if (sb) sb.style.display = '';
            if (stb) stb.style.display = 'none';
        }
        function thdUpdateResult(d) {
            if (!d) return;
            var el;
            el = document.getElementById('thdPercent');
            if (el) el.textContent = d.valid ? d.thdPlusNPercent.toFixed(3) + '%' : '\u2014';
            el = document.getElementById('thdDb');
            if (el) el.textContent = d.valid ? d.thdPlusNDb.toFixed(1) + ' dB' : '\u2014';
            el = document.getElementById('thdFund');
            if (el) el.textContent = d.valid ? d.fundamentalDbfs.toFixed(1) + ' dBFS' : '\u2014';
            el = document.getElementById('thdProgress');
            if (el) el.textContent = d.framesProcessed + '/' + d.framesTarget;
            if (d.valid && d.harmonicLevels) {
                var bars = document.getElementById('thdHarmBars');
                if (bars) {
                    var html = '';
                    for (var h = 0; h < d.harmonicLevels.length; h++) {
                        var lev = d.harmonicLevels[h];
                        var pct = Math.max(2, Math.min(100, (120 + lev) / 120 * 100));
                        html += '<div style="flex:1;display:flex;flex-direction:column;align-items:center;">';
                        html += '<div style="width:100%;background:var(--primary);border-radius:2px;height:' + pct + '%" title="' + lev.toFixed(1) + ' dB"></div>';
                        html += '<span style="font-size:10px;margin-top:2px;">' + (h+2) + '</span></div>';
                    }
                    bars.innerHTML = html;
                }
            }
            if (d.valid && !d.measuring) {
                var sb = document.getElementById('thdStartBtn');
                var stb = document.getElementById('thdStopBtn');
                if (sb) sb.style.display = '';
                if (stb) stb.style.display = 'none';
            }
        }

        // ===== DSP Routing Matrix =====
        let dspRouting = null;
        function dspRoutingPreset(name) {
            fetch('/api/dsp/routing', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ preset: name })
            })
            .then(r => r.json())
            .then(d => { if (d.success) { showToast('Routing: ' + name); dspLoadRouting(); } })
            .catch(err => showToast('Error: ' + err, true));
        }
        function dspLoadRouting() {
            fetch('/api/dsp/routing')
                .then(r => r.json())
                .then(d => { dspRouting = d.matrix; dspRenderRouting(); })
                .catch(() => {});
        }
        function dspRenderRouting() {
            var el = document.getElementById('dspRoutingGrid');
            if (!el || !dspRouting) return;
            var h = '<table style="border-collapse:collapse;font-size:12px;width:100%;">';
            h += '<tr><td></td>';
            for (var i = 0; i < DSP_MAX_CH; i++) h += '<td style="padding:4px;text-align:center;font-weight:600;color:var(--text-secondary);">' + dspChLabel(i) + '</td>';
            h += '</tr>';
            for (var o = 0; o < DSP_MAX_CH; o++) {
                h += '<tr><td style="padding:4px;font-weight:600;color:var(--text-secondary);">' + dspChLabel(o) + '</td>';
                for (var i = 0; i < DSP_MAX_CH; i++) {
                    var v = dspRouting[o] ? dspRouting[o][i] : 0;
                    var db = v <= 0.0001 ? 'Off' : (20 * Math.log10(v)).toFixed(1);
                    var bg = v > 0.001 ? 'rgba(255,152,0,0.15)' : 'transparent';
                    h += '<td style="padding:4px;text-align:center;background:' + bg + ';border:1px solid var(--border);cursor:pointer;border-radius:4px;" onclick="dspEditRoutingCell(' + o + ',' + i + ')">' + db + '</td>';
                }
                h += '</tr>';
            }
            h += '</table>';
            el.innerHTML = h;
        }
        function dspEditRoutingCell(o, i) {
            var current = dspRouting && dspRouting[o] ? dspRouting[o][i] : 0;
            var currentDb = current <= 0.0001 ? 'Off' : (20 * Math.log10(current)).toFixed(1);
            var val = prompt('Gain for ' + dspChLabel(o) + ' <- ' + dspChLabel(i) + ' (dB, or "off" for silence):', currentDb);
            if (val === null) return;
            var lv = val.trim().toLowerCase();
            var gainDb = lv === 'off' || lv === '-inf' || lv === '' ? -200 : parseFloat(val);
            if (isNaN(gainDb)) return;
            fetch('/api/dsp/routing', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ output: o, input: i, gainDb: gainDb })
            })
            .then(r => r.json())
            .then(d => { if (d.success) dspLoadRouting(); })
            .catch(err => showToast('Error: ' + err, true));
        }
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
            --bg-input: #2C2C2C;
            --accent: #FF9800;
            --accent-dark: #E68900;
            --text-primary: #FFFFFF;
            --text-secondary: #B0B0B0;
            --text-disabled: #666666;
            --success: #4CAF50;
            --error: #F44336;
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
            text-align: left;
        }

        .modal-title {
            font-size: 20px;
            font-weight: 600;
            margin-bottom: 16px;
            color: var(--text-primary);
        }

        .modal-actions {
            display: flex;
            gap: 12px;
            margin-top: 24px;
        }

        .modal-actions button {
            flex: 1;
            padding: 12px;
            border-radius: 8px;
            border: none;
            font-weight: 600;
            cursor: pointer;
        }

        .modal-actions .btn-primary { background: var(--accent); color: #000; }
        .modal-actions .btn-secondary { background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--border); }

        .animate-pulse {
            animation: pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: .5; }
        }

        .hidden { display: none; }

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
        </div>

        <p class="info-text">Enter your WiFi credentials to connect the device to your network.</p>
    </div>

    <div id="wifiConnectionModal" class="modal-overlay">
        <div class="modal">
            <div class="modal-title">Connecting to WiFi</div>
            <div style="text-align: center; padding: 20px;">
                <div id="wifiLoader" class="animate-pulse" style="font-size: 40px; margin-bottom: 16px;">📶</div>
                <div id="wifiStatusText">Connecting...</div>
                <div id="wifiIPInfo" class="hidden" style="margin-top: 16px; font-family: monospace; font-size: 18px; color: var(--success); font-weight: bold;"></div>
            </div>
            <div id="wifiModalActions" class="modal-actions">
                <button type="button" class="btn-secondary" onclick="closeWiFiModal()">Cancel</button>
            </div>
        </div>
    </div>

    <script>
        // Global fetch wrapper for API calls (handles 401 Unauthorized)
        async function apiFetch(url, options = {}) {
            try {
                const response = await fetch(url, options);
                if (response.status === 401) {
                    window.location.href = '/login';
                    return new Promise(() => {});
                }
                return response;
            } catch (error) {
                console.error(`API Fetch Error [${url}]:`, error);
                throw error;
            }
        }

        // REMOVED: Duplicate window.onload - merged into main onload at line 9492

        let apPagePollTimer = null;

        function showAPPageModal(ssid) {
            document.getElementById('wifiStatusText').innerHTML = `Connecting to <strong>${ssid}</strong>...`;
            document.getElementById('wifiLoader').textContent = '📶';
            document.getElementById('wifiLoader').classList.add('animate-pulse');
            document.getElementById('wifiIPInfo').classList.add('hidden');
            document.getElementById('wifiModalActions').innerHTML = '<button type="button" class="btn btn-secondary" onclick="closeAPPageModal()">Cancel</button>';
            document.getElementById('wifiConnectionModal').classList.add('active');
        }

        function closeAPPageModal() {
            document.getElementById('wifiConnectionModal').classList.remove('active');
            if (apPagePollTimer) {
                clearInterval(apPagePollTimer);
                apPagePollTimer = null;
            }
        }

        function updateAPPageStatus(type, message, ip = null) {
            const statusText = document.getElementById('wifiStatusText');
            const loader = document.getElementById('wifiLoader');
            const actions = document.getElementById('wifiModalActions');
            const ipInfo = document.getElementById('wifiIPInfo');

            statusText.innerHTML = message;
            loader.classList.remove('animate-pulse');

            if (type === 'success') {
                loader.textContent = '✅';
                if (ip) {
                    ipInfo.textContent = 'IP: ' + ip;
                    ipInfo.classList.remove('hidden');
                    actions.innerHTML = `<button class="btn btn-success" onclick="window.location.href='http://${ip}'">Go to Dashboard</button>`;
                } else {
                    actions.innerHTML = `<button class="btn btn-secondary" onclick="closeAPPageModal()">Close</button>`;
                }
            } else if (type === 'error') {
                loader.textContent = '❌';
                actions.innerHTML = `<button class="btn btn-secondary" onclick="closeAPPageModal()">Try Again</button>`;
            }
        }

        function submitWiFi(event) {
            event.preventDefault();
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;

            showAPPageModal(ssid);

            apiFetch('/api/wificonfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    if (apPagePollTimer) clearInterval(apPagePollTimer);
                    apPagePollTimer = setInterval(pollAPPageConnection, 2000);
                } else {
                    updateAPPageStatus('error', data.message || 'Connection failed');
                }
            })
            .catch(err => {
                updateAPPageStatus('error', 'Error: ' + err.message);
            });
        }

        function pollAPPageConnection() {
            apiFetch('/api/wifistatus')
                .then(res => res.json())
                .then(data => {
                    if (data.wifiConnecting) return;

                    clearInterval(apPagePollTimer);
                    apPagePollTimer = null;

                    if (data.wifiConnectSuccess) {
                        updateAPPageStatus('success', 'Connected successfully!', data.wifiNewIP);
                    } else {
                        updateAPPageStatus('error', data.message || 'Failed to connect. Check credentials.');
                    }
                })
                .catch(err => {
                    console.error('Polling error:', err);
                    updateAPPageStatus('error', 'Network error during connection');
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
