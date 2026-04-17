#include "sie_config.h"

#include "my-check.h"
#include "test-utils.h"

static char *in_f;
static char *out_f;
static char *frob_f;
static int do_kill_crcs;

START_TEST (frobit)
{
    char *victim = slurp(in_f);
    FILE *frob = fopen(frob_f, "rb");
    fail_if(!victim);
    fail_if(!frob);
    
    if (do_kill_crcs)
        kill_crcs(in_f, victim);
    
    while (maybe_frob_victim(frob, &victim))
        /* nothing */;
    
    fclose(frob);
    spew(victim, out_f);
}
END_TEST

void add_suite(SRunner **sr, Suite *suite)
{
    if (*sr)
        srunner_add_suite(*sr, suite);
    else
        *sr = srunner_create(suite);
}

int main(int argc, char **argv)
{
    if (argc <= 4) {
        printf("usage: frob-file in out frob do_kill_crcs\n");
        return 1;
    } else {
        int nf;
        SRunner *sr = NULL;
        Suite *s = suite_create("frob-file");
        TCase *tc = tcase_create("tc");
        tcase_add_checked_fixture(tc, setup_ctx, check_ctx);
        suite_add_tcase(s, tc);
        tcase_add_test(tc, frobit);
        sr = srunner_create(s);
        srunner_set_fork_status(sr, CK_NOFORK);

        in_f = argv[1];
        out_f = argv[2];
        frob_f = argv[3];
        do_kill_crcs = atoi(argv[4]);
        
        srunner_run_all(sr, CK_ENV);
        nf = srunner_ntests_failed(sr);
        srunner_free(sr);
        
        return (nf == 0) ? 0 : 1;
    }
}
