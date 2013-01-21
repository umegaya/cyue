#!/bin/bash

pushd `dirname $0`/..
scons only_impl_h=true
popd
