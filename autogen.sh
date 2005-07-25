#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="sound-juicer"
REQUIRED_AUTOMAKE_VERSION=1.6
export REQUIRED_AUTOMAKE_VERSION

(test -f $srcdir/configure.in) || {
    echo "**Error**: Directory "\`$srcdir\'" does not look like the top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
	echo "You need to install gnome-common from the GNOME CVS"
	exit 1
}

. gnome-autogen.sh
