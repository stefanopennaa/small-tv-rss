# 📺 SmallTV RSS - ESP8266 Weather Clock, RSS & GTT Feed

<p align="center">
  <img src="https://img.shields.io/badge/version-2026.03.15-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/platform-ESP8266-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/license-MIT-yellow.svg" alt="License">
</p>

Smart weather station + RSS news reader + GTT stop monitor for **GeekMagic SmallTV** with NTP clock, OTA updates, and responsive web dashboard.

> Amateur project with AI-assisted code. Use at your own risk.

---

## ✨ Features

- 🌡️ **Live Weather**: OpenWeatherMap API, temperature/humidity panel, auto-update every 10 minutes
- 🕐 **NTP Clock**: Italy timezone (CET/CEST with DST), large Bebas Neue clock font
- 📰 **RSS Feed**: ANSA top news (3 headlines), HTTPS fetch, retry logic
- 🚌 **GTT Feed** ⚠️ **BETA**: Turin bus stop data (`/gtt_data`) with unified fallback (same data on TFT + web)
- 🌐 **Web Dashboard**: Bootstrap 5, mobile responsive, brightness slider, status panels
- 🔄 **OTA Updates**: Wireless firmware update via ElegantOTA (`/update`)
- 🎨 **Custom UI**: Scene rotation (clock/news/GTT), weather + RSS + GTT icons, custom GFX fonts
- 🔒 **Hardening**: Input validation, HTTP timeouts, response size limits, secrets separated from repo

---

## 🛠️ Requirements

| Component | Details |
|-----------|---------|
| **Board** | ESP8266 (ESP-12E/12F, D1 mini, compatible) |
| **Display** | ST7789 240x240 TFT (GeekMagic SmallTV) |
| **Pin Config** | TFT: DC=GPIO0, RST=GPIO2, CS=GPIO15, BL=GPIO5 |
| **Power** | 5V USB |
| **WiFi** | 2.4GHz 802.11 b/g/n |

### Dependencies

```cpp
Adafruit_GFX, Adafruit_ST7789
ArduinoJson (6.x)
ElegantOTA
ESP8266WiFi, ESP8266WebServer, ESP8266HTTPClient (ESP8266 core)
```

**Fonts included**:
- Bebas Neue (`fonts/Bebas_Neue/`) for clock
- Oswald (`fonts/Oswald/`) for UI/news

---

## 🚀 Setup (5 steps)

**1. Clone and enter project:**

```bash
git clone https://github.com/stefanopennaa/SmallTV_RSS.git
cd SmallTV_RSS
```

**2. Configure WiFi:**

```bash
cp wifi_secrets.example.h wifi_secrets.h
# Edit wifi_secrets.h and set WIFI_SSID / WIFI_PASSWORD
```

**3. Configure API key:**

```bash
cp secrets.example.h secrets.h
# Edit secrets.h and set OWM_API_KEY
```

**4. (Optional) Set location/feed/timings:**
Edit `config.h`:

```cpp
constexpr char OWM_LAT[] = "45.0703";
constexpr char OWM_LON[] = "7.6869";
constexpr char ANSA_RSS_URL[] = "https://www.ansa.it/sito/notizie/topnews/topnews_rss.xml";
constexpr char GTT_STOP_URL[] = "https://gpa.madbob.org/query.php?stop=3445";  // ⚠️ BETA: Currently supports only 1 fixed stop (ID 3445)
```

**5. Upload firmware:**
- Open `SmallTV_RSS.ino` in Arduino IDE
- Select ESP8266 board + serial port
- Upload and open `http://DEVICE_IP`

---

## ⚙️ Configuration

Main knobs in `config.h`:

- **Weather interval**: `OWM_INTERVAL_MS` (default `600000`)
- **News interval**: `NEWS_INTERVAL_MS` (default `600000`)
- **GTT interval**: `GTT_INTERVAL_MS` (default `60000`)
- **Scene durations**: `DISPLAY_CLOCK_MS`, `DISPLAY_NEWS_MS`, `DISPLAY_GTT_MS`
- **Brightness default**: `setBrightness(50)` in `setup()`
- **Theme colors**: `BG_COLOR`, `TEMP_COLOR`, `HUM_COLOR`, etc. (RGB565)
- **Safety limits**:
  - `RSS_MAX_RESPONSE_SIZE = 32768`
  - `WEATHER_MAX_RESPONSE_SIZE = 4096`
  - `GTT_MAX_RESPONSE_SIZE = 4096`

---

## 🌐 Web API

| Endpoint | Purpose |
|----------|---------|
| `GET /` | Main dashboard page |
| `GET /gtt` | Dedicated GTT page |
| `GET /api` | Device/weather status JSON |
| `GET /news` | RSS + debug JSON |
| `GET /gtt_data` | GTT + debug JSON |
| `GET /brightness?value=N` | Backlight control (`0-255`) |
| `GET /update` | OTA update interface |

Example `GET /api` response (shortened):

```json
{
  "temp": 23.5,
  "humidity": 65,
  "description": "Partly cloudy",
  "ip": "192.168.1.100",
  "brightness": 128,
  "wifi": true,
  "ntp": true,
  "boot_state": "ready",
  "fw_version": "2026.03.15"
}
```

---

## 🏗️ Architecture

**Boot sequence:**

```text
Init display -> WiFi connect -> NTP sync -> HTTP routes + OTA -> Initial fetch -> Render
```

**Main loop tasks:**

- Handle HTTP requests and OTA loop
- WiFi reconnect retry
- NTP re-sync retry
- Periodic weather/news/GTT refresh
- Scene switching (`CLOCK -> NEWS -> GTT -> CLOCK`)

**Boot states:**

- `BOOT_WIFI`
- `BOOT_NTP`
- `BOOT_READY`
- `BOOT_DEGRADED`

---

## ⚠️ Beta Features

### GTT Feed (Transit Data)

The GTT (Gruppo Torinese Trasporti) feed is **currently in BETA** with the following limitations:

- **Single stop only**: Currently monitors a single hardcoded bus stop (ID 3445)
- **No nearby stops**: Cannot auto-detect stops near your location
- **No stop selection UI**: Stop ID must be changed manually in `config.h`

**Future improvements** (roadmap):
- Multi-stop support from `/gtt_data` endpoint
- Nearby stops discovery by GPS/address
- Web UI stop selector and favorites
- Real-time arrival countdown

For now, use the fallback dataset if the API is unavailable.

---

| Issue | Quick checks |
|-------|--------------|
| No WiFi | Verify `wifi_secrets.h`, 2.4GHz SSID, signal strength |
| Weather missing | Check `OWM_API_KEY` in `secrets.h`, API quota, lat/lon |
| Clock shows `--:--` | NTP requires internet; wait retry cycle (60s) |
| RSS empty/errors | Check internet/TLS availability and ANSA feed reachability |
| GTT unavailable | Check `GTT_STOP_URL`; fallback dataset is used automatically |
| **GTT limitations** ⚠️ | **BETA feature**: Currently supports only 1 fixed bus stop. Nearby stops and custom stop selection coming soon. |
| Brightness not changing | Use `/brightness?value=128` (range `0-255`) |
| OTA failed | Keep device powered, retry from `/update` |

---

## 🔐 Security Notes

Implemented:

- Input validation on brightness endpoint
- Bounded network timeouts and retry caps
- Payload size limits for weather/RSS/GTT responses
- JSON parsing with structure checks
- `secrets.h` / `wifi_secrets.h` excluded from git

Limitations:

- No authentication on web endpoints (LAN only)
- Credentials stored in plaintext on device flash
- OTA route not protected by additional auth

Recommendation: use only on trusted private networks.

---

## 📝 Changelog

### v2026.03.15 - GTT Integration & Hardening

- Added dedicated GTT web page in `gtt_html.h`
- Unified GTT fallback dataset for TFT + `/gtt_data`
- RAM/payload hardening and robustness improvements

### v2026.03.11 - UI & Typography Refresh

- Added/updated Bebas Neue + Oswald GFX fonts
- Improved clock and news readability on 240x240 TFT

---

## 🧪 Build (arduino-cli)

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini SmallTV_RSS.ino
```

---

## 🤝 Contributing

1. Fork the repository
2. Create your branch (`git checkout -b feature/my-feature`)
3. Commit (`git commit -m 'Add my feature'`)
4. Push (`git push origin feature/my-feature`)
5. Open a Pull Request

---

## 📄 License

MIT License - see [LICENSE](LICENSE).

---

## 🙏 Credits

- **Libraries**: Adafruit GFX, Adafruit ST7789, ArduinoJson, ElegantOTA, Bootstrap 5
- **Data providers**: OpenWeatherMap, ANSA RSS, GTT, pool.ntp.org
- **Hardware**: GeekMagic SmallTV

---

## 📧 Support

- **Issues**: [GitHub Issues](https://github.com/stefanopennaa/SmallTV_RSS/issues)
- **Email**: stefano@stefanopenna.it

<p align="center">Built for ESP8266 tinkerers and tiny displays.</p>
