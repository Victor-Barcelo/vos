#!/usr/bin/env bash
set -euo pipefail

DISK_IMG="${1:-vos-disk.img}"
# Optional: if mount point is provided, skip mounting (already mounted by caller)
PROVIDED_MOUNT="${2:-}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CROSS_ROOT="${CROSS_ROOT:-$ROOT_DIR/toolchain/opt/cross}"

# Minix filesystem partition starts at sector 2048 (1 MiB offset)
PARTITION_OFFSET=$((2048 * 512))
MOUNT_POINT=""
LOOP_DEV=""
SHOULD_CLEANUP=true

# Use sudo only if not already root
if [[ $EUID -eq 0 ]]; then
  SUDO=""
else
  SUDO="sudo"
fi

cleanup() {
  if [[ "$SHOULD_CLEANUP" == "false" ]]; then
    return
  fi
  if [[ -n "$MOUNT_POINT" && -d "$MOUNT_POINT" ]]; then
    $SUDO umount "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
  fi
  if [[ -n "$LOOP_DEV" ]]; then
    $SUDO losetup -d "$LOOP_DEV" 2>/dev/null || true
  fi
}
trap cleanup EXIT

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
VUTSNAME_H="$ROOT_DIR/user/sys/utsname.h"
VSYSMACROS_H="$ROOT_DIR/user/sys/sysmacros.h"
VDIRENT_H="$ROOT_DIR/user/sys/dirent.h"
VMMAN_H="$ROOT_DIR/user/sys/mman.h"
# Note: VOS sys/stat.h uses #include_next which TCC doesn't support, so we install a merged version
VSTAT_TCC_H="$ROOT_DIR/user/sys/stat_tcc.h"

VOLIVE="$ROOT_DIR/build/user/libolive.a"
VOLIVE_H="$ROOT_DIR/third_party/olive/olive.h"
VOLIVE_C="$ROOT_DIR/third_party/olive/olive.c"

VSMALL3D_H="$ROOT_DIR/third_party/small3dlib/small3d.h"
VSMALL3DLIB_H="$ROOT_DIR/third_party/small3dlib/small3dlib.h"
VTERMBOX2_H="$ROOT_DIR/third_party/termbox2.h"

# Additional single-header libraries for TCC development
VSTB_IMAGE_H="$ROOT_DIR/third_party/stb/stb_image.h"
VJSMN_H="$ROOT_DIR/third_party/jsmn/jsmn.h"
VJSON_H="$ROOT_DIR/third_party/sheredom_json/json.h"
VLINENOISE_H="$ROOT_DIR/third_party/linenoise/linenoise.h"
VLINENOISE_C="$ROOT_DIR/third_party/linenoise/linenoise.c"
VPEANUT_GB_H="$ROOT_DIR/third_party/peanut-gb/peanut_gb.h"

# sbase utility headers (hash functions, arg parsing, utf8)
VSBASE_ARG_H="$ROOT_DIR/third_party/sbase/arg.h"
VSBASE_UTF_H="$ROOT_DIR/third_party/sbase/utf.h"
VSBASE_MD5_H="$ROOT_DIR/third_party/sbase/md5.h"
VSBASE_SHA1_H="$ROOT_DIR/third_party/sbase/sha1.h"
VSBASE_SHA256_H="$ROOT_DIR/third_party/sbase/sha256.h"
VSBASE_SHA512_H="$ROOT_DIR/third_party/sbase/sha512.h"

# uBASIC interpreter headers
VUBASIC_H="$ROOT_DIR/user/basic/ubasic.h"
VUBASIC_TOK_H="$ROOT_DIR/user/basic/tokenizer.h"

VOLIVEDEMO_SRC="$ROOT_DIR/user/olivedemo.c"
VS3LCUBE_SRC="$ROOT_DIR/user/s3lcube.c"
VS3LFLY_SRC="$ROOT_DIR/user/s3lfly.c"
VELIZA_SRC="$ROOT_DIR/user/eliza.c"
VJSON_DEMO_SRC="$ROOT_DIR/user/json.c"
VNEOFETCH_SRC="$ROOT_DIR/user/neofetch.c"
VSYSVIEW_SRC="$ROOT_DIR/user/sysview.c"

# Game development libraries directory
VGAMEDEV_DIR="$ROOT_DIR/gameResources"
VGAMEDEV_EXAMPLES="$ROOT_DIR/gameResources/gameExamples"
VGAMEDEV_DOCS="$ROOT_DIR/gameResources/doc"

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
if [[ ! -f "$VUTSNAME_H" ]]; then
  echo "error: VOS utsname header not found: $VUTSNAME_H" >&2
  exit 1
fi
if [[ ! -f "$VSYSMACROS_H" ]]; then
  echo "error: VOS sysmacros header not found: $VSYSMACROS_H" >&2
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
if [[ ! -f "$VTERMBOX2_H" ]]; then
  echo "error: termbox2 header not found: $VTERMBOX2_H" >&2
  exit 1
fi

setup_mount() {
  if [[ -n "$PROVIDED_MOUNT" ]]; then
    # Mount point provided by caller (e.g., from make disk-setup)
    MOUNT_POINT="$PROVIDED_MOUNT"
    SHOULD_CLEANUP=false
  else
    # Self-mount mode
    MOUNT_POINT=$(mktemp -d)
    LOOP_DEV=$($SUDO losetup --find --show --offset "$PARTITION_OFFSET" "$DISK_IMG")
    $SUDO mount -t minix "$LOOP_DEV" "$MOUNT_POINT"
  fi
}

# Convert mtools-style path (::/) to real mount path
to_real_path() {
  local p="$1"
  # Strip leading "::" if present
  p="${p#::}"
  echo "$MOUNT_POINT$p"
}

mkdir_usr() {
  local dir="$1"
  local real_path
  real_path="$(to_real_path "$dir")"
  $SUDO mkdir -p "$real_path"
}

copy_one() {
  local src="$1"
  local dst="$2"
  local real_dst
  real_dst="$(to_real_path "$dst")"
  $SUDO cp "$src" "$real_dst"
}

copy_tree() {
  local src="$1"
  local dst="$2"
  local real_dst
  real_dst="$(to_real_path "$dst")"
  # Copy directory contents recursively
  $SUDO cp -r "$src"/* "$real_dst/"
}

rm_img_path() {
  local p="$1"
  local real_path
  real_path="$(to_real_path "$p")"
  $SUDO rm -rf "$real_path" 2>/dev/null || true
}

img_has_file() {
  local p="$1"
  local real_path
  real_path="$(to_real_path "$p")"
  [[ -f "$real_path" ]]
}

# Mount the Minix partition
setup_mount
echo "Mounted $DISK_IMG partition at $MOUNT_POINT"

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

# Create a patched libc.a without signal-related objects (they conflict with libvosposix.a in TCC)
PATCHED_LIBC="$ROOT_DIR/build/user/libc_patched.a"
cp "$NEWLIB_LIB/libc.a" "$PATCHED_LIBC"
# Remove signal-related objects that are redefined in libvosposix.a
# Note: newlib archives use "libc_a-" prefix for object names
"$CROSS_ROOT/bin/i686-elf-ar" d "$PATCHED_LIBC" libc_a-signal.o libc_a-signalr.o 2>/dev/null || true

copy_one "$PATCHED_LIBC" ::/usr/lib/libc.a
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

# Install sbase tools (portable Unix userland utilities).
SBASE_BIN_DIR="$ROOT_DIR/build/user/sbase_bin"
if [[ -d "$SBASE_BIN_DIR" ]]; then
  for f in "$SBASE_BIN_DIR"/*.elf; do
    if [[ -f "$f" ]]; then
      b="$(basename "$f" .elf)"
      copy_one "$f" "::/usr/bin/$b"
    fi
  done
fi

copy_one "$VSYSCALL_H" ::/usr/include/syscall.h
copy_one "$VTERMIOS_H" ::/usr/include/sys/termios.h
copy_one "$VIOCTL_H" ::/usr/include/sys/ioctl.h
copy_one "$VUTSNAME_H" ::/usr/include/sys/utsname.h
copy_one "$VSYSMACROS_H" ::/usr/include/sys/sysmacros.h
copy_one "$VDIRENT_H" ::/usr/include/sys/dirent.h
copy_one "$VMMAN_H" ::/usr/include/sys/mman.h
copy_one "$VSTAT_TCC_H" ::/usr/include/sys/stat.h

copy_one "$VOLIVE" ::/usr/lib/libolive.a
copy_one "$VOLIVE_H" ::/usr/include/olive.h
copy_one "$VOLIVE_C" ::/usr/include/olive.c
copy_one "$VSMALL3D_H" ::/usr/include/small3d.h
copy_one "$VSMALL3DLIB_H" ::/usr/include/small3dlib.h
copy_one "$VTERMBOX2_H" ::/usr/include/termbox2.h

# Additional single-header libraries
copy_one "$VSTB_IMAGE_H" ::/usr/include/stb_image.h
copy_one "$VJSMN_H" ::/usr/include/jsmn.h
copy_one "$VJSON_H" ::/usr/include/json.h
copy_one "$VLINENOISE_H" ::/usr/include/linenoise.h
copy_one "$VLINENOISE_C" ::/usr/include/linenoise.c
copy_one "$VPEANUT_GB_H" ::/usr/include/peanut_gb.h

# sbase utility headers
copy_one "$VSBASE_ARG_H" ::/usr/include/arg.h
copy_one "$VSBASE_UTF_H" ::/usr/include/utf.h
copy_one "$VSBASE_MD5_H" ::/usr/include/md5.h
copy_one "$VSBASE_SHA1_H" ::/usr/include/sha1.h
copy_one "$VSBASE_SHA256_H" ::/usr/include/sha256.h
copy_one "$VSBASE_SHA512_H" ::/usr/include/sha512.h

# uBASIC headers
copy_one "$VUBASIC_H" ::/usr/include/ubasic.h
copy_one "$VUBASIC_TOK_H" ::/usr/include/tokenizer.h

copy_one "$VOLIVEDEMO_SRC" ::/home/victor/examples/olivedemo.c
copy_one "$VS3LCUBE_SRC" ::/home/victor/examples/s3lcube.c
copy_one "$VS3LFLY_SRC" ::/home/victor/examples/s3lfly.c
copy_one "$VELIZA_SRC" ::/home/victor/examples/eliza.c
copy_one "$VJSON_DEMO_SRC" ::/home/victor/examples/json.c
copy_one "$VNEOFETCH_SRC" ::/home/victor/examples/neofetch.c
copy_one "$VSYSVIEW_SRC" ::/home/victor/examples/sysview.c

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
  copy_one "$PASSWD_TMP" ::/etc/passwd
  rm -f "$PASSWD_TMP"
fi

# Game development libraries and examples
if [[ -d "$VGAMEDEV_DIR" ]]; then
  echo "  installing game development libraries..."

  # Create directories for gamedev resources
  mkdir_usr ::/usr/include/gamedev
  mkdir_usr ::/usr/share/gamedev
  mkdir_usr ::/usr/share/gamedev/doc
  mkdir_usr ::/home/victor/examples/gamedev

  # Copy header files (*.h) to /usr/include/gamedev/
  for f in "$VGAMEDEV_DIR"/*.h; do
    if [[ -f "$f" ]]; then
      copy_one "$f" "::/usr/include/gamedev/$(basename "$f")"
    fi
  done

  # Copy source files (*.c) to /usr/include/gamedev/
  for f in "$VGAMEDEV_DIR"/*.c; do
    if [[ -f "$f" ]]; then
      copy_one "$f" "::/usr/include/gamedev/$(basename "$f")"
    fi
  done

  # Copy documentation to /usr/share/gamedev/doc/
  if [[ -d "$VGAMEDEV_DOCS" ]]; then
    for f in "$VGAMEDEV_DOCS"/*.md; do
      if [[ -f "$f" ]]; then
        copy_one "$f" "::/usr/share/gamedev/doc/$(basename "$f")"
      fi
    done
  fi

  # Copy examples to /home/victor/examples/gamedev/
  if [[ -d "$VGAMEDEV_EXAMPLES" ]]; then
    for f in "$VGAMEDEV_EXAMPLES"/*.c; do
      if [[ -f "$f" ]]; then
        copy_one "$f" "::/home/victor/examples/gamedev/$(basename "$f")"
      fi
    done
  fi

  echo "  game development libraries installed"
fi

# Sync and cleanup
echo "Syncing filesystem..."
$SUDO sync
echo "Done."
