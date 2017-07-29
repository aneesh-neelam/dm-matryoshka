#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")
cd "$SCRIPTPATH"/..

sudo make unload
make sign
sudo make install
sudo make load
