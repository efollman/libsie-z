## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES += src/libsie/libsie.la

nodist_include_HEADERS = \
    include/sie.h

include_sie_HEADERS += \
    include/sie_apr.h \
    include/sie_block.h \
    include/sie_byteswap.h \
    include/sie_channel.h \
    include/sie_combiner.h \
    include/sie_compiler.h \
    include/sie_config.h \
    include/sie_context.h \
    include/sie_debug.h \
    include/sie_decoder.h \
    include/sie_dimension.h \
    include/sie_exception.h \
    include/sie_file.h \
    include/sie_group.h \
    include/sie_histogram.h \
    include/sie_id_map.h \
    include/sie_intake.h \
    include/sie_internal.h \
    include/sie_iterator.h \
    include/sie_literals.h \
    include/sie_object.h \
    include/sie_order.h \
    include/sie_output.h \
    include/sie_parser.h \
    include/sie_plot_crusher.h \
    include/sie_recover.h \
    include/sie_ref.h \
    include/sie_relation.h \
    include/sie_sie_vec.h \
    include/sie_sifter.h \
    include/sie_spigot.h \
    include/sie_stream.h \
    include/sie_stringtable.h \
    include/sie_strtod.h \
    include/sie_tag.h \
    include/sie_test.h \
    include/sie_transform.h \
    include/sie_uthash.h \
    include/sie_types.h \
    include/sie_utils.h \
    include/sie_vec.h \
    include/sie_writer.h \
    include/sie_xml.h \
    include/sie_xml_merge.h

src_libsie_libsie_la_SOURCES = \
    src/libsie/block.c \
    src/libsie/channel.c \
    src/libsie/combiner.c \
    src/libsie/compiler.c \
    src/libsie/context.c \
    src/libsie/debug.c \
    src/libsie/decoder.c \
    src/libsie/dimension.c \
    src/libsie/exception.c \
    src/libsie/file.c \
    src/libsie/group.c \
    src/libsie/histogram.c \
    src/libsie/id_map.c \
    src/libsie/intake.c \
    src/libsie/iterator.c \
    src/libsie/object.c \
    src/libsie/output.c \
    src/libsie/parser.c \
    src/libsie/plot_crusher.c \
    src/libsie/recover.c \
    src/libsie/ref.c \
    src/libsie/relation.c \
    src/libsie/sie_apr.c \
    src/libsie/sie_vec.c \
    src/libsie/sifter.c \
    src/libsie/spigot.c \
    src/libsie/stdcall.c \
    src/libsie/stream.c \
    src/libsie/stringtable.c \
    src/libsie/strtod.c \
    src/libsie/tag.c \
    src/libsie/test.c \
    src/libsie/transform.c \
    src/libsie/utils.c \
    src/libsie/vec.c \
    src/libsie/writer.c \
    src/libsie/xml.c \
    src/libsie/xml_merge.c

src_libsie_libsie_la_LDFLAGS = -no-undefined \
    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
if WANT_LIBAPR
src_libsie_libsie_la_LDFLAGS += $(APR_LINK_LD) $(APR_LIBS)
endif
