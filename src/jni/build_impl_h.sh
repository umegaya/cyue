#!/bin/bash

pushd `dirname $0`/..
	if [ ! -e impl.h ]; then
		scons only_impl_h=true
	fi
popd

