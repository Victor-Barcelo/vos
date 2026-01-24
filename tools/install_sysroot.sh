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
VCRTI="$ROOT_DIR/build/user/crti.o"
VCRTN="$ROOT_DIR/build/user/crtn.o"
VLINKER="$ROOT_DIR/user/linker.ld"

VTCC="$ROOT_DIR/build/user/tcc.elf"
VTCC1="$ROOT_DIR/build/user/libtcc1.a"
VTCC_INC="$ROOT_DIR/third_party/tcc/include"
VSYSCALL_H="$ROOT_DIR/user/syscall.h"
VTERMIOS_H="$ROOT_DIR/user/sys/termios.h"
VIOCTL_H="$ROOT_DIR/user/sys/ioctl.h"

VOLIVE="$ROOT_DIR/build/user/libolive.a"
VOLIVE_H="$ROOT_DIR/third_party/olive/olive.h"
VOLIVE_C="$ROOT_DIR/third_party/olive/olive.c"

VSMALL3D_H="$ROOT_DIR/third_party/small3dlib/small3d.h"
VSMALL3DLIB_H="$ROOT_DIR/third_party/small3dlib/small3dlib.h"

VOLIVEDEMO_SRC="$ROOT_DIR/user/olivedemo.c"
VS3LCUBE_SRC="$ROOT_DIR/user/s3lcube.c"

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
if [[ ! -f "$VCRTI" ]]; then
  echo "error: VOS crti not built: $VCRTI" >&2
  echo "hint: run 'make sysroot' (or 'make $VCRTI') first." >&2
  exit 1
fi
if [[ ! -f "$VCRTN" ]]; then
  echo "error: VOS crtn not built: $VCRTN" >&2
  echo "hint: run 'make sysroot' (or 'make $VCRTN') first." >&2
  exit 1
fi
if [[ ! -f "$VTCC" ]]; then
  echo "error: tcc not built: $VTCC" >&2
  echo "hint: run 'make' (or 'make $VTCC') first." >&2
  exit 1
fi
if [[ ! -f "$VTCC1" ]]; then
  echo "error: libtcc1 not built: $VTCC1" >&2
  echo "hint: run 'make sysroot' (or 'make $VTCC1') first." >&2
  exit 1
fi
if [[ ! -d "$VTCC_INC" ]]; then
  echo "error: tcc include dir not found: $VTCC_INC" >&2
  exit 1
fi
if [[ ! -f "$VSYSCALL_H" ]]; then
  echo "error: VOS syscall header not found: $VSYSCALL_H" >&2
  exit 1
fi
if [[ ! -f "$VTERMIOS_H" ]]; then
  echo "error: VOS termios header not found: $VTERMIOS_H" >&2
  exit 1
fi
if [[ ! -f "$VIOCTL_H" ]]; then
  echo "error: VOS ioctl header not found: $VIOCTL_H" >&2
  exit 1
fi
if [[ ! -f "$VOLIVE" ]]; then
  echo "error: olive not built: $VOLIVE" >&2
  echo "hint: run 'make sysroot' (or 'make $VOLIVE') first." >&2
  exit 1
fi
if [[ ! -f "$VOLIVE_H" ]]; then
  echo "error: olive header not found: $VOLIVE_H" >&2
  exit 1
fi
if [[ ! -f "$VOLIVE_C" ]]; then
  echo "error: olive source not found: $VOLIVE_C" >&2
  exit 1
fi
if [[ ! -f "$VSMALL3D_H" ]]; then
  echo "error: small3d wrapper header not found: $VSMALL3D_H" >&2
  exit 1
fi
if [[ ! -f "$VSMALL3DLIB_H" ]]; then
  echo "error: small3dlib header not found: $VSMALL3DLIB_H" >&2
  exit 1
fi
if [[ ! -f "$VOLIVEDEMO_SRC" ]]; then
  echo "error: olive demo not found: $VOLIVEDEMO_SRC" >&2
  exit 1
fi
if [[ ! -f "$VS3LCUBE_SRC" ]]; then
  echo "error: small3dlib cube demo not found: $VS3LCUBE_SRC" >&2
  exit 1
fi

mkdir_usr() {
  local dir="$1"
  # Avoid mtools' interactive clash prompts in non-interactive installs.
  mmd -D s -i "$DISK_IMG" "$dir" >/dev/null 2>&1 || true
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

rm_img_path() {
  local p="$1"
  mdel -i "$DISK_IMG" "$p" >/dev/null 2>&1 || true
  mrd -i "$DISK_IMG" "$p" >/dev/null 2>&1 || true
}

img_has_file() {
  local p="$1"
  mtype -i "$DISK_IMG" "$p" >/dev/null 2>&1
}

mkdir_usr ::/usr
mkdir_usr ::/usr/include
mkdir_usr ::/usr/include/sys
mkdir_usr ::/usr/lib
mkdir_usr ::/usr/bin
mkdir_usr ::/usr/share
mkdir_usr ::/usr/share/ne
mkdir_usr ::/usr/share/ne/macros
mkdir_usr ::/usr/share/ne/syntax

# Linux-like top-level layout (stored on disk; exposed via VFS aliases).
mkdir_usr ::/etc
mkdir_usr ::/home
mkdir_usr ::/home/root
mkdir_usr ::/home/victor
mkdir_usr ::/home/victor/examples
mkdir_usr ::/var
mkdir_usr ::/var/log

mkdir_usr ::/usr/lib/gcc
mkdir_usr ::/usr/lib/gcc/i686-elf
mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER"
mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER/include"
mkdir_usr ::/usr/lib/tcc
mkdir_usr ::/usr/lib/tcc/include

echo "Installing sysroot to $DISK_IMG..."
echo "  gcc version: $GCC_VER"

if img_has_file ::/usr/include/stdio.h && img_has_file ::/usr/lib/libc.a && img_has_file ::/usr/lib/libgcc.a; then
  echo "  base sysroot detected; skipping header tree copy"
else
  copy_tree "$NEWLIB_INC" ::/usr/include
  copy_tree "$GCC_INC" "::/usr/lib/gcc/i686-elf/$GCC_VER/include"
  if [[ -d "$GCC_INC_FIXED" ]]; then
    mkdir_usr "::/usr/lib/gcc/i686-elf/$GCC_VER/include-fixed"
    copy_tree "$GCC_INC_FIXED" "::/usr/lib/gcc/i686-elf/$GCC_VER/include-fixed"
  fi
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
copy_one "$VCRT0" ::/usr/lib/crt1.o
copy_one "$VCRTI" ::/usr/lib/crti.o
copy_one "$VCRTN" ::/usr/lib/crtn.o
copy_one "$VLINKER" ::/usr/lib/vos.ld

copy_one "$VTCC" ::/usr/bin/tcc
copy_one "$VTCC1" ::/usr/lib/tcc/libtcc1.a
copy_tree "$VTCC_INC" ::/usr/lib/tcc/include

copy_one "$VSYSCALL_H" ::/usr/include/syscall.h
copy_one "$VTERMIOS_H" ::/usr/include/sys/termios.h
copy_one "$VIOCTL_H" ::/usr/include/sys/ioctl.h

copy_one "$VOLIVE" ::/usr/lib/libolive.a
copy_one "$VOLIVE_H" ::/usr/include/olive.h
copy_one "$VOLIVE_C" ::/usr/include/olive.c
copy_one "$VSMALL3D_H" ::/usr/include/small3d.h
copy_one "$VSMALL3DLIB_H" ::/usr/include/small3dlib.h

copy_one "$VOLIVEDEMO_SRC" ::/home/victor/examples/olivedemo.c
copy_one "$VS3LCUBE_SRC" ::/home/victor/examples/s3lcube.c

# Remove legacy raylib artifacts from older disk images.
rm_img_path ::/usr/include/raylib.h
rm_img_path ::/usr/lib/libraylib.a
rm_img_path ::/home/victor/examples/raycube.c

# ne runtime data (syntax highlighting, macros, extensions)
NE_DIR="$ROOT_DIR/third_party/ne"
if [[ -d "$NE_DIR/syntax" ]]; then
  copy_tree "$NE_DIR/syntax" ::/usr/share/ne/syntax
fi
if [[ -d "$NE_DIR/macros" ]]; then
  copy_tree "$NE_DIR/macros" ::/usr/share/ne/macros
fi
if [[ -f "$NE_DIR/extensions" ]]; then
  rm_img_path ::/usr/share/ne/extensions
  copy_one "$NE_DIR/extensions" ::/usr/share/ne/extensions
fi
if [[ -f "$NE_DIR/COPYING" ]]; then
  copy_one "$NE_DIR/COPYING" ::/usr/share/ne/COPYING
fi
if [[ -f "$NE_DIR/.menus" ]]; then
  copy_one "$NE_DIR/.menus" ::/usr/share/ne/.menus
fi
if [[ -f "$NE_DIR/.keys" ]]; then
  copy_one "$NE_DIR/.keys" ::/usr/share/ne/.keys
fi

if ! img_has_file ::/etc/passwd; then
  echo "  seeding /etc/passwd"
  PASSWD_TMP="$(mktemp)"
  cat >"$PASSWD_TMP" <<'EOF'
root:root:0:0:/home/root:/bin/sh
victor::1000:1000:/home/victor:/bin/sh
EOF
  mcopy -i "$DISK_IMG" -o "$PASSWD_TMP" ::/etc/passwd >/dev/null
  rm -f "$PASSWD_TMP"
fi

echo "Done."
