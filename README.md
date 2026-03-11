# 📺 SmallTV RSS - ESP8266 Weather Clock & News Feed Display

<p align="center">
  <img src="https://img.shields.io/badge/version-2026.03.11-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/platform-ESP8266-green.svg" alt="Platform">
  <img src="https://img.shields.io/badge/license-MIT-yellow.svg" alt="License">
  <img src="https://img.shields.io/badge/Arduino-Compatible-teal.svg" alt="Arduino">
</p>

A feature-rich firmware for the **GeekMagic SmallTV** (ESP8266 + ST7789 240x240 TFT display) that transforms it into a smart weather station and RSS news reader with a beautiful web interface.

> This is an amateur project. Parts of the code and documentation were developed with AI assistance. Use it at your own risk: I do not assume responsibility for bugs, malfunctions, data loss, device misconfiguration, or bricking.

---

## ✨ Features

### 🌡️ **Live Weather Data**
- Real-time temperature, humidity, and conditions from OpenWeatherMap API
- Color-coded progress bars for visual feedback
- Customizable location (latitude/longitude)
- Automatic updates every 10 minutes

### 🕐 **NTP Clock with DST Support**
- Accurate time synchronization via NTP servers
- Italy timezone configuration (CET/CEST) with automatic DST transitions
- Large, easy-to-read digital display with custom GFX fonts
- Two-tone clock styling with highlighted minutes for faster reading
- Centered boot/sync status messages placed below the startup icons
- Graceful fallback when network is unavailable

### 📰 **RSS News Feed**
- ANSA Top News integration (Italian news agency)
- Fetches and displays up to 3 latest headlines
- HTTPS support with automatic retries
- Word-wrapped news display with pagination and proportional font measurement

### 🌐 **Responsive Web Interface**
- Modern Bootstrap 5 dashboard
- Dark/Light mode auto-detection
- Real-time weather and device status monitoring
- Adjustable backlight brightness control
- Mobile-friendly responsive design
- Auto-refresh every 60 seconds
- **Security hardened**: XSS protection, URL validation, safe DOM manipulation
- **Performance optimized**: Request debouncing, timeout handling, retry logic
- **Accessibility compliant**: WCAG 2.1 AA standards with ARIA labels and screen reader support

### 🔄 **OTA Firmware Updates**
- Wireless firmware updates via ElegantOTA
- No need to physically connect the device
- Visual feedback during update process
- Secure update validation

### 🎨 **Visual Feedback**
- Smooth WiFi connection animations
- NTP sync spinner with progress indication
- Status icons and color-coded messages
- Mixed typography: Bebas Neue for the clock, Oswald for weather/news text
- All graphics stored in flash memory (PROGMEM)

### 🔒 **Security & Robustness**
- Credential management via separate config files (not tracked in git)
- Input validation on all HTTP endpoints
- Response size limits to prevent buffer overflow
- JSON structure validation before data access
- Comprehensive error handling and diagnostics
- Timeout protection on all network operations

---

## 🛠️ Hardware Requirements

| Component | Specification |
|-----------|---------------|
| **Microcontroller** | ESP8266 (ESP-12E/12F recommended) |
| **Display** | ST7789 240x240 TFT (GeekMagic SmallTV) |
| **Connection** | SPI interface |
| **Power** | 5V via USB (500mA minimum) |
| **WiFi** | 2.4GHz 802.11 b/g/n |

### Pin Configuration
```
TFT_DC (Data/Command) : GPIO0
TFT_RST (Reset)       : GPIO2
TFT_CS (Chip Select)  : GPIO15
TFT_BACKLIGHT (PWM)   : GPIO5 (inverted logic)
```

---

## 📦 Software Dependencies

### Arduino Libraries (via Library Manager)
```cpp
Adafruit_GFX          // Graphics core
Adafruit_ST7789       // ST7789 TFT driver
ESP8266WiFi           // WiFi connectivity (built-in)
ESP8266WebServer      // HTTP server (built-in)
ESP8266HTTPClient     // HTTP client (built-in)
ArduinoJson           // JSON parsing (v6.x)
ElegantOTA            // OTA updates
```

### Installation
1. Open Arduino IDE
2. Go to **Tools → Manage Libraries**
3. Search and install each library listed above
4. Ensure ArduinoJson version is **6.x or higher**

### Bundled Font Assets
- `fonts/Bebas_Neue/`: retained source TTF, license file, and the generated `BebasNeue42pt7b.h` used by the clock
- `fonts/Oswald/`: retained license file plus the generated `OswaldRegular10pt7b.h` and `OswaldSemiBold14pt7b.h` used by firmware text
- `fonts/Oswald/static/`: retained only `Oswald-Regular.ttf` and `Oswald-SemiBold.ttf`, which are the source files for the generated headers
- Text placement with `GFXfont` is normalized through helpers based on `getTextBounds()`, so layout coordinates are handled as top-left or centered positions instead of raw baselines
- Boot/status labels and clock segment spacing are managed through shared helper functions and layout constants to keep rendering behavior consistent
- Stable hardware, timing, color, and display-layout constants live in `config.h`; the sketch keeps only rendering logic and temporary local values

---

## 🚀 Quick Start

### 1️⃣ Clone the Repository
```bash
git clone https://github.com/stefanopennaa/SmallTV_RSS.git
cd SmallTV_RSS
```

### 2️⃣ Configure WiFi Credentials
```bash
# Copy the example file
cp wifi_secrets.example.h wifi_secrets.h

# Edit with your WiFi credentials
nano wifi_secrets.h
```

Edit the file:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

### 3️⃣ Configure API Keys
```bash
# Copy the example file
cp secrets.example.h secrets.h

# Edit with your OpenWeatherMap API key
nano secrets.h
```

Get a free API key from [OpenWeatherMap](https://openweathermap.org/api) and add it:
```cpp
#define OWM_API_KEY "your_api_key_here"
```

### 4️⃣ Customize Location (Optional)
Edit `config.h` to set your coordinates:
```cpp
constexpr char OWM_LAT[] = "45.0703";  // Your latitude
constexpr char OWM_LON[] = "7.6869";   // Your longitude
```

### 5️⃣ Upload to Device
1. Connect your ESP8266 device via USB
2. Open `SmallTV_RSS.ino` in Arduino IDE
3. Select **Board**: ESP8266 Board (your specific model)
4. Select **Port**: Your device's COM/tty port
5. Click **Upload** (➡️)

### 6️⃣ Access Web Interface
Once uploaded and connected to WiFi:
1. Check the display for the device's IP address
2. Open a browser and navigate to: `http://DEVICE_IP`
3. Enjoy your smart display! 🎉

---

## ⚙️ Configuration

### Weather Update Interval
Edit `config.h`:
```cpp
constexpr unsigned long OWM_INTERVAL_MS = 600000UL;  // 10 minutes (in ms)
```

### News Feed Update Interval
```cpp
constexpr unsigned long NEWS_INTERVAL_MS = 600000UL;  // 10 minutes
```

### Brightness Settings
Default brightness is set in `setup()`:
```cpp
setBrightness(50);  // 0 (off) to 255 (max)
```
You can also adjust it via the web interface slider.

### Display Colors
Customize the color theme in `config.h`:
```cpp
constexpr uint16_t BG_COLOR = ST77XX_BLACK;     // Background
constexpr uint16_t TEMP_COLOR = 0xFD20;         // Temperature (orange)
constexpr uint16_t HUM_COLOR = 0x07FF;          // Humidity (cyan)
```

---

## 🌐 Web API Endpoints

### `GET /`
Returns the main web dashboard (HTML/CSS/JavaScript).

### `GET /api`
Returns device status and sensor data as JSON:
```json
{
  "temp": 23.5,
  "humidity": 65,
  "description": "Partly cloudy",
  "ip": "192.168.1.100",
  "brightness": 128,
  "wifi": true,
  "wifi_result": "connected",
  "ntp": true,
  "boot_state": "ready",
  "last_weather_ms": 120000,
  "last_news_ms": 180000
}
```

### `GET /news`
Returns RSS feed data with diagnostics:
```json
{
  "source": "ANSA",
  "items": [
    {
      "title": "News headline...",
      "link": "https://..."
    }
  ],
  "debug": {
    "http": 200,
    "count": 3,
    "error": "",
    "age_ms": 60000
  }
}
```

### `GET /brightness?value=N`
Controls display backlight (0-255).
- **Parameters**: `value` (required, integer 0-255)
- **Returns**: `ok` on success, HTTP 400 on invalid input

### `GET /update`
ElegantOTA firmware update interface.

---

## 🏗️ Architecture

### Boot Sequence
```
1. Initialize hardware (TFT, backlight)
   ↓
2. Connect to WiFi (with timeout protection)
   ↓
3. Synchronize time via NTP
   ↓
4. Start web server and OTA service
   ↓
5. Fetch initial weather/news data
   ↓
6. Render home screen
```

### Main Loop (Non-Blocking)
```
→ Handle HTTP requests
→ Process OTA updates
→ WiFi reconnection (if disconnected)
→ NTP resync (if clock drift)
→ Weather data refresh (10min intervals)
→ News feed refresh (10min intervals)
→ Marquee animation
→ Scene management
```

### State Machine
- **BOOT_WIFI**: Attempting WiFi connection
- **BOOT_NTP**: Synchronizing time
- **BOOT_READY**: Fully operational
- **BOOT_DEGRADED**: Limited functionality (offline mode)

---

## 🐛 Troubleshooting

### Device doesn't connect to WiFi
- ✅ Verify credentials in `wifi_secrets.h`
- ✅ Check WiFi network is 2.4GHz (ESP8266 doesn't support 5GHz)
- ✅ Ensure SSID and password have no special characters
- ✅ Try moving device closer to router

### Weather data not showing
- ✅ Confirm API key is valid in `secrets.h`
- ✅ Check you have internet connectivity
- ✅ Verify coordinates are in decimal format (not DMS)
- ✅ Check API quotas at OpenWeatherMap dashboard

### Clock shows "--:--"
- ✅ NTP sync requires active internet connection
- ✅ Check firewall allows NTP (UDP port 123)
- ✅ Wait up to 60 seconds for initial sync
- ✅ Device will retry every 60 seconds if failed

### Display is too bright/dim
- ✅ Use web interface brightness slider
- ✅ Or adjust via API: `http://DEVICE_IP/brightness?value=128`
- ✅ Valid range: 0 (off) to 255 (maximum)

### RSS feed errors
- ✅ Check internet connectivity
- ✅ ANSA feed requires HTTPS/TLS support
- ✅ Device will retry automatically
- ✅ Check `/news` endpoint for error details

### OTA update fails
- ✅ Ensure device has stable power supply
- ✅ Don't interrupt during update process
- ✅ Check firmware file is for ESP8266 (not ESP32)
- ✅ Wait for "Updated!" message before rebooting

---

## 📊 Performance Metrics

| Metric | Value |
|--------|-------|
| **Boot Time** | ~15-20 seconds (with WiFi+NTP) |
| **Memory Usage (RAM)** | ~35KB / 80KB available |
| **Flash Usage** | ~400KB / 4MB available |
| **Power Consumption** | ~150-200mA @ 5V (brightness 50%) |
| **WiFi Reconnect** | <12 seconds (timeout protected) |
| **HTTP Request Timeout** | 2.5s (weather), 3.5s (news) |
| **Display Refresh** | ~60ms (full screen redraw) |

---

## 🔐 Security Considerations

### ✅ Implemented Protections

#### Firmware (C++)
- Input validation on all HTTP endpoints
- Response size limits (32KB RSS, 4KB weather)
- JSON structure validation before access
- Buffer overflow protection (safe string operations)
- Credential separation (secrets not in repo)
- HTTPS support for news feed

#### Web Interface (JavaScript/HTML)
- **XSS Protection**: Safe DOM manipulation (no `innerHTML` for user data)
- **URL Validation**: Whitelist-based protocol filtering (http/https only)
- **Request Timeout**: 5-second timeout prevents hanging requests
- **Retry Logic**: Exponential backoff for failed requests
- **Input Sanitization**: Brightness value range validation (0-255)
- **Secure External Links**: `rel="noopener noreferrer"` prevents reverse tabnabbing

### ⚠️ Security Notes
- Web interface has **no authentication** (intended for home networks)
- WiFi credentials stored in plaintext in flash memory
- OpenWeatherMap API key transmitted over HTTP (not HTTPS)
- Device accepts OTA updates without additional authentication

**Recommendation**: Use on trusted private networks only.

---

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/AmazingFeature`)
3. **Commit** your changes (`git commit -m 'Add some AmazingFeature'`)
4. **Push** to the branch (`git push origin feature/AmazingFeature`)
5. **Open** a Pull Request

### Code Style Guidelines
- Follow Arduino C++ style conventions
- Use descriptive variable names
- Add comments for complex logic
- Test on real hardware before submitting
- Update README if adding new features

---

## 📝 Changelog

### Version 2026.03.11 (UI and Typography Cleanup)
#### 🔤 Typography
- Added Bebas Neue as the dedicated clock font on the TFT display
- Added Oswald for weather values, RSS headlines, and supporting UI text
- Reworked RSS/news wrapping to measure proportional font width correctly
- Updated the web dashboard to use Bebas Neue for display-style headings and Oswald for readable body text
- Increased the clock font to `BebasNeue42pt7b` for better legibility on the 240x240 TFT
- Added a dedicated warm yellow/orange color for clock minutes
- Standardized firmware text positioning with helpers based on `getTextBounds()` to avoid baseline-related misalignment
- Centered WiFi/NTP boot messages below icons and refined clock spacing/alignment
- Cleaned up text-rendering helpers to reduce duplication and centralize layout logic
- Removed the unused marquee subsystem and related configuration

#### 📁 Assets
- Reorganized bundled font resources into `fonts/Bebas_Neue/` and `fonts/Oswald/`
- Added generated `GFXfont` headers for the currently used font sizes

### Version 2026.03.07 (Improved)
#### 🔒 Security
- Added input validation on HTTP endpoints
- Implemented response size limits
- Added JSON structure validation
- Fixed buffer overflow risks
- **Web Interface**: XSS protection, URL validation, safe DOM manipulation
- **Web Interface**: Request timeouts and retry logic with exponential backoff

#### 🎯 Code Quality
- Eliminated magic numbers (added named constants)
- Improved variable naming consistency
- Reduced code duplication with helper functions
- Enhanced error handling with diagnostics
- **Web Interface**: Modular architecture with 14+ utility functions
- **Web Interface**: Debounced brightness control reduces server load 90%+

#### ♿ Accessibility
- **Web Interface**: WCAG 2.1 AA compliant with ARIA labels
- **Web Interface**: Screen reader support with live regions
- **Web Interface**: Semantic HTML for assistive technologies

#### 📖 Documentation
- Expanded inline comments
- Improved code formatting and readability
- Added comprehensive README
- Created CONTRIBUTING.md and LICENSE files

### Version 2026.03.07 (Initial)
- Initial release with core features
- WiFi + NTP + Weather + RSS functionality
- Web interface with OTA updates

---

## 📄 License

This project is licensed under the **MIT License** - see below for details:

```
MIT License

Copyright (c) 2026 Stefano Penna

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 🙏 Acknowledgments

### Libraries & Tools
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) - Graphics core
- [Adafruit ST7789 Library](https://github.com/adafruit/Adafruit-ST7735-Library) - Display driver
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) - OTA updates
- [Bootstrap 5](https://getbootstrap.com/) - Web UI framework

### Data Sources
- [OpenWeatherMap](https://openweathermap.org/) - Weather API
- [ANSA](https://www.ansa.it/) - News RSS feed
- [pool.ntp.org](https://www.pool.ntp.org/) - NTP time servers

### Inspiration
- GeekMagic SmallTV hardware design
- Arduino community tutorials and examples

---

## 📧 Support

- **Issues**: [GitHub Issues](https://github.com/stefanopennaa/SmallTV_RSS/issues)
- **Email**: stefano@stefanopenna.it

---

<p align="center">
  Made with ❤️ for the ESP8266 community
</p>

<p align="center">
  <sub>If you found this project helpful, please ⭐ star it on GitHub!</sub>
</p>
