---
title: WiFi Configuration
sidebar_position: 5
description: Connecting the ALX Nova to your WiFi network.
---

# WiFi Configuration

ALX Nova supports two wireless roles at the same time — it can connect to your home network as a regular client (**Station mode**, or STA) while simultaneously broadcasting its own access point (**AP mode**) for direct device access. It can also connect to a wired network via **Ethernet** if you prefer a cable.

---

## First Boot — Access Point Mode

On a fresh device with no WiFi credentials saved, the ALX Nova starts in AP mode automatically.

1. On your phone or laptop, open the WiFi settings and look for a network named **ALX-XXXXXXXXXXXX** (the Xs are your device's unique serial number, derived from its MAC address).
2. Connect using the default password: **12345678**
3. Open a browser and go to **http://192.168.4.1**
4. Log in — the one-time password is shown on the TFT display's boot screen and on the serial console.
5. Go to the **WiFi** tab and add your home network (see below).

:::info
The AP and the TFT display both show the same serial number suffix. If you have multiple ALX Nova devices, this lets you tell them apart.
:::

---

## Connecting to Your Home Network (Station Mode)

### What You Will Need

- Your WiFi network name (SSID) and password
- The ALX Nova reachable via its AP (see above) or an existing network connection

### Steps

1. Open the web interface and go to the **WiFi** tab.
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

---

## Saving Multiple Networks

ALX Nova can remember up to **5 WiFi networks**. This is useful if your controller moves between locations (workshop, listening room, rack) or if your router has separate 2.4 GHz and 5 GHz SSIDs.

1. Connect to the first network as described above.
2. Return to the **WiFi** tab.
3. Scan again and select a second network.
4. Enter the password and click **Add to saved networks**.
5. Drag entries in the saved network list to reorder them — higher entries are tried first.

---

## Access Point Mode

AP mode lets you connect to the ALX Nova directly — useful for setup, for reaching the device when your main router is offline, or for connecting a phone at a listening location without network coverage.

### Enabling AP Mode

- **From the web UI**: Toggle the **Access Point** switch on the WiFi tab.
- **From the physical button**: Double-click the button on the controller (see [Button Controls](./button-controls.md)).

### AP Settings

| Setting | Default |
|---|---|
| SSID | ALX-\{serial\} |
| Password | 12345678 |
| IP address | 192.168.4.1 |

You can change the SSID and password from the WiFi tab. The password must be at least 8 characters.

:::warning
Change the default AP password if the controller is used in a shared or public environment. Anyone who knows the default password can connect to the AP and access the web interface.
:::

### STA + AP Simultaneously

When both STA and AP are active at the same time, the controller is reachable on both IP addresses — its home-network IP (e.g. `192.168.1.45`) and the AP gateway (`192.168.4.1`). This is the default behaviour when you enable AP mode after already being connected to a network.

---

## Ethernet

If a network cable is connected to the ESP32-P4's Ethernet port, the controller will use both WiFi and Ethernet. Ethernet takes priority for internet access (OTA updates) while WiFi remains active for local clients.

The Ethernet status and IP address are shown at the top of the **WiFi** tab.

:::info
OTA update checks accept either WiFi or Ethernet connectivity. If the cable is plugged in and the internet is reachable, OTA checks will succeed even if WiFi is not configured.
:::

---

## Finding Your Device After a Network Change

If you change your router or the device gets a new IP address:

- Check your **router's DHCP client list** — look for a hostname starting with `ALX-`.
- **Enable AP mode** using a double-click of the physical button, then connect via `192.168.4.1`.
- Look at the **TFT Home screen** — the current IP address is displayed there when connected.

---

## Connection Troubleshooting

See the [Troubleshooting](./troubleshooting.md) page for a full FAQ. Quick checks:

- Confirm the password is correct (passwords are case-sensitive).
- Make sure no MAC address filtering is active on your router, or add the controller's MAC to the allow list.
- Move the controller closer to the access point temporarily to rule out signal strength as the cause.
- If the controller keeps failing to connect, try a factory reset and start the WiFi setup from scratch.
