## Process this file with automake to produce Makefile.in

noinst_PROGRAMS += src/testrig/testrig

src_testrig_testrig_SOURCES = \
    src/testrig/testrig.c

src_testrig_testrig_LDADD = $(libsie_lib)
if WANT_LIBAPR
src_testrig_testrig_LDFLAGS = $(APR_LINK_LD) $(APR_LIBS)
endif
