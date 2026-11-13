#!/usr/bin/env bash
# Usage: ./upload.sh <title>

TITLE="$1"
if [ -z "$TITLE" ]; then
    echo "Usage: ./upload.sh <title>"
    exit 1
fi

SITE_DIR="/home/krishnang/krishnang.dev-personal-website-project"

# 1. Push changes in blogWRITER repo
echo "[upload] Pushing blogWRITER changes to GitHub..."
git add .
if ! git diff-index --quiet HEAD --; then
    git commit -m "added $TITLE"
    git push
else
    echo "[upload] blogWRITER repo is clean (nothing to commit)."
    git push
fi

# 2. Push changes in personal website repo if present
if [ -d "$SITE_DIR/.git" ]; then
    echo "[upload] Pushing website changes to GitHub..."
    (
        cd "$SITE_DIR" || exit 1
        git add .
        if ! git diff-index --quiet HEAD --; then
            git commit -m "added $TITLE"
            git push
        else
            echo "[upload] Website repo is clean (nothing to commit)."
            git push
        fi
    )
fi

echo "✓ Successfully pushed changes to GitHub for '$TITLE'!"
