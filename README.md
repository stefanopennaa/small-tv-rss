# SmallTV RSS (ESP8266 + ST7789)

<p align="center">
  <img src="https://img.shields.io/badge/version-2026.05.14-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/platform-ESP8266-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/license-MIT-yellow.svg" alt="License">
</p>

Firmware for **GeekMagic SmallTV** with weather, RSS news, NTP clock, and GTT monitoring, including a web dashboard and OTA updates.

> Hobby project (AI-assisted), use at your own risk.

## Overview

- **Weather** from OpenWeatherMap (temperature, humidity, description).
- **NTP clock** with Italy timezone (CET/CEST).
- **ANSA RSS news** with automatic rotation.
- **GTT (beta)** from 2 configured stops, merged into a 4x2 TFT layout.
- **Responsive web dashboard** with brightness control and real-time online status.
- **OTA** via `/update`.
- **WiFi resiliency** with timeout-based reconnect and periodic end-to-end internet health-check.
- **Robust connectivity** with race-condition fixes and atomic status reporting.

## Requirements

- ESP8266 (e.g. D1 mini or compatible)
- Display ST7789 240x240 (GeekMagic SmallTV)
- 2.4 GHz WiFi network
- Arduino libraries: `Adafruit_GFX`, `Adafruit_ST7789`, `ArduinoJson`, `ElegantOTA`

## Quick start

1. Clone the repository.
   ```bash
   git clone https://github.com/stefanopennaa/small-tv-rss.git
   cd small-tv-rss
   ```
2. Configure WiFi/OTA credentials.
   ```bash
   cp wifi_secrets_template.h wifi_secrets.h
   ```
3. Configure the weather API key.
   ```bash
   cp secrets_template.h secrets.h
   ```
4. (Optional) Customize coordinates and stop URLs in `app_config.h`.
5. Open `small-tv-rss.ino` in Arduino IDE, select board/port, and upload the firmware.

## Web endpoints

| Endpoint | Description |
| --- | --- |
| `/` | Main dashboard with online status badge |
| `/gtt` | GTT page with bus times |
| `/api` | Device status JSON (includes `online` flag and timestamp) |
| `/news` | News feed + debug JSON |
| `/gtt_data` | GTT data + debug JSON |
| `/brightness?value=N` | Display brightness (0-255) |
| `/update` | OTA page |

## Main configuration (`app_config.h`)

- Update intervals: `OWM_INTERVAL_MS`, `NEWS_INTERVAL_MS`, `GTT_INTERVAL_MS`
- Connectivity recovery: `WIFI_CONNECT_TIMEOUT_MS`, `WIFI_RETRY_INTERVAL_MS`, `INTERNET_HEALTHCHECK_INTERVAL_MS`, `INTERNET_HEALTHCHECK_FAILURE_THRESHOLD`, `INTERNET_RECOVERY_COOLDOWN_MS`
- Scene durations: `DISPLAY_CLOCK_MS`, `DISPLAY_NEWS_MS`, `DISPLAY_GTT_MS`
- RGB565 color theme: `BG_COLOR`, `TEMP_COLOR`, `HUM_COLOR`, etc.
- Payload safety limits: `RSS_MAX_RESPONSE_SIZE`, `WEATHER_MAX_RESPONSE_SIZE`, `GTT_MAX_RESPONSE_SIZE`
- HTTP timeouts: `HTTP_TIMEOUT_MS` (reduced to 3s for better UI responsiveness)

## Known limitations

- **GTT**: supports 2 fixed stops (`GTT_STOP_URL_1`, `GTT_STOP_URL_2`), no dynamic stop selection.
- **GTT TFT layout**: up to 4 lines, 2 departure times per line.
- **UI error handling**: simplified messages on display/web; details in browser debug and JSON endpoints.

## Quick troubleshooting

| Problem | Check |
| --- | --- |
| WiFi not connected | `wifi_secrets.h`, 2.4 GHz network |
| Missing weather data | `OWM_API_KEY` in `secrets.h` |
| Clock shows `--:--` | Wait for NTP sync with internet access |
| Empty GTT data | Stop URLs and internet connectivity |
| Brightness unchanged | Use `/brightness?value=128` (0-255) |
| Badge showing offline when device works | This is fixed; ensure device boots fresh after firmware update |

## Changelog

### 2026.05.14 (Connectivity & Robustness Audit)
- **CRITICAL FIX**: Removed a JavaScript syntax regression in dashboard `CONFIG` block that could break page rendering (`Unexpected token ':'` in browser console).
- **CRITICAL FIX**: Fixed race condition in main loop - `server.handleClient()` now only runs when WiFi connected, preventing stale responses when connectivity lost.
- **CRITICAL FIX**: Fixed badge "Offline" false positives - API now reports actual `online` status flag instead of inferring from request success.
- Made `tickInternetHealth()` non-blocking - recovery intent is scheduled for next tick instead of blocking 12+ seconds.
- Reduced HTTP timeouts from 3.5s to 3s for better responsiveness and UX.
- Increased client fetch timeout from 5s to 8s to handle slow networks reliably.
- Increased internet health-check interval from 5 min to 10 min and recovery cooldown from 1 min to 2 min (reduces false recovery triggers).
- Added `online` boolean and `ts` (timestamp) fields to `/api` response for accurate client-side status.
- Added input validation and truncation for weather description, news titles/links, GTT data in JSON responses.
- Added IP address validation to prevent "0.0.0.0" from being reported.
- Server no longer serves requests when WiFi is disconnected (prevents misleading "OK" responses).

### 2026.05.11
- Fixed Italy DST rule handling for NTP timezone (`CET/CEST`) to avoid local time offset errors.
- Hardened WiFi connectivity detection to avoid stale-IP "connected" false positives.
- Added periodic end-to-end internet health-check (`INTERNET_HEALTHCHECK_URL`) every 5 minutes.
- Added threshold-based recovery: only after consecutive failed checks, firmware forces WiFi reconnect and repeats NTP/data recovery flow.

### 2026.05.03
- Formal cleanup of source file headers/comments.
- README rewritten and presentation normalized.

### 2026.03.27
- GTT layout updated to 4 lines, 2 times per line.
- `OswaldSemiBold10pt7b` integrated for clearer line numbers.

### 2026.03.26
- User-friendly error messages on display (news/GTT).
- GTT error screen duration reduced to 5 seconds.

### 2026.03.22
- Removed GTT placeholders.
- Direct error propagation to web interface.

### 2026.03.15
- First GTT page integration.
- RAM optimization and hardening.

## Security

- Secrets kept outside the repository (`wifi_secrets.h`, `secrets.h`).
- Endpoint input validation and response truncation.
- Network timeouts and response size limits.
- Server blocks requests when WiFi is disconnected.
- No application-level authentication: use trusted networks.

## License

MIT, see [LICENSE](LICENSE).

## Support

- Issue tracker: [GitHub Issues](https://github.com/stefanopennaa/SmallTV_RSS/issues)
- Email: stefano@stefanopenna.it
