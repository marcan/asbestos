#!/bin/sh

# Copyright (C) 2007 Segher Boessenkool <segher@kernel.crashing.org>
# Copyright (C) 2009-2010 Hector Martin "marcan" <hector@marcansoft.com>
# Copyright (C) 2009 Andre Heider "dhewg" <dhewg@wiibrew.org>

# Released under the terms of the GNU GPL, version 2
SCRIPTDIR=`dirname $PWD/$0`

BINUTILS_VER=2.20.1
BINUTILS_DIR="binutils-$BINUTILS_VER"
BINUTILS_TARBALL="binutils-$BINUTILS_VER.tar.bz2"
BINUTILS_URI="http://ftp.gnu.org/gnu/binutils/$BINUTILS_TARBALL"

GMP_VER=5.0.1
GMP_DIR="gmp-$GMP_VER"
GMP_TARBALL="gmp-$GMP_VER.tar.bz2"
GMP_URI="http://ftp.gnu.org/gnu/gmp/$GMP_TARBALL"

MPFR_VER=3.0.0
MPFR_DIR="mpfr-$MPFR_VER"
MPFR_TARBALL="mpfr-$MPFR_VER.tar.bz2"
MPFR_URI="http://www.mpfr.org/mpfr-$MPFR_VER/$MPFR_TARBALL"

GCC_VER=4.4.4
GCC_DIR="gcc-$GCC_VER"
GCC_CORE_TARBALL="gcc-core-$GCC_VER.tar.bz2"
GCC_CORE_URI="http://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/$GCC_CORE_TARBALL"

BUILDTYPE=$1

PPU_TARGET=powerpc64-linux
SPU_TARGET=spu-elf

if [ -z $MAKEOPTS ]; then
	MAKEOPTS=-j3
fi

# End of configuration section.

case `uname -s` in
	*BSD*)
		MAKE=gmake
		;;
	*)
		MAKE=make
esac

export PATH=$PS3DEV/bin:$PATH

die() {
	echo $@
	exit 1
}

cleansrc() {
	[ -e $PS3DEV/$BINUTILS_DIR ] && rm -rf $PS3DEV/$BINUTILS_DIR
	[ -e $PS3DEV/$GCC_DIR ] && rm -rf $PS3DEV/$GCC_DIR
}

cleanbuild() {
	[ -e $PS3DEV/build_binutils ] && rm -rf $PS3DEV/build_binutils
	[ -e $PS3DEV/build_gcc ] && rm -rf $PS3DEV/build_gcc
}

download() {
	DL=1
	if [ -f "$PS3DEV/$2" ]; then
		echo "Testing $2..."
		tar tjf "$PS3DEV/$2" >/dev/null 2>&1 && DL=0
	fi

	if [ $DL -eq 1 ]; then
		echo "Downloading $2..."
		wget "$1" -c -O "$PS3DEV/$2" || die "Could not download $2"
	fi
}

extract() {
	echo "Extracting $1..."
	tar xjf "$PS3DEV/$1" -C "$2" || die "Error unpacking $1"
}

makedirs() {
	mkdir -p $PS3DEV/build_binutils || die "Error making binutils build directory $PS3DEV/build_binutils"
	mkdir -p $PS3DEV/build_gcc || die "Error making gcc build directory $PS3DEV/build_gcc"
}

buildbinutils() {
	TARGET=$1
	(
		cd $PS3DEV/build_binutils && \
		$PS3DEV/$BINUTILS_DIR/configure --target=$TARGET \
			--prefix=$PS3DEV --disable-werror --enable-64-bit-bfd && \
		nice $MAKE $MAKEOPTS && \
		$MAKE install
	) || die "Error building binutils for target $TARGET"
}

buildgcc() {
	TARGET=$1
	(
		cd $PS3DEV/build_gcc && \
		$PS3DEV/$GCC_DIR/configure --target=$TARGET --enable-targets=all \
			--prefix=$PS3DEV \
			--enable-languages=c --without-headers \
			--disable-nls --disable-threads --disable-shared \
			--disable-libmudflap --disable-libssp --disable-libgomp \
			--disable-decimal-float \
			--enable-checking=release $EXTRA_CONFIG_OPTS && \
		nice $MAKE $MAKEOPTS && \
		$MAKE install
	) || die "Error building binutils for target $TARGET"
}

buildspu() {
	cleanbuild
	makedirs
	echo "******* Building SPU binutils"
	buildbinutils $SPU_TARGET
	echo "******* Building SPU GCC"
	buildgcc $SPU_TARGET
	echo "******* SPU toolchain built and installed"
}

buildppu() {
	cleanbuild
	makedirs
	echo "******* Building PowerPC binutils"
	buildbinutils $PPU_TARGET
	echo "******* Building PowerPC GCC"
	EXTRA_CONFIG_OPTS="--with-cpu=cell" buildgcc $PPU_TARGET
	echo "******* PowerPC toolchain built and installed"
}

if [ -z "$PS3DEV" ]; then
	die "Please set PS3DEV in your environment."
fi

case $BUILDTYPE in
	ppu|spu|both|clean)	;;
	"")
		die "Please specify build type (ppu/spu/both/clean)"
		;;
	*)
		die "Unknown build type $BUILDTYPE"
		;;
esac

if [ "$BUILDTYPE" = "clean" ]; then
	cleanbuild
	cleansrc
	exit 0
fi

download "$BINUTILS_URI" "$BINUTILS_TARBALL"
download "$GMP_URI" "$GMP_TARBALL"
download "$MPFR_URI" "$MPFR_TARBALL"
download "$GCC_CORE_URI" "$GCC_CORE_TARBALL"

cleansrc

extract "$BINUTILS_TARBALL" "$PS3DEV"
extract "$GCC_CORE_TARBALL" "$PS3DEV"
extract "$GMP_TARBALL" "$PS3DEV/$GCC_DIR"
extract "$MPFR_TARBALL" "$PS3DEV/$GCC_DIR"

# in-tree gmp and mpfr
mv "$PS3DEV/$GCC_DIR/$GMP_DIR" "$PS3DEV/$GCC_DIR/gmp" || die "Error renaming $GMP_DIR -> gmp"
mv "$PS3DEV/$GCC_DIR/$MPFR_DIR" "$PS3DEV/$GCC_DIR/mpfr" || die "Error renaming $MPFR_DIR -> mpfr"

# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=42424
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=44455
patch -d $PS3DEV/$GCC_DIR -u -i $SCRIPTDIR/gcc.patch || die "Error applying gcc patch"

case $BUILDTYPE in
	spu)		buildspu ;;
	ppu)		buildppu ;;
	both)		buildppu ; buildspu; cleanbuild; cleansrc ;;
esac

