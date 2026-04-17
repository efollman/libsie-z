#include "sie_config.h"

#include <math.h>
#include "my-check.h"
#include "test-utils.h"

static size_t accumulator;

static size_t accumulate_fn(void *file, const char *data, size_t size)
{
    /* sie_Block *block = sie_block_new(ctx); */
    /* printf("accumulate: %lu\n", size); */
    /* sie_block_expand(block, size); */
    /* memcpy(block->data, data, size - 8); */
    /* block->size = size - SIE_OVERHEAD_SIZE; */
    /* block->group = sie_ntoh32(block->data->group); */
    /* printf("  group: %u\n", block->group); */
    /* if (block->group == 0) { */
    /*     printf("  %.*s\n", block->size, block->data->payload); */
    /* } */
    /* sie_release(block); */

    accumulator += size;

    return size;
}

START_TEST (test_sifter_basic)
{
    sie_File *file = sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Writer *writer;
    sie_Sifter *sifter;
    sie_Channel *channel;
    size_t nbytes;

    accumulator = 0;

    writer = sie_writer_new(ctx, accumulate_fn, NULL);
    sie_writer_xml_header(writer);
    sifter = sie_sifter_new(writer);
    channel = sie_get_channel(file, 1);

    sie_sifter_add(sifter, channel);

    nbytes = (size_t)sie_sifter_total_size(sifter);

    sie_release(channel);
    sie_release(sifter);
    sie_release(writer);

    /* printf("nbytes: %lu\n", nbytes); */
    /* printf("accumulator: %lu\n", accumulator); */

    fail_unless(nbytes == accumulator);
    fail_unless(nbytes == 4568);

    sie_release(file);
}
END_TEST

START_TEST (test_sifter_partial)
{
    sie_File *file = sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Writer *writer;
    sie_Sifter *sifter;
    sie_Channel *channel;
    size_t nbytes;

    accumulator = 0;

    writer = sie_writer_new(ctx, accumulate_fn, NULL);
    sie_writer_xml_header(writer);
    sifter = sie_sifter_new(writer);
    channel = sie_get_channel(file, 1);

    sie_sifter_add_channel(sifter, channel, 42, 63);

    nbytes = (size_t)sie_sifter_total_size(sifter);

    sie_release(channel);
    sie_release(sifter);
    sie_release(writer);

    /* printf("nbytes: %lu\n", nbytes); */
    /* printf("accumulator: %lu\n", accumulator); */

    fail_unless(nbytes == accumulator);
    fail_unless(nbytes == 272037);

    sie_release(file);
}
END_TEST

Suite *sifter_suite(void)
{
    Suite *s = suite_create("sifter");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_sifter_basic);
    tcase_add_test(tc_basics, test_sifter_partial);

    return s;
}
