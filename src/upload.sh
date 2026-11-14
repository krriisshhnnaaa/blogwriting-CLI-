#!/usr/bin/env bash

TITLE="$1"
if [ -z "$TITLE" ]; then
    echo "Usage: ./upload.sh <title>"
    exit 1
fi

SITE_DIR="/home/krishnang/krishnang.dev-personal-website-project"

echo "[upload] Pushing blogWRITER changes to GitHub..."
git add .
if ! git diff-index --quiet HEAD --; then
    git commit -m "added $TITLE"
    git push
else
    echo "[upload] blogWRITER repo is clean (nothing to commit)."
    git push
fi

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
