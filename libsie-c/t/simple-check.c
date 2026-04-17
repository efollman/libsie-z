#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple-check.h"

static char *current_suite;
static char *current_tcase;
char *current_test;
int current_loop;

SRunner *srunner_create(Suite *suite)
{
    SRunner *sr = calloc(1, sizeof(*sr));
    sr->suites = suite;
    return sr;
}

void srunner_add_suite(SRunner *sr, Suite *suite)
{
    suite->next = sr->suites;
    sr->suites = suite;
}

void srunner_run_all(SRunner *sr, int opts)
{
    int count = 0;
    Suite *suite = sr->suites;
    for ( ; suite != NULL; suite = suite->next) {
        TCase *tcase = suite->tcases;
        current_suite = suite->name;
        for ( ; tcase != NULL; tcase = tcase->next) {
            Test *test = tcase->tests;
            current_tcase = tcase->name;
            for ( ; test != NULL; test = test->next) {
                Setup *node;
                int i;
                current_test = "__setup__";
                for (node = tcase->setup; node != NULL; node = node->next)
                    node->fn();
                for (i = test->start; i < test->end; i++) {
                    current_loop = i;
                    test->fn(i);
                    fprintf(stderr, "Pass: %s:%s:%s:%d\n",
                            current_suite, current_tcase, current_test, i);
                    count++;
                }
                current_test = "__teardown__";
                for (node = tcase->teardown; node != NULL; node = node->next)
                    node->fn();
            }
        }
    }
    fprintf(stderr, "All %d tests pass!\n", count);
}

void srunner_free(SRunner *sr)
{
    Suite *suite = sr->suites, *nsuite;
    for ( ; suite != NULL; suite = nsuite) {
        TCase *tcase = suite->tcases, *ntcase;
        for ( ; tcase != NULL; tcase = ntcase) {
            Test *test = tcase->tests, *ntest;
            Setup *node, *nnode;
            for (node = tcase->setup; node != NULL; node = nnode) {
                nnode = node->next;
                free(node);
            }
            for (node = tcase->teardown; node != NULL; node = nnode) {
                nnode = node->next;
                free(node);
            }
            for ( ; test != NULL; test = ntest) {
                ntest = test->next;
                free(test);
            }
            ntcase = tcase->next;
            free(tcase->name);
            free(tcase);
        }
        nsuite = suite->next;
        free(suite->name);
        free(suite);
    }
    free(sr);
}

Suite *suite_create(char *name)
{
    Suite *suite = calloc(1, sizeof(*suite));
    suite->name = strdup(name);
    return suite;
}

void suite_add_tcase(Suite *suite, TCase *tcase)
{
    tcase->next = suite->tcases;
    suite->tcases = tcase;
}

TCase *tcase_create(char *name)
{
    TCase *tcase = calloc(1, sizeof(*tcase));
    tcase->name = strdup(name);
    return tcase;
}

void tcase_add_checked_fixture(TCase *tcase,
                               void (*setup)(void),
                               void (*teardown)(void))
{
    if (setup) {
        Setup *node = calloc(1, sizeof(*node));
        node->fn = setup;
        node->next = tcase->setup;
        tcase->setup = node;
    }

    if (teardown) {
        Setup *node = calloc(1, sizeof(*node));
        node->fn = teardown;
        if (tcase->teardown_end)
            tcase->teardown_end->next = node;
        else
            tcase->teardown = node;
        tcase->teardown_end = node;
    }
}

void tcase_add_loop_test(TCase *tcase, void (*fn)(int), int start, int end)
{
    Test *test = calloc(1, sizeof(*test));
    test->fn = fn;
    test->start = start;
    test->end = end;
    test->next = tcase->tests;
    tcase->tests = test;
}

void tcase_add_test(TCase *tcase, void (*fn)(int))
{
    tcase_add_loop_test(tcase, fn, 0, 1);
}

void fail(char *f, ...)
{
    fprintf(stderr, "Failed: %s:%s:%s:%d\n",
            current_suite, current_tcase, current_test, current_loop);
    abort();
}

#undef fail_if
void fail_if(int test, ...)
{
    if (test) fail("");
}

#undef fail_unless
void fail_unless(int test, ...)
{
    if (!test) fail("");
}

void nothing(void)
{
}
