NDK=$2
NDKABI=$3
NDKVER=$NDK/toolchains/$4
NDKP=$NDKVER/prebuilt/linux-x86/bin/arm-linux-androideabi-
NDKF="--sysroot $NDK/platforms/android-$NDKABI/arch-arm $5"
CURRENTDIR=`dirname $0`
pushd $1
#make clean
MAKE="make HOST_CC=\"gcc -m32\" CROSS=$NDKP TARGET_SYS=Linux TARGET_FLAGS=\"$NDKF\""
>&2 printf "build luajit: $MAKE\n"
if [ $# -gt 4 ]; then
	echo $MAKE | sh 2> /dev/null
else
	echo $MAKE | sh
fi
popd
#cp $1/src/libluajit.a $CURRENTDIR/

