NDK=$2
NDKABI=$3
NDKVER=$NDK/toolchains/$4
NDKP=$NDKVER/prebuilt/linux-x86/bin/arm-linux-androideabi-
NDKF="--sysroot $NDK/platforms/android-$NDKABI/arch-arm"
CURRENTDIR=`dirname $0`
TARGETDIR=$CURRENTDIR/../../../obj/local/armeabi
if [ -e $CURRENTDIR/libluajit.a ]; then
	>&2 printf "libluajit.a built already. use cache.\n";
	>&2 printf "for rebuild, remove $CURRENTDIR/libluajit.a\n"
	mkdir -p $TARGETDIR
	cp $CURRENTDIR/libluajit.a $TARGETDIR/
	exit;
fi
pushd $1
make clean
make HOST_CC="gcc -m32" CROSS=$NDKP TARGET_SYS=Linux TARGET_FLAGS="$NDKF"
popd
mkdir -p $TARGETDIR
cp $1/src/libluajit.a $CURRENTDIR/
cp $1/src/libluajit.a $TARGETDIR/

