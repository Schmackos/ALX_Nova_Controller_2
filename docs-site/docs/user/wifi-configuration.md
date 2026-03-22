---
title: Network Configuration
sidebar_position: 5
description: Connecting the ALX Nova to your network via Ethernet or WiFi.
---

# Network Configuration

ALX Nova supports both wired and wireless networking. Ethernet is the preferred interface when a cable is connected — WiFi automatically acts as backup. The **Network** tab in the web interface shows both interfaces side by side and lets you configure each independently.

---

## Network Overview

The **Network Overview** card at the top of the Network tab always shows which interface is currently active and what IP address the device is using. When Ethernet is connected it takes priority and the status bar at the top of the page shows **ETH**. When the controller is on WiFi only it shows **WiFi**. When both are active simultaneously it shows **Net**.

### Ethernet-Preferred Auto Failover

- When an Ethernet cable is plugged in, the controller promotes Ethernet to the active interface automatically. No configuration is needed.
- If the cable is unplugged, the controller falls back to the configured WiFi network without any manual intervention.
- Both interfaces can be active simultaneously — Ethernet carries primary traffic while WiFi remains available for local access.

---

## Ethernet

The ALX Nova automatically detects an Ethernet connection. When a cable is plugged in, Ethernet becomes the preferred network interface.

### Ethernet Status

The **Ethernet Status** card shows real-time link information:

| Field | Description |
|-------|-------------|
| Link Status | Cable detection — Connected, Link Up, or No Cable |
| IP Address | Assigned via DHCP or static configuration |
| MAC Address | Hardware address (always visible, useful for DHCP reservations) |
| Speed / Duplex | Link negotiation result (e.g., 100 Mbps Full Duplex) |
| Gateway | Default gateway IP |
| DNS | Primary and secondary DNS servers |
| Subnet Mask | Network subnet |

The card uses colour-coded borders to indicate link health at a glance:
- **Green** — Connected with a valid IP address
- **Amber** — Cable connected, awaiting a DHCP lease
- **Grey** — No cable detected

### Static IP Configuration

To assign a static IP address to the Ethernet interface:

1. Open the **Ethernet Configuration** card on the Network tab.
2. Toggle **Use Static IP** on. The configuration fields appear below.
3. Fill in the required fields:
   - **IPv4 Address** — The static IP you want the device to use (e.g., `192.168.1.100`)
   - **Subnet Mask** — Usually `255.255.255.0`
   - **Gateway** — Your router's IP address (e.g., `192.168.1.1`)
   - **Primary DNS** — e.g., `8.8.8.8` (optional)
   - **Secondary DNS** — e.g., `8.8.4.4` (optional)
4. Click **Apply Static IP**.

:::warning Safety Timer
After applying a static IP, you have **60 seconds** to confirm the configuration. If you do not click **Confirm** within that window, the device automatically reverts to DHCP. This prevents you from being locked out by a misconfigured IP address.
:::

:::tip Recovery
If you lose connection after applying a static IP, you can still access the device via:
- **WiFi** — Connect to the device's home WiFi network using its WiFi IP address.
- **Access Point** — Enable AP mode (double-click the physical button) and connect at `192.168.4.1`.
:::

### Hostname

The **Hostname** field in the Ethernet Configuration card sets the device name advertised on your network. This name applies to both the Ethernet and WiFi interfaces — your router's DHCP client list will show it for whichever interface is active. The default hostname is `alx-nova`.

Hostnames must be 1–63 characters, using only letters, numbers, and hyphens. The hostname cannot start or end with a hyphen.

:::info AP Banner
If you access the web interface via the Access Point while Ethernet is also connected, a banner appears at the top of the page reminding you that you are using the AP interface and that the device is also reachable via the Ethernet IP.
:::

---

## WiFi

WiFi acts as the secondary interface when Ethernet is connected, and as the primary interface when no cable is plugged in.

### First Boot — Access Point Mode

On a fresh device with no WiFi credentials saved, the ALX Nova starts in AP mode automatically.

1. On your phone or laptop, open the WiFi settings and look for a network named **ALX-XXXXXXXXXXXX** (the Xs are your device's unique serial number, derived from its MAC address).
2. Connect using the default password: **12345678**
3. Open a browser and go to **http://192.168.4.1**
4. Log in — the one-time password is shown on the TFT display's boot screen and on the serial console.
5. Go to the **Network** tab and add your home network (see below).

:::info
The AP and the TFT display both show the same serial number suffix. If you have multiple ALX Nova devices, this lets you tell them apart.
:::

### Connecting to Your Home Network (Station Mode)

**What you will need:**
- Your WiFi network name (SSID) and password
- The ALX Nova reachable via its AP (see above) or an existing network connection

**Steps:**

1. Open the web interface and go to the **Network** tab.
2. Click the **Scan** button (magnifying glass icon) to see nearby networks.
3. Select your network from the dropdown list.
4. Enter your WiFi password.
5. Click **Connect**.
6. The controller will attempt to join the network. A progress indicator shows the connection attempt.
7. On success, the current IP address appears in the Status section.

:::tip
The controller cycles through all saved networks in priority order on each connection attempt, so if your primary router is unavailable it will automatically try the next saved network.
:::

:::warning
The ESP32-P4's onboard WiFi (via the ESP32-C6 co-processor) supports **2.4 GHz and 5 GHz** networks. Most home routers broadcast both. If you have trouble connecting, try selecting the 2.4 GHz network name specifically — it typically has better range through walls.
:::

### Saving Multiple Networks

ALX Nova can remember up to **5 WiFi networks**. This is useful if your controller moves between locations (workshop, listening room, rack) or if your router has separate 2.4 GHz and 5 GHz SSIDs.

1. Connect to the first network as described above.
2. Return to the **Network** tab.
3. Scan again and select a second network.
4. Enter the password and click **Add to saved networks**.
5. Drag entries in the saved network list to reorder them — higher entries are tried first.

### Access Point Mode

AP mode lets you connect to the ALX Nova directly — useful for setup, for reaching the device when your main router is offline, or for connecting a phone at a listening location without network coverage.

**Enabling AP Mode:**
- **From the web UI**: Toggle the **Access Point** switch on the Network tab.
- **From the physical button**: Double-click the button on the controller (see [Button Controls](./button-controls.md)).

**AP Settings:**

| Setting | Default |
|---|---|
| SSID | ALX-\{serial\} |
| Password | 12345678 |
| IP address | 192.168.4.1 |

You can change the SSID and password from the Network tab. The password must be at least 8 characters.

:::warning
Change the default AP password if the controller is used in a shared or public environment. Anyone who knows the default password can connect to the AP and access the web interface.
:::

**STA + AP Simultaneously:**

When both STA and AP are active at the same time, the controller is reachable on both IP addresses — its home-network IP (e.g., `192.168.1.45`) and the AP gateway (`192.168.4.1`). This is the default behaviour when you enable AP mode after already being connected to a network.

---

## Network Failover

ALX Nova manages Ethernet and WiFi together with automatic failover:

| Scenario | Active Interface | Status Bar |
|---|---|---|
| Ethernet only | ETH | **ETH** |
| WiFi only | WiFi | **WiFi** |
| Both connected | ETH (primary), WiFi (backup) | **Net** |
| Ethernet cable removed | WiFi | **WiFi** |

The failover is automatic and requires no configuration. When Ethernet is preferred (cable connected), OTA updates, MQTT, and API traffic all route over Ethernet. WiFi remains associated so the device remains reachable from the local network via both interfaces simultaneously.

---

## Finding Your Device After a Network Change

If you change your router or the device gets a new IP address:

- Check your **router's DHCP client list** — look for the configured hostname (default: `alx-nova`) or a device named `ALX-`.
- **Enable AP mode** using a double-click of the physical button, then connect via `192.168.4.1`.
- Look at the **TFT Home screen** — the current IP address is displayed there when connected.
- If Ethernet is connected, the Ethernet IP is always shown in the **Ethernet Status** card even if the device's WiFi IP has changed.

---

## Connection Troubleshooting

See the [Troubleshooting](./troubleshooting.md) page for a full FAQ. Quick checks:

- Confirm the WiFi password is correct (passwords are case-sensitive).
- Make sure no MAC address filtering is active on your router, or add the controller's MAC to the allow list.
- Move the controller closer to the access point temporarily to rule out WiFi signal strength as the cause.
- If the controller keeps failing to connect via WiFi, try a factory reset and start the setup from scratch.
- For Ethernet issues, try a different cable and confirm the switch port has link activity lights.
