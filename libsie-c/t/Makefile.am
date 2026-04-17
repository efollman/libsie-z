## Process this file with automake to produce Makefile.in

EXTRA_DIST += t/data

check_PROGRAMS += t/frob-file

noinst_HEADERS +=				\
    t/my-check.h				\
    t/simple-check.h				\
    t/test-suites.h				\
    t/test-utils.h

t_frob_file_SOURCES =				\
    t/frob-file.c				\
    t/test-utils.c

t_frob_file_LDADD = $(libsie_lib)

if WANT_LIBAPR
t_frob_file_LDADD += $(libapr_lib)
endif

if WANT_LIBCHECK
t_frob_file_LDADD += $(libcheck_lib)
else
t_frob_file_SOURCES += t/simple-check.c
endif

check_PROGRAMS += t/test-runner

t_test_runner_SOURCES =				\
    t/t_api.c					\
    t/t_decoder.c				\
    t/t_exception.c				\
    t/t_file.c					\
    t/t_functional.c				\
    t/t_histogram.c				\
    t/t_id_map.c				\
    t/t_object.c				\
    t/t_output.c				\
    t/t_progress.c				\
    t/t_regression.c				\
    t/t_relation.c				\
    t/t_sifter.c				\
    t/t_spigot.c				\
    t/t_stringtable.c				\
    t/t_xml.c					\
    t/t_xml_merge.c				\
    t/test-runner.c				\
    t/test-utils.c

t_test_runner_LDADD = -lm $(libsie_lib)

if WANT_LIBAPR
t_test_runner_LDFLAGS = $(APR_LINK_LD) $(APR_LIBS)
endif

if WANT_LIBCHECK
t_test_runner_LDADD += $(libcheck_lib)
else
t_test_runner_SOURCES += t/simple-check.c
endif

TESTS += t/test-runner

vgcheck:: t/test-runner
	libtool --mode=execute valgrind --tool=memcheck --leak-check=full \
	    $(VGARGS) ./t/test-runner -f $(ARGS)
