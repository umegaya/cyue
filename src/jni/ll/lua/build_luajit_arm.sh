NDK=$2
NDKABI=$3
NDKVER=$NDK/toolchains/$4
NDKP=$NDKVER/prebuilt/linux-x86/bin/arm-linux-androideabi-
NDKF="--sysroot $NDK/platforms/android-$NDKABI/arch-arm"
CURRENTDIR=`dirname $0`
if [ -e $CURRENTDIR/libluajit.a ]; then
	>&2 printf "libluajit.a built already. use cache.\n";
	>&2 printf "for rebuild, remove $CURRENTDIR/libluajit.a\n"
	exit;
fi
pushd $1
make clean
make HOST_CC="gcc -m32" CROSS=$NDKP TARGET_SYS=Linux TARGET_FLAGS="$NDKF"
popd
cp $1/src/libluajit.a $CURRENTDIR/

