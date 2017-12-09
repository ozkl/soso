#!/bin/bash

mkdir build-newlib
cd build-newlib
../newlib-2.5.0/configure --prefix=/usr --target=i686-soso
make all
make DESTDIR=/home/alp/git/soso install
