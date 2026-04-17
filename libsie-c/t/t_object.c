#include "sie_config.h"

#include "my-check.h"
#include "sie_object.h"

typedef struct _sie_Destroy_Check {
    sie_Object parent;
    int *target;
    int *free_target;
    int value;
} sie_Destroy_Check;
SIE_CLASS_DECL(sie_Destroy_Check);
#define SIE_DESTROY_CHECK(p) SIE_SAFE_CAST(p, sie_Destroy_Check)

static void sie_destroy_check_destroy(sie_Destroy_Check *self)
{
    *self->target = self->value;
    sie_object_destroy(SIE_OBJECT(self));
}

static void sie_destroy_check_free_object(sie_Destroy_Check *self)
{
    *self->free_target = self->value;
    sie_base_object_free_object(SIE_BASE_OBJECT(self));
}

static void *sie_destroy_check_init(sie_Destroy_Check *self,
                                    int *target, int *free_target,
                                    int value, int fail)
{
    void *result = sie_object_init(SIE_OBJECT(self));
    self->target = target;
    self->free_target = free_target;
    self->value = value;
    return fail ? NULL : result;
}

SIE_OBJECT_NEW_FN(sie_destroy_check_new, sie_Destroy_Check, self,
                  (int *target, int *free_target, int value, int fail),
                  sie_destroy_check_init(self, target, free_target,
                                         value, fail));

#ifdef _MSC_VER
#pragma warning (disable: 4273) /* inconsistent dll linkage */
#endif
SIE_CLASS(sie_Destroy_Check, sie_Object,
          SIE_MDEF(sie_destroy, sie_destroy_check_destroy)
          SIE_MDEF(sie_free_object, sie_destroy_check_free_object));

START_TEST (test_destroy)
{
    int destroyed = 0;
    int freed = 0;
    sie_Destroy_Check *dc = sie_destroy_check_new(&destroyed, &freed, 1, 0);
    fail_if(!dc, "Object allocation failed");
    fail_if(destroyed == 1 || freed == 1, NULL);
    sie_retain(dc);
    fail_if(destroyed == 1 || freed == 1, NULL);
    sie_release(dc);
    fail_if(destroyed == 1 || freed == 1, NULL);
    sie_release(dc);
    fail_unless(destroyed == 1, "Object didn't destroy");
    fail_unless(freed == 1, "Object didn't free");
}
END_TEST

START_TEST (test_weak_ref)
{
    int destroyed = 0;
    int freed = 0;
    sie_Destroy_Check *dc = sie_destroy_check_new(&destroyed, &freed, 1, 0);
    sie_Weak_Ref *w = sie_get_weak_ref(dc);
    fail_if(!w, "Weak ref allocation failed");
    fail_if(!sie_weak_deref(w), NULL);
    sie_release(dc);
    fail_unless(!sie_weak_deref(w),
                "Weak reference didn't show object deletion.");
    sie_release(w);
}
END_TEST

START_TEST (test_failed_init)
{
    int destroyed = 0;
    int freed = 0;
    sie_Destroy_Check *dc = sie_destroy_check_new(&destroyed, &freed, 1, 1);
    fail_unless(!dc, "Object was returned when init failed");
    fail_unless(destroyed == 1, "Object didn't actually destroy");
    fail_unless(freed == 1, "Object didn't actually free");
}
END_TEST

START_TEST (test_null_args)
{
    void *foo;
    (void)foo;
    foo = sie_retain(NULL);
    sie_release(NULL);
    sie_weak_deref(NULL);
}
END_TEST

Suite *object_suite(void)
{
    Suite *s = suite_create("object");
    TCase *tc_basics = tcase_create("basics");

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_destroy);
    tcase_add_test(tc_basics, test_weak_ref);
    tcase_add_test(tc_basics, test_failed_init);
    tcase_add_test(tc_basics, test_null_args);

    return s;
}

