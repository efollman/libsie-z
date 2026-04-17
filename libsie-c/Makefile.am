## Process this file with automake to produce Makefile.in

libcheck_dir = $(top_srcdir)/build/@libcheck_name@
libcheck_lib = $(libcheck_dir)/src/libcheck.a

libsie_lib = src/libsie/libsie.la

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS =
check_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
include_HEADERS =
include_siedir = $(includedir)/sie
include_sie_HEADERS =
EXTRA_DIST = \
    autogen.sh \
    contrib \
    docs/standard/Makefile \
    docs/standard/common.tex \
    docs/standard/core-schema.tex \
    docs/standard/sie-format.tex \
    docs/standard/somat-schema.tex \
    docs/reference/Makefile \
    docs/reference/h2mdwn.pl \
    docs/example.sie \
    include/sie.h.in \
    tarballs/check-cvs-20051222-hacked.tar.gz
TESTS =

if WANT_DOCS
EXTRA_DIST += \
    docs/standard/core-schema.pdf \
    docs/standard/sie-format.pdf \
    docs/standard/somat-schema.pdf \
    docs/reference/libsie-reference.html
noinst_SCRIPTS = docs
endif

include src/libsie/Makefile.am
include src/testrig/Makefile.am
include src/libsie-demo/Makefile.am
#include src/misc/Makefile.am
include t/Makefile.am

############################################################

#pkgconfigdir=$(libdir)/pkgconfig
#pkgconfig_DATA=libsie-0.01.pc 

.PHONY: docs
docs:
	cd docs/standard && $(MAKE)
	cd docs/reference && $(MAKE)

$(libcheck_lib):
	echo "Building @libcheck_name@"
	cd $(libcheck_dir) && $(MAKE)
	echo "Done building @libcheck_name@"

autocleanfiles = Makefile.in configure config.sub config.guess \
    INSTALL depcomp aclocal.m4 install-sh ltmain.sh missing config.h.in* \
    compile mkinstalldirs build libsie-*.tar.gz

autoclean: maintainer-clean
	rm -rf $(autocleanfiles)

win32-dist:: dist
	sh win32-dist.sh $(distdir)

docs-dist:: include/sie.h
	cd docs/standard && make
	cd docs/reference && make
	rm -rf $(distdir)-docs
	mkdir $(distdir)-docs
	cp docs/reference/libsie-reference.html $(distdir)-docs/
	cp src/libsie-demo/libsie-demo.c $(distdir)-docs/
	cp docs/standard/sie-format.pdf $(distdir)-docs/sie-format.pdf
	tar zcvf $(distdir)-docs.tar.gz $(distdir)-docs
	perl -pi -e 'chomp; $$_.="\r\n";' $(distdir)-docs/libsie-reference.html $(distdir)-docs/libsie-demo.c
	rm -f $(distdir)-docs.zip
	zip -r $(distdir)-docs.zip $(distdir)-docs
	rm -rf $(distdir)-docs
