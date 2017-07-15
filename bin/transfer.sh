#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")

scp $SCRIPTPATH/../dm-matryoshka.ko testbed.local:~/Developer/dm-matryoshka
