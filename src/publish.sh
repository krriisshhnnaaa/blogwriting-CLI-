#!/usr/bin/env bash

TITLE="$1"
if [ -z "$TITLE" ]; then
    echo "Usage: ./publish.sh <title>"
    exit 1
fi

SITE_DIR="/home/krishnang/krishnang.dev-personal-website-project"
TARGET_DIR="$SITE_DIR/blog/$TITLE"
DATE=$(date +%Y-%m-%d)

if [ -x "./blogwriter" ]; then
    ./blogwriter make "$TITLE"
elif [ -x "$(dirname "$0")/../blogwriter" ]; then
    "$(dirname "$0")/../blogwriter" make "$TITLE"
else
    blogwriter make "$TITLE"
fi

mkdir -p "$TARGET_DIR"

cp "blogs/$TITLE.html" "$TARGET_DIR/"
[ -f "blogs/$TITLE.jpg" ] && cp "blogs/$TITLE.jpg" "$TARGET_DIR/"
[ -f "blogs/bg.jpg" ] && cp "blogs/bg.jpg" "$TARGET_DIR/"
[ -f "blogs/wood.jpg" ] && cp "blogs/wood.jpg" "$TARGET_DIR/"

if [ ! -f "temp/$TITLE.txt" ] && [ -f "$TITLE.txt" ]; then
    mkdir -p temp
    cp "$TITLE.txt" "temp/$TITLE.txt"
fi
DESC=$(grep -v '^#' "temp/$TITLE.txt" | grep -v '^[[:space:]]*$' | head -n 1 | cut -c 1-140)

cat <<EOF > "$TARGET_DIR/post.md"
---
title: "$TITLE"
date: "$DATE"
description: "$DESC"
cover: "$TITLE.jpg"
url: "/blog/$TITLE/$TITLE.html"
---

EOF
cat "temp/$TITLE.txt" >> "$TARGET_DIR/post.md"

python3 -c "
import json
p = '$SITE_DIR/blog-index.json'
with open(p, 'r') as f:
    data = json.load(f)
entry = 'blog/$TITLE/post.md'
if entry not in data:
    data.insert(0, entry)
    with open(p, 'w') as f:
        json.dump(data, f, indent=2)
    print(f'Added {entry} to blog-index.json')
else:
    print(f'{entry} already registered')
"

echo "✓ Successfully published '$TITLE' to $TARGET_DIR and registered on krishnang.dev!"
