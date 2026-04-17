#include "sie_config.h"

#include "my-check.h"
#include "test-utils.h"

static const char *files[] = {
    "simple",
    "th16-little",
    "th16-big",
    "fibonacci",
    "big-expr-1",
    "big-expr-2",
    "old-message",
    "new-message",
    "bit-decoder-1",
    "bit-decoder-2",
    "read-no-var",
    "read-assert",
    "seek",
};
static const int num_files = sizeof(files) / sizeof(*files);


static char *get_line(char *line, size_t size, FILE *f)
{
    do {
        if (!fgets(line, (int)size, f))
            return NULL;
        if (!strcmp(line, "\n"))
            return NULL;
    } while (line[0] == '#');
    return line;
}

static sie_Decoder *parse_decoder(const char *filename, FILE *f)
{
    char line[2000];
    char *xml_string = NULL;
    sie_XML *xml;
    sie_Decoder *decoder;
    
    while (get_line(line, sizeof(line), f)) {
        sie_vec_strcatf(0, &xml_string, "%s", line);
    }
    xml = sie_xml_parse_string(ctx, xml_string);
    fail_unless(xml, "xml didn't parse for %s", filename);

    decoder = sie_decoder_new(ctx, xml);
    sie_release(xml);
    sie_vec_free(xml_string);

    return decoder;
}

void parse_number(const char *num,
                  sie_float64 *float64,
                  char **raw,
                  int *is_raw)
{
    if (num[0] == 'x') {
        size_t i;
        *is_raw = 1;
        fail_unless(strlen(num) % 2, "odd number of hex digits in %s", num);
        for (i = 1; i < strlen(num); i += 2) {
            char hexbyte[3];
            char *endptr;
            long byte;
            strncpy(hexbyte, &num[i], 2);
            hexbyte[2] = 0;
            byte = strtol(hexbyte, &endptr, 16);
            fail_if(endptr == hexbyte || *endptr != 0);
            sie_vec_push_back(*raw, (char)byte);
        }
    } else if (num[0] == '0' && num[1] == 'x') {
        char *endptr;
        *float64 = strtoul(num, &endptr, 0);
        fail_if(endptr == num || *endptr != 0,
                "failed to read '%s' as hex", num);
    } else {
        char *endptr;
        *float64 = strtod(num, &endptr);
        fail_if(endptr == num || *endptr != 0,
                "failed to read '%s' as float", num);
    }
}

void parse_decoder_testcase(const char *filename,
                            int testcase,
                            FILE *f,
                            char **data_vec,
                            sie_Output **expected)
{
    char *startptr;
    char *endptr;
    char line[2000];
    char num[2000];
    long datapoint;
    size_t num_dims;
    size_t dim = 0;
    size_t row = 0;

    while (get_line(line, sizeof(line), f)) {
        startptr = line;
        while ((datapoint = strtol(startptr, &endptr, 16)),
               startptr != endptr) {
            fail_unless(datapoint >= 0 && datapoint <= 255,
                        "%ld is not a byte in '%s' case %d",
                        datapoint, filename, testcase);
            sie_vec_push_back(*data_vec, (char)datapoint);
            startptr = endptr;
        }
    }
    if (feof(f))
        fail("nothing after data segment in '%s' case %d\n",
             filename, testcase);

    get_line(line, sizeof(line), f);
    if (!strncmp(line, "fail", 4)) {
        /* failure expected */
        *expected = NULL;
        get_line(line, sizeof(line), f); /* consume empty line */
        return;
    }
    fail_unless(sscanf(line, "%"APR_SIZE_T_FMT, &num_dims) == 1);
    fail_unless(num_dims >= 0);
    *expected = sie_output_new(ctx, num_dims);

    while (get_line(line, sizeof(line), f)) {
        int consumed;
        startptr = line;
        while (sscanf(startptr, " %s %n", num, &consumed) >= 1) {
            sie_float64 float64;
            char *raw = NULL;
            int is_raw = 0;
            sie_Output_V *vv = &(*expected)->v[dim];
            sie_Output_V_Guts *vg = &(*expected)->v_guts[dim];
            
            startptr += consumed;
            parse_number(num, &float64, &raw, &is_raw);
            if (row == 0) {
                sie_output_set_type(*expected, dim,
                                    (is_raw ?
                                     SIE_OUTPUT_RAW :
                                     SIE_OUTPUT_FLOAT64));
            }
            if (vg->size >= vg->max_size)
                sie_output_grow(*expected, dim);

            if (is_raw) {
                fail_unless(vv->type == SIE_OUTPUT_RAW);
                sie_output_set_raw(*expected, dim, (int)row,
                                   raw, sie_vec_size(raw));
            } else {
                fail_unless(vv->type == SIE_OUTPUT_FLOAT64);
                vv->float64[row] = float64;
            }
            ++vg->size;

            ++dim;
            if (dim >= num_dims) {
                ++row;
                ++(*expected)->num_scans;
                dim = 0;
            }
            sie_vec_free(raw);
        }
    }
}

static void output_dump(sie_Output *output, FILE *file)
{
    if (!output)
        fprintf(file, "failure\n");
    else
        sie_output_dump(output, file);
}

static int output_compare(sie_Output *output, sie_Output *expected,
                          int failure)
{
    if (expected == NULL) {
        return failure;
    } else {
        return sie_output_compare(output, expected);
    }
}

START_TEST (test_decoder)
{
    char *data;
    sie_Decoder *decoder;
    sie_Output *expected;
    char *filename = qpf("t/data/decoders/%s", files[i]);
    FILE *f = fopen(filename, "r");
    int testcase = 0;
    sie_Decoder_Machine *machine;
    int volatile failure = 0;
    fail_unless(f, "couldn't open '%s'", filename);
    decoder = parse_decoder(filename, f);
    machine = sie_decoder_machine_new(decoder);
    do {
        data = NULL;
        parse_decoder_testcase(filename, testcase, f, &data, &expected);
        sie_decoder_machine_prep(machine, data, sie_vec_size(data));
        SIE_TRY(ctx) {
            sie_decoder_machine_run(machine);
        } SIE_CATCH(ex) {
            failure = 1;
        } SIE_NO_FINALLY();
        if (!output_compare(machine->output, expected, failure)) {
            printf("Decoder testcase %s:%d failed, wanted:\n",
                   filename, testcase);
            output_dump(expected, stdout);
            printf("got:\n");
            output_dump(machine->output, stdout);
            fail("comparison failed on %s:%d", filename, testcase);
        }
        sie_vec_free(data);
        sie_release(expected);
        ++testcase;
    } while (!feof(f));
    sie_release(machine);
    sie_release(decoder);
    fclose(f);
    qpf_free();
}
END_TEST

START_TEST (test_equal_signature)
{
    sie_XML *x1 = sie_xml_parse_string(
        ctx,
        "<decoder>"
        " <read var='v0' bits='64' type='uint' endian='little'/>"
        " <loop var='v0'>"
        "  <set var='foo' value='{$v0}'/>"
        "  <read var='v1' bits='32' type='float' endian='little'/>"
        "  <sample/>"
        " </loop>"
        "</decoder>");
    sie_XML *x2 = sie_xml_parse_string(
        ctx,
        "<decoder>"
        " <read var='v0' bits='64' type='uint' endian='little'/>"
        " <loop var='v0'>"
        "  <set var='bar' value='{$v0}'/>"
        "  <read var='v1' bits='32' type='float' endian='little'/>"
        "  <sample/>"
        " </loop>"
        "</decoder>");
    sie_XML *x3 = sie_xml_parse_string(
        ctx,
        "<decoder>"
        " <read var='v0' bits='64' type='uint' endian='little'/>"
        " <loop var='v0'>"
        "  <set var='foo' value='{$v1}'/>"
        "  <read var='v1' bits='32' type='float' endian='little'/>"
        "  <sample/>"
        " </loop>"
        "</decoder>");

    sie_Decoder *d1, *d2, *d3;

    fail_if(!x1);
    fail_if(!x2);
    fail_if(!x3);

    d1 = sie_decoder_new(ctx, x1);
    d2 = sie_decoder_new(ctx, x2);
    d3 = sie_decoder_new(ctx, x3);

    fail_if(!d1);
    fail_if(!d2);
    fail_if(!d3);

    fail_if(sie_decoder_signature(d1) != sie_decoder_signature(d2));
    fail_if(sie_decoder_signature(d1) == sie_decoder_signature(d3));

    fail_if(!sie_decoder_is_equal(d1, d2));
    fail_if(sie_decoder_is_equal(d1, d3));
    
    sie_release(x1);
    sie_release(x2);
    sie_release(x3);
    sie_release(d1);
    sie_release(d2);
    sie_release(d3);
}
END_TEST

Suite *decoder_suite(void)
{
    Suite *s = suite_create("decoder");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_loop_test(tc_basics, test_decoder, 0, num_files);
    tcase_add_test(tc_basics, test_equal_signature);

    return s;
}

