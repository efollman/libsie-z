#include "sie_config.h"

#include <math.h>
#include "my-check.h"
#include "test-utils.h"

START_TEST (test_double_merge_corruption)
{
    static const char *expected_raw =
        "<ch id=\"1\" base=\"0\" group=\"5\" name=\"raw_can1_cx23r_ff0027\">\n"
        " <tag id=\"somat:datamode_name\">dm</tag>\n"
        " <tag id=\"core:sample_rate\">0</tag>\n"
        " <tag id=\"somat:input_channel\">raw_can1_cx23r_ff0027</tag>\n"
        " <tag id=\"somat:connector\">@can1.cx23r-ff0027</tag>\n"
        " <tag id=\"core:description\">Raw CAN messages</tag>\n"
        " <tag id=\"data_type\">message_can</tag>\n"
        " <dim index=\"0\">\n"
        "  <tag id=\"core:units\">Seconds</tag>\n"
        "  <tag id=\"core:description\"/>\n"
        "  <tag id=\"core:label\">Time</tag>\n"
        "  <data decoder=\"2\" v=\"0\"/>\n"
        "  <xform scale=\"2.5e-08\" offset=\"0\"/>\n"
        " </dim>\n"
        " <dim index=\"1\">\n"
        "  <tag id=\"core:description\"/>\n"
        "  <tag id=\"core:label\">raw_can1_cx23r_ff0027</tag>\n"
        "  <data decoder=\"2\" v=\"1\"/>\n"
        " </dim>\n"
        "</ch>\n"
        ;
    static const char *expected_expanded =
        "<ch id=\"1\" name=\"raw_can1_cx23r_ff0027\" base=\"0\" group=\"5\">\n"
        " <tag id=\"somat:data_format\">message_can</tag>\n"
        " <tag id=\"somat:version\">1.0</tag>\n"
        " <tag id=\"core:description\">Raw CAN messages</tag>\n"
        " <tag id=\"somat:datamode_type\">message_log</tag>\n"
        " <tag id=\"somat:data_bits\">1</tag>\n"
        " <tag id=\"core:schema\">somat:message</tag>\n"
        " <tag id=\"somat:datamode_name\">dm</tag>\n"
        " <tag id=\"core:sample_rate\">0</tag>\n"
        " <tag id=\"somat:input_channel\">raw_can1_cx23r_ff0027</tag>\n"
        " <tag id=\"somat:connector\">@can1.cx23r-ff0027</tag>\n"
        " <tag id=\"data_type\">message_can</tag>\n"
        " <dim index=\"0\">\n"
        "  <tag id=\"core:units\">Seconds</tag>\n"
        "  <tag id=\"core:description\"/>\n"
        "  <tag id=\"core:label\">Time</tag>\n"
        "  <data decoder=\"2\" v=\"0\"/>\n"
        "  <xform scale=\"2.5e-08\" offset=\"0\"/>\n"
        " </dim>\n"
        " <dim index=\"1\">\n"
        "  <tag id=\"core:description\"/>\n"
        "  <tag id=\"core:label\">raw_can1_cx23r_ff0027</tag>\n"
        "  <data decoder=\"2\" v=\"1\"/>\n"
        " </dim>\n"
        "</ch>\n"
        ;

    sie_File *file = sie_file_open(ctx, "t/data/can_raw_test-v-1-5-0-129-build-1218.sie");
    sie_Channel *chan = sie_get_channel(file, 1);
    char *xml = NULL;

    sie_xml_output(chan->raw_xml, &xml, 0);
    fail_if(strcmp(xml, expected_raw));
    xml[0] = 0; sie_vec_clear(xml);
    fail_if(chan->expanded_xml);

    sie_release(sie_attach_spigot(chan));

    fail_unless(chan->expanded_xml);
    sie_xml_output(chan->raw_xml, &xml, 0);
    fail_if(strcmp(xml, expected_raw));
    xml[0] = 0; sie_vec_clear(xml);
    sie_xml_output(chan->expanded_xml, &xml, 0);
    fail_if(strcmp(xml, expected_expanded));
    xml[0] = 0; sie_vec_clear(xml);

    sie_release(chan);
    chan = sie_get_channel(file, 1);

    sie_xml_output(chan->raw_xml, &xml, 0);
    fail_if(strcmp(xml, expected_raw));
    xml[0] = 0; sie_vec_clear(xml);
    fail_if(chan->expanded_xml);

    sie_release(sie_attach_spigot(chan));

    fail_unless(chan->expanded_xml);
    sie_xml_output(chan->raw_xml, &xml, 0);
    fail_if(strcmp(xml, expected_raw));
    xml[0] = 0; sie_vec_clear(xml);
    sie_xml_output(chan->expanded_xml, &xml, 0);
    fail_if(strcmp(xml, expected_expanded));
    xml[0] = 0; sie_vec_clear(xml);

    sie_vec_free(xml);
    sie_release(chan);
    sie_release(file);
}
END_TEST

Suite *regression_suite(void)
{
    Suite *s = suite_create("regression");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_double_merge_corruption);

    return s;
}
