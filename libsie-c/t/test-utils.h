#ifndef T_TEST_UTILS_H
#define T_TEST_UTILS_H

#include "sie_context.h"

extern sie_Context *ctx;

void setup_ctx(void);
void teardown_ctx(void);
void check_ctx(void);

apr_pool_t *pool;

void setup_apr(void);
void teardown_apr(void);
void mkdir_p(const char *dirname);
int compare(const char *a, const char *b);

char *qpf(const char *fmt, ...);
void qpf_free(void);
char *safe_fn(const char *name);

char *slurp(const char *filename);
void spew(char *vec, const char *filename);

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

void test_exception_result(void *obj, const char *expected);

typedef struct _sie_Ctx_Destroy_Check {
    sie_Context_Object parent;
    int *target;
    int *free_target;
    int value;
} sie_Ctx_Destroy_Check;
SIE_CLASS_DECL(sie_Ctx_Destroy_Check);
#define SIE_CTX_DESTROY_CHECK(p) SIE_SAFE_CAST(p, sie_Ctx_Destroy_Check)

void sie_ctx_destroy_check_destroy(sie_Ctx_Destroy_Check *self);
void sie_ctx_destroy_check_free_object(sie_Ctx_Destroy_Check *self);
void sie_ctx_destroy_check_init(sie_Ctx_Destroy_Check *self, void *ctx_obj,
                                int *target, int *free_target,
                                int value, int except);
sie_Ctx_Destroy_Check *sie_ctx_destroy_check_new(void *ctx_obj,
                                                 int *target,
                                                 int *free_target,
                                                 int value,
                                                 int except);

int maybe_frob_victim(FILE *frob_file, char **victim);
void kill_crcs(const char *sie_file_name, char *victim);

#endif
