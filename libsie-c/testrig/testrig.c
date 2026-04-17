/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define SIE_VEC_CONTEXT_OBJECT NULL

#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>

static void sigalrm_handler(int signum)
{
    printf("==%d== Timed out, exiting!\n", getpid());
    fprintf(stderr, "==%d== Timed out, exiting!\n", getpid());
    exit(1);
}

typedef int (test_main_fn)(int argc, char **argv, sie_Context *ctx);

static test_main_fn main_ls;
static test_main_fn main_lsbench;
static test_main_fn main_ls_tags;
static test_main_fn main_benchmark;
static test_main_fn main_membench;
static test_main_fn main_decoder;
static test_main_fn main_test;
static test_main_fn main_tests;
static test_main_fn main_channel;
static test_main_fn main_all_channels;
static test_main_fn main_image;
static test_main_fn main_images;
static test_main_fn main_plot_crusher;
static test_main_fn main_spigot;
static test_main_fn main_open;
static test_main_fn main_xml;
static test_main_fn main_sifter;
static test_main_fn main_sizes;
static test_main_fn main_disassemble;
static test_main_fn main_compile;
static test_main_fn main_excercise;
static test_main_fn main_recover;

static struct {
    char *name;
    test_main_fn *function;
} ad_hoc_tests[] = {
    { "ls", main_ls },
    { "lsbench", main_lsbench },
    { "ls_tags", main_ls_tags },
    { "benchmark", main_benchmark },
    { "membench", main_membench },
    { "disassemble", main_disassemble },
    { "test", main_test },
    { "tests", main_tests },
    { "channel", main_channel },
    { "all_channels", main_all_channels },
    { "image", main_image },
    { "images", main_images },
    { "plot_crusher", main_plot_crusher },
    { "spigot", main_spigot },
    { "decoder", main_decoder },
    { "open", main_open },
    { "xml", main_xml },
    { "sifter", main_sifter },
    { "sizes", main_sizes },
    { "compile", main_compile },
    { "excercise", main_excercise },
    { "recover", main_recover },
};

#define arg(index) _arg(argc, argv, index)
static char *_arg(int argc, char **argv, int index)
{
    if (argc <= index) {
        fprintf(stderr, "missing argument %d\n", index);
        exit(1);
    }
    return argv[index];
}

static int api_ex_count = 0;

void api_ex_cb(sie_Exception *ex, void *ignore)
{
    api_ex_count++;
    fprintf(stderr, "api exception:\n%s\n", sie_verbose_report(ex));
}

int check_done(void *ctx_obj)
{
    int ret = 1;
    int i = sie_context_done(sie_context(ctx_obj));
    if (i) {
        fprintf(stderr, "ERROR: %d objects unreleased!\n", i);
        ret = 0;
    }
    if (api_ex_count) {
        fprintf(stderr, "ERROR: %d api exceptions!\n", api_ex_count);
        ret = 0;
    }
    return ret;
}

char *pmsg = NULL;

int set_message(void *data, const char *msg)
{
    free(pmsg);
    pmsg = strdup(msg);
    fprintf(stderr, "PROGRESS: %s\n", pmsg);
    return 0;
}

int percent(void *data, sie_float64 percent_done)
{
    fprintf(stderr, "PROGRESS: %s %3.2f\n", pmsg, percent_done);
    return 0;
}

int main(int argc, char **argv)
{
    char *operation;
    int retval = -42;
    size_t i;
    int ch;
    sie_Context *ctx = sie_context_new();

    signal(SIGALRM, sigalrm_handler);

    sie_set_api_exception_callback(ctx, api_ex_cb, NULL);

    while ((ch = getopt(argc, argv, "d:g:p")) != -1) {
        switch (ch) {
        case 'd':
            ctx->debug_stream = stderr;
            ctx->debug_level = atoi(optarg);
            break;
        case 'g':
	    sie_ignore_trailing_garbage(ctx, atoi(optarg));
            break;
        case 'p':
            sie_set_progress_callbacks(ctx, NULL, set_message, percent);
            break;
        }
    }
    argc -= optind;
    argv += optind;

    operation = argc > 0 ? arg(0) : "";

    for (i = 0; i < sizeof(ad_hoc_tests) / sizeof(*ad_hoc_tests); i++)
        if (!strcmp(ad_hoc_tests[i].name, operation))
            retval = ad_hoc_tests[i].function(argc, argv, ctx);

    if (retval == -42) {
        fprintf(stderr, "No such operation '%s'\n", operation);
        fprintf(stderr, "Maybe you want:\n");
        for (i = 0; i < sizeof(ad_hoc_tests) / sizeof(*ad_hoc_tests); i++)
            fprintf(stderr, "  testrig %s\n", ad_hoc_tests[i].name);
    }
    
    if (!check_done(ctx))
        retval = 500;
    ctx = NULL;

    return retval;
}

static char decoder_sample[];
static char data_sample[];
static int data_sample_size;

#include <sys/time.h>

static sie_File *my_file_open(void *ctx_obj, char *name)
{
    sie_File *file = sie_file_open(ctx_obj, name);
    if (!file) {
        fprintf(stderr, "file_open failed!\n");
        check_done(ctx_obj);
        exit(1);
    }
    return file;
}

static int main_ls(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_Iterator *iter;
    sie_Channel *chan;

    iter = sie_get_channels(file);
    while ((chan = sie_iterator_next(iter))) {
        printf("%d: %s\n", sie_get_id(chan), sie_get_name(chan));
    }
    sie_release(iter);
    sie_release(file);

    return 0;
}

static int main_lsbench(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file;
    struct timeval start, end, diff = { 0, 0 };
    sie_Iterator *iter;
    sie_Channel *chan;
    sie_Tag *tag;

    gettimeofday(&start, NULL);
    file = my_file_open(ctx, arg(1));
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    printf("sie_file_open time:  %lu.%06lu seconds\n",
           diff.tv_sec, diff.tv_usec);

    iter = sie_get_tags(file);
    while ((tag = sie_iterator_next(iter))) {
	if (!strcmp(sie_tag_get_id(tag), "sie:xml_metadata")) {
	    gettimeofday(&start, NULL);
	    free(sie_tag_get_value(tag));
	    gettimeofday(&end, NULL);
	    timersub(&end, &start, &diff);
	    printf("'tag value 0' time:  %lu.%06lu seconds\n",
		    diff.tv_sec, diff.tv_usec);
	    break;
	}
    }
    sie_release(iter);

    gettimeofday(&start, NULL);
    iter = sie_get_channels(file);
    while ((chan = sie_iterator_next(iter))) {
        sie_get_id(chan);
        sie_get_name(chan);
    }
    sie_release(iter);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    printf("         'ls' time:  %lu.%06lu seconds\n",
           diff.tv_sec, diff.tv_usec);

    gettimeofday(&start, NULL);
    iter = sie_get_channels(file);
    while ((chan = sie_iterator_next(iter))) {
        sie_release(sie_attach_spigot(chan));
    }
    sie_release(iter);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    printf(" 'spigot all' time:  %lu.%06lu seconds\n",
           diff.tv_sec, diff.tv_usec);
    
    gettimeofday(&start, NULL);
    sie_release(file);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    printf("    'release' time:  %lu.%06lu seconds\n",
           diff.tv_sec, diff.tv_usec);


    return 0;
}

static int main_ls_tags(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_Iterator *iter, *tags;
    sie_Channel *chan;
    sie_Tag *tag;

    iter = sie_get_channels(file);
    while ((chan = sie_iterator_next(iter))) {
        printf("%d: %s\n", sie_get_id(chan), sie_get_name(chan));
        tags = sie_get_tags(chan);
        while ((tag = sie_iterator_next(tags))) {
            const char *id = sie_tag_get_id(tag);
            char *val = NULL;
            size_t size;
            sie_tag_get_value_b(tag, &val, &size);
             printf("  %s: %s\n", id, val);
            free(val);
        } 
        sie_release(tags);
    }
    sie_release(iter);
    sie_release(file);

    return 0;
}

static int main_benchmark(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    sie_Channel *channel = sie_get_channel(file, id);
    sie_Spigot *spigot = sie_attach_spigot(channel); /* expand XML */
    sie_Output *output;
    int group_id = channel->dimensions[0]->group;
    int decoder_id = channel->dimensions[0]->decoder_id;
    void *gh = sie_get_group_handle(file, group_id);
    int n_bytes = 0, times = 0;
    struct timeval start, end, diff, total = { 0, 0 };
    double total_f;

    sie_release(spigot);

    printf("group_id = %d\n", group_id);
    printf("decoder_id = %d\n", decoder_id);
    n_bytes = sie_get_group_num_bytes(file, gh);

    while (total.tv_sec < 10) {
        spigot = sie_attach_spigot(channel);
        assert(spigot);
        gettimeofday(&start, NULL);
        while ( (output = sie_spigot_get(spigot)) )
            ;
        gettimeofday(&end, NULL);
        timersub(&end, &start, &diff);
        timeradd(&diff, &total, &total);
        sie_release(spigot);
        times++;
    }

    sie_release(channel);
    sie_release(file);

    total_f = total.tv_sec + total.tv_usec / 1000000.0;
    
    printf("Channel contained %d bytes.\n", n_bytes);
    printf("Read channel %d times (%d bytes) in %f seconds,\n",
           times, n_bytes * times, total_f);
    printf("for a throughput of %f bytes/second.\n",
           (n_bytes * times) / total_f);
    printf("(%f seconds per read.)\n", total_f / times);

    return 0;
}

struct index_sum {
    size_t groups;
    size_t blocks;
    size_t bytes;
    size_t overhead;
};

static void sum_groups(sie_id id, sie_File_Group_Index *group_index,
                       void *sum_v)
{
    struct index_sum *sum = sum_v;

    ++sum->groups;
    sum->blocks += sie_vec_size(group_index->entries);
    sum->bytes +=
        sie_vec_capacity(group_index->entries) * sizeof(*group_index->entries);
    sum->overhead +=
        ((sie_vec_capacity(group_index->entries) *
          sizeof(*group_index->entries)) -
         (sie_vec_size(group_index->entries) *
          sizeof(*group_index->entries)));
}


static int main_membench(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    struct index_sum index = { 0, 0, 0, 0 };
    sie_XML *node;
    size_t xml_nodes = 0, xml_size = 0, attrs = 0, static_attr_overhead = 0;
    sie_String *string;
    size_t strings = 0, strings_size = 0;

    printf("Memory sizes for '%s':\n", arg(1));

    printf("Index:\n");
    sie_file_group_foreach(file, sum_groups, &index);
    printf("    %"APR_SIZE_T_FMT" groups\n", index.groups);
    printf("    %"APR_SIZE_T_FMT" blocks\n", index.blocks);
    printf("    %"APR_SIZE_T_FMT" bytes of overhead\n", index.overhead);
    printf("    %"APR_SIZE_T_FMT" bytes total\n", index.bytes);

    printf("XML metadata:\n");
    node = file->parent.xml->sie_node;
    while (node) {
        ++xml_nodes;
        xml_size += sizeof(*node);
        if (node->type == SIE_XML_ELEMENT) {
            attrs += node->value.element.num_attrs;
            if ((node->value.element.attrs !=
                 node->value.element.static_attrs)) {
                xml_size +=
                    sizeof(*node->value.element.attrs) *
                    node->value.element.num_attrs;
                static_attr_overhead +=
                    sizeof(node->value.element.static_attrs);
            } else {
                static_attr_overhead +=
                    sizeof(node->value.element.static_attrs) -
                    (sizeof(*node->value.element.attrs) *
                     node->value.element.num_attrs);
            }
        } else {
            static_attr_overhead +=
                sizeof(node->value.element.static_attrs);
        }
        node = sie_xml_walk_next(node, file->parent.xml->sie_node,
                                 SIE_XML_DESCEND);
    }
    printf("    %"APR_SIZE_T_FMT" XML nodes\n", xml_nodes);
    printf("    %"APR_SIZE_T_FMT" attributes\n", attrs);
    printf("    %"APR_SIZE_T_FMT" bytes static attribute overhead\n",
           static_attr_overhead);
    printf("    %"APR_SIZE_T_FMT" bytes total\n", xml_size);

    printf("Strings:\n");
    for (string = sie_string_table(ctx)->table_head; string != NULL;
         string = string->hh.next) {
        ++strings;
        strings_size += sizeof(*string);
        strings_size += string->size + 1;
    }
    printf("    %"APR_SIZE_T_FMT" strings\n", strings);
    printf("    %"APR_SIZE_T_FMT" bytes total\n", strings_size);

    sie_release(file);

    return 0;
}

static int main_decoder(int argc, char **argv, sie_Context *ctx)
{
    sie_XML *node = sie_xml_parse_string(ctx, decoder_sample);
    sie_Decoder *decoder = sie_decoder_new(ctx, node->child);
    sie_Decoder_Machine *machine = sie_decoder_machine_new(decoder);

    printf("%s", sie_decoder_disassemble(decoder));
    sie_decoder_machine_prep(machine, data_sample, data_sample_size);
    sie_decoder_machine_run(machine);

    sie_output_dump(machine->output, stdout);

    sie_release(machine);
    sie_release(decoder);

    return 0;
}

#include "sie_sie_vec.h"

static int main_spigot(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_Group *group = sie_group_new(file, atoi(arg(2)));
    sie_Spigot *spigot = sie_attach_spigot(group);
    sie_Output *output;

    while ( (output = sie_spigot_get(spigot)) )
        fwrite(output->v[0].raw[0].ptr, output->v[0].raw[0].size,
               1, stdout);
    
    sie_release(spigot);
    sie_release(group);
    sie_release(file);

    return 0;
}

static int main_test(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    sie_Test *test = sie_get_test(file, id);

    sie_test_dump(test, stdout);

    sie_release(test);
    sie_release(file);

    return 0;
}

static int main_tests(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_Iterator *iter = sie_get_tests(file);
    sie_Test *test;
    
    while ((test = sie_iterator_next(iter)))
        sie_test_dump(test, stdout);

    sie_release(iter);
    sie_release(file);

    return 0;
}

static void dump_channel(sie_id id, void *ignore, void *file)
{
    sie_Channel *channel = sie_get_channel(file, id);
    sie_Test *test = sie_get_containing_test(channel);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;

    if (!spigot) goto out;

    sie_channel_dump(channel, stdout);
    sie_test_dump(test, stdout);

    while ( (output = sie_spigot_get(spigot)) )
        sie_output_dump(output, stdout);
    
out:
    sie_release(spigot);
    sie_release(test);
    sie_release(channel);
}

static int main_channel(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));

    dump_channel(id, NULL, file);

    sie_release(file);

    return 0;
}

static int main_all_channels(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));

    /* KLUDGE not final interface */
    sie_id_map_foreach(file->parent.xml->channel_map, dump_channel, file);

    sie_release(file);

    return 0;
}

static int main_image(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    sie_Channel *channel = sie_get_channel(file, id);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    double time = atof(arg(3));
    size_t block, scan;
    sie_Output *output;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    if (!sie_binary_search(spigot, 0, time, &block, &scan))
        return 1;
    sie_spigot_seek(spigot, block);
    if ((output = sie_spigot_get(spigot))) {
        sie_Output_Struct *os = sie_output_get_struct(output);
        char name[64];
        FILE *out;
        sprintf(name, "image-%08lx%08lx.jpg", tv.tv_sec, tv.tv_usec);
        out = fopen(name, "w");
        if (!out)
            return 1;
        if (!fwrite(os->dim[1].raw[scan].ptr, os->dim[1].raw[scan].size,
                    1, out))
            return 1;
        fclose(out);
        printf("%s\n", name);
    }
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);

    return 0;
}

static int main_images(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    sie_Channel *channel = sie_get_channel(file, id);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;
    unsigned int count = 0;

    while ((output = sie_spigot_get(spigot))) {
        sie_Output_Struct *os = sie_output_get_struct(output);
        size_t row;
        for (row = 0; row < os->num_rows; ++row) {
            char name[64];
            FILE *out;
            sprintf(name, "image-%010u-%.4f.jpg",
                    count++, os->dim[0].float64[row]);
            out = fopen(name, "w");
            if (!out)
                return 1;
            if (!fwrite(os->dim[1].raw[row].ptr, os->dim[1].raw[row].size,
                        1, out))
                return 1;
            fclose(out);
            printf("%s\n", name);
        }
    }
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);

    return 0;
}

static int main_plot_crusher(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    int num_scans = atoi(arg(3));
    sie_Channel *channel = sie_get_channel(file, id);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Plot_Crusher *pc = sie_plot_crusher_new(spigot, num_scans);
    sie_Output *output;

    if (!spigot || !pc) return 1;

    sie_channel_dump(channel, stdout);

    output = sie_plot_crusher_finish(pc);
    sie_output_dump(output, stdout);
    
    sie_release(pc);
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);

    return 0;
}

static int main_disassemble(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    int id = atoi(arg(2));
    sie_Decoder *decoder =
        sie_id_map_get(file->parent.xml->compiled_decoder_map, id);

    assert(decoder);
    printf("%s\n", sie_decoder_disassemble(decoder));

    sie_release(file);

    return 0;
}

static int main_open(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_release(file);

    return 0;
}

static int main_xml(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    char *type = arg(2);
    sie_Group *group = sie_group_new(file, SIE_XML_GROUP);
    sie_Spigot *spigot = sie_attach_spigot(group);
    sie_vec(xml_string, char, 0);
    sie_Output *output;
    sie_XML_Definition *xml;
    int id = atoi(arg(3));
    char *xml_output = NULL;

    while ( (output = sie_spigot_get(spigot)) )
        sie_vec_append(xml_string,
                       output->v[0].raw[0].ptr,
                       output->v[0].raw[0].size);
    
    sie_release(spigot);
    sie_release(group);
    sie_release(file);

    sie_vec_append(xml_string, "</sie>", 7);

    xml = sie_xml_definition_new(ctx);
    sie_xml_definition_add_string(xml, xml_string, strlen(xml_string));

    sie_vec_clear(xml_output);
    sie_xml_output(sie_id_map_get(xml->channel_map, id), &xml_output, 0);
    printf("\nch %d raw:\n%s\n", id, xml_output);
    sie_vec_clear(xml_output);
    sie_xml_output(sie_xml_expand(xml,
                                  sie_string_get(ctx, type, strlen(type)),
                                  id),
                   &xml_output, 0);
    printf("\n%s %d expanded:\n%s\n", type, id, xml_output);

    return 0;
}

static size_t write_block(void *file, const char *data, size_t size)
{
    if (!fwrite(data, size, 1, file))
        return 0;
    return size;
}

static int main_sifter(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    FILE *out = fopen("out.sie", "wb");
    sie_Writer *writer = sie_writer_new(ctx, write_block, out);
    sie_Sifter *sifter = sie_sifter_new(writer);
    sie_Test *test;
    sie_Tag *tag;
    sie_Channel *channel;

    sie_writer_xml_header(writer);

    tag = sie_get_tag(file, "core:setup_name");
    sie_sifter_add(sifter, tag);
    sie_release(tag);

    test = sie_get_test(file, 1);
    tag = sie_get_tag(test, "core:start_time");
    sie_sifter_add(sifter, tag);
    sie_release(tag);
    sie_release(test);

    test = sie_get_test(file, 0);
    tag = sie_get_tag(test, "core:start_time");
    sie_sifter_add(sifter, tag);
    sie_release(tag);
    sie_release(test);

    test = sie_get_test(file, 1);
    tag = sie_get_tag(test, "core:start_time");
    sie_sifter_add(sifter, tag);
    sie_release(tag);
    sie_release(test);

    tag = sie_get_tag(file, "somat:tce_setup");
    sie_sifter_add(sifter, tag);
    sie_release(tag);

    channel = sie_get_channel(file, 2);
    sie_sifter_add(sifter, channel);
    sie_release(channel);

    channel = sie_get_channel(file, 4);
    sie_sifter_add(sifter, channel);
    sie_release(channel);

    channel = sie_get_channel(file, 3);
    sie_sifter_add(sifter, channel);
    sie_release(channel);

    channel = sie_get_channel(file, 5);
    sie_sifter_add(sifter, channel);
    sie_release(channel);

    channel = sie_get_channel(file, 2);
    sie_sifter_add(sifter, channel);
    sie_release(channel);

    printf("total size: %"APR_UINT64_T_FMT"\n",
           sie_sifter_total_size(sifter));

    sie_release(sifter);
    sie_release(writer);
    fclose(out);
    sie_release(file);

    return 0;
}

static int main_sizes(int argc, char **argv, sie_Context *ctx)
{
    sie_File *file = my_file_open(ctx, arg(1));
    sie_Iterator *channels = sie_get_channels(file);
    sie_Iterator *tests = sie_get_tests(file);
    sie_Channel *channel;
    sie_Test *test;

    while ((channel = sie_iterator_next(channels))) {
        sie_Writer *writer = sie_writer_new(ctx, NULL, NULL);
        sie_Sifter *sifter = sie_sifter_new(writer);

        sie_sifter_add(sifter, channel);

        printf("%s: %"APR_UINT64_T_FMT" bytes\n",
               sie_get_name(channel), sie_sifter_total_size(sifter));

        sie_release(sifter);
        sie_release(writer);
    }
    sie_release(channels);

    while ((test = sie_iterator_next(tests))) {
        sie_Writer *writer = sie_writer_new(ctx, NULL, NULL);
        sie_Sifter *sifter = sie_sifter_new(writer);

        channels = sie_get_channels(test);
        while ((channel = sie_iterator_next(channels)))
            sie_sifter_add(sifter, channel);
        sie_release(channels);

        printf("test %d: %"APR_UINT64_T_FMT" bytes\n",
               sie_get_id(test), sie_sifter_total_size(sifter));

        sie_release(sifter);
        sie_release(writer);
    }
    sie_release(tests);

    sie_release(file);

    return 0;
}

static int main_compile(int argc, char **argv, sie_Context *ctx)
{
    char decoder_string[50000];
    size_t got;
    sie_XML *node;
    sie_Decoder *decoder;
    got = fread(decoder_string, 1, sizeof(decoder_string) - 1, stdin);
    decoder_string[got] = 0;
    node = sie_xml_parse_string(ctx, decoder_string);
    decoder = sie_decoder_new(ctx, node);

    printf("%s\n", sie_decoder_disassemble(decoder));

    sie_release(decoder);
    sie_release(node);

    return 0;
}

static void touch_tag(sie_Tag *ref)
{
    char *value = NULL;
    size_t size = 0;

    sie_tag_get_id(ref);
    sie_tag_get_value_b(ref, &value, &size);
    free(value);
}

static sie_float64 f64;
static unsigned char uc;

static void touch_output(sie_Output *output)
{
    size_t v, scan, i;
    unsigned char *cptr;

    for (scan = 0; scan < output->num_scans; scan++) {
        for (v = 0; v < output->num_vs; v++) {
            switch (output->v[v].type) {
            case SIE_OUTPUT_FLOAT64:
                f64 = output->v[v].float64[scan];
                break;
            case SIE_OUTPUT_RAW:
                cptr = output->v[v].raw[scan].ptr;
                for (i = 0; i < output->v[v].raw[scan].size; i++)
                    uc = cptr[i];
                break;
            }
        }
    }
}

static void touch_channel_headers(sie_Channel *ref)
{
    sie_Iterator *iter, *iter2;
    sie_Tag *tag;
    sie_Dimension *dimension;
    sie_Test *test;

    iter = sie_get_tags(ref);
    while ( (tag = sie_iterator_next(iter)) )
        touch_tag(tag);
    sie_release(iter);

    test = sie_get_containing_test(ref);
    iter = sie_get_tags(test);
    while ( (tag = sie_iterator_next(iter)) )
        touch_tag(tag);
    sie_release(iter);
    sie_release(test);

    iter = sie_get_dimensions(ref);
    while ( (dimension = sie_iterator_next(iter)) ) {
        iter2 = sie_get_tags(dimension);
        while ( (tag = sie_iterator_next(iter2)) )
            touch_tag(tag);
        sie_release(iter2);
    }
    sie_release(iter);
}

static int main_excercise(int argc, char **argv, sie_Context *ctx)
{
    alarm(30);
    {
    sie_File *file = sie_file_open(ctx, arg(1));
    sie_Iterator *chans = sie_get_channels(file);
    sie_Channel *chan;
    
    while ( (chan = sie_iterator_next(chans)) ) {
        sie_Spigot *spigot = sie_attach_spigot(chan);
        sie_Output *output;
        sie_get_name(chan);
        touch_channel_headers(chan);
        while ( (output = sie_spigot_get(spigot)) )
            touch_output(output);
        sie_release(spigot);
    }

    sie_release(chans);
    sie_release(file);

    return 0;
    }
}

static int main_recover(int argc, char **argv, sie_Context *ctx)
{
    sie_file_recover(ctx, arg(1), atoi(arg(2)));
    return 0;
}

static char decoder_sample[] =
#if 0
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<decoder id=\"2\">"
    "  <read endian=\"little\" type=\"uint\" bits=\"64\" var=\"v0\"/>"
    "  <set var=\"v2\" value=\"1\"/>"
    "  <loop var=\"v0\">"
    "    <read type=\"raw\" octets=\"6\" var=\"v1\"/>"
    "    <set var=\"v2\" value=\"{$v2 + $v2/2 + 3 &amp; 0xfff}\"/>"
    "    <sample/>"
    "  </loop>"
    "</decoder>";
#elif 0
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<decoder id=\"2\">"
    " <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <read var=\"count\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <loop var=\"t0\" end=\"{$count >> 3}\">"
    "   <read var=\"vb\" bits=\"8\" type=\"uint\" endian=\"little\"/>"
    "   <loop var=\"shift\" start=\"7\" increment=\"-1\" end=\"-1\">"
    "    <set var=\"v1\" value=\"{($vb >> $shift) &amp; 1}\"/>"
    "    <sample/>"
    "    <set var=\"v0\" value=\"{$v0 + 1}\"/>"
    "   </loop>"
    " </loop>"
    " <read var=\"vb\" bits=\"8\" type=\"uint\" endian=\"little\"/>"
    " <loop var=\"shift\" start=\"7\" increment=\"-1\" end=\"{7 - ($count &amp; 7)}\">"
    "  <set var=\"v1\" value=\"{($vb >> $shift) &amp; 1}\"/>"
    "  <sample/>"
    "  <set var=\"v0\" value=\"{$v0 + 1}\"/>"
    " </loop>"
    "</decoder>";
#elif 0
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<decoder id=\"2\">"
    " <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <read var=\"count\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <loop var=\"t0\" end=\"{$count}\">"
    "  <loop start=\"{$t0 &amp; 7}\" var=\"t1\" end=\"1\">"
    "   <read var=\"vb\" bits=\"8\" type=\"uint\" endian=\"little\"/>"
    "  </loop>"
    "  <set var=\"v1\" value=\"{($vb >> (7 - ($t0 &amp; 7))) &amp; 1}\"/>"
    "  <sample/>"
    "  <set var=\"v0\" value=\"{$v0 + 1}\"/>"
    " </loop>"
    "</decoder>";
#elif 1
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<decoder id=\"2\">"
    " <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <read var=\"count\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <set var=\"v0\" value=\"{$v0 + 42}\"/>"
    " <sample/>"
    " <seek offset=\"0\" whence=\"0\"/>"
    " <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <read var=\"count\" bits=\"64\" type=\"uint\" endian=\"little\"/>"
    " <loop var=\"t0\" end=\"{$count}\">"
    "  <if condition=\"{($t0 &amp; 7) == 0}\">"
    "   <read var=\"vb\" bits=\"8\" type=\"uint\" endian=\"little\"/>"
    "  </if>"
    "  <set var=\"v1\" value=\"{($vb >> (7 - ($t0 &amp; 7))) &amp; 1}\"/>"
    "  <sample/>"
    "  <set var=\"v0\" value=\"{$v0 + 1}\"/>"
    " </loop>"
    "</decoder>";
#else
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<decoder id=\"2\">"
    "  <read endian=\"little\" type=\"uint\" bits=\"64\" var=\"v0\"/>"
    "  <set var=\"v2\" value=\"1\"/>"
    "  <loop var=\"v0\">"
    "    <read endian=\"little\" type=\"uint\" bits=\"16\" var=\"v1\"/>"
    "    <set var=\"tmp\" value=\"{$v2}\"/>"
    "    <set var=\"v2\" value=\"{$v2 + $old}\"/>"
    "    <set var=\"old\" value=\"{$tmp}\"/>"
    "    <sample/>"
    "  </loop>"
    "</decoder>";
#endif

static char data_sample[] = {
#if 0
    0, 0, 0, 0, 0, 0, 0, 0, 229, 212, 210, 212, 192, 212, 174, 212, 155, 212,
    137, 212, 119, 212, 101, 212, 83, 212, 64, 212, 46, 212, 28, 212, 11, 212,
    251, 211, 235, 211, 217, 211, 199, 211, 181, 211, 163, 211, 145, 211, 127,
    211, 109, 211, 91, 211, 73, 211, 55, 211, 38, 211, 20, 211, 2, 211, 240,
    210, 223, 210, 205, 210, 187, 210, 169, 210, 151, 210, 133, 210, 116, 210,
    99, 210, 81, 210, 62, 210, 44, 210, 27, 210, 8, 210, 247, 209, 230, 209,
    212, 209, 194, 209, 176, 209, 158, 209, 140, 209, 123, 209, 106, 209, 88,
    209, 70, 209, 53, 209, 35, 209, 18, 209, 1, 209, 240, 208, 223, 208, 205,
    208, 187, 208, 170, 208, 153, 208, 137, 208, 120, 208, 103, 208, 86, 208,
    69, 208, 53, 208, 36, 208, 20, 208, 3, 208, 245, 207, 230, 207, 212, 207,
    195, 207, 178, 207, 160, 207, 144, 207, 127, 207, 110, 207, 92, 207, 75,
    207, 59, 207, 41, 207, 24, 207, 7, 207, 246, 206, 230, 206, 213, 206, 197,
    206, 179, 206, 163, 206, 145, 206, 128, 206, 111, 206, 94, 206, 77, 206,
    61, 206, 43, 206, 26, 206, 9, 206, 249, 205, 233, 205, 216, 205, 199, 205,
    182, 205, 165, 205, 149, 205, 132, 205, 116, 205, 99, 205, 83, 205, 66,
    205, 49, 205, 32, 205, 15, 205, 255, 204, 239, 204, 222, 204, 206, 204,
    189, 204, 173, 204, 155, 204, 138, 204, 122, 204, 105, 204, 88, 204, 72,
    204, 55, 204, 38, 204, 21, 204, 6, 204, 248, 203, 233, 203, 216, 203, 199,
    203, 182, 203, 166, 203, 149, 203, 133, 203, 116, 203, 99, 203, 83, 203,
    66, 203, 49, 203, 32, 203, 16, 203, 0, 203, 240, 202, 223, 202, 206, 202,
    189, 202, 174, 202, 157, 202, 141, 202, 124, 202, 107, 202, 91, 202, 74,
    202, 58, 202, 41, 202, 25, 202, 9, 202, 249, 201, 233, 201, 216, 201, 199,
    201, 183, 201, 166, 201, 150, 201, 133, 201, 117, 201, 101, 201, 84, 201,
    67, 201, 50, 201, 34, 201, 18, 201, 1, 201, 241, 200, 225, 200, 208, 200,
    193, 200, 177, 200, 161, 200, 144, 200, 128, 200, 112, 200, 96, 200, 80,
    200, 63, 200, 46, 200, 31, 200, 14, 200, 0, 200, 242, 199, 226, 199, 211,
    199, 194, 199, 177, 199, 161, 199, 146, 199, 130, 199, 113, 199, 98, 199,
    81, 199, 66, 199, 51, 199, 34, 199, 19, 199, 3, 199, 243, 198, 228, 198,
    212, 198, 196, 198, 179, 198, 163, 198, 147, 198, 131, 198, 115, 198, 100,
    198, 84, 198, 69, 198, 53, 198, 36, 198, 21, 198, 5, 198, 246, 197, 231,
    197, 216, 197, 200, 197, 184, 197, 169, 197, 154, 197, 138, 197, 122, 197,
    106, 197, 91, 197, 76, 197, 60, 197, 45, 197, 29, 197, 15, 197, 255, 196,
    240, 196, 225, 196, 208, 196, 194, 196, 178, 196, 161, 196, 146, 196, 130,
    196, 115, 196, 99, 196, 84, 196, 67, 196, 51, 196, 35, 196, 20, 196, 6,
    196, 249, 195, 234, 195, 219, 195, 204, 195, 188, 195, 173, 195, 157, 195,
    141, 195, 126, 195, 110, 195, 95, 195, 79, 195, 64, 195, 48, 195, 33, 195,
    18, 195, 3, 195, 245, 194, 230, 194, 214, 194, 200, 194, 186, 194, 171,
    194, 156, 194, 142, 194, 127, 194, 112, 194, 97, 194, 82, 194, 68, 194, 53,
    194, 38, 194, 23, 194, 7, 194, 250, 193, 236, 193, 221, 193, 207, 193, 192,
    193, 177, 193, 162, 193, 148, 193, 133, 193, 119, 193, 105, 193, 90, 193,
    75, 193, 60, 193, 45, 193, 31, 193, 16, 193, 1, 193, 243, 192, 228, 192,
    214, 192, 199, 192, 184, 192, 170, 192, 155, 192, 140, 192, 126, 192, 111,
    192, 97, 192, 82, 192, 67, 192, 53, 192, 38, 192, 23, 192, 8, 192, 252,
    191, 239, 191, 224, 191, 210, 191, 195, 191, 180, 191, 166, 191, 151, 191,
    136, 191, 122, 191, 107, 191, 92, 191, 77, 191, 63, 191, 48, 191, 33, 191,
    19, 191, 4, 191, 245, 190, 231, 190, 216, 190, 201, 190, 186, 190, 172,
    190, 157, 190, 143, 190, 129, 190, 115, 190, 101, 190, 87, 190, 72, 190,
    59, 190, 44, 190, 30, 190, 16, 190, 3, 190, 245, 189, 231, 189, 216, 189,
    202, 189, 188, 189, 173, 189, 159, 189, 145, 189, 131, 189, 117, 189, 103,
    189, 88, 189, 74, 189, 60, 189, 46, 189, 33, 189, 18, 189, 5, 189, 248,
    188, 234, 188, 221, 188, 207, 188, 193, 188, 179, 188, 165, 188, 152, 188,
    138, 188, 124, 188, 111, 188, 97, 188, 84, 188, 70, 188, 57, 188, 44, 188,
    31, 188, 18, 188, 6, 188, 251, 187, 240, 187, 228, 187, 215, 187, 201, 187,
    188, 187, 175, 187, 162, 187, 149, 187, 136, 187, 123, 187, 110, 187, 97,
    187, 84, 187, 71, 187, 58, 187, 45, 187, 32, 187, 20, 187, 7, 187, 250,
    186, 237, 186, 225, 186, 211, 186, 198, 186, 185, 186, 172, 186, 159, 186,
    145, 186, 131, 186, 119, 186, 105, 186, 92, 186, 79, 186, 65, 186, 53, 186,
    39, 186, 25, 186, 12, 186, 255, 185, 242, 185, 230, 185, 217, 185, 204,
    185, 191, 185, 179, 185, 165, 185, 152, 185, 140, 185, 127, 185, 115, 185,
    102, 185, 90, 185, 77, 185, 65, 185, 53, 185, 41, 185, 29, 185, 16, 185, 4,
    185, 248, 184, 236, 184, 224, 184, 212, 184, 199, 184, 188, 184, 176, 184,
    163, 184, 152, 184, 139, 184, 128, 184, 116, 184, 104, 184, 91, 184, 79,
    184, 67, 184, 55, 184, 43, 184, 31, 184, 18, 184, 8, 184, 253, 183, 243,
    183, 231, 183, 219, 183, 207, 183, 194, 183, 182, 183, 169, 183, 157, 183,
    144, 183, 131, 183, 118, 183, 106, 183, 93, 183, 81, 183, 68, 183, 56, 183,
    43, 183, 29, 183, 17, 183, 3, 183, 246, 182, 233, 182, 220, 182, 206, 182,
    191, 182, 177, 182, 162, 182, 149, 182, 136, 182, 119, 182, 106, 182, 90,
    182, 76, 182, 61, 182, 45, 182, 31, 182, 14, 182, 255, 181, 238, 181, 222,
    181, 206, 181, 188, 181, 173, 181, 155, 181, 137, 181, 120, 181, 99, 181,
    83, 181, 62, 181, 43, 181, 25, 181, 4, 181, 244, 180, 222, 180, 203, 180,
    186, 180, 165, 180, 150, 180, 129, 180, 111, 180, 94, 180, 72, 180, 56,
    180, 36, 180, 17, 180, 253, 179, 231, 179, 215, 179, 193, 179, 173, 179,
    155, 179, 134, 179, 118, 179, 96, 179, 76, 179, 55, 179, 32, 179, 13, 179,
    244, 178, 224, 178, 202, 178, 179, 178, 160, 178, 135, 178, 114, 178, 92,
    178, 67, 178, 49, 178, 24, 178, 3, 178, 239, 177, 215, 177, 198, 177, 175,
    177, 155, 177, 137, 177, 110, 177, 93, 177, 67, 177, 45, 177, 24, 177, 251,
    176, 232, 176, 204, 176, 181, 176, 159, 176, 128, 176, 108, 176, 84, 176,
    58, 176, 34, 176, 3, 176, 238, 175, 208, 175, 184, 175, 159, 175, 128, 175,
    107, 175, 75, 175, 50, 175, 26, 175, 251, 174, 230, 174, 197, 174, 170,
    174, 145, 174, 110, 174, 89, 174, 64, 174, 31, 174, 2, 174, 225, 173, 202,
    173, 169, 173, 141, 173, 115, 173, 78, 173, 56, 173, 33, 173, 253, 172,
    221, 172, 188, 172, 165, 172, 128, 172, 101, 172, 73, 172, 34, 172, 11,
    172, 231, 171, 203, 171, 173, 171, 158, 171, 162, 171, 154, 171, 158, 171,
    162, 171, 156, 171, 166, 171, 160, 171, 167, 171, 177, 171, 162, 171, 172,
    171, 169, 171, 171, 171, 173, 171, 169, 171, 178, 171, 171, 171, 175, 171,
    178, 171, 173, 171, 182, 171, 186, 171, 183, 171, 183, 171, 180, 171, 190,
    171, 184, 171, 187, 171, 189, 171, 185, 171, 195, 171, 188, 171, 191, 171,
    194, 171, 190, 171, 200, 171, 195, 171, 199, 171, 203, 171, 197, 171, 206,
    171, 199, 171, 203, 171, 208, 171, 202, 171, 211, 171, 217, 171, 211, 171,
    212, 171, 209, 171, 217, 171, 214, 171, 215, 171, 216, 171, 213, 171, 222,
    171, 215, 171, 219, 171, 220, 171, 225, 171, 233, 171, 218, 171, 225, 171,
    229, 171, 221, 171, 230, 171, 226, 171, 230, 171, 230, 171, 235, 171, 243,
    171, 231, 171, 235, 171, 235, 171, 231, 171, 239, 171, 234, 171, 237, 171,
    239, 171, 234, 171, 244, 171, 239, 171, 240, 171, 242, 171, 240, 171, 255,
    171, 245, 171, 247, 171, 244, 171, 251, 171, 7, 172, 247, 171, 254, 171,
    253, 171, 249, 171, 4, 172, 251, 171, 3, 172, 16, 172, 253, 171, 8, 172,
    11, 172, 4, 172, 8, 172, 3, 172, 11, 172, 5, 172, 7, 172, 12, 172, 6, 172,
    15, 172, 10, 172, 13, 172, 18, 172, 10, 172, 21, 172, 17, 172, 18, 172, 19,
    172, 15, 172, 25, 172, 20, 172, 23, 172, 25, 172, 20, 172, 29, 172, 25,
    172, 28, 172, 32, 172, 25, 172, 34, 172, 31, 172, 32, 172, 34, 172, 32,
    172, 41, 172, 34, 172, 37, 172, 42, 172, 35, 172, 43, 172, 39, 172, 42,
    172, 48, 172, 40, 172, 49, 172, 44, 172, 46, 172, 47, 172, 53, 172, 60,
    172, 46, 172, 52, 172, 55, 172, 50, 172, 58, 172, 51, 172, 56, 172, 61,
    172, 56, 172, 64, 172, 58, 172, 62, 172, 65, 172, 60, 172, 69, 172, 65,
    172, 69, 172, 71, 172, 75, 172, 83, 172, 78, 172, 81, 172, 85, 172, 81,
    172, 89, 172, 75, 172, 80, 172, 82, 172, 79, 172, 88, 172, 82, 172, 85,
    172, 85, 172, 90, 172, 98, 172, 93, 172, 91, 172, 88, 172, 88, 172, 96,
    172, 91, 172, 93, 172, 95, 172, 95, 172, 104, 172, 103, 172, 104, 172, 109,
    172, 103, 172, 111, 172, 108, 172, 106, 172, 105, 172, 102, 172, 111, 172,
    104, 172, 108, 172, 110, 172, 106, 172, 117, 172, 109, 172, 113, 172, 118,
    172, 111, 172, 120, 172, 115, 172, 117, 172, 120, 172, 116, 172, 125, 172,
    121, 172, 123, 172, 123, 172, 129, 172, 136, 172, 122, 172, 128, 172, 130,
    172, 125, 172, 134, 172, 138, 172, 134, 172, 134, 172, 138, 172, 145, 172,
    142, 172, 140, 172, 138, 172, 144, 172, 152, 172, 138, 172, 144, 172, 149,
    172, 143, 172, 151, 172, 155, 172, 151, 172, 154, 172, 148, 172, 155, 172,
    162, 172, 156, 172, 156, 172, 153, 172, 162, 172, 157, 172, 159, 172, 163,
    172, 156, 172, 166, 172, 158, 172, 167, 172, 176, 172, 159, 172, 169, 172,
    160, 172, 164, 172, 168, 172, 160, 172, 169, 172, 164, 172, 167, 172, 169,
    172, 164, 172, 172, 172, 165, 172, 168, 172, 173, 172, 163, 172, 170, 172,
    167, 172, 169, 172, 173, 172, 167, 172, 175, 172, 170, 172, 172, 172, 176,
    172, 170, 172, 178, 172, 173, 172, 175, 172, 178, 172, 174, 172, 183, 172,
    178, 172, 181, 172, 184, 172, 177, 172, 186, 172, 183, 172, 185, 172, 189,
    172, 181, 172, 188, 172, 185, 172, 187, 172, 187, 172, 191, 172, 198, 172,
    186, 172, 190, 172, 191, 172, 186, 172, 194, 172, 188, 172, 190, 172, 195,
    172, 188, 172, 196, 172, 192, 172, 191, 172, 194, 172, 189, 172, 199, 172,
    193, 172, 197, 172, 202, 172, 195, 172, 204, 172, 199, 172, 201, 172, 203,
    172, 204, 172, 213, 172, 202, 172, 205, 172, 208, 172, 203, 172, 212, 172,
    206, 172, 209, 172, 212, 172, 207, 172, 216, 172, 210, 172, 213, 172, 218,
    172, 209, 172, 218, 172, 215, 172, 215, 172, 218, 172, 213, 172, 221, 172,
    222, 172, 219, 172, 221, 172, 214, 172, 223, 172, 228, 172, 222, 172, 223,
    172, 216, 172, 225, 172, 216, 172, 224, 172, 237, 172, 217, 172, 228, 172,
    225, 172, 226, 172, 230, 172, 224, 172, 232, 172, 225, 172, 229, 172, 233,
    172, 226, 172, 235, 172, 229, 172, 232, 172, 238, 172, 230, 172, 238, 172,
    233, 172, 235, 172, 240, 172, 236, 172, 243, 172, 238, 172, 240, 172, 243,
    172, 238, 172, 249, 172, 240, 172, 248, 172, 3, 173, 240, 172, 251, 172,
    245, 172, 248, 172,
#else
    00, 00, 00, 00, 00, 00, 00, 00,
    33, 00, 33, 00, 00, 00, 00, 00,
    0xff, 0x00, 0xaa, 0x00, 0x80,
    229, 212, 210, 212, 192, 212, 174, 212, 155, 212,
    137, 212, 119, 212, 101, 212, 83, 212, 64, 212, 46, 212, 28, 212, 11, 212,
    251, 211, 235, 211, 217, 211, 199, 211, 181, 211, 163, 211, 145, 211, 127,
    211, 109, 211, 91, 211, 73, 211, 55, 211, 38, 211, 20, 211, 2, 211, 240,
    210, 223, 210, 205, 210, 187, 210, 169, 210, 151, 210, 133, 210, 116, 210,
    99, 210, 81, 210, 62, 210, 44, 210, 27, 210, 8, 210, 247, 209, 230, 209,
    212, 209, 194, 209, 176, 209, 158, 209, 140, 209, 123, 209, 106, 209, 88,
    209, 70, 209, 53, 209, 35, 209, 18, 209, 1, 209, 240, 208, 223, 208, 205,
    208, 187, 208, 170, 208, 153, 208, 137, 208, 120, 208, 103, 208, 86, 208,
    69, 208, 53, 208, 36, 208, 20, 208, 3, 208, 245, 207, 230, 207, 212, 207,
    195, 207, 178, 207, 160, 207, 144, 207, 127, 207, 110, 207, 92, 207, 75,
    207, 59, 207, 41, 207, 24, 207, 7, 207, 246, 206, 230, 206, 213, 206, 197,
    206, 179, 206, 163, 206, 145, 206, 128, 206, 111, 206, 94, 206, 77, 206,
    61, 206, 43, 206, 26, 206, 9, 206, 249, 205, 233, 205, 216, 205, 199, 205,
    182, 205, 165, 205, 149, 205, 132, 205, 116, 205, 99, 205, 83, 205, 66,
    205, 49, 205, 32, 205, 15, 205, 255, 204, 239, 204, 222, 204, 206, 204,
    189, 204, 173, 204, 155, 204, 138, 204, 122, 204, 105, 204, 88, 204, 72,
    204, 55, 204, 38, 204, 21, 204, 6, 204, 248, 203, 233, 203, 216, 203, 199,
    203, 182, 203, 166, 203, 149, 203, 133, 203, 116, 203, 99, 203, 83, 203,
    66, 203, 49, 203, 32, 203, 16, 203, 0, 203, 240, 202, 223, 202, 206, 202,
    189, 202, 174, 202, 157, 202, 141, 202, 124, 202, 107, 202, 91, 202, 74,
    202, 58, 202, 41, 202, 25, 202, 9, 202, 249, 201, 233, 201, 216, 201, 199,
    201, 183, 201, 166, 201, 150, 201, 133, 201, 117, 201, 101, 201, 84, 201,
    67, 201, 50, 201, 34, 201, 18, 201, 1, 201, 241, 200, 225, 200, 208, 200,
    193, 200, 177, 200, 161, 200, 144, 200, 128, 200, 112, 200, 96, 200, 80,
    200, 63, 200, 46, 200, 31, 200, 14, 200, 0, 200, 242, 199, 226, 199, 211,
    199, 194, 199, 177, 199, 161, 199, 146, 199, 130, 199, 113, 199, 98, 199,
    81, 199, 66, 199, 51, 199, 34, 199, 19, 199, 3, 199, 243, 198, 228, 198,
    212, 198, 196, 198, 179, 198, 163, 198, 147, 198, 131, 198, 115, 198, 100,
    198, 84, 198, 69, 198, 53, 198, 36, 198, 21, 198, 5, 198, 246, 197, 231,
    197, 216, 197, 200, 197, 184, 197, 169, 197, 154, 197, 138, 197, 122, 197,
    106, 197, 91, 197, 76, 197, 60, 197, 45, 197, 29, 197, 15, 197, 255, 196,
    240, 196, 225, 196, 208, 196, 194, 196, 178, 196, 161, 196, 146, 196, 130,
    196, 115, 196, 99, 196, 84, 196, 67, 196, 51, 196, 35, 196, 20, 196, 6,
    196, 249, 195, 234, 195, 219, 195, 204, 195, 188, 195, 173, 195, 157, 195,
    141, 195, 126, 195, 110, 195, 95, 195, 79, 195, 64, 195, 48, 195, 33, 195,
    18, 195, 3, 195, 245, 194, 230, 194, 214, 194, 200, 194, 186, 194, 171,
    194, 156, 194, 142, 194, 127, 194, 112, 194, 97, 194, 82, 194, 68, 194, 53,
    194, 38, 194, 23, 194, 7, 194, 250, 193, 236, 193, 221, 193, 207, 193, 192,
    193, 177, 193, 162, 193, 148, 193, 133, 193, 119, 193, 105, 193, 90, 193,
    75, 193, 60, 193, 45, 193, 31, 193, 16, 193, 1, 193, 243, 192, 228, 192,
    214, 192, 199, 192, 184, 192, 170, 192, 155, 192, 140, 192, 126, 192, 111,
    192, 97, 192, 82, 192, 67, 192, 53, 192, 38, 192, 23, 192, 8, 192, 252,
    191, 239, 191, 224, 191, 210, 191, 195, 191, 180, 191, 166, 191, 151, 191,
    136, 191, 122, 191, 107, 191, 92, 191, 77, 191, 63, 191, 48, 191, 33, 191,
    19, 191, 4, 191, 245, 190, 231, 190, 216, 190, 201, 190, 186, 190, 172,
    190, 157, 190, 143, 190, 129, 190, 115, 190, 101, 190, 87, 190, 72, 190,
    59, 190, 44, 190, 30, 190, 16, 190, 3, 190, 245, 189, 231, 189, 216, 189,
    202, 189, 188, 189, 173, 189, 159, 189, 145, 189, 131, 189, 117, 189, 103,
    189, 88, 189, 74, 189, 60, 189, 46, 189, 33, 189, 18, 189, 5, 189, 248,
    188, 234, 188, 221, 188, 207, 188, 193, 188, 179, 188, 165, 188, 152, 188,
    138, 188, 124, 188, 111, 188, 97, 188, 84, 188, 70, 188, 57, 188, 44, 188,
    31, 188, 18, 188, 6, 188, 251, 187, 240, 187, 228, 187, 215, 187, 201, 187,
    188, 187, 175, 187, 162, 187, 149, 187, 136, 187, 123, 187, 110, 187, 97,
    187, 84, 187, 71, 187, 58, 187, 45, 187, 32, 187, 20, 187, 7, 187, 250,
    186, 237, 186, 225, 186, 211, 186, 198, 186, 185, 186, 172, 186, 159, 186,
    145, 186, 131, 186, 119, 186, 105, 186, 92, 186, 79, 186, 65, 186, 53, 186,
    39, 186, 25, 186, 12, 186, 255, 185, 242, 185, 230, 185, 217, 185, 204,
    185, 191, 185, 179, 185, 165, 185, 152, 185, 140, 185, 127, 185, 115, 185,
    102, 185, 90, 185, 77, 185, 65, 185, 53, 185, 41, 185, 29, 185, 16, 185, 4,
    185, 248, 184, 236, 184, 224, 184, 212, 184, 199, 184, 188, 184, 176, 184,
    163, 184, 152, 184, 139, 184, 128, 184, 116, 184, 104, 184, 91, 184, 79,
    184, 67, 184, 55, 184, 43, 184, 31, 184, 18, 184, 8, 184, 253, 183, 243,
    183, 231, 183, 219, 183, 207, 183, 194, 183, 182, 183, 169, 183, 157, 183,
    144, 183, 131, 183, 118, 183, 106, 183, 93, 183, 81, 183, 68, 183, 56, 183,
    43, 183, 29, 183, 17, 183, 3, 183, 246, 182, 233, 182, 220, 182, 206, 182,
    191, 182, 177, 182, 162, 182, 149, 182, 136, 182, 119, 182, 106, 182, 90,
    182, 76, 182, 61, 182, 45, 182, 31, 182, 14, 182, 255, 181, 238, 181, 222,
    181, 206, 181, 188, 181, 173, 181, 155, 181, 137, 181, 120, 181, 99, 181,
    83, 181, 62, 181, 43, 181, 25, 181, 4, 181, 244, 180, 222, 180, 203, 180,
    186, 180, 165, 180, 150, 180, 129, 180, 111, 180, 94, 180, 72, 180, 56,
    180, 36, 180, 17, 180, 253, 179, 231, 179, 215, 179, 193, 179, 173, 179,
    155, 179, 134, 179, 118, 179, 96, 179, 76, 179, 55, 179, 32, 179, 13, 179,
    244, 178, 224, 178, 202, 178, 179, 178, 160, 178, 135, 178, 114, 178, 92,
    178, 67, 178, 49, 178, 24, 178, 3, 178, 239, 177, 215, 177, 198, 177, 175,
    177, 155, 177, 137, 177, 110, 177, 93, 177, 67, 177, 45, 177, 24, 177, 251,
    176, 232, 176, 204, 176, 181, 176, 159, 176, 128, 176, 108, 176, 84, 176,
    58, 176, 34, 176, 3, 176, 238, 175, 208, 175, 184, 175, 159, 175, 128, 175,
    107, 175, 75, 175, 50, 175, 26, 175, 251, 174, 230, 174, 197, 174, 170,
    174, 145, 174, 110, 174, 89, 174, 64, 174, 31, 174, 2, 174, 225, 173, 202,
    173, 169, 173, 141, 173, 115, 173, 78, 173, 56, 173, 33, 173, 253, 172,
    221, 172, 188, 172, 165, 172, 128, 172, 101, 172, 73, 172, 34, 172, 11,
    172, 231, 171, 203, 171, 173, 171, 158, 171, 162, 171, 154, 171, 158, 171,
    162, 171, 156, 171, 166, 171, 160, 171, 167, 171, 177, 171, 162, 171, 172,
    171, 169, 171, 171, 171, 173, 171, 169, 171, 178, 171, 171, 171, 175, 171,
    178, 171, 173, 171, 182, 171, 186, 171, 183, 171, 183, 171, 180, 171, 190,
    171, 184, 171, 187, 171, 189, 171, 185, 171, 195, 171, 188, 171, 191, 171,
    194, 171, 190, 171, 200, 171, 195, 171, 199, 171, 203, 171, 197, 171, 206,
    171, 199, 171, 203, 171, 208, 171, 202, 171, 211, 171, 217, 171, 211, 171,
    212, 171, 209, 171, 217, 171, 214, 171, 215, 171, 216, 171, 213, 171, 222,
    171, 215, 171, 219, 171, 220, 171, 225, 171, 233, 171, 218, 171, 225, 171,
    229, 171, 221, 171, 230, 171, 226, 171, 230, 171, 230, 171, 235, 171, 243,
    171, 231, 171, 235, 171, 235, 171, 231, 171, 239, 171, 234, 171, 237, 171,
    239, 171, 234, 171, 244, 171, 239, 171, 240, 171, 242, 171, 240, 171, 255,
    171, 245, 171, 247, 171, 244, 171, 251, 171, 7, 172, 247, 171, 254, 171,
    253, 171, 249, 171, 4, 172, 251, 171, 3, 172, 16, 172, 253, 171, 8, 172,
    11, 172, 4, 172, 8, 172, 3, 172, 11, 172, 5, 172, 7, 172, 12, 172, 6, 172,
    15, 172, 10, 172, 13, 172, 18, 172, 10, 172, 21, 172, 17, 172, 18, 172, 19,
    172, 15, 172, 25, 172, 20, 172, 23, 172, 25, 172, 20, 172, 29, 172, 25,
    172, 28, 172, 32, 172, 25, 172, 34, 172, 31, 172, 32, 172, 34, 172, 32,
    172, 41, 172, 34, 172, 37, 172, 42, 172, 35, 172, 43, 172, 39, 172, 42,
    172, 48, 172, 40, 172, 49, 172, 44, 172, 46, 172, 47, 172, 53, 172, 60,
    172, 46, 172, 52, 172, 55, 172, 50, 172, 58, 172, 51, 172, 56, 172, 61,
    172, 56, 172, 64, 172, 58, 172, 62, 172, 65, 172, 60, 172, 69, 172, 65,
    172, 69, 172, 71, 172, 75, 172, 83, 172, 78, 172, 81, 172, 85, 172, 81,
    172, 89, 172, 75, 172, 80, 172, 82, 172, 79, 172, 88, 172, 82, 172, 85,
    172, 85, 172, 90, 172, 98, 172, 93, 172, 91, 172, 88, 172, 88, 172, 96,
    172, 91, 172, 93, 172, 95, 172, 95, 172, 104, 172, 103, 172, 104, 172, 109,
    172, 103, 172, 111, 172, 108, 172, 106, 172, 105, 172, 102, 172, 111, 172,
    104, 172, 108, 172, 110, 172, 106, 172, 117, 172, 109, 172, 113, 172, 118,
    172, 111, 172, 120, 172, 115, 172, 117, 172, 120, 172, 116, 172, 125, 172,
    121, 172, 123, 172, 123, 172, 129, 172, 136, 172, 122, 172, 128, 172, 130,
    172, 125, 172, 134, 172, 138, 172, 134, 172, 134, 172, 138, 172, 145, 172,
    142, 172, 140, 172, 138, 172, 144, 172, 152, 172, 138, 172, 144, 172, 149,
    172, 143, 172, 151, 172, 155, 172, 151, 172, 154, 172, 148, 172, 155, 172,
    162, 172, 156, 172, 156, 172, 153, 172, 162, 172, 157, 172, 159, 172, 163,
    172, 156, 172, 166, 172, 158, 172, 167, 172, 176, 172, 159, 172, 169, 172,
    160, 172, 164, 172, 168, 172, 160, 172, 169, 172, 164, 172, 167, 172, 169,
    172, 164, 172, 172, 172, 165, 172, 168, 172, 173, 172, 163, 172, 170, 172,
    167, 172, 169, 172, 173, 172, 167, 172, 175, 172, 170, 172, 172, 172, 176,
    172, 170, 172, 178, 172, 173, 172, 175, 172, 178, 172, 174, 172, 183, 172,
    178, 172, 181, 172, 184, 172, 177, 172, 186, 172, 183, 172, 185, 172, 189,
    172, 181, 172, 188, 172, 185, 172, 187, 172, 187, 172, 191, 172, 198, 172,
    186, 172, 190, 172, 191, 172, 186, 172, 194, 172, 188, 172, 190, 172, 195,
    172, 188, 172, 196, 172, 192, 172, 191, 172, 194, 172, 189, 172, 199, 172,
    193, 172, 197, 172, 202, 172, 195, 172, 204, 172, 199, 172, 201, 172, 203,
    172, 204, 172, 213, 172, 202, 172, 205, 172, 208, 172, 203, 172, 212, 172,
    206, 172, 209, 172, 212, 172, 207, 172, 216, 172, 210, 172, 213, 172, 218,
    172, 209, 172, 218, 172, 215, 172, 215, 172, 218, 172, 213, 172, 221, 172,
    222, 172, 219, 172, 221, 172, 214, 172, 223, 172, 228, 172, 222, 172, 223,
    172, 216, 172, 225, 172, 216, 172, 224, 172, 237, 172, 217, 172, 228, 172,
    225, 172, 226, 172, 230, 172, 224, 172, 232, 172, 225, 172, 229, 172, 233,
    172, 226, 172, 235, 172, 229, 172, 232, 172, 238, 172, 230, 172, 238, 172,
    233, 172, 235, 172, 240, 172, 236, 172, 243, 172, 238, 172, 240, 172, 243,
    172, 238, 172, 249, 172, 240, 172, 248, 172, 3, 173, 240, 172, 251, 172,
    245, 172, 248, 172
#endif
};

static int data_sample_size = sizeof(data_sample);
