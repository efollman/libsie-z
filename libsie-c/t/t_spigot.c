#include "sie_config.h"

#include <math.h>
#include "my-check.h"
#include "test-utils.h"

START_TEST (test_group_spigot_seek_tell)
{
    const static int n = 7;
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Group *group = sie_group_new(file, 0);
    sie_Spigot *spigot;
    struct {
        size_t size;
        void *ptr;
    } stuff[7]; /* KLUDGE n */
    int i;
    sie_Output *output;

    fail_unless(spigot = sie_attach_spigot(group));
    for (i = 0; i < n; i++) {
        fail_unless(output = sie_spigot_get(spigot));
        fail_unless(output->block == i);
        stuff[i].ptr = output->v[0].raw[0].ptr;
        stuff[i].size = output->v[0].raw[0].size;
        output->v[0].raw[0].claimed = 1;
    }
    fail_if(sie_spigot_get(spigot));
    sie_release(spigot);

    spigot = sie_attach_spigot(group);
    fail_unless(sie_spigot_tell(spigot) == 0);
    fail_unless(sie_spigot_seek(spigot, 0) == 0);
    fail_unless(sie_spigot_seek(spigot, SIE_SPIGOT_SEEK_END) == n);
    fail_unless(sie_spigot_tell(spigot) == n);
    fail_if(sie_spigot_get(spigot));

    for (i = n - 1; i >= 0; i--) {
        fail_unless(sie_spigot_seek(spigot, i) == i);
        fail_unless(output = sie_spigot_get(spigot));
        fail_unless(stuff[i].size == output->v[0].raw[0].size);
        fail_if(memcmp(stuff[i].ptr, output->v[0].raw[0].ptr,
                       stuff[i].size));
    }

    for (i = 0; i < n; i++)
        free(stuff[i].ptr);
    
    sie_release(spigot);
    sie_release(group);
    sie_release(file);
}
END_TEST

START_TEST (test_channel_spigot_seek_tell)
{
    static const int n = 82;
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    sie_Output *output;

    fail_unless(spigot = sie_attach_spigot(channel));
    fail_unless(sie_spigot_tell(spigot) == 0);
    fail_unless(sie_spigot_seek(spigot, 0) == 0);
    fail_unless(sie_spigot_seek(spigot, SIE_SPIGOT_SEEK_END) == n);
    fail_unless(sie_spigot_tell(spigot) == n);
    fail_if(sie_spigot_get(spigot));

    fail_unless(sie_spigot_seek(spigot, 10) == 10);
    fail_unless(sie_spigot_tell(spigot) == 10);
    fail_unless(output = sie_spigot_get(spigot));
    fail_unless(output->block == 10);
    fail_unless(output->v[0].float64[0] == 32);

    fail_unless(sie_spigot_seek(spigot, n - 1) == n - 1);
    fail_unless(sie_spigot_tell(spigot) == n - 1);
    fail_unless(output = sie_spigot_get(spigot));
    fail_unless(output->block == n - 1);
    fail_unless(output->v[0].float64[0] == 259.2);
    
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_channel_spigot_lower_bound)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    size_t block = 42, scan = 42;
    int i, found;
    double d;
    sie_Output *output;

    fail_unless(spigot = sie_attach_spigot(channel));

    fail_unless(sie_lower_bound(spigot, 0, -1, &block, &scan));
    fail_unless(block == 0);
    fail_unless(scan == 0);

    fail_if(sie_lower_bound(spigot, 0, 4000, &block, &scan));

    for (i = 0; i < 300; i += 26) {
        found = sie_lower_bound(spigot, 0, i, &block, &scan);
        fail_if(!found && i < 262.4, "didn't find time value %d", i);
        if (found) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(output->v[0].float64[scan] == i,
                        "block %d scan %d value %f != %d",
                        block, scan, output->v[0].float64[scan], i);
        }
    }

    for (d = -1; d < 264; d += 0.125) {
        found = sie_lower_bound(spigot, 0, d, &block, &scan);
        fail_if(!found && d < 262.4, "didn't find time value %f", d);
        if (found && d >= 0) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(output->v[0].float64[scan] == d,
                        "block %d scan %d value %f != %f",
                        block, scan, output->v[0].float64[scan], d);
        }
        if (d == 7)
            d = 260;
    }

    for (i = 0, d = 0; d < 264; ++i, d = i * 3.2 - 0.000001) {
        found = sie_lower_bound(spigot, 0, d, &block, &scan);
        fail_if(!found && d <= 262.399, "didn't find time value %f", d);
        if (found) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(fabs(output->v[0].float64[scan] - d) < 0.00001,
                        "block %d scan %d value %f != %f",
                        block, scan, output->v[0].float64[scan], d);
        }
    }

    fail_unless(sie_lower_bound(spigot, 0, 1.9989, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 1.999);

    fail_unless(sie_lower_bound(spigot, 0, 1.999, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 1.999);

    fail_unless(sie_lower_bound(spigot, 0, 1.9991, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 2.0);
    
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_channel_spigot_upper_bound)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    size_t block = 42, scan = 42;
    int i, found;
    double d;
    sie_Output *output;

    fail_unless(spigot = sie_attach_spigot(channel));

    fail_unless(sie_upper_bound(spigot, 0, 4000, &block, &scan));
    fail_unless(block == 81);
    fail_unless(block == sie_spigot_seek(spigot, ~0) - 1);
    fail_unless(scan == 3199);

    fail_if(sie_upper_bound(spigot, 0, -1, &block, &scan));

    for (i = 0; i < 300; i += 26) {
        found = sie_upper_bound(spigot, 0, i, &block, &scan);
        fail_if(!found, "didn't find time value %d", i);
        if (found && i < 263) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(output->v[0].float64[scan] == i,
                        "block %d scan %d value %f != %d",
                        block, scan, output->v[0].float64[scan], i);
        }
    }

    for (d = -1; d < 264; d += 0.125) {
        found = sie_upper_bound(spigot, 0, d, &block, &scan);
        fail_if(!found && d >= 0, "didn't find time value %f", d);
        if (found && d < 262.4) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(output->v[0].float64[scan] == d,
                        "block %d scan %d value %f != %f",
                        block, scan, output->v[0].float64[scan], d);
        }
        if (d == 7)
            d = 260;
    }

    for (i = 0, d = 0; d < 264; ++i, d = i * 3.2 + 0.000001) {
        found = sie_upper_bound(spigot, 0, d, &block, &scan);
        fail_if(!found, "didn't find time value %f", d);
        if (found && d < 262.4) {
            sie_spigot_seek(spigot, block);
            output = sie_spigot_get(spigot);
            fail_unless(fabs(output->v[0].float64[scan] - d) < 0.00001,
                        "block %d scan %d value %f != %f",
                        block, scan, output->v[0].float64[scan], d);
        }
    }

    fail_unless(sie_upper_bound(spigot, 0, 1.9989, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 1.998);

    fail_unless(sie_upper_bound(spigot, 0, 1.999, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 1.999);

    fail_unless(sie_upper_bound(spigot, 0, 1.9991, &block, &scan));
    sie_spigot_seek(spigot, block);
    output = sie_spigot_get(spigot);
    fail_unless(output->v[0].float64[scan] == 1.999);

    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_channel_spigot_disable_transforms)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    sie_Output *output;
    size_t i;

    fail_unless(spigot = sie_attach_spigot(channel));
    sie_spigot_disable_transforms(spigot, 1);
    fail_unless(output = sie_spigot_get(spigot));

    for (i = 0; i < output->num_scans; i++)
        fail_unless(i == output->v[0].float64[i]);

    fail_unless(sie_spigot_seek(spigot, 0) == 0);
    sie_spigot_disable_transforms(spigot, 0);
    fail_unless(output = sie_spigot_get(spigot));

    for (i = 0; i < output->num_scans; i++)
        fail_unless(i == floor(output->v[0].float64[i] * 1000 + 0.5));
    
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_channel_spigot_transform_output)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    sie_Output *output;
    size_t i;

    fail_unless(spigot = sie_attach_spigot(channel));
    sie_spigot_disable_transforms(spigot, 1);
    fail_unless(output = sie_spigot_get(spigot));

    for (i = 0; i < output->num_scans; i++)
        fail_unless(i == output->v[0].float64[i]);

    sie_spigot_transform_output(spigot, output);

    for (i = 0; i < output->num_scans; i++)
        fail_unless(i == floor(output->v[0].float64[i] * 1000 + 0.5));

    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_spigot_clear_output)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_seek_test.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot;
    sie_Spigot *clear_spigot;
    sie_Output *output;
    sie_Output *clear_output;
    int i = 0;

    fail_unless(spigot = sie_attach_spigot(channel));
    fail_unless(clear_spigot = sie_attach_spigot(channel));
    while ((output = sie_spigot_get(spigot))) {
        clear_output = sie_spigot_get(clear_spigot);
        fail_unless(sie_output_compare(output, clear_output));
        if (++i % 2)
            sie_spigot_clear_output(clear_spigot);
    }
    
    sie_release(clear_spigot);
    sie_release(spigot);
    sie_release(channel);
    sie_release(file);
}
END_TEST

Suite *spigot_suite(void)
{
    Suite *s = suite_create("spigot");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_group_spigot_seek_tell);
    tcase_add_test(tc_basics, test_channel_spigot_seek_tell);
    tcase_add_test(tc_basics, test_channel_spigot_lower_bound);
    tcase_add_test(tc_basics, test_channel_spigot_upper_bound);
    tcase_add_test(tc_basics, test_channel_spigot_disable_transforms);
    tcase_add_test(tc_basics, test_channel_spigot_transform_output);
    tcase_add_test(tc_basics, test_spigot_clear_output);

    return s;
}
