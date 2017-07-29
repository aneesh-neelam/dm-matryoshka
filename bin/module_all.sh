#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")
cd "$SCRIPTPATH"/..

make clean
sudo make unload
make
make sign
sudo make install
sudo make load
