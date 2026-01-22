#!/usr/bin/env bash
set -euo pipefail

DISK_IMG="${1:-vos-disk.img}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CROSS_ROOT="${CROSS_ROOT:-$ROOT_DIR/toolchain/opt/cross}"

NEWLIB_INC="$CROSS_ROOT/i686-elf/include"
NEWLIB_LIB="$CROSS_ROOT/i686-elf/lib"

GCC_BASE="$CROSS_ROOT/lib/gcc/i686-elf"
if [[ ! -d "$GCC_BASE" ]]; then
  echo "error: gcc base not found: $GCC_BASE" >&2
  exit 1
fi

GCC_VER="$(ls -1 "$GCC_BASE" | sort -V | tail -n1)"
if [[ -z "$GCC_VER" ]]; then
  echo "error: could not detect gcc version under: $GCC_BASE" >&2
  exit 1
fi

GCC_DIR="$GCC_BASE/$GCC_VER"
GCC_INC="$GCC_DIR/include"
GCC_INC_FIXED="$GCC_DIR/include-fixed"

VMLINUX="$ROOT_DIR/build/user/libvosposix.a"
VCRT0="$ROOT_DIR/build/user/crt0.o"
VLINKER="$ROOT_DIR/user/linker.ld"

if [[ ! -f "$DISK_IMG" ]]; then
  echo "error: disk image not found: $DISK_IMG" >&2
  echo "hint: run 'make vos-disk.img' to create one." >&2
  exit 1
fi
if [[ ! -d "$NEWLIB_INC" ]]; then
  echo "error: newlib headers not found: $NEWLIB_INC" >&2
  exit 1
fi
if [[ ! -f "$NEWLIB_LIB/libc.a" ]]; then
  echo "error: newlib libc not found: $NEWLIB_LIB/libc.a" >&2
  exit 1
fi
if [[ ! -f "$GCC_DIR/libgcc.a" ]]; then
  echo "error: libgcc not found: $GCC_DIR/libgcc.a" >&2
  exit 1
fi
if [[ ! -f "$VMLINUX" ]]; then
  echo "error: VOS runtime archive not built: $VMLINUX" >&2
  echo "hint: run 'make' first." >&2
  exit 1
fi
if [[ ! -f "$VCRT0" ]]; then
  echo "error: VOS crt0 not built: $VCRT0" >&2
  echo "hint: run 'make' first." >&2
  exit 1
fi

mkdir_usr() {
  mmd -i "$DISK_IMG" "$1" >/dev/null 2>&1 || true
}

copy_one() {
  local src="$1"
  local dst="$2"
  mcopy -i "$DISK_IMG" -o "$src" "$dst" >/dev/null
}

copy_tree() {
  local src="$1"
  local dst="$2"
  mcopy -i "$DISK_IMG" -o -s "$src"/* "$dst" >/dev/null
}

mkdir_usr ::/usr
mkdir_usr ::/usr/include
mkdir_usr ::/usr/lib

mkdir_usr ::/usr/lib/gcc
mkdir_usr ::/usr/lib/gcc/i686-elf
mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER"
mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER/include"

echo "Installing sysroot to $DISK_IMG..."
echo "  gcc version: $GCC_VER"

copy_tree "$NEWLIB_INC" ::/usr/include
copy_tree "$GCC_INC" "::/usr/lib/gcc/i686-elf/$GCC_VER/include"
if [[ -d "$GCC_INC_FIXED" ]]; then
  mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER/include-fixed"
  copy_tree "$GCC_INC_FIXED" "::/usr/lib/gcc/i686-elf/$GCC_VER/include-fixed"
fi

copy_one "$NEWLIB_LIB/libc.a" ::/usr/lib/libc.a
if [[ -f "$NEWLIB_LIB/libm.a" ]]; then
  copy_one "$NEWLIB_LIB/libm.a" ::/usr/lib/libm.a
fi

copy_one "$GCC_DIR/libgcc.a" ::/usr/lib/libgcc.a
copy_one "$GCC_DIR/libgcc.a" "::/usr/lib/gcc/i686-elf/$GCC_VER/libgcc.a"
if [[ -f "$GCC_DIR/crtbegin.o" ]]; then
  copy_one "$GCC_DIR/crtbegin.o" "::/usr/lib/gcc/i686-elf/$GCC_VER/crtbegin.o"
fi
if [[ -f "$GCC_DIR/crtend.o" ]]; then
  copy_one "$GCC_DIR/crtend.o" "::/usr/lib/gcc/i686-elf/$GCC_VER/crtend.o"
fi

copy_one "$VMLINUX" ::/usr/lib/libvosposix.a
copy_one "$VCRT0" ::/usr/lib/crt0.o
copy_one "$VLINKER" ::/usr/lib/vos.ld

echo "Done."
