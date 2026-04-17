#include "sie_config.h"

#include "my-check.h"
#include "sie_xml.h"
#include "test-utils.h"
#include "sie_vec.h"
#include "sie_xml_merge.h"

START_TEST (test_merge1)
{
    char *data =
        "<sie>"
        "<ch id=\"1\" private=\"1\">"
        "<tag id=\"foo\">bar</tag>"
        "<tag id=\"bar\">bar-value</tag>"
        "</ch>"
        "<ch id=\"1\">"
        "<tag id=\"foo\">foo-value</tag>"
        "</ch>"
        "<ch id=\"2\" base=\"1\">"
        "<tag id=\"foo\">new-foo-value</tag>"
        "</ch>"
        "<ch id=\"3\" base=\"1\" private=\"1\">"
        "<tag id=\"foo\">new-foo-value-2</tag>"
        "</ch>"
        "<tag ch=\"1\" id=\"direct\"/>"
        "<tag test=\"0\" ch=\"4\" dim=\"0\" id=\"direct-4\"/>"
        "</sie>";
    char *merged =
        "<sie>"
        "<ch id=\"1\" private=\"1\">"
        "<tag id=\"foo\">foo-value</tag>"
        "<tag id=\"bar\">bar-value</tag>"
        "<tag id=\"direct\"/>"
        "</ch>"
        "<ch id=\"2\" base=\"1\">"
        "<tag id=\"foo\">new-foo-value</tag>"
        "</ch>"
        "<ch id=\"3\" base=\"1\" private=\"1\">"
        "<tag id=\"foo\">new-foo-value-2</tag>"
        "</ch>"
        "<test id=\"0\">"
        "<ch id=\"4\">"
        "<dim index=\"0\">"
        "<tag id=\"direct-4\"/>"
        "</dim>"
        "</ch>"
        "</test>"
        "</sie>";
    char *expansion2 =
        "<ch id=\"2\" base=\"1\">"
        "<tag id=\"foo\">new-foo-value</tag>"
        "<tag id=\"bar\">bar-value</tag>"
        "<tag id=\"direct\"/>"
        "</ch>";
    char *expansion3 =
        "<ch id=\"3\" base=\"1\" private=\"1\">"
        "<tag id=\"foo\">new-foo-value-2</tag>"
        "<tag id=\"bar\">bar-value</tag>"
        "<tag id=\"direct\"/>"
        "</ch>";
    sie_XML_Definition *xd = sie_xml_definition_new(ctx);
    sie_XML *node;
    char *output = NULL;

    sie_xml_definition_add_string(xd, data, strlen(data));

    fail_unless(xd != NULL);
    fail_unless(xd->sie_node != NULL);
    
    sie_xml_output(xd->sie_node, &output, -1);
    fail_if(strcmp(output, merged));
    sie_vec_clear(output);

    node = sie_xml_expand(xd, sie_literal(ctx, ch), 2);
    sie_xml_output(node, &output, -1);
    fail_if(strcmp(output, expansion2));
    sie_release(node);
    sie_vec_free(output);
    output = NULL;

    node = sie_xml_expand(xd, sie_literal(ctx, ch), 3);
    sie_xml_output(node, &output, -1);
    fail_if(strcmp(output, expansion3));
    sie_release(node);
    sie_vec_free(output);

    sie_release(xd);
}
END_TEST

Suite *xml_merge_suite(void)
{
    Suite *s = suite_create("xml_merge");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);
    
    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_merge1);
    
    return s;
}

