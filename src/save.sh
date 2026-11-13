#!/usr/bin/env bash
# Usage: ./save.sh <title>

TITLE="$1"
if [ -z "$TITLE" ]; then
    echo "Usage: ./save.sh <title>"
    exit 1
fi

SITE_DIR="/home/krishnang/krishnang.dev-personal-website-project"
TARGET_DIR="$SITE_DIR/blog/$TITLE"

# Create destination directory in the website project
mkdir -p "$TARGET_DIR"

# Locate <title>.md (check root then temp/)
MD_FILE=""
if [ -f "$TITLE.md" ]; then
    MD_FILE="$TITLE.md"
elif [ -f "temp/$TITLE.md" ]; then
    MD_FILE="temp/$TITLE.md"
elif [ -f "$(dirname "$0")/../$TITLE.md" ]; then
    MD_FILE="$(dirname "$0")/../$TITLE.md"
elif [ -f "$(dirname "$0")/../temp/$TITLE.md" ]; then
    MD_FILE="$(dirname "$0")/../temp/$TITLE.md"
fi

if [ -z "$MD_FILE" ]; then
    echo "Error: $TITLE.md not found in current directory or temp/"
    exit 1
fi

# Save <title>.md to path : /home/krishnang/krishnang.dev-personal-website-project/blog/<title>
cp "$MD_FILE" "$TARGET_DIR/$TITLE.md"
cp "$MD_FILE" "$TARGET_DIR/post.md"

echo "✓ Successfully saved $TITLE.md to $TARGET_DIR"
