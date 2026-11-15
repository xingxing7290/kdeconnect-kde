#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Run from a MSVC developer environment. No changes to the shared Craft prefix.
set -euo pipefail
phase=${1:?configure, make, or install}
root=${2:?dedicated FFmpeg source directory}
prefix=${3:?dedicated install prefix}
export PATH="/d/KDECraft/msys/usr/bin:$PATH"
if [ -n "${KDE_FFMPEG_TOOLSET_ROOT:-}" ]; then
    export PATH="$KDE_FFMPEG_TOOLSET_ROOT/bin/Hostx64/x64:$PATH"
    export INCLUDE="$(cygpath -w "$KDE_FFMPEG_TOOLSET_ROOT/include");${INCLUDE:-}"
    export LIB="$(cygpath -w "$KDE_FFMPEG_TOOLSET_ROOT/lib/x64");${LIB:-}"
fi
cd "$root"
export PKG_CONFIG_PATH='D:/KDECraft/lib/pkgconfig'
export PKG_CONFIG='D:/KDECraft/bin/pkgconf.exe'
case "$phase" in
configure)
    command -v cl.exe
    command -v link.exe
    ./configure --cc=cl.exe --cxx=cl.exe --toolchain=msvc --arch=x86_64 \
      --disable-debug --disable-doc --enable-gpl --enable-version3 \
      --enable-openssl --disable-xlib --disable-libxcb --enable-dxva2 \
      --enable-libmp3lame --enable-libass --enable-libvpx --enable-libx264 \
      --enable-libx265 --enable-libopus --enable-libvorbis --enable-libaom \
      --enable-libdav1d --enable-libwebp --enable-ffnvcodec --enable-nvdec \
      --enable-nvenc --enable-cuvid --enable-amf --enable-libsvtav1 \
      --enable-libvpl --disable-static --enable-shared --disable-ffplay \
      --extra-cflags='-FS -MD' --prefix="$prefix" --libdir="$prefix/lib"
    ;;
make) make -r -j8 ;;
install) make -r install ;;
*) echo "Unexpected phase: $phase" >&2; exit 2 ;;
esac
