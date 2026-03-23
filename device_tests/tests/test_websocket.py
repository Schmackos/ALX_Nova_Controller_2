"""WebSocket protocol tests.

Verifies the firmware's WebSocket auth handshake, command dispatch,
state broadcasts, binary frame delivery, and edge case handling.
Requires the ``websocket-client`` package.
"""

import json
import time

import pytest

from utils.ws_client import DeviceWebSocket


# ===========================================================================
# Auth handshake
# ===========================================================================

@pytest.mark.ws
@pytest.mark.network
class TestWebSocketAuth:
    """Verify WebSocket authentication flow."""

    def test_ws_connect_receives_auth_required(self, device_ip):
        """First message after connect must be authRequired."""
        ws = DeviceWebSocket(device_ip)
        ws.connect(timeout=10)
        try:
            msg = ws.recv_json(timeout=5)
            assert msg is not None, "No message received after WS connect"
            assert msg.get("type") == "authRequired", (
                f"Expected authRequired, got: {msg.get('type')}"
            )
        finally:
            ws.close()

    def test_ws_auth_with_valid_token(self, device_ip, ws_token):
        """Valid token produces authSuccess."""
        ws = DeviceWebSocket(device_ip)
        ws.connect(timeout=10)
        try:
            success = ws.authenticate(ws_token, timeout=10)
            assert success, "WS auth with valid token failed"
        finally:
            ws.close()

    def test_ws_auth_with_invalid_token(self, device_ip):
        """Invalid token produces authFailed and disconnects."""
        ws = DeviceWebSocket(device_ip)
        ws.connect(timeout=10)
        try:
            # Consume authRequired
            ws.recv_until("authRequired", timeout=5)
            ws.send_command("auth", token="invalid-token-xyz")
            # Expect authFailed or disconnect
            msg = ws.recv_json(timeout=5)
            if msg is not None:
                assert msg.get("type") == "authFailed", (
                    f"Expected authFailed, got: {msg.get('type')}"
                )
            # Connection should close soon
            time.sleep(1)
            assert not ws.connected, "WS should disconnect after bad auth"
        finally:
            ws.close()

    def test_ws_auth_timeout(self, device_ip):
        """Connection without auth should time out and disconnect."""
        ws = DeviceWebSocket(device_ip)
        ws.connect(timeout=10)
        try:
            # Consume authRequired but don't authenticate
            ws.recv_until("authRequired", timeout=5)
            # Wait for server-side auth timeout (~5s)
            time.sleep(7)
            assert not ws.connected, (
                "WS should disconnect after auth timeout"
            )
        finally:
            ws.close()

    def test_ws_auth_receives_initial_state(self, ws_client):
        """After auth, firmware sends initial state broadcasts."""
        # Collect messages for a few seconds
        time.sleep(3)
        messages = ws_client.drain_json()
        types = {m.get("type") for m in messages}
        # At minimum we expect some of the 17 init state types
        expected_any = {
            "wiFiStatus", "halDeviceState", "dspState", "dacState",
            "displayState", "debugState", "hardwareStats",
            "audioChannelMap", "buzzerState",
        }
        found = types & expected_any
        assert len(found) >= 3, (
            f"Expected >=3 initial state types, got {len(found)}: {found}"
        )


# ===========================================================================
# Command dispatch
# ===========================================================================

@pytest.mark.ws
@pytest.mark.network
class TestWebSocketCommands:
    """Verify WS command → response flow."""

    def test_ws_get_hardware_stats(self, ws_client):
        """getHardwareStats command returns hardwareStats response."""
        # Drain any queued init messages
        ws_client.drain_json()
        ws_client.send_command("getHardwareStats")
        resp = ws_client.recv_until("hardwareStats", timeout=5)
        assert resp is not None, "No hardwareStats response received"
        # Should contain heap info
        assert "freeHeap" in resp or "heap" in resp or "heapFree" in resp

    def test_ws_get_health_check(self, ws_client):
        """getHealthCheck command returns healthCheckState response."""
        ws_client.drain_json()
        ws_client.send_command("getHealthCheck")
        resp = ws_client.recv_until("healthCheckState", timeout=10)
        assert resp is not None, "No healthCheckState response received"

    def test_ws_set_debug_mode_roundtrip(self, ws_client):
        """setDebugMode toggles and produces debugState broadcast."""
        ws_client.drain_json()
        # Enable debug mode
        ws_client.send_command("setDebugMode", enabled=True)
        resp = ws_client.recv_until("debugState", timeout=5)
        assert resp is not None, "No debugState after setDebugMode(true)"

        # Restore: disable
        ws_client.send_command("setDebugMode", enabled=False)
        resp2 = ws_client.recv_until("debugState", timeout=5)
        assert resp2 is not None, "No debugState after setDebugMode(false)"

    def test_ws_subscribe_audio(self, ws_client):
        """subscribeAudio enable/disable doesn't crash."""
        ws_client.drain_json()
        ws_client.send_command("subscribeAudio", enabled=True)
        time.sleep(1)
        # Disable (restore)
        ws_client.send_command("subscribeAudio", enabled=False)
        time.sleep(0.5)
        # If we get here without crash/disconnect, pass
        assert ws_client.connected

    def test_ws_oversized_message(self, ws_client):
        """Message >4096 bytes should be rejected, not crash the device."""
        big_payload = json.dumps({"type": "test", "data": "x" * 5000})
        ws_client.send_raw(big_payload)
        time.sleep(1)
        # Connection should still be alive (message was rejected silently)
        assert ws_client.connected, "Device crashed on oversized WS message"


# ===========================================================================
# Binary frames
# ===========================================================================

@pytest.mark.ws
@pytest.mark.network
class TestWebSocketBinaryFrames:
    """Verify binary audio data frames."""

    def test_ws_binary_waveform_frame(self, ws_client):
        """After audio subscribe, receive waveform binary (0x01)."""
        ws_client.drain_binary()
        ws_client.send_command(
            "subscribeAudio", enabled=True,
        )
        ws_client.send_command("setWaveformEnabled", enabled=True)
        frame = ws_client.recv_binary(timeout=5)
        # Restore
        ws_client.send_command("setWaveformEnabled", enabled=False)
        ws_client.send_command("subscribeAudio", enabled=False)
        if frame is None:
            pytest.skip("No binary waveform frame received (may need ADC)")
        assert frame[0] == 0x01, f"Expected waveform type 0x01, got 0x{frame[0]:02x}"

    def test_ws_binary_spectrum_frame(self, ws_client):
        """After spectrum subscribe, receive spectrum binary (0x02)."""
        ws_client.drain_binary()
        ws_client.send_command("subscribeAudio", enabled=True)
        ws_client.send_command("setSpectrumEnabled", enabled=True)
        frame = ws_client.recv_binary(timeout=5)
        # Restore
        ws_client.send_command("setSpectrumEnabled", enabled=False)
        ws_client.send_command("subscribeAudio", enabled=False)
        if frame is None:
            pytest.skip("No binary spectrum frame received (may need ADC)")
        assert frame[0] == 0x02, f"Expected spectrum type 0x02, got 0x{frame[0]:02x}"

    def test_ws_binary_frame_size(self, ws_client):
        """Binary frames should match expected sizes."""
        ws_client.drain_binary()
        ws_client.send_command("subscribeAudio", enabled=True)
        ws_client.send_command("setWaveformEnabled", enabled=True)
        ws_client.send_command("setSpectrumEnabled", enabled=True)
        time.sleep(2)
        # Restore
        ws_client.send_command("setWaveformEnabled", enabled=False)
        ws_client.send_command("setSpectrumEnabled", enabled=False)
        ws_client.send_command("subscribeAudio", enabled=False)

        frames = ws_client.drain_binary()
        if not frames:
            pytest.skip("No binary frames received (may need ADC)")

        for frame in frames:
            if frame[0] == 0x01:
                # Waveform: [type:1][adc:1][samples:256] = 258 bytes
                assert len(frame) == 258, f"Waveform frame size: {len(frame)} != 258"
            elif frame[0] == 0x02:
                # Spectrum: [type:1][adc:1][freq:4][bands:16*4] = 70 bytes
                assert len(frame) == 70, f"Spectrum frame size: {len(frame)} != 70"


# ===========================================================================
# Edge cases
# ===========================================================================

@pytest.mark.ws
@pytest.mark.network
class TestWebSocketEdgeCases:
    """Edge-case and multi-client scenarios."""

    def test_ws_multiple_connections(self, device_ip, api):
        """Two authenticated clients can coexist."""
        # Get two tokens
        t1 = api.get("/api/ws-token").json()["token"]
        t2 = api.get("/api/ws-token").json()["token"]

        ws1 = DeviceWebSocket(device_ip)
        ws2 = DeviceWebSocket(device_ip)
        ws1.connect(timeout=10)
        ws2.connect(timeout=10)
        try:
            ok1 = ws1.authenticate(t1, timeout=10)
            ok2 = ws2.authenticate(t2, timeout=10)
            assert ok1, "Client 1 auth failed"
            assert ok2, "Client 2 auth failed"
            # Both should be connected
            time.sleep(1)
            assert ws1.connected and ws2.connected
        finally:
            ws1.close()
            ws2.close()

    def test_ws_unauthenticated_command_rejected(self, device_ip):
        """Commands sent without auth should be ignored / cause disconnect."""
        ws = DeviceWebSocket(device_ip)
        ws.connect(timeout=10)
        try:
            # Consume authRequired
            ws.recv_until("authRequired", timeout=5)
            # Send command without authenticating
            ws.send_command("getHardwareStats")
            # Should not receive a response (or get disconnected)
            resp = ws.recv_until("hardwareStats", timeout=3)
            assert resp is None, "Received response without auth"
        finally:
            ws.close()
