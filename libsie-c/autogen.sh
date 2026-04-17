#!/bin/sh -ex

# -ex is the same as:
# set trace
# set errexit

# aclocal && automake --add-missing --copy --foreign Makefile && autoconf

if test -f config.cache ; then
    rm -f config.cache
fi
if test -f acconfig.h ; then
    rm -f acconfig.h
fi

#touch NEWS README AUTHORS ChangeLog
#touch stamp-h

aclocal
libtoolize --force --copy
autoconf
#autoheader
automake --add-missing --copy --foreign

