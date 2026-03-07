// =====================================================================
// SmallTV Firmware - ESP8266 Weather Clock with RSS Feed Display
// =====================================================================
// Version: 2026.03.07 (Improved)
//
// Hardware: GeekMagic SmallTV (ESP8266 + ST7789 240x240 TFT)
//
// Main Features:
// - Real-time clock with NTP sync (Italy timezone: CET/CEST with DST)
// - Live weather data from OpenWeatherMap API (temperature, humidity, conditions)
// - RSS news feed from ANSA (Italian news agency)
// - Responsive web interface for monitoring and control
// - Adjustable backlight brightness via web UI
// - Over-The-Air (OTA) firmware updates via ElegantOTA
// - Secure credential management (WiFi and API keys in separate files)
// - Optimized boot sequence with timeout protection
// - Smooth animations during WiFi connection and NTP synchronization
// - Scrolling marquee text display
// - Flash-optimized storage (all icons and HTML in PROGMEM)
//
// Architecture:
// 1) setup()   - Hardware initialization, WiFi connection, NTP sync, web server startup
// 2) loop()    - Non-blocking event handlers (WiFi/NTP retry, data refresh, UI updates)
// 3) Rendering - Alternates between home screen and paged news scene
//
// Code Quality Improvements (v2026.03.07):
// - Replaced magic numbers with named constants for maintainability
// - Added input validation on HTTP endpoints (security hardening)
// - Implemented size limits on HTTP responses (buffer overflow protection)
// - Added JSON key validation before data access (crash prevention)
// - Refactored duplicate animation code into reusable function
// - Created time interval helper to reduce code duplication
// - Improved variable naming consistency (weather data structure)
// - Enhanced error handling throughout network operations
// - Added bounds checking on array access operations
// - Implemented safe string formatting with buffer size checks
// =====================================================================

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <time.h>
#include <pgmspace.h>
#include <string.h>

#include "config.h"

// Web UI HTML page stored in flash memory
#include "index_html.h"

// Icon assets (stored in flash memory to save RAM):
#include "icons/wifi/icons_1bit.h"   // WiFi connection animation (32x32, 1-bit monochrome)
#include "icons/sync/icons_1bit.h"   // NTP sync spinner animation (32x32, 1-bit monochrome)
#include "icons/mm/mm_rgb565.h"      // Weather condition icon (80x80, RGB565 color)
#include "icons/temp/temp_rgb565.h"  // Temperature indicator (12x32, RGB565 color)
#include "icons/temp/humi_rgb565.h"  // Humidity indicator (22x32, RGB565 color)
#include "icons/rss/rss_rgb565.h"    // RSS feed icon (32x32, RGB565 color)

// WiFi Credentials
// Include wifi_secrets.h if it exists (not tracked by git)
// Otherwise use empty defaults (device will boot in degraded mode)
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// External API Keys
// Include secrets.h if it exists (not tracked by git)
// Without API keys, weather data will not be available
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef OWM_API_KEY
#define OWM_API_KEY ""
#endif

// Hardware instances
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);  // ST7789 240x240 TFT display driver
ESP8266WebServer server(80);                                     // HTTP server on port 80

// UI Text Strings Structure
// Centralizes all user-visible text to simplify localization and reduce string literals in code
struct UiText {
  const char* wifi;         // WiFi connection in progress
  const char* connected;    // WiFi successfully connected
  const char* ntpSync;      // NTP synchronization in progress
  const char* synced;       // NTP sync successful
  const char* updating;     // OTA firmware update in progress
  const char* updated;      // OTA update completed successfully
  const char* failed;       // Operation failed
  const char* ipPrefix;     // IP address label prefix
  const char* wifiMissing;  // WiFi credentials not configured
  const char* wifiOffline;  // WiFi not connected
  const char* wifiTimeout;  // WiFi connection timeout
};

// UI text constants (stored in flash memory)
constexpr UiText UI = {
  "WiFi...",
  "Connected!",
  "NTP sync...",
  "Synced!",
  "Updating...",
  "Updated!",
  "Failed!",
  "IP: ",
  "WiFi cfg missing",
  "WiFi offline",
  "WiFi timeout"
};

// Boot State Machine
// Tracks the current initialization phase of the device
enum class BootState : uint8_t {
  BOOT_WIFI,     // Attempting to connect to WiFi network
  BOOT_NTP,      // Synchronizing time with NTP servers
  BOOT_READY,    // Fully operational (WiFi + NTP successful)
  BOOT_DEGRADED  // Operating with limited functionality (WiFi/NTP failed)
};

// WiFi Connection Result
// Return status from WiFi connection attempts
enum class WiFiConnectResult : uint8_t {
  Connected,           // Successfully connected to WiFi
  MissingCredentials,  // SSID or password not configured
  Timeout              // Connection timeout exceeded
};

// ===== Global State Variables =====

// System State
BootState bootState = BootState::BOOT_WIFI;                     // Current boot/initialization phase
WiFiConnectResult lastWiFiResult = WiFiConnectResult::Timeout;  // Result of last WiFi connection attempt
bool ntpSynced = false;                                         // True when NTP time sync is successful
bool otaInProgress = false;                                     // True during OTA firmware update
unsigned long bootMs = 0;                                       // Timestamp when device booted

// Network Retry Timers
unsigned long lastWiFiRetry = 0;  // Last WiFi reconnection attempt
unsigned long lastNtpRetry = 0;   // Last NTP sync retry

// Data Refresh Timers
unsigned long lastWeatherFetch = 0;   // Last weather data fetch attempt
unsigned long lastNewsFetch = 0;      // Last RSS feed fetch attempt
unsigned long lastWeatherUpdate = 0;  // Last successful weather API call
unsigned long lastNewsUpdate = 0;     // Last successful news feed fetch
bool initialDataFetched = false;      // True after first data fetch completes

// Display State
bool showNews = false;                // Toggle between clock and news scenes
int currentNewsIndex = 0;             // Currently displayed news item index
unsigned long lastDisplay = 0;        // Scene switching timer
unsigned long lastClockRefresh = 0;   // Clock refresh timer (while in clock scene)
int lastMinute = -1;                  // Last displayed minute (avoids redundant clock redraws)
unsigned long lastMarqueeUpdate = 0;  // Marquee animation timer
int16_t marqueeX = SCREEN_W;          // Current X position of scrolling text
char marqueeMessage[32] = "Torino";   // Scrolling marquee text content

// Display Brightness
// Value range: 0-255 (automatically inverted for PWM since this panel uses inverted backlight control)
int currentBrightness = 50;

// Weather Data (from OpenWeatherMap API)
float weatherTemp = 0;         // Temperature in Celsius
int weatherHumidity = 0;       // Relative humidity percentage
String weatherDesc = "";       // Weather condition description (e.g., "Partly cloudy")
String lastWeatherError = "";  // Last error message from weather API

// RSS News Feed Data
String newsTitles[NEWS_MAX];  // News headlines from ANSA feed
String newsLinks[NEWS_MAX];   // URLs for each news item
int lastNewsHttpCode = 0;     // HTTP response code from last feed fetch
String lastNewsError = "";    // Error message from last fetch attempt
int lastNewsCount = 0;        // Number of items successfully parsed

// =====================================================================
// Utility Helper Functions
// =====================================================================

// hasIntervalPassed()
// Checks if a time interval has elapsed and updates the last time marker
// Reduces code duplication for periodic task scheduling
// Parameters:
//   lastTime - Reference to timestamp of last event
//   interval - Minimum interval that must pass (milliseconds)
//   now      - Current time from millis()
// Returns: true if interval has passed, false otherwise
inline bool hasIntervalPassed(unsigned long& lastTime,
                              unsigned long interval,
                              unsigned long now) {
  if (now - lastTime >= interval) {
    lastTime = now;
    return true;
  }

  return false;
}

// validateBrightnessInput()
// Validates brightness value from HTTP input
// Parameters:
//   input - String containing brightness value
//   output - Reference to store validated integer value
// Returns: true if valid (0-255), false otherwise
bool validateBrightnessInput(const String& input, int& output) {
  // Check length is reasonable (1-3 digits)
  if (input.length() == 0 || input.length() > 3) {
    return false;
  }

  // Verify all characters are numeric
  for (size_t i = 0; i < input.length(); i++) {
    if (!isdigit(input[i])) {
      return false;
    }
  }

  // Convert and validate range
  output = input.toInt();
  return (output >= 0 && output <= 255);
}

// =====================================================================
// Display Helper Functions
// =====================================================================

// showStatus()
// Displays a full-screen status message with customizable color and position
// Used during boot sequence, WiFi connection, NTP sync, and OTA updates
// Parameters:
//   msg   - Text message to display
//   color - Text color (RGB565 format)
//   x, y  - Screen coordinates for text positioning
void showStatus(const String& msg,
                uint16_t color = ST77XX_WHITE,
                int16_t x = STATUS_TEXT_X,
                int16_t y = STATUS_TEXT_Y) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(x, y);
  tft.print(msg);
}

// drawRGB565_P()
// Renders a color icon stored in flash memory (PROGMEM) to the display
// Uses line-by-line buffering to minimize RAM usage during rendering
// Parameters:
//   x, y  - Screen coordinates for top-left corner of icon
//   w, h  - Icon dimensions (width and height in pixels)
//   icon  - Pointer to RGB565 pixel data in PROGMEM
// Note: Icons wider than RGB565_LINE_MAX pixels will be clipped to prevent buffer overflow
void drawRGB565_P(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* icon) {
  // Clip width to prevent buffer overflow
  if (w > RGB565_LINE_MAX) {
    w = RGB565_LINE_MAX;
  }

  uint16_t line[RGB565_LINE_MAX];

  // Render icon line by line to minimize RAM usage
  for (int16_t row = 0; row < h; row++) {
    // Copy one line from PROGMEM to RAM buffer
    for (int16_t col = 0; col < w; col++) {
      line[col] = pgm_read_word(&icon[row * w + col]);
    }

    // Draw the buffered line to display
    tft.drawRGBBitmap(x, y + row, line, w, 1);
  }
}

// =====================================================================
// XML/RSS Parsing Functions
// =====================================================================

// extractTag()
// Extracts text content from an XML tag
// Handles CDATA sections automatically (removes <![CDATA[ and ]]> markers)
// Parameters:
//   src - Source XML string
//   tag - Tag name to search for (without < > brackets)
// Returns: Extracted and trimmed text content, or empty string if tag not found
String extractTag(const String& src, const char* tag) {
  String openTag = String("<") + tag + ">";
  String closeTag = String("</") + tag + ">";

  // Find opening tag
  int start = src.indexOf(openTag);
  if (start < 0) {
    return "";
  }
  start += openTag.length();

  // Find closing tag
  int end = src.indexOf(closeTag, start);
  if (end < 0) {
    return "";
  }

  // Extract content between tags
  String out = src.substring(start, end);

  // Remove CDATA markers if present
  out.replace("<![CDATA[", "");
  out.replace("]]>", "");
  out.trim();

  return out;
}

// =====================================================================
// JSON Utility Functions
// =====================================================================

// jsonEscape()
// Escapes special characters in a string for safe JSON output
// Handles: backslashes, quotes, newlines, tabs, and other control characters
// Parameters:
//   s - Input string to escape
// Returns: JSON-safe escaped string
String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + JSON_ESCAPE_BUFFER_MARGIN);

  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];

    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;

      default:
        // Replace control characters with space
        if ((uint8_t)c < 0x20) {
          out += ' ';
        } else {
          out += c;
        }
        break;
    }
  }

  return out;
}

// =====================================================================
// Enum to String Conversion Functions (for API responses)
// =====================================================================

// bootStateToString()
// Converts BootState enum to human-readable string for API diagnostics
// Used by the /api endpoint to report device initialization status
const char* bootStateToString(BootState state) {
  switch (state) {
    case BootState::BOOT_WIFI: return "boot_wifi";
    case BootState::BOOT_NTP: return "boot_ntp";
    case BootState::BOOT_READY: return "ready";
    case BootState::BOOT_DEGRADED: return "degraded";
    default: return "unknown";
  }
}

// wifiResultToString()
// Converts WiFiConnectResult enum to human-readable string for API diagnostics
// Used by the /api endpoint to report WiFi connection status
const char* wifiResultToString(WiFiConnectResult result) {
  switch (result) {
    case WiFiConnectResult::Connected: return "connected";
    case WiFiConnectResult::MissingCredentials: return "missing_credentials";
    case WiFiConnectResult::Timeout: return "timeout";
    default: return "unknown";
  }
}

// =====================================================================
// Hardware Control Functions
// =====================================================================

// setBrightness()
// Sets the TFT backlight brightness level
// Parameters:
//   value - Brightness level (0-255, where 0=off and 255=maximum)
// Note: PWM signal is inverted because this display panel uses inverted backlight control
void setBrightness(int value) {
  currentBrightness = constrain(value, 0, 255);
  analogWrite(TFT_BACKLIGHT, 255 - currentBrightness);
}

// =====================================================================
// Network Connection Functions
// =====================================================================

// connectWiFi()
// Attempts to connect to WiFi network with animated visual feedback
// Displays connection status and animates WiFi icon during connection attempt
// Blocks execution but has built-in timeout protection (WIFI_CONNECT_TIMEOUT_MS)
// Returns: WiFiConnectResult indicating success, missing credentials, or timeout
// Note: Uses WiFi.persistent(false) to avoid excessive flash wear
WiFiConnectResult ICACHE_FLASH_ATTR connectWiFi() {
  if (strlen(WIFI_SSID) == 0) {
    showStatus(UI.wifiMissing, ST77XX_RED, 15, 110);
    delay(WIFI_STATUS_DELAY_MS);
    lastWiFiResult = WiFiConnectResult::MissingCredentials;
    return lastWiFiResult;
  }

  showStatus(UI.wifi, ST77XX_WHITE, 78, 135);
  WiFi.persistent(false);  // Avoid flash writes on reconnect loops.
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI, WIFI_W, WIFI_H, ST77XX_WHITE, BG_COLOR);
  unsigned long start = millis();
  unsigned long lastStep = millis();
  uint8_t frameIndex = 0;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    if (millis() - lastStep < ANIMATION_FRAME_INTERVAL_MS) {
      delay(1);
      continue;
    }

    lastStep = millis();
    switch (frameIndex) {
      case 0:
        tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI_1, WIFI_1_W, WIFI_1_H, ST77XX_WHITE, BG_COLOR);
        break;
      case 1:
        tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI_2, WIFI_2_W, WIFI_2_H, ST77XX_WHITE, BG_COLOR);
        break;
      case 2:
        tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI_3, WIFI_3_W, WIFI_3_H, ST77XX_WHITE, BG_COLOR);
        break;
      default:
        tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI, WIFI_W, WIFI_H, ST77XX_WHITE, BG_COLOR);
        break;
    }
    frameIndex = (frameIndex + 1) & 0x03;
  }

  const bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    showStatus(UI.connected, ST77XX_GREEN, 60, 110);
    delay(WIFI_CONNECTED_DELAY_MS);
    showStatus(String(UI.ipPrefix) + WiFi.localIP().toString(), ST77XX_WHITE, 18, 110);
    delay(WIFI_STATUS_DELAY_MS);
    lastWiFiResult = WiFiConnectResult::Connected;
  } else {
    showStatus(UI.wifiTimeout, ST77XX_YELLOW, 40, 110);
    delay(WIFI_STATUS_DELAY_MS);
    lastWiFiResult = WiFiConnectResult::Timeout;
  }

  return lastWiFiResult;
}

// syncNTP()
// Synchronizes system time with NTP servers
// Configures Italy timezone with automatic DST handling (CET/CEST transition)
// Shows animated sync icon during the synchronization process
// Blocks execution but has timeout protection (NTP_SYNC_TIMEOUT_MS)
// Returns: true if sync successful, false otherwise
// Note: Animation is visible for at least NTP_MIN_SYNC_ANIM_MS to provide user feedback
bool ICACHE_FLASH_ATTR syncNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    ntpSynced = false;
    return false;
  }

  showStatus(UI.ntpSync, ST77XX_WHITE, 54, 135);

  // Configure NTP servers and Italy timezone (CET-1CEST with DST)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  const unsigned long MIN_SYNC_MS = NTP_MIN_SYNC_ANIM_MS;
  unsigned long syncStart = millis();
  unsigned long lastStep = millis();
  uint8_t frameIndex = 0;

  // Initial sync icon frame
  tft.drawBitmap(SYNC_ICON_X, SYNC_ICON_Y, SYNC_1, SYNC_1_W, SYNC_1_H,
                 ST77XX_WHITE, BG_COLOR);

  // Wait for NTP sync with animated feedback
  // Condition: time must be valid (>100000 unix epoch) AND minimum animation shown
  while ((time(nullptr) < 100000 || (millis() - syncStart) < MIN_SYNC_MS) && (millis() - syncStart) < NTP_SYNC_TIMEOUT_MS) {

    // Frame rate limiting
    if (millis() - lastStep < ANIMATION_FRAME_INTERVAL_MS) {
      delay(1);
      continue;
    }

    lastStep = millis();

    // Cycle through sync animation frames
    switch (frameIndex) {
      case 0:
        tft.drawBitmap(SYNC_ICON_X, SYNC_ICON_Y, SYNC_2, SYNC_2_W, SYNC_2_H,
                       ST77XX_WHITE, BG_COLOR);
        break;
      case 1:
        tft.drawBitmap(SYNC_ICON_X, SYNC_ICON_Y, SYNC_3, SYNC_3_W, SYNC_3_H,
                       ST77XX_WHITE, BG_COLOR);
        break;
      case 2:
        tft.drawBitmap(SYNC_ICON_X, SYNC_ICON_Y, SYNC_4, SYNC_4_W, SYNC_4_H,
                       ST77XX_WHITE, BG_COLOR);
        break;
      default:
        tft.drawBitmap(SYNC_ICON_X, SYNC_ICON_Y, SYNC_1, SYNC_1_W, SYNC_1_H,
                       ST77XX_WHITE, BG_COLOR);
        break;
    }

    frameIndex = (frameIndex + 1) & 0x03;  // Cycle 0-3
  }

  // Check if sync was successful (valid unix timestamp)
  ntpSynced = (time(nullptr) >= 100000);

  // Show result status
  showStatus(ntpSynced ? UI.synced : UI.failed,
             ntpSynced ? ST77XX_GREEN : ST77XX_RED,
             78, 110);
  delay(NTP_STATUS_DELAY_MS);
  tft.fillScreen(BG_COLOR);

  return ntpSynced;
}

// =====================================================================
// Data Fetching Functions
// =====================================================================

// fetchWeather()
// Fetches current weather data from OpenWeatherMap API
// Updates global variables: weatherTemp, weatherHumidity, weatherDesc, lastWeatherUpdate
// Silently fails if WiFi is disconnected or API key is missing
// Uses short timeout (WEATHER_HTTP_TIMEOUT_MS) to keep UI responsive
// Note: Coordinates are configured in config.h (OWM_LAT, OWM_LON)
void fetchWeather() {
  // Pre-flight checks
  if (WiFi.status() != WL_CONNECTED) {
    lastWeatherError = UI.wifiOffline;
    return;
  }

  if (strlen(OWM_API_KEY) == 0) {
    lastWeatherError = "API key missing";
    return;
  }

  WiFiClient client;
  HTTPClient http;

  // Build OpenWeatherMap API URL
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=";
  url += OWM_LAT;
  url += "&lon=";
  url += OWM_LON;
  url += "&appid=";
  url += OWM_API_KEY;
  url += "&units=metric&lang=it";

  http.begin(client, url);
  http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);

  // Perform HTTP GET request
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    // Security check: validate response size before reading
    int contentLength = http.getSize();
    if (contentLength > 0 && contentLength > WEATHER_MAX_RESPONSE_SIZE) {
      lastWeatherError = "Response too large";
      http.end();
      return;
    }

    // Read and parse JSON response
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      // Validate JSON structure before accessing keys (prevents crashes)
      if (doc.containsKey("main") && doc["main"].containsKey("temp") && doc["main"].containsKey("humidity") && doc.containsKey("weather") && doc["weather"].size() > 0) {

        // Extract weather data
        weatherTemp = doc["main"]["temp"].as<float>();
        weatherHumidity = doc["main"]["humidity"].as<int>();
        weatherDesc = doc["weather"][0]["description"].as<String>();

        // Capitalize first letter of description
        if (weatherDesc.length() > 0) {
          weatherDesc[0] = toupper(weatherDesc[0]);
        }

        lastWeatherUpdate = millis();
        lastWeatherError = "";

      } else {
        lastWeatherError = "Invalid JSON structure";
      }
    } else {
      lastWeatherError = "JSON parse error";
    }
  } else {
    lastWeatherError = "HTTP error: " + String(code);
  }

  http.end();
}

// parseRssItems()
// Parses RSS 2.0 XML feed and extracts news items
// Searches for <item> tags and extracts <title> and <link> content
// Handles CDATA sections automatically via extractTag()
// Parameters:
//   xml        - Raw RSS XML string
//   outTitles  - Output array for news headlines
//   outLinks   - Output array for news URLs
//   maxItems   - Maximum number of items to parse
//   parseError - Output string for error reporting
// Returns: Number of successfully parsed items (0 indicates failure)
int parseRssItems(const String& xml, String* outTitles, String* outLinks, int maxItems, String& parseError) {
  int pos = 0;
  int found = 0;
  parseError = "";

  while (found < maxItems) {
    int itemStart = xml.indexOf("<item>", pos);
    if (itemStart < 0) break;
    int itemEnd = xml.indexOf("</item>", itemStart);
    if (itemEnd < 0) {
      parseError = "Malformed XML: missing </item>";
      break;
    }

    String item = xml.substring(itemStart, itemEnd);
    String title = extractTag(item, "title");
    String link = extractTag(item, "link");

    if (title.length() > 0) {
      outTitles[found] = title;
      outLinks[found] = link;
      found++;
    }

    pos = itemEnd + 7;
  }

  // Returning 0 with an explicit error keeps caller-side diagnostics simple.
  if (found == 0 && parseError.length() == 0) {
    parseError = "No <item> parsed";
  }
  return found;
}

// fetchAnsaRSS()
// Fetches ANSA news RSS feed over HTTPS
// Updates global arrays: newsTitles[], newsLinks[]
// Updates diagnostics: lastNewsHttpCode, lastNewsError, lastNewsCount, lastNewsUpdate
// Preserves previous news data if fetch fails (graceful degradation)
// Uses automatic retry logic (RSS_HTTP_ATTEMPTS) to handle transient network errors
// Parameters:
//   feedUrl - URL of RSS feed to fetch (must be HTTPS for ANSA)
void fetchAnsaRSS(const char* feedUrl) {
  // Pre-flight check
  if (WiFi.status() != WL_CONNECTED) {
    lastNewsHttpCode = -1;
    lastNewsError = UI.wifiOffline;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation for simplicity

  int code = -1;
  String xml;

  // Retry loop for transient network failures
  for (uint8_t attempt = 0; attempt < RSS_HTTP_ATTEMPTS; attempt++) {
    HTTPClient http;
    http.begin(client, feedUrl);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", "Mozilla/5.0");

    code = http.GET();

    if (code == HTTP_CODE_OK) {
      // Security check: validate response size before reading
      int contentLength = http.getSize();
      if (contentLength > 0 && contentLength > RSS_MAX_RESPONSE_SIZE) {
        lastNewsError = "Feed too large";
        http.end();
        return;
      }

      xml = http.getString();
      http.end();

      // Validate we actually got data
      if (xml.length() == 0) {
        lastNewsError = "Empty response";
        return;
      }

      break;  // Success - exit retry loop
    }

    http.end();

    // Delay before next attempt (except on last attempt)
    if (attempt < RSS_HTTP_ATTEMPTS - 1) {
      delay(RSS_RETRY_DELAY_MS);
    }
  }

  // Update diagnostics
  lastNewsHttpCode = code;
  lastNewsError = "";
  lastNewsCount = 0;

  if (code == HTTP_CODE_OK) {
    // Parse RSS XML and extract news items
    lastNewsCount = parseRssItems(xml, newsTitles, newsLinks,
                                  NEWS_MAX, lastNewsError);

    if (lastNewsCount > 0) {
      lastNewsUpdate = millis();
    }
  } else {
    lastNewsError = "HTTP error: " + String(code);
  }
}

// =====================================================================
// UI Rendering Functions
// =====================================================================

// drawWeather()
// Renders the weather information panel in the lower section of the screen
// Displays:
//   - Temperature value with color-coded progress bar
//   - Humidity percentage with color-coded progress bar
//   - Weather condition icon
//   - Temperature and humidity indicator icons
// Called periodically when new weather data is available
void drawWeather() {
  // Clear weather panel area
  tft.fillRect(0, 125, 240, 115, BG_COLOR);
  tft.drawFastHLine(20, 135, 200, 0x4208);  // Separator line

  // === TEMPERATURE DISPLAY ===
  tft.setTextSize(2);
  tft.setTextColor(TEMP_COLOR);
  tft.setCursor(45, 150);

  // Temperature progress bar background
  tft.drawRoundRect(45, 170, 80, 9, 50, ST77XX_WHITE);
  tft.fillCircle(48, 174, 2, TEMP_COLOR);  // Indicator dot

  // Calculate and draw temperature fill (0-40°C scale)
  float t = constrain(weatherTemp, 0.0f, 40.0f);
  int tempWidth = (int)((78.0f * t) / 40.0f);
  tft.fillRect(48, 171, tempWidth, 7, TEMP_COLOR);

  // Temperature icon
  drawRGB565_P(TEMP_ICON_X, TEMP_ICON_Y, TEMP_ICON_W, TEMP_ICON_H, TEMP_ICON_RGB565);

  // Temperature text
  char tempText[12];
  snprintf(tempText, sizeof(tempText), "%.1fC", weatherTemp);
  tft.print(tempText);

  // === HUMIDITY DISPLAY ===
  tft.setTextSize(2);
  tft.setTextColor(HUM_COLOR);
  tft.setCursor(45, 195);

  // Humidity progress bar background
  tft.drawRoundRect(45, 215, 80, 9, 50, ST77XX_WHITE);
  tft.fillCircle(48, 219, 2, HUM_COLOR);  // Indicator dot

  // Calculate and draw humidity fill (0-100% scale)
  int h = constrain(weatherHumidity, 0, 100);
  int humiWidth = (int)((78.0f * h) / 100.0f);
  tft.fillRect(48, 216, humiWidth, 7, HUM_COLOR);

  // Humidity icon
  drawRGB565_P(HUMI_ICON_X, HUMI_ICON_Y, HUMI_ICON_W, HUMI_ICON_H, HUMI_ICON_RGB565);

  // Humidity text
  char humidityText[8];
  snprintf(humidityText, sizeof(humidityText), "%d%%", weatherHumidity);
  tft.print(humidityText);

  // Weather condition icon (right side)
  drawRGB565_P(MM_ICON_X, MM_ICON_Y, MM_RGB565_W, MM_RGB565_H, MM_RGB565);

  // Reset text color for future draws
  tft.setTextColor(ST77XX_WHITE);
}

// drawNews()
// Renders a single news headline with automatic word-wrapping
// Displays:
//   - RSS feed icon in top-left corner
//   - Item counter in top-right (e.g., "2/3")
//   - Word-wrapped news headline text
//   - Source attribution footer
// Handles long words and prevents text overflow using intelligent line breaking
// Parameters:
//   index - Index of news item to display (0 to NEWS_MAX-1)
void drawNews(int index) {
  const int16_t textX = 10;
  const int16_t textY = 50;
  const int16_t maxW = 220;  // 240 - margins
  const int16_t maxH = 185;  // from textY down to bottom margin
  const int16_t charW = 12;  // default font at text size 2
  const int16_t lineH = 16;  // default font at text size 2
  const int16_t maxChars = maxW / charW;

  tft.fillScreen(BG_COLOR);

  // RSS Icon
  drawRGB565_P(RSS_ICON_X, RSS_ICON_Y, RSS_ICON_W, RSS_ICON_H, RSS_ICON_RGB565);

  // RSS News
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(false);
  tft.setTextSize(2);

  tft.setCursor(textX, textY);
  if (index < 0 || index >= NEWS_MAX || newsTitles[index].length() == 0) {
    tft.print("No news available");
  } else {
    // Current item index (1-based)
    tft.setCursor(185, 10);
    tft.print(String(index + 1) + "/" + String(NEWS_MAX));

    // Normalize text before wrapping (single spaces, no newlines).
    String text = newsTitles[index];
    text.replace("\n", " ");
    text.trim();

    // Word-wrap renderer: wraps on spaces, avoids splitting words.
    String line = "";
    int y = textY;
    int pos = 0;

    while (pos < text.length() && (y - textY) <= maxH) {
      while (pos < text.length() && text[pos] == ' ') pos++;
      int start = pos;
      while (pos < text.length() && text[pos] != ' ') pos++;
      String word = text.substring(start, pos);
      if (word.length() == 0) continue;

      String candidate = (line.length() == 0) ? word : (line + " " + word);
      if (candidate.length() <= maxChars) {
        line = candidate;
        continue;
      }

      if (line.length() > 0) {
        tft.setCursor(textX, y);
        tft.print(line);
        y += lineH;
      }

      if (word.length() <= maxChars) {
        line = word;
      } else {
        // Fallback for single words longer than one full line.
        int wp = 0;
        while (wp < word.length() && (y - textY) <= maxH) {
          String chunk = word.substring(wp, wp + maxChars);
          tft.setCursor(textX, y);
          tft.print(chunk);
          y += lineH;
          wp += maxChars;
        }
        line = "";
      }
    }

    if (line.length() > 0 && (y - textY) <= maxH) {
      tft.setCursor(textX, y);
      tft.print(line);
    }
  }

  // Fonte
  tft.setCursor(10, 210);
  tft.print("Fonte: ANSA");
}

// drawClock()
// Renders digital clock display in the upper panel
// Shows current time in HH:MM format (24-hour) using large bold font
// Displays "--:--" if NTP sync has not yet succeeded
// Automatically centers the time display on screen
// Called periodically by the main loop to update the clock display
void drawClock() {
  time_t now = time(nullptr);
  char timeStr[TIME_STRING_BUFFER_SIZE];
  if (now < 100000) {
    snprintf(timeStr, sizeof(timeStr), "--:--");
  } else {
    struct tm* timeinfo = localtime(&now);
    if (timeinfo == nullptr) {
      snprintf(timeStr, sizeof(timeStr), "--:--");
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
    }
  }

  tft.setFont(&FreeSansBold18pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int cx = ((240 - w) / 2) - 5;
  int cy = 40 + h;
  tft.fillRect(25, 35, 190, 65, BG_COLOR);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(cx, cy);
  tft.print(timeStr);
  tft.setFont(NULL);
}

// =====================================================================
// OTA (Over-The-Air Update) Callback Functions
// =====================================================================
// These callbacks are triggered by ElegantOTA during firmware updates
// Important: Avoid SPI display operations during actual flash writing

// onOTAStart()
// Called when OTA update begins
// Sets flag to pause normal display updates and shows status message
void onOTAStart() {
  otaInProgress = true;
  showStatus(UI.updating, ST77XX_YELLOW);
}

// onOTAProgress()
// Called repeatedly during OTA update progress
// Intentionally left empty to avoid screen flicker and SPI bus conflicts
// Parameters:
//   current - Bytes written so far
//   total   - Total firmware size
void onOTAProgress(size_t current, size_t total) {
  // Intentionally empty
}

// onOTAEnd()
// Called when OTA update completes (success or failure)
// Displays final result message to user before device reboots
// Parameters:
//   success - true if update succeeded, false if it failed
void onOTAEnd(bool success) {
  if (success) {
    showStatus(UI.updated, ST77XX_GREEN);
  } else {
    showStatus(UI.failed, ST77XX_RED);
  }
}

// =====================================================================
// Non-Blocking Task Handlers (Tick Functions)
// =====================================================================
// These functions are called periodically from loop() to handle
// background tasks without blocking the main execution

// tickWiFiRetry()
// Attempts to reconnect WiFi if connection is lost
// Runs at intervals defined by WIFI_RETRY_INTERVAL_MS
// Updates boot state based on reconnection result
// Allows device to recover automatically from temporary network outages
void tickWiFiRetry(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (now - lastWiFiRetry < WIFI_RETRY_INTERVAL_MS) return;

  lastWiFiRetry = now;
  WiFiConnectResult result = connectWiFi();
  if (result == WiFiConnectResult::Connected) {
    bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_NTP;
  } else {
    bootState = BootState::BOOT_DEGRADED;
    ntpSynced = false;
  }
}

// tickNtpRetry()
// Retries NTP synchronization if WiFi is connected but time is not yet synced
// Runs at intervals defined by NTP_RETRY_INTERVAL_MS
// Updates boot state based on sync result
// Ensures accurate time even if initial NTP sync failed during boot
void tickNtpRetry(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (ntpSynced) return;
  if (now - lastNtpRetry < NTP_RETRY_INTERVAL_MS) return;

  lastNtpRetry = now;
  bootState = BootState::BOOT_NTP;
  ntpSynced = syncNTP();
  bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_DEGRADED;
}

// tickInitialDataFetch()
// Performs the first data fetch after a short delay following boot
// Defers initial HTTP requests (INITIAL_DATA_FETCH_DELAY_MS) so the UI appears quickly
// Fetches both weather and news data in a single burst
// Runs only once per boot cycle
void tickInitialDataFetch(unsigned long now) {
  if (initialDataFetched) return;
  if (now - bootMs < INITIAL_DATA_FETCH_DELAY_MS) return;

  fetchWeather();
  fetchAnsaRSS(ANSA_RSS_URL);
  lastWeatherFetch = now;
  lastNewsFetch = now;
  initialDataFetched = true;
  if (!showNews) drawWeather();
}

// tickWeather()
// Periodically fetches updated weather data from OpenWeatherMap
// Runs at intervals defined by OWM_INTERVAL_MS (typically 10 minutes)
// Automatically refreshes the weather display panel after successful fetch
// Only updates display when home screen is visible (not during news scene)
void tickWeather(unsigned long now) {
  if (hasIntervalPassed(lastWeatherFetch, OWM_INTERVAL_MS, now)) {
    fetchWeather();
    if (!showNews) drawWeather();
  }
}

// tickNews()
// Periodically fetches updated RSS news feed from ANSA
// Runs at intervals defined by NEWS_INTERVAL_MS (typically 10 minutes)
// Updates news data used by the news scene scheduler
void tickNews(unsigned long now) {
  if (hasIntervalPassed(lastNewsFetch, NEWS_INTERVAL_MS, now)) {
    fetchAnsaRSS(ANSA_RSS_URL);
  }
}

// tickMarquee()
// Animates scrolling marquee text across the display
// Moves text horizontally at intervals defined by MARQUEE_INTERVAL_MS
// Text wraps around when it scrolls off the left edge
// Only active when home screen is visible (pauses during news scene)
void tickMarquee(unsigned long now) {
  if (showNews) return;
  if (now - lastMarqueeUpdate < MARQUEE_INTERVAL_MS) return;

  lastMarqueeUpdate = now;
  tft.fillRect(0, MARQUEE_Y, SCREEN_W, MARQUEE_H, BG_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  // Prevent line-wrap artifacts when marqueeX becomes negative.
  tft.setTextWrap(false);
  tft.setCursor(marqueeX, MARQUEE_Y);
  tft.print(marqueeMessage);

  int16_t bx, by;
  uint16_t bw, bh;
  tft.getTextBounds(marqueeMessage, 0, 0, &bx, &by, &bw, &bh);
  marqueeX -= MARQUEE_STEP_PX;
  if (marqueeX < -bw) {
    marqueeX = SCREEN_W;
  }
}

// tickClockRefresh()
// Redraws the clock display only when the displayed minute has changed,
// checked every CLOCK_REFRESH_MS while in the clock scene
// Parameters:
//   now - Current timestamp from millis()
void tickClockRefresh(unsigned long now) {
  if (showNews) return;
  if (!hasIntervalPassed(lastClockRefresh, CLOCK_REFRESH_MS, now)) return;

  time_t t = time(nullptr);
  struct tm* ti = localtime(&t);
  int currentMinute = ntpSynced && ti ? ti->tm_hour * 60 + ti->tm_min : -1;
  if (currentMinute == lastMinute) return;
  lastMinute = currentMinute;
  drawClock();
}

// tickSceneScheduler()
// Manages switching between clock/home scene and news display scenes
// Parameters:
//   now - Current timestamp from millis()
void tickSceneScheduler(unsigned long now) {
  unsigned long interval = showNews ? DISPLAY_NEWS_MS : DISPLAY_CLOCK_MS;
  if (now - lastDisplay < interval) {
    return;
  }

  lastDisplay = now;
  if (!showNews) {
    showNews = true;
    currentNewsIndex = 0;
    drawNews(currentNewsIndex);
    return;
  }

  currentNewsIndex++;
  if (currentNewsIndex >= NEWS_MAX) {
    showNews = false;
    tft.fillScreen(BG_COLOR);
    marqueeX = SCREEN_W;
    drawClock();
    drawWeather();
  } else {
    drawNews(currentNewsIndex);
  }
}

// =====================================================================
// Arduino Core Functions
// =====================================================================

// setup()
// Main initialization function (called once at startup)
//
// Initialization sequence:
//   1. Configure TFT display and backlight
//   2. Attempt WiFi connection with visual feedback
//   3. Synchronize time with NTP servers (if WiFi connected)
//   4. Configure HTTP web server endpoints:
//      - GET /          -> Web UI (HTML dashboard)
//      - GET /api       -> Device status JSON
//      - GET /news      -> RSS feed data JSON
//      - GET /brightness -> Backlight control
//   5. Initialize OTA update service
//   6. Render initial home screen (clock + weather)
//
// Note: Uses timeout-protected blocking for WiFi/NTP to ensure boot completes
//       even if network services are unavailable
void setup() {
  bootMs = millis();
  analogWriteFreq(5000);  // Increase PWM frequency to reduce backlight flicker during WiFi ops
  analogWriteRange(255);
  setBrightness(50);
  tft.init(SCREEN_W, SCREEN_H, SPI_MODE3);
  tft.setRotation(2);
  tft.fillScreen(BG_COLOR);

  WiFiConnectResult wifiResult = connectWiFi();
  if (wifiResult == WiFiConnectResult::Connected) {
    bootState = BootState::BOOT_NTP;
    ntpSynced = syncNTP();
    bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_DEGRADED;
  } else {
    bootState = BootState::BOOT_DEGRADED;
    ntpSynced = false;
  }

  lastWeatherFetch = millis();
  lastNewsFetch = millis();
  lastDisplay = millis();

  // HTTP Endpoint: GET /
  // Serves the main web dashboard UI (HTML/CSS/JavaScript)
  // Page is stored in flash memory (PROGMEM) to save RAM
  server.on(
    "/",
    []() {
      server.send_P(200, "text/html", INDEX_HTML);
    });

  // HTTP Endpoint: GET /api
  // Returns current device status and sensor data as JSON
  // Used by web UI for live updates and diagnostics
  server.on(
    "/api",
    []() {
      String escapedDesc = jsonEscape(weatherDesc);
      char tempText[12];
      snprintf(tempText, sizeof(tempText), "%.1f", weatherTemp);

      unsigned long now = millis();
      // Calculate data age in milliseconds (sentinel value -1 means "never fetched")
      long weatherAge = (lastWeatherUpdate == 0) ? -1L : (long)(now - lastWeatherUpdate);
      long newsAge = (lastNewsUpdate == 0) ? -1L : (long)(now - lastNewsUpdate);

      char json[512];
      snprintf(
        json,
        sizeof(json),
        "{\"temp\":%s,\"humidity\":%d,\"description\":\"%s\",\"ip\":\"%s\",\"brightness\":%d,"
        "\"wifi\":%s,\"wifi_result\":\"%s\",\"ntp\":%s,\"boot_state\":\"%s\","
        "\"last_weather_ms\":%ld,\"last_news_ms\":%ld}",
        tempText,
        weatherHumidity,
        escapedDesc.c_str(),
        WiFi.localIP().toString().c_str(),
        currentBrightness,
        (WiFi.status() == WL_CONNECTED) ? "true" : "false",
        wifiResultToString(lastWiFiResult),
        ntpSynced ? "true" : "false",
        bootStateToString(bootState),
        weatherAge,
        newsAge);

      server.send(200, "application/json", json);
    });

  // HTTP Endpoint: GET /news
  // Returns RSS news feed data with debugging information
  // Includes all fetched headlines, links, and fetch diagnostics
  server.on(
    "/news",
    []() {
      String json = "{";
      json += "\"source\":\"ANSA\",";
      json += "\"items\":[";
      for (int i = 0; i < NEWS_MAX; i++) {
        if (i > 0) {
          json += ",";
        }
        json += "{";
        json += "\"title\":\"" + jsonEscape(newsTitles[i]) + "\",";
        json += "\"link\":\"" + jsonEscape(newsLinks[i]) + "\"";
        json += "}";
      }
      json += "],";
      json += "\"debug\":{";
      json += "\"http\":" + String(lastNewsHttpCode) + ",";
      json += "\"count\":" + String(lastNewsCount) + ",";
      json += "\"error\":\"" + jsonEscape(lastNewsError) + "\",";
      json += "\"age_ms\":" + String(millis() - lastNewsFetch);
      json += "}}";

      server.send(200, "application/json", json);
    });

  // HTTP Endpoint: GET /brightness?value=N
  // Controls TFT backlight brightness (0-255)
  // Parameter: value - brightness level (0=off, 255=maximum)
  // Validates input to prevent invalid values
  server.on(
    "/brightness",
    []() {
      if (!server.hasArg("value")) {
        server.send(400, "text/plain", "Missing value parameter");
        return;
      }

      int brightness;
      if (!validateBrightnessInput(server.arg("value"), brightness)) {
        server.send(400, "text/plain", "Invalid value (0-255)");
        return;
      }

      setBrightness(brightness);
      server.send(200, "application/json", "{\"ok\":true}");
    });

  // Fetch weather and news immediately before first render, then skip deferred initial fetch.
  fetchWeather();
  fetchAnsaRSS(ANSA_RSS_URL);
  initialDataFetched = true;

  // ElegantOTA
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();

  // Show Clock and Weather Info
  drawClock();
  drawWeather();

  // Prime clock refresh state so tickClockRefresh doesn't redraw immediately
  {
    time_t t = time(nullptr);
    struct tm* ti = localtime(&t);
    lastMinute = (ntpSynced && ti) ? ti->tm_hour * 60 + ti->tm_min : -1;
    lastClockRefresh = millis();
  }
}

// loop()
// Main runtime loop (called continuously after setup completes)
//
// Execution flow:
//   1. Handle incoming HTTP requests (web UI and API)
//   2. Process OTA update requests
//   3. Skip normal operations if OTA is in progress (prevents SPI conflicts)
//   4. Execute all non-blocking tick functions:
//      - WiFi reconnection attempts
//      - NTP resync attempts
//      - Initial data fetch (one-time after boot)
//      - Periodic weather updates
//      - Periodic news feed updates
//      - Marquee scrolling animation
//      - Scene switching logic
//
// Note: All tasks are non-blocking with time-based scheduling
void loop() {
  server.handleClient();
  ElegantOTA.loop();

  if (otaInProgress) return;

  unsigned long now = millis();
  tickWiFiRetry(now);
  tickNtpRetry(now);
  tickInitialDataFetch(now);
  tickWeather(now);
  tickNews(now);
  tickMarquee(now);
  tickClockRefresh(now);
  tickSceneScheduler(now);
}
