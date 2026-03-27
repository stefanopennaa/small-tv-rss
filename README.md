# 📺 SmallTV RSS - ESP8266 Weather + RSS + GTT

<p align="center">
  <img src="https://img.shields.io/badge/version-2026.03.26-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/platform-ESP8266-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/license-MIT-yellow.svg" alt="License">
</p>

Smart weather station + RSS news + GTT bus monitor for **GeekMagic SmallTV** with NTP clock, OTA updates, and web dashboard.

> Amateur project with AI-assisted code. Use at your own risk.

---

## ✨ Features

- 🌡️ **Weather**: OpenWeatherMap API with temperature/humidity panel
- 🕐 **NTP Clock**: Italy timezone (CET/CEST) with large clock display
- 📰 **RSS Feed**: ANSA top news with automatic rotation
- 🚌 **GTT** ⚠️ **BETA**: Turin bus stop data (2 fixed stops merged, compact 4×2 display)
- 🌐 **Web Dashboard**: Bootstrap 5, mobile responsive, brightness control
- 🔄 **OTA Updates**: Wireless firmware updates via `/update`
- 🎨 **Custom UI**: Auto scene rotation, custom fonts and icons

---

## 🛠️ Requirements

- **Board**: ESP8266 (D1 mini or compatible)
- **Display**: ST7789 240x240 TFT (GeekMagic SmallTV)
- **WiFi**: 2.4GHz network
- **Libraries**: Adafruit_GFX, Adafruit_ST7789, ArduinoJson, ElegantOTA

---

## 🚀 Quick Setup

**1. Clone repository:**
```bash
git clone https://github.com/stefanopennaa/SmallTV_RSS.git
cd SmallTV_RSS
```

**2. Configure WiFi:**
```bash
cp wifi_secrets.example.h wifi_secrets.h
# Edit wifi_secrets.h: set WIFI_SSID and WIFI_PASSWORD
```

**3. Configure API key:**
```bash
cp secrets.example.h secrets.h
# Edit secrets.h: set OWM_API_KEY (get it from openweathermap.org)
```

**4. (Optional) Customize location/feeds in `config.h`:**
```cpp
constexpr char OWM_LAT[] = "45.0703";  // Your latitude
constexpr char OWM_LON[] = "7.6869";   // Your longitude
constexpr char GTT_STOP_URL_1[] = "https://gpa.madbob.org/query.php?stop=3445";  // First bus stop ID
constexpr char GTT_STOP_URL_2[] = "https://gpa.madbob.org/query.php?stop=3742";  // Second bus stop ID
```

**5. Upload:**
- Open `SmallTV_RSS.ino` in Arduino IDE
- Select ESP8266 board and port
- Upload and visit `http://DEVICE_IP`

---

## 🌐 Web Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | Main dashboard |
| `/gtt` | GTT bus stops page |
| `/api` | JSON status (weather, device info) |
| `/news` | JSON news feed + debug info |
| `/gtt_data` | JSON GTT data + debug info |
| `/brightness?value=N` | Set backlight (0-255) |
| `/update` | OTA firmware update |

---

## ⚙️ Configuration

Edit `config.h` for:
- **Update intervals**: `OWM_INTERVAL_MS`, `NEWS_INTERVAL_MS`, `GTT_INTERVAL_MS`
- **Display durations**: `DISPLAY_CLOCK_MS`, `DISPLAY_NEWS_MS`, `DISPLAY_GTT_MS`
- **Colors**: `BG_COLOR`, `TEMP_COLOR`, etc. (RGB565)
- **Safety limits**: `RSS_MAX_RESPONSE_SIZE`, `WEATHER_MAX_RESPONSE_SIZE`

---

## ⚠️ Known Limitations

**GTT Feed (BETA):**
- Supports **2 fixed stops** (`GTT_STOP_URL_1`, `GTT_STOP_URL_2`) merged into one list
- TFT layout is fixed to **4 lines × 2 times per line**
- No nearby stops discovery or web UI selector
- Future: dynamic stop selection, favorites, countdown timers

**Error Display:**
- Display shows user-friendly messages ("Dati non disponibili", "News non disponibili")
- GTT error screen shows for 5 seconds only
- Web interface shows simplified messages, errors logged to browser console

---

## 🔧 Troubleshooting

| Issue | Solution |
|-------|----------|
| No WiFi | Check `wifi_secrets.h`, verify 2.4GHz network |
| No weather | Verify `OWM_API_KEY` in `secrets.h` |
| Clock shows `--:--` | Wait for NTP sync (requires internet) |
| GTT not working | Check `GTT_STOP_URL_1`, `GTT_STOP_URL_2` and internet connection |
| Brightness not changing | Use `/brightness?value=128` (0-255) |

---

## 📝 Changelog

### v2026.03.27 - GTT Layout Refresh
- GTT scene now shows up to **4 bus lines** (instead of 3)
- Each line now shows up to **2 departure times** (instead of 3) to avoid wrapping
- Added and integrated `OswaldSemiBold10pt7b` for clearer line numbers
- README and inline comments aligned with current GTT behavior

### v2026.03.26 - Display Error Messages
- Display shows user-friendly error messages: "Dati non disponibili" (GTT), "News non disponibili" (News)
- GTT error screen duration reduced to 5 seconds (from 15s)
- Web interface shows simplified "Dati non disponibili" message
- Technical errors logged to browser console for debugging

### v2026.03.22 - GTT Error Visibility
- Removed placeholder GTT stops
- Direct error propagation to web interface

### v2026.03.15 - GTT Integration
- Added GTT web page
- RAM optimization and hardening

---

## 🔐 Security

**Implemented:**
- Input validation on endpoints
- Network timeouts and response size limits
- Secrets separated from repository

**Limitations:**
- No authentication (LAN use only)
- Credentials in plaintext on device
- Use on trusted networks only

---

## 📄 License

MIT License - see [LICENSE](LICENSE)

---

## 🙏 Credits

- **Libraries**: Adafruit, ArduinoJson, ElegantOTA, Bootstrap
- **Data**: OpenWeatherMap, ANSA RSS, GTT, NTP
- **Hardware**: GeekMagic SmallTV

---

## 📧 Support

- **Issues**: [GitHub Issues](https://github.com/stefanopennaa/SmallTV_RSS/issues)
- **Email**: stefano@stefanopenna.it

<p align="center">Built for ESP8266 tinkerers and tiny displays.</p>
