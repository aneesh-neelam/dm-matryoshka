#!/usr/bin/env bash

SCRIPTPATH=$(dirname $0)
cat $(SCRIPTPATH)/dm_create.conf | dmsetup create mydev
