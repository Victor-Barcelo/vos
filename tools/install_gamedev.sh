#!/usr/bin/env bash
set -euo pipefail

DISK_IMG="${1:-vos-disk.img}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VGAMEDEV_DIR="$ROOT_DIR/gameResources"
VGAMEDEV_EXAMPLES="$ROOT_DIR/gameResources/gameExamples"
VGAMEDEV_DOCS="$ROOT_DIR/gameResources/doc"

if [[ ! -f "$DISK_IMG" ]]; then
  echo "error: disk image not found: $DISK_IMG" >&2
  exit 1
fi

if [[ ! -d "$VGAMEDEV_DIR" ]]; then
  echo "error: gameResources directory not found: $VGAMEDEV_DIR" >&2
  exit 1
fi

MOUNT_POINT="/tmp/vos_disk_mount_$$"
LOOP_DEV=""

cleanup() {
  if [[ -n "$MOUNT_POINT" ]] && mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    sudo umount "$MOUNT_POINT" 2>/dev/null || true
  fi
  if [[ -n "$LOOP_DEV" ]]; then
    sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
  fi
  rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

echo "Installing game development libraries to $DISK_IMG..."

# Create mount point
mkdir -p "$MOUNT_POINT"

# Set up loop device for the partition (offset = 2048 sectors * 512 bytes)
LOOP_DEV=$(sudo losetup -f --show -o $((2048*512)) --sizelimit $((1046528*512)) "$DISK_IMG")
echo "  loop device: $LOOP_DEV"

# Mount the Minix partition
sudo mount -t minix "$LOOP_DEV" "$MOUNT_POINT"
echo "  mounted at: $MOUNT_POINT"

# Create directories
sudo mkdir -p "$MOUNT_POINT/usr/include/gamedev"
sudo mkdir -p "$MOUNT_POINT/usr/share/gamedev/doc"
sudo mkdir -p "$MOUNT_POINT/home/victor/examples/gamedev"

# Copy header files
echo "  copying headers..."
for f in "$VGAMEDEV_DIR"/*.h; do
  if [[ -f "$f" ]]; then
    sudo cp "$f" "$MOUNT_POINT/usr/include/gamedev/"
  fi
done

# Copy source files
echo "  copying source files..."
for f in "$VGAMEDEV_DIR"/*.c; do
  if [[ -f "$f" ]]; then
    sudo cp "$f" "$MOUNT_POINT/usr/include/gamedev/"
  fi
done

# Copy documentation
if [[ -d "$VGAMEDEV_DOCS" ]]; then
  echo "  copying documentation..."
  for f in "$VGAMEDEV_DOCS"/*.md; do
    if [[ -f "$f" ]]; then
      sudo cp "$f" "$MOUNT_POINT/usr/share/gamedev/doc/"
    fi
  done
fi

# Copy examples
if [[ -d "$VGAMEDEV_EXAMPLES" ]]; then
  echo "  copying examples..."
  for f in "$VGAMEDEV_EXAMPLES"/*.c; do
    if [[ -f "$f" ]]; then
      sudo cp "$f" "$MOUNT_POINT/home/victor/examples/gamedev/"
    fi
  done
fi

# Show what was installed
echo ""
echo "Installed:"
echo "  Headers:  $(ls "$MOUNT_POINT/usr/include/gamedev/"*.h 2>/dev/null | wc -l) files"
echo "  Sources:  $(ls "$MOUNT_POINT/usr/include/gamedev/"*.c 2>/dev/null | wc -l) files"
echo "  Docs:     $(ls "$MOUNT_POINT/usr/share/gamedev/doc/"*.md 2>/dev/null | wc -l) files"
echo "  Examples: $(ls "$MOUNT_POINT/home/victor/examples/gamedev/"*.c 2>/dev/null | wc -l) files"

# Sync and unmount
sudo sync
echo ""
echo "Done."
