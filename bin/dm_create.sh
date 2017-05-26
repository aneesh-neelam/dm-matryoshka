#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

cat $(SCRIPTPATH)/dm_create.conf | dmsetup create mydev
