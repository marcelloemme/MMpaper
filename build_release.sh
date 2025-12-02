#!/bin/bash
# build_release.sh - Build and prepare MMpaper binary for release

set -e  # Exit on error

echo "=========================================="
echo "  MMpaper Release Build Script"
echo "=========================================="
echo ""

# 1. Check if source files changed
echo "📋 Checking for source code changes..."
if git diff --quiet HEAD src/ include/ platformio.ini 2>/dev/null; then
    echo "⚠️  No source code changes detected since last commit"
    echo "   Binary is likely up to date"
    echo ""
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Aborted"
        exit 1
    fi
fi

# 2. Clean old build
echo ""
echo "🧹 Cleaning old build..."
rm -rf .pio/build/PaperS3/src/
rm -f .pio/build/PaperS3/firmware.*

# 3. Compile firmware
echo ""
echo "🔨 Compiling firmware..."
~/.platformio/penv/bin/pio run

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed!"
    exit 1
fi

# 4. Copy and rename binary
echo ""
echo "📦 Preparing release binary..."
cp .pio/build/PaperS3/firmware.bin ./MMpaper.bin

# 5. Show file info
echo ""
echo "✅ Build complete!"
echo ""
echo "📊 Binary info:"
ls -lh MMpaper.bin
echo ""
md5 MMpaper.bin | sed 's/MD5 (/MD5: /' | sed 's/)//'

# 6. Show size comparison
OLD_SIZE=$(git show HEAD:MMpaper.bin 2>/dev/null | wc -c)
NEW_SIZE=$(wc -c < MMpaper.bin)

if [ $OLD_SIZE -ne 0 ]; then
    DIFF=$((NEW_SIZE - OLD_SIZE))
    if [ $DIFF -gt 0 ]; then
        echo "📈 Size change: +${DIFF} bytes (larger)"
    elif [ $DIFF -lt 0 ]; then
        echo "📉 Size change: ${DIFF} bytes (smaller)"
    else
        echo "📊 Size unchanged"
    fi
fi

echo ""
echo "=========================================="
echo "✅ Ready to commit and push!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Update version in include/config.h"
echo "  2. Update version in firmware.json"
echo "  3. git add MMpaper.bin firmware.json include/config.h"
echo "  4. git commit -m 'Release vX.X.X'"
echo "  5. git push"
echo ""
