#!/bin/bash
# SSFN (Scalable Screen Font) Setup Script for VOS
# Run this script to download and set up SSFN with emoji support

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== SSFN Setup for VOS ==="
echo ""

# 1. Download the full ssfn.h header
echo "[1/6] Downloading ssfn.h..."
curl -sL "https://gitlab.com/bztsrc/scalable-font2/-/raw/master/ssfn.h" -o ssfn.h
echo "  Downloaded ssfn.h ($(wc -c < ssfn.h) bytes)"

# 2. Create directories
echo "[2/6] Creating directories..."
mkdir -p fonts emoji converted

# 3. Download Noto Color Emoji
echo "[3/6] Downloading Noto Color Emoji..."
curl -sL "https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoColorEmoji.ttf" -o emoji/NotoColorEmoji.ttf
echo "  Downloaded NotoColorEmoji.ttf ($(wc -c < emoji/NotoColorEmoji.ttf) bytes)"

# 4. Download additional high-quality fonts
echo "[4/6] Downloading additional fonts..."

# JetBrains Mono (if available)
curl -sL "https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip" -o fonts/jetbrains.zip 2>/dev/null && \
    unzip -q -o fonts/jetbrains.zip -d fonts/jetbrains 2>/dev/null && \
    echo "  Downloaded JetBrains Mono" || echo "  JetBrains Mono skipped"

# Fira Code
curl -sL "https://github.com/tonsky/FiraCode/releases/download/6.2/Fira_Code_v6.2.zip" -o fonts/firacode.zip 2>/dev/null && \
    unzip -q -o fonts/firacode.zip -d fonts/firacode 2>/dev/null && \
    echo "  Downloaded Fira Code" || echo "  Fira Code skipped"

# 5. Build sfnconv tool
echo "[5/6] Building sfnconv tool..."
if [ -d "sfnconv" ]; then
    echo "  sfnconv directory already exists, skipping clone"
else
    # Clone just the sfnconv directory (sparse checkout)
    git clone --filter=blob:none --sparse https://gitlab.com/bztsrc/scalable-font2.git sfnconv_repo 2>/dev/null || true
    if [ -d "sfnconv_repo" ]; then
        cd sfnconv_repo
        git sparse-checkout set sfnconv
        mv sfnconv ../
        cd ..
        rm -rf sfnconv_repo
    fi
fi

if [ -d "sfnconv" ] && [ -f "sfnconv/Makefile" ]; then
    cd sfnconv
    make -j$(nproc) 2>/dev/null && echo "  Built sfnconv" || echo "  sfnconv build failed (may need dependencies)"
    cd ..
else
    echo "  sfnconv not available - will use pre-converted fonts only"
fi

# 6. Convert fonts to SSFN format
echo "[6/6] Converting fonts..."

if [ -x "sfnconv/sfnconv" ]; then
    SFNCONV="./sfnconv/sfnconv"

    # Convert Noto Color Emoji at various sizes
    if [ -f "emoji/NotoColorEmoji.ttf" ]; then
        $SFNCONV -s 16 emoji/NotoColorEmoji.ttf converted/emoji-16.sfn 2>/dev/null && echo "  Converted emoji-16.sfn" || true
        $SFNCONV -s 24 emoji/NotoColorEmoji.ttf converted/emoji-24.sfn 2>/dev/null && echo "  Converted emoji-24.sfn" || true
        $SFNCONV -s 32 emoji/NotoColorEmoji.ttf converted/emoji-32.sfn 2>/dev/null && echo "  Converted emoji-32.sfn" || true
    fi

    # Convert system fonts if available
    for ttf in /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf \
               /usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf \
               /usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf; do
        if [ -f "$ttf" ]; then
            name=$(basename "$ttf" .ttf)
            $SFNCONV -s 16 "$ttf" "converted/${name}-16.sfn" 2>/dev/null && echo "  Converted ${name}-16.sfn" || true
            $SFNCONV -s 24 "$ttf" "converted/${name}-24.sfn" 2>/dev/null && echo "  Converted ${name}-24.sfn" || true
            $SFNCONV -s 32 "$ttf" "converted/${name}-32.sfn" 2>/dev/null && echo "  Converted ${name}-32.sfn" || true
        fi
    done

    # Convert downloaded fonts
    for ttf in fonts/jetbrains/fonts/ttf/JetBrainsMono-Regular.ttf \
               fonts/firacode/ttf/FiraCode-Regular.ttf; do
        if [ -f "$ttf" ]; then
            name=$(basename "$ttf" .ttf)
            $SFNCONV -s 16 "$ttf" "converted/${name}-16.sfn" 2>/dev/null && echo "  Converted ${name}-16.sfn" || true
            $SFNCONV -s 24 "$ttf" "converted/${name}-24.sfn" 2>/dev/null && echo "  Converted ${name}-24.sfn" || true
            $SFNCONV -s 32 "$ttf" "converted/${name}-32.sfn" 2>/dev/null && echo "  Converted ${name}-32.sfn" || true
        fi
    done
else
    echo "  sfnconv not available - skipping conversions"
    echo "  You can convert fonts manually later with:"
    echo "    ./sfnconv/sfnconv -s SIZE input.ttf output.sfn"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Files created:"
ls -la ssfn.h 2>/dev/null || true
ls -la converted/*.sfn 2>/dev/null || echo "  (no converted fonts yet)"
echo ""
echo "To use SSFN in VOS, add to your kernel:"
echo '  #define SSFN_IMPLEMENTATION'
echo '  #include "ssfn.h"'
echo ""
echo "See /home/victor/www/vos/third_party/ssfn/README.md for integration details."
