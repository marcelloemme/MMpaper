# MMpaper Update Workflow - SD Strategy

## 🎯 Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  FLASH INTERNA (16MB) - IMMUTABILE                      │
├─────────────────────────────────────────────────────────┤
│  Bootloader                                             │
│  Launcher ← UNICO firmware installato (PERMANENTE!)    │
│  (Niente MMpaper sulla flash!)                         │
└─────────────────────────────────────────────────────────┘
                           ↓ legge da
┌─────────────────────────────────────────────────────────┐
│  microSD (FAT32, max 32GB) - AGGIORNABILE              │
├─────────────────────────────────────────────────────────┤
│  /MMpaper.bin ← Auto-update SCRIVE QUI!                │
│  /other_app.bin (opzionale)                            │
└─────────────────────────────────────────────────────────┘
```

## 🔄 Boot Sequence

```
1. Power ON
   ↓
2. Launcher starts (from flash)
   ↓
3. Launcher splash screen (~2s)
   ↓
4. No button pressed?
   ↓
5. Launcher loads /MMpaper.bin from SD
   ↓
6. MMpaper runs (from RAM, loaded from SD)
   ↓
7. MMpaper checks for updates (every 24h)
   ├─ Update available?
   │  ├─ Download new firmware.bin from GitHub
   │  ├─ Replace /MMpaper.bin on SD
   │  ├─ Restart
   │  └─ Launcher loads NEW version from SD ✅
   └─ No update? Continue running
```

## ✅ Advantages of SD Strategy

| Advantage | Description |
|-----------|-------------|
| **Safety** | Launcher never overwritten = no brick risk |
| **Rollback** | Just copy old .bin to SD, reboot |
| **Flexibility** | Multiple .bin on SD, choose from Launcher |
| **Updates** | Auto-update without touching flash |
| **Recovery** | Launcher always accessible (button during boot) |

## 🆚 Comparison: Flash OTA vs SD Update

| Aspect | Flash OTA (traditional) | SD Update (MMpaper) |
|--------|------------------------|---------------------|
| Update target | Flash OTA partition | /MMpaper.bin on SD |
| Launcher safety | ⚠️ Can be overwritten | ✅ Never touched |
| Rollback | ❌ Complex (need backup) | ✅ Copy old .bin to SD |
| Brick risk | ⚠️ Medium | ✅ Minimal |
| Recovery | ⚠️ Need reflash | ✅ Launcher menu |
| Speed | ⚡ Fast (flash) | 🐢 Slower (SD) |

## 📋 Complete Update Workflow

### Initial Setup (once)

```bash
# 1. Flash Launcher to device (web flasher)
https://bmorcelli.github.io/Launcher/webflasher.html

# 2. Compile MMpaper
pio run

# 3. Copy to SD card as /MMpaper.bin
cp .pio/build/PaperS3/firmware.bin /Volumes/SD/MMpaper.bin

# 4. Insert SD into PaperS3

# 5. Power on → Launcher menu
#    Select: /MMpaper.bin → Load
#    (Note: NOT "Install" - just "Load"!)

# 6. MMpaper runs from SD ✅
```

### Development Cycle

```bash
# 1. Modify code
vim src/main.cpp

# 2. Update version
vim include/config.h  # Change FIRMWARE_VERSION

# 3. Update manifest
vim firmware.json     # Change version

# 4. Compile
pio run

# 5. Copy and rename binary
cp .pio/build/PaperS3/firmware.bin ./MMpaper.bin

# 6. Commit & push
git add -A
git commit -m "Release v0.X.X"
git push

# 7. Within 24h: ALL devices auto-update! 🎉
```

### Auto-Update Process (device side)

```
Day 1, 00:00:
  MMpaper v0.1.0 running from SD

Day 1, 24:00:
  Check GitHub: firmware.json
  → Remote version: 0.2.0
  → Local version: 0.1.0
  → UPDATE NEEDED!

  Steps:
  1. WiFi connect
  2. Download firmware.bin → /MMpaper_new.bin (temp)
  3. Delete /MMpaper.bin (old)
  4. Rename /MMpaper_new.bin → /MMpaper.bin
  5. Restart

  Launcher loads:
  → /MMpaper.bin (now v0.2.0) ✅
```

## 🛠️ Manual Operations

### Update SD manually (without auto-update)

```bash
# Compile new version
pio run

# Copy to SD (device off or via card reader)
cp .pio/build/PaperS3/firmware.bin /Volumes/SD/MMpaper.bin

# Reboot device
# Launcher loads new version ✅
```

### Rollback to previous version

```bash
# Option 1: From backup on SD
cp /Volumes/SD/MMpaper_v0.1.0_backup.bin /Volumes/SD/MMpaper.bin

# Option 2: From old GitHub release
# Download old firmware.bin from GitHub releases
# Copy to SD as /MMpaper.bin

# Reboot → old version restored ✅
```

### Test new version without overwriting

```bash
# Copy new version with different name
cp firmware.bin /Volumes/SD/MMpaper_test.bin

# Boot → Launcher menu (hold button)
# Select: /MMpaper_test.bin → Load
# Test new version
# If good: rename to /MMpaper.bin
```

## ⚠️ Important Notes

### SD Card Requirements
- Format: FAT32
- Type: SDHC (not SDXC)
- Size: Max 32GB (recommended 8-16GB)
- File: `/MMpaper.bin` (exact name!)

### WiFi Required for Auto-Update
- Auto-update needs WiFi connection
- Configure SSID/password in `include/config.h`
- Update skipped if no WiFi (safe fallback)

### Battery Management
- Auto-update skipped if battery < 30%
- Download ~1.1MB consumes ~0.5mAh
- Update time: ~30s on good WiFi

### Version Comparison
- Simple string comparison: "0.1.0" vs "0.2.0"
- SemVer format recommended: major.minor.patch
- Must match exactly in config.h and firmware.json

## 🔍 Troubleshooting

### Update not working?

**Check serial monitor (115200 baud):**
```
=== AUTO-UPDATE CHECK ===
SD card initialized ✅
Connecting to WiFi... ✅
WiFi connected! ✅
Checking version at: https://... ✅
Current version: 0.1.0
Remote version: 0.2.0
New version found! Downloading to SD...
Downloaded: 100 KB / 1100 KB
...
Download complete! 1123497 bytes written ✅
New firmware.bin installed on SD! ✅
Rebooting to Launcher...
```

**Common issues:**
- SD card not detected → check formatting (FAT32)
- WiFi timeout → check credentials in config.h
- Download fails → check GitHub URL in firmware.json
- Version unchanged → check firmware.json version number

### Launcher doesn't load MMpaper?

- Check filename: must be exactly `/MMpaper.bin`
- Check SD format: must be FAT32
- Check file size: should be ~1.1MB
- Try loading manually from Launcher menu

### MMpaper stuck in update loop?

- Bad firmware.json on GitHub (fix and push)
- Battery too low (charge device)
- SD card corrupted (reformat and copy fresh .bin)

## 📊 Performance Metrics

| Metric | Value |
|--------|-------|
| Firmware size | ~1.1 MB |
| Download time | ~30s @ good WiFi |
| Battery cost | ~0.5 mAh |
| SD write time | ~5s |
| Total update | ~40-60s |
| Boot time | ~5s (Launcher + MMpaper) |

## 🚀 Future Enhancements

Possible improvements:
- [ ] Delta updates (only changed bytes)
- [ ] Multiple firmware slots on SD
- [ ] A/B partition on SD (safe rollback)
- [ ] Progress bar on e-ink during download
- [ ] Checksum verification (MD5/SHA256)
- [ ] Update scheduling (specific time/day)

---

**Summary**: Launcher = permanent bootloader, MMpaper = updatable app on SD!
