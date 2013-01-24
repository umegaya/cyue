#!/bin/bash

tar -zxvf scons-2.1.0.tar.gz
pushd scons-2.1.0
sudo python setup.py install
popd
rm -rf scons-2.1.0
