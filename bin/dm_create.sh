#!/bin/sh

SCRIPTPATH=$(dirname $0)
cat $(SCRIPTPATH)/dm_create.conf | dmsetup create mydev
