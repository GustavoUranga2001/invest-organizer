#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ ! -x ./invest ]; then
    make
fi
exec ./invest
