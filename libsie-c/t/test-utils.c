#include "sie_config.h"

#include <apr_general.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_strings.h>
#include <stdio.h>

#include "my-check.h"
#include "test-utils.h"

FILE *sie_debug_stream;
int sie_debug_level;

sie_Context *ctx;
static int num_inits;
static int num_destroys;

void setup_ctx(void)
{
    ctx = sie_context_new();
    num_inits = 1;
    num_destroys = 0;
    ctx->debug_stream = sie_debug_stream;
    ctx->debug_level = sie_debug_level;
}

void teardown_ctx(void)
{
    sie_context_done(ctx);
}

void check_ctx(void)
{
    int remaining;
    sie_Exception *ex = sie_get_exception(ctx);
    fail_if(ex, "Leftover exception:\n%s", sie_verbose_report(ex));
    sie_release(ex);
    num_inits = ctx->num_inits;
    num_destroys = ctx->num_destroys;
    ctx->num_inits_p = &num_inits;
    ctx->num_destroys_p = &num_destroys;
    remaining = sie_context_done(ctx);
    fail_if(remaining || num_inits > num_destroys,
            "Object leak: Claimed remaining=%d, inits=%d, destroys=%d.",
            remaining, num_inits, num_destroys);
}

apr_pool_t *pool;

void setup_apr(void)
{
    fail_if(apr_initialize(), NULL);
    fail_if(apr_pool_create(&pool, NULL), NULL);
}

void teardown_apr(void)
{
    apr_pool_destroy(pool);
    pool = NULL;
    apr_terminate();
}

#include "sie_vec.h"

char *slurp(const char *filename)
{
    apr_finfo_t finfo;
    apr_file_t *file;
    char *vec = NULL;
    apr_size_t bytes_read;
    apr_pool_t *fpool;
    fail_if(apr_pool_create(&fpool, pool),
            "slurp: apr_pool_create(fpool) failed");
    fail_if(apr_stat(&finfo, filename, APR_FINFO_NORM, fpool),
            "slurp: apr_stat(%s) failed", filename);
    sie_vec_reserve(vec, (size_t)finfo.size + 1);
    fail_if(apr_file_open(&file, filename,
                          APR_READ | APR_BINARY | APR_BUFFERED,
                          APR_OS_DEFAULT, fpool),
            "slurp: apr_file_open(%s) failed", filename);
    fail_if(apr_file_read_full(file, vec, (size_t)finfo.size, &bytes_read),
            "slurp: apr_file_read_full(%s) failed", filename);
    vec[bytes_read] = 0;
    sie_vec_raw_size(vec) = bytes_read;
    fail_if(apr_file_close(file),
            "slurp: apr_file_close(%s) failed", filename);
    apr_pool_destroy(fpool);
    return vec;
}

void spew(char *vec, const char *filename)
{
    apr_file_t *file;
    apr_size_t to_write = sie_vec_size(vec);
    apr_pool_t *fpool;
    fail_if(apr_pool_create(&fpool, pool),
            "spew: apr_pool_create(fpool) failed");
    fail_if(apr_file_open(&file, filename,
                          APR_WRITE | APR_CREATE | APR_TRUNCATE |
                          APR_BINARY | APR_BUFFERED,
                          APR_OS_DEFAULT, fpool),
            "spew: apr_file_open(%s) failed", filename);
    fail_if(apr_file_write_full(file, vec, to_write, NULL),
            "spew: apr_file_write_full(%s) failed", filename);
    fail_if(apr_file_close(file),
            "spew: apr_file_close(%s) failed", filename);
    apr_pool_destroy(fpool);
}

void mkdir_p(const char *dirname)
{
    fail_if(apr_dir_make_recursive(dirname, APR_OS_DEFAULT, pool),
            "mkdir_p(%s) failed", dirname);
}

int compare(const char *a, const char *b)
{
    if (sie_vec_size(a) != sie_vec_size(b))
        return -1;
    return memcmp(a, b, sie_vec_size(a));
}


static char **qpf_cleanup;

char *qpf(const char *fmt, ...)
{
    size_t len;
    va_list args;
    char *new_string = NULL;
    va_start(args, fmt);
    len = apr_vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    sie_vec_reserve(new_string, len + 1);
    va_start(args, fmt);
    apr_vsnprintf(new_string, len + 1, fmt, args);
    va_end(args);
    sie_vec_push_back(qpf_cleanup, new_string);
    return new_string;
}

void qpf_free(void)
{
    char **cur;
    sie_vec_forall(qpf_cleanup, cur) {
        sie_vec_free(*cur);
    }
    sie_vec_free(qpf_cleanup);
    qpf_cleanup = NULL;
}

static const char *bad_windows_chars = "/\\:*?\"><|";

char *safe_fn(const char *name)
{
    size_t i, j;
    char *copy = qpf("%s", name);
    size_t len = strlen(copy);
    for (i = 0; i < len; i++) {
        for (j = 0; j < sizeof(bad_windows_chars); j++) {
            if (copy[i] == bad_windows_chars[j])
                copy[i] = '_';
        }
    }
    return copy;
}

void test_exception_result(void *obj, const char *expected)
{
    char *report;
    sie_Exception *ex;
    fail_unless(obj == NULL, "excepting API method returned a pointer");
    ex = sie_get_exception(ctx);
    fail_if(ex == NULL, "no exception");
    report = sie_verbose_report(ex);
    fail_unless(!strncmp(report, expected, strlen(expected)),
                "Exception report '%s' did not match expected '%s'",
                report, expected);
    sie_release(ex);
}


void sie_ctx_destroy_check_destroy(sie_Ctx_Destroy_Check *self)
{
    *self->target = self->value;
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_ctx_destroy_check_free_object(sie_Ctx_Destroy_Check *self)
{
    *self->free_target = self->value;
    sie_base_object_free_object(SIE_BASE_OBJECT(self));
}

void sie_ctx_destroy_check_init(sie_Ctx_Destroy_Check *self, void *ctx_obj,
                                 int *target, int *free_target,
                                 int value, int except)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->target = target;
    self->free_target = free_target;
    self->value = value;
    if (except) sie_throw(sie_exception_new(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_ctx_destroy_check_new, sie_Ctx_Destroy_Check, self, ctx_obj,
    (void *ctx_obj, int *target, int *free_target,
     int value, int except),
    sie_ctx_destroy_check_init(self, ctx_obj, target, free_target,
                               value, except));

#ifdef _MSC_VER
#pragma warning (disable: 4273) /* inconsistent dll linkage */
#endif
SIE_CLASS(sie_Ctx_Destroy_Check, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_ctx_destroy_check_destroy)
          SIE_MDEF(sie_free_object, sie_ctx_destroy_check_free_object));

/* FROB FILE SPECIFICATION:
 * a offset bytes\ndata\n - add bytes of data to offset
 * m offset bytes\ndata\n - modify bytes of data at offet
 * d offset bytes\n       - delete bytes of data at offset
 */

int maybe_frob_victim(FILE *frob_file, char **victim)
{
    char line[128];
    char *data = NULL;
    char op;
    int offset, bytes;

    if (!fgets(line, sizeof(line), frob_file))
        return 0;
    fail_if(sscanf(line, "%c %d %d", &op, &offset, &bytes) != 3);
    /* fprintf(stderr, "frobbing %s", line); */

    if (op != 'd') {
        char nl;
        data = malloc(bytes);
        if (bytes)
            fail_if(!fread(data, bytes, 1, frob_file));
        fail_if(!fread(&nl, 1, 1, frob_file));
        fail_unless(nl == '\n');
    }

    fail_if(offset < 0);
    fail_if(bytes < 0);

    switch (op) {
    case 'm':
        fail_if(offset + bytes > (apr_ssize_t)sie_vec_size(*victim));
        memcpy(*victim + offset, data, bytes);
        break;
    case 'a':
        fail_if(offset > (apr_ssize_t)sie_vec_size(*victim));
        sie_vec_reserve(*victim, sie_vec_size(*victim) + bytes);
        memmove(*victim + offset + bytes, *victim + offset,
                sie_vec_size(*victim) - offset);
        sie_vec_raw_size(*victim) += bytes;
        break;
    case 'd':
        fail_if(offset + bytes > (apr_ssize_t)sie_vec_size(*victim));
        memmove(*victim + offset, *victim + offset + bytes,
                sie_vec_size(*victim) - offset - bytes);
        sie_vec_raw_size(*victim) -= bytes;
        break;
    }

    return 1;
}

static void kill_crcs_group(sie_id group, sie_File_Group_Index *index,
                            void *v_victim)
{
    sie_Block *block = sie_block_new(ctx);
    char *victim = v_victim;
    sie_File_Group_Index_Entry *entry;
    sie_vec_forall(index->entries, entry) {
        memset(victim + (entry->offset + entry->size + 12), 0, 4);
    }
    sie_release(block);
}

void kill_crcs(const char *sie_file_name, char *victim)
{
    sie_File *file = sie_file_open(ctx, sie_file_name);
    fail_if(!file);

    sie_file_group_foreach(file, kill_crcs_group, victim);

    sie_release(file);
}
