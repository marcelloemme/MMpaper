# Release Checklist

## 🤔 Do I need to recompile?

### ✅ YES - Recompile if you changed:
- [ ] `src/*.cpp` or `src/*.h` files
- [ ] `include/*.h` files
- [ ] `platformio.ini` (libraries, build flags)
- [ ] Any code that affects the binary

### ❌ NO - Don't recompile if you only changed:
- [ ] `README.md`, `*.md` documentation files
- [ ] `.gitignore`, `.gitattributes`
- [ ] Comments in code (they're stripped during compilation)
- [ ] `firmware.json` (only metadata, not compiled)

---

## 📋 Release Process

### Quick Method (use script):
```bash
./build_release.sh
```

### Manual Method:

#### 1. Modify Code
```bash
vim src/main.cpp
vim include/config.h
```

#### 2. Update Version
```bash
# In include/config.h
#define FIRMWARE_VERSION "0.2.0"  # Change version

# In firmware.json
"version": "0.2.0"  # Change version
```

#### 3. Compile
```bash
~/.platformio/penv/bin/pio run
```

#### 4. Copy Binary
```bash
cp .pio/build/PaperS3/firmware.bin ./MMpaper.bin
```

#### 5. Verify
```bash
# Check size
ls -lh MMpaper.bin

# Check hash changed
md5 MMpaper.bin
git show HEAD:MMpaper.bin | md5  # Compare with previous
```

#### 6. Commit & Push
```bash
git add MMpaper.bin firmware.json include/config.h
git commit -m "Release v0.2.0: Description of changes"
git push
```

---

## 🔍 How to Check if Binary Needs Update

### Check if source changed since last binary update:
```bash
# When was MMpaper.bin last updated?
git log -1 --format="%ai" -- MMpaper.bin

# What source files changed since then?
git diff $(git log -1 --format="%H" -- MMpaper.bin) HEAD -- src/ include/ platformio.ini
```

### Compare binary hashes:
```bash
# Current compiled binary
md5 .pio/build/PaperS3/firmware.bin

# Binary in repository
md5 MMpaper.bin

# If different → need to update MMpaper.bin!
```

---

## ⚠️ Common Mistakes

### ❌ DON'T:
- Commit code changes without updating `MMpaper.bin`
- Change version in code but forget `firmware.json`
- Push without testing compilation first
- Forget to update version number

### ✅ DO:
- Always recompile after code changes
- Update both `config.h` and `firmware.json` versions
- Test compilation before committing
- Use semantic versioning (major.minor.patch)

---

## 🎯 Version Numbering (SemVer)

```
v0.1.0 → v0.1.1  = Patch (bug fix)
v0.1.0 → v0.2.0  = Minor (new feature, backward compatible)
v0.1.0 → v1.0.0  = Major (breaking change)
```

### Examples:
- `v0.1.0 → v0.1.1`: Fix WiFi timeout bug
- `v0.1.0 → v0.2.0`: Add battery level display
- `v0.1.0 → v1.0.0`: Complete rewrite with new API

---

## 📊 Release History Template

Keep track in git tags or CHANGELOG.md:

```markdown
## v0.2.0 - 2024-12-03
### Added
- Battery level indicator on display
- Deep sleep mode for power saving

### Fixed
- WiFi connection timeout issue
- SD card detection bug

### Changed
- Improved auto-update reliability
```

---

## 🚀 Automated Checks (Future)

Consider adding to `.git/hooks/pre-commit`:
```bash
#!/bin/bash
# Check if source changed but binary didn't update

if git diff --cached --name-only | grep -q "^src/\|^include/"; then
    if ! git diff --cached --name-only | grep -q "^MMpaper.bin"; then
        echo "⚠️  Warning: Source code changed but MMpaper.bin not updated"
        echo "   Did you forget to recompile?"
        exit 1
    fi
fi
```

---

**TL;DR**:
- Code changed? Run `./build_release.sh`
- Docs changed? Just commit docs, no recompile needed
