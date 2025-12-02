# Image Folder

This folder contains images for the M5PaperS3 display.

## 📁 Structure

```
image/
├── current.jpg          ← Image displayed on device (960x540)
├── image_meta.json      ← Metadata (timestamp, MD5)
├── photo1.jpg           ← Your local photos (not tracked by Git)
├── photo2.jpg           ← Your local photos (not tracked by Git)
└── ...                  ← Add as many as you want locally!
```

**Only `current.jpg` and `image_meta.json` are tracked by Git.**

All other files in this folder are ignored (see `.gitignore`).

## 🖼️ Display Specs

- **Resolution**: 540 × 960 pixels (PORTRAIT mode)
- **Aspect ratio**: 9:16 (portrait - vertical orientation)
- **Orientation**: Bottom side has the wider bezel with mounting holes
- **Format**: JPEG (recommended quality: 85-95%)
- **Color**: Grayscale (16 levels - color images automatically converted)

## 📐 Image Processing

The `update_image.sh` script automatically handles different image sizes:

### ✅ What it does:
- **Maintains aspect ratio** (no stretching/distortion)
- **Smart crop to center** if aspect ratio differs
- **Scales up or down** as needed
- **Converts to grayscale** (16 levels)

### 📏 Examples:

| Input Size | Input Aspect | What Happens |
|------------|--------------|--------------|
| 1080×1920 | 9:16 (portrait) | ✅ Perfect! Scales to 540×960 |
| 2000×3000 | 2:3 (portrait) | ✅ Scales + crops top/bottom to fit 9:16 |
| 3000×4000 | 3:4 (portrait) | ✅ Scales + crops top/bottom to fit 9:16 |
| 1920×1080 | 16:9 (landscape) | ✅ Scales + crops sides to fit 9:16 |
| 1200×1200 | 1:1 (square) | ✅ Scales + crops top/bottom to fit 9:16 |
| 500×800 | Any portrait | ✅ Upscales (may be slightly pixelated) |

### 🎯 Cropping Behavior (PORTRAIT MODE):
- **Taller than 9:16** (e.g., 2:3, 3:4): Crops top/bottom
- **Wider than 9:16** (e.g., 16:9 landscape, 1:1 square): Crops left/right sides
- **Center gravity**: Always crops from center, keeping main subject visible

### 💡 Tips:
- For best results, use **portrait photos** close to 9:16 aspect ratio (540×960, 1080×1920, etc.)
- Landscape photos (16:9) will be cropped heavily on the sides
- Square images will lose top/bottom portions
- The crop is intelligent: centers the image before cropping

## 🚀 Quick Update

### Using the helper script (recommended):

```bash
./update_image.sh path/to/your/photo.jpg "Description"
```

The script will:
1. Resize image to 960x540
2. Copy to `image/current.jpg`
3. Generate metadata with MD5 hash
4. Optionally commit and push to GitHub

### Manual update:

```bash
# 1. Process your image (smart crop, no distortion)
convert my_photo.jpg \
  -resize 960x540^ \
  -gravity center \
  -extent 960x540 \
  -quality 90 \
  image/current.jpg

# 2. Update metadata
echo '{"updated":"'$(date -u +%Y-%m-%dT%H:%M:%SZ)'","md5":"'$(md5 -q image/current.jpg)'"}' > image/image_meta.json

# 3. Commit and push
git add image/current.jpg image/image_meta.json
git commit -m "Update display image"
git push
```

**Explanation of convert command:**
- `-resize 960x540^` = Scale to fill (minimum dimension = 960 or 540)
- `-gravity center` = Crop from center
- `-extent 960x540` = Final exact size 960×540
- `-quality 90` = JPEG quality (85-95 recommended)

## 📱 Device Behavior

MMpaper checks for new images:
- **At boot** (after firmware check)
- **Every 3 hours** from 6:00 AM to midnight (6:00, 9:00, 12:00, 15:00, 18:00, 21:00, 00:00)

When a new image is detected:
1. Downloads `image_meta.json` (1 KB)
2. Compares MD5 hash
3. If different, downloads `current.jpg` (~200-400 KB)
4. Displays image fullscreen
5. Goes to sleep until next check

## 💡 Tips

- Keep local copies with descriptive names (`vacation_beach.jpg`, `family_2024.jpg`, etc.)
- Only copy the one you want to display as `current.jpg`
- Device updates automatically when you push changes
- To force immediate update: restart the device

## 🔋 Power Consumption

- Image check: ~2-3 seconds WiFi ON
- Download: ~5-10 seconds WiFi ON (only if image changed)
- Display: 0 mA when static (e-ink magic!)
- Between checks: deep sleep (~0.01 mA)
