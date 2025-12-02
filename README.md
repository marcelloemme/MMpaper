# MMpaper - Auto-Updating E-Ink App for M5PaperS3

Self-updating e-ink application for M5Stack PaperS3 with automatic GitHub releases integration.

## Features

- ✅ **Auto-update from GitHub releases** - Checks for new versions every 24h
- ✅ **Smart power management** - WiFi/IMU off by default, battery-aware updates
- ✅ **E-ink optimized** - Smart refresh management (partial/full refresh)
- ✅ **Graceful fallback** - Works offline if no WiFi available
- ✅ **User feedback** - Update progress shown on e-ink display

## Hardware Requirements

- M5Stack PaperS3 (ESP32-S3, 4.7" e-ink display)
- microSD card (optional, for Launcher)
- WiFi connection (for auto-updates)

## Setup

### 1. Configure WiFi & GitHub

Edit `include/config.h`:

```cpp
#define GITHUB_USER "your_github_username"  // Your GitHub username
#define GITHUB_REPO "MMpaper"               // This repository name

#define WIFI_SSID "YourWiFiSSID"           // Your WiFi network
#define WIFI_PASSWORD "YourPassword"        // Your WiFi password
```

### 2. Build & Upload

```bash
# Clone repository
git clone https://github.com/YOUR_USER/MMpaper.git
cd MMpaper

# Build with PlatformIO
pio run

# Upload to device
pio run --target upload
```

### 3. Create GitHub Release

After making changes:

```bash
# Build firmware
pio run

# Create GitHub release
gh release create v0.1.0 \
  .pio/build/PaperS3/firmware.bin \
  --title "Initial Release" \
  --notes "First auto-updating version"
```

Or via GitHub web UI:
1. Go to Releases → New Release
2. Tag: `v0.1.0` (use SemVer)
3. Upload: `.pio/build/PaperS3/firmware.bin`
4. Publish release

## How Auto-Update Works

```
Boot → Check (every 24h) → GitHub API → New version?
                                            ├─ Yes → Download → Install → Restart
                                            └─ No  → Start app normally
```

**Conditions for update:**
- 24 hours passed since last check
- WiFi available
- Battery level ≥ 30%

**Fallback:**
- No WiFi → Skip update, start app
- Battery low → Skip update, start app
- Download fails → Start old version

## Usage with Launcher

1. Flash [BMorcelli Launcher](https://bmorcelli.github.io/Launcher/webflasher.html)
2. Copy `firmware.bin` to microSD as `MMpaper.bin`
3. Launcher menu → Install `MMpaper.bin`
4. Reboot → MMpaper auto-starts and checks for updates

## Configuration Options

All in `include/config.h`:

```cpp
UPDATE_CHECK_INTERVAL      // 24h default
MIN_BATTERY_PERCENT        // 30% minimum
WIFI_CONNECT_TIMEOUT       // 10s WiFi timeout
FULL_REFRESH_MIN_INTERVAL  // 10s between full refreshes
PARTIAL_REFRESH_MAX_COUNT  // 5 partial before full refresh
ENABLE_IMU                 // false (battery saving)
```

## Power Consumption

| Mode | Current | Battery Life (1800mAh) |
|------|---------|----------------------|
| Deep sleep | 9.28 µA | ~220 days |
| Idle (no IMU) | ~1 mA | ~75 days |
| WiFi update | ~150 mA | ~12 hours |
| Display refresh | ~200 mA (2-3s) | - |

**Estimated battery life**: 3-4 weeks with daily updates, 2-3 months static

## Development

### Project Structure

```
MMpaper/
├── include/
│   └── config.h           # Configuration (WiFi, GitHub, timings)
├── src/
│   └── main.cpp           # Main application with auto-update logic
├── platformio.ini         # PlatformIO configuration
└── .claude.md             # Project documentation (development notes)
```

### Adding Features

1. Edit `src/main.cpp` → `runApp()` function
2. Update `FIRMWARE_VERSION` in `config.h`
3. Build and create new GitHub release
4. Devices auto-update within 24h

### Debugging

Monitor serial output (115200 baud):

```bash
pio device monitor
```

Look for:
- `=== AUTO-UPDATE CHECK ===`
- `Checking GitHub for new version...`
- Update success/failure messages

## Troubleshooting

**Update not working?**
- Check WiFi credentials in `config.h`
- Verify GitHub username/repo name
- Ensure GitHub release has `firmware.bin` attached
- Check battery level (≥30%)
- Wait 24h or reset update timer:
  ```cpp
  prefs.begin("mmconfig", false);
  prefs.putULong("lastUpdateCheck", 0);
  prefs.end();
  ```

**Display issues?**
- E-ink ghosting: Normal after many partial refreshes
- Wait for full refresh (every 5 partial or 10s interval)
- Manual full refresh: restart device

## Credits

- [M5Unified](https://github.com/m5stack/M5Unified) - M5Stack library
- [EPDIY](https://github.com/vroland/epdiy) - E-ink driver
- [esp_ghota](https://github.com/Fishwaldo/esp_ghota) - GitHub OTA updates
- [Launcher](https://github.com/bmorcelli/Launcher) - Firmware launcher

## License

MIT License - See LICENSE file

---

**Note**: This is a starting template. Implement your app logic in `runApp()` function.
