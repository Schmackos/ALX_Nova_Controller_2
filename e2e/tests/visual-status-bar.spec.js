/**
 * visual-status-bar.spec.js — Visual regression tests for the top status bar
 * showing amplifier, WiFi/Ethernet, MQTT, and WebSocket indicators.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@visual Visual Status Bar', () => {

  test('status bar with amp ON and WiFi connected', async ({ connectedPage: page }) => {
    // Push smart sensing with amp ON
    page.wsRoute.send({
      type: 'smartSensing',
      mode: 'smart_auto',
      timerDuration: 30,
      timerRemaining: 0,
      timerActive: false,
      amplifierState: true,
      audioThreshold: -40.0,
      audioLevel: -30.0,
      signalDetected: true,
      audioSampleRate: 48000
    });
    await page.waitForTimeout(300);

    // WiFi is already connected from initial state
    const statusBar = page.locator('#statusBar');
    await expect(statusBar).toBeVisible({ timeout: 3000 });

    // Verify amp indicator shows ON before screenshot
    await expect(page.locator('#statusAmpText')).toHaveText('Amp ON', { timeout: 3000 });
    await expect(statusBar).toHaveScreenshot('status-bar-amp-on-wifi.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('status bar with amp OFF', async ({ connectedPage: page }) => {
    // Default smart-sensing has amplifierState: false
    // Just verify the default state
    const statusBar = page.locator('#statusBar');
    await expect(statusBar).toBeVisible({ timeout: 3000 });
    await expect(page.locator('#statusAmpText')).toHaveText('Amp OFF', { timeout: 3000 });

    await expect(statusBar).toHaveScreenshot('status-bar-amp-off.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('status bar with WS disconnected', async ({ connectedPage: page }) => {
    // Capture the status bar first in connected state, then simulate disconnect
    const statusBar = page.locator('#statusBar');
    await expect(statusBar).toBeVisible({ timeout: 3000 });

    // Simulate WS disconnect by evaluating the updateStatusBar function
    // with wsConnected = false
    await page.evaluate(() => {
      const wsIndicator = document.getElementById('statusWs');
      if (wsIndicator) {
        wsIndicator.className = 'status-indicator offline';
      }
    });
    await page.waitForTimeout(200);

    await expect(statusBar).toHaveScreenshot('status-bar-ws-disconnected.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('status bar with MQTT connected', async ({ connectedPage: page }) => {
    // MQTT is connected from initial state (mqtt-settings.json has connected: true)
    // Push explicit MQTT connected state
    page.wsRoute.send({
      type: 'mqttSettings',
      enabled: true,
      broker: '192.168.1.50',
      port: 1883,
      username: '',
      hasPassword: false,
      baseTopic: 'alx-nova',
      haDiscovery: true,
      connected: true
    });
    await page.waitForTimeout(300);

    const statusBar = page.locator('#statusBar');
    await expect(statusBar).toBeVisible({ timeout: 3000 });

    // Verify MQTT indicator shows online
    const mqttIndicator = page.locator('#statusMqtt');
    await expect(mqttIndicator).toHaveClass(/online/, { timeout: 3000 });

    await expect(statusBar).toHaveScreenshot('status-bar-mqtt-connected.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

});
