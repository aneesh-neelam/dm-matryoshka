#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")
cd "$SCRIPTPATH"/..

make
make sign
sudo make unload
sudo make install
sudo make load
