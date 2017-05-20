#!/bin/sh

VERSION=$(git describe --abbrev=0)
NAME="epiphany-$VERSION"

echo "Creating git tree archive…"
git archive --prefix="${NAME}/" --format=tar --output="${NAME}.tar" HEAD

echo "Compressing archive…"
xz --force --verbose "${NAME}.tar"
