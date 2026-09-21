#!/usr/bin/env bash
set -euo pipefail

FEED_URL="https://api.cityofkingston.ca/gtfs/gtfs.zip"
DEST="$(dirname "$0")/../data"

mkdir -p "$DEST"
curl -L -o "$DEST/kingston.zip" "$FEED_URL"
unzip -o "$DEST/kingston.zip" -d "$DEST/kingston"

echo "Feed extracted to $DEST/kingston"