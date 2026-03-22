---
title: Getting Started
sidebar_position: 2
description: Set up your ALX Nova controller from first power-on to full configuration.
---

# Getting Started

This guide walks you through everything from unboxing your ALX Nova to having it connected to your network and ready to control your amplifier. Most users are up and running within ten minutes.

## What You Need

Before you begin, make sure you have the following on hand.

**Included with the ALX Nova:**
- The ALX Nova controller board (ESP32-P4)
- The controller is ready to use — no assembly required

**You will need to provide:**
- A 5V DC power supply capable of at least 1A (a quality USB-C power supply works well)
- A speaker amplifier with a relay-compatible enable input, or a relay module wired to your amplifier
- An audio source connected to the controller's ADC inputs
- A smartphone or laptop with a web browser for initial setup
- Your home WiFi network name and password (for connecting to your network)

**Optional but recommended:**
- An Ethernet cable — the ALX Nova supports wired Ethernet for a rock-solid connection when WiFi is not ideal

:::tip Save your WiFi password
Have your WiFi password ready before you start. You will need to enter it through the web interface during setup, and it is easiest when you have it at hand.
:::

---

## Step 1: First Power-On

1. Connect your power supply to the ALX Nova controller.
2. The controller will start up. If a TFT display is fitted, you will see the boot animation play on screen as the firmware initialises.
3. Within a few seconds the controller enters **Access Point mode** automatically, because no WiFi network has been configured yet.

:::info What is Access Point mode?
In Access Point (AP) mode, the ALX Nova broadcasts its own WiFi network so you can connect directly to it from your phone or laptop for initial configuration. Think of it like a setup hotspot.
:::

The TFT display (if fitted) will show the login password for the web interface. Keep it visible during setup.

---

## Step 2: Connect to the ALX Nova's WiFi Network

On your phone or laptop, open the WiFi settings and scan for nearby networks.

1. Look for a network named **ALX-XXXXXXXXXXXX** where the X characters are unique to your device's MAC address.
2. Select that network.
3. Enter the password: **12345678**
4. Wait for your device to connect. You may see a warning that the network has no internet access — this is expected and safe to dismiss.

:::warning Stay on the ALX network
Some phones will automatically switch back to a WiFi network that has internet access. If your browser shows a connection error, check that your phone is still connected to the ALX network and has not silently switched.
:::

---

## Step 3: Open the Web Interface

With your device connected to the ALX network, open a web browser and navigate to:

**http://192.168.4.1**

The ALX Nova login page will appear.

---

## Step 4: Log In for the First Time

The ALX Nova generates a unique random password on first boot. This keeps the device secure even before you have set your own password.

1. Find the login password — it is shown on the **TFT display** if one is fitted, and also printed to the serial output if you have a serial connection.
2. Enter that password in the login field on the web interface.
3. Click **Login**.

:::tip Can't see the password on the display?
If the display is off or you do not have a TFT display fitted, connect a USB serial cable and open a serial monitor at 115200 baud. The password is printed during boot as part of the startup log.
:::

:::info Password changes after factory reset
A new random password is generated any time the device is factory reset. After you log in successfully, you can change the password to something memorable in the Settings tab.
:::

---

## Step 5: Connect to Your Network

You can connect the ALX Nova to your network via WiFi or Ethernet — or both at the same time.

### Option A: WiFi Setup

1. Click the **Network** tab at the top of the page.
2. Click the scan button (shown as a search icon) to discover nearby networks.
3. Select your home network from the dropdown list.
4. Type your WiFi password into the password field.
5. Click **Connect**.

The controller will attempt to join your network. This takes about 10–15 seconds.

:::warning 2.4 GHz networks only
The ALX Nova's onboard WiFi operates on the 2.4 GHz band. If your router has separate 2.4 GHz and 5 GHz networks, make sure you select the 2.4 GHz one. The WiFi module does support WiFi 6 at 2.4 GHz.
:::

:::tip Save multiple networks
The ALX Nova can remember up to 5 WiFi networks. If you want the controller to work in different locations, add all the networks in the Network tab. It will automatically try each one in order on startup.
:::

### Option B: Ethernet Setup

For a wired connection, plug an Ethernet cable into the ESP32-P4's Ethernet port. No configuration is required — the controller configures itself via DHCP automatically and Ethernet becomes the preferred interface.

The **Network** tab shows both the Ethernet status and the WiFi status side by side. When Ethernet is connected, the status bar at the top of the page shows **ETH**.

:::tip Ethernet is recommended for reliability
A wired connection eliminates WiFi range and interference issues. If your equipment rack has network access, use Ethernet as the primary interface and leave WiFi as a backup.
:::

See the [Network Configuration](./wifi-configuration.md) page for static IP setup, hostname configuration, and auto failover details.

---

## Step 6: Find the Controller on Your Network

After joining your home network, the ALX Nova will no longer be accessible at 192.168.4.1. You need to find its new IP address on your local network.

There are several ways to do this:

- **Check your router's device list.** Log in to your router's admin page and look for a device named something like "ALX-Nova" or "ESP32" in the connected devices or DHCP client list. The IP address is shown alongside it.
- **Use the TFT display.** The IP address is shown on the controller's home screen once it has connected.
- **Double-click the physical button.** This toggles AP mode back on temporarily, letting you reconnect at 192.168.4.1 and check the status from there.

Once you have the IP address, type it into your browser to access the web interface on your home network — for example, **http://192.168.1.42**.

:::tip Assign a static IP address
To avoid the IP address changing over time, log in to your router and assign a static (reserved) IP address to the ALX Nova based on its MAC address. This makes it consistently reachable at the same address.
:::

---

## Step 7: What to Configure Next

You are now connected. Here is a quick overview of what to set up next, in a suggested order.

**Smart Sensing (Control tab)**
Configure how the controller detects audio signals and manages your amplifier relay. You can choose between Always On, Always Off, or Smart Auto mode. See the [Smart Sensing](./smart-sensing.md) guide for full details.

**MQTT and Home Assistant (MQTT tab)**
If you use Home Assistant or another smart home platform with MQTT, configure your broker details here. The controller will auto-discover in Home Assistant once MQTT is enabled.

**Audio Routing (Audio tab)**
The ALX Nova supports up to 8 input lanes and an 8-output sink system with a 16x16 routing matrix and per-channel DSP. If you have multiple audio sources or outputs, configure the routing here.

**Security (Settings tab)**
Change the web interface password from the auto-generated one to something you will remember. You can also configure the screen timeout and display brightness here.

**Firmware Updates (Settings tab)**
Check that you are running the latest firmware. The controller can check GitHub releases and update itself over the air, with SHA256 integrity verification to ensure the firmware is genuine.

:::info Current firmware version
This documentation covers firmware version **1.12.1**. You can always see the running version in the Debug tab under Hardware Stats, or in the Settings tab.
:::

---

## Physical Button Quick Reference

The ALX Nova has a single physical button (GPIO 46) that gives you quick access to common functions without needing the web interface.

| Press Type | Duration | What It Does |
|---|---|---|
| Short press | Less than 0.5 seconds | Prints current device status to serial output |
| Double-click | Two quick presses | Toggles Access Point mode on or off |
| Triple-click | Three quick presses | Toggles the amplifier relay on or off |
| Long press | Hold for 2 seconds | Restarts the controller |
| Very long press | Hold for 10 seconds | Factory reset with visual countdown feedback |

:::danger Factory reset erases everything
A 10-second hold triggers a full factory reset. This erases all WiFi credentials, MQTT settings, sensing configuration, and your custom password. The controller returns to first-boot state. You can cancel by releasing the button before 10 seconds are up.
:::

---

## Troubleshooting First-Time Setup

**I cannot see the ALX-XXXXXXXXXXXX network.**
Wait 30 seconds after powering on and scan again. If still not visible, check the power supply is adequate (5V, 1A minimum).

**My phone keeps disconnecting from the ALX network.**
Some phones disconnect from networks without internet. Look for an option like "Keep WiFi connected" or "Use this network even without internet" in your phone's WiFi settings.

**The web page at 192.168.4.1 does not load.**
Make sure you are connected to the ALX network (not your home network) and using HTTP, not HTTPS. Type http://192.168.4.1 exactly.

**I cannot find the controller's IP address on my home network.**
Use the double-click button shortcut to re-enable AP mode, then connect back to the ALX network and check the Settings or Debug tab for the current IP address.

**The login password does not work.**
The password is case-sensitive. If you have a serial connection, the exact password is printed during boot. If in doubt, a factory reset (10-second button hold) generates a new one.
