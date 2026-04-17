#include "sie_config.h"

#include "my-check.h"
#include "sie_xml.h"
#include "test-utils.h"
#include "sie_vec.h"

START_TEST (test_node_attrs)
{
    sie_XML *node = sie_xml_new_element(ctx, "test");
    const char *value;
    size_t value_size;
    sie_xml_set_attribute(node, "name", "value");
    fail_if(strcmp(sie_xml_get_attribute(node, "name"), "value"));
    sie_xml_set_attribute(node, "name2", "value2");
    fail_if(strcmp(sie_xml_get_attribute(node, "name2"), "value2"));
    sie_xml_set_attribute_b(node, "bin\0ary", 7, "val\0ue", 6);
    fail_unless(sie_xml_get_attribute_b(node, "bin\0ary", 7,
                                        &value, &value_size));
    fail_unless(value_size == 6);
    fail_if(memcmp("val\0ue", value, 6));
    fail_if(strcmp(sie_xml_get_attribute(node, "name2"), "value2"));
    sie_xml_set_attribute(node, "name", "value3");
    fail_if(strcmp(sie_xml_get_attribute(node, "name"), "value3"));
    sie_release(node);
}
END_TEST

START_TEST (test_node_link1)
{
    sie_XML *node[4];
    size_t i;
    
    for (i = 0; i < sizeof(node) / sizeof(*node); i++)
        node[i] = sie_xml_new_element(ctx, "test");
    
    sie_xml_link(node[1], node[0], SIE_XML_LINK_AFTER, NULL);
    sie_release(node[1]);
    sie_xml_link(node[2], node[0], SIE_XML_LINK_AFTER, NULL);
    sie_release(node[2]);
    sie_xml_link(node[3], node[0], SIE_XML_LINK_BEFORE, NULL);
    sie_release(node[3]);
    
    fail_unless(node[0]->child == node[3]);
    fail_unless(node[0]->last_child == node[2]);
    fail_unless(node[3]->prev == NULL);
    fail_unless(node[3]->next == node[1]);
    fail_unless(node[1]->prev == node[3]);
    fail_unless(node[1]->next == node[2]);
    fail_unless(node[2]->prev == node[1]);
    fail_unless(node[2]->next == NULL);
    
    sie_xml_unlink(node[1]);
    
    fail_unless(node[3]->next == node[2]);
    fail_unless(node[2]->prev == node[3]);
    
    sie_xml_unlink(node[2]);
    
    fail_unless(node[0]->child == node[3]);
    fail_unless(node[0]->last_child == node[3]);
    fail_unless(node[3]->next == NULL);
    fail_unless(node[3]->prev == NULL);
    
    sie_xml_unlink(node[3]);
    
    fail_unless(node[0]->child == NULL);
    fail_unless(node[0]->last_child == NULL);
    
    sie_release(node[0]);
}
END_TEST

START_TEST (test_node_link2)
{
    sie_XML *node[5];
    size_t i;
    
    for (i = 0; i < sizeof(node) / sizeof(*node); i++) {
        node[i] = sie_xml_new(ctx);
        node[i]->type = SIE_XML_ELEMENT;
    }
    
    sie_xml_link(node[1], node[0], SIE_XML_LINK_BEFORE, NULL);
    sie_release(node[1]);
    sie_xml_link(node[4], node[0], SIE_XML_LINK_AFTER, NULL);
    sie_release(node[4]);
    sie_xml_link(node[2], node[0], SIE_XML_LINK_AFTER, node[1]);
    sie_release(node[2]);
    sie_xml_link(node[3], node[0], SIE_XML_LINK_BEFORE, node[4]);
    sie_release(node[3]);
    
    fail_unless(node[0]->child == node[1]);
    fail_unless(node[0]->last_child == node[4]);
    fail_unless(node[1]->prev == NULL);
    fail_unless(node[1]->next == node[2]);
    fail_unless(node[2]->prev == node[1]);
    fail_unless(node[2]->next == node[3]);
    fail_unless(node[3]->prev == node[2]);
    fail_unless(node[3]->next == node[4]);
    fail_unless(node[4]->prev == node[3]);
    fail_unless(node[4]->next == NULL);
    
    sie_release(node[0]);
}
END_TEST

static sie_XML **nodes;

static void check(sie_XML *node, sie_XML *parent, int level, void *data)
{
    fail_unless(node->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(node->value.element.name), "sie"));
    fail_unless(level == 0);
    fail_if(node->parent);
}

static int store(sie_XML *node, sie_XML *parent, int level, void *data)
{
    sie_vec_push_back(nodes, sie_retain(node));
    return 1;
}

static int always(sie_XML *node, sie_XML *parent, int level, void *data)
{
    return 1;
}

static void smoke_tests_a(size_t which)
{
    fail_unless(nodes[0]->type == SIE_XML_TEXT);
    fail_unless(!strcmp(sie_sv(nodes[0]->value.text.text),
                        "Hello, <>&'\" world!"));
    fail_unless(nodes[0]->parent == nodes[1]);
    
    fail_unless(nodes[1]->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(nodes[1]->value.element.name), "sie"));
    fail_unless(!strcmp(sie_xml_get_attribute(nodes[1], "test"),
                        "1\""
                        "\xc4\xa3"
                        "\xe1\x88\xb4"
                        "\xf0\x92\x8d\x85"
                        "\"2"));
    fail_unless(!strcmp(sie_xml_get_attribute(nodes[1], "foo"), "ph'oo"));
    fail_unless(nodes[1]->child == nodes[0]);
    fail_unless(nodes[1]->last_child == nodes[0]);
    
    sie_release(nodes[which]);
    if (which) {
        fail_unless(nodes[0]->parent == NULL);
    } else {
        /* node[0] still held by node[1] */
        fail_if(nodes[1]->child == NULL);
        fail_if(nodes[1]->last_child == NULL);
    }
    sie_release(nodes[!which]);
    sie_vec_clear(nodes);
}

START_TEST (test_parse1)
{
    size_t i;
    char *data =
        "<sie test='1\"&#x123;&#x1234;&#x12345;\"2' foo=\"ph'o&#111;\">"
        "Hello, &lt;&gt;&amp;&apos;&quot; world!"
        "</sie>";
    sie_XML *c1;
    sie_XML_Incremental_Parser *ip =
        sie_xml_incremental_parser_new(ctx, check, NULL, store, NULL);
    sie_xml_incremental_parser_parse(ip, data, strlen(data));
    smoke_tests_a(1);
    sie_release(ip);
    
    /* Read in two chunks, try all possibilities.
       Also test releasing the two nodes in both orders. */
    for (i = 0; i < strlen(data) * 2; i++) {
        size_t index = i / 2;
        size_t which = i % 2;
        ip = sie_xml_incremental_parser_new(ctx, check, NULL, store, NULL);
        sie_xml_incremental_parser_parse(ip, data, index);
        sie_xml_incremental_parser_parse(ip, data + index,
                                         strlen(data) - index);
        smoke_tests_a(which);
        sie_release(ip);
    }
    
    /* By character */
    ip = sie_xml_incremental_parser_new(ctx, check, NULL, store, NULL);
    for (i = 0; i < strlen(data); i++)
        sie_xml_incremental_parser_parse(ip, data + i, 1);
    c1 = sie_copy(nodes[1]);
    smoke_tests_a(1);
    sie_release(ip);
    nodes[1] = c1;
    nodes[0] = sie_retain(c1->child);
    smoke_tests_a(1);
    sie_vec_free(nodes);
    nodes = 0;
}
END_TEST

START_TEST (test_parse2)
{
    char *data =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>  "
        "<test>"
        "Some text.  \nAnd some more.\n"
        "<!-- A comment. -->"
        "<an_empty_tag/>"
        "<another_empty_tag with='attribute' />"
        "<a_full_tag >with contents</a_full_tag >"
        "</test>  ";
    sie_XML_Parser *p = sie_xml_parser_new(ctx);
    sie_XML *document, *d;
    sie_xml_parser_parse(p, data, strlen(data));
    document = sie_xml_parser_get_document(p);
    
    fail_unless(document != NULL);
    d = document;
    fail_unless(d->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(d->value.element.name), "test"));
    d = d->child;
    fail_unless(d->type == SIE_XML_TEXT);
    fail_unless(!strcmp(sie_sv(d->value.text.text),
                        "Some text.  \nAnd some more.\n"));
    d = d->next;
    fail_unless(d->type == SIE_XML_COMMENT);
    fail_unless(!strcmp(sie_sv(d->value.text.text), "!-- A comment. --"));
    d = d->next;
    fail_unless(d->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(d->value.element.name), "an_empty_tag"));
    fail_unless(d->child == NULL);
    d = d->next;
    fail_unless(d->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(d->value.element.name), "another_empty_tag"));
    fail_unless(d->child == NULL);
    fail_unless(!strcmp(sie_xml_get_attribute(d, "with"), "attribute"));
    d = d->next;
    fail_unless(d->type == SIE_XML_ELEMENT);
    fail_unless(!strcmp(sie_sv(d->value.element.name), "a_full_tag"));
    fail_if(d->child == NULL);
    fail_unless(d->child->type == SIE_XML_TEXT);
    fail_unless(!strcmp(sie_sv(d->child->value.text.text), "with contents"));
    fail_unless(d->next == NULL);

    sie_release(document);
    sie_release(p);
}
END_TEST

START_TEST (test_cleanup)
{
    char *data =
        "<?xml <foobar>?>"
        "more <text /> here"
        "<sie attr='1' attr2='3'>"
        "s<ome>even more</ome> text <here>"
        "<!-- and a comment -->text";
    sie_XML_Incremental_Parser *ip =
        sie_xml_incremental_parser_new(ctx, NULL, NULL, always, NULL);
    int i;
    for (i = 0; i < 2000; i++)
        sie_xml_incremental_parser_parse(ip, data, strlen(data));
    sie_release(ip);
}
END_TEST

START_TEST (test_output)
{
    char *data =
        "<test node='f&amp;oo\"' \n><bar></bar>"
        "<txt>this is some \n<text/>\nreally\n</txt>"
        "</test>";
    char *expected_noindent =
        "<test node=\"f&amp;oo&quot;\"><bar/>"
        "<txt>this is some \n<text/>\nreally\n</txt>"
        "</test>";
    char *expected_indent2 =
        "  <test node=\"f&amp;oo&quot;\">\n   <bar/>\n"
        "   <txt>this is some \n<text/>\nreally\n</txt>\n"
        "  </test>\n";
    sie_XML_Parser *p = sie_xml_parser_new(ctx);
    sie_XML *document;
    char *output = NULL;
    sie_xml_parser_parse(p, data, strlen(data));
    document = sie_xml_parser_get_document(p);
    fail_unless(document);

    sie_xml_output(document, &output, -1);
    if (strcmp(output, expected_noindent)) {
        fprintf(stderr, "expected:\n%s\ngot:\n%s\n",
                expected_noindent, output);
        fail("output mismatch");
    }

    sie_vec_clear(output);
    sie_xml_output(document, &output, 2);
    if (strcmp(output, expected_indent2)) {
        fprintf(stderr, "expected:\n%s\ngot:\n%s\n",
                expected_indent2, output);
        fail("output mismatch");
    }

    sie_vec_free(output);
    sie_release(document);
    sie_release(p);
}
END_TEST

Suite *xml_suite(void)
{
    Suite *s = suite_create("xml");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);
    
    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_node_attrs);
    tcase_add_test(tc_basics, test_node_link1);
    tcase_add_test(tc_basics, test_node_link2);
    tcase_add_test(tc_basics, test_parse1);
    tcase_add_test(tc_basics, test_parse2);
    tcase_add_test(tc_basics, test_cleanup);
    tcase_add_test(tc_basics, test_output);
    
    return s;
}

