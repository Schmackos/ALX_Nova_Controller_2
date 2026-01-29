#include <pgmspace.h>
#include "web_pages.h"

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 LED Control</title>
    <style>
        /* Day/Night Mode Variables */
        :root {
            --bg-gradient-start: #667eea;
            --bg-gradient-end: #764ba2;
            --container-bg: rgba(255, 255, 255, 0.1);
            --text-color: white;
            --shadow-color: rgba(0, 0, 0, 0.3);
            --transition-speed: 0.3s;
        }
        
        body.night-mode {
            --bg-gradient-start: #1a1a2e;
            --bg-gradient-end: #16213e;
            --container-bg: rgba(0, 0, 0, 0.4);
            --text-color: #e0e0e0;
            --shadow-color: rgba(0, 0, 0, 0.6);
        }
        
        body {
            font-family: Arial, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
            margin: 0;
            background: linear-gradient(135deg, var(--bg-gradient-start) 0%, var(--bg-gradient-end) 100%);
            color: var(--text-color);
            transition: background var(--transition-speed) ease, color var(--transition-speed) ease;
            position: relative;
        }
        .container {
            background: var(--container-bg);
            padding: 40px;
            border-radius: 20px;
            box-shadow: 0 8px 32px var(--shadow-color);
            backdrop-filter: blur(10px);
            text-align: center;
            transition: background var(--transition-speed) ease, box-shadow var(--transition-speed) ease;
        }
        h1 {
            margin-top: 0;
            font-size: 2.5em;
        }
        .led-container {
            margin: 40px 0;
        }
        .led {
            width: 100px;
            height: 100px;
            border-radius: 50%;
            margin: 20px auto;
            border: 4px solid rgba(255, 255, 255, 0.3);
            box-shadow: 0 0 20px rgba(0, 0, 0, 0.3);
            transition: all 0.3s ease;
        }
        .led.on {
            background: #ffeb3b;
            box-shadow: 0 0 30px #ffeb3b, inset 0 0 20px rgba(255, 235, 59, 0.5);
        }
        .led.off {
            background: #333;
            box-shadow: inset 0 0 20px rgba(0, 0, 0, 0.5);
        }
        button {
            background: #4CAF50;
            color: white;
            border: none;
            padding: 15px 40px;
            font-size: 1.2em;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        button:hover {
            background: #45a049;
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0, 0, 0, 0.3);
        }
        button:active {
            transform: translateY(0);
        }
        button.stop {
            background: #f44336;
        }
        button.stop:hover {
            background: #da190b;
        }
        .status {
            margin-top: 20px;
            font-size: 1.1em;
            opacity: 0.9;
        }
        .connection-status {
            margin-top: 10px;
            font-size: 0.9em;
            padding: 10px;
            border-radius: 5px;
            background: rgba(0, 0, 0, 0.2);
        }
        .connected {
            color: #4CAF50;
        }
        .disconnected {
            color: #f44336;
        }
        .wifi-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .wifi-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
        }
        .form-group {
            margin: 15px 0;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: bold;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 1em;
            color: #333;
        }
        .toggle-container {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin: 20px 0;
            padding: 15px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 10px;
        }
        .toggle-label {
            font-size: 1.1em;
            font-weight: bold;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
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
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: #4CAF50;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .wifi-status {
            margin-top: 15px;
            padding: 10px;
            border-radius: 5px;
            background: rgba(0, 0, 0, 0.2);
            font-size: 0.9em;
        }
        .wifi-status.connected {
            color: #4CAF50;
        }
        .wifi-status.ap {
            color: #ff9800;
        }
        button.config-btn {
            background: #2196F3;
            margin-top: 10px;
        }
        button.config-btn:hover {
            background: #0b7dda;
        }
        .password-wrapper {
            position: relative;
        }
        .password-toggle {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            cursor: pointer;
            padding: 5px;
            font-size: 1.2em;
            color: #666;
        }
        .password-toggle:hover {
            color: #333;
        }
        .form-note {
            margin-top: 5px;
            font-size: 0.85em;
            color: #ff9800;
            font-style: italic;
        }
        .ota-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .ota-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
        }
        .version-info {
            margin: 15px 0;
            padding: 15px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 5px;
            font-size: 0.95em;
        }
        .version-info .current {
            margin-bottom: 10px;
        }
        .version-info .latest {
            margin-top: 10px;
            padding-top: 10px;
            border-top: 1px solid rgba(255, 255, 255, 0.1);
        }
        .version-info .update-available {
            color: #4CAF50;
            font-weight: bold;
        }
        .version-info .up-to-date {
            color: #4CAF50;
        }
        button.check-btn {
            background: #2196F3;
            margin-top: 10px;
            width: 100%;
            display: block;
        }
        button.check-btn:hover {
            background: #0b7dda;
        }
        button.update-btn {
            background: #ff9800;
            margin-top: 15px;
            width: 100%;
            display: block;
        }
        button.update-btn:hover {
            background: #f57c00;
        }
        .ota-status {
            margin-top: 10px;
            padding: 10px;
            border-radius: 5px;
            background: rgba(0, 0, 0, 0.2);
            font-size: 0.9em;
            display: none;
        }
        .ota-status.show {
            display: block;
        }
        .ota-status.updating {
            color: #ff9800;
        }
        .ota-status.success {
            color: #4CAF50;
        }
        .ota-status.error {
            color: #f44336;
        }
        .ota-status.update-available {
            color: #ff9800;
        }
        /* Progress Bar Styles */
        .progress-container {
            width: 100%;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            overflow: hidden;
            margin: 20px 0;
            display: none;
        }
        .progress-container.show {
            display: block;
        }
        .progress-bar {
            height: 30px;
            background: linear-gradient(90deg, #4CAF50, #45a049);
            border-radius: 10px;
            transition: width 0.3s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
            font-size: 0.9em;
            box-shadow: 0 2px 10px rgba(76, 175, 80, 0.5);
        }
        .progress-status {
            margin-top: 10px;
            font-size: 0.9em;
            color: #fff;
            text-align: center;
        }
        /* Modal for Release Notes */
        .modal {
            display: none;
            position: fixed;
            z-index: 1000;
            left: 0;
            top: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.7);
            backdrop-filter: blur(5px);
        }
        .modal.show {
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .modal-content {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 30px;
            border-radius: 20px;
            max-width: 600px;
            max-height: 80vh;
            overflow-y: auto;
            box-shadow: 0 10px 50px rgba(0, 0, 0, 0.5);
            color: white;
        }
        .modal-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid rgba(255, 255, 255, 0.2);
        }
        .modal-header h2 {
            margin: 0;
        }
        .close-btn {
            background: rgba(255, 255, 255, 0.2);
            border: none;
            color: white;
            font-size: 1.5em;
            cursor: pointer;
            width: 35px;
            height: 35px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: background 0.3s;
            padding: 0;
        }
        .close-btn:hover {
            background: rgba(255, 255, 255, 0.3);
            transform: none;
        }
        .release-notes-content {
            background: rgba(0, 0, 0, 0.2);
            padding: 20px;
            border-radius: 10px;
            white-space: pre-wrap;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            line-height: 1.6;
        }
        .notes-btn {
            background: #2196F3;
            margin-left: 10px;
            padding: 8px 20px;
            font-size: 0.9em;
            display: inline-block;
        }
        .notes-btn:hover {
            background: #0b7dda;
        }
        .timezone-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .timezone-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
        }
        select {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 1em;
            color: #333;
            background: white;
            cursor: pointer;
        }
        select:focus {
            outline: 2px solid #4CAF50;
        }
        .timezone-info {
            margin-top: 15px;
            padding: 10px;
            border-radius: 5px;
            background: rgba(0, 0, 0, 0.2);
            font-size: 0.9em;
        }
        .reboot-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .reboot-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
            color: #3b82f6;
        }
        .reboot-info {
            padding: 15px;
            background: rgba(59, 130, 246, 0.2);
            border-left: 4px solid #3b82f6;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 0.95em;
        }
        button.reboot-btn {
            background: #3b82f6;
            width: 100%;
            position: relative;
            overflow: hidden;
        }
        button.reboot-btn:hover {
            background: #2563eb;
        }
        button.reboot-btn:disabled {
            background: #999;
            cursor: not-allowed;
            opacity: 0.6;
        }
        button.reboot-btn.holding {
            background: #60a5fa;
        }
        .reboot-progress {
            position: absolute;
            left: 0;
            top: 0;
            height: 100%;
            background: rgba(255, 255, 255, 0.3);
            width: 0%;
            transition: width 0.1s linear;
        }
        .reboot-btn-text {
            position: relative;
            z-index: 1;
        }
        .factory-reset-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .factory-reset-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
            color: #ff9800;
        }
        .reset-warning {
            padding: 15px;
            background: rgba(244, 67, 54, 0.2);
            border-left: 4px solid #f44336;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 0.95em;
        }
        button.reset-btn {
            background: #f44336;
            width: 100%;
            position: relative;
            overflow: hidden;
        }
        button.reset-btn:hover {
            background: #da190b;
        }
        button.reset-btn:disabled {
            background: #999;
            cursor: not-allowed;
            opacity: 0.6;
        }
        button.reset-btn.holding {
            background: #ff5722;
        }
        .reset-progress {
            position: absolute;
            left: 0;
            top: 0;
            height: 100%;
            background: rgba(255, 255, 255, 0.3);
            width: 0%;
            transition: width 0.1s linear;
        }
        .reset-btn-text {
            position: relative;
            z-index: 1;
        }
        /* Debug Console Section */
        .debug-section {
            margin-top: 30px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
        }
        .debug-section h2 {
            font-size: 1.5em;
            margin-bottom: 20px;
            color: #9c27b0;
        }
        .debug-info {
            padding: 15px;
            background: rgba(156, 39, 176, 0.2);
            border-left: 4px solid #9c27b0;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 0.95em;
        }
        .debug-console {
            background: #1e1e1e;
            border-radius: 8px;
            padding: 15px;
            font-family: 'Courier New', Consolas, monospace;
            font-size: 0.85em;
            height: 300px;
            overflow-y: auto;
            text-align: left;
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        .debug-console::-webkit-scrollbar {
            width: 8px;
        }
        .debug-console::-webkit-scrollbar-track {
            background: #2d2d2d;
            border-radius: 4px;
        }
        .debug-console::-webkit-scrollbar-thumb {
            background: #555;
            border-radius: 4px;
        }
        .debug-console::-webkit-scrollbar-thumb:hover {
            background: #777;
        }
        .log-entry {
            padding: 3px 0;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            word-wrap: break-word;
        }
        .log-entry:last-child {
            border-bottom: none;
        }
        .log-timestamp {
            color: #888;
            margin-right: 10px;
        }
        .log-message {
            color: #00ff00;
        }
        .log-message.info {
            color: #00bcd4;
        }
        .log-message.warn {
            color: #ffeb3b;
        }
        .log-message.error {
            color: #f44336;
        }
        .debug-controls {
            display: flex;
            gap: 10px;
            margin-top: 15px;
            flex-wrap: wrap;
        }
        .debug-controls button {
            padding: 10px 20px;
            font-size: 0.9em;
        }
        .debug-btn-clear {
            background: #757575;
        }
        .debug-btn-clear:hover {
            background: #616161;
        }
        .debug-btn-pause {
            background: #ff9800;
        }
        .debug-btn-pause:hover {
            background: #f57c00;
        }
        .debug-btn-pause.paused {
            background: #4CAF50;
        }
        .debug-btn-pause.paused:hover {
            background: #45a049;
        }
        .debug-status {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-left: auto;
            font-size: 0.9em;
        }
        .debug-status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #4CAF50;
        }
        .debug-status-indicator.paused {
            background: #ff9800;
        }
        /* Hardware Stats Panel */
        .hardware-stats-section {
            margin-bottom: 20px;
        }
        .hardware-stats-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            flex-wrap: wrap;
            gap: 10px;
        }
        .hardware-stats-header h3 {
            margin: 0;
            color: #9c27b0;
            font-size: 1.2em;
        }
        .refresh-selector {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.9em;
        }
        .refresh-selector select {
            padding: 5px 10px;
            border-radius: 5px;
            border: 1px solid rgba(255, 255, 255, 0.3);
            background: rgba(0, 0, 0, 0.3);
            color: inherit;
            font-size: 0.9em;
            cursor: pointer;
        }
        .hardware-stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
            margin-bottom: 15px;
        }
        @media (max-width: 600px) {
            .hardware-stats-grid {
                grid-template-columns: 1fr;
            }
        }
        .stats-card {
            background: rgba(0, 0, 0, 0.2);
            border-radius: 10px;
            padding: 15px;
            border-left: 4px solid #888;
        }
        .stats-card.memory {
            border-left-color: #2196F3;
        }
        .stats-card.cpu {
            border-left-color: #ff9800;
        }
        .stats-card.storage {
            border-left-color: #4CAF50;
        }
        .stats-card.wifi {
            border-left-color: #9c27b0;
        }
        .stats-card h4 {
            margin: 0 0 12px 0;
            font-size: 0.95em;
            text-transform: uppercase;
            letter-spacing: 1px;
            opacity: 0.8;
        }
        .stats-card.memory h4 { color: #2196F3; }
        .stats-card.cpu h4 { color: #ff9800; }
        .stats-card.storage h4 { color: #4CAF50; }
        .stats-card.wifi h4 { color: #9c27b0; }
        .stat-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
            font-size: 0.9em;
        }
        .stat-row:last-child {
            margin-bottom: 0;
        }
        .stat-label {
            opacity: 0.7;
        }
        .stat-value {
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        .stat-bar {
            width: 100%;
            height: 8px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 4px;
            margin-top: 5px;
            overflow: hidden;
        }
        .stat-bar-fill {
            height: 100%;
            border-radius: 4px;
            transition: width 0.3s ease;
        }
        .stat-bar-fill.memory { background: #2196F3; }
        .stat-bar-fill.cpu { background: #ff9800; }
        .stat-bar-fill.storage { background: #4CAF50; }
        .stat-bar-fill.wifi { background: #9c27b0; }
        .stat-bar-fill.warning { background: #ff9800; }
        .stat-bar-fill.danger { background: #f44336; }
        .cpu-core-label {
            font-size: 0.8em;
            opacity: 0.6;
            margin-right: 5px;
        }
        .cpu-usage-value {
            min-width: 45px;
            text-align: right;
        }
        .uptime-bar {
            text-align: center;
            padding: 10px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 1.1em;
        }
        .uptime-label {
            opacity: 0.7;
            margin-right: 10px;
        }
        .signal-quality {
            display: inline-block;
            padding: 2px 8px;
            border-radius: 4px;
            font-size: 0.85em;
            margin-left: 5px;
        }
        .signal-quality.excellent { background: #4CAF50; color: white; }
        .signal-quality.good { background: #8BC34A; color: white; }
        .signal-quality.fair { background: #ff9800; color: white; }
        .signal-quality.poor { background: #f44336; color: white; }
        .device-control-section {
            margin-bottom: 30px;
            padding: 30px;
            border: 2px solid rgba(255, 255, 255, 0.3);
            border-radius: 15px;
            background: rgba(255, 255, 255, 0.05);
        }
        .device-control-section h3 {
            font-size: 1.3em;
            margin-top: 20px;
            margin-bottom: 15px;
        }
        .radio-group {
            display: flex;
            flex-direction: column;
            gap: 12px;
            margin: 15px 0;
        }
        .radio-option {
            display: flex;
            align-items: center;
            padding: 12px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 8px;
            cursor: pointer;
            transition: background 0.3s;
        }
        .radio-option:hover {
            background: rgba(0, 0, 0, 0.3);
        }
        .radio-option input[type="radio"] {
            margin-right: 12px;
            cursor: pointer;
            width: 18px;
            height: 18px;
        }
        .radio-option span {
            font-size: 1.05em;
        }
        .timer-display {
            font-size: 2em;
            font-weight: bold;
            color: #4CAF50;
            margin: 20px 0;
            padding: 20px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            text-align: center;
        }
        .amplifier-status {
            display: inline-block;
            padding: 8px 16px;
            border-radius: 20px;
            font-weight: bold;
            margin: 10px 0;
        }
        .amplifier-status.on {
            background: #4CAF50;
            color: white;
        }
        .amplifier-status.off {
            background: #f44336;
            color: white;
        }
        .manual-controls {
            display: flex;
            gap: 10px;
            margin-top: 15px;
        }
        .manual-controls button {
            flex: 1;
        }
        input[type="number"] {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 5px;
            font-size: 1em;
            color: #333;
        }
        input[type="number"]:disabled {
            background: #e0e0e0;
            cursor: not-allowed;
        }
        .voltage-info {
            margin-top: 10px;
            padding: 10px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 5px;
            font-size: 0.9em;
        }
        /* Theme Toggle Button */
        .theme-toggle {
            position: absolute;
            top: 20px;
            right: 20px;
            background: rgba(255, 255, 255, 0.2);
            border: none;
            padding: 12px 16px;
            border-radius: 50px;
            cursor: pointer;
            font-size: 1.2em;
            transition: all 0.3s ease;
            backdrop-filter: blur(10px);
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
            color: var(--text-color);
            z-index: 1000;
        }
        .theme-toggle:hover {
            background: rgba(255, 255, 255, 0.3);
            transform: scale(1.1);
        }
        .theme-toggle:active {
            transform: scale(0.95);
        }
        /* Drag and Drop Zone */
        .drop-zone {
            border: 2px dashed rgba(255, 255, 255, 0.4);
            border-radius: 10px;
            padding: 40px 20px;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s ease;
            background: rgba(0, 0, 0, 0.2);
            margin-top: 15px;
        }
        .drop-zone:hover {
            border-color: rgba(255, 255, 255, 0.6);
            background: rgba(0, 0, 0, 0.3);
            transform: scale(1.02);
        }
        .drop-zone.drag-over {
            border-color: #4CAF50;
            background: rgba(76, 175, 80, 0.2);
            border-style: solid;
            transform: scale(1.05);
        }
        .drop-zone-icon {
            font-size: 3em;
            margin-bottom: 10px;
            opacity: 0.8;
        }
        .drop-zone-text {
            font-size: 1.1em;
            margin-bottom: 5px;
        }
        .drop-zone-hint {
            font-size: 0.9em;
            opacity: 0.7;
        }
        /* Performance History Section */
        .history-section {
            margin-top: 20px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 10px;
            overflow: hidden;
        }
        .history-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px 20px;
            cursor: pointer;
            background: rgba(0, 0, 0, 0.1);
            transition: background 0.2s ease;
        }
        .history-header:hover {
            background: rgba(0, 0, 0, 0.2);
        }
        .history-header h3 {
            margin: 0;
            font-size: 1.1em;
            color: #9c27b0;
        }
        .collapse-icon {
            font-size: 0.9em;
            transition: transform 0.3s ease;
        }
        .collapse-icon.collapsed {
            transform: rotate(-90deg);
        }
        .history-content {
            padding: 15px 20px 20px;
            display: block;
        }
        .history-content.collapsed {
            display: none;
        }
        .history-controls {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 15px;
            font-size: 0.9em;
        }
        .history-controls select {
            padding: 5px 10px;
            border-radius: 5px;
            border: 1px solid rgba(255, 255, 255, 0.3);
            background: rgba(0, 0, 0, 0.3);
            color: inherit;
            font-size: 0.9em;
            cursor: pointer;
        }
        .graph-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        @media (max-width: 700px) {
            .graph-container {
                grid-template-columns: 1fr;
            }
        }
        .graph-card {
            background: rgba(0, 0, 0, 0.15);
            border-radius: 8px;
            padding: 15px;
        }
        .graph-card h4 {
            margin: 0 0 10px 0;
            font-size: 0.95em;
            opacity: 0.8;
        }
        .graph-card canvas {
            width: 100%;
            height: 150px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 5px;
        }
        .graph-legend {
            display: flex;
            justify-content: center;
            gap: 15px;
            margin-top: 10px;
            font-size: 0.8em;
        }
        .legend-item {
            display: flex;
            align-items: center;
            gap: 5px;
            opacity: 0.8;
        }
        .legend-item.total { color: #ff9800; }
        .legend-item.core0 { color: #ffb74d; }
        .legend-item.core1 { color: #f57c00; }
        .legend-item.heap { color: #2196F3; }
    </style>
</head>
<body>
    <!-- Theme Toggle Button -->
    <button class="theme-toggle" onclick="toggleTheme()" id="themeToggle" title="Toggle Day/Night Mode">
        🌙
    </button>
    
    <div class="container">
        <h1>ESP32-S3 LED Control</h1>
        
        <!-- Device Control Settings Section -->
        <div class="device-control-section">
            <h2>Device Control Settings</h2>
            
            <h3>Smart Sensing On/Off</h3>
            <div class="radio-group">
                <label class="radio-option">
                    <input type="radio" name="sensingMode" value="always_on" checked onchange="updateSensingMode()">
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
            
            <div class="form-group">
                <label for="timerDuration">Auto-Off Timer Duration (minutes):</label>
                <input type="number" id="timerDuration" min="1" max="60" value="15" 
                       onchange="updateTimerDuration()" 
                       onfocus="inputFocusState.timerDuration = true" 
                       onblur="inputFocusState.timerDuration = false">
            </div>
            
            <div class="form-group">
                <label for="voltageThreshold">Voltage Detection Threshold (volts):</label>
                <input type="number" id="voltageThreshold" min="0.1" max="3.3" step="0.1" value="1.0" 
                       onchange="updateVoltageThreshold()" 
                       onfocus="inputFocusState.voltageThreshold = true" 
                       onblur="inputFocusState.voltageThreshold = false">
            </div>
            
            <div class="timer-display" id="timerDisplay" style="display: none;">
                Time Remaining: <span id="timerValue">--:--</span>
            </div>
            
            <div class="status">
                Amplifier Status: <span class="amplifier-status off" id="amplifierStatus">OFF</span>
            </div>
            
            <div class="voltage-info" id="voltageInfo">
                Voltage Detected: <span id="voltageDetected">No</span> | 
                Current Reading: <span id="voltageReading">0.00V</span>
            </div>
            
            <div class="manual-controls">
                <button onclick="manualOverride(true)" class="config-btn" style="background: #4CAF50;">Turn On Now</button>
                <button onclick="manualOverride(false)" class="config-btn" style="background: #f44336;">Turn Off Now</button>
            </div>
        </div>
        
        <div class="led-container">
            <div id="led" class="led off"></div>
        </div>
        <button id="toggleBtn" onclick="toggleBlinking();">Stop Blinking</button>
        <div class="status" id="status">Blinking: ON</div>
        <div class="connection-status" id="connectionStatus">
            <span class="disconnected">Disconnected</span>
        </div>
        
        <div class="wifi-section">
            <h2>WiFi Configuration</h2>
            <div class="wifi-status" id="wifiStatus">Loading...</div>
            
            <div class="toggle-container">
                <span class="toggle-label">ESP32 Access Point</span>
                <label class="switch">
                    <input type="checkbox" id="apToggle" onchange="toggleAP()">
                    <span class="slider"></span>
                </label>
            </div>
            
            <button class="config-btn" onclick="showAPConfig()">Configure Access Point</button>
            
            <form onsubmit="submitWiFiConfig(event)">
                <div class="form-group">
                    <label for="wifiSSID">Network Name (SSID):</label>
                    <input type="text" id="wifiSSID" name="ssid" placeholder="Enter WiFi SSID">
                </div>
                <div class="form-group">
                    <label for="wifiPassword">Password:</label>
                    <div class="password-wrapper">
                        <input type="password" id="wifiPassword" name="password" placeholder="Enter WiFi password">
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('wifiPassword', this)">👁️</button>
                    </div>
                </div>
                <button type="submit" class="config-btn">Connect to WiFi</button>
            </form>
        </div>
        
        <div class="ota-section">
            <h2>Firmware Update</h2>
            <div class="version-info">
                <div class="current">
                    <strong>Current Version:</strong> <span id="currentVersion">Loading...</span>
                    <button class="notes-btn" id="currentVersionNotesBtn" onclick="showCurrentVersionNotes()" style="display: inline-block; margin-left: 10px;">
                        View Release Notes
                    </button>
                </div>
                <div class="latest" id="latestVersionContainer" style="display: none;">
                    <strong>Latest Version:</strong> <span id="latestVersion"></span>
                    <button class="notes-btn" id="latestVersionNotesBtn" onclick="showReleaseNotes()" style="display: none;">
                        View Release Notes
                    </button>
                </div>
            </div>
            <div class="toggle-container">
                <span class="toggle-label">Auto update on boot</span>
                <label class="switch">
                    <input type="checkbox" id="autoUpdateToggle" onchange="toggleAutoUpdate()">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="toggle-container">
                <span class="toggle-label">Enable SSL Certificate Validation</span>
                <label class="switch">
                    <input type="checkbox" id="certValidationToggle" onchange="toggleCertValidation()">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="timezone-info" style="margin-top: 10px; margin-bottom: 15px;">
                Uses Mozilla's trusted certificate bundle for automatic SSL validation of all public servers.
            </div>
            
            <button class="check-btn" onclick="checkForUpdate()">Check for Updates</button>
            <button class="update-btn" id="updateBtn" onclick="startOTAUpdate()" style="display: none;">Update to Latest Version</button>
            <button class="update-btn" id="cancelUpdateBtn" onclick="cancelAutoUpdate()" style="display: none; background: #f44336;">Cancel Auto-Update</button>
            
            <!-- Progress Bar -->
            <div class="progress-container" id="progressContainer">
                <div class="progress-bar" id="progressBar">0%</div>
            </div>
            <div class="progress-status" id="progressStatus"></div>
            
            <div class="ota-status" id="otaStatus"></div>
        </div>

        <!-- Timezone Configuration Section -->
        <div class="timezone-section">
            <h2>⏰ Timezone Configuration</h2>
            <div class="form-group">
                <label for="timezoneSelect">Select Timezone:</label>
                <select id="timezoneSelect" onchange="updateTimezone()">
                    <option value="0">UTC (GMT+0:00)</option>
                    <option value="3600">CET - Central European Time (GMT+1:00)</option>
                    <option value="7200">CEST - Central European Summer Time (GMT+2:00)</option>
                    <option value="-18000">EST - Eastern Standard Time (GMT-5:00)</option>
                    <option value="-14400">EDT - Eastern Daylight Time (GMT-4:00)</option>
                    <option value="-21600">CST - Central Standard Time (GMT-6:00)</option>
                    <option value="-18000">CDT - Central Daylight Time (GMT-5:00)</option>
                    <option value="-25200">MST - Mountain Standard Time (GMT-7:00)</option>
                    <option value="-21600">MDT - Mountain Daylight Time (GMT-6:00)</option>
                    <option value="-28800">PST - Pacific Standard Time (GMT-8:00)</option>
                    <option value="-25200">PDT - Pacific Daylight Time (GMT-7:00)</option>
                    <option value="28800">CST - China Standard Time (GMT+8:00)</option>
                    <option value="32400">JST - Japan Standard Time (GMT+9:00)</option>
                    <option value="36000">AEST - Australian Eastern Standard Time (GMT+10:00)</option>
                    <option value="39600">AEDT - Australian Eastern Daylight Time (GMT+11:00)</option>
                    <option value="43200">NZST - New Zealand Standard Time (GMT+12:00)</option>
                    <option value="46800">NZDT - New Zealand Daylight Time (GMT+13:00)</option>
                </select>
            </div>
            <div class="timezone-info" id="timezoneInfo">
                Current offset: <span id="timezoneOffsetDisplay">Loading...</span>
            </div>
        </div>

        <!-- MQTT Configuration Section -->
        <div class="timezone-section">
            <h2>📡 MQTT Configuration</h2>
            <div class="timezone-info">
                Connect to an MQTT broker to control this device remotely. Enable Home Assistant auto-discovery for seamless integration.
            </div>
            
            <div class="toggle-container" style="margin-top: 20px;">
                <span class="toggle-label">Enable MQTT</span>
                <label class="switch">
                    <input type="checkbox" id="mqttEnabled" onchange="updateMqttSettings()">
                    <span class="slider"></span>
                </label>
            </div>
            
            <div id="mqttSettingsPanel" style="display: none;">
                <div class="form-group">
                    <label for="mqttBroker">Broker Address:</label>
                    <input type="text" id="mqttBroker" placeholder="e.g., 192.168.1.100 or mqtt.example.com">
                </div>
                
                <div class="form-group">
                    <label for="mqttPort">Port:</label>
                    <input type="number" id="mqttPort" value="1883" min="1" max="65535">
                </div>
                
                <div class="form-group">
                    <label for="mqttUsername">Username (optional):</label>
                    <input type="text" id="mqttUsername" placeholder="Leave empty if not required">
                </div>
                
                <div class="form-group">
                    <label for="mqttPassword">Password (optional):</label>
                    <input type="password" id="mqttPassword" placeholder="Leave empty if not required">
                </div>
                
                <div class="form-group">
                    <label for="mqttBaseTopic">Base Topic:</label>
                    <input type="text" id="mqttBaseTopic" placeholder="e.g., home/audio-controller">
                </div>
                
                <div class="toggle-container">
                    <span class="toggle-label">Enable Home Assistant Auto-Discovery</span>
                    <label class="switch">
                        <input type="checkbox" id="mqttHADiscovery" onchange="updateMqttSettings()">
                        <span class="slider"></span>
                    </label>
                </div>
                
                <button class="check-btn" onclick="saveMqttSettings()" style="margin-top: 15px;">Save MQTT Settings</button>
                
                <div class="timezone-info" id="mqttStatus" style="margin-top: 15px;">
                    Status: <span id="mqttConnectionStatus">Loading...</span>
                </div>
            </div>
        </div>

        <!-- Export/Import Settings Section -->
        <div class="timezone-section">
            <h2>💾 Export/Import Settings</h2>
            <div class="timezone-info">
                Export all device settings to a JSON file for backup or transfer to another device.
            </div>
            <button class="check-btn" onclick="exportSettings()" style="margin-top: 15px;">Download Settings File</button>
            
            <div class="timezone-info" style="margin-top: 30px; border-top: 1px solid rgba(255, 255, 255, 0.2); padding-top: 20px;">
                Import settings from a previously exported JSON file. The device will reboot after import.
            </div>
            <input type="file" id="settingsFileInput" accept=".json" style="display: none;" onchange="handleFileSelect(event)">
            <div class="drop-zone" id="dropZone" onclick="document.getElementById('settingsFileInput').click()">
                <div class="drop-zone-icon">📁</div>
                <div class="drop-zone-text">Drag & Drop Settings File Here</div>
                <div class="drop-zone-hint">or click to browse</div>
            </div>
            <div class="ota-status" id="importStatus" style="margin-top: 15px;"></div>
        </div>

        <!-- Reboot Device Section -->
        <div class="reboot-section">
            <h2>🔄 Reboot Device</h2>
            <div class="reboot-info">
                <strong>INFO:</strong> This will restart the ESP32. All settings will be preserved. The device will reconnect to WiFi automatically.
            </div>
            <button class="reboot-btn" id="rebootBtn" 
                    onmousedown="startReboot()" 
                    onmouseup="cancelReboot()" 
                    onmouseleave="cancelReboot()"
                    ontouchstart="startReboot()" 
                    ontouchend="cancelReboot()">
                <div class="reboot-progress" id="rebootProgress"></div>
                <span class="reboot-btn-text" id="rebootBtnText">Hold for 2 Seconds to Reboot</span>
            </button>
        </div>

        <!-- Factory Reset Section -->
        <div class="factory-reset-section">
            <h2>⚠️ Factory Reset</h2>
            <div class="reset-warning">
                <strong>WARNING:</strong> This will erase all settings including WiFi credentials, timezone, auto-update preferences, and Smart Sensing configuration. The device will reboot and start in Access Point mode.
            </div>
            <button class="reset-btn" id="factoryResetBtn" 
                    onmousedown="startFactoryReset()" 
                    onmouseup="cancelFactoryReset()" 
                    onmouseleave="cancelFactoryReset()"
                    ontouchstart="startFactoryReset()" 
                    ontouchend="cancelFactoryReset()">
                <div class="reset-progress" id="resetProgress"></div>
                <span class="reset-btn-text" id="resetBtnText">Hold for 3 Seconds to Reset</span>
            </button>
        </div>

        <!-- Debugging Section -->
        <div class="debug-section">
            <h2>🔧 Debugging</h2>
            
            <!-- Hardware Stats Panel -->
            <div class="hardware-stats-section">
                <div class="hardware-stats-header">
                    <h3>📊 Hardware Utilization</h3>
                    <div class="refresh-selector">
                        <label for="statsRefreshRate">Refresh:</label>
                        <select id="statsRefreshRate" onchange="changeStatsRefreshRate()">
                            <option value="1000">1s</option>
                            <option value="3000">3s</option>
                            <option value="5000" selected>5s</option>
                            <option value="10000">10s</option>
                        </select>
                    </div>
                </div>
                
                <div class="hardware-stats-grid">
                    <!-- Memory Card -->
                    <div class="stats-card memory">
                        <h4>Memory</h4>
                        <div class="stat-row">
                            <span class="stat-label">Heap</span>
                            <span class="stat-value" id="statHeapUsed">--</span>
                        </div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill memory" id="statHeapBar" style="width: 0%;"></div>
                        </div>
                        <div class="stat-row" style="margin-top: 8px;">
                            <span class="stat-label">Min Free</span>
                            <span class="stat-value" id="statHeapMinFree">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Max Block</span>
                            <span class="stat-value" id="statHeapMaxBlock">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">PSRAM</span>
                            <span class="stat-value" id="statPsram">--</span>
                        </div>
                    </div>
                    
                    <!-- CPU Card -->
                    <div class="stats-card cpu">
                        <h4>CPU</h4>
                        <div class="stat-row">
                            <span class="stat-label">Model</span>
                            <span class="stat-value" id="statCpuModel">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Frequency</span>
                            <span class="stat-value" id="statCpuFreq">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Temperature</span>
                            <span class="stat-value" id="statCpuTemp">--</span>
                        </div>
                        <div class="stat-row" style="margin-top: 10px;">
                            <span class="stat-label"><span class="cpu-core-label">Core 0</span>Usage</span>
                            <span class="stat-value cpu-usage-value" id="statCpuCore0">--%</span>
                        </div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill cpu" id="statCpuCore0Bar" style="width: 0%;"></div>
                        </div>
                        <div class="stat-row" style="margin-top: 8px;">
                            <span class="stat-label"><span class="cpu-core-label">Core 1</span>Usage</span>
                            <span class="stat-value cpu-usage-value" id="statCpuCore1">--%</span>
                        </div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill cpu" id="statCpuCore1Bar" style="width: 0%;"></div>
                        </div>
                        <div class="stat-row" style="margin-top: 8px;">
                            <span class="stat-label">Total</span>
                            <span class="stat-value cpu-usage-value" id="statCpuTotal">--%</span>
                        </div>
                    </div>
                    
                    <!-- Storage Card -->
                    <div class="stats-card storage">
                        <h4>Storage</h4>
                        <div class="stat-row">
                            <span class="stat-label">Flash Size</span>
                            <span class="stat-value" id="statFlashSize">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Sketch Size</span>
                            <span class="stat-value" id="statSketchSize">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">OTA Free</span>
                            <span class="stat-value" id="statOtaFree">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">SPIFFS</span>
                            <span class="stat-value" id="statSpiffs">--</span>
                        </div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill storage" id="statSpiffsBar" style="width: 0%;"></div>
                        </div>
                    </div>
                    
                    <!-- WiFi Card -->
                    <div class="stats-card wifi">
                        <h4>WiFi</h4>
                        <div class="stat-row">
                            <span class="stat-label">RSSI</span>
                            <span class="stat-value">
                                <span id="statWifiRssi">--</span>
                                <span class="signal-quality" id="statWifiQuality">--</span>
                            </span>
                        </div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill wifi" id="statWifiBar" style="width: 0%;"></div>
                        </div>
                        <div class="stat-row" style="margin-top: 8px;">
                            <span class="stat-label">Channel</span>
                            <span class="stat-value" id="statWifiChannel">--</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">AP Clients</span>
                            <span class="stat-value" id="statApClients">--</span>
                        </div>
                    </div>
                </div>
                
                <!-- Uptime Bar -->
                <div class="uptime-bar">
                    <span class="uptime-label">UPTIME:</span>
                    <span id="statUptime">--</span>
                </div>
            </div>
            
            <!-- Performance History Section -->
            <div class="history-section">
                <div class="history-header" onclick="toggleHistorySection()">
                    <h3>Performance History</h3>
                    <span class="collapse-icon" id="historyCollapseIcon">▼</span>
                </div>
                <div class="history-content" id="historyContent">
                    <div class="history-controls">
                        <label>Time Range:</label>
                        <select id="historyTimeRange" onchange="changeHistoryRange()">
                            <option value="60">1 minute</option>
                            <option value="300" selected>5 minutes</option>
                            <option value="600">10 minutes</option>
                        </select>
                    </div>
                    <div class="graph-container">
                        <div class="graph-card">
                            <h4>CPU Usage</h4>
                            <canvas id="cpuGraph"></canvas>
                            <div class="graph-legend cpu-legend">
                                <span class="legend-item total">● Total</span>
                                <span class="legend-item core0">● Core 0</span>
                                <span class="legend-item core1">● Core 1</span>
                            </div>
                        </div>
                        <div class="graph-card">
                            <h4>Memory Usage</h4>
                            <canvas id="memoryGraph"></canvas>
                            <div class="graph-legend memory-legend">
                                <span class="legend-item heap">● Heap %</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            
            <div class="debug-info">
                Real-time terminal output from the ESP32. Messages are streamed via WebSocket when connected.
            </div>
            <div class="debug-console" id="debugConsole">
                <div class="log-entry">
                    <span class="log-timestamp">[--:--:--]</span>
                    <span class="log-message info">Waiting for connection...</span>
                </div>
            </div>
            <div class="debug-controls">
                <button class="debug-btn-clear" onclick="clearDebugConsole()">Clear</button>
                <button class="debug-btn-pause" id="debugPauseBtn" onclick="toggleDebugPause()">Pause</button>
                <label style="display: flex; align-items: center; gap: 5px;">
                    <input type="checkbox" id="debugAutoScroll" checked>
                    Auto-scroll
                </label>
                <div class="debug-status">
                    <div class="debug-status-indicator" id="debugStatusIndicator"></div>
                    <span id="debugStatusText">Receiving</span>
                </div>
            </div>
        </div>
    </div>

    <!-- Release Notes Modal -->
    <div class="modal" id="releaseNotesModal">
        <div class="modal-content">
            <div class="modal-header">
                <h2>Release Notes</h2>
                <button class="close-btn" onclick="closeReleaseNotes()">&times;</button>
            </div>
            <div class="release-notes-content" id="releaseNotesContent">
                Loading release notes...
            </div>
        </div>
    </div>

    <!-- AP Configuration Modal -->
    <div class="modal" id="apConfigModal">
        <div class="modal-content">
            <div class="modal-header">
                <h2>Access Point Configuration</h2>
                <button class="close-btn" onclick="closeAPConfig()">&times;</button>
            </div>
            <form onsubmit="submitAPConfig(event)">
                <div class="form-group">
                    <label for="apSSID">AP Network Name (SSID):</label>
                    <input type="text" id="apSSID" name="apSSID" placeholder="Enter AP SSID" required>
                </div>
                <div class="form-group">
                    <label for="apPassword">AP Password:</label>
                    <div class="password-wrapper">
                        <input type="password" id="apPassword" name="apPassword" placeholder="Enter AP password (min 8 characters)" minlength="8" required>
                        <button type="button" class="password-toggle" onclick="togglePasswordVisibility('apPassword', this)">👁️</button>
                    </div>
                    <div class="form-note">⚠️ Setting or changing password will cause device's AP clients to disconnect!</div>
                </div>
                <button type="submit" class="config-btn" style="width: 100%;">Save AP Configuration</button>
            </form>
        </div>
    </div>

    <script>
        let ws;
        let blinkingEnabled = true;
        let ledState = false;
        let autoUpdateEnabled = true;
        let currentLatestVersion = '';
        let currentFirmwareVersion = '';
        let currentTimezoneOffset = 0;
        let currentSensingMode = 'always_on';
        let timerDurationMinutes = 15;
        let voltageThresholdVolts = 1.0;
        let nightMode = false;
        let enableCertValidation = false;
        
        // Track which input fields are currently being edited
        let inputFocusState = {
            timerDuration: false,
            voltageThreshold: false
        };
        
        // Debug console state
        let debugPaused = false;
        let debugLogBuffer = [];
        const DEBUG_MAX_LINES = 500;
        
        // WebSocket reconnection with exponential backoff
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
        let maxHistoryPoints = 300; // 5 minutes at 1s intervals
        let historyCollapsed = false;

        function initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.hostname;
            ws = new WebSocket(`${wsProtocol}//${wsHost}:81`);

            ws.onopen = function() {
                console.log('WebSocket connected');
                updateConnectionStatus(true);
                // Reset reconnect delay on successful connection
                wsReconnectDelay = WS_MIN_RECONNECT_DELAY;
            };

            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                console.log('Received:', data);
                
                if (data.type === 'ledState') {
                    ledState = data.state;
                    updateLED();
                } else if (data.type === 'blinkingEnabled') {
                    blinkingEnabled = data.enabled;
                    updateButton();
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
                console.log('WebSocket disconnected, reconnecting in ' + (wsReconnectDelay / 1000) + 's...');
                updateConnectionStatus(false);
                setTimeout(initWebSocket, wsReconnectDelay);
                // Exponential backoff: double the delay for next attempt, up to max
                wsReconnectDelay = Math.min(wsReconnectDelay * 2, WS_MAX_RECONNECT_DELAY);
            };
        }

        function updateLED() {
            const led = document.getElementById('led');
            if (ledState) {
                led.classList.remove('off');
                led.classList.add('on');
            } else {
                led.classList.remove('on');
                led.classList.add('off');
            }
        }

        function updateButton() {
            const btn = document.getElementById('toggleBtn');
            const status = document.getElementById('status');
            
            if (blinkingEnabled) {
                btn.textContent = 'Stop Blinking';
                btn.classList.remove('stop');
                status.textContent = 'Blinking: ON';
            } else {
                btn.textContent = 'Start Blinking';
                btn.classList.add('stop');
                status.textContent = 'Blinking: OFF';
            }
        }

        function toggleBlinking() {
            blinkingEnabled = !blinkingEnabled;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'toggle',
                    enabled: blinkingEnabled
                }));
            }
            updateButton();
        }

        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('connectionStatus');
            if (connected) {
                statusEl.innerHTML = '<span class="connected">● Connected</span>';
            } else {
                statusEl.innerHTML = '<span class="disconnected">● Disconnected</span>';
            }
        }

        function updateWiFiStatus(data) {
            const statusEl = document.getElementById('wifiStatus');
            const apToggle = document.getElementById('apToggle');
            const autoUpdateToggle = document.getElementById('autoUpdateToggle');
            
            const macAddress = data.mac || 'Unknown';
            const rssi = data.rssi !== undefined ? `${data.rssi} dBm` : 'N/A';
            const firmwareVersion = data.firmwareVersion || 'Unknown';
            const manufacturer = data.manufacturer || 'Unknown';
            const model = data.model || 'Unknown';
            const serialNumber = data.serialNumber || 'Unknown';
            
            // Build device info string
            const deviceInfoStr = `<br><small style="opacity: 0.8;">Manufacturer: ${manufacturer}<br>Model: ${model}<br>Serial: ${serialNumber}</small>`;
            
            if (data.mode === 'ap') {
                statusEl.className = 'wifi-status ap';
                statusEl.innerHTML = `● Access Point Mode<br>SSID: ${data.apSSID || 'ESP32-LED-Setup'}<br>IP: ${data.ip || '192.168.4.1'}<br>MAC: ${macAddress}<br>Firmware: ${firmwareVersion}${deviceInfoStr}`;
                apToggle.checked = true;
            } else if (data.connected) {
                statusEl.className = 'wifi-status connected';
                statusEl.innerHTML = `● Connected to: ${data.ssid || 'Unknown'}<br>IP: ${data.ip || 'Unknown'}<br>MAC: ${macAddress}<br>Firmware: ${firmwareVersion}<br>Signal: ${rssi}${deviceInfoStr}`;
                apToggle.checked = data.apEnabled || false;
            } else {
                statusEl.className = 'wifi-status';
                statusEl.innerHTML = `● Not Connected<br>SSID: ${data.ssid || 'None'}<br>MAC: ${macAddress}<br>Firmware: ${firmwareVersion}${deviceInfoStr}`;
                apToggle.checked = data.apEnabled || false;
            }

            if (typeof data.autoUpdateEnabled !== 'undefined') {
                autoUpdateEnabled = !!data.autoUpdateEnabled;
                autoUpdateToggle.checked = autoUpdateEnabled;
            }
            
            // Update timezone information if available
            if (typeof data.timezoneOffset !== 'undefined') {
                currentTimezoneOffset = data.timezoneOffset;
                const timezoneSelect = document.getElementById('timezoneSelect');
                timezoneSelect.value = data.timezoneOffset.toString();
                updateTimezoneDisplay(data.timezoneOffset);
            }
            
            // Update night mode if available
            if (typeof data.nightMode !== 'undefined') {
                nightMode = !!data.nightMode;
                applyTheme(nightMode);
            }
            
            // Update certificate validation setting if available
            if (typeof data.enableCertValidation !== 'undefined') {
                enableCertValidation = !!data.enableCertValidation;
                const certValidationToggle = document.getElementById('certValidationToggle');
                if (certValidationToggle) {
                    certValidationToggle.checked = enableCertValidation;
                }
            }
            
            // Update version information if available
            if (data.firmwareVersion) {
                currentFirmwareVersion = data.firmwareVersion;
                document.getElementById('currentVersion').textContent = data.firmwareVersion;
            }
            
            if (data.latestVersion && data.latestVersion !== 'Checking...' && data.latestVersion !== 'Unknown') {
                currentLatestVersion = data.latestVersion;
                const latestVersionContainer = document.getElementById('latestVersionContainer');
                const latestVersionSpan = document.getElementById('latestVersion');
                const latestVersionNotesBtn = document.getElementById('latestVersionNotesBtn');
                const updateBtn = document.getElementById('updateBtn');
                const otaStatus = document.getElementById('otaStatus');
                
                latestVersionContainer.style.display = 'block';
                latestVersionSpan.textContent = data.latestVersion;
                
                if (data.updateAvailable) {
                    latestVersionSpan.className = 'update-available';
                    latestVersionNotesBtn.style.display = 'inline-block';
                    updateBtn.style.display = 'block';
                    otaStatus.className = 'ota-status show update-available';
                    otaStatus.textContent = 'Update available! You can view the release notes or click update to install.';
                } else {
                    latestVersionSpan.className = 'up-to-date';
                    latestVersionNotesBtn.style.display = 'none';
                    updateBtn.style.display = 'none';
                    otaStatus.className = 'ota-status show success';
                    otaStatus.textContent = 'You have the latest version installed.';
                }
            }
        }

        function toggleAP() {
            const enabled = document.getElementById('apToggle').checked;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'toggleAP',
                    enabled: enabled
                }));
            } else {
                // Fallback to HTTP if WebSocket not available
                fetch('/api/toggleap', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ enabled: enabled })
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        setTimeout(() => location.reload(), 2000);
                    }
                });
            }
        }

        function toggleAutoUpdate() {
            const enabled = document.getElementById('autoUpdateToggle').checked;
            autoUpdateEnabled = enabled;

            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ autoUpdateEnabled: enabled })
            })
            .then(response => response.json())
            .then(data => {
                // Optionally handle response or errors here
                if (!data.success) {
                    alert('Failed to update settings: ' + (data.message || 'Unknown error'));
                    document.getElementById('autoUpdateToggle').checked = !enabled;
                }
            })
            .catch(error => {
                alert('Error updating settings: ' + error);
                document.getElementById('autoUpdateToggle').checked = !enabled;
            });
        }

        function toggleTheme() {
            nightMode = !nightMode;
            applyTheme(nightMode);
            
            // Save to backend
            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ nightMode: nightMode })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    console.error('Failed to save theme preference');
                }
            })
            .catch(error => {
                console.error('Error saving theme preference:', error);
            });
        }

        function applyTheme(isNightMode) {
            const body = document.body;
            const themeToggleBtn = document.getElementById('themeToggle');
            
            if (isNightMode) {
                body.classList.add('night-mode');
                themeToggleBtn.textContent = '☀️';
                themeToggleBtn.title = 'Switch to Day Mode';
            } else {
                body.classList.remove('night-mode');
                themeToggleBtn.textContent = '🌙';
                themeToggleBtn.title = 'Switch to Night Mode';
            }
        }

        function toggleCertValidation() {
            const enabled = document.getElementById('certValidationToggle').checked;
            enableCertValidation = enabled;
            
            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ enableCertValidation: enabled })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    alert('Failed to update certificate validation setting: ' + (data.message || 'Unknown error'));
                    document.getElementById('certValidationToggle').checked = !enabled;
                } else {
                    const statusMsg = enabled ? 'SSL Certificate Validation ENABLED' : 'SSL Certificate Validation DISABLED';
                    console.log(statusMsg);
                }
            })
            .catch(error => {
                alert('Error updating certificate validation setting: ' + error);
                document.getElementById('certValidationToggle').checked = !enabled;
            });
        }

        // Note: Certificate editor functions removed - now using Mozilla certificate bundle

        function updateTimezone() {
            const select = document.getElementById('timezoneSelect');
            const offset = parseInt(select.value);
            currentTimezoneOffset = offset;

            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ timezoneOffset: offset })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    updateTimezoneDisplay(offset);
                    alert('Timezone updated successfully! Time will be re-synchronized.');
                } else {
                    alert('Failed to update timezone: ' + (data.message || 'Unknown error'));
                    // Revert selection
                    select.value = currentTimezoneOffset.toString();
                }
            })
            .catch(error => {
                alert('Error updating timezone: ' + error);
                // Revert selection
                select.value = currentTimezoneOffset.toString();
            });
        }

        function updateTimezoneDisplay(offset) {
            const hours = offset / 3600;
            const sign = hours >= 0 ? '+' : '';
            const displayText = `GMT${sign}${hours.toFixed(1)} (${offset} seconds)`;
            document.getElementById('timezoneOffsetDisplay').textContent = displayText;
        }

        // ===== MQTT Configuration Functions =====
        
        function loadMqttSettings() {
            fetch('/api/mqtt')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('mqttEnabled').checked = data.enabled;
                        document.getElementById('mqttBroker').value = data.broker || '';
                        document.getElementById('mqttPort').value = data.port || 1883;
                        document.getElementById('mqttUsername').value = data.username || '';
                        document.getElementById('mqttBaseTopic').value = data.baseTopic || 'esp32-audio';
                        document.getElementById('mqttHADiscovery').checked = data.haDiscovery;
                        
                        // Show/hide settings panel based on enabled state
                        document.getElementById('mqttSettingsPanel').style.display = data.enabled ? 'block' : 'none';
                        
                        // Update connection status
                        updateMqttConnectionStatus(data.connected, data.deviceId);
                    }
                })
                .catch(error => {
                    console.error('Error loading MQTT settings:', error);
                    document.getElementById('mqttConnectionStatus').textContent = 'Error loading settings';
                });
        }
        
        function updateMqttSettings() {
            const enabled = document.getElementById('mqttEnabled').checked;
            document.getElementById('mqttSettingsPanel').style.display = enabled ? 'block' : 'none';
        }
        
        function updateMqttConnectionStatus(connected, deviceId) {
            const statusEl = document.getElementById('mqttConnectionStatus');
            if (connected) {
                statusEl.innerHTML = '<span style="color: #4CAF50;">✅ Connected</span>';
                if (deviceId) {
                    statusEl.innerHTML += ' (Device ID: ' + deviceId + ')';
                }
            } else {
                const enabled = document.getElementById('mqttEnabled').checked;
                if (enabled) {
                    statusEl.innerHTML = '<span style="color: #ff9800;">⚠️ Disconnected</span>';
                } else {
                    statusEl.innerHTML = '<span style="color: #888;">MQTT disabled</span>';
                }
            }
        }
        
        function saveMqttSettings() {
            const enabled = document.getElementById('mqttEnabled').checked;
            const broker = document.getElementById('mqttBroker').value.trim();
            const port = parseInt(document.getElementById('mqttPort').value) || 1883;
            const username = document.getElementById('mqttUsername').value.trim();
            const password = document.getElementById('mqttPassword').value;
            const baseTopic = document.getElementById('mqttBaseTopic').value.trim() || 'esp32-audio';
            const haDiscovery = document.getElementById('mqttHADiscovery').checked;
            
            // Validate broker if enabled
            if (enabled && !broker) {
                alert('Please enter a broker address');
                document.getElementById('mqttBroker').focus();
                return;
            }
            
            const statusEl = document.getElementById('mqttConnectionStatus');
            statusEl.innerHTML = '<span style="color: #2196F3;">⏳ Saving...</span>';
            
            const settings = {
                enabled: enabled,
                broker: broker,
                port: port,
                username: username,
                baseTopic: baseTopic,
                haDiscovery: haDiscovery
            };
            
            // Only include password if it was entered
            if (password) {
                settings.password = password;
            }
            
            fetch('/api/mqtt', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settings)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    // Clear password field after saving
                    document.getElementById('mqttPassword').value = '';
                    
                    // Update status
                    updateMqttConnectionStatus(data.connected);
                    
                    // Show success message briefly
                    statusEl.innerHTML = '<span style="color: #4CAF50;">✅ Settings saved!</span>';
                    
                    // Reload status after a short delay
                    setTimeout(loadMqttSettings, 2000);
                } else {
                    statusEl.innerHTML = '<span style="color: #f44336;">❌ ' + (data.message || 'Failed to save') + '</span>';
                }
            })
            .catch(error => {
                console.error('Error saving MQTT settings:', error);
                statusEl.innerHTML = '<span style="color: #f44336;">❌ Error saving settings</span>';
            });
        }

        function exportSettings() {
            // Create a download link and trigger it
            const link = document.createElement('a');
            link.href = '/api/settings/export';
            link.download = 'device-settings.json';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }

        function handleFileSelect(event) {
            const file = event.target.files[0];
            if (!file) {
                return;
            }
            
            const statusEl = document.getElementById('importStatus');
            const dropZone = document.getElementById('dropZone');
            
            // Reset drop zone appearance
            dropZone.classList.remove('drag-over');
            
            // Validate file type
            if (!file.name.endsWith('.json')) {
                statusEl.className = 'ota-status show error';
                statusEl.textContent = '❌ Error: Please select a JSON file';
                return;
            }
            
            // Show selected file name
            statusEl.className = 'ota-status show';
            statusEl.textContent = '📄 Selected: ' + file.name;
            
            // Read file
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const settings = JSON.parse(e.target.result);
                    
                    // Validate it's a settings file
                    if (!settings.exportInfo || !settings.settings) {
                        statusEl.className = 'ota-status show error';
                        statusEl.textContent = 'Error: Invalid settings file format';
                        return;
                    }
                    
                    // Confirm before importing
                    const exportTime = settings.exportInfo.timestamp || 'unknown';
                    const confirmMsg = `Import settings from backup?\n\nExport Date: ${exportTime}\n\nThis will:\n- Apply all saved settings\n- Reboot the device in 3 seconds\n- Cannot be cancelled once started\n\nContinue?`;
                    
                    if (!confirm(confirmMsg)) {
                        statusEl.className = 'ota-status show';
                        statusEl.textContent = 'Import cancelled';
                        event.target.value = ''; // Reset file input
                        return;
                    }
                    
                    // Import settings
                    importSettings(e.target.result);
                    
                } catch (error) {
                    statusEl.className = 'ota-status show error';
                    statusEl.textContent = 'Error: Failed to parse JSON file - ' + error.message;
                }
            };
            
            reader.onerror = function() {
                statusEl.className = 'ota-status show error';
                statusEl.textContent = 'Error: Failed to read file';
            };
            
            reader.readAsText(file);
        }

        function importSettings(jsonData) {
            const statusEl = document.getElementById('importStatus');
            
            statusEl.className = 'ota-status show updating';
            statusEl.textContent = 'Importing settings...';
            
            fetch('/api/settings/import', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: jsonData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    statusEl.className = 'ota-status show success';
                    statusEl.textContent = '✅ Settings imported successfully!';
                    
                    // Show countdown
                    let countdown = 3;
                    const countdownInterval = setInterval(() => {
                        statusEl.textContent = `✅ Settings imported successfully! Rebooting in ${countdown} seconds...`;
                        countdown--;
                        
                        if (countdown < 0) {
                            clearInterval(countdownInterval);
                            statusEl.textContent = '🔄 Device is rebooting...';
                            
                            // Try to reconnect after reboot
                            setTimeout(() => {
                                statusEl.textContent = '⏳ Waiting for device to come back online...';
                                // Attempt to reload page after device reboots
                                setTimeout(() => {
                                    window.location.reload();
                                }, 5000);
                            }, 3000);
                        }
                    }, 1000);
                } else {
                    statusEl.className = 'ota-status show error';
                    statusEl.textContent = '❌ Import failed: ' + (data.message || 'Unknown error');
                }
            })
            .catch(error => {
                // If fetch fails, device might already be rebooting
                statusEl.className = 'ota-status show success';
                statusEl.textContent = '✅ Import started. Device is rebooting...';
                
                setTimeout(() => {
                    statusEl.textContent = '⏳ Waiting for device to come back online...';
                    setTimeout(() => {
                        window.location.reload();
                    }, 5000);
                }, 3000);
            });
            
            // Reset file input
            document.getElementById('settingsFileInput').value = '';
        }

        let rebootHoldTimer = null;
        let rebootStartTime = 0;
        let rebootAnimationFrame = null;
        let resetHoldTimer = null;
        let resetStartTime = 0;
        let resetAnimationFrame = null;

        function startReboot() {
            const btn = document.getElementById('rebootBtn');
            const progress = document.getElementById('rebootProgress');
            const text = document.getElementById('rebootBtnText');
            
            rebootStartTime = Date.now();
            btn.classList.add('holding');
            text.textContent = 'Keep Holding...';
            
            // Animate progress bar
            function updateProgress() {
                const elapsed = Date.now() - rebootStartTime;
                const percentage = Math.min((elapsed / 2000) * 100, 100);
                progress.style.width = percentage + '%';
                
                if (elapsed >= 2000) {
                    // 2 seconds elapsed - perform reboot
                    performReboot();
                } else {
                    rebootAnimationFrame = requestAnimationFrame(updateProgress);
                }
            }
            
            rebootAnimationFrame = requestAnimationFrame(updateProgress);
        }

        function cancelReboot() {
            const btn = document.getElementById('rebootBtn');
            const progress = document.getElementById('rebootProgress');
            const text = document.getElementById('rebootBtnText');
            
            if (rebootAnimationFrame) {
                cancelAnimationFrame(rebootAnimationFrame);
                rebootAnimationFrame = null;
            }
            
            btn.classList.remove('holding');
            progress.style.width = '0%';
            text.textContent = 'Hold for 2 Seconds to Reboot';
        }

        function performReboot() {
            const btn = document.getElementById('rebootBtn');
            const text = document.getElementById('rebootBtnText');
            
            btn.disabled = true;
            text.textContent = 'Rebooting...';
            
            fetch('/api/reboot', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    text.textContent = 'Rebooting Device...';
                    // Show message for 2 seconds
                    setTimeout(() => {
                        alert('Device is rebooting. The page will reconnect automatically.');
                        // Reload page after device reboots
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    }, 1000);
                } else {
                    alert('Reboot failed: ' + (data.message || 'Unknown error'));
                    btn.disabled = false;
                    cancelReboot();
                }
            })
            .catch(error => {
                alert('Error performing reboot: ' + error);
                btn.disabled = false;
                cancelReboot();
            });
        }

        function startFactoryReset() {
            const btn = document.getElementById('factoryResetBtn');
            const progress = document.getElementById('resetProgress');
            const text = document.getElementById('resetBtnText');
            
            resetStartTime = Date.now();
            btn.classList.add('holding');
            text.textContent = 'Keep Holding...';
            
            // Animate progress bar
            function updateProgress() {
                const elapsed = Date.now() - resetStartTime;
                const percentage = Math.min((elapsed / 3000) * 100, 100);
                progress.style.width = percentage + '%';
                
                if (elapsed >= 3000) {
                    // 3 seconds elapsed - perform reset
                    performFactoryReset();
                } else {
                    resetAnimationFrame = requestAnimationFrame(updateProgress);
                }
            }
            
            resetAnimationFrame = requestAnimationFrame(updateProgress);
        }

        function cancelFactoryReset() {
            const btn = document.getElementById('factoryResetBtn');
            const progress = document.getElementById('resetProgress');
            const text = document.getElementById('resetBtnText');
            
            if (resetAnimationFrame) {
                cancelAnimationFrame(resetAnimationFrame);
                resetAnimationFrame = null;
            }
            
            btn.classList.remove('holding');
            progress.style.width = '0%';
            text.textContent = 'Hold for 3 Seconds to Reset';
        }

        function performFactoryReset() {
            const btn = document.getElementById('factoryResetBtn');
            const text = document.getElementById('resetBtnText');
            
            btn.disabled = true;
            text.textContent = 'Resetting...';
            
            fetch('/api/factoryreset', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    text.textContent = 'Reset Complete! Rebooting...';
                    // Show message for 2 seconds
                    setTimeout(() => {
                        alert('Factory reset complete. The device will reboot and start in Access Point mode. You will need to reconnect.');
                        // Optionally reload page after device reboots
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    }, 2000);
                } else {
                    alert('Factory reset failed: ' + (data.message || 'Unknown error'));
                    btn.disabled = false;
                    cancelFactoryReset();
                }
            })
            .catch(error => {
                alert('Error performing factory reset: ' + error);
                btn.disabled = false;
                cancelFactoryReset();
            });
        }

        function handlePhysicalResetProgress(data) {
            const btn = document.getElementById('factoryResetBtn');
            const progress = document.getElementById('resetProgress');
            const text = document.getElementById('resetBtnText');
            
            if (data.resetTriggered) {
                // Physical button triggered reset
                btn.disabled = true;
                btn.classList.add('holding');
                progress.style.width = '100%';
                text.textContent = 'Physical Reset Triggered! Rebooting...';
                
                // Show notification
                setTimeout(() => {
                    alert('Factory reset triggered by physical button. The device will reboot and start in Access Point mode.');
                    setTimeout(() => {
                        window.location.reload();
                    }, 5000);
                }, 1000);
                
            } else if (data.secondsHeld > 0) {
                // Physical button is being held - show progress
                btn.classList.add('holding');
                progress.style.width = data.progress + '%';
                text.textContent = `Physical Button: ${data.secondsHeld}/${data.secondsRequired}s`;
                
            } else {
                // Physical button released or cancelled
                btn.classList.remove('holding');
                progress.style.width = '0%';
                text.textContent = 'Hold for 3 Seconds to Reset';
                btn.disabled = false;
            }
        }

        function handlePhysicalRebootProgress(data) {
            const btn = document.getElementById('rebootBtn');
            const progress = document.getElementById('rebootProgress');
            const text = document.getElementById('rebootBtnText');
            
            if (data.rebootTriggered) {
                // Physical button triggered reboot
                btn.disabled = true;
                btn.classList.add('holding');
                progress.style.width = '100%';
                text.textContent = 'Physical Reboot Triggered! Rebooting...';
                
                // Show notification
                setTimeout(() => {
                    alert('Reboot triggered by physical button. The device will restart shortly.');
                    setTimeout(() => {
                        window.location.reload();
                    }, 5000);
                }, 1000);
                
            } else if (data.secondsHeld > 0) {
                // Physical button is being held - show progress
                btn.classList.add('holding');
                progress.style.width = data.progress + '%';
                text.textContent = `Physical Button: ${data.secondsHeld}/${data.secondsRequired}s`;
                
            } else {
                // Physical button released or cancelled
                btn.classList.remove('holding');
                progress.style.width = '0%';
                text.textContent = 'Hold for 2 Seconds to Reboot';
                btn.disabled = false;
            }
        }

        function submitWiFiConfig(event) {
            event.preventDefault();
            const ssid = document.getElementById('wifiSSID').value;
            const password = document.getElementById('wifiPassword').value;
            
            if (!ssid || !password) {
                alert('Please enter both SSID and password');
                return;
            }
            
            fetch('/api/wificonfig', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    ssid: ssid,
                    password: password
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('WiFi configuration saved! Connecting...');
                    setTimeout(() => location.reload(), 3000);
                } else {
                    alert('Error: ' + (data.message || 'Failed to save configuration'));
                }
            })
            .catch(error => {
                alert('Error: ' + error);
            });
        }

        function checkForUpdate() {
            const statusEl = document.getElementById('otaStatus');
            const checkBtn = document.querySelector('.check-btn');
            const latestVersionContainer = document.getElementById('latestVersionContainer');
            const updateBtn = document.getElementById('updateBtn');
            const latestVersionNotesBtn = document.getElementById('latestVersionNotesBtn');
            
            statusEl.className = 'ota-status show updating';
            statusEl.textContent = 'Checking for updates...';
            checkBtn.disabled = true;
            latestVersionContainer.style.display = 'none';
            updateBtn.style.display = 'none';
            latestVersionNotesBtn.style.display = 'none';
            
            fetch('/api/checkupdate')
                .then(response => response.json())
                .then(data => {
                    checkBtn.disabled = false;
                    
                    if (data.success) {
                        const currentVersion = data.currentVersion || 'Unknown';
                        const latestVersion = data.latestVersion || 'Unknown';
                        currentLatestVersion = latestVersion;  // Store for release notes
                        currentFirmwareVersion = currentVersion;  // Store current version
                        
                        document.getElementById('currentVersion').textContent = currentVersion;
                        latestVersionContainer.style.display = 'block';
                        document.getElementById('latestVersion').textContent = latestVersion;
                        
                        if (data.updateAvailable) {
                            document.getElementById('latestVersion').className = 'update-available';
                            updateBtn.style.display = 'block';
                            latestVersionNotesBtn.style.display = 'inline-block';
                            statusEl.className = 'ota-status show update-available';
                            statusEl.textContent = 'Update available! View release notes or click update to proceed.';
                        } else {
                            document.getElementById('latestVersion').className = 'up-to-date';
                            statusEl.className = 'ota-status show success';
                            statusEl.textContent = 'You have the latest version installed.';
                        }
                    } else {
                        statusEl.className = 'ota-status show error';
                        statusEl.textContent = 'Error: ' + (data.message || 'Failed to check for updates');
                    }
                })
                .catch(error => {
                    checkBtn.disabled = false;
                    statusEl.className = 'ota-status show error';
                    statusEl.textContent = 'Error: ' + error;
                });
        }

        function startOTAUpdate() {
            const statusEl = document.getElementById('otaStatus');
            const updateBtn = document.getElementById('updateBtn');
            const checkBtn = document.querySelector('.check-btn');
            
            if (!confirm('This will download and install the new firmware. The device will restart automatically. Continue?')) {
                return;
            }
            
            statusEl.className = 'ota-status show updating';
            statusEl.textContent = 'Starting OTA update... This may take a few minutes.';
            updateBtn.disabled = true;
            checkBtn.disabled = true;
            
            fetch('/api/startupdate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    statusEl.className = 'ota-status show updating';
                    statusEl.textContent = 'Update started. Downloading firmware... The device will restart when complete.';
                    
                    // Poll for update status
                    let pollCount = 0;
                    const pollInterval = setInterval(() => {
                        pollCount++;
                        fetch('/api/updatestatus')
                            .then(response => response.json())
                            .then(statusData => {
                                if (statusData.status === 'complete') {
                                    clearInterval(pollInterval);
                                    statusEl.className = 'ota-status show success';
                                    statusEl.textContent = 'Update complete! Device will restart in a few seconds...';
                                } else if (statusData.status === 'error') {
                                    clearInterval(pollInterval);
                                    statusEl.className = 'ota-status show error';
                                    statusEl.textContent = 'Update failed: ' + (statusData.message || 'Unknown error');
                                    updateBtn.disabled = false;
                                    checkBtn.disabled = false;
                                } else if (statusData.status === 'downloading') {
                                    statusEl.textContent = 'Downloading: ' + (statusData.progress || '0') + '%';
                                }
                            })
                            .catch(() => {
                                // Server might have restarted
                                if (pollCount > 30) {
                                    clearInterval(pollInterval);
                                    statusEl.className = 'ota-status show success';
                                    statusEl.textContent = 'Update may have completed. Please refresh the page.';
                                }
                            });
                    }, 2000);
                } else {
                    statusEl.className = 'ota-status show error';
                    statusEl.textContent = 'Error: ' + (data.message || 'Failed to start update');
                    updateBtn.disabled = false;
                    checkBtn.disabled = false;
                }
            })
            .catch(error => {
                statusEl.className = 'ota-status show error';
                statusEl.textContent = 'Error: ' + error;
                updateBtn.disabled = false;
                checkBtn.disabled = false;
            });
        }

        function cancelAutoUpdate() {
            autoUpdateEnabled = false;
            
            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ autoUpdateEnabled: false })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    document.getElementById('autoUpdateToggle').checked = false;
                    document.getElementById('cancelUpdateBtn').style.display = 'none';
                    const otaStatus = document.getElementById('otaStatus');
                    otaStatus.className = 'ota-status show update-available';
                    otaStatus.textContent = 'Auto-update cancelled. Update available for manual installation.';
                }
            })
            .catch(error => {
                alert('Error cancelling auto-update: ' + error);
            });
        }

        function showUpdateSuccessNotification(data) {
            // Create and show a success notification banner
            const notification = document.createElement('div');
            notification.id = 'updateSuccessNotification';
            notification.style.cssText = `
                position: fixed;
                top: 20px;
                left: 50%;
                transform: translateX(-50%);
                background: linear-gradient(135deg, #28a745 0%, #20c997 100%);
                color: white;
                padding: 20px 30px;
                border-radius: 12px;
                box-shadow: 0 8px 32px rgba(40, 167, 69, 0.4);
                z-index: 10000;
                text-align: center;
                max-width: 90%;
                animation: slideDown 0.5s ease-out;
            `;
            notification.innerHTML = `
                <div style="font-size: 24px; margin-bottom: 8px;">✅ Firmware Updated Successfully!</div>
                <div style="font-size: 14px; opacity: 0.9;">
                    Updated from <strong>${data.previousVersion}</strong> to <strong>${data.currentVersion}</strong>
                </div>
                <div style="font-size: 12px; margin-top: 10px; opacity: 0.7;">
                    Click to dismiss or wait 10 seconds
                </div>
            `;
            
            // Add animation keyframes if not already present
            if (!document.getElementById('updateNotificationStyles')) {
                const style = document.createElement('style');
                style.id = 'updateNotificationStyles';
                style.textContent = `
                    @keyframes slideDown {
                        from { transform: translateX(-50%) translateY(-100px); opacity: 0; }
                        to { transform: translateX(-50%) translateY(0); opacity: 1; }
                    }
                    @keyframes fadeOut {
                        from { opacity: 1; }
                        to { opacity: 0; }
                    }
                `;
                document.head.appendChild(style);
            }
            
            document.body.appendChild(notification);
            
            // Auto-dismiss after 10 seconds
            const dismissTimeout = setTimeout(() => {
                dismissNotification(notification);
            }, 10000);
            
            // Click to dismiss
            notification.onclick = () => {
                clearTimeout(dismissTimeout);
                dismissNotification(notification);
            };
            
            function dismissNotification(el) {
                el.style.animation = 'fadeOut 0.3s ease-out forwards';
                setTimeout(() => el.remove(), 300);
            }
            
            // Also update the OTA status section
            const otaStatus = document.getElementById('otaStatus');
            if (otaStatus) {
                otaStatus.className = 'ota-status show success';
                otaStatus.textContent = `Firmware updated from ${data.previousVersion} to ${data.currentVersion}`;
            }
            
            console.log('Firmware update notification:', data.message);
        }

        function handleUpdateStatus(data) {
            const updateBtn = document.getElementById('updateBtn');
            const cancelBtn = document.getElementById('cancelUpdateBtn');
            const otaStatus = document.getElementById('otaStatus');
            const latestVersionContainer = document.getElementById('latestVersionContainer');
            const progressContainer = document.getElementById('progressContainer');
            const progressBar = document.getElementById('progressBar');
            const progressStatus = document.getElementById('progressStatus');
            
            // Handle progress bar for downloading/preparing/complete states
            if (data.status === 'downloading' || data.status === 'preparing' || data.status === 'complete') {
                progressContainer.classList.add('show');
                progressBar.style.width = data.progress + '%';
                progressBar.textContent = data.progress + '%';
                
                if (data.message) {
                    progressStatus.textContent = data.message;
                }
                
                if (data.bytesDownloaded && data.totalBytes) {
                    const downloadedKB = (data.bytesDownloaded / 1024).toFixed(1);
                    const totalKB = (data.totalBytes / 1024).toFixed(1);
                    progressStatus.textContent = `${data.message} (${downloadedKB} / ${totalKB} KB)`;
                }
            } else {
                progressContainer.classList.remove('show');
            }
            
            if (data.status === 'complete') {
                otaStatus.className = 'ota-status show success';
                otaStatus.textContent = 'Update complete! Device rebooting...';
            } else if (data.status === 'error') {
                otaStatus.className = 'ota-status show error';
                otaStatus.textContent = 'Update failed: ' + (data.message || 'Unknown error');
                progressContainer.classList.remove('show');
            }
            
            if (data.updateAvailable) {
                document.getElementById('latestVersion').textContent = data.latestVersion;
                currentLatestVersion = data.latestVersion;
                latestVersionContainer.style.display = 'block';
                
                if (data.countdownSeconds > 0) {
                    // Show countdown
                    updateBtn.style.display = 'block';
                    updateBtn.textContent = 'Update Now (Auto-updating in ' + data.countdownSeconds + 's)';
                    cancelBtn.style.display = 'block';
                    otaStatus.className = 'ota-status show updating';
                    otaStatus.textContent = 'New firmware detected. Auto-update in ' + data.countdownSeconds + ' seconds...';
                } else if (data.autoUpdateEnabled) {
                    // Countdown ended, update imminent
                    updateBtn.style.display = 'block';
                    updateBtn.textContent = 'Update Now';
                    cancelBtn.style.display = 'none';
                    otaStatus.className = 'ota-status show updating';
                    otaStatus.textContent = 'Update starting...';
                } else {
                    // Auto-update disabled, show manual button
                    updateBtn.style.display = 'block';
                    updateBtn.textContent = 'Update to Latest Version';
                    cancelBtn.style.display = 'none';
                    otaStatus.className = 'ota-status show update-available';
                    otaStatus.textContent = 'Update available! Click to install.';
                }
            } else {
                updateBtn.style.display = 'none';
                cancelBtn.style.display = 'none';
                if (otaStatus.className.includes('show')) {
                    otaStatus.className = 'ota-status';
                }
            }
        }

        function showReleaseNotes() {
            const modal = document.getElementById('releaseNotesModal');
            const content = document.getElementById('releaseNotesContent');
            const header = document.querySelector('.modal-header h2');
            
            header.textContent = 'Release Notes - Version ' + currentLatestVersion;
            modal.classList.add('show');
            content.textContent = 'Loading release notes...';
            
            fetch(`/api/releasenotes?version=${encodeURIComponent(currentLatestVersion)}`)
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        content.textContent = data.notes;
                    } else {
                        content.textContent = data.notes || data.message || 'Failed to load release notes';
                    }
                })
                .catch(error => {
                    content.textContent = 'Error loading release notes: ' + error;
                });
        }

        function showCurrentVersionNotes() {
            const modal = document.getElementById('releaseNotesModal');
            const content = document.getElementById('releaseNotesContent');
            const header = document.querySelector('.modal-header h2');
            
            header.textContent = 'Release Notes - Version ' + currentFirmwareVersion;
            modal.classList.add('show');
            content.textContent = 'Loading release notes...';
            
            fetch(`/api/releasenotes?version=${encodeURIComponent(currentFirmwareVersion)}`)
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        content.textContent = data.notes;
                    } else {
                        content.textContent = data.notes || data.message || 'Failed to load release notes';
                    }
                })
                .catch(error => {
                    content.textContent = 'Error loading release notes: ' + error;
                });
        }

        function closeReleaseNotes() {
            const modal = document.getElementById('releaseNotesModal');
            modal.classList.remove('show');
        }

        // Password visibility toggle
        function togglePasswordVisibility(inputId, button) {
            const input = document.getElementById(inputId);
            if (input.type === 'password') {
                input.type = 'text';
                button.textContent = '🙈';  // Hide icon
            } else {
                input.type = 'password';
                button.textContent = '👁️';  // Show icon
            }
        }

        // AP Configuration Modal functions
        let currentAPSSID = '';
        let currentAPPassword = '';

        function showAPConfig() {
            const modal = document.getElementById('apConfigModal');
            const ssidInput = document.getElementById('apSSID');
            const passwordInput = document.getElementById('apPassword');
            
            // Fetch current AP settings from server
            fetch('/api/wifistatus')
                .then(response => response.json())
                .then(data => {
                    // Prefill with current AP SSID
                    if (data.apSSID) {
                        currentAPSSID = data.apSSID;
                        ssidInput.value = data.apSSID;
                    }
                    // Password cannot be retrieved for security - use placeholder
                    passwordInput.value = '';
                    passwordInput.placeholder = '********';
                    currentAPPassword = '';
                })
                .catch(error => {
                    console.error('Error fetching AP config:', error);
                });
            
            modal.classList.add('show');
        }

        function closeAPConfig() {
            const modal = document.getElementById('apConfigModal');
            modal.classList.remove('show');
        }

        function submitAPConfig(event) {
            event.preventDefault();
            
            const ssid = document.getElementById('apSSID').value;
            const password = document.getElementById('apPassword').value;
            
            // Password validation: if provided, must be at least 8 characters
            // Empty password means keep existing
            if (password.length > 0 && password.length < 8) {
                alert('Password must be at least 8 characters long (or leave empty to keep current password)');
                return;
            }
            
            // Build request body - only include password if changed
            const requestBody = { ssid: ssid };
            if (password.length >= 8) {
                requestBody.password = password;
            }
            
            // Send AP configuration to server
            fetch('/api/apconfig', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(requestBody)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('AP configuration updated successfully!\\nNew SSID: ' + ssid + '\\nNote: Connected AP clients have been disconnected.');
                    closeAPConfig();
                    // Refresh WiFi status
                    fetch('/api/wifistatus')
                        .then(response => response.json())
                        .then(data => updateWiFiStatus(data));
                } else {
                    alert('Failed to update AP configuration: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                alert('Error updating AP configuration: ' + error);
            });
        }

        // Close modal when clicking outside
        window.onclick = function(event) {
            const releaseModal = document.getElementById('releaseNotesModal');
            const apModal = document.getElementById('apConfigModal');
            if (event.target === releaseModal) {
                closeReleaseNotes();
            } else if (event.target === apModal) {
                closeAPConfig();
            }
        }

        // Smart Sensing Functions
        function updateSensingMode() {
            const selected = document.querySelector('input[name="sensingMode"]:checked').value;
            currentSensingMode = selected;
            
            const timerDisplay = document.getElementById('timerDisplay');
            
            // Show/hide timer display based on mode (but keep input always editable)
            if (selected === 'smart_auto') {
                timerDisplay.style.display = 'block';
            } else {
                timerDisplay.style.display = 'none';
            }
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mode: selected })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    alert('Failed to update mode: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Error updating mode:', error);
            });
        }

        function updateTimerDuration() {
            const duration = parseInt(document.getElementById('timerDuration').value);
            if (duration < 1 || duration > 60) {
                alert('Timer duration must be between 1 and 60 minutes');
                return;
            }
            
            timerDurationMinutes = duration;
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timerDuration: duration })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    alert('Failed to update timer: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Error updating timer:', error);
            });
        }

        function updateVoltageThreshold() {
            const threshold = parseFloat(document.getElementById('voltageThreshold').value);
            if (threshold < 0.1 || threshold > 3.3) {
                alert('Voltage threshold must be between 0.1 and 3.3 volts');
                return;
            }
            
            voltageThresholdVolts = threshold;
            
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ voltageThreshold: threshold })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    alert('Failed to update threshold: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Error updating threshold:', error);
            });
        }

        function manualOverride(state) {
            fetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ manualOverride: state })
            })
            .then(response => response.json())
            .then(data => {
                if (!data.success) {
                    alert('Failed to override: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Error with manual override:', error);
            });
        }

        function updateSmartSensingUI(data) {
            // Update mode radio buttons
            if (data.mode) {
                const modeRadio = document.querySelector(`input[name="sensingMode"][value="${data.mode}"]`);
                if (modeRadio) {
                    modeRadio.checked = true;
                    currentSensingMode = data.mode;
                    
                    const timerDisplay = document.getElementById('timerDisplay');
                    
                    // Show/hide timer display based on mode
                    if (data.mode === 'smart_auto') {
                        timerDisplay.style.display = 'block';
                    } else {
                        timerDisplay.style.display = 'none';
                    }
                }
            }
            
            // Get input elements
            const timerDurationInput = document.getElementById('timerDuration');
            const voltageThresholdInput = document.getElementById('voltageThreshold');
            
            // Update timer duration input (ONLY if user is not currently editing it)
            if (data.timerDuration !== undefined && !inputFocusState.timerDuration) {
                timerDurationInput.value = data.timerDuration;
                timerDurationMinutes = data.timerDuration;
            }
            
            // Update timer display
            if (data.timerRemaining !== undefined) {
                const minutes = Math.floor(data.timerRemaining / 60);
                const seconds = data.timerRemaining % 60;
                document.getElementById('timerValue').textContent = 
                    String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
            }
            
            // Update amplifier status
            if (data.amplifierState !== undefined) {
                const statusEl = document.getElementById('amplifierStatus');
                statusEl.textContent = data.amplifierState ? 'ON' : 'OFF';
                statusEl.className = 'amplifier-status ' + (data.amplifierState ? 'on' : 'off');
            }
            
            // Update voltage detection
            if (data.voltageDetected !== undefined) {
                const voltageDetectedEl = document.getElementById('voltageDetected');
                voltageDetectedEl.textContent = data.voltageDetected ? 'Yes' : 'No';
                voltageDetectedEl.style.color = data.voltageDetected ? '#4CAF50' : '#f44336';
            }
            
            // Update voltage reading
            if (data.voltageReading !== undefined) {
                document.getElementById('voltageReading').textContent = data.voltageReading.toFixed(2) + 'V';
            }
            
            // Update threshold (ONLY if user is not currently editing it)
            if (data.voltageThreshold !== undefined && !inputFocusState.voltageThreshold) {
                voltageThresholdInput.value = data.voltageThreshold;
                voltageThresholdVolts = data.voltageThreshold;
            }
        }

        // Initialize drag and drop zone
        function initDragAndDrop() {
            const dropZone = document.getElementById('dropZone');
            const fileInput = document.getElementById('settingsFileInput');
            
            // Prevent default drag behaviors
            ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, preventDefaults, false);
                document.body.addEventListener(eventName, preventDefaults, false);
            });
            
            // Highlight drop zone when item is dragged over it
            ['dragenter', 'dragover'].forEach(eventName => {
                dropZone.addEventListener(eventName, highlight, false);
            });
            
            ['dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, unhighlight, false);
            });
            
            // Handle dropped files
            dropZone.addEventListener('drop', handleDrop, false);
            
            function preventDefaults(e) {
                e.preventDefault();
                e.stopPropagation();
            }
            
            function highlight(e) {
                dropZone.classList.add('drag-over');
            }
            
            function unhighlight(e) {
                dropZone.classList.remove('drag-over');
            }
            
            function handleDrop(e) {
                const dt = e.dataTransfer;
                const files = dt.files;
                
                if (files.length > 0) {
                    // Create a fake event object to pass to handleFileSelect
                    const fakeEvent = {
                        target: {
                            files: files,
                            value: ''
                        }
                    };
                    handleFileSelect(fakeEvent);
                }
            }
        }

        window.onload = function() {
            initWebSocket();
            initDragAndDrop();
            loadMqttSettings();
            // Request initial WiFi status
            fetch('/api/wifistatus')
                .then(response => response.json())
                .then(data => {
                    updateWiFiStatus(data);
                    // Set current version
                    if (data.firmwareVersion) {
                        currentFirmwareVersion = data.firmwareVersion;
                        document.getElementById('currentVersion').textContent = data.firmwareVersion;
                    }
                    
                    // Display latest version if available
                    if (data.latestVersion && data.latestVersion !== 'Checking...') {
                        currentLatestVersion = data.latestVersion;
                        const latestVersionContainer = document.getElementById('latestVersionContainer');
                        const latestVersionSpan = document.getElementById('latestVersion');
                        const latestVersionNotesBtn = document.getElementById('latestVersionNotesBtn');
                        
                        latestVersionContainer.style.display = 'block';
                        latestVersionSpan.textContent = data.latestVersion;
                        
                        if (data.updateAvailable) {
                            latestVersionSpan.className = 'update-available';
                            latestVersionNotesBtn.style.display = 'inline-block';
                            document.getElementById('updateBtn').style.display = 'block';
                            
                            const otaStatus = document.getElementById('otaStatus');
                            otaStatus.className = 'ota-status show update-available';
                            otaStatus.textContent = 'Update available! You can view the release notes or click update to install.';
                        } else {
                            latestVersionSpan.className = 'up-to-date';
                            
                            const otaStatus = document.getElementById('otaStatus');
                            otaStatus.className = 'ota-status show success';
                            otaStatus.textContent = 'You have the latest version installed.';
                        }
                    }
                })
                .catch(error => {
                    console.error('Error fetching WiFi status:', error);
                });
            
            // Load initial Smart Sensing state
            fetch('/api/smartsensing')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        updateSmartSensingUI(data);
                    }
                })
                .catch(error => console.error('Error loading smart sensing state:', error));
        };

        // ===== Debug Console Functions =====
        
        function formatDebugTimestamp(millis) {
            const totalSeconds = Math.floor(millis / 1000);
            const hours = Math.floor(totalSeconds / 3600) % 24;
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            const ms = millis % 1000;
            return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}.${ms.toString().padStart(3, '0')}`;
        }
        
        function getLogMessageClass(message) {
            const lowerMsg = message.toLowerCase();
            if (lowerMsg.includes('error') || lowerMsg.includes('failed') || lowerMsg.includes('fail')) {
                return 'error';
            } else if (lowerMsg.includes('warning') || lowerMsg.includes('warn')) {
                return 'warn';
            } else if (lowerMsg.includes('===') || lowerMsg.includes('connected') || lowerMsg.includes('success') || lowerMsg.includes('initialized')) {
                return 'info';
            }
            return '';
        }
        
        function appendDebugLog(timestamp, message) {
            if (debugPaused) {
                // Buffer messages while paused
                debugLogBuffer.push({ timestamp, message });
                if (debugLogBuffer.length > DEBUG_MAX_LINES) {
                    debugLogBuffer.shift();
                }
                return;
            }
            
            const console = document.getElementById('debugConsole');
            const autoScroll = document.getElementById('debugAutoScroll').checked;
            const wasAtBottom = console.scrollHeight - console.scrollTop <= console.clientHeight + 50;
            
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            
            const timestampSpan = document.createElement('span');
            timestampSpan.className = 'log-timestamp';
            timestampSpan.textContent = '[' + formatDebugTimestamp(timestamp) + ']';
            
            const messageSpan = document.createElement('span');
            messageSpan.className = 'log-message ' + getLogMessageClass(message);
            messageSpan.textContent = message;
            
            entry.appendChild(timestampSpan);
            entry.appendChild(messageSpan);
            console.appendChild(entry);
            
            // Limit the number of log entries
            while (console.children.length > DEBUG_MAX_LINES) {
                console.removeChild(console.firstChild);
            }
            
            // Auto-scroll if enabled and was at bottom
            if (autoScroll && wasAtBottom) {
                console.scrollTop = console.scrollHeight;
            }
        }
        
        // ===== Hardware Stats Functions =====
        let statsRefreshInterval = null;
        let statsRefreshRate = 5000; // Default 5 seconds
        
        function initHardwareStats() {
            // Load saved refresh rate from localStorage
            const savedRate = localStorage.getItem('statsRefreshRate');
            if (savedRate) {
                statsRefreshRate = parseInt(savedRate);
                document.getElementById('statsRefreshRate').value = statsRefreshRate;
            }
            
            // Start requesting stats
            startStatsPolling();
        }
        
        function startStatsPolling() {
            // Clear existing interval if any
            if (statsRefreshInterval) {
                clearInterval(statsRefreshInterval);
            }
            
            // Request stats immediately
            requestHardwareStats();
            
            // Set up periodic polling
            statsRefreshInterval = setInterval(requestHardwareStats, statsRefreshRate);
        }
        
        function requestHardwareStats() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'getHardwareStats' }));
            }
        }
        
        function changeStatsRefreshRate() {
            const select = document.getElementById('statsRefreshRate');
            statsRefreshRate = parseInt(select.value);
            localStorage.setItem('statsRefreshRate', statsRefreshRate);
            startStatsPolling();
        }
        
        function formatBytes(bytes, decimals = 1) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(decimals)) + ' ' + sizes[i];
        }
        
        function formatUptime(ms) {
            const seconds = Math.floor(ms / 1000);
            const days = Math.floor(seconds / 86400);
            const hours = Math.floor((seconds % 86400) / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            
            let result = '';
            if (days > 0) result += days + 'd ';
            if (hours > 0 || days > 0) result += hours + 'h ';
            if (minutes > 0 || hours > 0 || days > 0) result += minutes + 'm ';
            result += secs + 's';
            return result;
        }
        
        function getRssiQuality(rssi) {
            if (rssi >= -50) return { label: 'Excellent', class: 'excellent', percent: 100 };
            if (rssi >= -60) return { label: 'Good', class: 'good', percent: 80 };
            if (rssi >= -70) return { label: 'Fair', class: 'fair', percent: 60 };
            return { label: 'Poor', class: 'poor', percent: Math.max(20, 100 + rssi) };
        }
        
        function updateBarColor(barElement, percentage, baseClass) {
            barElement.className = 'stat-bar-fill ' + baseClass;
            if (percentage > 80) barElement.classList.add('warning');
            if (percentage > 90) barElement.classList.replace('warning', 'danger');
        }
        
        // ===== Performance History Functions =====
        
        function toggleHistorySection() {
            historyCollapsed = !historyCollapsed;
            const content = document.getElementById('historyContent');
            const icon = document.getElementById('historyCollapseIcon');
            
            if (historyCollapsed) {
                content.classList.add('collapsed');
                icon.classList.add('collapsed');
            } else {
                content.classList.remove('collapsed');
                icon.classList.remove('collapsed');
                // Redraw graphs when expanding
                drawCpuGraph();
                drawMemoryGraph();
            }
            
            // Save preference
            localStorage.setItem('historyCollapsed', historyCollapsed);
        }
        
        function changeHistoryRange() {
            const select = document.getElementById('historyTimeRange');
            maxHistoryPoints = parseInt(select.value);
            localStorage.setItem('historyTimeRange', maxHistoryPoints);
            
            // Trim existing data if needed
            while (historyData.timestamps.length > maxHistoryPoints) {
                historyData.timestamps.shift();
                historyData.cpuTotal.shift();
                historyData.cpuCore0.shift();
                historyData.cpuCore1.shift();
                historyData.memoryPercent.shift();
            }
            
            // Redraw graphs
            drawCpuGraph();
            drawMemoryGraph();
        }
        
        function addHistoryDataPoint(data) {
            const now = Date.now();
            
            // Add new data points
            historyData.timestamps.push(now);
            
            if (data.cpu) {
                historyData.cpuTotal.push(data.cpu.usageTotal || 0);
                historyData.cpuCore0.push(data.cpu.usageCore0 || 0);
                historyData.cpuCore1.push(data.cpu.usageCore1 || 0);
            } else {
                historyData.cpuTotal.push(0);
                historyData.cpuCore0.push(0);
                historyData.cpuCore1.push(0);
            }
            
            if (data.memory && data.memory.heapTotal > 0) {
                const heapUsed = data.memory.heapTotal - data.memory.heapFree;
                const heapPercent = (heapUsed / data.memory.heapTotal) * 100;
                historyData.memoryPercent.push(heapPercent);
            } else {
                historyData.memoryPercent.push(0);
            }
            
            // Trim old data if exceeds max points
            while (historyData.timestamps.length > maxHistoryPoints) {
                historyData.timestamps.shift();
                historyData.cpuTotal.shift();
                historyData.cpuCore0.shift();
                historyData.cpuCore1.shift();
                historyData.memoryPercent.shift();
            }
            
            // Redraw graphs if section is visible
            if (!historyCollapsed) {
                drawCpuGraph();
                drawMemoryGraph();
            }
        }
        
        function initHistorySettings() {
            // Load saved preferences
            const savedCollapsed = localStorage.getItem('historyCollapsed');
            if (savedCollapsed === 'true') {
                historyCollapsed = true;
                document.getElementById('historyContent').classList.add('collapsed');
                document.getElementById('historyCollapseIcon').classList.add('collapsed');
            }
            
            const savedRange = localStorage.getItem('historyTimeRange');
            if (savedRange) {
                maxHistoryPoints = parseInt(savedRange);
                document.getElementById('historyTimeRange').value = savedRange;
            }
        }
        
        // ===== Canvas Graph Rendering =====
        
        function drawGraph(canvasId, datasets, options = {}) {
            const canvas = document.getElementById(canvasId);
            if (!canvas) return;
            
            const ctx = canvas.getContext('2d');
            const rect = canvas.getBoundingClientRect();
            
            // Set canvas size for high DPI displays
            const dpr = window.devicePixelRatio || 1;
            canvas.width = rect.width * dpr;
            canvas.height = rect.height * dpr;
            ctx.scale(dpr, dpr);
            
            const width = rect.width;
            const height = rect.height;
            const padding = { top: 10, right: 10, bottom: 25, left: 40 };
            const graphWidth = width - padding.left - padding.right;
            const graphHeight = height - padding.top - padding.bottom;
            
            // Clear canvas
            ctx.clearRect(0, 0, width, height);
            
            // Get text color based on theme
            const isDarkTheme = !document.body.classList.contains('light-theme');
            const textColor = isDarkTheme ? 'rgba(255, 255, 255, 0.6)' : 'rgba(0, 0, 0, 0.6)';
            const gridColor = isDarkTheme ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)';
            
            // Draw grid and Y-axis labels
            ctx.strokeStyle = gridColor;
            ctx.lineWidth = 1;
            ctx.font = '10px sans-serif';
            ctx.fillStyle = textColor;
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';
            
            const yLabels = [0, 25, 50, 75, 100];
            yLabels.forEach(val => {
                const y = padding.top + graphHeight - (val / 100) * graphHeight;
                
                // Grid line
                ctx.beginPath();
                ctx.moveTo(padding.left, y);
                ctx.lineTo(width - padding.right, y);
                ctx.stroke();
                
                // Label
                ctx.fillText(val + '%', padding.left - 5, y);
            });
            
            // Draw X-axis time labels
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            
            const timeRangeSeconds = maxHistoryPoints * (statsRefreshRate / 1000);
            const timeLabels = [];
            
            if (timeRangeSeconds <= 60) {
                timeLabels.push({ pos: 0, label: 'now' }, { pos: 0.5, label: '-30s' }, { pos: 1, label: '-1m' });
            } else if (timeRangeSeconds <= 300) {
                timeLabels.push({ pos: 0, label: 'now' }, { pos: 0.2, label: '-1m' }, { pos: 0.4, label: '-2m' }, 
                               { pos: 0.6, label: '-3m' }, { pos: 0.8, label: '-4m' }, { pos: 1, label: '-5m' });
            } else {
                timeLabels.push({ pos: 0, label: 'now' }, { pos: 0.2, label: '-2m' }, { pos: 0.4, label: '-4m' }, 
                               { pos: 0.6, label: '-6m' }, { pos: 0.8, label: '-8m' }, { pos: 1, label: '-10m' });
            }
            
            timeLabels.forEach(item => {
                const x = padding.left + graphWidth - (item.pos * graphWidth);
                ctx.fillText(item.label, x, height - padding.bottom + 5);
            });
            
            // Draw data lines
            if (historyData.timestamps.length < 2) {
                // Not enough data yet
                ctx.fillStyle = textColor;
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText('Collecting data...', width / 2, height / 2);
                return;
            }
            
            datasets.forEach(dataset => {
                const data = dataset.data;
                if (!data || data.length < 2) return;
                
                ctx.strokeStyle = dataset.color;
                ctx.lineWidth = dataset.lineWidth || 2;
                ctx.lineCap = 'round';
                ctx.lineJoin = 'round';
                
                ctx.beginPath();
                
                for (let i = 0; i < data.length; i++) {
                    // X position: rightmost = newest data
                    const x = padding.left + (i / (maxHistoryPoints - 1)) * graphWidth;
                    // Y position: 0% at bottom, 100% at top
                    const y = padding.top + graphHeight - (data[i] / 100) * graphHeight;
                    
                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                
                ctx.stroke();
            });
        }
        
        function drawCpuGraph() {
            drawGraph('cpuGraph', [
                { data: historyData.cpuTotal, color: '#ff9800', lineWidth: 2.5 },
                { data: historyData.cpuCore0, color: '#ffb74d', lineWidth: 1.5 },
                { data: historyData.cpuCore1, color: '#f57c00', lineWidth: 1.5 }
            ]);
        }
        
        function drawMemoryGraph() {
            drawGraph('memoryGraph', [
                { data: historyData.memoryPercent, color: '#2196F3', lineWidth: 2.5 }
            ]);
        }
        
        function updateHardwareStats(data) {
            // Memory stats
            if (data.memory) {
                const heapUsed = data.memory.heapTotal - data.memory.heapFree;
                const heapPercent = Math.round((heapUsed / data.memory.heapTotal) * 100);
                
                document.getElementById('statHeapUsed').textContent = 
                    formatBytes(data.memory.heapFree) + ' / ' + formatBytes(data.memory.heapTotal);
                
                const heapBar = document.getElementById('statHeapBar');
                heapBar.style.width = heapPercent + '%';
                heapBar.className = 'stat-bar-fill memory';
                if (heapPercent > 80) heapBar.classList.add('warning');
                if (heapPercent > 90) heapBar.classList.replace('warning', 'danger');
                
                document.getElementById('statHeapMinFree').textContent = formatBytes(data.memory.heapMinFree);
                document.getElementById('statHeapMaxBlock').textContent = formatBytes(data.memory.heapMaxBlock);
                
                // PSRAM
                if (data.memory.psramTotal > 0) {
                    document.getElementById('statPsram').textContent = 
                        formatBytes(data.memory.psramFree) + ' / ' + formatBytes(data.memory.psramTotal);
                } else {
                    document.getElementById('statPsram').textContent = 'N/A';
                }
            }
            
            // CPU stats
            if (data.cpu) {
                document.getElementById('statCpuModel').textContent = data.cpu.model || '--';
                document.getElementById('statCpuFreq').textContent = data.cpu.freqMHz + ' MHz';
                
                // Temperature (might be NaN if not supported)
                if (data.cpu.temperature && !isNaN(data.cpu.temperature) && data.cpu.temperature > 0) {
                    document.getElementById('statCpuTemp').textContent = data.cpu.temperature.toFixed(1) + ' °C';
                } else {
                    document.getElementById('statCpuTemp').textContent = 'N/A';
                }
                
                // CPU Usage per core
                if (data.cpu.usageCore0 !== undefined) {
                    const core0Usage = data.cpu.usageCore0.toFixed(1);
                    document.getElementById('statCpuCore0').textContent = core0Usage + '%';
                    const core0Bar = document.getElementById('statCpuCore0Bar');
                    core0Bar.style.width = core0Usage + '%';
                    updateBarColor(core0Bar, data.cpu.usageCore0, 'cpu');
                }
                
                if (data.cpu.usageCore1 !== undefined) {
                    const core1Usage = data.cpu.usageCore1.toFixed(1);
                    document.getElementById('statCpuCore1').textContent = core1Usage + '%';
                    const core1Bar = document.getElementById('statCpuCore1Bar');
                    core1Bar.style.width = core1Usage + '%';
                    updateBarColor(core1Bar, data.cpu.usageCore1, 'cpu');
                }
                
                if (data.cpu.usageTotal !== undefined) {
                    document.getElementById('statCpuTotal').textContent = data.cpu.usageTotal.toFixed(1) + '%';
                }
            }
            
            // Storage stats
            if (data.storage) {
                document.getElementById('statFlashSize').textContent = formatBytes(data.storage.flashSize);
                document.getElementById('statSketchSize').textContent = formatBytes(data.storage.sketchSize);
                document.getElementById('statOtaFree').textContent = formatBytes(data.storage.sketchFree);
                
                if (data.storage.spiffsTotal > 0) {
                    const spiffsPercent = Math.round((data.storage.spiffsUsed / data.storage.spiffsTotal) * 100);
                    document.getElementById('statSpiffs').textContent = 
                        formatBytes(data.storage.spiffsUsed) + ' / ' + formatBytes(data.storage.spiffsTotal);
                    
                    const spiffsBar = document.getElementById('statSpiffsBar');
                    spiffsBar.style.width = spiffsPercent + '%';
                    spiffsBar.className = 'stat-bar-fill storage';
                    if (spiffsPercent > 80) spiffsBar.classList.add('warning');
                    if (spiffsPercent > 90) spiffsBar.classList.replace('warning', 'danger');
                } else {
                    document.getElementById('statSpiffs').textContent = 'N/A';
                    document.getElementById('statSpiffsBar').style.width = '0%';
                }
            }
            
            // WiFi stats
            if (data.wifi) {
                const rssi = data.wifi.rssi;
                document.getElementById('statWifiRssi').textContent = rssi + ' dBm';
                
                const quality = getRssiQuality(rssi);
                const qualityEl = document.getElementById('statWifiQuality');
                qualityEl.textContent = quality.label;
                qualityEl.className = 'signal-quality ' + quality.class;
                
                document.getElementById('statWifiBar').style.width = quality.percent + '%';
                document.getElementById('statWifiChannel').textContent = data.wifi.channel || '--';
                document.getElementById('statApClients').textContent = data.wifi.apClients;
            }
            
            // Uptime
            if (data.uptime !== undefined) {
                document.getElementById('statUptime').textContent = formatUptime(data.uptime);
            }
            
            // Add data point to history graphs
            addHistoryDataPoint(data);
        }
        
        // Initialize hardware stats when page loads
        document.addEventListener('DOMContentLoaded', function() {
            // Initialize history settings
            initHistorySettings();
            
            // Delay slightly to ensure WebSocket is ready
            setTimeout(initHardwareStats, 1000);
            
            // Initial graph draw (empty state)
            setTimeout(function() {
                drawCpuGraph();
                drawMemoryGraph();
            }, 100);
        });
        
        // Redraw graphs on window resize
        window.addEventListener('resize', function() {
            if (!historyCollapsed) {
                drawCpuGraph();
                drawMemoryGraph();
            }
        });
        
        function clearDebugConsole() {
            const console = document.getElementById('debugConsole');
            console.innerHTML = '<div class="log-entry"><span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span></div>';
            debugLogBuffer = [];
        }
        
        function toggleDebugPause() {
            debugPaused = !debugPaused;
            const btn = document.getElementById('debugPauseBtn');
            const indicator = document.getElementById('debugStatusIndicator');
            const statusText = document.getElementById('debugStatusText');
            
            if (debugPaused) {
                btn.textContent = 'Resume';
                btn.classList.add('paused');
                indicator.classList.add('paused');
                statusText.textContent = 'Paused (' + debugLogBuffer.length + ' buffered)';
            } else {
                btn.textContent = 'Pause';
                btn.classList.remove('paused');
                indicator.classList.remove('paused');
                statusText.textContent = 'Receiving';
                
                // Flush buffered messages
                debugLogBuffer.forEach(item => {
                    appendDebugLog(item.timestamp, item.message);
                });
                debugLogBuffer = [];
            }
        }
        
        // Update buffered count while paused
        setInterval(() => {
            if (debugPaused && debugLogBuffer.length > 0) {
                document.getElementById('debugStatusText').textContent = 'Paused (' + debugLogBuffer.length + ' buffered)';
            }
        }, 500);
    </script>
</body>
</html>
)rawliteral";

const char apHtmlPage[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 WiFi Setup</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
            margin: 0;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .container {
            background: rgba(255, 255, 255, 0.1);
            padding: 40px;
            border-radius: 20px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            backdrop-filter: blur(10px);
            text-align: center;
            width: 100%;
            max-width: 400px;
        }
        h1 {
            margin-top: 0;
            font-size: 2em;
        }
        .form-group {
            margin: 20px 0;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: bold;
        }
        input {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 1em;
        }
        button {
            background: #4CAF50;
            color: white;
            border: none;
            padding: 12px 40px;
            font-size: 1.1em;
            border-radius: 5px;
            cursor: pointer;
            transition: all 0.3s ease;
            margin-top: 20px;
            width: 100%;
        }
        button:hover {
            background: #45a049;
            transform: translateY(-2px);
        }
        .info {
            margin-top: 20px;
            font-size: 0.9em;
            opacity: 0.9;
        }
        .status-message {
            margin-top: 15px;
            padding: 10px;
            border-radius: 5px;
            display: none;
        }
        .status-message.success {
            background: #4CAF50;
            display: block;
        }
        .status-message.error {
            background: #f44336;
            display: block;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 WiFi Setup</h1>
        <div class="info">
            <p id="currentAPInfo">Current AP: Loading...</p>
        </div>
        <form onsubmit="submitWiFi(event)">
            <div class="form-group">
                <label for="ssid">Network Name (SSID):</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Enter WiFi SSID">
            </div>
            <div class="form-group">
                <label for="password">Password:</label>
                <input type="password" id="password" name="password" required placeholder="Enter WiFi password">
            </div>
            <button type="submit">Connect to WiFi</button>
        </form>
        <div class="info">
            <p>Enter your WiFi network credentials to connect the ESP32.</p>
        </div>
        <div class="status-message" id="statusMessage"></div>
    </div>

    <script>
        // Load current AP information on page load
        window.onload = function() {
            fetch('/api/wifistatus')
                .then(response => response.json())
                .then(data => {
                    const apInfo = document.getElementById('currentAPInfo');
                    const ssidInput = document.getElementById('ssid');
                    
                    if (data.apSSID) {
                        apInfo.textContent = 'Current AP: ' + data.apSSID;
                        // Prefill SSID field with current AP SSID
                        ssidInput.value = data.apSSID;
                    } else {
                        apInfo.textContent = 'Current AP: ESP32-LED-Setup';
                    }
                })
                .catch(error => {
                    console.error('Error fetching AP info:', error);
                });
        };

        function submitWiFi(event) {
            event.preventDefault();
            const ssid = document.getElementById("ssid").value;
            const password = document.getElementById("password").value;
            const statusMsg = document.getElementById('statusMessage');
            
            fetch('/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    ssid: ssid,
                    password: password
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    statusMsg.className = 'status-message success';
                    statusMsg.textContent = 'Configuration saved! Connecting to WiFi...';
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 3000);
                } else {
                    statusMsg.className = 'status-message error';
                    statusMsg.textContent = 'Error: ' + (data.message || 'Unknown error');
                }
            })
            .catch(error => {
                statusMsg.className = 'status-message error';
                statusMsg.textContent = 'Error: ' + error;
            });
        }
    </script>
</body>
</html>
)rawliteral";

