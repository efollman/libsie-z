#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>

#include "my-check.h"

#include "test-suites.h"

/* The contents of test-suites.h will look something like this:
 *
 * Suite *object_suite(void);
 * 
 * static struct {
 *     char *name;
 *     Suite *(*fn)(void);
 * } suites[] = {
 *     { "object", object_suite },
 * };
 */

int num_suites = sizeof(suites) / sizeof(*suites);

int sie_debug_level = 0;
FILE *sie_debug_stream;

void add_suite(SRunner **sr, Suite *suite)
{
    if (*sr)
        srunner_add_suite(*sr, suite);
    else
        *sr = srunner_create(suite);
}

int main(int argc, char **argv)
{
    int ch, suite, arg, nf;
    int dont_fork = 0;
    char *log_file = NULL;
    char *xml_log_file = NULL;
    SRunner *sr = NULL;
    
    sie_debug_stream = stderr;

#ifndef WIN32
    while ((ch = getopt(argc, argv, "d:o:l:x:f")) != -1) {
        switch (ch) {
        case 'd':
            sie_debug_level = atoi(optarg);
            break;
        case 'o':
            if (!(sie_debug_stream = fopen(optarg, "w")))
                abort();
            break;
        case 'l':
            log_file = optarg;
            break;
        case 'x':
            xml_log_file = optarg;
            break;
        case 'f':
            dont_fork = 1;
        }
    }
    argc -= optind;
    argv += optind;
#else
    (void)ch;
    argc -= 1;
    argv += 1;
#endif

    if (!argc) {
        for (suite = 0; suite < num_suites; suite++) {
            add_suite(&sr, suites[suite].fn());
        }
    }

    for (arg = 0; arg < argc; arg++) {
        for (suite = 0; suite < num_suites; suite++) {
            if (!strcmp(argv[arg], suites[suite].name)) {
                add_suite(&sr, suites[suite].fn());
            }
        }
    }

    if (!sr) {
        printf("Nothing to run!\n\n");
        printf("Usage: test-runner <options> <suite> ...\n");
        printf("If no suites are specified defaults to running all.\n");
        printf("Options:  -d <level> SIE debug level\n");
        printf("          -o <file>  SIE debug output (defaults to stderr)\n");
        printf("          -l <file>  Test log file\n");
        printf("          -x <file>  Test XML log file\n");
        printf("          -f         Don't fork for tests\n");
        printf("Known suites are:\n    ");
        for (suite = 0; suite < num_suites; suite++) {
            printf("%s ", suites[suite].name);
        }
        printf("\n");
        return 1;
    }

    if (dont_fork)
        srunner_set_fork_status(sr, CK_NOFORK);
    if (log_file)
        srunner_set_log(sr, log_file);
    if (xml_log_file)
        srunner_set_xml(sr, xml_log_file);

    srunner_run_all(sr, CK_ENV);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (nf == 0) ? 0 : 1;
}
