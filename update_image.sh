#!/bin/bash
# update_image.sh - Helper script to update display image

set -e

if [ $# -eq 0 ]; then
    echo "Usage: ./update_image.sh <image_path> [description]"
    echo ""
    echo "Example:"
    echo "  ./update_image.sh my_photo.jpg \"Sunset in Rome\""
    echo ""
    echo "Note: Image will be resized to 960x540 for M5PaperS3 display"
    exit 1
fi

IMAGE_PATH="$1"
DESCRIPTION="${2:-Display image updated}"

if [ ! -f "$IMAGE_PATH" ]; then
    echo "❌ Error: Image file not found: $IMAGE_PATH"
    exit 1
fi

echo "📸 Processing image: $IMAGE_PATH"

# Check if ImageMagick is installed
if command -v convert &> /dev/null; then
    echo "🔧 Resizing to 960x540..."
    convert "$IMAGE_PATH" -resize 960x540\! -quality 90 image/current.jpg
else
    echo "⚠️  ImageMagick not installed, copying without resize"
    echo "   Install with: brew install imagemagick"
    cp "$IMAGE_PATH" image/current.jpg
fi

# Generate metadata
echo "📝 Generating metadata..."
TIMESTAMP=$(date -u +%Y-%m-%dT%H:%M:%SZ)
MD5=$(md5 -q image/current.jpg)

cat > image/image_meta.json <<EOF
{
  "updated": "$TIMESTAMP",
  "md5": "$MD5",
  "description": "$DESCRIPTION"
}
EOF

echo ""
echo "✅ Image prepared!"
echo "   Updated: $TIMESTAMP"
echo "   MD5: $MD5"
echo ""
echo "Next steps:"
echo "  git add image/current.jpg image/image_meta.json"
echo "  git commit -m \"Update display: $DESCRIPTION\""
echo "  git push"
echo ""

# Optionally auto-commit
read -p "Auto-commit and push? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git add image/current.jpg image/image_meta.json
    git commit -m "Update display: $DESCRIPTION"
    git push
    echo ""
    echo "🚀 Pushed to GitHub! Device will update at next check."
fi
