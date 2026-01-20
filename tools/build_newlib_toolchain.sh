#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$ROOT_DIR/toolchain}"
SRC_DIR="$TOOLCHAIN_DIR/src"
BUILD_DIR="$TOOLCHAIN_DIR/build"
PREFIX="$TOOLCHAIN_DIR/opt/cross"
TARGET="${TARGET:-i686-elf}"

BINUTILS_VER="${BINUTILS_VER:-2.42}"
GCC_VER="${GCC_VER:-13.2.0}"

JOBS="${JOBS:-$(nproc)}"

say() { printf '%s\n' "$*"; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { say "missing required command: $1"; exit 1; }
}

fetch() {
  local url="$1"
  local out="$2"
  if [[ -f "$out" ]]; then
    return 0
  fi
  if command -v curl >/dev/null 2>&1; then
    curl -L --retry 3 --retry-delay 2 -o "$out" "$url"
  else
    wget -O "$out" "$url"
  fi
}

mkdir -p "$SRC_DIR" "$BUILD_DIR" "$PREFIX"

need_cmd make
need_cmd gcc
need_cmd g++
need_cmd ld
need_cmd ar
need_cmd as
need_cmd git

say "[toolchain] prefix: $PREFIX"
say "[toolchain] target: $TARGET"
say "[toolchain] jobs:   $JOBS"

export PATH="$PREFIX/bin:$PATH"

BINUTILS_TAR="$SRC_DIR/binutils-$BINUTILS_VER.tar.xz"
GCC_TAR="$SRC_DIR/gcc-$GCC_VER.tar.xz"

say "[toolchain] fetching binutils-$BINUTILS_VER"
fetch "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz" "$BINUTILS_TAR"
say "[toolchain] fetching gcc-$GCC_VER"
fetch "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz" "$GCC_TAR"

if [[ ! -d "$SRC_DIR/binutils-$BINUTILS_VER" ]]; then
  say "[toolchain] extracting binutils"
  tar -C "$SRC_DIR" -xf "$BINUTILS_TAR"
fi

if [[ ! -d "$SRC_DIR/gcc-$GCC_VER" ]]; then
  say "[toolchain] extracting gcc"
  tar -C "$SRC_DIR" -xf "$GCC_TAR"
fi

if [[ ! -d "$SRC_DIR/newlib-cygwin" ]]; then
  say "[toolchain] cloning newlib-cygwin"
  git clone --depth 1 https://sourceware.org/git/newlib-cygwin.git "$SRC_DIR/newlib-cygwin"
fi

if [[ ! -x "$PREFIX/bin/$TARGET-ld" ]]; then
  say "[toolchain] building binutils"
  rm -rf "$BUILD_DIR/binutils"
  mkdir -p "$BUILD_DIR/binutils"
  pushd "$BUILD_DIR/binutils" >/dev/null
  "$SRC_DIR/binutils-$BINUTILS_VER/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
  make -j"$JOBS"
  make install
  popd >/dev/null
fi

if [[ ! -x "$PREFIX/bin/$TARGET-gcc" ]]; then
  say "[toolchain] building gcc (stage1, C only)"
  rm -rf "$BUILD_DIR/gcc-stage1"
  mkdir -p "$BUILD_DIR/gcc-stage1"
  pushd "$BUILD_DIR/gcc-stage1" >/dev/null
  "$SRC_DIR/gcc-$GCC_VER/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers \
    --disable-multilib
  make -j"$JOBS" all-gcc
  make -j"$JOBS" all-target-libgcc
  make install-gcc
  make install-target-libgcc
  popd >/dev/null
fi

if [[ ! -f "$PREFIX/$TARGET/include/stdio.h" ]]; then
  say "[toolchain] building newlib (from newlib-cygwin.git)"
  rm -rf "$BUILD_DIR/newlib"
  mkdir -p "$BUILD_DIR/newlib"
  pushd "$BUILD_DIR/newlib" >/dev/null
  "$SRC_DIR/newlib-cygwin/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls
  make -j"$JOBS"
  make install
  popd >/dev/null
fi

say "[toolchain] building gcc (stage2, with newlib)"
rm -rf "$BUILD_DIR/gcc-final"
mkdir -p "$BUILD_DIR/gcc-final"
pushd "$BUILD_DIR/gcc-final" >/dev/null
"$SRC_DIR/gcc-$GCC_VER/configure" \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --disable-nls \
  --enable-languages=c \
  --disable-multilib \
  --with-newlib
make -j"$JOBS" all-gcc
make -j"$JOBS" all-target-libgcc
make install-gcc
make install-target-libgcc
popd >/dev/null

say "[toolchain] done"
say ""
say "Add this to your shell before running make:"
say "  export PATH=\"$PREFIX/bin:\$PATH\""
