#ifndef STRINGS_H
#define STRINGS_H

// ============================================================================
// UI Text Strings - Externalized for easy modification
// ============================================================================
// This file contains all user-visible text strings for the web interface.
// Edit strings here without modifying the HTML layout or JavaScript logic.
// ============================================================================

// ===== Page Titles =====
#define STR_PAGE_TITLE                      "ESP32-S3 LED Control"
#define STR_AP_PAGE_TITLE                   "ESP32 WiFi Setup"

// ===== Main Section Headings =====
#define STR_HEADING_MAIN                    "ESP32-S3 LED Control"
#define STR_HEADING_DEVICE_CONTROL          "Device Control Settings"
#define STR_HEADING_SMART_SENSING           "Smart Sensing On/Off"
#define STR_HEADING_WIFI_CONFIG             "WiFi Configuration"
#define STR_HEADING_FIRMWARE_UPDATE         "Firmware Update"
#define STR_HEADING_TIMEZONE                "Timezone Configuration"
#define STR_HEADING_MQTT                    "MQTT Configuration"
#define STR_HEADING_EXPORT_IMPORT           "Export/Import Settings"
#define STR_HEADING_REBOOT                  "Reboot Device"
#define STR_HEADING_FACTORY_RESET           "Factory Reset"
#define STR_HEADING_DEBUGGING               "Debugging"
#define STR_HEADING_HARDWARE_UTIL           "Hardware Utilization"
#define STR_HEADING_RELEASE_NOTES           "Release Notes"
#define STR_HEADING_AP_CONFIG               "Access Point Configuration"

// ===== Section Icons (Emojis) =====
#define STR_ICON_TIMEZONE                   "⏰"
#define STR_ICON_MQTT                       "📡"
#define STR_ICON_EXPORT_IMPORT              "💾"
#define STR_ICON_REBOOT                     "🔄"
#define STR_ICON_FACTORY_RESET              "⚠️"
#define STR_ICON_DEBUGGING                  "🔧"
#define STR_ICON_HARDWARE_UTIL              "📊"

// ===== Smart Sensing Options =====
#define STR_OPT_ALWAYS_ON                   "Always On"
#define STR_OPT_ALWAYS_OFF                  "Always Off"
#define STR_OPT_SMART_AUTO                  "Smart Auto Sensing"

// ===== Form Labels =====
#define STR_LABEL_TIMER_DURATION            "Auto-Off Timer Duration (minutes):"
#define STR_LABEL_VOLTAGE_THRESHOLD         "Voltage Detection Threshold (volts):"
#define STR_LABEL_TIME_REMAINING            "Time Remaining:"
#define STR_LABEL_AMPLIFIER_STATUS          "Amplifier Status:"
#define STR_LABEL_VOLTAGE_DETECTED          "Voltage Detected:"
#define STR_LABEL_CURRENT_READING           "Current Reading:"
#define STR_LABEL_SSID                      "Network Name (SSID):"
#define STR_LABEL_PASSWORD                  "Password:"
#define STR_LABEL_CURRENT_VERSION           "Current Version:"
#define STR_LABEL_LATEST_VERSION            "Latest Version:"
#define STR_LABEL_SELECT_TIMEZONE           "Select Timezone:"
#define STR_LABEL_CURRENT_OFFSET            "Current offset:"
#define STR_LABEL_BROKER_ADDRESS            "Broker Address:"
#define STR_LABEL_PORT                      "Port:"
#define STR_LABEL_USERNAME                  "Username (optional):"
#define STR_LABEL_PASSWORD_OPTIONAL         "Password (optional):"
#define STR_LABEL_BASE_TOPIC                "Base Topic:"
#define STR_LABEL_REFRESH                   "Refresh:"
#define STR_LABEL_AP_SSID                   "AP Network Name (SSID):"
#define STR_LABEL_AP_PASSWORD               "AP Password:"
#define STR_LABEL_ROOT_CA_CERT              "Root CA Certificate (PEM format):"
#define STR_LABEL_STATUS                    "Status:"

// ===== Toggle Labels =====
#define STR_TOGGLE_ESP32_AP                 "ESP32 Access Point"
#define STR_TOGGLE_AUTO_UPDATE              "Auto update on boot"
#define STR_TOGGLE_SSL_VALIDATION           "Enable SSL Certificate Validation"
#define STR_TOGGLE_ENABLE_MQTT              "Enable MQTT"
#define STR_TOGGLE_HA_DISCOVERY             "Enable Home Assistant Auto-Discovery"
#define STR_TOGGLE_AUTO_SCROLL              "Auto-scroll"

// ===== Button Labels =====
#define STR_BTN_TURN_ON                     "Turn On Now"
#define STR_BTN_TURN_OFF                    "Turn Off Now"
#define STR_BTN_STOP_BLINKING               "Stop Blinking"
#define STR_BTN_START_BLINKING              "Start Blinking"
#define STR_BTN_CONFIGURE_AP                "Configure Access Point"
#define STR_BTN_CONNECT_WIFI                "Connect to WiFi"
#define STR_BTN_VIEW_RELEASE_NOTES          "View Release Notes"
#define STR_BTN_CONFIGURE_SSL_CERT          "Configure SSL Certificate"
#define STR_BTN_SAVE_CERTIFICATE            "Save Certificate"
#define STR_BTN_RESET_TO_DEFAULT            "Reset to Default"
#define STR_BTN_CHECK_UPDATES               "Check for Updates"
#define STR_BTN_UPDATE_TO_LATEST            "Update to Latest Version"
#define STR_BTN_CANCEL_AUTO_UPDATE          "Cancel Auto-Update"
#define STR_BTN_SAVE_MQTT                   "Save MQTT Settings"
#define STR_BTN_DOWNLOAD_SETTINGS           "Download Settings File"
#define STR_BTN_HOLD_REBOOT                 "Hold for 2 Seconds to Reboot"
#define STR_BTN_HOLD_RESET                  "Hold for 3 Seconds to Reset"
#define STR_BTN_CLEAR                       "Clear"
#define STR_BTN_PAUSE                       "Pause"
#define STR_BTN_RESUME                      "Resume"
#define STR_BTN_SAVE_AP_CONFIG              "Save AP Configuration"

// ===== Placeholder Text =====
#define STR_PLACEHOLDER_SSID                "Enter WiFi SSID"
#define STR_PLACEHOLDER_PASSWORD            "Enter WiFi password"
#define STR_PLACEHOLDER_BROKER              "e.g., 192.168.1.100 or mqtt.example.com"
#define STR_PLACEHOLDER_LEAVE_EMPTY         "Leave empty if not required"
#define STR_PLACEHOLDER_BASE_TOPIC          "e.g., home/audio-controller"
#define STR_PLACEHOLDER_AP_SSID             "Enter AP SSID"
#define STR_PLACEHOLDER_AP_PASSWORD         "Enter AP password (min 8 characters)"
#define STR_PLACEHOLDER_CERTIFICATE         "-----BEGIN CERTIFICATE-----\\n...\\n-----END CERTIFICATE-----"

// ===== Status Text =====
#define STR_STATUS_ON                       "ON"
#define STR_STATUS_OFF                      "OFF"
#define STR_STATUS_YES                      "Yes"
#define STR_STATUS_NO                       "No"
#define STR_STATUS_CONNECTED                "Connected"
#define STR_STATUS_DISCONNECTED             "Disconnected"
#define STR_STATUS_LOADING                  "Loading..."
#define STR_STATUS_RECEIVING                "Receiving"
#define STR_STATUS_PAUSED                   "Paused"
#define STR_STATUS_BLINKING_ON              "Blinking: ON"
#define STR_STATUS_BLINKING_OFF             "Blinking: OFF"
#define STR_STATUS_AP_MODE                  "Access Point Mode"
#define STR_STATUS_NOT_CONNECTED            "Not Connected"
#define STR_STATUS_WAITING                  "Waiting for connection..."
#define STR_STATUS_USING_DEFAULT_CERT       "Using default certificate"
#define STR_STATUS_USING_CUSTOM_CERT        "Using custom certificate"
#define STR_STATUS_MQTT_CONNECTED           "Connected"
#define STR_STATUS_MQTT_DISCONNECTED        "Disconnected"
#define STR_STATUS_MQTT_DISABLED            "MQTT disabled"
#define STR_STATUS_NA                       "N/A"

// ===== Hardware Stats Labels =====
#define STR_STAT_MEMORY                     "Memory"
#define STR_STAT_HEAP                       "Heap"
#define STR_STAT_MIN_FREE                   "Min Free"
#define STR_STAT_MAX_BLOCK                  "Max Block"
#define STR_STAT_PSRAM                      "PSRAM"
#define STR_STAT_CPU                        "CPU"
#define STR_STAT_MODEL                      "Model"
#define STR_STAT_FREQUENCY                  "Frequency"
#define STR_STAT_CORES                      "Cores"
#define STR_STAT_TEMPERATURE                "Temperature"
#define STR_STAT_STORAGE                    "Storage"
#define STR_STAT_FLASH_SIZE                 "Flash Size"
#define STR_STAT_SKETCH_SIZE                "Sketch Size"
#define STR_STAT_OTA_FREE                   "OTA Free"
#define STR_STAT_SPIFFS                     "SPIFFS"
#define STR_STAT_WIFI                       "WiFi"
#define STR_STAT_RSSI                       "RSSI"
#define STR_STAT_CHANNEL                    "Channel"
#define STR_STAT_AP_CLIENTS                 "AP Clients"
#define STR_STAT_UPTIME                     "UPTIME:"

// ===== Signal Quality Labels =====
#define STR_SIGNAL_EXCELLENT                "Excellent"
#define STR_SIGNAL_GOOD                     "Good"
#define STR_SIGNAL_FAIR                     "Fair"
#define STR_SIGNAL_POOR                     "Poor"

// ===== Informational Text =====
#define STR_INFO_CERT_DESCRIPTION           "Configure the root CA certificate used for secure connections to GitHub. Update this if the current certificate expires or GitHub changes their certificate chain."
#define STR_INFO_MQTT_DESCRIPTION           "Connect to an MQTT broker to control this device remotely. Enable Home Assistant auto-discovery for seamless integration."
#define STR_INFO_EXPORT_DESCRIPTION         "Export all device settings to a JSON file for backup or transfer to another device."
#define STR_INFO_IMPORT_DESCRIPTION         "Import settings from a previously exported JSON file. The device will reboot after import."
#define STR_INFO_REBOOT_DESCRIPTION         "This will restart the ESP32. All settings will be preserved. The device will reconnect to WiFi automatically."
#define STR_INFO_DEBUG_DESCRIPTION          "Real-time terminal output from the ESP32. Messages are streamed via WebSocket when connected."

// ===== Warning Text =====
#define STR_WARNING_FACTORY_RESET           "This will erase all settings including WiFi credentials, timezone, auto-update preferences, and Smart Sensing configuration. The device will reboot and start in Access Point mode."
#define STR_WARNING_AP_PASSWORD             "Setting or changing password will cause device's AP clients to disconnect!"

// ===== Drop Zone Text =====
#define STR_DROP_ZONE_TEXT                  "Drag & Drop Settings File Here"
#define STR_DROP_ZONE_HINT                  "or click to browse"

// ===== Timezone Options =====
#define STR_TZ_UTC                          "UTC (GMT+0:00)"
#define STR_TZ_CET                          "CET - Central European Time (GMT+1:00)"
#define STR_TZ_CEST                         "CEST - Central European Summer Time (GMT+2:00)"
#define STR_TZ_EST                          "EST - Eastern Standard Time (GMT-5:00)"
#define STR_TZ_EDT                          "EDT - Eastern Daylight Time (GMT-4:00)"
#define STR_TZ_CST_US                       "CST - Central Standard Time (GMT-6:00)"
#define STR_TZ_CDT                          "CDT - Central Daylight Time (GMT-5:00)"
#define STR_TZ_MST                          "MST - Mountain Standard Time (GMT-7:00)"
#define STR_TZ_MDT                          "MDT - Mountain Daylight Time (GMT-6:00)"
#define STR_TZ_PST                          "PST - Pacific Standard Time (GMT-8:00)"
#define STR_TZ_PDT                          "PDT - Pacific Daylight Time (GMT-7:00)"
#define STR_TZ_CST_CHINA                    "CST - China Standard Time (GMT+8:00)"
#define STR_TZ_JST                          "JST - Japan Standard Time (GMT+9:00)"
#define STR_TZ_AEST                         "AEST - Australian Eastern Standard Time (GMT+10:00)"
#define STR_TZ_AEDT                         "AEDT - Australian Eastern Daylight Time (GMT+11:00)"
#define STR_TZ_NZST                         "NZST - New Zealand Standard Time (GMT+12:00)"
#define STR_TZ_NZDT                         "NZDT - New Zealand Daylight Time (GMT+13:00)"

// ===== Refresh Rate Options =====
#define STR_REFRESH_1S                      "1s"
#define STR_REFRESH_3S                      "3s"
#define STR_REFRESH_5S                      "5s"
#define STR_REFRESH_10S                     "10s"

// ===== Tooltip/Title Text =====
#define STR_TITLE_THEME_TOGGLE              "Toggle Day/Night Mode"
#define STR_TITLE_SWITCH_DAY                "Switch to Day Mode"
#define STR_TITLE_SWITCH_NIGHT              "Switch to Night Mode"

// ===== JavaScript Alert/Error Messages =====
#define STR_JS_ERR_SETTINGS_FAILED          "Failed to update settings: "
#define STR_JS_ERR_SETTINGS_ERROR           "Error updating settings: "
#define STR_JS_ERR_CERT_VALIDATION          "Failed to update certificate validation setting: "
#define STR_JS_ERR_CERT_VALIDATION_ERROR    "Error updating certificate validation setting: "
#define STR_JS_ERR_CERT_EMPTY               "Please enter a certificate"
#define STR_JS_ERR_CERT_INVALID             "Invalid certificate format. Must include BEGIN and END markers."
#define STR_JS_ERR_CERT_SAVE_FAILED         "Failed to save certificate: "
#define STR_JS_ERR_CERT_SAVE_ERROR          "Error saving certificate: "
#define STR_JS_ERR_CERT_RESET_FAILED        "Failed to reset certificate: "
#define STR_JS_ERR_CERT_RESET_ERROR         "Error resetting certificate: "
#define STR_JS_ERR_TZ_FAILED                "Failed to update timezone: "
#define STR_JS_ERR_TZ_ERROR                 "Error updating timezone: "
#define STR_JS_ERR_BROKER_REQUIRED          "Please enter a broker address"
#define STR_JS_ERR_FILE_SELECT              "Error: Please select a JSON file"
#define STR_JS_ERR_FILE_INVALID             "Error: Invalid settings file format"
#define STR_JS_ERR_FILE_PARSE               "Error: Failed to parse JSON file - "
#define STR_JS_ERR_FILE_READ                "Error: Failed to read file"
#define STR_JS_ERR_IMPORT_FAILED            "Import failed: "
#define STR_JS_ERR_REBOOT_FAILED            "Reboot failed: "
#define STR_JS_ERR_REBOOT_ERROR             "Error performing reboot: "
#define STR_JS_ERR_RESET_FAILED             "Factory reset failed: "
#define STR_JS_ERR_RESET_ERROR              "Error performing factory reset: "
#define STR_JS_ERR_WIFI_REQUIRED            "Please enter both SSID and password"
#define STR_JS_ERR_WIFI_ERROR               "Error: "
#define STR_JS_ERR_UPDATE_ERROR             "Error: "

// ===== JavaScript Success Messages =====
#define STR_JS_SUCCESS_CERT_SAVED           "Certificate saved successfully!"
#define STR_JS_SUCCESS_CERT_RESET           "Certificate reset to default!"
#define STR_JS_SUCCESS_TZ_UPDATED           "Timezone updated successfully! Time will be re-synchronized."
#define STR_JS_SUCCESS_MQTT_SAVED           "Settings saved!"
#define STR_JS_SUCCESS_SETTINGS_IMPORTED    "Settings imported successfully!"
#define STR_JS_SUCCESS_WIFI_SAVED           "WiFi configuration saved! Connecting..."
#define STR_JS_SUCCESS_IMPORT_STARTED       "Import started. Device is rebooting..."

// ===== JavaScript Status Messages =====
#define STR_JS_STATUS_SAVING                "Saving..."
#define STR_JS_STATUS_IMPORTING             "Importing settings..."
#define STR_JS_STATUS_KEEP_HOLDING          "Keep Holding..."
#define STR_JS_STATUS_REBOOTING             "Rebooting..."
#define STR_JS_STATUS_RESETTING             "Resetting..."
#define STR_JS_STATUS_RESET_COMPLETE        "Reset Complete! Rebooting..."
#define STR_JS_STATUS_REBOOTING_DEVICE      "Rebooting Device..."
#define STR_JS_STATUS_DEVICE_REBOOTING      "Device is rebooting. The page will reconnect automatically."
#define STR_JS_STATUS_CHECKING_UPDATES      "Checking for updates..."
#define STR_JS_STATUS_STARTING_OTA          "Starting OTA update... This may take a few minutes."
#define STR_JS_STATUS_WAITING_DEVICE        "Waiting for device to come back online..."
#define STR_JS_STATUS_FILE_SELECTED         "Selected: "
#define STR_JS_STATUS_IMPORT_CANCELLED      "Import cancelled"

// ===== JavaScript Confirmation Messages =====
#define STR_JS_CONFIRM_CERT_RESET           "Are you sure you want to reset to the default certificate?"
#define STR_JS_CONFIRM_OTA_UPDATE           "This will download and install the new firmware. The device will restart automatically. Continue?"

// ===== JavaScript Dynamic Messages (with placeholders) =====
#define STR_JS_REBOOTING_IN                 "Settings imported successfully! Rebooting in "
#define STR_JS_REBOOTING_IN_SUFFIX          " seconds..."
#define STR_JS_PHYSICAL_BUTTON              "Physical Button: "
#define STR_JS_PHYSICAL_RESET_TRIGGERED     "Physical Reset Triggered! Rebooting..."
#define STR_JS_PHYSICAL_RESET_MSG           "Factory reset triggered by physical button. The device will reboot and start in Access Point mode."
#define STR_JS_PHYSICAL_REBOOT_TRIGGERED    "Physical Reboot Triggered! Rebooting..."
#define STR_JS_PHYSICAL_REBOOT_MSG          "Reboot triggered by physical button. The device will restart shortly."
#define STR_JS_FACTORY_RESET_MSG            "Factory reset complete. The device will reboot and start in Access Point mode. You will need to reconnect."
#define STR_JS_UPDATE_AVAILABLE             "Update available! You can view the release notes or click update to install."
#define STR_JS_UP_TO_DATE                   "You have the latest version installed."
#define STR_JS_PAUSED_BUFFERED              "Paused (%d buffered)"

// ===== Connection Status =====
#define STR_CONN_CONNECTED_SYMBOL           "●"
#define STR_CONN_CONNECTED                  "● Connected"
#define STR_CONN_DISCONNECTED               "● Disconnected"

// ===== AP Setup Page Strings =====
#define STR_AP_HEADING                      "WiFi Setup"
#define STR_AP_DESCRIPTION                  "Connect to your home WiFi network"
#define STR_AP_CURRENT_CONFIG               "Current Configuration"
#define STR_AP_MODE                         "Mode:"
#define STR_AP_AP_MODE_ONLY                 "AP Mode Only"
#define STR_AP_IP                           "IP:"
#define STR_AP_NEW_WIFI_CONFIG              "New WiFi Configuration"
#define STR_AP_BTN_CONNECT                  "Connect to WiFi"
#define STR_AP_STATUS_CONNECTING            "Connecting..."

// ===== Modal Close Button =====
#define STR_MODAL_CLOSE                     "&times;"

// ===== Loading Text =====
#define STR_LOADING_RELEASE_NOTES           "Loading release notes..."

// ===== Misc Labels =====
#define STR_LABEL_STRONG                    "strong"
#define STR_LABEL_INFO                      "INFO:"
#define STR_LABEL_WARNING                   "WARNING:"
#define STR_LABEL_MANUFACTURER              "Manufacturer:"
#define STR_LABEL_MODEL_DEVICE              "Model:"
#define STR_LABEL_SERIAL                    "Serial:"
#define STR_LABEL_FIRMWARE                  "Firmware:"
#define STR_LABEL_SIGNAL                    "Signal:"
#define STR_LABEL_MAC                       "MAC:"
#define STR_LABEL_IP                        "IP:"
#define STR_LABEL_SSID_DISPLAY              "SSID:"
#define STR_LABEL_CONNECTED_TO              "Connected to:"

#endif // STRINGS_H
