#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sie_internal.h"

static size_t write_fn(void *file, const char *data, size_t size)
{
    return fwrite(data, 1, size, file);
}

static char *basename_no_ext(const char *name)
{
    char *prefix = strdup(name);
    char *dot, *slash;

    dot = strrchr(prefix, '.');
    if (dot)
        *dot = 0;

    slash = strrchr(prefix, '/');
    if (slash)
        memmove(prefix, slash + 1, strlen(slash));

    return prefix;
}

static int split(char *fname, int tests_per_file)
{
    sie_Context *ctx = sie_context_new();
    sie_File *infile = sie_file_open(ctx, fname);
    sie_Iterator *test_iter;
    sie_Test *test;
    sie_Iterator *tag_iter;
    sie_Tag *tag;
    sie_Iterator *ch_iter;
    sie_Channel *ch;
    int ntests = 0;
    int last = 0;
    int ctest = 0;
    char *prefix = basename_no_ext(fname);
    char *outfname = calloc(1, strlen(prefix) + 32);
    FILE *outfile = NULL;
    sie_Writer *writer;
    sie_Sifter *sifter;

    test_iter = sie_get_tests(infile);
    while ((test = sie_iterator_next(test_iter)))
        ++ntests;
    sie_release(test_iter);

    test_iter = sie_get_tests(infile);
    while ((test = sie_iterator_next(test_iter))) {
        ++ctest;
        if (!outfile) {
            last = ctest + tests_per_file - 1;
            if (last > ntests)
                last = ntests;
            sprintf(outfname, "%s_%04d-%04d.sie", prefix, ctest, last);
            fprintf(stderr, "opening %s\n", outfname);
            outfile = fopen(outfname, "w");
            if (!outfile) {
                perror("Couldn't open output file");
                exit(1);
            }
            writer = sie_writer_new(ctx, write_fn, outfile);
            sie_writer_xml_header(writer);
            sifter = sie_sifter_new(writer);

            tag_iter = sie_get_tags(infile);
            while ((tag = sie_iterator_next(tag_iter)))
                sie_sifter_add(sifter, tag);
            sie_release(tag_iter);
        }

        tag_iter = sie_get_tags(test);
        while ((tag = sie_iterator_next(tag_iter)))
            sie_sifter_add(sifter, tag);
        sie_release(tag_iter);

        ch_iter = sie_get_channels(test);
        while ((ch = sie_iterator_next(ch_iter)))
            sie_sifter_add(sifter, ch);
        sie_release(ch_iter);
        
        if (ctest == last) {
            sie_release(sifter);
            sie_release(writer);
            fclose(outfile);
            outfile = NULL;
        }
    }
    sie_release(test_iter);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage\n");
        exit(1);
    }

    return split(argv[1], atoi(argv[2]));
}
