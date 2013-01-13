#!/bin/sh

if [ $# -lt 1 ]; then
	echo "please specify luarocks source path";
	exit;
fi

echo "luarocks $1"

PWD=`pwd`
cd $1

./configure --with-lua=/usr/local --prefix=/usr/local --lua-suffix=jit --with-lua-include=/usr/local/include/luajit-2.0

make ; make install

cd $PWD



