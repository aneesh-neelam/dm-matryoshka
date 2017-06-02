#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")
cat "$SCRIPTPATH"/dm_table.conf | dmsetup create mydev
