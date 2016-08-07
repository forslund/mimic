#!/bin/sh
mkdir -p "m4" "config"
libtoolize --copy \
&& aclocal \
&& automake --add-missing \
&& autoconf
