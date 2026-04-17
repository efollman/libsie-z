#include "sie_config.h"

#include "my-check.h"
#include "test-utils.h"

START_TEST (test_null_histogram)
{
    sie_Histogram *hist = sie_histogram_new(NULL);
    fail_unless(hist == NULL);
}
END_TEST

START_TEST (test_histogram_null_accessors)
{
    sie_float64 a = 3, b = 42;
    fail_unless(sie_histogram_get_num_dims(NULL) == 0);
    fail_unless(sie_histogram_get_num_bins(NULL, 0) == 0);
    fail_unless(sie_histogram_get_num_bins(NULL, 12) == 0);
    sie_histogram_get_bin_bounds(NULL, 0, &a, &b);
    fail_unless(a == 3);
    fail_unless(b == 42);
    fail_unless(sie_histogram_get_bin(NULL, NULL) == 0.0);
    fail_unless(sie_histogram_get_next_nonzero_bin(NULL, NULL, NULL) == 0.0);
}
END_TEST

START_TEST (test_histogram_open)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 49);
    sie_Histogram *hist = sie_histogram_new(channel);
    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_if(hist == NULL);
    sie_release(hist);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_histogram_open_timhis)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 32);
    sie_Histogram *hist = sie_histogram_new(channel);
    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_unless(hist == NULL);
    fail_unless(sie_check_exception(ctx) != NULL);
    sie_release(sie_get_exception(ctx));
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_histogram_basic_accessors)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 49);
    sie_Histogram *hist = sie_histogram_new(channel);
    sie_float64 lower[12], upper[12];
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output = sie_spigot_get(spigot);
    int i;

    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_if(hist == NULL);

    fail_unless(sie_histogram_get_num_dims(hist) == 1);
    fail_unless(sie_histogram_get_num_bins(hist, 0) == 12);
    fail_unless(sie_histogram_get_num_bins(hist, 12) == 0);

    fail_if(spigot == NULL);
    fail_if(output == NULL);
    
    sie_histogram_get_bin_bounds(hist, 0, lower, upper);

    for (i = 0; i < 12; i++) {
        fail_if(lower[i] != output->v[1].float64[i],
                "lower bound mismatch: %d", i);
        fail_if(upper[i] != output->v[2].float64[i],
                "upper bound mismatch: %d", i);
    }
    
    sie_release(spigot);
    sie_release(hist);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_histogram_basic_accessors_2d)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 54);
    sie_Histogram *hist = sie_histogram_new(channel);
    sie_float64 lower[13], upper[13];
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output = sie_spigot_get(spigot);

    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_if(hist == NULL);

    fail_unless(sie_histogram_get_num_dims(hist) == 2);
    fail_unless(sie_histogram_get_num_bins(hist, 0) == 9);
    fail_unless(sie_histogram_get_num_bins(hist, 1) == 13);
    fail_unless(sie_histogram_get_num_bins(hist, 12) == 0);

    fail_if(spigot == NULL);
    fail_if(output == NULL);

    sie_histogram_get_bin_bounds(hist, 0, lower, upper);
    
    for (i = 0; i < 9; i++) {
        fail_if(lower[i] != output->v[1].float64[i * 13],
                "lower bound mismatch: %d", i);
        fail_if(upper[i] != output->v[2].float64[i * 13],
                "upper bound mismatch: %d", i);
    }

    sie_histogram_get_bin_bounds(hist, 1, lower, upper);
    
    for (i = 0; i < 13; i++) {
        fail_if(lower[i] != output->v[3].float64[i],
                "lower bound mismatch: %d", i);
        fail_if(upper[i] != output->v[4].float64[i],
                "upper bound mismatch: %d", i);
    }

    sie_release(spigot);
    sie_release(hist);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_histogram_bins)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 49);
    sie_Histogram *hist = sie_histogram_new(channel);
    size_t indices[1];
    size_t start = 0;

    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_if(hist == NULL);

    indices[0] = 5;
    fail_unless(sie_histogram_get_bin(hist, indices) == 460.0);

    indices[0] = 0;
    fail_unless(sie_histogram_get_next_nonzero_bin(hist, &start, indices)
                == 460.0);
    fail_unless(indices[0] == 5);
    fail_unless(sie_histogram_get_next_nonzero_bin(hist, &start, indices)
                == 0.0);

    sie_release(hist);
    sie_release(channel);
    sie_release(file);
}
END_TEST

START_TEST (test_histogram_bins_2d)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_comprehensive2_VBM_20050908.sie");
    sie_Channel *channel = sie_get_channel(file, 54);
    sie_Histogram *hist = sie_histogram_new(channel);
    size_t indices[2];
/*
    size_t start = 0;
*/

    fail_if(file == NULL);
    fail_if(channel == NULL);
    fail_if(hist == NULL);

    indices[0] = 0; 
    indices[1] = 2;
    fail_unless(sie_histogram_get_bin(hist, indices) == 10.0);

/*
    sie_float64 v;
    while ((v = sie_histogram_get_next_nonzero_bin(hist, &start, indices))) {
        printf("value %10f: %3d (%2d, %2d)\n",
               v, start - 1, indices[0], indices[1]);
    }
*/

/*
    indices[0] = 0;
    fail_unless(sie_histogram_get_next_nonzero_bin(hist, &start, indices)
                == 460.0);
    fail_unless(indices[0] == 5);
    fail_unless(sie_histogram_get_next_nonzero_bin(hist, &start, indices)
                == 0.0);
*/

    sie_release(hist);
    sie_release(channel);
    sie_release(file);
}
END_TEST

Suite *histogram_suite(void)
{
    Suite *s = suite_create("histogram");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_null_histogram);
    tcase_add_test(tc_basics, test_histogram_null_accessors);
    tcase_add_test(tc_basics, test_histogram_open);
    tcase_add_test(tc_basics, test_histogram_open_timhis);
    tcase_add_test(tc_basics, test_histogram_basic_accessors);
    tcase_add_test(tc_basics, test_histogram_basic_accessors_2d);
    tcase_add_test(tc_basics, test_histogram_bins);
    tcase_add_test(tc_basics, test_histogram_bins_2d);

    return s;
}
