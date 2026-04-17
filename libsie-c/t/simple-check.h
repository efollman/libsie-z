#ifndef SIMPLE_CHECK_H
#define SIMPLE_CHECK_H

extern char *current_test;

typedef struct _SRunner SRunner;
typedef struct _Suite Suite;
typedef struct _TCase TCase;
typedef struct _Setup Setup;
typedef struct _Test Test;

struct _SRunner {
    Suite *suites;
};

struct _Suite {
    char *name;
    TCase *tcases;
    Suite *next;
};

struct _TCase {
    char *name;
    Setup *setup;
    Setup *teardown;
    Setup *teardown_end;
    Test *tests;
    TCase *next;
};

struct _Setup {
    void (*fn)(void);
    Setup *next;
};

struct _Test {
    void (*fn)(int);
    Test *next;
    int start;
    int end;
};

#define START_TEST(name) void name(int i) { current_test = #name;
#define END_TEST }

#define CK_FORK 0
#define CK_NOFORK 0
#define CK_ENV 0

#define srunner_set_fork_status(a, b) nothing()
#define srunner_set_log(a, b) nothing()
#define srunner_set_xml(a, b) nothing()
#define srunner_ntests_failed(a) 0

SRunner *srunner_create(Suite *suite);
void srunner_add_suite(SRunner *sr, Suite *suite);
void srunner_run_all(SRunner *sr, int opts);
void srunner_free(SRunner *sr);

Suite *suite_create(char *name);
void suite_add_tcase(Suite *suite, TCase *tcase);

TCase *tcase_create(char *name);
void tcase_add_checked_fixture(TCase *tcase,
                               void (*setup)(void),
                               void (*teardown)(void));
void tcase_add_loop_test(TCase *tcase, void (*fn)(int), int start, int end);
void tcase_add_test(TCase *tcase, void (*fn)(int));

void fail(char *f, ...);
void fail_unless(int test, ...);
void fail_if(int test, ...);
#if !defined(_MSC_VER) || _MSC_VER >= 1400
#define fail_unless(test, ...) fail_unless(!!(test), ##__VA_ARGS__)
#define fail_if(test, ...) fail_if(!!(test), ##__VA_ARGS__)
#endif

void nothing(void);

#endif
