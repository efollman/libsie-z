#include "sie_config.h"

#include "my-check.h"
#include "sie_exception.h"
#include "test-utils.h"

START_TEST (test_exception)
{
    int catch = 0, finally = 0;
    SIE_TRY(ctx) {
        sie_throw(sie_exception_new(ctx));
        fail("Throw didn't throw.");
    } SIE_CATCH(e) {
        catch = 1;
    } SIE_FINALLY() {
        finally = 1;
    } SIE_END_FINALLY();
    fail_unless(catch, "Catch didn't fire.");
    fail_unless(finally, "Finally didn't fire.");
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

START_TEST (test_exception_2)
{
    volatile int catch1 = 0, catch2 = 0, finally1 = 0, finally2 = 0;
    SIE_TRY(ctx) {
        SIE_TRY(ctx) {
            sie_throw(sie_exception_new(ctx));
            fail("Throw didn't throw.");
        } SIE_CATCH(e) {
            catch1 = 1;
            SIE_RETHROW();
        } SIE_FINALLY() {
            finally1 = 1;
        } SIE_END_FINALLY();
        fail("Rethrow didn't work.");
    } SIE_CATCH(e) {
        catch2 = 1;
    } SIE_FINALLY() {
        finally2 = 1;
    } SIE_END_FINALLY();
    fail_unless(catch1, "Catch 1 didn't fire.");
    fail_unless(finally1, "Finally 1 didn't fire.");
    fail_unless(catch2, "Catch 2 didn't fire.");
    fail_unless(finally2, "Finally 2 didn't fire.");
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

static int finally_sum = 0;
static int end_sum = 0;

void deep(int n)
{
    SIE_UNWIND_PROTECT(ctx) {
        if (!n)
            sie_throw(sie_exception_new(ctx));
        else
            deep(n - 1);
    } SIE_CLEANUP() {
        finally_sum++;
    } SIE_END_CLEANUP();
    end_sum++;
}

START_TEST (test_exception_uwp_deep)
{
    volatile int catch = 0, finally = 0;
    finally_sum = end_sum = 0;
    SIE_TRY(ctx) {
        deep(99);
        fail("Throw didn't throw.");
    } SIE_CATCH(e) {
        catch = 1;
    } SIE_FINALLY() {
        finally = 1;
    } SIE_END_FINALLY();
    fail_unless(finally_sum == 100, NULL);
    fail_unless(end_sum == 0, NULL);
    fail_unless(catch, "Catch didn't fire.");
    fail_unless(finally, "Finally didn't fire.");
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

void thunk(void *foo)
{
    fail_unless(foo == ctx, "Cleanup passing is broken.");
    finally_sum++;
}

void deep2(int n)
{
    int i;
    for (i = n; i > 0; i--)
        sie_cleanup_push(ctx, thunk, ctx);
    sie_throw(sie_exception_new(ctx));
    end_sum++;
}

START_TEST (test_exception_cleanup_deep)
{
    volatile int catch = 0, finally = 0;
    finally_sum = end_sum = 0;
    SIE_TRY(ctx) {
        deep(99);
        fail("Throw didn't throw.");
    } SIE_CATCH(e) {
        catch = 1;
    } SIE_FINALLY() {
        finally = 1;
    } SIE_END_FINALLY();
    fail_unless(finally_sum == 100, NULL);
    fail_unless(end_sum == 0, NULL);
    fail_unless(catch, "Catch didn't fire.");
    fail_unless(finally, "Finally didn't fire.");
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

START_TEST (test_exception_report)
{
    volatile int line;
    SIE_TRY(ctx) {
        line = __LINE__; sie_throw(sie_exception_new(ctx));
        fail("Throw didn't throw.");
    } SIE_CATCH(e) {
        char expected[500];
        char *report = sie_report(e);
        sprintf(expected, "An exception occurred. (at %s:%d)", __FILE__, line);
        fail_unless(!strcmp(report, expected),
                    "Report string was '%s', not expected '%s'.",
                    report, expected);
    } SIE_NO_FINALLY();
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

START_TEST (test_simple_error_report)
{
    volatile int line;
    SIE_TRY(ctx) {
        line = __LINE__; sie_throw(sie_simple_error_new(ctx, "This is a %s of a %s!", "test", "simple error"));
        fail("Throw didn't throw.");
    } SIE_CATCH(e) {
        char expected[500];
        char *report = sie_report(e);
        sprintf(expected, "This is a test of a simple error! (at %s:%d)",
                __FILE__, line);
        fail_unless(!strcmp(report, expected),
                    "Report string was '%s', not expected '%s'.",
                    report, expected);
    } SIE_NO_FINALLY();
    fail_if(ctx->top_handler, "Handler left around.");
}
END_TEST

START_TEST (test_destruction_on_exception)
{
    int destroyed = 0;
    int freed = 0;
    sie_Ctx_Destroy_Check *dc = NULL;
    SIE_TRY(ctx) {
        dc = sie_ctx_destroy_check_new(ctx, &destroyed, &freed, 1, 1);
        fail("Exception not rethrown");
    } SIE_CATCH(e) {
    } SIE_NO_FINALLY();
    fail_unless(!dc, "Object was returned when init failed");
    fail_unless(destroyed == 1, "Object didn't actually destroy");
    fail_unless(freed == 1, "Object didn't actually free");
}
END_TEST

typedef struct _sie_API_Test {
    sie_Context_Object parent;
} sie_API_Test;
SIE_CLASS_DECL(sie_API_Test);
#define SIE_API_TEST(p) SIE_SAFE_CAST(p)

static void sie_api_test_init(sie_API_Test *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

static SIE_CONTEXT_OBJECT_NEW_FN(
    sie_api_test_new, sie_API_Test, self, ctx_obj,
    (void *ctx_obj), sie_api_test_init(self, ctx_obj));

static void _sie_api_test_frob(sie_API_Test *self, int error)
{
    if (error)
        sie_assertf(0, (self, "whoops"));
}

static SIE_VOID_API_METHOD(sie_api_test_frob, self,
                           (void *self, int error), (self, error));

static int _sie_api_test_tweak(sie_API_Test *self, int error)
{
    if (error)
        sie_assertf(0, (self, "whoops"));
    return 42;
}

static SIE_API_METHOD(sie_api_test_tweak, int, -1, self,
                      (void *self, int error), (self, error));

#ifdef _MSC_VER
#pragma warning (disable: 4273) /* inconsistent dll linkage */
#endif
SIE_CLASS(sie_API_Test, sie_Context_Object,
          SIE_MDEF(sie_api_test_frob, _sie_api_test_frob)
          SIE_MDEF(sie_api_test_tweak, _sie_api_test_tweak));

START_TEST (test_api_void_method)
{
    sie_Exception *ex = NULL;
    sie_API_Test *apit = sie_api_test_new(ctx);

    sie_api_test_frob(apit, 0);
    ex = sie_get_exception(apit);
    fail_unless(ex == NULL, "Got an erroneous exception.");

    sie_api_test_frob(apit, 1);
    ex = sie_get_exception(apit);
    fail_if(ex == NULL, "Didn't get an exception.");
    sie_report(ex);
    sie_release(ex);

    sie_api_test_frob(apit, 1);
    sie_api_test_frob(apit, 1);
    sie_api_test_frob(apit, 1);
    ex = sie_get_exception(apit);
    fail_if(ex == NULL, "Didn't get an exception after multiple times.");
    sie_report(ex);
    sie_release(ex);

    sie_release(apit);
}
END_TEST

START_TEST (test_api_method)
{
    sie_Exception *ex = NULL;
    sie_API_Test *apit = sie_api_test_new(ctx);

    fail_unless(sie_api_test_tweak(apit, 0) == 42, "Normal value wrong.");
    ex = sie_get_exception(apit);
    fail_unless(ex == NULL, "Got an erroneous exception.");

    fail_unless(sie_api_test_tweak(apit, 1) == -1, "Error value wrong.");
    ex = sie_get_exception(apit);
    fail_if(ex == NULL, "Didn't get an exception.");
    sie_report(ex);
    sie_release(ex);

    fail_unless(sie_api_test_tweak(apit, 1) == -1, "Error value wrong.");
    fail_unless(sie_api_test_tweak(apit, 1) == -1, "Error value wrong.");
    fail_unless(sie_api_test_tweak(apit, 1) == -1, "Error value wrong.");
    ex = sie_get_exception(apit);
    fail_if(ex == NULL, "Didn't get an exception after multiple times.");
    sie_report(ex);
    sie_release(ex);

    sie_release(apit);
}
END_TEST

Suite *exception_suite(void)
{
    Suite *s = suite_create("exception");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_exception);
    tcase_add_test(tc_basics, test_exception_2);
    tcase_add_test(tc_basics, test_exception_uwp_deep);
    tcase_add_test(tc_basics, test_exception_cleanup_deep);
    tcase_add_test(tc_basics, test_exception_report);
    tcase_add_test(tc_basics, test_simple_error_report);
    tcase_add_test(tc_basics, test_destruction_on_exception);

    tcase_add_test(tc_basics, test_api_void_method);
    tcase_add_test(tc_basics, test_api_method);

    return s;
}

