#!/bin/sh
test -n "$srcdir" || srcdir=$1
test -n "$srcdir" || srcdir=.

cd $srcdir

VERSION=$(git describe --abbrev=0)
NAME="epiphany-$VERSION"

echo "Creating git tree archive…"
git archive --prefix="${NAME}/" --format=tar HEAD > epiphany.tar

rm -f "${NAME}.tar"
tar -Af "${NAME}.tar" epiphany.tar
rm -f epiphany.tar

echo "Compressing archive…"
xz -f "${NAME}.tar"
