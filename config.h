#pragma once

#include <Arduino.h>

// =====================================================================
// Hardware Configuration
// =====================================================================

// TFT Display Pin Assignments (GeekMagic SmallTV Hardware)
// ST7789 240x240 color display using SPI communication
constexpr uint8_t TFT_DC = 0;         // Data/Command select pin (GPIO0)
constexpr uint8_t TFT_RST = 2;        // Hardware reset pin (GPIO2)
constexpr uint8_t TFT_CS = 15;        // SPI chip select pin (GPIO15)
constexpr uint8_t TFT_BACKLIGHT = 5;  // PWM backlight control (GPIO5) - Note: Inverted logic

// =====================================================================
// Weather API Configuration (OpenWeatherMap)
// =====================================================================

// Geographic coordinates for weather data (Turin, Italy)
constexpr char OWM_LAT[] = "45.0703";  // Latitude
constexpr char OWM_LON[] = "7.6869";   // Longitude

// Weather data refresh interval (10 minutes)
// Adjust based on API rate limits and data freshness requirements
constexpr unsigned long OWM_INTERVAL_MS = 600000UL;

// =====================================================================
// RSS News Feed Configuration (ANSA)
// =====================================================================

// ANSA Top News RSS feed URL (Italian news agency)
constexpr char ANSA_RSS_URL[] = "https://www.ansa.it/sito/notizie/topnews/topnews_rss.xml";

// Maximum number of news items to fetch and store
constexpr int NEWS_MAX = 3;

// News feed refresh interval (10 minutes)
constexpr unsigned long NEWS_INTERVAL_MS = 600000UL;

// Scene Display Durations
// Controls how long each screen type is visible before switching
constexpr unsigned long DISPLAY_CLOCK_MS = 10000UL;  // Clock scene: 10 seconds
constexpr unsigned long DISPLAY_NEWS_MS = 5000UL;    // News scene: 5 seconds per item

// =====================================================================
// Color Theme (RGB565 Format)
// =====================================================================

constexpr uint16_t BG_COLOR = ST77XX_BLACK;  // Background: Black
constexpr uint16_t TEMP_COLOR = 0xFD20;      // Temperature: Orange/Red
constexpr uint16_t HUM_COLOR = 0x07FF;       // Humidity: Cyan/Blue
constexpr uint16_t DESC_COLOR = 0xC618;      // Weather description: Purple

// =====================================================================
// Screen Layout Configuration
// =====================================================================

// Display Dimensions
constexpr int16_t SCREEN_W = 240;  // Screen width in pixels
constexpr int16_t SCREEN_H = 240;  // Screen height in pixels

// Panel Heights (screen is split horizontally)
constexpr int16_t TOP_PANEL_H = 124;  // Upper panel: clock display
constexpr int16_t WEATHER_Y = 125;    // Weather panel Y start position
constexpr int16_t WEATHER_H = 115;    // Weather panel height

// Icon Positions (X, Y coordinates for top-left corner)
// Boot/Status Icons
constexpr int16_t WIFI_ICON_X = 104;  // WiFi animation icon (centered)
constexpr int16_t WIFI_ICON_Y = 90;
constexpr int16_t SYNC_ICON_X = 104;  // NTP sync animation icon (centered)
constexpr int16_t SYNC_ICON_Y = 90;

// Weather Display Icons
constexpr int16_t MM_ICON_X = 140;  // Large weather condition icon (right side)
constexpr int16_t MM_ICON_Y = 145;
constexpr int16_t TEMP_ICON_X = 25;  // Temperature indicator icon
constexpr int16_t TEMP_ICON_Y = 150;
constexpr int16_t HUMI_ICON_X = 15;  // Humidity indicator icon
constexpr int16_t HUMI_ICON_Y = 195;

// News Display Icons
constexpr int16_t RSS_ICON_X = 10;  // RSS feed icon (top-left)
constexpr int16_t RSS_ICON_Y = 10;

// Status Message Position
constexpr int16_t STATUS_TEXT_X = 10;  // Boot/error message text position
constexpr int16_t STATUS_TEXT_Y = 10;

// =====================================================================
// Marquee (Scrolling Text) Configuration
// =====================================================================

constexpr unsigned long MARQUEE_INTERVAL_MS = 500;  // Animation frame interval (milliseconds)
constexpr int16_t MARQUEE_Y = 100;                  // Vertical position on screen
constexpr int16_t MARQUEE_H = 16;                   // Text height (pixels)
constexpr int16_t MARQUEE_STEP_PX = 5;              // Horizontal scroll distance per frame

// =====================================================================
// Boot Sequence and Timing Configuration
// =====================================================================
// All timeout values are in milliseconds
// Timeouts prevent indefinite blocking during network operations

// Animation Frame Timing
constexpr unsigned long ANIMATION_FRAME_INTERVAL_MS = 250;  // Frame duration for WiFi/NTP animations

// WiFi Connection Timing
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;  // Maximum time to wait for WiFi connection (12 sec)
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;   // Retry interval if WiFi disconnects (10 sec)
constexpr unsigned long WIFI_STATUS_DELAY_MS = 250;       // Status message display duration
constexpr unsigned long WIFI_CONNECTED_DELAY_MS = 1000;   // Success message display duration

// NTP Synchronization Timing
constexpr unsigned long NTP_SYNC_TIMEOUT_MS = 7000;     // Maximum time to wait for NTP sync (7 sec)
constexpr unsigned long NTP_MIN_SYNC_ANIM_MS = 1250;    // Minimum animation duration for user feedback
constexpr unsigned long NTP_RETRY_INTERVAL_MS = 60000;  // Retry interval if sync fails (60 sec)
constexpr unsigned long NTP_STATUS_DELAY_MS = 250;      // Status message display duration

// Data Fetching Timing
// Delays first HTTP request so UI appears responsive immediately after boot
constexpr unsigned long INITIAL_DATA_FETCH_DELAY_MS = 1200;  // Delay before first weather/news fetch (1.2 sec)

// =====================================================================
// Network Request Configuration
// =====================================================================
// Short timeouts ensure UI remains responsive even on slow/unreliable networks

// HTTP Request Timeouts
constexpr uint16_t WEATHER_HTTP_TIMEOUT_MS = 2500;  // OpenWeatherMap API timeout (2.5 sec)
constexpr uint16_t RSS_HTTP_TIMEOUT_MS = 3500;      // RSS feed fetch timeout (3.5 sec)

// Request Retry Logic
// RSS feeds can fail due to TLS handshake issues or redirects - retry once automatically
constexpr uint8_t RSS_HTTP_ATTEMPTS = 2;    // Number of attempts for RSS fetch
constexpr uint8_t RSS_RETRY_DELAY_MS = 30;  // Delay between retry attempts

// Response Size Limits (security/stability)
constexpr size_t RSS_MAX_RESPONSE_SIZE = 32768;     // Maximum RSS feed size (32 KB)
constexpr size_t WEATHER_MAX_RESPONSE_SIZE = 4096;  // Maximum weather API response (4 KB)

// =====================================================================
// Graphics Rendering Configuration
// =====================================================================

// Maximum icon width for line-buffered RGB565 rendering
// Limits RAM usage during icon drawing (1 line buffer)
// Icons wider than this will be clipped to prevent buffer overflow
constexpr int16_t RGB565_LINE_MAX = 80;

// String Formatting Safety
constexpr uint8_t JSON_ESCAPE_BUFFER_MARGIN = 16;  // Extra buffer space for JSON escape sequences
constexpr uint8_t TIME_STRING_BUFFER_SIZE = 10;    // Safe buffer size for time strings (HH:MM + null)
