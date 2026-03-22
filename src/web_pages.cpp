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
    <style>/* AUTO-GENERATED from src/design_tokens.h — do not edit manually */
/* Run: node tools/extract_tokens.js */

:root {
  --accent: #FF9800;
  --accent-light: #FFB74D;
  --accent-dark: #E68900;
  --bg-primary: #F5F5F5;
  --bg-surface: #FFFFFF;
  --bg-card: #EEEEEE;
  --bg-input: #E0E0E0;
  --border: #E0E0E0;
  --text-primary: #212121;
  --text-secondary: #757575;
  --success: #4CAF50;
  --warning: #FFC107;
  --error: #F44336;
  --info: #2196F3;
}

body.night-mode {
  --bg-primary: #121212;
  --bg-surface: #1E1E1E;
  --bg-card: #252525;
  --bg-input: #2C2C2C;
  --border: #333333;
  --text-primary: #FFFFFF;
  --text-secondary: #B0B0B0;
  --text-disabled: #666666;
}

/* ===== 01-variables.css ===== */

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

/* ===== 02-layout.css ===== */

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

/* ===== 03-components.css ===== */

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

        /* ===== HAL Device State Colors ===== */
        .status-dot.status-green { background: var(--success, #4caf50); box-shadow: 0 0 4px var(--success, #4caf50); }
        .status-dot.status-red { background: var(--error, #e53935); box-shadow: 0 0 4px var(--error, #e53935); }
        .status-dot.status-amber { background: var(--warning, #ff9800); box-shadow: 0 0 4px var(--warning, #ff9800); }
        .status-dot.status-blue { background: var(--info, #2196f3); box-shadow: 0 0 4px var(--info, #2196f3); }
        .status-dot.status-grey { background: #666; }

        .badge-green { background: rgba(76,175,80,0.15); color: #4caf50; }
        .badge-red { background: rgba(229,57,53,0.15); color: #e53935; }
        .badge-amber { background: rgba(255,152,0,0.15); color: #ff9800; }
        .badge-blue { background: rgba(33,150,243,0.15); color: #2196f3; }
        .badge-grey { background: rgba(102,102,102,0.15); color: #999; }

        /* PSRAM Budget Table */
        .budget-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.85em;
            padding: 0 16px;
        }
        .budget-table th,
        .budget-table td {
            padding: 4px 8px;
            text-align: left;
            border-bottom: 1px solid var(--border-color, #333);
        }
        .budget-table th {
            font-weight: 600;
            opacity: 0.7;
            font-size: 0.9em;
        }

        /* Health Banner */
        .health-banner {
            padding: 8px 12px;
            border-radius: 6px;
            margin-bottom: 12px;
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.9em;
        }
        .health-banner-amber {
            background: rgba(255, 152, 0, 0.15);
            border: 1px solid rgba(255, 152, 0, 0.3);
            color: #ffb74d;
        }
        .health-banner-red {
            background: rgba(244, 67, 54, 0.15);
            border: 1px solid rgba(244, 67, 54, 0.3);
            color: #ef5350;
        }

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
        .task-table th .sort-arrow.asc::after {
            content: '';
            display: inline-block;
            width: 10px;
            height: 10px;
            background-color: currentColor;
            -webkit-mask-image: url("data:image/svg+xml,<svg viewBox='0 0 24 24' xmlns='http://www.w3.org/2000/svg'><path d='M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z'/></svg>");
            mask-image: url("data:image/svg+xml,<svg viewBox='0 0 24 24' xmlns='http://www.w3.org/2000/svg'><path d='M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z'/></svg>");
            mask-size: contain;
            mask-repeat: no-repeat;
            opacity: 1;
            vertical-align: middle;
        }
        .task-table th .sort-arrow.desc::after {
            content: '';
            display: inline-block;
            width: 10px;
            height: 10px;
            background-color: currentColor;
            -webkit-mask-image: url("data:image/svg+xml,<svg viewBox='0 0 24 24' xmlns='http://www.w3.org/2000/svg'><path d='M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z'/></svg>");
            mask-image: url("data:image/svg+xml,<svg viewBox='0 0 24 24' xmlns='http://www.w3.org/2000/svg'><path d='M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z'/></svg>");
            mask-size: contain;
            mask-repeat: no-repeat;
            opacity: 1;
            vertical-align: middle;
        }
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
            margin-left: 6px;
            color: var(--accent);
            text-decoration: none;
            vertical-align: middle;
            opacity: 0.85;
        }

        .release-notes-link:hover {
            color: var(--accent-dark);
            opacity: 1;
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
        .dsp-preset-item {
            background: var(--bg-card);
            border-radius: 10px;
            margin-bottom: 6px;
            border: 1px solid var(--border);
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 12px;
            cursor: pointer;
            transition: border-color 0.15s;
        }
        .dsp-preset-item:hover { border-color: var(--text-secondary); }
        .dsp-preset-item.active {
            background: linear-gradient(135deg, var(--accent), var(--accent-dark, #E68900));
            border-color: var(--accent);
            box-shadow: 0 2px 8px rgba(255,152,0,0.3);
        }
        .dsp-preset-item.active .preset-name { color: #fff; }
        .dsp-preset-item.active .dsp-stage-actions button {
            background: rgba(0,0,0,0.2);
            color: rgba(255,255,255,0.85);
        }
        .dsp-preset-item.active .dsp-stage-actions button:hover {
            background: rgba(0,0,0,0.35);
        }
        .dsp-preset-item .preset-name {
            flex: 1;
            font-size: 13px;
            font-weight: 500;
            color: var(--text-primary);
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .dsp-preset-item .dsp-stage-actions button {
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
        .dsp-preset-item .dsp-stage-actions button:hover {
            background: var(--border);
        }
        .dsp-preset-item .dsp-stage-actions button.del:hover {
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

        /* DSP Compare button */
        #dspCompareBtn { margin-left: 8px; }
        #dspCompareBtn.active {
          background: var(--accent-color, #FF9800);
          color: #000;
        }

        /* ===== Debug Console Chips & Search ===== */
        .chip-container { display: flex; flex-wrap: wrap; gap: 4px; align-items: center; }
        .btn-chip {
          display: inline-flex; align-items: center; gap: 4px;
          padding: 3px 10px; border-radius: 12px; font-size: 11px; font-weight: 500;
          border: 1px solid var(--border); background: var(--bg-input); color: var(--text-secondary);
          cursor: pointer; transition: all 0.15s; white-space: nowrap;
        }
        .btn-chip:hover { border-color: var(--accent); color: var(--text-primary); }
        .btn-chip.active { background: var(--accent); color: #fff; border-color: var(--accent); }
        .btn-chip .chip-badge {
          font-size: 9px; padding: 1px 5px; border-radius: 8px;
          background: rgba(255,255,255,0.2); margin-left: 2px;
        }
        .btn-chip .chip-badge.has-errors { background: var(--error); color: #fff; }
        .btn-chip .chip-badge.has-warnings { background: var(--warning); color: #000; }
        .btn-chip-action { font-size: 12px; padding: 3px 8px; }
        .log-highlight { background: rgba(255, 165, 0, 0.3); border-radius: 2px; }

        /* ===== Routing Matrix Table ===== */
        .routing-matrix-table { border-collapse: collapse; font-size: 11px; width: 100%; }
        .routing-matrix-table td { padding: 3px 4px; text-align: center; white-space: nowrap; }
        .routing-corner { width: 60px; }
        .routing-header { font-weight: 600; color: var(--text-secondary); font-size: 10px; }
        .routing-row-label { font-weight: 600; color: var(--text-secondary); text-align: left !important; font-size: 10px; }
        .routing-cell { border: 1px solid var(--border); cursor: pointer; border-radius: 3px; transition: background 0.15s; color: var(--text-secondary); }
        .routing-cell:hover { background: rgba(255,152,0,0.1); }
        .routing-cell.active { background: rgba(255,152,0,0.12); color: var(--text-primary); }
        .routing-cell.unity { background: rgba(255,152,0,0.22); color: var(--accent); font-weight: 600; }

        /* ===== Output DSP Panels ===== */
        .output-dsp-stage { display: flex; align-items: center; gap: 8px; padding: 6px 8px; background: var(--bg-surface); border: 1px solid var(--border); border-radius: 6px; margin-bottom: 4px; }
        .output-dsp-stage .stage-label { flex: 1; font-size: 12px; font-weight: 500; }
        .output-dsp-stage .stage-type { font-size: 10px; color: var(--text-secondary); padding: 1px 6px; background: var(--bg-card); border-radius: 3px; }
        .output-dsp-stage .stage-params { font-size: 11px; color: var(--text-secondary); }
        .output-dsp-bypass { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
        .output-dsp-add-row { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 8px; }

        /* ===== HAL Capacity Indicator ===== */
        .hal-capacity { font-size: 11px; opacity: 0.6; padding: 4px 0; letter-spacing: 0.3px; }
        .hal-capacity-warn { color: var(--warning, #ff9800); opacity: 1; font-weight: 500; }

        /* ===== HAL Device Cards ===== */
        .hal-device-card { margin-bottom: 8px; }
        .hal-device-card.expanded { border-color: var(--accent); }
        .hal-device-card.state-available { border-left: 3px solid var(--success, #4caf50); }
        .hal-device-card.state-error, .hal-device-card.state-unavailable { border-left: 3px solid var(--error, #e53935); }
        .hal-device-card.state-configuring, .hal-device-card.state-manual { border-left: 3px solid var(--warning, #ff9800); }
        .hal-device-card.state-detected { border-left: 3px solid var(--info, #2196f3); }
        .hal-device-card.state-unknown, .hal-device-card.state-removed { border-left: 3px solid #666; }
        .hal-device-header { display: flex; align-items: center; gap: 8px; cursor: pointer; padding: 4px 0; }
        .hal-device-name { flex: 1; font-weight: 600; font-size: 13px; }
        .hal-temp-reading { font-size: 12px; color: var(--accent); font-weight: 500; }
        .hal-expand-icon { transition: transform 0.2s; opacity: 0.5; }
        .hal-expand-icon.rotated { transform: rotate(180deg); }
        .hal-device-info { display: flex; gap: 4px; flex-wrap: wrap; margin: 4px 0; }
        .hal-device-details { margin-top: 8px; padding-top: 8px; border-top: 1px solid var(--border); }
        .hal-detail-row { display: flex; justify-content: space-between; font-size: 12px; padding: 2px 0; }
        .hal-detail-row span:first-child { color: var(--text-secondary); }
        .hal-device-actions { display: flex; gap: 6px; margin-top: 10px; flex-wrap: wrap; }
        .btn-sm { font-size: 11px; padding: 3px 8px; }
        .hal-btn-remove { color: var(--error, #e53935); border-color: var(--error, #e53935); }
        .hal-btn-remove:hover { background: var(--error, #e53935); color: white; }

        /* HAL Edit Form */
        .hal-edit-form { margin-top: 10px; padding: 10px; border: 1px solid var(--border); border-radius: 6px; background: var(--bg-surface); }
        .hal-form-title { font-size: 12px; font-weight: 600; margin-bottom: 8px; color: var(--accent); }
        .hal-form-section { font-size: 11px; font-weight: 600; color: var(--text-secondary); margin-top: 10px; margin-bottom: 4px; border-top: 1px solid var(--border); padding-top: 6px; }
        .hal-form-row { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; font-size: 12px; }
        .hal-form-row label { min-width: 100px; color: var(--text-secondary); }
        .hal-form-row input[type="text"], .hal-form-row input[type="number"], .hal-form-row select {
            padding: 4px 8px; border: 1px solid var(--border); border-radius: 4px;
            background: var(--bg-primary); color: var(--text-primary); font-size: 12px;
        }
        .hal-form-row input[type="range"] { flex: 1; }
        .hal-form-buttons { display: flex; gap: 8px; margin-top: 10px; }
        .hal-cap-badge { display:inline-block; padding:1px 5px; border-radius:3px; font-size:10px; background:rgba(255,255,255,0.1); color:rgba(255,255,255,0.6); margin:0 1px; }

        /* HAL Device Enable Toggle */
        .hal-enable-toggle { display:inline-flex; align-items:center; cursor:pointer; margin-right:4px; vertical-align:middle; }
        .hal-enable-toggle input { display:none; }
        .hal-toggle-track { width:28px; height:14px; border-radius:7px; background:rgba(255,255,255,0.2); position:relative; transition:background .2s; flex-shrink:0; }
        .hal-enable-toggle input:checked + .hal-toggle-track { background:#4aaa88; }
        .hal-toggle-track::after { content:''; position:absolute; top:2px; left:2px; width:10px; height:10px; border-radius:50%; background:#fff; transition:left .2s; }
        .hal-enable-toggle input:checked + .hal-toggle-track::after { left:16px; }

        /* HAL card header icon action buttons */
        .hal-icon-btn { display:inline-flex; align-items:center; justify-content:center; width:24px; height:24px; padding:0; border:none; border-radius:4px; background:transparent; color:rgba(255,255,255,0.5); cursor:pointer; transition:background .15s, color .15s; flex-shrink:0; }
        .hal-icon-btn:hover { background:rgba(255,255,255,0.12); color:rgba(255,255,255,0.9); }
        .hal-icon-btn-danger { color:rgba(220,80,80,0.7); }
        .hal-icon-btn-danger:hover { background:rgba(220,80,80,0.15); color:#e05555; }
        .hal-remove-row { display:flex; align-items:center; gap:6px; justify-content:flex-end; margin-top:8px; padding-top:8px; border-top:1px solid rgba(255,255,255,0.08); font-size:12px; }

        /* HAL Add Device */
        .hal-add-row { display: flex; gap: 8px; align-items: center; }

        /* ===== Audio Tab Sub-Navigation ===== */
        .audio-subnav {
            display: flex;
            gap: 2px;
            background: var(--bg-card);
            border-radius: 10px;
            padding: 3px;
            margin-bottom: 12px;
        }
        .audio-subnav-btn {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
            padding: 10px 8px;
            font-size: 13px;
            font-weight: 600;
            border: none;
            border-radius: 8px;
            background: transparent;
            color: var(--text-secondary);
            cursor: pointer;
            transition: all 0.2s ease;
        }
        .audio-subnav-btn.active {
            background: var(--accent);
            color: #000;
        }
        .audio-subnav-btn:not(.active):hover {
            background: var(--bg-input);
        }
        .audio-subnav-btn:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: -2px;
        }
        .audio-subnav-btn svg {
            flex-shrink: 0;
        }

        /* Sub-view panels */
        .audio-subview {
            display: none;
        }
        .audio-subview.active {
            display: block;
            animation: fadeIn 0.15s ease;
        }

        /* ===== Channel Strip Grid ===== */
        .channel-strip-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 12px;
        }
        .channel-strip {
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 14px;
            display: flex;
            flex-direction: column;
            gap: 8px;
            transition: box-shadow 0.2s ease;
        }
        .channel-strip:hover {
            box-shadow: 0 4px 16px var(--shadow);
        }
        .channel-strip-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .channel-device-name {
            font-size: 14px;
            font-weight: 600;
            color: var(--text-primary);
        }
        .channel-strip-sub {
            font-size: 11px;
            color: var(--text-secondary);
            margin-top: -4px;
        }
        .channel-status {
            font-size: 11px;
            font-weight: 600;
            padding: 2px 8px;
            border-radius: 10px;
            transition: background-color 0.2s ease, color 0.2s ease;
        }
        .channel-status.status-ok {
            background: rgba(76,175,80,0.15);
            color: var(--success);
        }
        .channel-status.status-off {
            background: rgba(158,158,158,0.15);
            color: var(--text-disabled);
        }

        /* VU meters in channel strips */
        .channel-vu-pair {
            display: flex;
            align-items: flex-end;
            gap: 4px;
            justify-content: center;
            min-height: 80px;
        }
        .channel-vu-wrapper {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 2px;
        }
        .channel-vu-canvas {
            border-radius: 3px;
            background: var(--bg-card);
        }
        .channel-vu-label {
            font-size: 10px;
            color: var(--text-secondary);
            text-align: center;
        }
        .channel-vu-readout {
            font-size: 11px;
            color: var(--text-secondary);
            font-family: monospace;
            margin-left: 4px;
            align-self: center;
        }

        /* Channel controls */
        .channel-control-row {
            display: flex;
            align-items: center;
            gap: 6px;
        }
        .channel-control-label {
            font-size: 11px;
            color: var(--text-secondary);
            min-width: 32px;
        }
        .channel-gain-slider {
            flex: 1;
            height: 4px;
            -webkit-appearance: none;
            background: var(--bg-card);
            border-radius: 2px;
            outline: none;
        }
        .channel-gain-slider:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .channel-gain-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--accent);
            cursor: pointer;
        }
        .channel-gain-value {
            font-size: 11px;
            color: var(--text-secondary);
            font-family: monospace;
            min-width: 50px;
            text-align: right;
        }
        .channel-delay-input {
            width: 60px;
            font-size: 12px;
            padding: 4px 6px;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 4px;
            color: var(--text-primary);
        }
        .channel-delay-input:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .channel-button-row {
            display: flex;
            gap: 4px;
        }
        .channel-btn {
            flex: 1;
            padding: 6px 8px;
            font-size: 11px;
            font-weight: 600;
            border: 1px solid var(--border);
            border-radius: 6px;
            background: var(--bg-card);
            color: var(--text-secondary);
            cursor: pointer;
            transition: background-color 0.15s ease, color 0.15s ease, border-color 0.15s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 4px;
        }
        .channel-btn:hover {
            border-color: var(--accent);
            color: var(--text-primary);
        }
        .channel-btn:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .channel-btn.active {
            background: var(--accent);
            color: #000;
            border-color: var(--accent);
        }
        .channel-btn-wide {
            flex: none;
            width: 100%;
        }
        .channel-dsp-section {
            display: flex;
            flex-direction: column;
            gap: 4px;
            padding-top: 4px;
            border-top: 1px solid var(--border);
        }
        .channel-dsp-row {
            display: flex;
            gap: 4px;
        }

        /* ===== Matrix Grid ===== */
        .matrix-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 11px;
        }
        .matrix-table th, .matrix-table td {
            padding: 6px 4px;
            text-align: center;
            border: 1px solid var(--border);
        }
        .matrix-col-hdr {
            font-size: 10px;
            font-weight: 600;
            color: var(--text-secondary);
            writing-mode: vertical-lr;
            transform: rotate(180deg);
            max-width: 36px;
            white-space: nowrap;
        }
        .matrix-row-hdr {
            font-size: 10px;
            font-weight: 600;
            color: var(--text-secondary);
            text-align: right !important;
            padding-right: 8px !important;
            white-space: nowrap;
        }
        .matrix-cell {
            cursor: pointer;
            color: var(--text-disabled);
            font-family: monospace;
            transition: background-color 0.15s ease;
        }
        .matrix-cell:hover {
            background: var(--bg-input);
        }
        .matrix-cell:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: -2px;
        }
        .matrix-cell.matrix-active {
            background: rgba(255,152,0,0.12);
            color: var(--accent);
            font-weight: 600;
        }
        .matrix-cell.matrix-active:hover {
            background: rgba(255,152,0,0.22);
        }
        .matrix-presets {
            display: flex;
            gap: 6px;
            margin-top: 10px;
            flex-wrap: wrap;
        }
        .btn-sm {
            min-height: 32px;
            padding: 6px 12px;
            font-size: 12px;
            width: auto;
        }

        /* Matrix gain popup */
        .matrix-gain-popup {
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: var(--bg-surface);
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 8px 32px var(--shadow);
            z-index: 1000;
            min-width: 260px;
            display: none;
        }
        .matrix-gain-popup-inner {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .matrix-gain-popup-inner label {
            font-size: 14px;
            font-weight: 600;
            color: var(--text-primary);
        }

        /* ===== PEQ / DSP Overlay ===== */
        @keyframes peqSlideUp {
            from { opacity: 0; transform: translateY(24px); }
            to { opacity: 1; transform: translateY(0); }
        }
        .peq-overlay {
            position: fixed;
            inset: 0;
            z-index: 2000;
            background: var(--bg-primary);
            backdrop-filter: blur(4px);
            -webkit-backdrop-filter: blur(4px);
            display: none;
            flex-direction: column;
            overflow-y: auto;
            animation: peqSlideUp 0.25s ease-out;
        }
        .peq-overlay-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 16px;
            background: var(--bg-surface);
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 1;
        }
        .peq-overlay-title {
            font-size: 16px;
            font-weight: 700;
            color: var(--text-primary);
        }
        .peq-overlay-close {
            background: none;
            border: none;
            color: var(--text-secondary);
            cursor: pointer;
            padding: 4px;
            border-radius: 6px;
            transition: background 0.15s;
        }
        .peq-overlay-close:hover {
            background: var(--bg-input);
            color: var(--text-primary);
        }
        .peq-overlay-close:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .peq-graph-wrap {
            padding: 8px 16px;
            flex-shrink: 0;
        }
        .peq-overlay-canvas {
            width: 100%;
            height: 220px;
            border-radius: 8px;
            border: 1px solid var(--border);
        }
        .peq-band-table-wrap {
            padding: 0 16px;
            overflow-x: auto;
        }
        .peq-band-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
        }
        .peq-band-table th {
            text-align: left;
            padding: 6px 4px;
            border-bottom: 2px solid var(--accent);
            color: var(--accent);
            font-size: 10px;
            font-weight: 600;
            text-transform: uppercase;
        }
        .peq-band-table td {
            padding: 4px;
            border-bottom: 1px solid var(--border);
            vertical-align: middle;
        }
        .peq-band-table tr:hover td {
            background: var(--bg-card);
        }
        .peq-input {
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: 4px;
            color: var(--text-primary);
            padding: 4px 6px;
            font-size: 12px;
            width: 70px;
        }
        .peq-input:focus {
            border-color: var(--accent);
            outline: none;
        }
        .peq-input:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .peq-type-sel {
            width: 80px;
        }
        .peq-overlay-actions {
            display: flex;
            gap: 6px;
            padding: 12px 16px;
            border-top: 1px solid var(--border);
            flex-wrap: wrap;
        }

        /* ===== Confirmation Dialog ===== */
        .confirm-overlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.6);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 10000;
        }

        .confirm-dialog {
            background: var(--bg-surface);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 20px;
            max-width: 400px;
            width: 90%;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
        }

        .confirm-header {
            font-size: 1.1em;
            font-weight: 600;
            margin-bottom: 12px;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .confirm-body {
            color: var(--text-secondary);
            margin-bottom: 20px;
            line-height: 1.5;
        }

        .confirm-actions {
            display: flex;
            justify-content: flex-end;
            gap: 10px;
        }

        .confirm-cancel-btn {
            background: var(--bg-card);
            color: var(--text-primary);
            border: 1px solid var(--border);
        }

        .confirm-confirm-btn {
            background: var(--error, #F44336);
            color: #fff;
            border: none;
        }

        .confirm-confirm-btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }

        .confirm-confirm-btn:not(:disabled):hover {
            background: #c0392b;
        }

/* ===== 04-canvas.css ===== */

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
        .dual-canvas-grid .canvas-panel {
            min-width: 0;
        }
        .canvas-panel-title {
            font-size: 12px;
            font-weight: 600;
            color: var(--text-secondary);
            margin-bottom: 4px;
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

        .dsp-freq-canvas {
            display: block;
            width: 100%;
            height: 220px;
            border-radius: 8px;
            background: var(--bg-card);
        }

/* ===== 05-responsive.css ===== */

        /* Mobile: single column */
        @media (max-width: 767px) {
            .info-box-compact {
                grid-template-columns: 1fr;
            }
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

        /* Dual ADC responsive grid */
        @media (max-width: 768px) {
            .dual-canvas-grid {
                grid-template-columns: 1fr;
            }
        }

        /* Mobile: Audio tab channel strips stack vertically */
        @media (max-width: 767px) {
            .channel-strip-grid {
                grid-template-columns: 1fr;
            }
            .audio-subnav {
                gap: 2px;
                padding: 6px 8px;
            }
            .audio-subnav-btn {
                padding: 6px 8px;
                font-size: 11px;
                min-height: 44px;
            }
            .channel-btn {
                min-height: 44px;
                min-width: 44px;
            }
            .matrix-cell {
                min-width: 36px;
                min-height: 36px;
            }
            .peq-overlay-canvas {
                height: 160px;
            }
            .peq-band-table {
                font-size: 11px;
            }
            .peq-input {
                width: 56px;
                font-size: 11px;
            }
            .matrix-col-hdr {
                font-size: 9px;
            }
            /* Matrix horizontal scroll on narrow screens */
            .matrix-table {
                display: block;
                overflow-x: auto;
                -webkit-overflow-scrolling: touch;
            }
        }

        /* Ultra-small screens (< 360px) */
        @media (max-width: 359px) {
            .audio-subnav-btn {
                padding: 6px 4px;
                font-size: 10px;
                gap: 2px;
            }
            .audio-subnav-btn svg {
                width: 14px;
                height: 14px;
            }
            .channel-strip-grid {
                grid-template-columns: 1fr;
            }
        }

        /* Tablet: 2-col channel strips */
        @media (min-width: 768px) and (max-width: 1023px) {
            .channel-strip-grid {
                grid-template-columns: repeat(2, 1fr);
            }
        }

        /* Desktop: wider channel strips */
        @media (min-width: 1024px) {
            .channel-strip-grid {
                grid-template-columns: repeat(2, 1fr);
            }
            .peq-overlay-canvas {
                height: 280px;
            }
        }

/* ===== 06-health-dashboard.css ===== */

        .health-device-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
            gap: 12px;
        }
        .health-device-card {
            background: var(--bg-surface);
            border-radius: 8px;
            padding: 12px;
            border-left: 3px solid var(--border);
        }
        .health-device-card.state-ok { border-left-color: var(--success); }
        .health-device-card.state-warn { border-left-color: var(--warning); }
        .health-device-card.state-error { border-left-color: var(--error); }
        .health-device-card .device-name { font-weight: 600; font-size: 14px; }
        .health-device-card .device-meta { font-size: 12px; color: var(--text-secondary); margin-top: 4px; }
        .health-device-card .device-stats {
            font-size: 12px;
            margin-top: 8px;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 2px;
        }
        .health-device-card .device-stats .stat-label { color: var(--text-secondary); }
        .health-device-card .device-stats .stat-val-err { color: var(--error); font-weight: 600; }
        .health-device-card .device-stats .stat-val-ok { color: var(--text-secondary); }

        .health-event-list { max-height: 400px; overflow-y: auto; }
        .health-event-list table { width: 100%; border-collapse: collapse; font-size: 13px; }
        .health-event-list th {
            text-align: left;
            padding: 6px 8px;
            border-bottom: 1px solid var(--border);
            color: var(--text-secondary);
            font-weight: 500;
            position: sticky;
            top: 0;
            background: var(--bg-card);
        }
        .health-event-list td {
            padding: 4px 8px;
            border-bottom: 1px solid var(--border);
        }
        .health-event-list tr:hover { background: var(--bg-input); }
        .health-event-list .sev-i { color: var(--info); }
        .health-event-list .sev-w { color: var(--warning); }
        .health-event-list .sev-e { color: var(--error); }
        .health-event-list .sev-c { color: #CE93D8; }
        .health-event-list .corr { color: var(--text-secondary); font-size: 11px; }

        .health-counter-table { width: 100%; font-size: 13px; border-collapse: collapse; }
        .health-counter-table td { padding: 6px 8px; border-bottom: 1px solid var(--border); }
        .health-counter-table .cnt-err { color: var(--error); font-weight: 600; }
        .health-counter-table .cnt-warn { color: var(--warning); }
        .health-counter-table .cnt-none { color: var(--text-secondary); }
        .health-counter-table .subsys-label { font-weight: 500; }

        .health-empty {
            text-align: center;
            padding: 24px;
            color: var(--text-secondary);
            font-size: 13px;
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
            <button class="sidebar-item" data-tab="devices" onclick="switchTab('devices')">
                <svg viewBox="0 0 24 24"><path d="M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11"/></svg>
                <span>Devices</span>
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
            <button class="sidebar-item" data-tab="health" onclick="switchTab('health')">
                <svg viewBox="0 0 24 24"><path d="M13,3V9H21V3M13,21H21V11H13M3,21H11V15H3M3,13H11V3H3V13Z"/></svg>
                <span>Health</span>
            </button>
            <button class="sidebar-item" data-tab="debug" onclick="switchTab('debug')">
                <svg viewBox="0 0 24 24"><path d="M20 8h-2.81c-.45-.78-1.07-1.45-1.82-1.96L17 4.41 15.59 3l-2.17 2.17C12.96 5.06 12.49 5 12 5c-.49 0-.96.06-1.41.17L8.41 3 7 4.41l1.62 1.63C7.88 6.55 7.26 7.22 6.81 8H4v2h2.09c-.05.33-.09.66-.09 1v1H4v2h2v1c0 .34.04.67.09 1H4v2h2.81c1.04 1.79 2.97 3 5.19 3s4.15-1.21 5.19-3H20v-2h-2.09c.05-.33.09-.66.09-1v-1h2v-2h-2v-1c0-.34-.04-.67-.09-1H20V8zm-6 8h-4v-2h4v2zm0-4h-4v-2h4v2z"/></svg>
                <span>Debug</span>
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
        <button class="tab" data-tab="devices" onclick="switchTab('devices')">
            <svg viewBox="0 0 24 24"><path d="M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11"/></svg>
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
        <button class="tab" data-tab="health" onclick="switchTab('health')">
            <svg viewBox="0 0 24 24"><path d="M13,3V9H21V3M13,21H21V11H13M3,21H11V15H3M3,13H11V3H3V13Z"/></svg>
        </button>
        <button class="tab" data-tab="debug" onclick="switchTab('debug')">
            <svg viewBox="0 0 24 24"><path d="M20 19V7H4v12h16m0-16c1.11 0 2 .89 2 2v14c0 1.11-.89 2-2 2H4c-1.11 0-2-.89-2-2V5c0-1.11.89-2 2-2h16zM7 9h10v2H7V9zm0 4h7v2H7v-2z"/></svg>
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
            <!-- Audio Sub-Navigation -->
            <div class="audio-subnav">
                <button class="audio-subnav-btn active" data-view="inputs" onclick="switchAudioSubView('inputs')">
                    <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M14,12L10,8V11H2V13H10V16M20,18V6C20,4.89 19.1,4 18,4H6A2,2 0 0,0 4,6V9H6V6H18V18H6V15H4V18A2,2 0 0,0 6,20H18A2,2 0 0,0 20,18Z"/></svg>
                    Inputs
                </button>
                <button class="audio-subnav-btn" data-view="matrix" onclick="switchAudioSubView('matrix')">
                    <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M14,14H7V16H14M19,19H5V8H19M19,3H18V1H16V3H8V1H6V3H5C3.89,3 3,3.9 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V5A2,2 0 0,0 19,3M14,10H7V12H14V10Z"/></svg>
                    Matrix
                </button>
                <button class="audio-subnav-btn" data-view="outputs" onclick="switchAudioSubView('outputs')">
                    <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M23,12L19,8V11H7V13H19V16M1,18V6C1,4.89 1.9,4 3,4H15A2,2 0 0,1 17,6V9H15V6H3V18H15V15H17V18A2,2 0 0,1 15,20H3A2,2 0 0,1 1,18Z"/></svg>
                    Outputs
                </button>
                <button class="audio-subnav-btn" data-view="siggen" onclick="switchAudioSubView('siggen')">
                    <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M16,11.78L20.24,4.45L21.97,5.45L16.74,14.5L10.23,10.75L5.46,19H22V21H2V3H4V17.54L9.5,8L16,11.78Z"/></svg>
                    SigGen
                </button>
            </div>

            <!-- ===== INPUTS Sub-View ===== -->
            <div id="audio-sv-inputs" class="audio-subview active">
                <div class="channel-strip-grid" id="audio-inputs-container">
                    <!-- Dynamically populated from audioChannelMap -->
                    <div class="card" style="text-align:center;padding:32px;color:var(--text-secondary)">
                        Waiting for device data...
                    </div>
                </div>



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




            </div><!-- end audio-sv-inputs -->

            <!-- ===== MATRIX Sub-View ===== -->
            <div id="audio-sv-matrix" class="audio-subview">
                <div class="card">
                    <div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
                        Routing Matrix
                        <div style="display:flex;gap:4px;">
                            <button class="btn btn-sm btn-secondary" onclick="matrixPreset1to1()">1:1 Pass</button>
                            <button class="btn btn-sm btn-secondary" onclick="matrixPresetClear()">Clear</button>
                        </div>
                    </div>
                    <div id="audio-matrix-container" style="overflow-x:auto;">
                        <div style="text-align:center;padding:32px;color:var(--text-secondary)">Waiting for device data...</div>
                    </div>
                </div>
            </div>

            <!-- ===== OUTPUTS Sub-View ===== -->
            <div id="audio-sv-outputs" class="audio-subview">
                <div class="channel-strip-grid" id="audio-outputs-container">
                    <!-- Dynamically populated from audioChannelMap -->
                    <div class="card" style="text-align:center;padding:32px;color:var(--text-secondary)">
                        Waiting for device data...
                    </div>
                </div>




            </div><!-- end audio-sv-outputs -->

            <!-- ===== SIGGEN Sub-View ===== -->
            <div id="audio-sv-siggen" class="audio-subview">
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
            </div><!-- end audio-sv-siggen -->
        </section>

        <!-- DSP tab removed — DSP controls merged into Audio tab overlays -->

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
                            <a href="#" id="currentVersionNotes" class="release-notes-link" onclick="showReleaseNotesFor('current'); return false;" title="View release notes"><svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M13 9H11V7H13V9M13 17H11V11H13V17M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2M12 20C7.58 20 4 16.42 4 12C4 7.58 7.58 4 12 4C16.42 4 20 7.58 20 12C20 16.42 16.42 20 12 20Z"/></svg></a>
                        </span>
                    </div>
                    <div class="version-row" id="latestVersionRow">
                        <span class="version-label">Latest Version</span>
                        <span class="version-value version-update">
                            <span id="latestVersion" style="opacity: 0.6; font-style: italic;">Checking...</span>
                            <a href="#" id="latestVersionNotes" class="release-notes-link" onclick="showReleaseNotesFor('latest'); return false;" title="View release notes"><svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M13 9H11V7H13V9M13 17H11V11H13V17M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2M12 20C7.58 20 4 16.42 4 12C4 7.58 7.58 4 12 4C16.42 4 20 7.58 20 12C20 16.42 16.42 20 12 20Z"/></svg></a>
                        </span>
                    </div>
                </div>
                
                <!-- Inline Release Notes Accordion -->
                <div id="inlineReleaseNotes" class="release-notes-inline">
                    <div class="release-notes-header">
                        <span id="inlineReleaseNotesTitle" class="release-notes-title">Release Notes</span>
                        <button type="button" class="release-notes-close" onclick="toggleInlineReleaseNotes(false)" aria-label="Close"><svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg></button>
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
                <div class="toggle-row">
                    <div>
                        <div class="toggle-label">Update Channel</div>
                        <div class="toggle-sublabel">Stable: tested releases. Beta: prerelease builds.</div>
                    </div>
                    <select id="otaChannelSelect" class="select-input" onchange="setOtaChannel()">
                        <option value="0">Stable</option>
                        <option value="1">Beta</option>
                    </select>
                </div>
                <button class="btn btn-secondary mb-8" onclick="toggleReleasesBrowser()" id="browseReleasesBtn">Browse Releases</button>
                <div id="releasesBrowser" class="hidden" style="margin-bottom:12px;">
                    <div class="release-notes-header">
                        <span class="release-notes-title">Available Releases</span>
                        <button type="button" class="release-notes-close" onclick="toggleReleasesBrowser()" aria-label="Close"><svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg></button>
                    </div>
                    <div id="releaseListContent" style="max-height:280px;overflow-y:auto;">
                        <div id="releaseListLoading" class="text-secondary" style="font-size:13px;padding:8px;">Loading...</div>
                        <div id="releaseListItems"></div>
                    </div>
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

        <!-- ===== HEALTH TAB ===== -->
        <section id="health" class="panel">
            <!-- Quick Actions -->
            <div class="card" style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
                <h3 style="flex:1;margin:0;">Health Dashboard</h3>
                <button class="btn btn-primary" onclick="healthSnapshot()">Snapshot</button>
                <button class="btn btn-secondary" onclick="healthDownloadJournal()">Download</button>
                <button class="btn btn-secondary" onclick="healthClearJournal()">Clear</button>
                <span id="healthLastUpdate" style="font-size:12px;color:var(--text-secondary);"></span>
            </div>

            <!-- Device Health Grid -->
            <div class="card">
                <div class="card-title">Device Health</div>
                <div id="healthDeviceGrid" class="health-device-grid"></div>
            </div>

            <!-- Error Counters -->
            <div class="card">
                <div class="card-title">Error Counters</div>
                <div id="healthErrorCounters"></div>
            </div>

            <!-- Recent Events Table -->
            <div class="card">
                <div class="card-title">Recent Events</div>
                <div id="healthEventList" class="health-event-list"></div>
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
                    <div class="toggle-row">
                        <div>
                            <div class="toggle-label">Audio Stream Rate</div>
                            <div class="toggle-sublabel">Audio level update interval</div>
                        </div>
                        <select id="audioUpdateRateSelect" onchange="setAudioUpdateRate()" style="padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-primary);font-size:14px;">
                            <option value="33">30 fps (33ms)</option>
                            <option value="50">20 fps (50ms)</option>
                            <option value="100" selected>10 fps (100ms)</option>
                            <option value="200">5 fps (200ms)</option>
                        </select>
                    </div>
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
                <div class="form-row mb-8" id="moduleCategoryRow">
                    <label style="margin-right: 8px; font-weight: 500;">Categories:</label>
                    <div id="moduleChips" class="chip-container">
                        <!-- Chips auto-populated from received messages -->
                    </div>
                    <button class="btn-chip btn-chip-action" onclick="clearModuleFilter()"
                            title="Show all categories" style="margin-left:4px;">All</button>
                </div>
                <div class="form-row mb-8">
                    <input type="text" id="debugSearchInput" class="form-input" placeholder="Search logs..."
                           oninput="setDebugSearch(this.value)" style="flex:1; font-size:13px;">
                    <button class="btn-chip btn-chip-action" onclick="clearDebugSearch()"
                            style="margin-left:4px;" title="Clear search"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg></button>
                    <button class="btn-chip btn-chip-action" id="timestampToggle"
                            onclick="toggleTimestampMode()" title="Toggle timestamp format"
                            style="margin-left:4px;">Uptime</button>
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
                    <div class="info-row" id="heapCriticalRow" style="display:none">
                        <span class="info-label" style="color:var(--danger)">Heap Critical</span>
                        <span class="info-value" id="heapCriticalValue" style="color:var(--danger)">YES</span>
                    </div>
                    <div class="info-row" id="dmaAllocFailRow" style="display:none">
                        <span class="info-label" style="color:var(--danger)">DMA Alloc Failed</span>
                        <span class="info-value" id="dmaAllocFailValue" style="color:var(--danger)">&mdash;</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">PSRAM Free</span>
                        <span class="info-value" id="psramFree">--</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">PSRAM Total</span>
                        <span class="info-value" id="psramTotal">--</span>
                    </div>
                    <div class="info-row" id="psramFallbackRow" style="display:none">
                        <span class="info-label" style="color:var(--warning)">PSRAM Fallbacks</span>
                        <span class="info-value" id="psramFallbackCount" style="color:var(--warning)">0</span>
                    </div>
                </div>
                <!-- PSRAM Budget Breakdown -->
                <div class="collapsible-header" onclick="togglePsramBudget()" id="psramBudgetHeader" style="display:none;padding:0 16px;">
                    <span style="font-size:0.9em;color:var(--text-secondary);">PSRAM Budget</span>
                    <span style="display:flex;align-items:center;gap:6px;">
                        <span id="psramPressureBadge" class="badge" style="display:none"></span>
                        <span id="psramFallbackBadge" class="badge badge-amber" style="display:none"></span>
                        <svg viewBox="0 0 24 24" id="psramBudgetChevron"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z"/></svg>
                    </span>
                </div>
                <div class="collapsible-content" id="psramBudgetContent">
                    <table class="budget-table" id="psramBudgetTable">
                        <thead>
                            <tr><th>Subsystem</th><th>Bytes</th><th>Type</th></tr>
                        </thead>
                        <tbody></tbody>
                    </table>
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

            <!-- DSP CPU Load -->
            <div class="card" id="dsp-cpu-section" style="display:none">
                <div class="card-title">DSP CPU Load</div>
                <div class="info-box-compact">
                    <div class="info-row">
                        <span class="info-label">Per-Input DSP</span>
                        <span class="info-value" id="dsp-cpu-input">—</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Pipeline Total</span>
                        <span class="info-value" id="dsp-cpu-pipeline">—</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Frame Time</span>
                        <span class="info-value" id="dsp-frame-us">—</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Matrix Mix</span>
                        <span class="info-value" id="dsp-matrix-us">—</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Output DSP</span>
                        <span class="info-value" id="dsp-output-us">—</span>
                    </div>
                    <div class="info-row" id="dsp-cpu-warn-row" style="display:none">
                        <span class="info-label" style="color:var(--warning)">FIR Bypassed</span>
                        <span class="info-value" id="dsp-fir-bypass" style="color:var(--warning)">0</span>
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
                            <th onclick="sortPinTable(0)" data-col="0">GPIO <span class="sort-arrow"><svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg></span></th>
                            <th onclick="sortPinTable(1)" data-col="1">Function <span class="sort-arrow"><svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg></span></th>
                            <th onclick="sortPinTable(2)" data-col="2">Device <span class="sort-arrow"><svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg></span></th>
                            <th onclick="sortPinTable(3)" data-col="3">Category <span class="sort-arrow"><svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg></span></th>
                        </tr>
                    </thead>
                    <tbody id="pinTableBody">
                        <tr><td colspan="4" style="text-align:center;opacity:0.5">Waiting for pin data...</td></tr>
                    </tbody>
                </table>
            </div>

        </section>

        <!-- ===== DEVICES TAB ===== -->
        <section id="devices" class="panel">
            <h2>HAL Devices</h2>
            <div class="card-row" style="margin-bottom:12px;gap:8px;">
                <button id="hal-rescan-btn" class="btn btn-primary" onclick="triggerHalRescan()">Rescan Devices</button>
                <button class="btn" onclick="importDeviceYaml()">Import YAML</button>
                <button class="btn" onclick="halOpenCustomUpload()">
                  <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true" style="margin-right:4px;vertical-align:-2px;"><path d="M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2M13,9V3.5L18.5,9H13M9,13H11V11H13V13H15V15H13V17H11V15H9V13Z"/></svg>
                  Custom Device
                </button>
            </div>
            <div class="card" style="padding:10px 16px;margin-bottom:4px;">
                <div style="display:flex;align-items:center;justify-content:space-between;">
                    <div>
                        <span style="font-weight:500;font-size:13px;">Auto-configure detected devices</span>
                        <div style="font-size:11px;opacity:0.55;margin-top:2px;">Automatically initialise devices with a recognised EEPROM or GPIO ID</div>
                    </div>
                    <label class="switch"><input type="checkbox" id="halAutoDiscovery" onchange="setHalAutoDiscovery(this.checked)"><span class="slider round"></span></label>
                </div>
            </div>
            <div id="hal-capacity-indicator" class="hal-capacity"></div>
            <div id="hal-device-list"></div>
            <div id="hal-unknown-list"></div>

            <!-- Add Device Panel -->
            <div class="card" style="margin-top:12px;">
                <div class="card-title">Add Device</div>
                <div class="hal-add-row">
                    <select id="halAddPresetSelect" style="flex:1;font-size:12px;padding:4px 8px;border-radius:4px;border:1px solid var(--border);background:var(--bg-surface);color:var(--text-primary);">
                        <option value="">-- Select Device --</option>
                    </select>
                    <button class="btn btn-sm" onclick="halAddFromPreset()">Load Presets</button>
                    <button class="btn btn-primary btn-sm" onclick="halRegisterPreset()">Register</button>
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

    <script>        // ===== WebSocket instance and reconnect state =====
        let ws = null;
        let wsReconnectDelay = 2000;
        const WS_MIN_RECONNECT_DELAY = 2000;
        const WS_MAX_RECONNECT_DELAY = 30000;
        let wasDisconnectedDuringUpdate = false;
        let hadPreviousConnection = false;

        // ===== Session & Authentication =====
        // Cookie is HttpOnly — browser sends it automatically with credentials: 'include'

        // Global fetch wrapper for API calls (handles 401 Unauthorized)
        async function apiFetch(url, options = {}) {
            const defaultOptions = {
                credentials: 'include'
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

        // ===== WebSocket Init =====
        function initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.hostname;
            ws = new WebSocket(`${wsProtocol}//${wsHost}:81`);
            ws.binaryType = 'arraybuffer';

            ws.onopen = async function() {
                console.log('WebSocket connected');

                // Fetch one-time WS token from server (cookie sent automatically)
                try {
                    const resp = await apiFetch('/api/ws-token');
                    const data = await resp.json();
                    if (data.success && data.token) {
                        ws.send(JSON.stringify({ type: 'auth', token: data.token }));
                    } else {
                        console.error('Failed to get WS token:', data.error);
                        window.location.href = '/login';
                    }
                } catch (e) {
                    console.error('WS token fetch failed:', e);
                    window.location.href = '/login';
                }
            };

            ws.onmessage = function(event) {
                if (event.data instanceof ArrayBuffer) { handleBinaryMessage(event.data); return; }
                const data = JSON.parse(event.data);
                routeWsMessage(data);
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

        // ===== WebSocket Send Helper =====
        function wsSend(type, payload) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return false;
            ws.send(JSON.stringify(Object.assign({ type: type }, payload || {})));
            return true;
        }

//# sourceURL=01-core.js

        // Binary WS message handler (waveform + spectrum)
        var _binDiag = { wf: 0, sp: 0, last: 0 };
        function handleBinaryMessage(buf) {
            const dv = new DataView(buf);
            const type = dv.getUint8(0);
            const adc = dv.getUint8(1);
            if (type === 0x01) _binDiag.wf++;
            else if (type === 0x02) _binDiag.sp++;
            var now = Date.now();
            if (now - _binDiag.last > 2000) {
                console.log('[BIN diag] wf=' + _binDiag.wf + ' sp=' + _binDiag.sp + ' tab=' + currentActiveTab + ' bytes=' + buf.byteLength);
                _binDiag.wf = 0; _binDiag.sp = 0; _binDiag.last = now;
            }
            if (type === 0x01 && currentActiveTab === 'audio') {
                // Waveform: [type:1][adc:1][samples:256]
                if (adc < numInputLanes && buf.byteLength >= 258) {
                    const samples = new Uint8Array(buf, 2, 256);
                    waveformTarget[adc] = samples;
                    if (!waveformCurrent[adc]) waveformCurrent[adc] = new Uint8Array(samples);
                    startAudioAnimation();
                }
            } else if (type === 0x02) {
                // Spectrum: [type:1][adc:1][freq:f32LE][bands:16xf32LE]
                if (adc < numInputLanes && buf.byteLength >= 70) {
                    const freq = dv.getFloat32(2, true);
                    for (let i = 0; i < 16; i++) spectrumTarget[adc][i] = dv.getFloat32(6 + i * 4, true);
                    targetDominantFreq[adc] = freq;
                    if (currentActiveTab === 'audio') startAudioAnimation();
                }
            }
        }

        // ===== WS Message Router =====
        function routeWsMessage(data) {
            if (data.type === 'authRequired') {
                // Server requesting authentication — fetch one-time token
                apiFetch('/api/ws-token').then(r => r.json()).then(d => {
                    if (d.success && d.token) {
                        ws.send(JSON.stringify({ type: 'auth', token: d.token }));
                    } else {
                        showToast('Connection failed: token error', 'error');
                    }
                }).catch(() => {
                    showToast('Connection failed: auth error', 'error');
                });
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

                // Re-subscribe to audio stream if audio tab is active
                if (audioSubscribed && currentActiveTab === 'audio') {
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
            else if (data.type === 'wifiStatus') {
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
                appendDebugLog(data.timestamp, data.message, data.level, data.module);
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
                    }
                    // Per-ADC VU/peak data
                    if (data.adc && Array.isArray(data.adc)) {
                        for (let a = 0; a < data.adc.length && a < numInputLanes; a++) {
                            const ad = data.adc[a];
                            vuTargetArr[a][0] = ad.vu1 !== undefined ? ad.vu1 : 0;
                            vuTargetArr[a][1] = ad.vu2 !== undefined ? ad.vu2 : 0;
                            peakTargetArr[a][0] = ad.peak1 !== undefined ? ad.peak1 : 0;
                            peakTargetArr[a][1] = ad.peak2 !== undefined ? ad.peak2 : 0;
                        }
                    }
                    vuDetected = data.signalDetected !== undefined ? data.signalDetected : false;
                    startVuAnimation();
                }
                // Feed new Audio tab VU meters
                audioTabUpdateLevels(data);
            } else if (data.type === 'inputNames') {
                if (data.names && Array.isArray(data.names)) {
                    for (let i = 0; i < data.names.length && i < numInputLanes * 2; i++) {
                        inputNames[i] = data.names[i];
                    }
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
            } else if (data.type === 'dacState') {
                if (data.eeprom) handleEepromDiag(data.eeprom);
            } else if (data.type === 'eepromProgramResult') {
                showToast(data.success ? 'EEPROM programmed' : 'EEPROM program failed', data.success ? 'success' : 'error');
            } else if (data.type === 'eepromEraseResult') {
                showToast(data.success ? 'EEPROM erased' : 'EEPROM erase failed', data.success ? 'success' : 'error');
            } else if (data.type === 'halDeviceState') {
                handleHalDeviceState(data);
                if (data.unknownDevices) handleHalUnknownDevices(data.unknownDevices);
            } else if (data.type === 'audioChannelMap') {
                handleAudioChannelMap(data);
            } else if (data.type === 'diagEvent') {
                handleDiagEvent(data);
            }
        }

//# sourceURL=02-ws-router.js

        // ===== State Variables =====
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
        let currentModuleFilters = new Set();    // empty = show all; non-empty = show only these
        let knownModules = {};                   // { "WiFi": { total: 0, errors: 0, warnings: 0 }, ... }
        let debugSearchTerm = '';                // current search filter text
        let debugTimestampMode = 'relative';     // 'relative' (uptime) or 'absolute' (wall clock)
        let ntpOffsetMs = 0;                     // millis() offset to epoch (set from firmware)
        let audioSubscribed = false;
        let currentActiveTab = 'control';

        let vuSegmentedMode = localStorage.getItem('vuSegmented') === 'true';

        // LED bar mode
        let ledBarMode = localStorage.getItem('ledBarMode') === 'true';

        // Input focus state to prevent overwrites during user input
        let inputFocusState = {
            timerDuration: false,
            audioThreshold: false
        };

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

        // Settings tab state
        let currentDstOffset = 0;
        let timeUpdateInterval = null;

        // Smart sensing
        let smartAutoSettingsCollapsed = true;

        // WiFi
        let wifiConnectionPollTimer = null;

        // OTA Channel & Release Browser
        let otaChannel = 0;
        let cachedReleaseList = [];
        let releaseListLoading = false;

        // Window resize handler
        let resizeTimeout;

        // ===== Utility Functions =====
        function showToast(message, type = 'info') {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = 'toast show ' + type;

            setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }

        function formatFreq(f) {
            return f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 0 : 1) + 'k' : f.toFixed(0);
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

//# sourceURL=03-app-state.js

        // ===== Shared Audio State =====

        // Dynamic input lane count — set by audioChannelMap broadcast
        let numInputLanes = 0;
        let numAdcsDetected = 1;
        let inputNames = [];

        // Waveform and spectrum animation state — per-input, dynamically sized
        let waveformCurrent = [], waveformTarget = [];
        let spectrumCurrent = [], spectrumTarget = [];
        let currentDominantFreq = [], targetDominantFreq = [];
        let audioAnimFrameId = null;

        // Spectrum peak hold state — per-input
        let spectrumPeaks = [], spectrumPeakTimes = [];

        // VU meter animation state — per-input [lane][L=0,R=1]
        let vuCurrent = [], vuTargetArr = [];
        let peakCurrent = [], peakTargetArr = [];
        let vuDetected = false;
        let vuAnimFrameId = null;

        // Resize all per-input audio arrays when audioChannelMap reports new count
        function resizeAudioArrays(count) {
            if (count === numInputLanes) return;
            numInputLanes = count;
            while (waveformCurrent.length < count) {
                waveformCurrent.push(null);
                waveformTarget.push(null);
                spectrumCurrent.push(new Float32Array(16));
                spectrumTarget.push(new Float32Array(16));
                currentDominantFreq.push(0);
                targetDominantFreq.push(0);
                spectrumPeaks.push(new Float32Array(16));
                spectrumPeakTimes.push(new Float64Array(16));
                vuCurrent.push([0, 0]);
                vuTargetArr.push([0, 0]);
                peakCurrent.push([0, 0]);
                peakTargetArr.push([0, 0]);
            }
            waveformCurrent.length = count;
            waveformTarget.length = count;
            spectrumCurrent.length = count;
            spectrumTarget.length = count;
            currentDominantFreq.length = count;
            targetDominantFreq.length = count;
            spectrumPeaks.length = count;
            spectrumPeakTimes.length = count;
            vuCurrent.length = count;
            vuTargetArr.length = count;
            peakCurrent.length = count;
            peakTargetArr.length = count;
            while (inputNames.length < count * 2) inputNames.push('Input ' + (inputNames.length + 1));
            inputNames.length = count * 2;
        }

//# sourceURL=04-shared-audio.js

        // ===== Audio Tab Controller =====
        // Unified Audio tab with sub-views: Inputs | Matrix | Outputs | SigGen
        // Dynamically populated from HAL device channel map.

        // Channel map state (received from firmware via WS)
        let audioChannelMap = null;
        let audioSubView = 'inputs';  // 'inputs' | 'matrix' | 'outputs' | 'siggen'

        // Per-channel VU state for inputs (lane-indexed) and outputs (sink-indexed)
        let inputVuCurrent = [], inputVuTarget = [];
        let outputVuCurrent = [], outputVuTarget = [];
        let audioTabAnimId = null;

        // ===== Channel Map Handler =====
        function handleAudioChannelMap(data) {
            audioChannelMap = data;

            // Resize shared audio arrays (waveform, spectrum, VU) to match input count
            resizeAudioArrays(data.inputs ? data.inputs.length : 0);

            // Resize VU arrays to match channel count
            while (inputVuCurrent.length < (data.inputs || []).length) {
                inputVuCurrent.push([0, 0]);
                inputVuTarget.push([0, 0]);
            }
            while (outputVuCurrent.length < (data.outputs || []).length * 2) {
                outputVuCurrent.push(0);
                outputVuTarget.push(0);
            }

            // Re-render current sub-view if audio tab is active
            if (currentActiveTab === 'audio') {
                renderAudioSubView();
            }
        }

        // ===== Sub-View Navigation =====
        function switchAudioSubView(view) {
            audioSubView = view;
            // Update sub-nav buttons
            document.querySelectorAll('.audio-subnav-btn').forEach(function(btn) {
                btn.classList.toggle('active', btn.dataset.view === view);
            });
            // Update sub-view panels
            document.querySelectorAll('.audio-subview').forEach(function(panel) {
                panel.classList.toggle('active', panel.id === 'audio-sv-' + view);
            });

            // Subscribe to audio stream when inputs or outputs sub-view is active
            if ((view === 'inputs' || view === 'outputs') && ws && ws.readyState === WebSocket.OPEN) {
                if (!audioSubscribed) {
                    audioSubscribed = true;
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                }
            }

            renderAudioSubView();
        }

        function renderAudioSubView() {
            if (!audioChannelMap) return;
            switch (audioSubView) {
                case 'inputs':  renderInputStrips(); break;
                case 'matrix':  renderMatrixGrid(); break;
                case 'outputs': renderOutputStrips(); break;
                case 'siggen':  renderSigGenView(); break;
            }
        }

        // ===== Input Channel Strips =====
        function renderInputStrips() {
            var container = document.getElementById('audio-inputs-container');
            if (!container || !audioChannelMap) return;

            var inputs = audioChannelMap.inputs || [];
            if (container.dataset.rendered === String(inputs.length)) return;  // Already rendered

            var html = '';
            for (var i = 0; i < inputs.length; i++) {
                var inp = inputs[i];
                var statusClass = inp.ready ? 'status-ok' : 'status-off';
                var statusText = inp.ready ? 'OK' : 'Offline';

                html += '<div class="channel-strip" data-lane="' + inp.lane + '">';
                html += '  <div class="channel-strip-header">';
                html += '    <span class="channel-device-name">' + escapeHtml(inp.deviceName) + '</span>';
                html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
                html += '  </div>';

                // Stereo VU meters
                html += '  <div class="channel-vu-pair">';
                html += '    <div class="channel-vu-wrapper">';
                html += '      <canvas class="channel-vu-canvas" id="inputVu' + inp.lane + 'L" width="24" height="120"></canvas>';
                html += '      <div class="channel-vu-label">L</div>';
                html += '    </div>';
                html += '    <div class="channel-vu-wrapper">';
                html += '      <canvas class="channel-vu-canvas" id="inputVu' + inp.lane + 'R" width="24" height="120"></canvas>';
                html += '      <div class="channel-vu-label">R</div>';
                html += '    </div>';
                html += '    <div class="channel-vu-readout" id="inputVuReadout' + inp.lane + '">-- dB</div>';
                html += '  </div>';

                // Gain slider
                html += '  <div class="channel-control-row">';
                html += '    <label class="channel-control-label">Gain</label>';
                html += '    <input type="range" class="channel-gain-slider" id="inputGain' + inp.lane + '" min="-72" max="12" step="0.5" value="0"';
                html += '      oninput="onInputGainChange(' + inp.lane + ',this.value)">';
                html += '    <span class="channel-gain-value" id="inputGainVal' + inp.lane + '">0.0 dB</span>';
                html += '  </div>';

                // Mute / Phase / Solo buttons
                html += '  <div class="channel-button-row">';
                html += '    <button class="channel-btn" id="inputMute' + inp.lane + '" onclick="toggleInputMute(' + inp.lane + ')">Mute</button>';
                html += '    <button class="channel-btn" id="inputPhase' + inp.lane + '" onclick="toggleInputPhase(' + inp.lane + ')">Phase</button>';
                html += '  </div>';

                // PEQ button
                html += '  <div class="channel-dsp-row">';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openInputPeq(' + inp.lane + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
                html += '      PEQ';
                html += '    </button>';
                html += '  </div>';

                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = String(inputs.length);
        }

        // ===== Output Channel Strips =====
        function renderOutputStrips() {
            var container = document.getElementById('audio-outputs-container');
            if (!container || !audioChannelMap) return;

            var outputs = audioChannelMap.outputs || [];
            if (container.dataset.rendered === String(outputs.length)) return;

            var html = '';
            for (var i = 0; i < outputs.length; i++) {
                var out = outputs[i];
                var statusClass = out.ready ? 'status-ok' : 'status-off';
                var statusText = out.ready ? 'OK' : 'Offline';
                var hasHwVol = (out.capabilities & 1);  // HAL_CAP_HW_VOLUME
                var hasHwMute = (out.capabilities & 4);  // HAL_CAP_MUTE

                html += '<div class="channel-strip channel-strip-output" data-sink="' + out.index + '">';
                html += '  <div class="channel-strip-header">';
                html += '    <span class="channel-device-name">' + escapeHtml(out.name) + '</span>';
                html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
                html += '  </div>';
                html += '  <div class="channel-strip-sub">Ch ' + out.firstChannel + '-' + (out.firstChannel + out.channels - 1) + '</div>';

                // VU meters
                html += '  <div class="channel-vu-pair">';
                for (var ch = 0; ch < out.channels && ch < 2; ch++) {
                    var label = out.channels > 1 ? (ch === 0 ? 'L' : 'R') : '';
                    html += '    <div class="channel-vu-wrapper">';
                    html += '      <canvas class="channel-vu-canvas" id="outputVu' + out.index + 'c' + ch + '" width="24" height="120"></canvas>';
                    if (label) html += '      <div class="channel-vu-label">' + label + '</div>';
                    html += '    </div>';
                }
                html += '    <div class="channel-vu-readout" id="outputVuReadout' + out.index + '">-- dB</div>';
                html += '  </div>';

                // Gain / HW Volume
                if (hasHwVol) {
                    html += '  <div class="channel-control-row">';
                    html += '    <label class="channel-control-label">HW Vol</label>';
                    html += '    <input type="range" class="channel-gain-slider" id="outputHwVol' + out.index + '" min="0" max="100" step="1" value="80"';
                    html += '      oninput="onOutputHwVolChange(' + out.index + ',this.value)">';
                    html += '    <span class="channel-gain-value" id="outputHwVolVal' + out.index + '">80%</span>';
                    html += '  </div>';
                }

                html += '  <div class="channel-control-row">';
                html += '    <label class="channel-control-label">Gain</label>';
                html += '    <input type="range" class="channel-gain-slider" id="outputGain' + out.index + '" min="-72" max="12" step="0.5" value="0"';
                html += '      oninput="onOutputGainChange(' + out.index + ',this.value)">';
                html += '    <span class="channel-gain-value" id="outputGainVal' + out.index + '">0.0 dB</span>';
                html += '  </div>';

                // Mute / Phase / Solo
                html += '  <div class="channel-button-row">';
                html += '    <button class="channel-btn" id="outputMute' + out.index + '" onclick="toggleOutputMute(' + out.index + ')">' + (hasHwMute ? 'HW Mute' : 'Mute') + '</button>';
                html += '    <button class="channel-btn" id="outputPhase' + out.index + '" onclick="toggleOutputPhase(' + out.index + ')">Phase</button>';
                html += '  </div>';

                // DSP controls
                html += '  <div class="channel-dsp-section">';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputPeq(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
                html += '      PEQ 10-band</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputCrossover(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M16,11.78L20.24,4.45L21.97,5.45L16.74,14.5L10.23,10.75L5.46,19H22V21H2V3H4V17.54L9.5,8L16,11.78Z"/></svg>';
                html += '      Crossover</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputCompressor(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M5,4V7H10.5V19H13.5V7H19V4H5Z"/></svg>';
                html += '      Compressor</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputLimiter(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M2,12A10,10 0 0,1 12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22A10,10 0 0,1 2,12M4,12A8,8 0 0,0 12,20A8,8 0 0,0 20,12A8,8 0 0,0 12,4A8,8 0 0,0 4,12M7,12L12,7V17L7,12Z"/></svg>';
                html += '      Limiter</button>';

                // Delay
                html += '    <div class="channel-control-row" style="margin-top:6px;">';
                html += '      <label class="channel-control-label">Delay</label>';
                html += '      <input type="number" class="channel-delay-input" id="outputDelay' + out.firstChannel + '" min="0" max="10" step="0.01" value="0.00"';
                html += '        onchange="onOutputDelayChange(' + out.firstChannel + ',this.value)">';
                html += '      <span class="channel-gain-value">ms</span>';
                html += '    </div>';
                html += '  </div>';

                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = String(outputs.length);
        }

        // ===== Matrix Grid =====
        function renderMatrixGrid() {
            var container = document.getElementById('audio-matrix-container');
            if (!container || !audioChannelMap) return;

            var matSize = audioChannelMap.matrixInputs || 8;
            var inputs = audioChannelMap.inputs || [];
            var outputs = audioChannelMap.outputs || [];
            var matrixGains = audioChannelMap.matrix || [];

            // Build column headers from output sinks
            var colLabels = [];
            for (var o = 0; o < matSize; o++) {
                var label = 'OUT ' + (o + 1);
                for (var si = 0; si < outputs.length; si++) {
                    var sk = outputs[si];
                    if (o >= sk.firstChannel && o < sk.firstChannel + sk.channels) {
                        var chOff = o - sk.firstChannel;
                        label = sk.name + (sk.channels > 1 ? (chOff === 0 ? ' L' : ' R') : '');
                        break;
                    }
                }
                colLabels.push(label);
            }

            // Build row labels from input lanes
            var rowLabels = [];
            for (var r = 0; r < matSize; r++) {
                var laneIdx = Math.floor(r / 2);
                var ch = r % 2;
                if (laneIdx < inputs.length) {
                    rowLabels.push(inputs[laneIdx].deviceName + (ch === 0 ? ' L' : ' R'));
                } else {
                    rowLabels.push('IN ' + (r + 1));
                }
            }

            var html = '<table class="matrix-table"><thead><tr><th></th>';
            for (var c = 0; c < matSize; c++) {
                html += '<th class="matrix-col-hdr">' + escapeHtml(colLabels[c]) + '</th>';
            }
            html += '</tr></thead><tbody>';

            for (var row = 0; row < matSize; row++) {
                html += '<tr><td class="matrix-row-hdr">' + escapeHtml(rowLabels[row]) + '</td>';
                for (var col = 0; col < matSize; col++) {
                    var gain = (matrixGains[col] && matrixGains[col][row] !== undefined) ? parseFloat(matrixGains[col][row]) : 0;
                    var active = gain > 0.0001 || gain < -0.0001;
                    var displayVal = active ? (gain >= 1.0 ? '+' + (20 * Math.log10(gain)).toFixed(1) : (20 * Math.log10(Math.max(gain, 0.0001))).toFixed(1)) : '--';
                    var cellClass = 'matrix-cell' + (active ? ' matrix-active' : '');
                    html += '<td class="' + cellClass + '" data-out="' + col + '" data-in="' + row + '" onclick="onMatrixCellClick(' + col + ',' + row + ')">';
                    html += displayVal;
                    html += '</td>';
                }
                html += '</tr>';
            }
            html += '</tbody></table>';

            // Quick presets
            html += '<div class="matrix-presets">';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixPreset1to1()">1:1 Pass</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixPresetClear()">Clear All</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixSave()">Save</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixLoad()">Load</button>';
            html += '</div>';

            container.innerHTML = html;
        }

        // ===== Matrix Cell Click — Popup Gain Slider =====
        function onMatrixCellClick(outCh, inCh) {
            var currentGain = 0;
            if (audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                currentGain = parseFloat(audioChannelMap.matrix[outCh][inCh]) || 0;
            }
            var currentDb = currentGain > 0.0001 ? (20 * Math.log10(currentGain)).toFixed(1) : '-72.0';

            var popup = document.getElementById('matrixGainPopup');
            if (!popup) {
                popup = document.createElement('div');
                popup.id = 'matrixGainPopup';
                popup.className = 'matrix-gain-popup';
                document.body.appendChild(popup);
            }

            popup.innerHTML = '<div class="matrix-gain-popup-inner">' +
                '<label>OUT ' + (outCh + 1) + ' \u2190 IN ' + (inCh + 1) + '</label>' +
                '<input type="range" id="matrixGainSlider" min="-72" max="12" step="0.5" value="' + currentDb + '" oninput="onMatrixGainSlide(' + outCh + ',' + inCh + ',this.value)">' +
                '<span id="matrixGainDbVal">' + currentDb + ' dB</span>' +
                '<div style="display:flex;gap:4px;margin-top:6px;">' +
                '<button class="btn btn-sm btn-primary" onclick="setMatrixGainDb(' + outCh + ',' + inCh + ',0);closeMatrixPopup()">0 dB</button>' +
                '<button class="btn btn-sm btn-secondary" onclick="setMatrixGainDb(' + outCh + ',' + inCh + ',-72);closeMatrixPopup()">Off</button>' +
                '<button class="btn btn-sm btn-secondary" onclick="closeMatrixPopup()">Close</button>' +
                '</div></div>';
            popup.style.display = 'block';
        }

        function onMatrixGainSlide(outCh, inCh, dbVal) {
            var label = document.getElementById('matrixGainDbVal');
            if (label) label.textContent = parseFloat(dbVal).toFixed(1) + ' dB';
            setMatrixGainDb(outCh, inCh, parseFloat(dbVal));
        }

        function closeMatrixPopup() {
            var popup = document.getElementById('matrixGainPopup');
            if (popup) popup.style.display = 'none';
        }

        function setMatrixGainDb(outCh, inCh, db) {
            fetch('/api/pipeline/matrix/cell', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ out: outCh, in: inCh, gainDb: db })
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                if (d.status === 'ok' && audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                    audioChannelMap.matrix[outCh][inCh] = d.gainLinear;
                    renderMatrixGrid();
                }
            })
            .catch(function() {});
            // Optimistic local update for immediate UI feedback
            if (audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                audioChannelMap.matrix[outCh][inCh] = Math.pow(10, db / 20);
            }
            renderMatrixGrid();
        }

        // Matrix presets — use existing REST endpoint
        function matrixPreset1to1() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var i = 0; i < size; i++) setMatrixGainDb(i, i, 0);
        }
        function matrixPresetClear() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var o = 0; o < size; o++)
                for (var i = 0; i < size; i++)
                    setMatrixGainDb(o, i, -96);
        }
        function matrixSave() {
            fetch('/api/pipeline/matrix/save', { method: 'POST' })
                .then(function() { showToast('Matrix saved', 'success'); })
                .catch(function() { showToast('Save failed', 'error'); });
        }
        function matrixLoad() {
            fetch('/api/pipeline/matrix/load', { method: 'POST' })
                .then(function() {
                    showToast('Matrix loaded', 'success');
                    // Refresh channel map to get updated matrix
                    fetch('/api/pipeline/matrix').then(function(r) { return r.json(); }).then(function(d) {
                        if (audioChannelMap) audioChannelMap.matrix = d.matrix;
                        renderMatrixGrid();
                    });
                })
                .catch(function() { showToast('Load failed', 'error'); });
        }

        // ===== Signal Generator Sub-View =====
        function renderSigGenView() {
            // Signal gen sub-view delegates to existing siggen controls (13-signal-gen.js)
            // The HTML is statically in the siggen sub-view panel
        }

        // ===== Input Controls =====
        function onInputGainChange(lane, val) {
            var label = document.getElementById('inputGainVal' + lane);
            if (label) label.textContent = parseFloat(val).toFixed(1) + ' dB';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputGain', lane: lane, db: parseFloat(val) }));
            }
        }

        function toggleInputMute(lane) {
            var btn = document.getElementById('inputMute' + lane);
            if (!btn) return;
            var muted = !btn.classList.contains('active');
            btn.classList.toggle('active', muted);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputMute', lane: lane, muted: muted }));
            }
        }

        function toggleInputPhase(lane) {
            var btn = document.getElementById('inputPhase' + lane);
            if (!btn) return;
            var inverted = !btn.classList.contains('active');
            btn.classList.toggle('active', inverted);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputPhase', lane: lane, inverted: inverted }));
            }
        }

        // ===== Output Controls =====
        function onOutputGainChange(sinkIdx, val) {
            var label = document.getElementById('outputGainVal' + sinkIdx);
            if (label) label.textContent = parseFloat(val).toFixed(1) + ' dB';
            // Output gain maps to per-output DSP gain stage
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputGain', channel: out.firstChannel, db: parseFloat(val) }));
            }
        }

        function onOutputHwVolChange(sinkIdx, val) {
            var label = document.getElementById('outputHwVolVal' + sinkIdx);
            if (label) label.textContent = val + '%';
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputHwVolume', channel: out.firstChannel, volume: parseInt(val) }));
            }
        }

        function toggleOutputMute(sinkIdx) {
            var btn = document.getElementById('outputMute' + sinkIdx);
            if (!btn) return;
            var muted = !btn.classList.contains('active');
            btn.classList.toggle('active', muted);
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputMute', channel: out.firstChannel, muted: muted }));
            }
        }

        function toggleOutputPhase(sinkIdx) {
            var btn = document.getElementById('outputPhase' + sinkIdx);
            if (!btn) return;
            var inverted = !btn.classList.contains('active');
            btn.classList.toggle('active', inverted);
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputPhase', channel: out.firstChannel, inverted: inverted }));
            }
        }

        function onOutputDelayChange(channel, val) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputDelay', channel: channel, ms: parseFloat(val) }));
            }
        }

        // PEQ / DSP overlay openers defined in 06-peq-overlay.js:
        // openInputPeq, openOutputPeq, openOutputCrossover, openOutputCompressor, openOutputLimiter

        // ===== VU Meter Drawing for Channel Strips =====
        function drawChannelVu(canvasId, value) {
            var canvas = document.getElementById(canvasId);
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var w = canvas.width, h = canvas.height;
            ctx.clearRect(0, 0, w, h);

            // Background
            ctx.fillStyle = 'var(--bg-card)';
            ctx.fillRect(0, 0, w, h);

            // VU bar (bottom-up)
            var pct = Math.max(0, Math.min(1, (value + 60) / 60));  // -60dB to 0dB range
            var barH = Math.round(pct * h);
            if (barH > 0) {
                var grad = ctx.createLinearGradient(0, h, 0, 0);
                grad.addColorStop(0, '#4CAF50');
                grad.addColorStop(0.7, '#FFC107');
                grad.addColorStop(1.0, '#F44336');
                ctx.fillStyle = grad;
                ctx.fillRect(2, h - barH, w - 4, barH);
            }
        }

        // ===== Audio Levels Handler (extends existing audioLevels route) =====
        function audioTabUpdateLevels(data) {
            if (!audioChannelMap || currentActiveTab !== 'audio') return;

            if (audioSubView === 'inputs' && data.adc) {
                for (var a = 0; a < data.adc.length; a++) {
                    var ad = data.adc[a];
                    drawChannelVu('inputVu' + a + 'L', ad.vu1 || -90);
                    drawChannelVu('inputVu' + a + 'R', ad.vu2 || -90);
                    var readout = document.getElementById('inputVuReadout' + a);
                    if (readout) {
                        var dbText = (ad.dBFS || -90).toFixed(1) + ' dB';
                        if (ad.vrms1 !== undefined) {
                            var avgVrms = ((ad.vrms1 || 0) + (ad.vrms2 || 0)) / 2;
                            dbText += ' | ' + (avgVrms < 0.001 ? '0.000' : avgVrms.toFixed(3)) + ' Vrms';
                        }
                        readout.textContent = dbText;
                    }
                }
            }
            // Output sink VU meters
            if (data.sinks && audioSubView === 'outputs') {
                for (var s = 0; s < data.sinks.length; s++) {
                    var sk = data.sinks[s];
                    drawChannelVu('outputVu' + s + 'c0', sk.vuL || -90);
                    drawChannelVu('outputVu' + s + 'c1', sk.vuR || -90);
                    readout = document.getElementById('outputVuReadout' + s);
                    if (readout) {
                        var avg = ((sk.vuL || -90) + (sk.vuR || -90)) / 2;
                        readout.textContent = avg.toFixed(1) + ' dB';
                    }
                }
            }
        }

        function escapeHtml(str) {
            if (!str) return '';
            return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }

//# sourceURL=05-audio-tab.js

        // ===== Canvas Helpers =====

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

        // Offscreen canvas background cache — static grids/labels drawn once
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
            for (let a = 0; a < numInputLanes; a++) {
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

//# sourceURL=06-canvas-helpers.js

        // ===== PEQ / DSP Overlay Editor =====
        // Full-screen overlay for editing per-channel PEQ, crossover, compressor, limiter.

        // Biquad magnitude response: returns dB at frequency f (Hz) given coefficients [b0,b1,b2,a1,a2]
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
        function dspComputeCoeffs(type, freq, gain, Q, fs) {
            var fn = freq / fs;
            if (fn < 0.0001) fn = 0.0001;
            if (fn > 0.4999) fn = 0.4999;
            var w0 = 2 * Math.PI * fn;
            var cosW = Math.cos(w0), sinW = Math.sin(w0);
            if (Q <= 0) Q = 0.707;
            var alpha = sinW / (2 * Q);
            var A, b0, b1, b2, a0, a1, a2, sq, wt, n;
            switch (type) {
                case 0: b1 = 1 - cosW; b0 = b1 / 2; b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 1: b1 = -(1 + cosW); b0 = -b1 / 2; b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 2: b0 = sinW / 2; b1 = 0; b2 = -b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 3: b0 = 1; b1 = -2 * cosW; b2 = 1; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 4: A = Math.pow(10, gain / 40); b0 = 1 + alpha * A; b1 = -2 * cosW; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cosW; a2 = 1 - alpha / A; break;
                case 5: A = Math.pow(10, gain / 40); sq = 2 * Math.sqrt(A) * alpha; b0 = A * ((A+1)-(A-1)*cosW+sq); b1 = 2*A*((A-1)-(A+1)*cosW); b2 = A*((A+1)-(A-1)*cosW-sq); a0 = (A+1)+(A-1)*cosW+sq; a1 = -2*((A-1)+(A+1)*cosW); a2 = (A+1)+(A-1)*cosW-sq; break;
                case 6: A = Math.pow(10, gain / 40); sq = 2 * Math.sqrt(A) * alpha; b0 = A*((A+1)+(A-1)*cosW+sq); b1 = -2*A*((A-1)+(A+1)*cosW); b2 = A*((A+1)+(A-1)*cosW-sq); a0 = (A+1)-(A-1)*cosW+sq; a1 = 2*((A-1)-(A+1)*cosW); a2 = (A+1)-(A-1)*cosW-sq; break;
                case 7: case 8: b0 = 1 - alpha; b1 = -2 * cosW; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 9: b0 = -(1 - alpha); b1 = 2 * cosW; b2 = -(1 + alpha); a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 10: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 19: wt = Math.tan(Math.PI * fn); n = 1 / (1 + wt); return [wt * n, wt * n, 0, (wt - 1) * n, 0];
                case 20: wt = Math.tan(Math.PI * fn); n = 1 / (1 + wt); return [n, -n, 0, (wt - 1) * n, 0];
                default: return [1, 0, 0, 0, 0];
            }
            var inv = 1 / a0;
            return [b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv];
        }

        let peqOverlayActive = false;
        let peqOverlayTarget = null;  // {type:'input'|'output', channel:int}
        let peqOverlayBands = [];     // [{type, freq, gain, Q, enabled}]
        let peqOverlayFs = 48000;

        // Filter type names matching firmware DspStageType enum
        var PEQ_TYPES = [
            {id: 0,  name: 'LPF'},
            {id: 1,  name: 'HPF'},
            {id: 2,  name: 'BPF'},
            {id: 3,  name: 'Notch'},
            {id: 4,  name: 'Peak'},
            {id: 5,  name: 'Lo Shelf'},
            {id: 6,  name: 'Hi Shelf'},
            {id: 7,  name: 'AP 360'},
            {id: 8,  name: 'AP 360'},
            {id: 9,  name: 'AP 180'},
            {id: 10, name: 'BPF0'},
            {id: 19, name: 'LPF1'},
            {id: 20, name: 'HPF1'}
        ];

        function peqTypeName(typeId) {
            var t = PEQ_TYPES.find(function(x) { return x.id === typeId; });
            return t ? t.name : 'PEQ';
        }

        // ===== Open PEQ Overlay =====
        function openPeqOverlay(target, bands, fs) {
            peqOverlayTarget = target;
            peqOverlayBands = bands || [];
            peqOverlayFs = fs || 48000;
            peqOverlayActive = true;

            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
            }

            var title = target.type === 'input' ? 'Input PEQ — Lane ' + target.channel : 'Output PEQ — Ch ' + target.channel;
            var maxBands = target.type === 'input' ? 6 : 10;

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">' + title + '</span>';
            html += '  <button class="peq-overlay-close" onclick="closePeqOverlay()">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';

            // Frequency response graph
            html += '<div class="peq-graph-wrap">';
            html += '  <canvas id="peqOverlayCanvas" class="peq-overlay-canvas"></canvas>';
            html += '</div>';

            // Band table
            html += '<div class="peq-band-table-wrap">';
            html += '  <table class="peq-band-table">';
            html += '    <thead><tr><th>#</th><th>Type</th><th>Freq</th><th>Gain</th><th>Q</th><th>On</th><th></th></tr></thead>';
            html += '    <tbody id="peqBandRows">';
            for (var i = 0; i < peqOverlayBands.length; i++) {
                html += peqBandRowHtml(i);
            }
            html += '    </tbody>';
            html += '  </table>';
            html += '</div>';

            // Actions
            html += '<div class="peq-overlay-actions">';
            if (peqOverlayBands.length < maxBands) {
                html += '  <button class="btn btn-sm btn-primary" onclick="peqAddBand()">Add Band</button>';
            }
            html += '  <button class="btn btn-sm btn-secondary" onclick="peqResetAll()">Reset All</button>';
            html += '  <button class="btn btn-sm btn-primary" onclick="peqApply()">Apply</button>';
            html += '  <button class="btn btn-sm btn-secondary" onclick="closePeqOverlay()">Cancel</button>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';

            // Draw initial graph
            setTimeout(peqDrawGraph, 50);
        }

        function peqBandRowHtml(idx) {
            var b = peqOverlayBands[idx];
            var typeOptions = '';
            for (var t = 0; t < PEQ_TYPES.length; t++) {
                var pt = PEQ_TYPES[t];
                typeOptions += '<option value="' + pt.id + '"' + (pt.id === b.type ? ' selected' : '') + '>' + pt.name + '</option>';
            }
            var html = '<tr data-band="' + idx + '">';
            html += '<td>' + (idx + 1) + '</td>';
            html += '<td><select class="peq-input peq-type-sel" onchange="peqUpdateBand(' + idx + ',\'type\',parseInt(this.value))">' + typeOptions + '</select></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.freq || 1000) + '" min="20" max="20000" step="1" onchange="peqUpdateBand(' + idx + ',\'freq\',parseFloat(this.value))"></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.gain || 0).toFixed(1) + '" min="-24" max="24" step="0.5" onchange="peqUpdateBand(' + idx + ',\'gain\',parseFloat(this.value))"></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.Q || 0.707).toFixed(3) + '" min="0.1" max="30" step="0.01" onchange="peqUpdateBand(' + idx + ',\'Q\',parseFloat(this.value))"></td>';
            html += '<td><input type="checkbox"' + (b.enabled !== false ? ' checked' : '') + ' onchange="peqUpdateBand(' + idx + ',\'enabled\',this.checked)"></td>';
            html += '<td><button class="channel-btn" onclick="peqRemoveBand(' + idx + ')" style="padding:2px 6px;min-width:0">';
            html += '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg>';
            html += '</button></td>';
            html += '</tr>';
            return html;
        }

        function peqUpdateBand(idx, field, value) {
            if (idx >= 0 && idx < peqOverlayBands.length) {
                peqOverlayBands[idx][field] = value;
                peqDrawGraph();
            }
        }

        function peqAddBand() {
            var maxBands = peqOverlayTarget && peqOverlayTarget.type === 'input' ? 6 : 10;
            if (peqOverlayBands.length >= maxBands) return;
            peqOverlayBands.push({ type: 4, freq: 1000, gain: 0, Q: 1.41, enabled: true });
            // Re-render table
            var tbody = document.getElementById('peqBandRows');
            if (tbody) tbody.innerHTML += peqBandRowHtml(peqOverlayBands.length - 1);
            peqDrawGraph();
        }

        function peqRemoveBand(idx) {
            peqOverlayBands.splice(idx, 1);
            // Re-render entire table (index shift)
            var tbody = document.getElementById('peqBandRows');
            if (tbody) {
                var html = '';
                for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                tbody.innerHTML = html;
            }
            peqDrawGraph();
        }

        function peqResetAll() {
            peqOverlayBands = [];
            var tbody = document.getElementById('peqBandRows');
            if (tbody) tbody.innerHTML = '';
            peqDrawGraph();
        }

        function closePeqOverlay() {
            peqOverlayActive = false;
            var overlay = document.getElementById('peqOverlay');
            if (overlay) overlay.style.display = 'none';
        }

        // ===== Apply PEQ changes to firmware =====
        function peqApply() {
            if (!peqOverlayTarget) return;
            var target = peqOverlayTarget;
            var bands = peqOverlayBands;

            if (target.type === 'output') {
                // Apply via output DSP REST API
                // First get current config, then update biquad stages
                fetch('/api/output/dsp?ch=' + target.channel)
                    .then(function(r) { return r.json(); })
                    .then(function(cfg) {
                        // Build updated stages: keep non-biquad stages, replace biquads with new bands
                        var stages = [];
                        // Keep existing non-biquad stages
                        if (cfg.stages) {
                            for (var s = 0; s < cfg.stages.length; s++) {
                                var st = cfg.stages[s];
                                // Biquad types are 0-10, 19-20
                                var isBiquad = st.type <= 10 || st.type === 19 || st.type === 20;
                                if (!isBiquad) stages.push(st);
                            }
                        }
                        // Add PEQ bands as biquad stages
                        for (var i = 0; i < bands.length; i++) {
                            stages.push({
                                enabled: bands[i].enabled !== false,
                                type: bands[i].type,
                                frequency: bands[i].freq,
                                gain: bands[i].gain,
                                Q: bands[i].Q
                            });
                        }

                        return fetch('/api/output/dsp', {
                            method: 'PUT',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({
                                ch: target.channel,
                                bypass: cfg.bypass || false,
                                stages: stages
                            })
                        });
                    })
                    .then(function() {
                        showToast('PEQ applied to Output Ch ' + target.channel, 'success');
                        closePeqOverlay();
                    })
                    .catch(function(err) { showToast('PEQ apply failed: ' + err, 'error'); });
            } else if (target.type === 'input') {
                // Apply via DSP pipeline WS commands
                // Input PEQ uses the per-input DSP (dsp_pipeline.h), channels 0-3
                var ch = target.channel;
                // Remove all existing biquad stages, then add new ones
                // This is simplified — a full implementation would diff and patch
                for (var i = 0; i < bands.length; i++) {
                    var b = bands[i];
                    if (ws && ws.readyState === WebSocket.OPEN) {
                        ws.send(JSON.stringify({
                            type: 'addDspStage', ch: ch, stageType: b.type,
                            frequency: b.freq, gain: b.gain, Q: b.Q
                        }));
                    }
                }
                showToast('PEQ applied to Input Lane ' + ch, 'success');
                closePeqOverlay();
            }
        }

        // ===== Draw Frequency Response Graph =====
        function peqDrawGraph() {
            var canvas = document.getElementById('peqOverlayCanvas');
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var rect = canvas.parentElement.getBoundingClientRect();
            canvas.width = Math.max(rect.width, 300);
            canvas.height = Math.max(Math.min(rect.height, 300), 180);
            var w = canvas.width, h = canvas.height;
            var fs = peqOverlayFs;

            // Styling
            var isDark = document.body.classList.contains('night-mode');
            var bgColor = isDark ? '#1E1E1E' : '#FFFFFF';
            var gridColor = isDark ? '#333' : '#E0E0E0';
            var textColor = isDark ? '#888' : '#999';
            var combinedColor = '#FF9800';

            // Clear
            ctx.fillStyle = bgColor;
            ctx.fillRect(0, 0, w, h);

            // Axes
            var padL = 40, padR = 10, padT = 15, padB = 25;
            var gW = w - padL - padR, gH = h - padT - padB;
            var dbMin = -18, dbMax = 18;

            // Frequency range: 20Hz to 20kHz (log scale)
            var fMin = 20, fMax = 20000;
            function fToX(f) { return padL + gW * (Math.log10(f / fMin) / Math.log10(fMax / fMin)); }
            function dbToY(db) { return padT + gH * (1 - (db - dbMin) / (dbMax - dbMin)); }

            // Grid lines
            ctx.strokeStyle = gridColor;
            ctx.lineWidth = 0.5;
            ctx.font = '10px monospace';
            ctx.fillStyle = textColor;

            // dB grid
            for (var db = dbMin; db <= dbMax; db += 6) {
                var y = dbToY(db);
                ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(w - padR, y); ctx.stroke();
                ctx.fillText(db + '', 2, y + 3);
            }

            // Frequency grid
            var freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
            for (var fi = 0; fi < freqs.length; fi++) {
                var x = fToX(freqs[fi]);
                ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, h - padB); ctx.stroke();
                var label = freqs[fi] >= 1000 ? (freqs[fi] / 1000) + 'k' : freqs[fi] + '';
                ctx.fillText(label, x - 6, h - 5);
            }

            // 0dB reference line
            ctx.strokeStyle = isDark ? '#555' : '#BBB';
            ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(padL, dbToY(0)); ctx.lineTo(w - padR, dbToY(0)); ctx.stroke();

            // Per-band curves
            var bandColors = ['#F44336', '#2196F3', '#4CAF50', '#FFC107', '#9C27B0', '#00BCD4', '#FF5722', '#607D8B', '#E91E63', '#3F51B5'];
            var numPoints = Math.max(gW, 200);

            var bi, p, f, coeffs;
            for (bi = 0; bi < peqOverlayBands.length; bi++) {
                var band = peqOverlayBands[bi];
                if (band.enabled === false) continue;
                coeffs = dspComputeCoeffs(band.type, band.freq || 1000, band.gain || 0, band.Q || 0.707, fs);

                ctx.strokeStyle = bandColors[bi % bandColors.length] + '55';
                ctx.lineWidth = 1;
                ctx.beginPath();
                for (p = 0; p <= numPoints; p++) {
                    f = fMin * Math.pow(fMax / fMin, p / numPoints);
                    var mag = dspBiquadMagDb(coeffs, f, fs);
                    x = fToX(f); y = dbToY(Math.max(dbMin, Math.min(dbMax, mag)));
                    if (p === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }

            // Combined response
            ctx.strokeStyle = combinedColor;
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (p = 0; p <= numPoints; p++) {
                f = fMin * Math.pow(fMax / fMin, p / numPoints);
                var totalDb = 0;
                for (bi = 0; bi < peqOverlayBands.length; bi++) {
                    if (peqOverlayBands[bi].enabled === false) continue;
                    coeffs = dspComputeCoeffs(peqOverlayBands[bi].type, peqOverlayBands[bi].freq || 1000,
                        peqOverlayBands[bi].gain || 0, peqOverlayBands[bi].Q || 0.707, fs);
                    totalDb += dspBiquadMagDb(coeffs, f, fs);
                }
                x = fToX(f); y = dbToY(Math.max(dbMin, Math.min(dbMax, totalDb)));
                if (p === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        // ===== Wire up overlay openers from 05-audio-tab.js =====
        // Override the stubs defined in 05-audio-tab.js
        function openInputPeq(lane) {
            // Fetch current DSP state for this input channel
            // Input DSP uses dsp_pipeline channels (lane 0=ch0, lane 1=ch1, etc.)
            // For now, open with empty bands — user adds bands
            openPeqOverlay({ type: 'input', channel: lane }, [], peqOverlayFs);
        }

        function openOutputPeq(channel) {
            // Fetch current output DSP config and extract biquad stages
            fetch('/api/output/dsp?ch=' + channel)
                .then(function(r) { return r.json(); })
                .then(function(cfg) {
                    var bands = [];
                    if (cfg.stages) {
                        for (var s = 0; s < cfg.stages.length; s++) {
                            var st = cfg.stages[s];
                            var isBiquad = st.type <= 10 || st.type === 19 || st.type === 20;
                            if (isBiquad) {
                                bands.push({
                                    type: st.type,
                                    freq: st.frequency || 1000,
                                    gain: st.gain || 0,
                                    Q: st.Q || 0.707,
                                    enabled: st.enabled !== false
                                });
                            }
                        }
                    }
                    peqOverlayFs = cfg.sampleRate || 48000;
                    openPeqOverlay({ type: 'output', channel: channel }, bands, peqOverlayFs);
                })
                .catch(function() {
                    openPeqOverlay({ type: 'output', channel: channel }, [], 48000);
                });
        }

        // ===== Crossover Overlay =====
        function openOutputCrossover(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Crossover — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" onclick="closePeqOverlay()">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div class="peq-graph-wrap"><canvas id="peqOverlayCanvas" class="peq-overlay-canvas"></canvas></div>';
            html += '<div style="padding:16px;">';
            html += '  <div class="channel-control-row" style="margin-bottom:8px;">';
            html += '    <label class="channel-control-label" style="min-width:80px">Type</label>';
            html += '    <select class="form-input" id="xoverType" style="max-width:200px">';
            html += '      <option value="lr2">Linkwitz-Riley 2nd (12dB/oct)</option>';
            html += '      <option value="lr4" selected>Linkwitz-Riley 4th (24dB/oct)</option>';
            html += '      <option value="lr8">Linkwitz-Riley 8th (48dB/oct)</option>';
            html += '      <option value="bw6">Butterworth 1st (6dB/oct)</option>';
            html += '      <option value="bw12">Butterworth 2nd (12dB/oct)</option>';
            html += '      <option value="bw18">Butterworth 3rd (18dB/oct)</option>';
            html += '      <option value="bw24">Butterworth 4th (24dB/oct)</option>';
            html += '    </select>';
            html += '  </div>';
            html += '  <div class="channel-control-row" style="margin-bottom:8px;">';
            html += '    <label class="channel-control-label" style="min-width:80px">Frequency</label>';
            html += '    <input type="number" class="form-input" id="xoverFreq" value="80" min="20" max="20000" step="1" style="max-width:120px">';
            html += '    <span class="channel-gain-value">Hz</span>';
            html += '  </div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" onclick="applyXover(' + channel + ')">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" onclick="closePeqOverlay()">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyXover(channel) {
            var type = document.getElementById('xoverType').value;
            var freq = parseFloat(document.getElementById('xoverFreq').value) || 80;
            var orderMap = { lr2: 2, lr4: 4, lr8: 8, bw6: 1, bw12: 2, bw18: 3, bw24: 4 };
            var order = orderMap[type] || 4;

            // Use the output DSP crossover REST endpoint
            fetch('/api/output/dsp/crossover', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ subCh: channel, mainCh: channel + 1, freqHz: freq, order: order })
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                showToast('Crossover applied (' + type + ' @ ' + freq + ' Hz)', 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Crossover failed', 'error'); });
        }

        // ===== Compressor Overlay =====
        function openOutputCompressor(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Compressor — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" onclick="closePeqOverlay()">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'compThreshold', -40, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'compRatio', 1, 20, 4, 0.5, ':1');
            html += peqControlRow('Attack', 'compAttack', 0.1, 200, 10, 0.1, 'ms');
            html += peqControlRow('Release', 'compRelease', 10, 2000, 100, 1, 'ms');
            html += peqControlRow('Knee', 'compKnee', 0, 20, 6, 0.5, 'dB');
            html += peqControlRow('Makeup', 'compMakeup', 0, 24, 0, 0.5, 'dB');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" onclick="applyCompressor(' + channel + ')">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" onclick="closePeqOverlay()">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyCompressor(channel) {
            var params = {
                thresholdDb: parseFloat(document.getElementById('compThreshold').value),
                ratio: parseFloat(document.getElementById('compRatio').value),
                attackMs: parseFloat(document.getElementById('compAttack').value),
                releaseMs: parseFloat(document.getElementById('compRelease').value),
                kneeDb: parseFloat(document.getElementById('compKnee').value),
                makeupGainDb: parseFloat(document.getElementById('compMakeup').value)
            };

            // Add compressor stage via REST API
            fetch('/api/output/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ch: channel, type: 14 })  // DSP_COMPRESSOR = 14
            })
            .then(function() {
                showToast('Compressor applied to Ch ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Compressor failed', 'error'); });
        }

        // ===== Limiter Overlay =====
        function openOutputLimiter(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Limiter — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" onclick="closePeqOverlay()">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'limThreshold', -40, 0, -3, 0.5, 'dBFS');
            html += peqControlRow('Attack', 'limAttack', 0.01, 50, 0.1, 0.01, 'ms');
            html += peqControlRow('Release', 'limRelease', 1, 1000, 50, 1, 'ms');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" onclick="applyLimiter(' + channel + ')">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" onclick="closePeqOverlay()">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyLimiter(channel) {
            fetch('/api/output/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ch: channel, type: 11 })  // DSP_LIMITER = 11
            })
            .then(function() {
                showToast('Limiter applied to Ch ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Limiter failed', 'error'); });
        }

        // Helper: control row for dynamics overlays
        function peqControlRow(label, id, min, max, defaultVal, step, unit) {
            return '<div class="channel-control-row" style="margin-bottom:8px;">' +
                '<label class="channel-control-label" style="min-width:80px">' + label + '</label>' +
                '<input type="range" class="channel-gain-slider" id="' + id + '" min="' + min + '" max="' + max + '" step="' + step + '" value="' + defaultVal + '" oninput="document.getElementById(\'' + id + 'Val\').textContent=this.value">' +
                '<span class="channel-gain-value" id="' + id + 'Val">' + defaultVal + ' ' + unit + '</span>' +
                '</div>';
        }

//# sourceURL=06-peq-overlay.js

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

            // Load HAL devices when switching to devices tab
            if (tabId === 'devices') {
                loadHalDeviceList();
            }

            // Load support content when switching to support tab
            if (tabId === 'support') {
                generateManualQRCode();
                loadManualContent();
            }

            // Health tab — lazy init + re-render
            if (tabId === 'health') {
                initHealthDashboard();
                renderHealthDashboard();
            }

            // Audio tab — render active sub-view on tab switch
            if (tabId === 'audio') {
                renderAudioSubView();
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
                for (let a = 0; a < numInputLanes; a++) {
                    drawAudioWaveform(null, a);
                    drawSpectrumBars(null, 0, a);
                }
            } else if (tabId !== 'audio' && audioSubscribed) {
                audioSubscribed = false;
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
                // Stop animation and reset state
                if (audioAnimFrameId) { cancelAnimationFrame(audioAnimFrameId); audioAnimFrameId = null; }
                if (vuAnimFrameId) { cancelAnimationFrame(vuAnimFrameId); vuAnimFrameId = null; }
                for (let a = 0; a < numInputLanes; a++) {
                    waveformCurrent[a] = null; waveformTarget[a] = null;
                    spectrumTarget[a].fill(0);
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

        function initSidebar() {
            const collapsed = localStorage.getItem('sidebarCollapsed') === 'true';
            if (collapsed) {
                document.getElementById('sidebar').classList.add('collapsed');
                document.body.classList.add('sidebar-collapsed');
            }
        }

//# sourceURL=07-ui-core.js

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

        function toggleLedMode() {
            ledBarMode = document.getElementById('ledModeToggle').checked;
            localStorage.setItem('ledBarMode', ledBarMode.toString());
        }

        // ===== WiFi Status =====
        function updateWiFiStatus(data) {
            const statusBox = document.getElementById('wifiStatusBox');
            const apToggle = document.getElementById('apToggle');
            const autoUpdateToggle = document.getElementById('autoUpdateToggle');

            // Store AP SSID for pre-filling the config modal
            // Firmware sends this as "appState.apSSID" (bracket notation required)
            if (data['appState.apSSID']) {
                currentAPSSID = data['appState.apSSID'];
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
            if (data.mode === 'ap' || data['appState.apEnabled']) {
                if (html !== '') html += '<div class="divider"></div>';

                html += `
                    <div class="info-row"><span class="info-label">AP Mode</span><span class="info-value text-warning">Active</span></div>
                    <div class="info-row"><span class="info-label">AP SSID</span><span class="info-value">${data['appState.apSSID'] || 'ALX-Device'}</span></div>
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

            apToggle.checked = !!(data['appState.apEnabled']) || (data.mode === 'ap');
            document.getElementById('apFields').style.display = apToggle.checked ? '' : 'none';
            statusBox.innerHTML = html;

            if (typeof data['appState.autoUpdateEnabled'] !== 'undefined') {
                autoUpdateEnabled = !!data['appState.autoUpdateEnabled'];
                autoUpdateToggle.checked = autoUpdateEnabled;
            }

            if (data.otaChannel !== undefined) {
                otaChannel = data.otaChannel;
                const sel = document.getElementById('otaChannelSelect');
                if (sel) sel.value = String(otaChannel);
            }

            if (typeof data['appState.autoAPEnabled'] !== 'undefined') {
                document.getElementById('autoAPToggle').checked = !!data['appState.autoAPEnabled'];
            }

            var tzOffset = data['appState.timezoneOffset'];
            var dstOff   = data['appState.dstOffset'];
            if (typeof tzOffset !== 'undefined') {
                currentTimezoneOffset = tzOffset;
                document.getElementById('timezoneSelect').value = tzOffset.toString();
                updateTimezoneDisplay(tzOffset, dstOff || 0);
            }

            if (typeof dstOff !== 'undefined') {
                currentDstOffset = dstOff;
                document.getElementById('dstToggle').checked = (dstOff === 3600);
            }

            if (typeof data['appState.darkMode'] !== 'undefined') {
                darkMode = !!data['appState.darkMode'];
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

            if (typeof data['appState.enableCertValidation'] !== 'undefined') {
                enableCertValidation = !!data['appState.enableCertValidation'];
                document.getElementById('certValidationToggle').checked = enableCertValidation;
            }

            if (typeof data['appState.hardwareStatsInterval'] !== 'undefined') {
                document.getElementById('statsIntervalSelect').value = data['appState.hardwareStatsInterval'].toString();
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
                if (!data['appState.updateAvailable'] && data.latestVersion !== 'Checking...' && data.latestVersion !== 'Unknown') {
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

                if (data['appState.updateAvailable']) {
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

        // ===== Timezone / Time Functions =====
        function updateTimezone() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            const dstOffset = document.getElementById('dstToggle').checked ? 3600 : 0;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ 'appState.timezoneOffset': offset, 'appState.dstOffset': dstOffset })
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
                body: JSON.stringify({ 'appState.timezoneOffset': offset, 'appState.dstOffset': dstOffset })
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
                    // Apply timezone and DST offsets (backend uses appState. prefix for these keys)
                    const offset = (data['appState.timezoneOffset'] || 0) + (data['appState.dstOffset'] || 0);
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

//# sourceURL=08-ui-status.js

        let LERP_SPEED = 0.25;
        let VU_LERP = 0.3;

        function updateLerpFactors(rateMs) {
            LERP_SPEED = Math.min(0.25 * (50 / rateMs), 0.7);
            VU_LERP = Math.min(0.3 * (50 / rateMs), 0.7);
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

            for (let a = 0; a < numInputLanes; a++) {
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
            for (let a = 0; a < numInputLanes; a++) {
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
            // Update combined dBFS readout from smoothed VU (matches per-channel source)
            var vuC = Math.sqrt((vu1 * vu1 + vu2 * vu2) * 0.5);
            var el = document.getElementById('adcReadout' + adcIdx);
            if (el) {
                var dbStr = vuC > 0 ? (20 * Math.log10(vuC)).toFixed(1) + ' dBFS' : '-inf dBFS';
                var old = el.textContent;
                var vrmsPart = old.indexOf('|') >= 0 ? old.substring(old.indexOf('|')) : '| -- Vrms';
                el.textContent = dbStr + ' ' + vrmsPart;
            }


        }

        function toggleVuMode(seg) {
            vuSegmentedMode = seg;
            localStorage.setItem('vuSegmented', seg);
            for (let a = 0; a < numInputLanes; a++) {
                var cont = document.getElementById('vuContinuous' + a);
                var segDiv = document.getElementById('vuSegmented' + a);
                if (cont) cont.style.display = seg ? 'none' : '';
                if (segDiv) segDiv.style.display = seg ? '' : 'none';
            }
        }

        function toggleGraphDisabled(id, disabled) {
            var el = document.getElementById(id);
            if (el) { if (disabled) el.classList.add('graph-disabled'); else el.classList.remove('graph-disabled'); }
        }

        function setGraphEnabled(graph, enabled) {
            var map = {vuMeter:'setVuMeterEnabled', waveform:'setWaveformEnabled', spectrum:'setSpectrumEnabled'};
            var contentMap = {vuMeter:'vuMeterContent', waveform:'waveformContent', spectrum:'spectrumContent'};
            toggleGraphDisabled(contentMap[graph], !enabled);
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:map[graph], enabled:enabled}));
        }

//# sourceURL=09-audio-viz.js

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

//# sourceURL=13-signal-gen.js

        // ===== HAL Device Management =====
        // Provides device discovery, configuration, monitoring, and CRUD UI

        // Capability flags (mirrors HAL_CAP_* in hal_types.h)
        var HAL_CAP_HW_VOLUME   = 1 << 0;
        var HAL_CAP_FILTERS     = 1 << 1;
        var HAL_CAP_MUTE        = 1 << 2;
        var HAL_CAP_ADC_PATH    = 1 << 3;
        var HAL_CAP_DAC_PATH    = 1 << 4;
        var HAL_CAP_PGA_CONTROL = 1 << 5;
        var HAL_CAP_HPF_CONTROL = 1 << 6;
        var HAL_CAP_CODEC       = 1 << 7;

        var halDevices = [];
        var halScanning = false;
        var halExpandedSlot = -1;
        var halEditingSlot = -1;
        var halDeviceCount = 0;
        var halDeviceMax = 24;
        var halDriverCount = 0;
        var halDriverMax = 24;

        function handleHalDeviceState(data) {
            halScanning = data.scanning || false;
            halDevices = data.devices || [];
            if (data.deviceCount !== undefined) halDeviceCount = data.deviceCount;
            if (data.deviceMax !== undefined) halDeviceMax = data.deviceMax;
            if (data.driverCount !== undefined) halDriverCount = data.driverCount;
            if (data.driverMax !== undefined) halDriverMax = data.driverMax;
            renderHalDevices();
        }

        function renderHalDevices() {
            var container = document.getElementById('hal-device-list');
            if (!container) return;

            var scanBtn = document.getElementById('hal-rescan-btn');
            if (scanBtn) {
                scanBtn.disabled = halScanning;
                scanBtn.textContent = halScanning ? 'Scanning...' : 'Rescan Devices';
            }

            // Update capacity indicator
            var capEl = document.getElementById('hal-capacity-indicator');
            if (capEl) {
                var devPct = halDeviceMax > 0 ? (halDeviceCount / halDeviceMax * 100) : 0;
                var drvPct = halDriverMax > 0 ? (halDriverCount / halDriverMax * 100) : 0;
                var warnClass = (devPct >= 80 || drvPct >= 80) ? ' hal-capacity-warn' : '';
                capEl.className = 'hal-capacity' + warnClass;
                capEl.textContent = 'Devices: ' + halDeviceCount + '/' + halDeviceMax +
                    '  Drivers: ' + halDriverCount + '/' + halDriverMax;
            }

            if (halDevices.length === 0) {
                container.innerHTML = '<div class="empty-state">No HAL devices registered</div>';
                return;
            }

            var html = '';
            for (var i = 0; i < halDevices.length; i++) {
                html += buildHalDeviceCard(halDevices[i]);
            }
            container.innerHTML = html;
        }

        function halGetStateInfo(state) {
            switch (state) {
                case 1: return { cls: 'blue', label: 'Detected' };
                case 2: return { cls: 'amber', label: 'Configuring' };
                case 3: return { cls: 'green', label: 'Available' };
                case 4: return { cls: 'red', label: 'Unavailable' };
                case 5: return { cls: 'red', label: 'Error' };
                case 6: return { cls: 'amber', label: 'Manual' };
                case 7: return { cls: 'grey', label: 'Removed' };
                default: return { cls: 'grey', label: 'Unknown' };
            }
        }

        function halGetTypeInfo(type) {
            var types = {
                1: { label: 'DAC', icon: 'M12,3L1,9L12,15L21,10.09V17H23V9M5,13.18V17.18L12,21L19,17.18V13.18L12,17L5,13.18Z' },
                2: { label: 'ADC', icon: 'M12,2A3,3 0 0,1 15,5V11A3,3 0 0,1 12,14A3,3 0 0,1 9,11V5A3,3 0 0,1 12,2M19,11C19,14.53 16.39,17.44 13,17.93V21H11V17.93C7.61,17.44 5,14.53 5,11H7A5,5 0 0,0 12,16A5,5 0 0,0 17,11H19Z' },
                3: { label: 'Codec', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' },
                4: { label: 'Amp', icon: 'M14,3.23V5.29C16.89,6.15 19,8.83 19,12C19,15.17 16.89,17.84 14,18.7V20.77C18,19.86 21,16.28 21,12C21,7.72 18,4.14 14,3.23M16.5,12C16.5,10.23 15.5,8.71 14,7.97V16C15.5,15.29 16.5,13.76 16.5,12M3,9V15H7L12,20V4L7,9H3Z' },
                5: { label: 'DSP', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' },
                6: { label: 'Sensor', icon: 'M15,13V5A3,3 0 0,0 9,5V13A5,5 0 1,0 15,13M12,4A1,1 0 0,1 13,5V8H11V5A1,1 0 0,1 12,4Z' },
                7: { label: 'Display', icon: 'M21,16H3V4H21M21,2H3C1.89,2 1,2.89 1,4V16A2,2 0 0,0 3,18H10V20H8V22H16V20H14V18H21A2,2 0 0,0 23,16V4C23,2.89 22.1,2 21,2Z' },
                8: { label: 'Input', icon: 'M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4M12,6A6,6 0 0,0 6,12A6,6 0 0,0 12,18A6,6 0 0,0 18,12A6,6 0 0,0 12,6M12,8A4,4 0 0,1 16,12A4,4 0 0,1 12,16A4,4 0 0,1 8,12A4,4 0 0,1 12,8Z' },
                9: { label: 'GPIO', icon: 'M16,7V3H14V7H10V3H8V7H8C7,7 6,8 6,9V14.5L9.5,18V21H14.5V18L18,14.5V9C18,8 17,7 16,7Z' }
            };
            return types[type] || { label: 'Unknown', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' };
        }

        function halGetBusLabel(busType) {
            return ['None', 'I2C', 'I2S', 'SPI', 'GPIO', 'Internal'][busType] || 'Unknown';
        }

        function halGetDiscLabel(disc) {
            return ['Builtin', 'EEPROM', 'GPIO ID', 'Manual', 'Online'][disc] || 'Unknown';
        }

        function buildHalDeviceCard(d) {
            var si = halGetStateInfo(d.state);
            var ti = halGetTypeInfo(d.type);
            var expanded = (halExpandedSlot === d.slot);
            var editing = (halEditingSlot === d.slot);
            var displayName = (d.userLabel && d.userLabel.length > 0) ? d.userLabel : (d.name || d.compatible);

            var stateClass = 'state-' + si.label.toLowerCase();
            var h = '<div class="card hal-device-card ' + stateClass + (expanded ? ' expanded' : '') + '">';

            // Header row - clickable to expand
            h += '<div class="hal-device-header" onclick="halToggleExpand(' + d.slot + ')">';
            h += '<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor" aria-hidden="true"><path d="' + ti.icon + '"/></svg>';
            h += '<span class="hal-device-name">' + escapeHtml(displayName) + '</span>';
            if (d.type === 6 && d.temperature !== undefined) {
                h += '<span class="hal-temp-reading">' + d.temperature.toFixed(1) + ' &deg;C</span>';
            }
            h += '<span class="status-dot status-' + si.cls + '" title="' + si.label + '"></span>';
            // Icon action buttons — left of toggle, stopPropagation so card doesn't expand
            h += '<button class="hal-icon-btn" onclick="event.stopPropagation();halStartEdit(' + d.slot + ')" title="Edit"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg></button>';
            if (d.discovery !== 0) {  // Can't remove builtins
                h += '<button class="hal-icon-btn hal-icon-btn-danger" onclick="event.stopPropagation();halConfirmRemove(' + d.slot + ',\'' + escapeHtml(displayName) + '\')" title="Remove device"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg></button>';
            }
            h += '<button class="hal-icon-btn" onclick="event.stopPropagation();halReinitDevice(' + d.slot + ')" title="Re-initialize"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M17.65,6.35C16.2,4.9 14.21,4 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20C15.73,20 18.84,17.45 19.73,14H17.65C16.83,16.33 14.61,18 12,18A6,6 0 0,1 6,12A6,6 0 0,1 12,6C13.66,6 15.14,6.69 16.22,7.78L13,11H20V4L17.65,6.35Z"/></svg></button>';
            h += '<button class="hal-icon-btn" onclick="event.stopPropagation();exportDeviceYaml(' + d.slot + ')" title="Export YAML"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M5,20H19V18H5M19,9H15V3H9V9H5L12,16L19,9Z"/></svg></button>';
            // Enable/disable toggle — visible on card without needing to open edit form
            var togChecked = (d.cfgEnabled !== false) ? 'checked' : '';
            h += '<label class="hal-enable-toggle" title="' + (d.cfgEnabled !== false ? 'Enabled — click to disable' : 'Disabled — click to enable') + '" onclick="event.stopPropagation()">';
            h += '<input type="checkbox" ' + togChecked + ' onchange="halToggleDeviceEnabled(' + d.slot + ',this.checked)">';
            h += '<span class="hal-toggle-track"></span></label>';
            h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" class="hal-expand-icon' + (expanded ? ' rotated' : '') + '" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
            h += '</div>';

            // Badge row
            h += '<div class="hal-device-info">';
            h += '<span class="badge badge-' + si.cls + '">' + si.label + '</span>';
            h += '<span class="badge">' + ti.label + '</span>';
            h += '<span class="badge">' + halGetDiscLabel(d.discovery) + '</span>';
            if (d.ready) h += '<span class="badge badge-green">Ready</span>';
            // Capability badges
            if (d.capabilities) {
                if (d.capabilities & HAL_CAP_HW_VOLUME)   h += '<span class="hal-cap-badge">Vol</span>';
                if (d.capabilities & HAL_CAP_MUTE)        h += '<span class="hal-cap-badge">Mute</span>';
                if (d.capabilities & HAL_CAP_PGA_CONTROL) h += '<span class="hal-cap-badge">PGA</span>';
                if (d.capabilities & HAL_CAP_HPF_CONTROL) h += '<span class="hal-cap-badge">HPF</span>';
                if (d.capabilities & HAL_CAP_CODEC)       h += '<span class="hal-cap-badge">Codec</span>';
            }
            h += '</div>';

            // Expanded detail section
            if (expanded) {
                h += '<div class="hal-device-details">';

                // Device info
                h += '<div class="hal-detail-row"><span>Compatible:</span><span>' + escapeHtml(d.compatible || '') + '</span></div>';
                if (d.manufacturer) h += '<div class="hal-detail-row"><span>Manufacturer:</span><span>' + escapeHtml(d.manufacturer) + '</span></div>';
                h += '<div class="hal-detail-row"><span>Bus:</span><span>' + halGetBusLabel(d.busType || 0) + (d.busIndex > 0 ? ' #' + d.busIndex : '') + '</span></div>';
                if (d.i2cAddr > 0) h += '<div class="hal-detail-row"><span>I2C Address:</span><span>0x' + d.i2cAddr.toString(16).toUpperCase().padStart(2, '0') + '</span></div>';
                if (d.busFreq > 0) h += '<div class="hal-detail-row"><span>Bus Freq:</span><span>' + (d.busFreq >= 1000000 ? (d.busFreq/1000000).toFixed(1) + ' MHz' : (d.busFreq/1000) + ' kHz') + '</span></div>';
                if (d.pinA > 0) {
                    var pinALabel = (d.busType === 1) ? 'Pin A (SDA):' : 'Pin A (Data):';
                    h += '<div class="hal-detail-row"><span>' + pinALabel + '</span><span>GPIO ' + d.pinA + '</span></div>';
                }
                if (d.pinB > 0) {
                    var pinBLabel = (d.busType === 1) ? 'Pin B (SCL):' : 'Pin B (CLK):';
                    h += '<div class="hal-detail-row"><span>' + pinBLabel + '</span><span>GPIO ' + d.pinB + '</span></div>';
                }
                if (d.channels > 0) h += '<div class="hal-detail-row"><span>Channels:</span><span>' + d.channels + '</span></div>';
                h += '<div class="hal-detail-row"><span>Slot:</span><span>' + d.slot + '</span></div>';

                // Capabilities
                if (d.capabilities > 0) {
                    var caps = [];
                    if (d.capabilities & 1) caps.push('HW Volume');
                    if (d.capabilities & 2) caps.push('Filters');
                    if (d.capabilities & 4) caps.push('Mute');
                    if (d.capabilities & 8) caps.push('ADC');
                    if (d.capabilities & 16) caps.push('DAC');
                    h += '<div class="hal-detail-row"><span>Capabilities:</span><span>' + caps.join(', ') + '</span></div>';
                }

                // Sample rates
                if (d.sampleRates > 0) {
                    var rates = [];
                    if (d.sampleRates & 1) rates.push('8k');
                    if (d.sampleRates & 2) rates.push('16k');
                    if (d.sampleRates & 4) rates.push('44.1k');
                    if (d.sampleRates & 8) rates.push('48k');
                    if (d.sampleRates & 16) rates.push('96k');
                    if (d.sampleRates & 32) rates.push('192k');
                    h += '<div class="hal-detail-row"><span>Sample Rates:</span><span>' + rates.join(', ') + '</span></div>';
                }

                // Edit form
                if (editing) {
                    h += halBuildEditForm(d);
                }


                h += '</div>'; // hal-device-details
            }

            h += '</div>'; // card
            return h;
        }

        function halBuildEditForm(d) {
            var busType = d.busType || 0;
            var h = '<div class="hal-edit-form">';
            h += '<div class="hal-form-title">Device Configuration</div>';

            // User label
            h += '<div class="hal-form-row">';
            h += '<label>Label:</label>';
            h += '<input type="text" id="halCfgLabel" value="' + escapeHtml(d.userLabel || '') + '" placeholder="' + escapeHtml(d.name || '') + '" maxlength="32">';
            h += '</div>';

            // Enable toggle
            h += '<div class="hal-form-row">';
            h += '<label>Enabled:</label>';
            h += '<label class="toggle-label"><input type="checkbox" id="halCfgEnabled" ' + (d.cfgEnabled !== false ? 'checked' : '') + '> <span>Active</span></label>';
            h += '</div>';

            // I2C settings (for I2C bus devices)
            if (busType === 1) {
                h += '<div class="hal-form-section">I2C Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Address:</label>';
                h += '<input type="text" id="halCfgI2cAddr" value="0x' + (d.i2cAddr || 0).toString(16).toUpperCase().padStart(2, '0') + '" style="width:60px;">';
                h += '</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Bus:</label>';
                h += '<select id="halCfgI2cBus">';
                h += '<option value="0"' + ((d.busIndex || 0) === 0 ? ' selected' : '') + '>External (GPIO 48/54)</option>';
                h += '<option value="1"' + ((d.busIndex || 0) === 1 ? ' selected' : '') + '>Onboard (GPIO 7/8)</option>';
                h += '<option value="2"' + ((d.busIndex || 0) === 2 ? ' selected' : '') + '>Expansion (GPIO 28/29)</option>';
                h += '</select>';
                h += '</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Speed:</label>';
                h += '<select id="halCfgI2cSpeed">';
                h += '<option value="100000"' + ((d.busFreq || 0) <= 100000 ? ' selected' : '') + '>100 kHz</option>';
                h += '<option value="400000"' + ((d.busFreq || 0) >= 400000 ? ' selected' : '') + '>400 kHz</option>';
                h += '</select>';
                h += '</div>';
            }

            // I2S settings (for audio devices: DAC, ADC, Codec)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Audio Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>I2S Port:</label>';
                h += '<select id="halCfgI2sPort">';
                for (var p = 0; p < 3; p++) {
                    h += '<option value="' + p + '"' + (p === (d.cfgI2sPort !== undefined ? d.cfgI2sPort : 0) ? ' selected' : '') + '>I2S ' + p + '</option>';
                }
                h += '</select>';
                h += '</div>';

                // Volume (for devices with HW volume capability)
                if (d.capabilities & 1) {
                    h += '<div class="hal-form-row">';
                    h += '<label>Volume:</label>';
                    h += '<input type="range" id="halCfgVolume" min="0" max="100" value="' + (d.cfgVolume !== undefined ? d.cfgVolume : 100) + '" oninput="document.getElementById(\'halCfgVolLabel\').textContent=this.value+\'%\'" style="flex:1;"><span id="halCfgVolLabel">' + (d.cfgVolume !== undefined ? d.cfgVolume : 100) + '%</span>';
                    h += '</div>';
                }

                // Mute (for devices with mute capability)
                if (d.capabilities & 4) {
                    h += '<div class="hal-form-row">';
                    h += '<label>Mute:</label>';
                    h += '<label class="toggle-label"><input type="checkbox" id="halCfgMute" ' + (d.cfgMute ? 'checked' : '') + '> <span>Muted</span></label>';
                    h += '</div>';
                }
            }

            // Audio Config section (for audio devices: DAC, ADC, Codec)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Audio Config</div>';
                h += '<div class="hal-form-row"><label>Sample Rate:</label>';
                h += '<select id="halCfgSampleRate">';
                h += '<option value="0"' + (!(d.cfgSampleRate) || d.cfgSampleRate === 0 ? ' selected' : '') + '>Auto</option>';
                h += '<option value="44100"' + (d.cfgSampleRate === 44100 ? ' selected' : '') + '>44100 Hz</option>';
                h += '<option value="48000"' + (d.cfgSampleRate === 48000 ? ' selected' : '') + '>48000 Hz</option>';
                h += '<option value="96000"' + (d.cfgSampleRate === 96000 ? ' selected' : '') + '>96000 Hz</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>Bit Depth:</label>';
                h += '<select id="halCfgBitDepth">';
                h += '<option value="0"' + (!(d.cfgBitDepth) || d.cfgBitDepth === 0 ? ' selected' : '') + '>Auto</option>';
                h += '<option value="16"' + (d.cfgBitDepth === 16 ? ' selected' : '') + '>16-bit</option>';
                h += '<option value="24"' + (d.cfgBitDepth === 24 ? ' selected' : '') + '>24-bit</option>';
                h += '<option value="32"' + (d.cfgBitDepth === 32 ? ' selected' : '') + '>32-bit</option>';
                h += '</select></div>';
            }

            // Advanced section (all audio devices)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Advanced</div>';

                h += '<div class="hal-form-row"><label>MCLK Multiple:</label>';
                h += '<select id="halCfgMclkMultiple">';
                h += '<option value="0"' + (!d.cfgMclkMultiple || d.cfgMclkMultiple === 0 ? ' selected' : '') + '>Auto (256×)</option>';
                h += '<option value="256"' + (d.cfgMclkMultiple === 256 ? ' selected' : '') + '>256×</option>';
                h += '<option value="384"' + (d.cfgMclkMultiple === 384 ? ' selected' : '') + '>384×</option>';
                h += '<option value="512"' + (d.cfgMclkMultiple === 512 ? ' selected' : '') + '>512×</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>I2S Format:</label>';
                h += '<select id="halCfgI2sFormat">';
                h += '<option value="0"' + (d.cfgI2sFormat === 0 || d.cfgI2sFormat === undefined ? ' selected' : '') + '>Philips (I2S)</option>';
                h += '<option value="1"' + (d.cfgI2sFormat === 1 ? ' selected' : '') + '>MSB / Left-Justified</option>';
                h += '<option value="2"' + (d.cfgI2sFormat === 2 ? ' selected' : '') + '>LSB / Right-Justified</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>PA Control Pin:</label>';
                h += '<input type="number" id="halCfgPaControlPin" value="' + (d.cfgPaControlPin !== undefined ? d.cfgPaControlPin : -1) + '" min="-1" max="53" style="width:60px;">';
                h += ' <span style="font-size:10px;opacity:0.5">(-1 = none)</span></div>';

                if (d.capabilities & HAL_CAP_PGA_CONTROL) {
                    h += '<div class="hal-form-row"><label>PGA Gain:</label>';
                    h += '<select id="halCfgPgaGain">';
                    for (var pi = 0; pi <= 42; pi += 6) {
                        h += '<option value="' + pi + '"' + (d.cfgPgaGain === pi ? ' selected' : '') + '>' + pi + ' dB</option>';
                    }
                    h += '</select></div>';
                }

                if (d.capabilities & HAL_CAP_HPF_CONTROL) {
                    h += '<div class="hal-form-row"><label>High-Pass Filter:</label>';
                    h += '<label class="toggle-label"><input type="checkbox" id="halCfgHpfEnabled"' + (d.cfgHpfEnabled ? ' checked' : '') + '> <span>Enabled</span></label>';
                    h += '</div>';
                }

                if (d.type === 2) {
                    var filterPresets = [
                        'Minimum Phase',
                        'Linear Apodizing Fast',
                        'Linear Fast',
                        'Linear Fast Low Ripple',
                        'Linear Slow',
                        'Minimum Fast',
                        'Minimum Slow',
                        'Minimum Slow Low Dispersion'
                    ];
                    h += '<div class="hal-form-row"><label>Filter Preset:</label>';
                    h += '<select id="halCfgFilterPreset">';
                    for (var fi = 0; fi < filterPresets.length; fi++) {
                        h += '<option value="' + fi + '"' + (d.cfgFilterMode === fi ? ' selected' : '') + '>' + filterPresets[fi] + '</option>';
                    }
                    h += '</select></div>';
                }
            }

            // GPIO settings (for GPIO devices like amp)
            if (busType === 4) {
                h += '<div class="hal-form-section">GPIO Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Pin:</label>';
                h += '<input type="number" id="halCfgPinA" value="' + (d.pinA >= 0 ? d.pinA : '') + '" min="0" max="54" style="width:60px;">';
                h += '</div>';
            }

            // Pin configuration
            if (busType === 1 || busType === 2) {
                h += '<div class="hal-form-section">Pin Configuration <span style="font-size:10px;opacity:0.5">(-1 = board default)</span></div>';
                if (busType === 1) {
                    h += '<div class="hal-form-row"><label>SDA Pin:</label>';
                    h += '<input type="number" id="halCfgPinSda" value="' + (d.cfgPinSda !== undefined ? d.cfgPinSda : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>SCL Pin:</label>';
                    h += '<input type="number" id="halCfgPinScl" value="' + (d.cfgPinScl !== undefined ? d.cfgPinScl : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                }
                if (busType === 2) {
                    h += '<div class="hal-form-row"><label>Data Pin:</label>';
                    h += '<input type="number" id="halCfgPinData" value="' + (d.cfgPinData !== undefined ? d.cfgPinData : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>MCLK Pin:</label>';
                    h += '<input type="number" id="halCfgPinMclk" value="' + (d.cfgPinMclk !== undefined ? d.cfgPinMclk : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>BCK Pin:</label>';
                    h += '<input type="number" id="halCfgPinBck" value="' + (d.cfgPinBck !== undefined ? d.cfgPinBck : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>LRC/WS Pin:</label>';
                    h += '<input type="number" id="halCfgPinLrc" value="' + (d.cfgPinLrc !== undefined ? d.cfgPinLrc : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    if (d.type === 2) {  // ADC only — FMT selects Philips vs MSB format
                        h += '<div class="hal-form-row"><label>FMT Pin:</label>';
                        h += '<input type="number" id="halCfgPinFmt" value="' + (d.cfgPinFmt !== undefined ? d.cfgPinFmt : -1) + '" min="-1" max="54" style="width:60px;">';
                        h += ' <span style="font-size:10px;opacity:0.5">(-1 = not wired; set HIGH for MSB, LOW for Philips)</span></div>';
                    }
                }
                h += '<div style="font-size:11px;opacity:0.55;margin-top:4px;padding:0 2px">Changes apply immediately on Save. I2S devices will have a brief audio dropout.</div>';
            }

            // Save/Cancel
            h += '<div class="hal-form-buttons">';
            h += '<button class="btn btn-primary btn-sm" onclick="halSaveConfig(' + d.slot + ')">Save</button>';
            h += '<button class="btn btn-sm" onclick="halCancelEdit()">Cancel</button>';
            h += '</div>';

            h += '</div>';
            return h;
        }

        function halToggleExpand(slot) {
            halExpandedSlot = (halExpandedSlot === slot) ? -1 : slot;
            halEditingSlot = -1;
            renderHalDevices();
        }

        function halToggleDeviceEnabled(slot, enabled) {
            fetch('/api/hal/devices', {
                method: 'PUT',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({slot: slot, enabled: enabled})
            }).then(function(r) {
                if (r.ok) loadHalDeviceList();
            });
        }

        function halStartEdit(slot) {
            halEditingSlot = slot;
            halExpandedSlot = slot;
            renderHalDevices();
        }

        function halCancelEdit() {
            halEditingSlot = -1;
            renderHalDevices();
        }

        function halSaveConfig(slot) {
            var cfg = {};
            cfg.slot = slot;

            var labelEl = document.getElementById('halCfgLabel');
            if (labelEl) cfg.label = labelEl.value;

            var enabledEl = document.getElementById('halCfgEnabled');
            if (enabledEl) cfg.enabled = enabledEl.checked;

            var i2cAddrEl = document.getElementById('halCfgI2cAddr');
            if (i2cAddrEl) cfg.i2cAddr = parseInt(i2cAddrEl.value, 16) || 0;

            var i2cBusEl = document.getElementById('halCfgI2cBus');
            if (i2cBusEl) cfg.i2cBus = parseInt(i2cBusEl.value) || 0;

            var i2cSpeedEl = document.getElementById('halCfgI2cSpeed');
            if (i2cSpeedEl) cfg.i2cSpeed = parseInt(i2cSpeedEl.value) || 0;

            var i2sPortEl = document.getElementById('halCfgI2sPort');
            if (i2sPortEl) cfg.i2sPort = parseInt(i2sPortEl.value) || 0;

            var volumeEl = document.getElementById('halCfgVolume');
            if (volumeEl) cfg.volume = parseInt(volumeEl.value) || 0;

            var muteEl = document.getElementById('halCfgMute');
            if (muteEl) cfg.mute = muteEl.checked;

            var pinSdaEl = document.getElementById('halCfgPinSda');
            if (pinSdaEl) cfg.pinSda = parseInt(pinSdaEl.value);

            var pinSclEl = document.getElementById('halCfgPinScl');
            if (pinSclEl) cfg.pinScl = parseInt(pinSclEl.value);

            cfg.pinData = parseInt(document.getElementById('halCfgPinData') ? document.getElementById('halCfgPinData').value : '-1') || -1;
            cfg.pinMclk = parseInt(document.getElementById('halCfgPinMclk') ? document.getElementById('halCfgPinMclk').value : '-1') || -1;

            var pinAEl = document.getElementById('halCfgPinA');
            if (pinAEl) cfg.pinSda = parseInt(pinAEl.value);

            // Audio config fields (key names match backend PUT handler)
            var srEl = document.getElementById('halCfgSampleRate');
            if (srEl) cfg.sampleRate = parseInt(srEl.value) || 0;
            var bdEl = document.getElementById('halCfgBitDepth');
            if (bdEl) cfg.bitDepth = parseInt(bdEl.value) || 0;
            var mclkEl = document.getElementById('halCfgMclkMultiple');
            if (mclkEl) cfg.cfgMclkMultiple = parseInt(mclkEl.value) || 0;
            var i2sfEl = document.getElementById('halCfgI2sFormat');
            if (i2sfEl) cfg.cfgI2sFormat = parseInt(i2sfEl.value) || 0;
            var pgaEl = document.getElementById('halCfgPgaGain');
            if (pgaEl) cfg.cfgPgaGain = parseInt(pgaEl.value) || 0;
            var hpfEl = document.getElementById('halCfgHpfEnabled');
            if (hpfEl) cfg.cfgHpfEnabled = hpfEl.checked;
            var filterPresetEl = document.getElementById('halCfgFilterPreset');
            if (filterPresetEl) cfg.filterMode = parseInt(filterPresetEl.value) || 0;
            var paEl = document.getElementById('halCfgPaControlPin');
            if (paEl) cfg.cfgPaControlPin = parseInt(paEl.value);
            // New I2S pin fields
            var pinBckEl = document.getElementById('halCfgPinBck');
            if (pinBckEl) cfg.pinBck = parseInt(pinBckEl.value);
            var pinLrcEl = document.getElementById('halCfgPinLrc');
            if (pinLrcEl) cfg.pinLrc = parseInt(pinLrcEl.value);
            var pinFmtEl = document.getElementById('halCfgPinFmt');
            if (pinFmtEl) cfg.pinFmt = parseInt(pinFmtEl.value);

            fetch('/api/hal/devices', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(cfg)
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.status === 'ok') {
                    showToast('Device config saved');
                    halEditingSlot = -1;
                    loadHalDeviceList();
                } else {
                    showToast('Save failed: ' + (data.error || ''), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halReinitDevice(slot) {
            fetch('/api/hal/devices/reinit', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slot })
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                showToast(data.status === 'ok' ? 'Device re-initialized' : 'Reinit failed', data.status !== 'ok');
                loadHalDeviceList();
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halConfirmRemove(slot, name) {
            if (!confirm('Remove "' + name + '"?\nThe device can be re-added later via Add Device.')) return;
            halRemoveDevice(slot);
        }

        function halRemoveDevice(slot) {
            fetch('/api/hal/devices', {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slot })
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.status === 'ok') {
                    showToast('Device removed');
                    halExpandedSlot = -1;
                    loadHalDeviceList();
                } else {
                    showToast('Remove failed: ' + (data.error || ''), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halAddFromPreset() {
            fetch('/api/hal/db/presets')
                .then(function(r) {
                    if (!r.ok) throw new Error('Server error ' + r.status);
                    return r.json();
                })
                .then(function(presets) {
                    var sel = document.getElementById('halAddPresetSelect');
                    if (!sel) return;
                    sel.innerHTML = '<option value="">-- Select Device --</option>';
                    for (var i = 0; i < presets.length; i++) {
                        var p = presets[i];
                        sel.innerHTML += '<option value="' + escapeHtml(p.compatible) + '">' + escapeHtml(p.name) + ' (' + ['Unknown','DAC','ADC','Codec','Amp','DSP','Sensor'][p.type || 0] + ')</option>';
                    }
                })
                .catch(function(err) { showToast('Failed to load presets: ' + err, true); });
        }

        function halRegisterPreset() {
            var sel = document.getElementById('halAddPresetSelect');
            if (!sel || !sel.value) { showToast('Select a device first', true); return; }

            fetch('/api/hal/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ compatible: sel.value })
            })
            .then(function(r) {
                if (!r.ok) return r.json().then(function(d) { throw new Error(d.error || ('HTTP ' + r.status)); });
                return r.json();
            })
            .then(function(data) {
                showToast('Device registered in slot ' + data.slot);
                loadHalDeviceList();
            })
            .catch(function(err) { showToast('Registration failed: ' + err, true); });
        }

        function triggerHalRescan() {
            if (halScanning) return;  // Prevent double-click
            halScanning = true;
            renderHalDevices();
            fetch('/api/hal/scan', { method: 'POST' })
                .then(function(r) {
                    if (r.status === 409) {
                        showToast('Scan already in progress');
                        return null;
                    }
                    return r.json();
                })
                .then(function(data) {
                    if (data) {
                        var msg = 'Scan complete: ' + (data.devicesFound || 0) + ' devices found';
                        if (data.partialScan) msg += ' (Bus 0 skipped — WiFi SDIO conflict)';
                        showToast(msg, data.partialScan);
                    }
                })
                .catch(function(err) {
                    showToast('Scan failed: ' + err.message, true);
                })
                .finally(function() {
                    halScanning = false;
                    renderHalDevices();
                });
        }

        function loadHalDeviceList() {
            fetch('/api/hal/devices')
                .then(function(r) { return r.json(); })
                .then(function(devices) {
                    halDevices = devices;
                    renderHalDevices();
                })
                .catch(function(err) {
                    console.error('Failed to load HAL devices:', err);
                });
            loadHalSettings();
        }

        function loadHalSettings() {
            fetch('/api/hal/settings')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    var cb = document.getElementById('halAutoDiscovery');
                    if (cb) cb.checked = d.halAutoDiscovery !== false;
                })
                .catch(function() {});
        }

        function setHalAutoDiscovery(enabled) {
            fetch('/api/hal/settings', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ halAutoDiscovery: enabled })
            })
            .then(function(r) { return r.json(); })
            .then(function() { showToast('Auto-discovery ' + (enabled ? 'enabled' : 'disabled'), 'success'); })
            .catch(function(err) { showToast('Failed: ' + err.message, 'error'); });
        }

        function exportDeviceYaml(slot) {
            var d = null;
            for (var i = 0; i < halDevices.length; i++) {
                if (halDevices[i].slot === slot) { d = halDevices[i]; break; }
            }
            if (!d) return;
            var yaml = deviceToYaml(d);
            var blob = new Blob([yaml], { type: 'text/yaml' });
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = (d.compatible || 'device').replace(/,/g, '_') + '.yaml';
            a.click();
            URL.revokeObjectURL(a.href);
        }

        function importDeviceYaml() {
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.yaml,.yml';
            input.onchange = function() {
                if (!input.files || !input.files[0]) return;
                var reader = new FileReader();
                reader.onload = function(e) {
                    var text = e.target.result;
                    var parsed = parseDeviceYaml(text);
                    if (!parsed || !parsed.compatible) {
                        showToast('Invalid YAML: missing compatible field', true);
                        return;
                    }
                    // Register with server
                    fetch('/api/hal/devices', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(parsed)
                    })
                    .then(function(r) { return r.json(); })
                    .then(function(data) {
                        if (data.status === 'ok') {
                            showToast('Device imported to slot ' + data.slot);
                            loadHalDeviceList();
                        } else {
                            showToast('Import failed: ' + (data.error || ''), true);
                        }
                    })
                    .catch(function(err) { showToast('Import error: ' + err, true); });
                };
                reader.readAsText(input.files[0]);
            };
            input.click();
        }

        var halUnknownDevices = [];

        function handleHalUnknownDevices(devices) {
            halUnknownDevices = devices || [];
            renderHalUnknownDevices();
        }

        // ===== Custom Device Upload =====

        var halCustomFileContent = null;

        function halOpenCustomUpload() {
            var modal = document.getElementById('halCustomUploadModal');
            if (modal) modal.style.display = 'flex';
            var preview = document.getElementById('halCustomPreview');
            if (preview) preview.style.display = 'none';
            var btn = document.getElementById('halCustomUploadBtn');
            if (btn) btn.disabled = true;
            halCustomFileContent = null;
        }

        function halCloseCustomUpload() {
            var modal = document.getElementById('halCustomUploadModal');
            if (modal) modal.style.display = 'none';
        }

        function halCustomFileSelected(input) {
            var file = input.files[0];
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function(e) {
                try {
                    var schema = JSON.parse(e.target.result);
                    halCustomFileContent = e.target.result;
                    var preview = document.getElementById('halCustomPreview');
                    if (preview) {
                        preview.style.display = 'block';
                        preview.innerHTML = '<strong>' + escapeHtml(schema.name || schema.compatible || 'Unknown') + '</strong><br>' +
                            'Compatible: ' + escapeHtml(schema.compatible || '—') + '<br>' +
                            'Component: ' + escapeHtml(schema.component || '—') + '<br>' +
                            'Bus: ' + escapeHtml(schema.bus || '—');
                    }
                    var btn = document.getElementById('halCustomUploadBtn');
                    if (btn) btn.disabled = false;
                } catch(err) {
                    showToast('Invalid JSON file', true);
                }
            };
            reader.readAsText(file);
        }

        function halUploadCustomDevice() {
            if (!halCustomFileContent) return;
            fetch('/api/hal/devices/custom', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: halCustomFileContent
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.ok) {
                    showToast('Custom device uploaded');
                    halCloseCustomUpload();
                    loadHalDeviceList();
                } else {
                    showToast('Upload failed: ' + (data.error || 'Unknown error'), true);
                }
            })
            .catch(function(err) { showToast('Upload error: ' + err, true); });
        }

        function renderHalUnknownDevices() {
            var container = document.getElementById('hal-unknown-list');
            if (!container) return;
            if (!halUnknownDevices || halUnknownDevices.length === 0) {
                container.innerHTML = '';
                return;
            }
            var h = '<div style="margin-top:8px;font-size:12px;font-weight:500;opacity:0.7;">Unidentified Devices</div>';
            for (var i = 0; i < halUnknownDevices.length; i++) {
                var u = halUnknownDevices[i];
                h += '<div class="card" style="border-left:3px solid #f90;margin-top:8px;">';
                h += '<div class="card-title" style="display:flex;align-items:center;gap:8px;">';
                h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="#f90" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg>';
                h += 'Unknown Device</div>';
                h += '<div class="info-row"><span class="info-label">I2C Address</span><span class="info-value">0x' + ((u.i2cAddr || u.i2cAddress || 0)).toString(16).toUpperCase().padStart(2,'0') + '</span></div>';
                if (u.deviceName) h += '<div class="info-row"><span class="info-label">Partial Name</span><span class="info-value">' + escapeHtml(u.deviceName) + '</span></div>';
                h += '<div style="font-size:11px;opacity:0.55;margin-top:6px;">No driver match found. Program via EEPROM Programming, then Rescan.</div>';
                h += '</div>';
            }
            container.innerHTML = h;
        }

//# sourceURL=15-hal-devices.js

        // HAL Device YAML Parser
        // Flat key-value YAML parser for ALX Nova HAL device descriptors

        function parseDeviceYaml(text) {
            var result = {};
            if (!text) return result;
            var lines = text.split('\n');
            for (var i = 0; i < lines.length; i++) {
                var line = lines[i].trim();
                if (!line || line.charAt(0) === '#') continue;
                var colonIdx = line.indexOf(':');
                if (colonIdx < 1) continue;
                var key = line.substring(0, colonIdx).trim();
                var val = line.substring(colonIdx + 1).trim();
                // Strip surrounding quotes
                if (val.length >= 2 && val.charAt(0) === '"' && val.charAt(val.length - 1) === '"') {
                    val = val.substring(1, val.length - 1);
                }
                result[key] = val;
            }
            return result;
        }

        function deviceToYaml(obj) {
            var lines = [];
            lines.push('hal_version: 1');
            if (obj.compatible) lines.push('compatible: "' + obj.compatible + '"');
            if (obj.name) lines.push('name: "' + obj.name + '"');
            if (obj.manufacturer) lines.push('manufacturer: "' + obj.manufacturer + '"');
            var typeNames = {1: 'DAC', 2: 'ADC', 3: 'CODEC', 4: 'AMP', 5: 'DSP', 6: 'SENSOR'};
            if (obj.type && typeNames[obj.type]) lines.push('device_type: ' + typeNames[obj.type]);
            if (obj.i2cAddr !== undefined) lines.push('i2c_default_address: 0x' + obj.i2cAddr.toString(16).toUpperCase().padStart(2, '0'));
            if (obj.channels) lines.push('channel_count: ' + obj.channels);
            return lines.join('\n') + '\n';
        }

        // importDeviceYaml() is defined in 15-hal-devices.js (full version with server registration)

//# sourceURL=15a-yaml-parser.js

// ===== WiFi Network Variables =====
let wifiScanInProgress = false;
// Store saved networks data globally for config management
let savedNetworksData = [];
// Store original network config to detect changes
let originalNetworkConfig = {
    useStaticIP: false,
    staticIP: '',
    subnet: '',
    gateway: '',
    dns1: '',
    dns2: ''
};
let networkRemovalPollTimer = null;
// Track connection attempts for network change detection
let connectionPollAttempts = 0;
let lastKnownNewIP = '';

// ===== WiFi Functions =====
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
        loader.innerHTML = '<svg viewBox="0 0 24 24" width="32" height="32" fill="var(--success)" aria-hidden="true"><path d="M12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22A10,10 0 0,1 2,12A10,10 0 0,1 12,2M11,16.5L18,9.5L16.59,8.09L11,13.67L7.91,10.59L6.5,12L11,16.5Z"/></svg>';
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
        loader.innerHTML = '<svg viewBox="0 0 24 24" width="32" height="32" fill="var(--error)" aria-hidden="true"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2M15.59,7L12,10.59L8.41,7L7,8.41L10.59,12L7,15.59L8.41,17L12,13.41L15.59,17L17,15.59L13.41,12L17,8.41L15.59,7Z"/></svg>';
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
                    <div class="modal-title"><svg viewBox="0 0 24 24" width="20" height="20" fill="var(--warning)" aria-hidden="true" style="vertical-align:middle;margin-right:6px;"><path d="M13,14H11V10H13M13,18H11V16H13M1,21H23L12,2L1,21Z"/></svg>Remove Current Network</div>
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

function openAPConfig() {
    document.getElementById('apConfigModal').style.display = 'flex';
}

function closeAPConfig() {
    document.getElementById('apConfigModal').classList.remove('active');
    document.getElementById('apConfigModal').style.display = 'none';
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

//# sourceURL=20-wifi-network.js

// ===== MQTT Settings =====

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

//# sourceURL=21-mqtt-settings.js

// ===== Settings / Theme Functions =====

        function handlePhysicalResetProgress(data) {
            showToast('Factory reset: ' + data.progress + '%', 'info');
        }

        function handlePhysicalRebootProgress(data) {
            showToast('Rebooting: ' + data.progress + '%', 'info');
        }

        function updateSmartAutoSettingsVisibility(mode) {
            const settingsCard = document.getElementById('smartAutoSettingsCard');
            if (settingsCard) {
                settingsCard.style.display = (mode === 'smart_auto') ? 'block' : 'none';
            }
        }

        function updateSmartSensingUI(data) {
            if (data.mode !== undefined) {
                let modeValue = data.mode;
                if (typeof data.mode === 'number') {
                    const modeMap = { 0: 'always_on', 1: 'always_off', 2: 'smart_auto' };
                    modeValue = modeMap[data.mode] || 'smart_auto';
                }
                document.querySelectorAll('input[name="sensingMode"]').forEach(function(radio) {
                    radio.checked = (radio.value === modeValue);
                });
                updateSmartAutoSettingsVisibility(modeValue);
                const modeLabels = { 'always_on': 'Always On', 'always_off': 'Always Off', 'smart_auto': 'Smart Auto' };
                const modeEl = document.getElementById('infoSensingMode');
                if (modeEl) modeEl.textContent = modeLabels[modeValue] || modeValue;
            }
            if (data.timerDuration !== undefined && !inputFocusState.timerDuration) {
                const el = document.getElementById('appState.timerDuration');
                if (el) el.value = data.timerDuration;
            }
            if (data.timerDuration !== undefined) {
                const el = document.getElementById('infoTimerDuration');
                if (el) el.textContent = data.timerDuration + ' min';
            }
            if (data.audioThreshold !== undefined && !inputFocusState.audioThreshold) {
                const el = document.getElementById('audioThreshold');
                if (el) el.value = Math.round(data.audioThreshold);
            }
            if (data.audioThreshold !== undefined) {
                const el = document.getElementById('infoAudioThreshold');
                if (el) el.textContent = Math.round(data.audioThreshold) + ' dBFS';
            }
            if (data.amplifierState !== undefined) {
                const display = document.getElementById('amplifierDisplay');
                const status = document.getElementById('amplifierStatus');
                if (display) display.classList.toggle('on', data.amplifierState);
                if (status) status.textContent = data.amplifierState ? 'ON' : 'OFF';
                currentAmpState = data.amplifierState;
                updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);
            }
            if (data.signalDetected !== undefined) {
                const el = document.getElementById('signalDetected');
                if (el) el.textContent = data.signalDetected ? 'Yes' : 'No';
            }
            if (data.audioLevel !== undefined) {
                const el = document.getElementById('audioLevel');
                if (el) el.textContent = data.audioLevel.toFixed(1) + ' dBFS';
            }
            if (data.audioVrms !== undefined) {
                const el = document.getElementById('audioVrms');
                if (el) el.textContent = data.audioVrms.toFixed(3) + ' V';
            }
            const timerDisplay = document.getElementById('timerDisplay');
            const timerValue = document.getElementById('timerValue');
            if (timerDisplay && timerValue) {
                if (data.timerActive && data.timerRemaining !== undefined) {
                    timerDisplay.classList.remove('hidden');
                    const mins = Math.floor(data.timerRemaining / 60);
                    const secs = data.timerRemaining % 60;
                    timerValue.textContent = mins.toString().padStart(2, '0') + ':' + secs.toString().padStart(2, '0');
                } else {
                    timerDisplay.classList.add('hidden');
                }
            }
            if (data.audioSampleRate !== undefined) {
                const sel = document.getElementById('audioSampleRateSelect');
                if (sel) sel.value = data.audioSampleRate.toString();
            }
        }

function toggleTheme() {
    darkMode = document.getElementById('darkModeToggle').checked;
    applyTheme(darkMode);
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 'appState.darkMode': darkMode })
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
        body: JSON.stringify({ 'appState.enableCertValidation': enableCertValidation })
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

function setFftWindow(val) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({type:'setFftWindowType', value:parseInt(val)}));
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

    // Populate pin table if included (sent once on connect)
    if (d.pins) {
        var ptb = document.getElementById('pinTableBody');
        if (ptb && !ptb.dataset.populated) {
            ptb.dataset.populated = '1';
            var catLabels = {audio:'Audio', display:'Display', input:'Input', core:'Core', network:'Network'};
            var html = '';
            for (var i = 0; i < d.pins.length; i++) {
                var p = d.pins[i];
                var catName = catLabels[p.c] || p.c;
                html += '<tr><td>' + p.g + '</td><td>' + p.f + '</td><td>' + p.d + '</td><td><span class="pin-cat pin-cat-' + p.c + '">' + catName + '</span></td></tr>';
            }
            ptb.innerHTML = html;
        }
    }
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

function confirmDestructiveAction(title, message, onConfirm) {
    var overlay = document.createElement('div');
    overlay.className = 'confirm-overlay';
    var dialog = document.createElement('div');
    dialog.className = 'confirm-dialog';

    var iconSvg = '<svg viewBox="0 0 24 24" width="24" height="24" fill="var(--warning, #FFC107)" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg>';

    dialog.innerHTML =
        '<div class="confirm-header">' + iconSvg + ' ' + title + '</div>' +
        '<div class="confirm-body">' + message + '</div>' +
        '<div class="confirm-actions">' +
            '<button class="btn confirm-cancel-btn" id="confirmCancelBtn">Cancel</button>' +
            '<button class="btn confirm-confirm-btn" id="confirmConfirmBtn" disabled>Confirm (3)</button>' +
        '</div>';

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    var confirmBtn = document.getElementById('confirmConfirmBtn');
    var cancelBtn = document.getElementById('confirmCancelBtn');
    var countdown = 3;
    var timer = setInterval(function() {
        countdown--;
        if (countdown > 0) {
            confirmBtn.textContent = 'Confirm (' + countdown + ')';
        } else {
            clearInterval(timer);
            confirmBtn.textContent = 'Confirm';
            confirmBtn.disabled = false;
        }
    }, 1000);

    cancelBtn.onclick = function() {
        clearInterval(timer);
        document.body.removeChild(overlay);
    };

    confirmBtn.onclick = function() {
        clearInterval(timer);
        document.body.removeChild(overlay);
        onConfirm();
    };

    overlay.onclick = function(e) {
        if (e.target === overlay) {
            clearInterval(timer);
            document.body.removeChild(overlay);
        }
    };
}

function startFactoryReset() {
    confirmDestructiveAction(
        'Factory Reset',
        'This will erase all settings and restore defaults. This cannot be undone.',
        function() {
            apiFetch('/api/factoryreset', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Factory reset in progress...', 'success');
            })
            .catch(err => showToast('Failed to reset', 'error'));
        }
    );
}

function startReboot() {
    confirmDestructiveAction(
        'Reboot Device',
        'The device will restart. You will lose connection temporarily.',
        function() {
            apiFetch('/api/reboot', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Rebooting...', 'success');
            })
            .catch(err => showToast('Failed to reboot', 'error'));
        }
    );
}

//# sourceURL=22-settings.js

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
    if (data.otaChannel !== undefined) {
        otaChannel = data.otaChannel;
        const sel = document.getElementById('otaChannelSelect');
        if (sel) sel.value = String(otaChannel);
    }

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

// ===== OTA Channel =====

function setOtaChannel() {
    const val = parseInt(document.getElementById('otaChannelSelect').value);
    otaChannel = val;
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ otaChannel: val })
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            showToast(val === 0 ? 'Channel: Stable' : 'Channel: Beta', 'success');
            cachedReleaseList = [];
            const browser = document.getElementById('releasesBrowser');
            if (browser && !browser.classList.contains('hidden')) {
                loadReleaseList();
            }
        }
    })
    .catch(() => showToast('Failed to set channel', 'error'));
}

// ===== Release Browser =====

function toggleReleasesBrowser() {
    const browser = document.getElementById('releasesBrowser');
    if (!browser) return;
    const isVisible = !browser.classList.contains('hidden');
    if (isVisible) {
        browser.classList.add('hidden');
    } else {
        browser.classList.remove('hidden');
        loadReleaseList();
    }
}

function loadReleaseList() {
    if (releaseListLoading) return;
    releaseListLoading = true;
    const loading = document.getElementById('releaseListLoading');
    const items = document.getElementById('releaseListItems');
    if (loading) loading.style.display = '';
    if (items) items.innerHTML = '';

    apiFetch('/api/releases')
    .then(res => res.json())
    .then(data => {
        releaseListLoading = false;
        if (loading) loading.style.display = 'none';
        if (!data.success || !data.releases || data.releases.length === 0) {
            if (items) items.innerHTML = '<div class="text-secondary" style="font-size:13px;padding:8px;">No releases found.</div>';
            return;
        }
        cachedReleaseList = data.releases;
        renderReleaseList(data.releases);
    })
    .catch(() => {
        releaseListLoading = false;
        if (loading) loading.style.display = 'none';
        if (items) items.innerHTML = '<div class="text-secondary" style="font-size:13px;padding:8px;">Failed to load releases.</div>';
    });
}

function renderReleaseList(releases) {
    const container = document.getElementById('releaseListItems');
    if (!container) return;
    const curVer = currentFirmwareVersion || '';
    container.innerHTML = '';
    releases.forEach(function(rel) {
        const isCurrent = rel.version === curVer || rel.version === 'v' + curVer || 'v' + rel.version === curVer;
        const isDown = isOlderRelease(rel.version, curVer);
        const badge = rel.isPrerelease ? ' <span style="color:var(--warning);font-size:11px;font-weight:600;">[beta]</span>' : '';
        const dateStr = rel.publishedAt || '';
        const div = document.createElement('div');
        div.style.cssText = 'display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid var(--border);';
        let btnHtml = '';
        if (isCurrent) {
            btnHtml = '<span style="color:var(--success);font-size:12px;white-space:nowrap;">Current</span>';
        } else if (isDown) {
            btnHtml = '<button class="btn btn-sm" style="background:var(--warning);color:#000;white-space:nowrap;" onclick="installRelease(\'' + rel.version.replace(/'/g, "\\'") + '\',true)">Downgrade</button>';
        } else {
            btnHtml = '<button class="btn btn-sm btn-primary" style="white-space:nowrap;" onclick="installRelease(\'' + rel.version.replace(/'/g, "\\'") + '\',false)">Install</button>';
        }
        const notesBtn = '<a href="#" class="release-notes-link" onclick="showReleaseNotesFor(\'' + rel.version.replace(/'/g, "\\'") + '\'); return false;" title="View release notes"><svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M13 9H11V7H13V9M13 17H11V11H13V17M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2M12 20C7.58 20 4 16.42 4 12C4 7.58 7.58 4 12 4C16.42 4 20 7.58 20 12C20 16.42 16.42 20 12 20Z"/></svg></a>';
        div.innerHTML = '<div style="min-width:0;"><span style="font-weight:600;">' + rel.version + '</span>' + badge + notesBtn +
            '<span class="text-secondary" style="font-size:12px;margin-left:8px;">' + dateStr + '</span></div>' +
            '<div style="margin-left:8px;">' + btnHtml + '</div>';
        container.appendChild(div);
    });
}

function isOlderRelease(v1, v2) {
    // Returns true if v1 is older than v2
    const strip = function(v) { return v.replace(/^v/, ''); };
    const a = strip(v1), b = strip(v2);
    const baseParts = function(s) { return s.replace(/-beta\.\d+/, '').split('.').map(Number); };
    const betaN = function(s) { const m = s.match(/-beta\.(\d+)/); return m ? parseInt(m[1]) : 0; };
    const pa = baseParts(a), pb = baseParts(b);
    for (let i = 0; i < 3; i++) {
        if ((pa[i]||0) < (pb[i]||0)) return true;
        if ((pa[i]||0) > (pb[i]||0)) return false;
    }
    const ba = betaN(a), bb = betaN(b);
    if (ba === 0 && bb === 0) return false;
    if (ba === 0 && bb > 0) return false;
    if (ba > 0 && bb === 0) return true;
    return ba < bb;
}

function installRelease(version, isDowngrade) {
    const msg = isDowngrade
        ? 'Downgrade to ' + version + '? Settings may be incompatible with older firmware.'
        : 'Install version ' + version + '?';
    if (!confirm(msg)) return;

    apiFetch('/api/installrelease', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ version: version })
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            document.getElementById('releasesBrowser').classList.add('hidden');
            document.getElementById('progressContainer').classList.remove('hidden');
            document.getElementById('progressStatus').classList.remove('hidden');
            showToast('Installing ' + version + '...', 'success');
        } else {
            showToast(data.message || 'Install failed', 'error');
        }
    })
    .catch(() => showToast('Failed to start install', 'error'));
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

//# sourceURL=23-firmware-update.js

// ===== Performance History =====

        var _eepromPresetsLoaded = false;
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

        function handleEepromDiag(eep) {
            if (!eep) return;
            var chipDetected = eep.scanned && eep.i2cMask > 0;
            var chipEmpty = chipDetected && !eep.found;
            var chipAddr = 0;
            if (eep.i2cMask > 0) {
                for (var b = 0; b < 8; b++) { if (eep.i2cMask & (1 << b)) { chipAddr = 0x50 + b; break; } }
            }
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
                var fields = { dbgEepromDeviceId: '0x' + (eep.deviceId||0).toString(16).padStart(4,'0').toUpperCase(),
                               dbgEepromName: eep.deviceName || '—', dbgEepromMfr: eep.manufacturer || '—',
                               dbgEepromRev: eep.hwRevision != null ? eep.hwRevision : '—',
                               dbgEepromCh: eep.maxChannels || '—',
                               dbgEepromDacAddr: eep.dacI2cAddress ? '0x' + eep.dacI2cAddress.toString(16).padStart(2,'0') : 'None' };
                Object.entries(fields).forEach(function(kv) { el = document.getElementById(kv[0]); if (el) el.textContent = kv[1]; });
                var flagStrs = [];
                if (eep.flags & 1) flagStrs.push('IndepClk');
                if (eep.flags & 2) flagStrs.push('HW Vol');
                if (eep.flags & 4) flagStrs.push('Filters');
                el = document.getElementById('dbgEepromFlags');
                if (el) el.textContent = flagStrs.length ? flagStrs.join(', ') : 'None';
                el = document.getElementById('dbgEepromRates');
                if (el) el.textContent = (eep.sampleRates || []).join(', ') || '—';
            } else {
                ['dbgEepromDeviceId','dbgEepromName','dbgEepromMfr','dbgEepromRev','dbgEepromCh','dbgEepromDacAddr','dbgEepromFlags','dbgEepromRates'].forEach(function(id) {
                    el = document.getElementById(id); if (el) el.textContent = '—';
                });
            }
            eepromLoadPresets();
        }

        function updateHardwareStats(data) {
            var el, i;
            // Update ADC count from hardware_stats (fires on all tabs)
            if (data.audio && data.audio.numAdcsDetected !== undefined) {
                numAdcsDetected = data.audio.numAdcsDetected;
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

                // Heap critical indicator
                const critRow = document.getElementById('heapCriticalRow');
                if (critRow) critRow.style.display = data.heapCritical ? '' : 'none';

                // DMA allocation failure indicator
                const dmaRow = document.getElementById('dmaAllocFailRow');
                if (dmaRow) {
                    dmaRow.style.display = data.dmaAllocFailed ? '' : 'none';
                    const dmaVal = document.getElementById('dmaAllocFailValue');
                    if (dmaVal && data.dmaAllocFailed) {
                        const mask = data.dmaAllocFailMask || 0;
                        const lanes = [];
                        for (let i = 0; i < 8; i++) { if (mask & (1 << i)) lanes.push('Lane ' + i); }
                        for (let i = 0; i < 8; i++) { if (mask & (1 << (i + 8))) lanes.push('Sink ' + i); }
                        dmaVal.textContent = lanes.length ? lanes.join(', ') : 'YES';
                    }
                }

                // PSRAM
                const psramTotal = data.memory.psramTotal || 0;
                const psramFree = data.memory.psramFree || 0;
                const psramPercent = psramTotal > 0 ? Math.round((1 - psramFree / psramTotal) * 100) : 0;
                document.getElementById('psramPercent').textContent = psramTotal > 0 ? psramPercent + '%' : 'N/A';
                document.getElementById('psramFree').textContent = psramTotal > 0 ? formatBytes(psramFree) : 'N/A';
                document.getElementById('psramTotal').textContent = psramTotal > 0 ? formatBytes(psramTotal) : 'N/A';
            }

            // PSRAM allocation tracking
            if (data.psramFallbackCount !== undefined) {
                var fbRow = document.getElementById('psramFallbackRow');
                if (fbRow) {
                    fbRow.style.display = data.psramFallbackCount > 0 ? '' : 'none';
                    var fbVal = document.getElementById('psramFallbackCount');
                    if (fbVal) fbVal.textContent = data.psramFallbackCount;
                }
                var fbBadge = document.getElementById('psramFallbackBadge');
                if (fbBadge) {
                    if (data.psramFallbackCount > 0) {
                        fbBadge.textContent = data.psramFallbackCount + ' fallback' + (data.psramFallbackCount > 1 ? 's' : '');
                        fbBadge.style.display = '';
                    } else {
                        fbBadge.style.display = 'none';
                    }
                }
                // Toast on new fallback
                if (typeof window._prevPsramFallbackCount === 'undefined') {
                    window._prevPsramFallbackCount = 0;
                }
                if (data.psramFallbackCount > window._prevPsramFallbackCount) {
                    showToast('PSRAM allocation fallback detected', 'warning');
                }
                window._prevPsramFallbackCount = data.psramFallbackCount;
            }

            // PSRAM pressure badge
            if (data.psramCritical !== undefined || data.psramWarning !== undefined) {
                var prBadge = document.getElementById('psramPressureBadge');
                if (prBadge) {
                    if (data.psramCritical) {
                        prBadge.textContent = 'CRITICAL';
                        prBadge.className = 'badge badge-red';
                        prBadge.style.display = '';
                    } else if (data.psramWarning) {
                        prBadge.textContent = 'WARNING';
                        prBadge.className = 'badge badge-amber';
                        prBadge.style.display = '';
                    } else {
                        prBadge.style.display = 'none';
                    }
                }
            }

            // PSRAM budget table
            if (data.heapBudget && Array.isArray(data.heapBudget)) {
                var budgetHeader = document.getElementById('psramBudgetHeader');
                if (budgetHeader && data.heapBudget.length > 0) budgetHeader.style.display = '';
                var budgetTbody = document.querySelector('#psramBudgetTable tbody');
                if (budgetTbody) {
                    budgetTbody.innerHTML = '';
                    for (var bi = 0; bi < data.heapBudget.length; bi++) {
                        var entry = data.heapBudget[bi];
                        if (entry.label && entry.bytes > 0) {
                            var bRow = document.createElement('tr');
                            var typeClass = entry.psram ? 'badge-green' : 'badge-amber';
                            var typeText = entry.psram ? 'PSRAM' : 'SRAM';
                            bRow.innerHTML = '<td>' + escapeHtml(entry.label) + '</td>' +
                                '<td>' + formatBytes(entry.bytes) + '</td>' +
                                '<td><span class="badge ' + typeClass + '">' + typeText + '</span></td>';
                            budgetTbody.appendChild(bRow);
                        }
                    }
                }
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
                    for (i = 0; i < data.audio.i2sConfig.length && i < 2; i++) {
                        var c = data.audio.i2sConfig[i];
                        el = undefined;
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
                        for (i = 0; i < rt.buffersPerSec.length && i < 2; i++) {
                            var tEl = document.getElementById('i2sThroughput' + i);
                            if (tEl) {
                                var bps = parseFloat(rt.buffersPerSec[i]) || 0;
                                tEl.textContent = bps.toFixed(1) + ' buf/s';
                                tEl.style.color = (bps > 0 && bps < expectedBps * 0.9) ? 'var(--error-color)' : '';
                            }
                        }
                    }
                    if (rt.avgReadLatencyUs) {
                        for (i = 0; i < rt.avgReadLatencyUs.length && i < 2; i++) {
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
                    tc = document.getElementById('taskCount');
                    if (tc) tc.textContent = 'Disabled';
                    la = document.getElementById('loopTimeAvg');
                    if (la) la.textContent = '-';
                    lm = document.getElementById('loopTimeMax');
                    if (lm) lm.textContent = '-';
                    lf = document.getElementById('tmLoopFreq');
                    if (lf) lf.textContent = '-';
                    el0 = document.getElementById('tmCpuCore0');
                    if (el0) el0.textContent = '-';
                    el1 = document.getElementById('tmCpuCore1');
                    if (el1) el1.textContent = '-';
                    elt = document.getElementById('tmCpuTotal');
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

            // DSP CPU Load
            var hasDspData = (data.pipelineCpu !== undefined || data.cpuLoadPercent !== undefined);
            var dspSection = document.getElementById('dsp-cpu-section');
            if (hasDspData) {
                if (dspSection) dspSection.style.display = '';

                // Per-input DSP CPU (from dspMetrics cpuLoadPercent)
                var dspInputEl = document.getElementById('dsp-cpu-input');
                if (dspInputEl && data.cpuLoadPercent !== undefined) {
                    dspInputEl.textContent = data.cpuLoadPercent.toFixed(1) + '%';
                    dspInputEl.style.color = data.cpuLoadPercent >= 95 ? 'var(--error-color)' :
                                             data.cpuLoadPercent >= 80 ? 'var(--warning-color)' : '';
                }

                // Pipeline total CPU
                var dspPipelineEl = document.getElementById('dsp-cpu-pipeline');
                if (dspPipelineEl && data.pipelineCpu !== undefined) {
                    dspPipelineEl.textContent = data.pipelineCpu.toFixed(1) + '%';
                    dspPipelineEl.style.color = data.pipelineCpu >= 95 ? 'var(--error-color)' :
                                                data.pipelineCpu >= 80 ? 'var(--warning-color)' : '';
                }

                // Frame time breakdown
                var dspFrameEl = document.getElementById('dsp-frame-us');
                if (dspFrameEl && data.pipelineFrameUs !== undefined) {
                    dspFrameEl.textContent = data.pipelineFrameUs + ' µs';
                }

                var dspMatrixEl = document.getElementById('dsp-matrix-us');
                if (dspMatrixEl && data.matrixUs !== undefined) {
                    dspMatrixEl.textContent = data.matrixUs + ' µs';
                }

                var dspOutputEl = document.getElementById('dsp-output-us');
                if (dspOutputEl && data.outputDspUs !== undefined) {
                    dspOutputEl.textContent = data.outputDspUs + ' µs';
                }

                // FIR bypass warning row
                var dspWarnRow = document.getElementById('dsp-cpu-warn-row');
                var dspFirEl = document.getElementById('dsp-fir-bypass');
                if (dspWarnRow && data.firBypassCount !== undefined) {
                    dspWarnRow.style.display = data.firBypassCount > 0 ? '' : 'none';
                    if (dspFirEl) dspFirEl.textContent = data.firBypassCount;
                }
            }

            // Add to history
            addHistoryDataPoint(data);

            // Pin table — populate from firmware data (once)
            if (data.pins) {
                var ptb = document.getElementById('pinTableBody');
                if (ptb && !ptb.dataset.populated) {
                    ptb.dataset.populated = '1';
                    var catLabels = {audio:'Audio', display:'Display', input:'Input', core:'Core', network:'Network'};
                    var html = '';
                    for (i = 0; i < data.pins.length; i++) {
                        var p = data.pins[i];
                        var catName = catLabels[p.c] || p.c;
                        html += '<tr><td>' + p.g + '</td><td>' + p.f + '</td><td>' + p.d + '</td><td><span class="pin-cat pin-cat-' + p.c + '">' + catName + '</span></td></tr>';
                    }
                    ptb.innerHTML = html;
                }
            }

            // Update PSRAM health banner on Health Dashboard
            if (data.psramFallbackCount !== undefined || data.psramWarning !== undefined || data.psramCritical !== undefined) {
                updatePsramHealthBanner(data);
            }
        }

        var psramBudgetOpen = false;
        function togglePsramBudget() {
            psramBudgetOpen = !psramBudgetOpen;
            var content = document.getElementById('psramBudgetContent');
            var chevron = document.getElementById('psramBudgetChevron');
            var header = document.getElementById('psramBudgetHeader');
            if (content) content.classList.toggle('open', psramBudgetOpen);
            if (chevron) chevron.parentElement.parentElement.classList.toggle('open', psramBudgetOpen);
            if (header) header.classList.toggle('open', psramBudgetOpen);
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
            var rows = [], i;
            for (i = 0; i < _taskData.length; i++) {
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
            for (i = 0; i < ths.length; i++) {
                ths[i].className = 'sort-arrow' + (i === col ? (asc ? ' asc' : ' desc') : '');
            }

            // Render rows
            var html = '';
            for (i = 0; i < rows.length; i++) {
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

        function eepromProgram() {
            var rates = document.getElementById('eepromRates').value.split(',').map(Number).filter(function(n){return n>0;});
            var flags = { independentClock: document.getElementById('eepromFlagClock').checked,
                          hwVolume: document.getElementById('eepromFlagVol').checked,
                          filters: document.getElementById('eepromFlagFilter').checked };
            var payload = {
                address: parseInt(document.getElementById('eepromTargetAddr').value),
                deviceId: parseInt(document.getElementById('eepromDeviceId').value),
                deviceName: document.getElementById('eepromDeviceName').value,
                manufacturer: document.getElementById('eepromManufacturer').value,
                hwRevision: parseInt(document.getElementById('eepromHwRev').value) || 1,
                maxChannels: parseInt(document.getElementById('eepromMaxCh').value) || 2,
                dacI2cAddress: parseInt(document.getElementById('eepromDacAddr').value),
                flags: flags, sampleRates: rates
            };
            apiFetch('/api/dac/eeprom', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(payload) })
            .then(function(r){ return r.json(); })
            .then(function(d){ if (d.success) showToast('EEPROM programmed successfully','success'); else showToast(d.message||'Program failed','error'); })
            .catch(function(){ showToast('EEPROM program failed','error'); });
        }

        function eepromErase() {
            if (!confirm('Erase EEPROM? This will clear all stored DAC identification data.')) return;
            var addr = parseInt(document.getElementById('eepromTargetAddr').value);
            apiFetch('/api/dac/eeprom/erase', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({ address: addr }) })
            .then(function(r){ return r.json(); })
            .then(function(d){ if (d.success) showToast('EEPROM erased','success'); else showToast(d.message||'Erase failed','error'); })
            .catch(function(){ showToast('EEPROM erase failed','error'); });
        }

        function eepromScan() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'eepromScan' }));
                showToast('Scanning I2C bus...', 'info');
            }
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

//# sourceURL=24-hardware-stats.js

// ===== Debug Console =====

        function appendDebugLog(timestamp, message, level, module) {
            level = level || 'info';
            if (debugPaused) {
                debugLogBuffer.push({ timestamp: timestamp, message: message, level: level, module: module });
                return;
            }

            // Determine log level from message if not provided
            var detectedLevel = level;
            if (message.includes('[E]') || message.includes('Error') || message.includes('❌')) {
                detectedLevel = 'error';
            } else if (message.includes('[W]') || message.includes('Warning') || message.includes('⚠')) {
                detectedLevel = 'warn';
            } else if (message.includes('[D]')) {
                detectedLevel = 'debug';
            } else if (message.includes('[I]') || message.includes('Info') || message.includes('ℹ')) {
                detectedLevel = 'info';
            }

            // Track module for chip creation
            module = module || extractModule(message) || 'Other';
            if (!knownModules[module]) {
                knownModules[module] = { total: 0, errors: 0, warnings: 0 };
                createModuleChip(module);
            }
            knownModules[module].total++;
            if (detectedLevel === 'error') knownModules[module].errors++;
            if (detectedLevel === 'warn')  knownModules[module].warnings++;
            updateChipBadge(module);

            var consoleEl = document.getElementById('debugConsole');
            var entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = detectedLevel;
            entry.dataset.module = module;

            var ts = formatDebugTimestamp(timestamp);
            var msgClass = 'log-message';
            if (detectedLevel === 'error') msgClass += ' error';
            else if (detectedLevel === 'warn') msgClass += ' warning';
            else if (detectedLevel === 'debug') msgClass += ' debug';
            else if (message.includes('✅') || message.includes('Success')) msgClass += ' success';
            else msgClass += ' info';

            var tsSpan = document.createElement('span');
            tsSpan.className = 'log-timestamp';
            tsSpan.dataset.ms = timestamp;
            tsSpan.textContent = '[' + ts + ']';

            var msgSpan = document.createElement('span');
            msgSpan.className = msgClass;
            msgSpan.textContent = message;

            entry.appendChild(tsSpan);
            entry.appendChild(msgSpan);

            // Apply combined filter visibility
            entry.style.display = isEntryVisible(entry) ? '' : 'none';

            // Apply search highlight if active
            if (debugSearchTerm) { applySearchHighlight(entry); }

            // Check if user is near the bottom before adding (within 40px)
            var wasAtBottom = (consoleEl.scrollHeight - consoleEl.scrollTop - consoleEl.clientHeight) < 40;

            consoleEl.appendChild(entry);

            // Limit entries
            while (consoleEl.children.length > DEBUG_MAX_LINES) {
                consoleEl.removeChild(consoleEl.firstChild);
            }

            // Only auto-scroll if user was already at the bottom
            if (wasAtBottom && entry.style.display !== 'none') {
                consoleEl.scrollTop = consoleEl.scrollHeight;
            }
        }

        function formatDebugTimestamp(ms) {
            if (debugTimestampMode === 'absolute' && ntpOffsetMs > 0) {
                var d = new Date(ntpOffsetMs + ms);
                return d.toLocaleTimeString() + '.' + String(ms % 1000).padStart(3, '0');
            }
            // Relative (uptime) mode
            var s = Math.floor(ms / 1000);
            var frac = ms % 1000;
            var hours = Math.floor(s / 3600); s %= 3600;
            var mins = Math.floor(s / 60); var secs = s % 60;
            return String(hours).padStart(2, '0') + ':' +
                   String(mins).padStart(2, '0') + ':' +
                   String(secs).padStart(2, '0') + '.' +
                   String(frac).padStart(3, '0');
        }

        function toggleDebugPause() {
            debugPaused = !debugPaused;
            const btn = document.getElementById('pauseBtn');
            if (debugPaused) {
                btn.textContent = 'Resume';
            } else {
                btn.textContent = 'Pause';
                // Flush buffer
                debugLogBuffer.forEach(function(log) { appendDebugLog(log.timestamp, log.message, log.level, log.module); });
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
                th.querySelector('.sort-arrow').innerHTML = '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg>';
            });
            const th = table.querySelectorAll('th')[col];
            th.classList.add('sorted');
            th.querySelector('.sort-arrow').innerHTML = pinSortAsc
                ? '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg>'
                : '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
        }

        function clearDebugConsole() {
            knownModules = {};
            var chipsContainer = document.getElementById('moduleChips');
            if (chipsContainer) chipsContainer.innerHTML = '';

            var consoleEl = document.getElementById('debugConsole');
            var entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = 'info';
            entry.dataset.module = 'Other';
            entry.innerHTML = '<span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span>';

            consoleEl.innerHTML = '';
            consoleEl.appendChild(entry);
            debugLogBuffer = [];
        }

        function setLogFilter(level) {
            currentLogFilter = level;
            refilterAll();
            saveDebugFilters();
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

        // ===== Module Chip Filtering =====
        // escapeHtml() defined in 14-io-registry.js (loads earlier in concat order)

        function extractModule(msg) {
            var m = msg.match(/\[[DIWE]\]\s*\[([^\]]+)\]/);
            return m ? m[1] : null;
        }

        function createModuleChip(module) {
            var container = document.getElementById('moduleChips');
            if (!container) return;
            var chip = document.createElement('button');
            chip.className = 'btn-chip';
            chip.dataset.module = module;
            chip.innerHTML = escapeHtml(module) + ' <span class="chip-badge">0</span>';
            chip.onclick = function() { toggleModuleFilter(module); };
            if (currentModuleFilters.has(module)) {
                chip.classList.add('active');
            }
            container.appendChild(chip);
        }

        function toggleModuleFilter(module) {
            if (currentModuleFilters.has(module)) {
                currentModuleFilters.delete(module);
            } else {
                currentModuleFilters.add(module);
            }
            var chips = document.querySelectorAll('#moduleChips .btn-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.toggle('active', currentModuleFilters.has(chips[i].dataset.module));
            }
            refilterAll();
            saveDebugFilters();
        }

        function clearModuleFilter() {
            currentModuleFilters.clear();
            var chips = document.querySelectorAll('#moduleChips .btn-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.remove('active');
            }
            refilterAll();
            saveDebugFilters();
        }

        function updateChipBadge(module) {
            var chip = document.querySelector('#moduleChips .btn-chip[data-module="' + CSS.escape(module) + '"]');
            if (!chip) return;
            var info = knownModules[module];
            var badge = chip.querySelector('.chip-badge');
            if (!badge) return;
            badge.textContent = info.total;
            badge.className = 'chip-badge';
            if (info.errors > 0) badge.classList.add('has-errors');
            else if (info.warnings > 0) badge.classList.add('has-warnings');
        }

        // ===== Search =====

        function setDebugSearch(term) {
            debugSearchTerm = term.toLowerCase();
            refilterAll();
            saveDebugFilters();
        }

        function clearDebugSearch() {
            debugSearchTerm = '';
            var input = document.getElementById('debugSearchInput');
            if (input) input.value = '';
            refilterAll();
            saveDebugFilters();
        }

        function applySearchHighlight(entry) {
            var msgSpan = entry.querySelector('.log-message');
            if (!msgSpan) return;
            if (!debugSearchTerm) {
                msgSpan.textContent = msgSpan.textContent;
                return;
            }
            var text = msgSpan.textContent;
            var lower = text.toLowerCase();
            var idx = lower.indexOf(debugSearchTerm);
            if (idx >= 0) {
                var before = text.substring(0, idx);
                var match = text.substring(idx, idx + debugSearchTerm.length);
                var after = text.substring(idx + debugSearchTerm.length);
                msgSpan.innerHTML = escapeHtml(before) +
                    '<span class="log-highlight">' + escapeHtml(match) + '</span>' +
                    escapeHtml(after);
            } else {
                msgSpan.textContent = msgSpan.textContent;
            }
        }

        // ===== Combined Visibility & Re-filter =====

        function isEntryVisible(entry) {
            if (currentLogFilter !== 'all' && entry.dataset.level !== currentLogFilter) return false;
            if (currentModuleFilters.size > 0 && !currentModuleFilters.has(entry.dataset.module)) return false;
            if (debugSearchTerm) {
                var msg = entry.querySelector('.log-message');
                if (msg && msg.textContent.toLowerCase().indexOf(debugSearchTerm) === -1) return false;
            }
            return true;
        }

        function refilterAll() {
            var consoleEl = document.getElementById('debugConsole');
            if (!consoleEl) return;
            for (var i = 0; i < consoleEl.children.length; i++) {
                var entry = consoleEl.children[i];
                entry.style.display = isEntryVisible(entry) ? '' : 'none';
                if (debugSearchTerm) applySearchHighlight(entry);
            }
            consoleEl.scrollTop = consoleEl.scrollHeight;
        }

        // ===== Timestamp Toggle =====

        function toggleTimestampMode() {
            debugTimestampMode = (debugTimestampMode === 'relative') ? 'absolute' : 'relative';
            var btn = document.getElementById('timestampToggle');
            if (btn) btn.textContent = (debugTimestampMode === 'relative') ? 'Uptime' : 'Clock';
            var entries = document.querySelectorAll('#debugConsole .log-entry');
            for (var i = 0; i < entries.length; i++) {
                var ts = entries[i].querySelector('.log-timestamp');
                if (ts && ts.dataset.ms) {
                    ts.textContent = '[' + formatDebugTimestamp(parseInt(ts.dataset.ms)) + ']';
                }
            }
            saveDebugFilters();
        }

        // ===== Filter Persistence =====

        function saveDebugFilters() {
            try {
                localStorage.setItem('debugFilters', JSON.stringify({
                    level: currentLogFilter,
                    modules: Array.from(currentModuleFilters),
                    search: debugSearchTerm,
                    timestampMode: debugTimestampMode
                }));
            } catch(e) {}
        }

        function loadDebugFilters() {
            try {
                var saved = JSON.parse(localStorage.getItem('debugFilters'));
                if (!saved) return;
                if (saved.level) {
                    currentLogFilter = saved.level;
                    var sel = document.getElementById('logLevelFilter');
                    if (sel) sel.value = saved.level;
                }
                if (saved.modules && saved.modules.length) {
                    currentModuleFilters = new Set(saved.modules);
                }
                if (saved.search) {
                    debugSearchTerm = saved.search;
                    var input = document.getElementById('debugSearchInput');
                    if (input) input.value = saved.search;
                }
                if (saved.timestampMode) {
                    debugTimestampMode = saved.timestampMode;
                    var btn = document.getElementById('timestampToggle');
                    if (btn) btn.textContent = (debugTimestampMode === 'relative') ? 'Uptime' : 'Clock';
                }
            } catch(e) {}
        }

//# sourceURL=25-debug-console.js

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
            const regex = new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g, '\\/* JS_INJECT */'), 'gi');

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

        // ===== Release Notes =====
        function showReleaseNotes() {
            showReleaseNotesFor('latest');
        }

        function showReleaseNotesFor(which) {
            let version, label;
            if (which === 'current') {
                version = currentFirmwareVersion;
                label = 'Current';
            } else if (which === 'latest') {
                version = currentLatestVersion;
                label = 'Latest';
            } else {
                version = which;
                label = null;
            }

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
                const titleText = label ? `Release Notes v${version} (${label})` : `Release Notes v${version}`;
                document.getElementById('inlineReleaseNotesTitle').textContent = titleText;
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

//# sourceURL=26-support.js

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
                <div class="warning-icon"><svg viewBox="0 0 24 24" width="24" height="24" fill="var(--warning)" aria-hidden="true"><path d="M13,14H11V10H13M13,18H11V16H13M1,21H23L12,2L1,21Z"/></svg></div>
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

//# sourceURL=27-auth.js

        // ===== Health Dashboard =====
        // Phase 8 of Debug Architecture: aggregated device health,
        // error counters, and diagnostic event timeline.

        // ----- State -----
        var healthEvents = [];         // diagEvent objects, newest-first (max 100)
        var healthErrorCounts = {};    // { subsystem: { err: N, warn: N } }
        var healthInitialized = false;

        // Subsystem list used for counter table (stable order)
        var HEALTH_SUBSYSTEMS = ['HAL', 'Audio', 'DSP', 'WiFi', 'MQTT', 'System', 'OTA', 'USB'];

        // ----- WS handler -----
        function handleDiagEvent(data) {
            // Prepend to local buffer, cap at 100
            healthEvents.unshift(data);
            if (healthEvents.length > 100) healthEvents.length = 100;

            // Increment error/warn counter for subsystem
            var sub = data.sub || 'System';
            if (!healthErrorCounts[sub]) healthErrorCounts[sub] = { err: 0, warn: 0 };
            if (data.sev === 'E' || data.sev === 'C') {
                healthErrorCounts[sub].err++;
            } else if (data.sev === 'W') {
                healthErrorCounts[sub].warn++;
            }

            // Re-render if health tab is visible
            if (currentActiveTab === 'health') {
                renderHealthDashboard();
            }
        }

        // ----- Rendering entry point -----
        function renderHealthDashboard() {
            renderHealthDeviceGrid();
            renderHealthErrorCounters();
            renderHealthEventList();
            var ts = document.getElementById('healthLastUpdate');
            if (ts) ts.textContent = 'Updated ' + new Date().toLocaleTimeString();
        }

        // ----- Device Grid -----
        function renderHealthDeviceGrid() {
            var container = document.getElementById('healthDeviceGrid');
            if (!container) return;

            if (!halDevices || halDevices.length === 0) {
                container.innerHTML = '<div class="health-empty">No HAL devices registered</div>';
                return;
            }

            var html = '';
            for (var i = 0; i < halDevices.length; i++) {
                var d = halDevices[i];
                var si = halGetStateInfo(d.state);
                var ti = halGetTypeInfo(d.type);
                var stateClass = 'state-ok';
                if (d.state === 5) stateClass = 'state-error';
                else if (d.state === 4 || d.state === 2) stateClass = 'state-warn';
                else if (d.state !== 3) stateClass = '';

                var retries = (d.retries !== undefined) ? d.retries : 0;
                var faults = (d.faults !== undefined) ? d.faults : 0;
                var lastErr = (d.lastErr !== undefined && d.lastErr !== 0) ? '0x' + d.lastErr.toString(16).toUpperCase().padStart(4, '0') : '';

                html += '<div class="health-device-card ' + stateClass + '">';
                html += '<div class="device-name">' + escapeHtml(d.name || 'Device ' + d.slot) + '</div>';
                html += '<div class="device-meta">' + escapeHtml(ti.label) + ' &middot; Slot ' + d.slot + ' &middot; <span style="color:var(--' + (si.cls === 'green' ? 'success' : si.cls === 'red' ? 'error' : si.cls === 'amber' ? 'warning' : 'text-secondary') + ')">' + escapeHtml(si.label) + '</span></div>';
                html += '<div class="device-stats">';
                html += '<span class="stat-label">Retries</span><span class="' + (retries > 0 ? 'stat-val-err' : 'stat-val-ok') + '">' + retries + '</span>';
                html += '<span class="stat-label">Faults</span><span class="' + (faults > 0 ? 'stat-val-err' : 'stat-val-ok') + '">' + faults + '</span>';
                if (lastErr) {
                    html += '<span class="stat-label">Last Err</span><span class="stat-val-err">' + lastErr + '</span>';
                }
                html += '</div>';
                html += '</div>';
            }
            container.innerHTML = html;
        }

        // ----- Error Counters -----
        function renderHealthErrorCounters() {
            var container = document.getElementById('healthErrorCounters');
            if (!container) return;

            var html = '<table class="health-counter-table">';
            html += '<tr><th style="text-align:left;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Subsystem</th>';
            html += '<th style="text-align:center;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Errors</th>';
            html += '<th style="text-align:center;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Warnings</th></tr>';

            for (var i = 0; i < HEALTH_SUBSYSTEMS.length; i++) {
                var sub = HEALTH_SUBSYSTEMS[i];
                var counts = healthErrorCounts[sub] || { err: 0, warn: 0 };
                html += '<tr>';
                html += '<td class="subsys-label">' + escapeHtml(sub) + '</td>';
                html += '<td style="text-align:center;" class="' + (counts.err > 0 ? 'cnt-err' : 'cnt-none') + '">';
                if (counts.err > 0) {
                    html += '<svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true" style="vertical-align:-1px;margin-right:2px;"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2Z"/></svg>';
                    html += counts.err;
                } else {
                    html += '—';
                }
                html += '</td>';
                html += '<td style="text-align:center;" class="' + (counts.warn > 0 ? 'cnt-warn' : 'cnt-none') + '">';
                if (counts.warn > 0) {
                    html += '<svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true" style="vertical-align:-1px;margin-right:2px;"><path d="M1,21H23L12,2L1,21Z"/></svg>';
                    html += counts.warn;
                } else {
                    html += '—';
                }
                html += '</td>';
                html += '</tr>';
            }
            html += '</table>';
            container.innerHTML = html;
        }

        // ----- Event List -----
        function renderHealthEventList() {
            var container = document.getElementById('healthEventList');
            if (!container) return;

            if (healthEvents.length === 0) {
                container.innerHTML = '<div class="health-empty">No diagnostic events recorded</div>';
                return;
            }

            var html = '<table><thead><tr>';
            html += '<th>Time</th><th>Sev</th><th>Code</th><th>Device</th><th>Message</th><th>Corr</th>';
            html += '</tr></thead><tbody>';

            var limit = Math.min(healthEvents.length, 50);
            var now = Date.now();
            for (var i = 0; i < limit; i++) {
                var ev = healthEvents[i];
                var sevClass = 'sev-i';
                var sevIcon = '';
                switch (ev.sev) {
                    case 'E':
                        sevClass = 'sev-e';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2Z"/></svg>';
                        break;
                    case 'W':
                        sevClass = 'sev-w';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M1,21H23L12,2L1,21Z"/></svg>';
                        break;
                    case 'I':
                        sevClass = 'sev-i';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M13,9H11V7H13M13,17H11V11H13M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg>';
                        break;
                    case 'C':
                        sevClass = 'sev-c';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M12,2L2,12L12,22L22,12L12,2Z"/></svg>';
                        break;
                }

                var timeStr = healthFormatRelativeTime(ev.ts, now);
                var codeStr = ev.code ? '0x' + ev.code.toString(16).toUpperCase().padStart(4, '0') : '';
                var deviceStr = ev.dev ? escapeHtml(ev.dev) : '';
                var msgStr = ev.msg ? escapeHtml(ev.msg) : '';
                var corrStr = (ev.corr !== undefined && ev.corr > 0) ? '#' + ev.corr : '';

                html += '<tr>';
                html += '<td style="white-space:nowrap;">' + timeStr + '</td>';
                html += '<td class="' + sevClass + '">' + sevIcon + '</td>';
                html += '<td style="font-family:monospace;font-size:12px;">' + codeStr + '</td>';
                html += '<td>' + deviceStr + '</td>';
                html += '<td>' + msgStr + '</td>';
                html += '<td class="corr">' + corrStr + '</td>';
                html += '</tr>';
            }
            html += '</tbody></table>';
            container.innerHTML = html;
        }

        // ----- Relative time formatter -----
        function healthFormatRelativeTime(tsMs, nowMs) {
            if (!tsMs) return '--';
            var diff = Math.max(0, nowMs - tsMs);
            if (diff < 1000) return 'now';
            if (diff < 60000) return Math.floor(diff / 1000) + 's ago';
            if (diff < 3600000) return Math.floor(diff / 60000) + 'm ago';
            if (diff < 86400000) return Math.floor(diff / 3600000) + 'h ago';
            return Math.floor(diff / 86400000) + 'd ago';
        }

        // ----- Action: Snapshot download -----
        function healthSnapshot() {
            apiFetch('/api/diag/snapshot').then(function(r) {
                return r.json();
            }).then(function(data) {
                var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                var a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = 'diag-snapshot-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
                a.click();
                URL.revokeObjectURL(a.href);
                showToast('Snapshot downloaded', 'success');
            }).catch(function(err) {
                showToast('Snapshot failed: ' + err.message, 'error');
            });
        }

        // ----- Action: Download journal -----
        function healthDownloadJournal() {
            apiFetch('/api/diagnostics/journal').then(function(r) {
                return r.json();
            }).then(function(data) {
                var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                var a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = 'diag-journal-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
                a.click();
                URL.revokeObjectURL(a.href);
                showToast('Journal downloaded', 'success');
            }).catch(function(err) {
                showToast('Download failed: ' + err.message, 'error');
            });
        }

        // ----- Action: Clear journal -----
        function healthClearJournal() {
            if (!confirm('Clear diagnostic journal? This cannot be undone.')) return;
            apiFetch('/api/diagnostics/journal', { method: 'DELETE' }).then(function() {
                healthEvents = [];
                healthErrorCounts = {};
                renderHealthDashboard();
                showToast('Journal cleared', 'success');
            }).catch(function(err) {
                showToast('Clear failed: ' + err.message, 'error');
            });
        }

        // ----- PSRAM Health Banner -----
        function updatePsramHealthBanner(data) {
            var banner = document.getElementById('psramHealthBanner');
            if (!banner) {
                var container = document.getElementById('healthDeviceGrid');
                if (!container || !container.parentElement) return;
                banner = document.createElement('div');
                banner.id = 'psramHealthBanner';
                banner.className = 'health-banner';
                banner.style.display = 'none';
                container.parentElement.insertBefore(banner, container.parentElement.firstChild.nextSibling);
            }

            var alertIcon = '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg> ';
            var infoIcon = '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="M13,9H11V7H13M13,17H11V11H13M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg> ';
            var fbCount = data.psramFallbackCount || 0;

            if (data.psramCritical) {
                banner.className = 'health-banner health-banner-red';
                banner.innerHTML = alertIcon + 'PSRAM Critical — ' + fbCount + ' allocation(s) fell back to SRAM';
                banner.style.display = '';
            } else if (data.psramWarning) {
                banner.className = 'health-banner health-banner-amber';
                banner.innerHTML = alertIcon + 'PSRAM Warning — ' + fbCount + ' allocation(s) fell back to SRAM';
                banner.style.display = '';
            } else if (fbCount > 0) {
                banner.className = 'health-banner health-banner-amber';
                banner.innerHTML = infoIcon + fbCount + ' PSRAM allocation(s) fell back to SRAM';
                banner.style.display = '';
            } else {
                banner.style.display = 'none';
            }
        }

        // ----- Tab init (called from switchTab) -----
        function initHealthDashboard() {
            if (healthInitialized) return;
            healthInitialized = true;
            // Initial render with whatever state we have
            renderHealthDashboard();
        }

//# sourceURL=27a-health-dashboard.js

        // ===== Window Resize Handler =====
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(function() {
                canvasDims = {};
                invalidateBgCache();
                drawCpuGraph();
                drawMemoryGraph();
                drawPsramGraph();
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

            // Initial status bar update
            updateStatusBar(false, null, false, false);

            // Check if settings tab is active and start time updates
            const activePanel = document.querySelector('.panel.active');
            if (activePanel && activePanel.id === 'settings') {
                startTimeUpdates();
            }

            // Check for default password warning
            checkPasswordWarning();

            // Restore debug console filter state from localStorage
            loadDebugFilters();
        };

//# sourceURL=28-init.js
</script>

    <!-- Custom Device Upload Modal -->
    <div id="halCustomUploadModal" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,0.6);z-index:1000;align-items:center;justify-content:center;">
      <div style="background:#1e2025;border-radius:8px;padding:20px;width:460px;max-width:95vw;box-shadow:0 8px 32px rgba(0,0,0,0.5);">
        <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:14px;">
          <span style="font-weight:600;font-size:14px;">Add Custom Device</span>
          <button onclick="halCloseCustomUpload()" style="background:none;border:none;color:inherit;cursor:pointer;font-size:18px;opacity:0.6;">&times;</button>
        </div>
        <p style="font-size:12px;opacity:0.6;margin:0 0 12px;">Upload a JSON device schema. The device will appear in the Devices list after upload.</p>
        <div onclick="document.getElementById('halCustomFileInput').click()" style="border:2px dashed rgba(255,255,255,0.15);border-radius:6px;padding:20px;text-align:center;cursor:pointer;transition:border-color .15s;" onmouseover="this.style.borderColor='rgba(255,255,255,0.3)'" onmouseout="this.style.borderColor='rgba(255,255,255,0.15)'">
          <svg viewBox="0 0 24 24" width="28" height="28" fill="currentColor" aria-hidden="true" style="opacity:0.35;margin-bottom:6px;display:block;margin-left:auto;margin-right:auto;"><path d="M9,16V10H5L12,3L19,10H15V16H9M5,20V18H19V20H5Z"/></svg>
          <div style="font-size:12px;opacity:0.45;">Click to select JSON file</div>
          <input type="file" id="halCustomFileInput" accept=".json" style="display:none" onchange="halCustomFileSelected(this)">
        </div>
        <div id="halCustomPreview" style="display:none;margin-top:10px;padding:8px 10px;background:rgba(255,255,255,0.05);border-radius:4px;font-size:12px;line-height:1.6;"></div>
        <div style="margin-top:14px;display:flex;justify-content:flex-end;gap:8px;">
          <button class="btn btn-sm" onclick="halCloseCustomUpload()">Cancel</button>
          <button class="btn btn-primary btn-sm" id="halCustomUploadBtn" onclick="halUploadCustomDevice()" disabled>Upload</button>
        </div>
      </div>
    </div>

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
