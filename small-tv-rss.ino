// =====================================================================
// File: small-tv-rss.ino
// Purpose: Main firmware entrypoint (ESP8266 + ST7789)
// Changelog (latest first):
//   - 2026.05.11: Added periodic internet health-check with threshold-based recovery flow
//   - 2026.05.03: Header/comment structure normalized (format-only update)
//   - 2026.03.26: User-friendly error messages on display (detailed errors remain on web)
//   - 2026.03.22: Removed GTT placeholder fallback, direct error propagation on TFT/Web
//   - 2026.03.15: GTT page in PROGMEM, RAM hardening
//   - 2026.03.11: UI and typography refresh
// =====================================================================

#include <Adafruit_GFX.h>
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

#include "app_config.h"

// Custom GFX fonts generated from local TTF assets:
// - Bebas Neue: digital clock only
// - Oswald: weather metrics and RSS/news text
#include "fonts/Bebas_Neue/BebasNeue42pt7b.h"
#include "fonts/Oswald/OswaldRegular10pt7b.h"
#include "fonts/Oswald/OswaldSemiBold10pt7b.h"
#include "fonts/Oswald/OswaldSemiBold14pt7b.h"

// Web UI HTML pages stored in flash memory
#include "web_dashboard_html.h"
#include "gtt_dashboard_html.h"

// Icon assets (stored in flash memory to save RAM):
#include "icons/wifi/icons_1bit.h"   // WiFi connection animation (32x32, 1-bit monochrome)
#include "icons/sync/icons_1bit.h"   // NTP sync spinner animation (32x32, 1-bit monochrome)
#include "icons/mm/mm_rgb565.h"      // Weather condition icon (80x80, RGB565 color)
#include "icons/temp/temp_rgb565.h"  // Temperature indicator (12x32, RGB565 color)
#include "icons/temp/humi_rgb565.h"  // Humidity indicator (22x32, RGB565 color)
#include "icons/rss/rss_rgb565.h"    // RSS feed icon (32x32, RGB565 color)
#include "icons/gtt/gtt_rgb565.h"    // GTT logo (78x32, RGB565 color)

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

#ifndef OTA_USERNAME
#define OTA_USERNAME ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
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
constexpr const char* FW_VERSION = "2026.05.11";

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

// ===== BOOT & SYSTEM STATE =====
BootState bootState = BootState::BOOT_WIFI;                     // Current boot/initialization phase
WiFiConnectResult lastWiFiResult = WiFiConnectResult::Timeout;  // Result of last WiFi connection attempt
bool ntpSynced = false;                                         // True when NTP time sync is successful
bool otaInProgress = false;                                     // True during OTA firmware update
unsigned long bootMs = 0;                                       // Timestamp when device booted

// ===== NETWORK RETRY TIMERS =====
unsigned long lastWiFiRetry = 0;                // Last WiFi reconnection attempt
unsigned long lastNtpRetry = 0;                 // Last NTP sync retry
unsigned long lastInternetCheck = 0;            // Last full internet health-check attempt
bool lastInternetCheckOk = true;                // Result of the latest internet health-check
int lastInternetCheckHttpCode = 0;              // HTTP code from latest internet health-check
uint8_t internetCheckFailures = 0;              // Consecutive failed internet health-checks
unsigned long lastInternetRecoveryAttempt = 0;  // Last forced reconnect/recovery attempt

// ===== WEATHER DATA (OpenWeatherMap API) =====
float weatherTemp = 0;                    // Temperature in Celsius
int weatherHumidity = 0;                  // Relative humidity percentage
String weatherDesc = "";                  // Weather condition description (e.g., "Partly cloudy")
unsigned long lastWeatherFetchTime = 0;   // Timestamp of last fetch attempt (regardless of success)
unsigned long lastWeatherUpdateTime = 0;  // Timestamp of last successful fetch
String lastWeatherError = "";             // Last error message from weather API fetch

// ===== NEWS DATA (ANSA RSS Feed) =====
String newsTitles[NEWS_MAX];           // News headlines from ANSA feed
String newsLinks[NEWS_MAX];            // URLs for each news item
unsigned long lastNewsFetchTime = 0;   // Timestamp of last feed fetch attempt (regardless of success)
unsigned long lastNewsUpdateTime = 0;  // Timestamp of last successful news fetch
int lastNewsHttpCode = 0;              // HTTP response code from last fetch attempt
String lastNewsError = "";             // Error message from last fetch attempt
int lastNewsCount = 0;                 // Number of items successfully parsed from latest feed
bool initialDataFetched = false;       // True once both weather AND news have been fetched successfully

// ===== GTT DATA (Turin Transport - Bus Stop) =====
struct GttStop {
  String line;    // Bus line number (e.g., "13")
  String hour;    // Time in HH:MM format
  bool realtime;  // true=realtime data, false=scheduled
};

constexpr int GTT_MAX = 8;            // Maximum number of GTT stops to display (4 lines × 2 times)
GttStop gttStops[GTT_MAX];            // Array of bus stop data
unsigned long lastGttFetchTime = 0;   // Timestamp of last GTT fetch attempt
unsigned long lastGttUpdateTime = 0;  // Timestamp of last successful GTT fetch
String lastGttError = "";             // Error message from last fetch attempt
int lastGttCount = 0;                 // Number of stops successfully parsed from latest fetch
constexpr uint8_t GTT_LINES_ON_SCREEN = 4;
constexpr uint8_t GTT_TIMES_PER_LINE = 2;
constexpr size_t GTT_JSON_DOC_SIZE = 1024;

// ===== UI DISPLAY STATE (Scene Machine) =====
enum class DisplayScene : uint8_t {
  SCENE_CLOCK = 0,  // Clock + weather (15s)
  SCENE_NEWS = 1,   // News headlines (5s per item)
  SCENE_GTT = 2     // GTT bus times (15s)
};

DisplayScene currentScene = DisplayScene::SCENE_CLOCK;  // Currently displayed scene
int currentNewsIndex = 0;                               // Currently displayed news item index (0 to NEWS_MAX-1)
unsigned long lastDisplay = 0;                          // Scene switching timer (when to switch to next scene)
unsigned long lastClockRefresh = 0;                     // Clock refresh timer (while in clock scene)
int lastMinute = -1;                                    // Last displayed minute (avoids redundant clock redraws)
bool offlineScreenShown = false;                        // True when timeout/offline error screen is currently displayed

// ===== DISPLAY BRIGHTNESS =====
// Value range: 0-255 (automatically inverted for PWM since this panel uses inverted backlight control)
int currentBrightness = 50;
constexpr uint8_t DEFAULT_BRIGHTNESS = 50;

// ===== SCENE/UI RENDERING CONSTANTS =====
constexpr int16_t SCENE_PROGRESS_X = 10;
constexpr int16_t SCENE_PROGRESS_WIDTH = 220;
constexpr int16_t SCENE_PROGRESS_HEIGHT = 5;
constexpr int16_t SCENE_PROGRESS_RADIUS = 2;
constexpr int16_t SCENE_PROGRESS_Y_CLOCK = 120;
constexpr int16_t SCENE_PROGRESS_Y_NEWS = 190;
constexpr int16_t SCENE_PROGRESS_Y_GTT = 216;

constexpr int16_t WEATHER_TEMP_BAR_X = 45;
constexpr int16_t WEATHER_TEMP_BAR_Y = 170;
constexpr int16_t WEATHER_HUM_BAR_X = 45;
constexpr int16_t WEATHER_HUM_BAR_Y = 215;
constexpr int16_t WEATHER_BAR_WIDTH = 80;
constexpr int16_t WEATHER_BAR_INNER_X = 48;
constexpr int16_t WEATHER_BAR_INNER_W = 78;
constexpr int16_t WEATHER_BAR_INNER_H = 7;
constexpr int16_t WEATHER_TEMP_MIN_C = 0;
constexpr int16_t WEATHER_TEMP_MAX_C = 40;
constexpr uint16_t GTT_SCHEDULED_COLOR = 0x8410;  // Grey
constexpr uint16_t GTT_SEPARATOR_COLOR = 0x39E7;  // Light grey

// =====================================================================
// Utility Helper Functions
// =====================================================================

void showStatusCentered(const String& msg,
                        uint16_t color = ST77XX_WHITE,
                        int16_t y = STATUS_TEXT_CENTER_Y);

// isWiFiConnected()
// Quick check for WiFi connectivity status
// Returns: true if WiFi is connected, false otherwise
bool isWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED || !WiFi.isConnected()) {
    return false;
  }
  const IPAddress ip = WiFi.localIP();
  return ip != INADDR_NONE && ip != INADDR_ANY;
}

// ensureWiFiConnected()
// Helper that sets error message and returns false if WiFi is down
// Used by fetch functions to standardize connection validation
// Parameters: errorString - Reference to error variable to populate if disconnected
// Returns: true if WiFi connected, false if disconnected (with error message set)
bool ensureWiFiConnected(String& errorString) {
  if (!isWiFiConnected()) {
    errorString = UI.wifiOffline;
    return false;
  }
  return true;
}

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

// Returns the most appropriate offline status message for UI display.
const char* getOfflineStatusMessage() {
  if (lastWiFiResult == WiFiConnectResult::MissingCredentials) {
    return UI.wifiMissing;
  }
  if (lastWiFiResult == WiFiConnectResult::Timeout) {
    return UI.wifiTimeout;
  }
  return UI.wifiOffline;
}

// Converts scene enum to its display duration.
// Special case: GTT error screen shows only 5 seconds
unsigned long getSceneIntervalMs(DisplayScene scene) {
  switch (scene) {
    case DisplayScene::SCENE_NEWS: return DISPLAY_NEWS_MS;
    case DisplayScene::SCENE_GTT:
      return (lastGttCount == 0) ? 5000UL : DISPLAY_GTT_MS;
    case DisplayScene::SCENE_CLOCK:
    default: return DISPLAY_CLOCK_MS;
  }
}

// Y coordinate used by the progress bar for each scene.
int16_t getSceneProgressY(DisplayScene scene) {
  switch (scene) {
    case DisplayScene::SCENE_NEWS: return SCENE_PROGRESS_Y_NEWS;
    case DisplayScene::SCENE_GTT: return SCENE_PROGRESS_Y_GTT;
    case DisplayScene::SCENE_CLOCK:
    default: return SCENE_PROGRESS_Y_CLOCK;
  }
}

// Color used by the progress bar for each scene.
uint16_t getSceneProgressColor(DisplayScene scene) {
  switch (scene) {
    case DisplayScene::SCENE_NEWS: return ST77XX_ORANGE;
    case DisplayScene::SCENE_GTT: return ST77XX_CYAN;
    case DisplayScene::SCENE_CLOCK:
    default: return ST77XX_WHITE;
  }
}

void drawSceneProgressBar(DisplayScene scene, unsigned long elapsed, unsigned long interval) {
  const int16_t width = static_cast<int16_t>((elapsed * SCENE_PROGRESS_WIDTH) / interval);
  tft.fillRoundRect(SCENE_PROGRESS_X,
                    getSceneProgressY(scene),
                    width,
                    SCENE_PROGRESS_HEIGHT,
                    SCENE_PROGRESS_RADIUS,
                    getSceneProgressColor(scene));
}

// Refreshes minute tracking used by tickClockRefresh().
void refreshClockTracking(unsigned long now) {
  time_t t = time(nullptr);
  struct tm* ti = localtime(&t);
  lastMinute = (ntpSynced && ti) ? ti->tm_hour * 60 + ti->tm_min : -1;
  lastClockRefresh = now;
}

// Renders the home scene (clock + weather), optionally clearing the full screen.
void renderClockScene(bool clearScreen) {
  if (clearScreen) {
    tft.fillScreen(BG_COLOR);
  }
  drawClock();
  drawWeather();
}

// Fetches all runtime datasets used by TFT and web API.
void fetchAllData() {
  fetchWeather();
  fetchAnsaRSS(ANSA_RSS_URL);
  fetchGTT(nullptr);  // URL parameter now ignored, using GTT_STOP_URL_1 and GTT_STOP_URL_2
}

// Refreshes runtime data/UI after internet connectivity is restored.
void refreshAfterInternetRecovery(unsigned long now) {
  showStatusCentered("Fetching data...", ST77XX_WHITE);
  fetchAllData();
  lastWeatherFetchTime = now;
  lastNewsFetchTime = now;
  lastGttFetchTime = now;

  currentScene = DisplayScene::SCENE_CLOCK;
  currentNewsIndex = 0;
  lastDisplay = now;
  renderClockScene(true);
  refreshClockTracking(now);
  offlineScreenShown = false;
}

// =====================================================================
// Display Helper Functions
// =====================================================================

// showStatus()
// Displays a full-screen status message with customizable color and position.
// Used during boot sequence, WiFi connection, NTP sync, and OTA updates
// Parameters:
//   msg   - Text message to display
//   color - Text color (RGB565 format)
//   x, y  - Top-left screen coordinates for text positioning
void showStatus(const String& msg,
                uint16_t color = ST77XX_WHITE,
                int16_t x = STATUS_TEXT_X,
                int16_t y = STATUS_TEXT_Y) {
  tft.fillScreen(BG_COLOR);
  tft.setFont(&Oswald_Regular10pt7b);
  tft.setTextColor(color);
  tft.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(msg.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(x - x1, y - y1);
  tft.print(msg);
  tft.setFont(NULL);
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
  // Keep original source stride for correct PROGMEM indexing when width is clipped.
  const int16_t sourceWidth = w;

  // Clip width to prevent buffer overflow
  if (w > RGB565_LINE_MAX) {
    w = RGB565_LINE_MAX;
  }

  uint16_t line[RGB565_LINE_MAX];

  // Render icon line by line to minimize RAM usage
  for (int16_t row = 0; row < h; row++) {
    // Copy one line from PROGMEM to RAM buffer
    for (int16_t col = 0; col < w; col++) {
      line[col] = pgm_read_word(&icon[row * sourceWidth + col]);
    }

    // Draw the buffered line to display
    tft.drawRGBBitmap(x, y + row, line, w, 1);
  }
}

// measureTextWidth()
// Returns rendered width for the currently selected font.
int16_t measureTextWidth(const String& text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  return static_cast<int16_t>(w);
}

int16_t measureTextWidth(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int16_t>(w);
}

// drawTextTopLeft()
// Draws text using top-left coordinates instead of baseline coordinates.
void drawTextTopLeft(int16_t x, int16_t y, const String& text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(x - x1, y - y1);
  tft.print(text);
}

void drawTextTopLeft(int16_t x, int16_t y, const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(x - x1, y - y1);
  tft.print(text);
}

// drawTextCenteredX()
// Draws text horizontally centered inside the provided width using top-left Y.
void drawTextCenteredX(int16_t areaX, int16_t areaY, int16_t areaW, const String& text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  const int16_t drawX = areaX + ((areaW - static_cast<int16_t>(w)) / 2);
  tft.setCursor(drawX - x1, areaY - y1);
  tft.print(text);
}

// showStatusCentered()
// Convenience wrapper around showStatus() for centered boot/status messages.
void showStatusCentered(const String& msg,
                        uint16_t color,
                        int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setFont(&Oswald_Regular10pt7b);
  tft.setTextSize(1);
  tft.getTextBounds(msg.c_str(), 0, 0, &x1, &y1, &w, &h);
  const int16_t centeredX = (SCREEN_W - static_cast<int16_t>(w)) / 2;
  tft.setFont(NULL);
  showStatus(msg, color, centeredX, y);
}

// drawClockSegment()
// Draws one clock segment and returns the next top-left X position.
int16_t drawClockSegment(int16_t x, int16_t y, const char* text, uint16_t color, int16_t trailingGap = 0) {
  tft.setTextColor(color);
  drawTextTopLeft(x, y, text);
  return x + measureTextWidth(text) + trailingGap;
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

void clearGttStops() {
  for (int i = 0; i < GTT_MAX; i++) {
    gttStops[i].line = "";
    gttStops[i].hour = "";
    gttStops[i].realtime = false;
  }
  lastGttCount = 0;
}

void appendGttStopJson(String& json, const String& line, const String& hour, bool realtime) {
  json += "{\"line\":\"";
  json += jsonEscape(line);
  json += "\",\"hour\":\"";
  json += jsonEscape(hour);
  json += "\",\"realtime\":";
  json += realtime ? "true" : "false";
  json += "}";
}

String buildApiJsonPayload() {
  const String escapedDesc = jsonEscape(weatherDesc);
  char tempText[12];
  snprintf(tempText, sizeof(tempText), "%.1f", weatherTemp);

  // Keep /api minimal: only fields used by the main dashboard.
  String json;
  json.reserve(180);
  json += "{\"temp\":";
  json += tempText;
  json += ",\"humidity\":";
  json += String(weatherHumidity);
  json += ",\"description\":\"";
  json += escapedDesc;
  json += "\",\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"brightness\":";
  json += String(currentBrightness);
  json += "}";
  return json;
}

String buildNewsJsonPayload() {
  String json;
  json.reserve(512);
  json += "{\"source\":\"ANSA\",\"items\":[";
  for (int i = 0; i < lastNewsCount; i++) {
    if (i > 0) {
      json += ",";
    }
    json += "{\"title\":\"";
    json += jsonEscape(newsTitles[i]);
    json += "\",\"link\":\"";
    json += jsonEscape(newsLinks[i]);
    json += "\"}";
  }
  json += "],\"debug\":{";
  json += "\"http\":";
  json += String(lastNewsHttpCode);
  json += ",\"count\":";
  json += String(lastNewsCount);
  json += ",\"error\":\"";
  json += jsonEscape(lastNewsError);
  json += "\",\"age_ms\":";
  json += String(millis() - lastNewsFetchTime);
  json += "}}";
  return json;
}

String buildGttJsonPayload() {
  String json;
  json.reserve(512);
  json += "{\"source\":\"GTT\",\"stops\":[";

  for (int i = 0; i < lastGttCount; i++) {
    if (i > 0) {
      json += ",";
    }
    appendGttStopJson(json, gttStops[i].line, gttStops[i].hour, gttStops[i].realtime);
  }

  json += "],\"debug\":{";
  json += "\"count\":";
  json += String(lastGttCount);
  json += ",\"error\":\"";
  json += jsonEscape(lastGttError);
  json += "\",\"age_ms\":";
  json += String(millis() - lastGttFetchTime);
  json += "}}";
  return json;
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

// Performs an end-to-end internet check (DNS + TLS + HTTP) to detect
// situations where WiFi is up but internet access is not available.
bool checkInternetHealth(int& httpCode) {
  httpCode = 0;
  if (!isWiFiConnected()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, INTERNET_HEALTHCHECK_URL)) {
    return false;
  }
  http.setTimeout(HTTP_TIMEOUT_MS);

  httpCode = http.sendRequest("HEAD");
  if (httpCode == 405) {
    httpCode = http.GET();
  }

  http.end();
  return httpCode >= 200 && httpCode < 400;
}

// connectWiFi()
// Attempts to connect to WiFi network with animated visual feedback
// Displays connection status and animates WiFi icon during connection attempt
// Blocks execution but has built-in timeout protection (WIFI_CONNECT_TIMEOUT_MS)
// Returns: WiFiConnectResult indicating success, missing credentials, or timeout
// Note: Uses WiFi.persistent(false) to avoid excessive flash wear
WiFiConnectResult ICACHE_FLASH_ATTR connectWiFi() {
  if (strlen(WIFI_SSID) == 0) {
    showStatusCentered(UI.wifiMissing, ST77XX_RED);
    delay(WIFI_STATUS_DELAY_MS);
    lastWiFiResult = WiFiConnectResult::MissingCredentials;
    return lastWiFiResult;
  }

  showStatusCentered(UI.wifi, ST77XX_WHITE);
  WiFi.persistent(false);  // Avoid flash writes on reconnect loops.
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);  // Reset stale station state before reconnect attempt.
  delay(50);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.drawBitmap(WIFI_ICON_X, WIFI_ICON_Y, WIFI, WIFI_W, WIFI_H, ST77XX_WHITE, BG_COLOR);
  unsigned long start = millis();
  unsigned long lastStep = millis();
  uint8_t frameIndex = 0;
  while (!isWiFiConnected() && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
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

  const bool connected = isWiFiConnected();
  if (connected) {
    showStatusCentered(UI.connected, ST77XX_GREEN);
    delay(WIFI_CONNECTED_DELAY_MS);
    showStatusCentered(String(UI.ipPrefix) + WiFi.localIP().toString(), ST77XX_WHITE);
    delay(WIFI_STATUS_DELAY_MS);
    lastWiFiResult = WiFiConnectResult::Connected;
  } else {
    showStatusCentered(UI.wifiTimeout, ST77XX_YELLOW);
    delay(WIFI_STATUS_DELAY_MS);
    WiFi.disconnect(false);  // Force a clean state for next retry cycle.
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
  if (!isWiFiConnected()) {
    ntpSynced = false;
    return false;
  }

  showStatusCentered(UI.ntpSync, ST77XX_WHITE);

  // Configure Italy timezone (CET/CEST with DST) before starting NTP sync.
  // DST changeovers: last Sunday in March at 02:00, last Sunday in October at 03:00.
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

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
  showStatusCentered(ntpSynced ? UI.synced : UI.failed,
                     ntpSynced ? ST77XX_GREEN : ST77XX_RED);
  delay(NTP_STATUS_DELAY_MS);
  tft.fillScreen(BG_COLOR);

  return ntpSynced;
}

// =====================================================================
// Data Fetching Functions
// =====================================================================

// fetchWeather()
// Fetches current weather data from OpenWeatherMap API
// Updates global variables: weatherTemp, weatherHumidity, weatherDesc, lastWeatherUpdateTime
// Silently fails if WiFi is disconnected or API key is missing
// Uses HTTP_TIMEOUT_MS and automatic retry logic (HTTP_MAX_RETRIES) for resilience
// Note: Coordinates are configured in app_config.h (OWM_LAT, OWM_LON)
void fetchWeather() {
  // Pre-flight checks
  if (!ensureWiFiConnected(lastWeatherError)) return;

  if (strlen(OWM_API_KEY) == 0) {
    lastWeatherError = "API key missing";
    return;
  }

  // Build OpenWeatherMap API URL
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += OWM_LAT;
  url += "&lon=";
  url += OWM_LON;
  url += "&appid=";
  url += OWM_API_KEY;
  url += "&units=metric&lang=it";

  int code = -1;

  // Retry loop: attempt multiple times for transient network failures
  for (uint8_t attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    // Perform HTTP GET request
    code = http.GET();

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

      http.end();

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

          lastWeatherUpdateTime = millis();
          lastWeatherError = "";
          return;  // Success - exit retry loop

        } else {
          lastWeatherError = "Invalid JSON structure";
        }
      } else {
        lastWeatherError = "JSON parse error";
      }
      return;  // Parsing errors shouldn't retry

    } else {
      // HTTP error - may be transient, retry if attempts remaining
      lastWeatherError = "HTTP error: " + String(code);
      http.end();

      if (attempt < HTTP_MAX_RETRIES - 1) {
        delay(HTTP_RETRY_DELAY_MS);
      }
    }
  }
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

  for (int i = 0; i < maxItems; i++) {
    outTitles[i] = "";
    outLinks[i] = "";
  }

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
// Updates diagnostics: lastNewsHttpCode, lastNewsError, lastNewsCount, lastNewsUpdateTime
// Preserves previous news data if fetch fails (graceful degradation)
// Uses automatic retry logic (HTTP_MAX_RETRIES) to handle transient network errors
// Parameters:
//   feedUrl - URL of RSS feed to fetch (must be HTTPS for ANSA)
void fetchAnsaRSS(const char* feedUrl) {
  // Pre-flight check
  if (!ensureWiFiConnected(lastNewsError)) {
    lastNewsHttpCode = -1;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation for simplicity

  int code = -1;
  String xml;

  // Retry loop for transient network failures
  for (uint8_t attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
    HTTPClient http;
    http.begin(client, feedUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);
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
    if (attempt < HTTP_MAX_RETRIES - 1) {
      delay(HTTP_RETRY_DELAY_MS);
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
      lastNewsUpdateTime = millis();
    }
  } else {
    lastNewsError = "HTTP error: " + String(code);
  }
}

// parseGttStops()
// Parses GTT API JSON response and extracts bus stop data
// Expected JSON format (from query.php):
// [{"line":"13","hour":"00:40","realtime":"true"}, ...]
// Parameters:
//   jsonStr    - Raw JSON string from API
//   outStops   - Output array for GttStop data
//   maxStops   - Maximum number of stops to parse
//   parseError - Output string for error reporting
// Returns: Number of successfully parsed stops
int parseGttStops(const String& jsonStr, GttStop* outStops, int maxStops, String& parseError) {
  parseError = "";
  int found = 0;

  // Initialize output array
  for (int i = 0; i < maxStops; i++) {
    outStops[i].line = "";
    outStops[i].hour = "";
    outStops[i].realtime = false;
  }

  // Parse JSON array
  StaticJsonDocument<GTT_JSON_DOC_SIZE> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);

  if (err) {
    parseError = "JSON parse error: " + String(err.c_str());
    return 0;
  }

  if (!doc.is<JsonArray>()) {
    parseError = "Expected JSON array";
    return 0;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) {
    parseError = "Empty stops array ([])";
    return 0;
  }
  for (JsonObject item : arr) {
    if (found >= maxStops) break;

    if (item.containsKey("line") && item.containsKey("hour") && item.containsKey("realtime")) {
      outStops[found].line = item["line"].as<String>();
      String fullTime = item["hour"].as<String>();

      // Extract only HH:MM (discard seconds if present)
      if (fullTime.length() >= 5) {
        outStops[found].hour = fullTime.substring(0, 5);  // Get first 5 chars (HH:MM)
      } else {
        outStops[found].hour = fullTime;
      }

      // Parse realtime flag (API may send boolean or string).
      JsonVariant realtimeValue = item["realtime"];
      if (realtimeValue.is<bool>()) {
        outStops[found].realtime = realtimeValue.as<bool>();
      } else {
        const char* realtimeStr = realtimeValue.as<const char*>();
        outStops[found].realtime = (realtimeStr != nullptr) && (!strcmp(realtimeStr, "true") || !strcmp(realtimeStr, "1"));
      }

      found++;
    }
  }

  if (found == 0) {
    parseError = "No stops parsed";
  }

  return found;
}

// fetchGTTSingleStop()
// Helper function: Fetches GTT data from a single stop URL
// Parameters:
//   apiUrl     - URL of GTT API endpoint
//   tempStops  - Temporary array to store parsed stops
//   maxStops   - Maximum number of stops to parse
//   errorMsg   - Output string for error reporting
// Returns: Number of stops successfully fetched and parsed
int fetchGTTSingleStop(const char* apiUrl, GttStop* tempStops, int maxStops, String& errorMsg) {
  errorMsg = "";

  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation
  int code = -1;
  String json;

  // Retry loop for transient network failures
  for (uint8_t attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
    HTTPClient http;
    http.begin(client, apiUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", "Mozilla/5.0");

    code = http.GET();

    if (code == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      if (contentLength > 0 && contentLength > GTT_MAX_RESPONSE_SIZE) {
        errorMsg = "Response too large";
        http.end();
        return 0;
      }

      json = http.getString();
      http.end();

      // Validate we actually got data
      if (json.length() == 0) {
        errorMsg = "Empty response";
        return 0;
      }
      if (json.length() > GTT_MAX_RESPONSE_SIZE) {
        errorMsg = "Payload too large";
        return 0;
      }

      break;  // Success - exit retry loop
    }

    http.end();

    // Delay before next attempt
    if (attempt < HTTP_MAX_RETRIES - 1) {
      delay(HTTP_RETRY_DELAY_MS);
    }
  }

  if (code != HTTP_CODE_OK) {
    errorMsg = "HTTP error: " + String(code);
    return 0;
  }

  // Parse JSON and extract bus stops
  return parseGttStops(json, tempStops, maxStops, errorMsg);
}

// fetchGTT()
// ⚠️ BETA FUNCTION: Fetches GTT bus stop data from multiple stops and merges results
// Limitations:
//   - Supports 2 hardcoded stop IDs (3445 and 3742)
//   - No nearby stops discovery, no stop selection UI
// Updates global array: gttStops[]
// Updates diagnostics: lastGttError, lastGttCount, lastGttUpdateTime
// Parameters:
//   apiUrl - URL of GTT API endpoint (ignored, kept for compatibility)
void fetchGTT(const char* apiUrl) {
  (void)apiUrl;  // Unused parameter, kept for backward compatibility

  // Pre-flight check
  if (!ensureWiFiConnected(lastGttError)) {
    clearGttStops();
    return;
  }

  // Temporary storage for stops from each API
  GttStop tempStops1[GTT_MAX];
  GttStop tempStops2[GTT_MAX];
  String error1, error2;

  // Fetch from both stops
  int count1 = fetchGTTSingleStop(GTT_STOP_URL_1, tempStops1, GTT_MAX, error1);
  int count2 = fetchGTTSingleStop(GTT_STOP_URL_2, tempStops2, GTT_MAX, error2);

  // Clear global array before merging
  clearGttStops();
  lastGttError = "";
  lastGttCount = 0;

  // Merge policy:
  // 1) Primary stop first (main rows)
  // 2) Keep a reserved tail for secondary stop so it can appear in the last rows
  int mergedCount = 0;
  int idx1 = 0;
  int idx2 = 0;

  // Reserve at least one full line (2 time slots) for the secondary stop when available.
  const int secondaryReserve = (count2 > 0) ? min(static_cast<int>(GTT_TIMES_PER_LINE), count2) : 0;
  const int primaryBudget = GTT_MAX - secondaryReserve;

  while (idx1 < count1 && mergedCount < primaryBudget) {
    gttStops[mergedCount++] = tempStops1[idx1++];
  }
  while (idx2 < count2 && mergedCount < GTT_MAX) {
    gttStops[mergedCount++] = tempStops2[idx2++];
  }
  while (idx1 < count1 && mergedCount < GTT_MAX) {
    gttStops[mergedCount++] = tempStops1[idx1++];
  }

  lastGttCount = mergedCount;

  // Update error message if both fetches failed
  if (count1 == 0 && count2 == 0) {
    if (error1.length() > 0 && error2.length() > 0) {
      lastGttError = "Both stops failed: " + error1 + " / " + error2;
    } else if (error1.length() > 0) {
      lastGttError = error1;
    } else {
      lastGttError = error2;
    }
  } else if (count1 == 0 && error1.length() > 0) {
    lastGttError = "Stop 1 error: " + error1;
  } else if (count2 == 0 && error2.length() > 0) {
    lastGttError = "Stop 2 error: " + error2;
  }

  // Update timestamp if we got at least some data
  if (mergedCount > 0) {
    lastGttUpdateTime = millis();
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
  tft.fillRect(0, WEATHER_Y, SCREEN_W, WEATHER_H, BG_COLOR);

  // === TEMPERATURE DISPLAY ===
  tft.setFont(&Oswald_SemiBold14pt7b);
  tft.setTextSize(1);
  tft.setTextColor(TEMP_COLOR);

  // Temperature progress bar background
  tft.drawRoundRect(WEATHER_TEMP_BAR_X, WEATHER_TEMP_BAR_Y, WEATHER_BAR_WIDTH, 9, 50, ST77XX_WHITE);
  tft.fillCircle(WEATHER_BAR_INNER_X, WEATHER_TEMP_BAR_Y + 4, 2, TEMP_COLOR);  // Indicator dot

  // Calculate and draw temperature fill on a 0-40 C scale.
  float t = constrain(weatherTemp, static_cast<float>(WEATHER_TEMP_MIN_C), static_cast<float>(WEATHER_TEMP_MAX_C));
  int tempWidth = (int)((WEATHER_BAR_INNER_W * t) / WEATHER_TEMP_MAX_C);
  tft.fillRect(WEATHER_BAR_INNER_X, WEATHER_TEMP_BAR_Y + 1, tempWidth, WEATHER_BAR_INNER_H, TEMP_COLOR);

  // Temperature icon
  drawRGB565_P(TEMP_ICON_X, TEMP_ICON_Y, TEMP_ICON_W, TEMP_ICON_H, TEMP_ICON_RGB565);

  // Temperature text
  char tempText[12];
  snprintf(tempText, sizeof(tempText), "%.1fC", weatherTemp);
  drawTextTopLeft(45, WEATHER_TEXT_Y1, tempText);

  // === HUMIDITY DISPLAY ===
  tft.setTextColor(HUM_COLOR);

  // Humidity progress bar background
  tft.drawRoundRect(WEATHER_HUM_BAR_X, WEATHER_HUM_BAR_Y, WEATHER_BAR_WIDTH, 9, 50, ST77XX_WHITE);
  tft.fillCircle(WEATHER_BAR_INNER_X, WEATHER_HUM_BAR_Y + 4, 2, HUM_COLOR);  // Indicator dot

  // Calculate and draw humidity fill (0-100% scale)
  int h = constrain(weatherHumidity, 0, 100);
  int humiWidth = (int)((WEATHER_BAR_INNER_W * h) / 100.0f);
  tft.fillRect(WEATHER_BAR_INNER_X, WEATHER_HUM_BAR_Y + 1, humiWidth, WEATHER_BAR_INNER_H, HUM_COLOR);

  // Humidity icon
  drawRGB565_P(HUMI_ICON_X, HUMI_ICON_Y, HUMI_ICON_W, HUMI_ICON_H, HUMI_ICON_RGB565);

  // Humidity text
  char humidityText[8];
  snprintf(humidityText, sizeof(humidityText), "%d%%", weatherHumidity);
  drawTextTopLeft(45, WEATHER_TEXT_Y2, humidityText);

  // Weather condition icon (right side)
  drawRGB565_P(MM_ICON_X, MM_ICON_Y, MM_RGB565_W, MM_RGB565_H, MM_RGB565);

  // Reset text color for future draws
  tft.setFont(NULL);
  tft.setTextColor(ST77XX_WHITE);
}

// drawNews()
// Renders a single news headline with automatic word-wrapping
// Displays:
//   - RSS feed icon in top-left corner
//   - Item counter in top-right (e.g., "2/3")
//   - Word-wrapped news headline text
//   - Source attribution footer
// If no news available, shows "News non disponibili" (detailed errors on web only)
// Handles long words and prevents text overflow using intelligent line breaking
// Parameters:
//   index - Index of news item to display (0 to NEWS_MAX-1)
void drawNews(int index) {
  tft.fillScreen(BG_COLOR);

  // RSS Icon
  drawRGB565_P(RSS_ICON_X, RSS_ICON_Y, RSS_ICON_W, RSS_ICON_H, RSS_ICON_RGB565);

  // RSS News
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setFont(&Oswald_Regular10pt7b);

  if (index < 0 || index >= lastNewsCount || newsTitles[index].length() == 0) {
    drawTextTopLeft(NEWS_TEXT_X, NEWS_TEXT_Y, "News non disponibili");
  } else {
    // Current item index (1-based)
    drawTextTopLeft(200, 17, String(index + 1) + "/" + String(lastNewsCount));

    // Normalize text before wrapping (single spaces, no newlines).
    String text = newsTitles[index];
    text.replace("\n", " ");
    text.trim();

    // Word-wrap renderer: wraps on spaces, avoids splitting words.
    String line = "";
    int y = NEWS_TEXT_Y;
    int pos = 0;

    while (pos < text.length() && (y - NEWS_TEXT_Y) <= NEWS_MAX_H) {
      while (pos < text.length() && text[pos] == ' ') pos++;
      int start = pos;
      while (pos < text.length() && text[pos] != ' ') pos++;
      String word = text.substring(start, pos);
      if (word.length() == 0) continue;

      String candidate = (line.length() == 0) ? word : (line + " " + word);
      if (measureTextWidth(candidate) <= NEWS_MAX_W) {
        line = candidate;
        continue;
      }

      if (line.length() > 0) {
        drawTextTopLeft(NEWS_TEXT_X, y, line);
        y += NEWS_LINE_H;
      }

      if (measureTextWidth(word) <= NEWS_MAX_W) {
        line = word;
      } else {
        // Fallback for single words longer than one full line.
        int wp = 1;
        while (wp <= word.length() && (y - NEWS_TEXT_Y) <= NEWS_MAX_H) {
          String chunk = word.substring(0, wp);
          while (measureTextWidth(chunk) <= NEWS_MAX_W && wp <= word.length()) {
            wp++;
            chunk = word.substring(0, wp);
          }
          if (measureTextWidth(chunk) > NEWS_MAX_W) {
            wp--;
            chunk = word.substring(0, wp);
          }

          drawTextTopLeft(NEWS_TEXT_X, y, chunk);
          y += NEWS_LINE_H;
          word.remove(0, wp);
          wp = 1;
        }
        line = "";
      }
    }

    if (line.length() > 0 && (y - NEWS_TEXT_Y) <= NEWS_MAX_H) {
      drawTextTopLeft(NEWS_TEXT_X, y, line);
    }
  }

  // Fonte
  drawTextTopLeft(NEWS_TEXT_X, NEWS_FOOTER_Y, "Fonte: ANSA");
  tft.setFont(NULL);
}

// drawGTT()
// ⚠️ BETA FUNCTION: renders merged GTT bus data on TFT.
// Source model:
//   - Data comes from two configured stops (see GTT_STOP_URL_1/2) and is merged.
// Rendering model:
//   - Fixed grid: up to 4 unique lines, up to 2 departure times per line.
//   - Line label uses SemiBold 10pt; times use Regular 10pt.
// Colors:
//   - Realtime = green, scheduled = grey.
// Fallback:
//   - If no valid rows are available, display "Dati non disponibili".
void drawGTT() {
  tft.fillScreen(BG_COLOR);
  tft.setFont(&Oswald_Regular10pt7b);
  tft.setTextSize(1);
  drawRGB565_P(GTT_ICON_X, GTT_ICON_Y, GTT_ICON_W, GTT_ICON_H, GTT_ICON_RGB565);

  if (lastGttCount == 0) {
    tft.setTextColor(ST77XX_WHITE);
    drawTextCenteredX(0, 113, SCREEN_W, "Dati non disponibili");
    return;
  }

  // Group stops by line using indexes only (avoids temporary String allocations).
  int8_t lineSeedIdx[GTT_LINES_ON_SCREEN];
  int8_t stopIdxByLine[GTT_LINES_ON_SCREEN][GTT_TIMES_PER_LINE];
  uint8_t lineCounts[GTT_LINES_ON_SCREEN];
  for (uint8_t i = 0; i < GTT_LINES_ON_SCREEN; i++) {
    lineSeedIdx[i] = -1;
    lineCounts[i] = 0;
    for (uint8_t j = 0; j < GTT_TIMES_PER_LINE; j++) {
      stopIdxByLine[i][j] = -1;
    }
  }

  for (int i = 0; i < lastGttCount && i < GTT_MAX; i++) {
    int8_t lineIdx = -1;

    // Find existing line bucket.
    for (uint8_t j = 0; j < GTT_LINES_ON_SCREEN; j++) {
      if (lineSeedIdx[j] >= 0 && gttStops[lineSeedIdx[j]].line == gttStops[i].line) {
        lineIdx = static_cast<int8_t>(j);
        break;
      }
    }

    // Create new line bucket when needed.
    if (lineIdx < 0) {
      for (uint8_t j = 0; j < GTT_LINES_ON_SCREEN; j++) {
        if (lineSeedIdx[j] < 0) {
          lineSeedIdx[j] = static_cast<int8_t>(i);
          lineIdx = static_cast<int8_t>(j);
          break;
        }
      }
    }

    if (lineIdx >= 0 && lineCounts[lineIdx] < GTT_TIMES_PER_LINE) {
      stopIdxByLine[lineIdx][lineCounts[lineIdx]] = static_cast<int8_t>(i);
      lineCounts[lineIdx]++;
    }
  }

  // Draw 4 rows (one per line)
  int16_t yPos = GTT_ICON_Y + GTT_ICON_H + 22;
  const int16_t rowHeight = 40;
  const int16_t timeWidth = 70;  // Space per time column

  for (uint8_t lineIdx = 0; lineIdx < GTT_LINES_ON_SCREEN; lineIdx++) {
    if (lineSeedIdx[lineIdx] < 0) break;

    // Draw line number (left column)
    tft.setTextColor(ST77XX_CYAN);
    tft.setFont(&Oswald_SemiBold10pt7b);
    tft.setCursor(10, yPos + 12);
    tft.print(gttStops[lineSeedIdx[lineIdx]].line);

    // Draw 2 time slots for this line
    tft.setFont(&Oswald_Regular10pt7b);
    for (uint8_t timeIdx = 0; timeIdx < GTT_TIMES_PER_LINE; timeIdx++) {
      int16_t xPos = 70 + (timeIdx * timeWidth);

      if (timeIdx < lineCounts[lineIdx]) {
        int8_t stopIdx = stopIdxByLine[lineIdx][timeIdx];
        if (stopIdx < 0) {
          continue;
        }
        // Draw time with color based on realtime flag
        uint16_t timeColor = gttStops[stopIdx].realtime ? ST77XX_GREEN : GTT_SCHEDULED_COLOR;
        tft.setTextColor(timeColor);
        tft.setCursor(xPos, yPos + 12);
        tft.print(gttStops[stopIdx].hour);
      }
    }

    // Thin separator to create a clearer line break between rows.
    if (lineIdx + 1 < GTT_LINES_ON_SCREEN && lineSeedIdx[lineIdx + 1] >= 0) {
      tft.drawFastHLine(10, yPos + 22, 220, GTT_SEPARATOR_COLOR);
    }
    yPos += rowHeight;
  }
}

// drawClock()
// Renders digital clock display in the upper panel
// Shows current time in HH:MM format (24-hour) using Bebas Neue 42pt
// Hours stay white while minutes use CLOCK_MINUTES_COLOR
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

  tft.setFont(&BebasNeue_Regular42pt7b);
  tft.setTextSize(1);
  tft.fillRect(0, 0, SCREEN_W, 115, BG_COLOR);
  if (strlen(timeStr) == 5 && timeStr[2] == ':') {
    char hours[3] = { timeStr[0], timeStr[1], '\0' };
    static const char colon[] = ":";
    char minutes[3] = { timeStr[3], timeStr[4], '\0' };

    int16_t x1, y1;
    uint16_t hoursW, hoursH, colonW, colonH, minutesW, minutesH;
    tft.getTextBounds(hours, 0, 0, &x1, &y1, &hoursW, &hoursH);
    tft.getTextBounds(colon, 0, 0, &x1, &y1, &colonW, &colonH);
    tft.getTextBounds(minutes, 0, 0, &x1, &y1, &minutesW, &minutesH);

    const int16_t totalW = static_cast<int16_t>(hoursW + colonW + minutesW + (CLOCK_COLON_GAP * 2));
    int16_t drawX = (SCREEN_W - totalW) / 2;

    drawX = drawClockSegment(drawX, CLOCK_TOP_Y, hours, ST77XX_WHITE, CLOCK_COLON_GAP);
    drawX = drawClockSegment(drawX, CLOCK_TOP_Y + 15, colon, ST77XX_WHITE, CLOCK_COLON_GAP);
    drawClockSegment(drawX, CLOCK_TOP_Y, minutes, CLOCK_MINUTES_COLOR);
  } else {
    tft.setTextColor(ST77XX_WHITE);
    drawTextCenteredX(0, CLOCK_TOP_Y, SCREEN_W, timeStr);
  }
  tft.setFont(NULL);
}

// =====================================================================
// OTA (Over-The-Air Update) Callback Functions
// =====================================================================
// These callbacks are triggered by ElegantOTA during firmware updates
// Important: avoid SPI display operations while flash is being written.

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
  otaInProgress = false;
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
  if (isWiFiConnected()) return;
  if (now - lastWiFiRetry < WIFI_RETRY_INTERVAL_MS) return;

  lastWiFiRetry = now;
  WiFiConnectResult result = connectWiFi();
  if (result == WiFiConnectResult::Connected) {
    bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_NTP;
    internetCheckFailures = 0;
    lastInternetCheck = now;
    lastInternetCheckOk = true;
    lastInternetCheckHttpCode = 0;
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
  if (!isWiFiConnected()) return;
  if (ntpSynced) return;
  if (now - lastNtpRetry < NTP_RETRY_INTERVAL_MS) return;

  lastNtpRetry = now;
  bootState = BootState::BOOT_NTP;
  ntpSynced = syncNTP();
  bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_DEGRADED;

  // syncNTP() clears the full screen; restore home scene to avoid blank weather area.
  if (currentScene == DisplayScene::SCENE_CLOCK) {
    renderClockScene(false);
    refreshClockTracking(now);
  }
}

// tickInternetHealth()
// Performs periodic internet health-checks while WiFi is connected.
// To avoid unnecessary reconnect churn, forced recovery is triggered only
// after INTERNET_HEALTHCHECK_FAILURE_THRESHOLD consecutive failures.
void tickInternetHealth(unsigned long now) {
  if (!isWiFiConnected()) return;
  if (now - lastInternetCheck < INTERNET_HEALTHCHECK_INTERVAL_MS) return;

  lastInternetCheck = now;

  int httpCode = 0;
  const bool internetOk = checkInternetHealth(httpCode);
  lastInternetCheckHttpCode = httpCode;
  lastInternetCheckOk = internetOk;
  if (internetOk) {
    internetCheckFailures = 0;
    return;
  }

  if (internetCheckFailures < 0xFF) {
    internetCheckFailures++;
  }
  if (internetCheckFailures < INTERNET_HEALTHCHECK_FAILURE_THRESHOLD) {
    return;
  }
  if (now - lastInternetRecoveryAttempt < INTERNET_RECOVERY_COOLDOWN_MS) {
    return;
  }

  lastInternetRecoveryAttempt = now;
  WiFi.disconnect(false);
  delay(50);
  WiFiConnectResult result = connectWiFi();
  if (result != WiFiConnectResult::Connected) {
    bootState = BootState::BOOT_DEGRADED;
    ntpSynced = false;
    return;
  }

  bootState = BootState::BOOT_NTP;
  ntpSynced = syncNTP();
  bootState = ntpSynced ? BootState::BOOT_READY : BootState::BOOT_DEGRADED;
  internetCheckFailures = 0;
  lastInternetCheckOk = true;
  lastInternetCheckHttpCode = 0;
  refreshAfterInternetRecovery(now);
}

// tickInitialDataFetch()
// Retry mechanism: if initial fetch in setup() failed, retries periodically
// Exits immediately once both weather AND news data are successfully fetched
// Retries every 30 seconds if either fetch has error state
void tickInitialDataFetch(unsigned long now) {
  if (initialDataFetched) return;

  // Check if both fetches succeeded (no error messages)
  bool weatherOk = lastWeatherError.isEmpty();
  bool newsOk = lastNewsError.isEmpty();

  if (weatherOk && newsOk) {
    initialDataFetched = true;
    if (currentScene == DisplayScene::SCENE_CLOCK) drawWeather();
    return;
  }

  // If either failed, retry every 30 seconds
  static unsigned long lastRetry = 0;
  if (now - lastRetry < 30000UL) return;

  lastRetry = now;
  if (!weatherOk) fetchWeather();
  if (!newsOk) fetchAnsaRSS(ANSA_RSS_URL);
}

// tickWeather()
// Periodically fetches updated weather data from OpenWeatherMap
// Runs at intervals defined by OWM_INTERVAL_MS (typically 10 minutes)
// Automatically refreshes the weather display panel after successful fetch
// Only updates display when home screen is visible (not during news/GTT scene)
void tickWeather(unsigned long now) {
  if (hasIntervalPassed(lastWeatherFetchTime, OWM_INTERVAL_MS, now)) {
    fetchWeather();
    if (currentScene == DisplayScene::SCENE_CLOCK) drawWeather();
  }
}

// tickNews()
// Periodically fetches updated RSS news feed from ANSA
// Runs at intervals defined by NEWS_INTERVAL_MS (typically 10 minutes)
// Updates news data used by the news scene scheduler
void tickNews(unsigned long now) {
  if (hasIntervalPassed(lastNewsFetchTime, NEWS_INTERVAL_MS, now)) {
    fetchAnsaRSS(ANSA_RSS_URL);
  }
}

// tickGTT()
// ⚠️ BETA FUNCTION: Periodically fetches updated GTT bus stop data
// Runs at intervals defined by GTT_INTERVAL_MS (typically 60 seconds)
// Updates GTT data used by drawGTT() on TFT display
// Handles network errors by exposing explicit error state and empty GTT list
// Note: HTTP requests are synchronous but bounded by short timeouts
// Limitation: Fetches 2 hardcoded stops and merges results
void tickGTT(unsigned long now) {
  if (hasIntervalPassed(lastGttFetchTime, GTT_INTERVAL_MS, now)) {
    fetchGTT(nullptr);  // URL parameter now ignored, using GTT_STOP_URL_1 and GTT_STOP_URL_2
  }
}

// tickClockRefresh()
// Redraws the clock display only when the displayed minute has changed,
// checked every CLOCK_REFRESH_MS while in the clock scene
// Parameters:
//   now - Current timestamp from millis()
void tickClockRefresh(unsigned long now) {
  if (currentScene != DisplayScene::SCENE_CLOCK) return;
  if (!hasIntervalPassed(lastClockRefresh, CLOCK_REFRESH_MS, now)) return;

  time_t t = time(nullptr);
  struct tm* ti = localtime(&t);
  int currentMinute = ntpSynced && ti ? ti->tm_hour * 60 + ti->tm_min : -1;
  if (currentMinute == lastMinute) return;
  lastMinute = currentMinute;
  drawClock();
}

// tickSceneScheduler()
// Manages scene transitions using a state machine: CLOCK → NEWS → GTT → CLOCK
// Displays progress bar indicating time until next scene transition
// Scene durations:
//   - CLOCK: DISPLAY_CLOCK_MS (15s)
//   - NEWS: DISPLAY_NEWS_MS (5s) per item, cycles through all items
//   - GTT: DISPLAY_GTT_MS (15s), or 5s if no data available
// Parameters:
//   now - Current timestamp from millis()
void tickSceneScheduler(unsigned long now) {
  // Disable news scene if no news data available
  if (lastNewsCount <= 0 && currentScene == DisplayScene::SCENE_NEWS) {
    currentScene = DisplayScene::SCENE_CLOCK;
    currentNewsIndex = 0;
  }

  const unsigned long interval = getSceneIntervalMs(currentScene);

  // If not time to switch scenes yet, draw progress bar
  if (now - lastDisplay < interval) {
    drawSceneProgressBar(currentScene, now - lastDisplay, interval);
    return;
  }

  // Time to switch scenes
  lastDisplay = now;

  switch (currentScene) {
    case DisplayScene::SCENE_CLOCK:
      if (lastNewsCount > 0) {
        // Switch to first news item
        currentScene = DisplayScene::SCENE_NEWS;
        currentNewsIndex = 0;
        drawNews(currentNewsIndex);
      } else {
        // Skip news, go to GTT (also when empty to show explicit error screen)
        currentScene = DisplayScene::SCENE_GTT;
        drawGTT();
      }
      break;

    case DisplayScene::SCENE_NEWS:
      currentNewsIndex++;
      if (currentNewsIndex >= lastNewsCount) {
        // News cycle complete, always switch to GTT (even on error/empty data)
        currentScene = DisplayScene::SCENE_GTT;
        drawGTT();
      } else {
        // Show next news item
        drawNews(currentNewsIndex);
      }
      break;

    case DisplayScene::SCENE_GTT:
      // GTT cycle complete, back to clock
      currentScene = DisplayScene::SCENE_CLOCK;
      renderClockScene(true);
      break;

    default:
      // Fallback
      currentScene = DisplayScene::SCENE_CLOCK;
      lastDisplay = now;
      break;
  }
}


// Arduino core
// setup: init hardware, network, endpoints, OTA and first render.
void setup() {
  bootMs = millis();
  analogWriteFreq(5000);  // Increase PWM frequency to reduce backlight flicker during WiFi ops
  analogWriteRange(255);
  setBrightness(DEFAULT_BRIGHTNESS);
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

  lastWeatherFetchTime = millis();
  lastNewsFetchTime = millis();
  lastDisplay = millis();
  lastInternetCheck = millis();

  // GET / -> dashboard HTML (PROGMEM)
  server.on(
    "/",
    []() {
      server.send_P(200, "text/html", INDEX_HTML);
    });

  // GET /gtt -> GTT HTML page (PROGMEM)
  server.on(
    "/gtt",
    []() {
      server.send_P(200, "text/html", GTT_HTML);
    });

  // GET /api -> device status JSON
  server.on(
    "/api",
    []() {
      String json = buildApiJsonPayload();
      server.send(200, "application/json", json);
    });

  // GET /news -> RSS + debug JSON
  server.on(
    "/news",
    []() {
      String json = buildNewsJsonPayload();
      server.send(200, "application/json", json);
    });

  // GET /gtt_data -> GTT + debug JSON (on failure: stops[] + debug.error)
  server.on(
    "/gtt_data",
    []() {
      String json = buildGttJsonPayload();
      server.send(200, "application/json", json);
    });

  // GET /brightness?value=N -> set backlight (0..255)
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

  // Initial fetch before first render.
  showStatusCentered("Fetching data...", ST77XX_WHITE);
  fetchAllData();

  // Mark initial fetch complete only when weather and news are both valid.
  bool weatherOk = lastWeatherError.isEmpty();
  bool newsOk = lastNewsError.isEmpty();
  if (weatherOk && newsOk) {
    initialDataFetched = true;
  }

  // OTA
  if (strlen(OTA_USERNAME) > 0 && strlen(OTA_PASSWORD) > 0) {
    ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  } else {
    ElegantOTA.begin(&server);
  }
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();

  // Render initial state.
  if (isWiFiConnected()) {
    renderClockScene(false);
  } else {
    showStatusCentered(getOfflineStatusMessage(), ST77XX_YELLOW);
    offlineScreenShown = true;
  }

  // Prime clock refresh state.
  refreshClockTracking(millis());
}

// loop: non-blocking scheduler + web/OTA handlers.
void loop() {
  server.handleClient();
  ElegantOTA.loop();

  if (otaInProgress) return;

  unsigned long now = millis();

  if (!isWiFiConnected()) {
    if (!offlineScreenShown) {
      showStatusCentered(getOfflineStatusMessage(), ST77XX_YELLOW);
      offlineScreenShown = true;
    }

    // Freeze UI rotation while offline.
    currentScene = DisplayScene::SCENE_CLOCK;
    currentNewsIndex = 0;
    lastDisplay = now;

    tickWiFiRetry(now);
    return;
  }

  if (offlineScreenShown) {
    // Network restored: refresh data immediately.
    refreshAfterInternetRecovery(now);
  }

  tickWiFiRetry(now);
  tickNtpRetry(now);
  tickInternetHealth(now);
  tickInitialDataFetch(now);
  tickWeather(now);
  tickNews(now);
  tickGTT(now);
  tickClockRefresh(now);
  tickSceneScheduler(now);
}
