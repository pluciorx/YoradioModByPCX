#!/bin/bash

SRC_DIR="./www"
DST_DIR="./www_gz"

mkdir -p "$DST_DIR"

EXTENSIONS=("html" "css" "js" "json" "svg" "txt")

find "$SRC_DIR" -type f | while read -r file; do
    ext="${file##*.}"

    for e in "${EXTENSIONS[@]}"; do
        if [[ "$ext" == "$e" ]]; then
            rel_path="${file#$SRC_DIR/}"
            out_file="$DST_DIR/$rel_path.gz"

            mkdir -p "$(dirname "$out_file")"
            gzip -c -9 "$file" > "$out_file"

            echo "✔ $rel_path"
        fi
    done
done
