#!/usr/bin/env bash

SCRIPTPATH=$(dirname "$0")
cat "$SCRIPTPATH"/dm_table.conf | sudo dmsetup create matryoshka1
