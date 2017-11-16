#!/bin/sh

# This script is a not-very-elaborate workaround for the fact that Meson's
# run_command() executes commands in an unspecified directory. All we need to
# do here is ensure we run git describe in the source directory.
#
# It would be nice if we didn't need to use a script for something so simple.

cd $MESON_SOURCE_ROOT
git describe
