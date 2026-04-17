#include "sie_config.h"

#include <stdio.h>
#include <stdarg.h>
#include "my-check.h"
#include "sie_file.h"
#include "test-utils.h"
#include "sie_group.h"

static const char *test_data = "t/data";
static const char *test_output = "t/output";

static const char *files[] = {
    "sie_min_timhis_a_19EFAA61",
    "sie_comprehensive_VBM_DE81A7BA",
    "sie_comprehensive2_VBM_20050908",
    "sie_float_conversions_20050908",
};
static int num_files = sizeof(files) / sizeof(*files);
 
START_TEST (test_spigot_xml)
{
    sie_File *file =
        sie_file_open(ctx, qpf("%s/%s.sie", test_data, files[i]));
    char *reference;
    sie_Group *group = sie_group_new(file, SIE_XML_GROUP);
    sie_Spigot *spigot = sie_attach_spigot(group);
    sie_vec(xml_string, char, 0);
    sie_Output *output;

    while ( (output = sie_spigot_get(spigot)) )
        sie_vec_append(xml_string,
                       output->v[0].raw[0].ptr,
                       output->v[0].raw[0].size);
    
    sie_release(spigot);
    sie_release(group);
    sie_release(file);

    sie_vec_strcatf(0, &xml_string, "</sie>\n");

    mkdir_p(test_output);
    spew(xml_string, qpf("%s/%s.xml", test_output, files[i]));
    reference = slurp(qpf("%s/%s.xml", test_data, files[i]));

    fail_if(compare(xml_string, reference),
            "XML for %s.sie didn't match expected.", files[i]);
    
    sie_vec_free(xml_string);
    sie_vec_free(reference);
    qpf_free();
}
END_TEST

static const char *cur_file;

static void dump_channel(sie_Channel *channel)
{
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;
    FILE *outf;

    fail_if(spigot == NULL);

    outf = fopen(qpf("%s/%s/%s", test_output, cur_file,
                     safe_fn(sie_get_name(channel))),
                 "wb");
    fail_if(!outf, NULL);

    sie_channel_dump(channel, outf);
    while ( (output = sie_spigot_get(spigot)) )
        sie_output_dump(output, outf);
    
    fclose(outf);

    sie_release(spigot);
}

static void compare_channel(sie_Channel *channel)
{
    char *dump = slurp(qpf("%s/%s/%s", test_output, cur_file,
                           safe_fn(sie_get_name(channel))));
    char *reference = slurp(qpf("%s/%s/%s", test_data, cur_file,
                                safe_fn(sie_get_name(channel))));

    fail_if(compare(dump, reference),
            "Channel dump for %s.sie, channel %d (%s) didn't match expected.",
            cur_file, channel->id, channel->name);

    sie_vec_free(dump);
    sie_vec_free(reference);
}

START_TEST (test_dump_file)
{
    sie_File *file =
        sie_file_open(ctx, qpf("%s/%s.sie", test_data, files[i]));
    sie_Iterator *iterator;
    sie_Channel *channel;
    sie_Tag *tag;
    fail_if(!file, "Couldn't open %s.sie to dump.", files[i]);

    iterator = sie_get_tags(file);
    while ((tag = sie_iterator_next(iterator))) {
        /* KLUDGE - should totally check something here, other than
         * not crashing */
    }
    sie_release(iterator);

    mkdir_p(qpf("%s/%s", test_output, files[i]));
    cur_file = files[i];

    iterator = sie_get_channels(file);
    while ( (channel = sie_iterator_next(iterator)) )
        dump_channel(channel);
    sie_release(iterator);
    iterator = sie_get_channels(file);
    while ( (channel = sie_iterator_next(iterator)) )
        compare_channel(channel);
    sie_release(iterator);

    sie_release(file);
    qpf_free();
}
END_TEST

Suite *functional_suite(void)
{
    Suite *s = suite_create("functional");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);
    tcase_add_checked_fixture(tc_basics, setup_apr, teardown_apr);

    suite_add_tcase(s, tc_basics);
    tcase_add_loop_test(tc_basics, test_spigot_xml, 0, num_files);
    tcase_add_loop_test(tc_basics, test_dump_file, 0, num_files);


    return s;
}

