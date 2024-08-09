#!/bin/sh
#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#
set -e

if ! command -v astyle >/dev/null 2>&1
then
    echo "astyle could not be found, it must be available to run the script."
    exit 1
fi

# Format (in place) a list of files as C code.
astyle --options="${0%/*}/astylerc" "$@"


for f
do
    python3 -m guardonce.guard2once -s "$f"
done
