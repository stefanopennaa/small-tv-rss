// =====================================================================
// File: wifi_secrets_template.h
// Purpose: Template for WiFi and OTA credentials
// Changelog (latest first):
//   - 2026.05.03: Header/comment structure normalized (format-only update)
// =====================================================================

#pragma once

// =====================================================================
// WiFi Network Credentials Configuration
// =====================================================================
//
// Setup Instructions:
// 1. Copy this file to "wifi_secrets.h" in the same directory
// 2. Replace "YOUR_WIFI_SSID" with your WiFi network name
// 3. Replace "YOUR_WIFI_PASSWORD" with your WiFi password
// 4. (Optional but recommended) Set OTA username/password to protect /update
//
// Security Note: wifi_secrets.h is intentionally excluded from git
//                (.gitignore) to protect your network credentials
//                from being committed to version control
// =====================================================================

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define OTA_USERNAME "YOUR_OTA_USERNAME"
#define OTA_PASSWORD "YOUR_OTA_PASSWORD"
