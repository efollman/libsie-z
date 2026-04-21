/*
 * sie.h — C ABI for libsie-z (SIE file reader, Zig port).
 *
 * All handles are opaque pointers. Functions returning `int` return 0
 * (`SIE_OK`) on success, or a non-zero status code (see SIE_E_*).
 * Strings are returned as (ptr, len) pairs and are NOT NUL-terminated;
 * tag values may contain embedded NULs.
 *
 * Memory ownership:
 *   - `*_open`, `*_attach`, `*_new`, `*_from_*` create handles you must
 *     release with the matching `*_close` / `*_free` function.
 *   - Accessor handles (channels, tests, tags, dims, outputs) are borrowed
 *     and remain valid until the owning object is freed.
 *   - An Output returned from `sie_spigot_get` is invalidated by the next
 *     call to `sie_spigot_get` on the same spigot.
 */
#ifndef LIBSIE_SIE_H
#define LIBSIE_SIE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status codes ──────────────────────────────────────────────────────── */
#define SIE_OK                       0
#define SIE_E_FILE_NOT_FOUND         1
#define SIE_E_PERMISSION_DENIED      2
#define SIE_E_FILE_OPEN              3
#define SIE_E_FILE_READ              4
#define SIE_E_FILE_WRITE             5
#define SIE_E_FILE_SEEK              6
#define SIE_E_FILE_TRUNCATED         7
#define SIE_E_INVALID_FORMAT        10
#define SIE_E_INVALID_BLOCK         11
#define SIE_E_UNEXPECTED_EOF        12
#define SIE_E_CORRUPTED_DATA        13
#define SIE_E_INVALID_XML           20
#define SIE_E_INVALID_EXPRESSION    21
#define SIE_E_PARSE                 22
#define SIE_E_OUT_OF_MEMORY         30
#define SIE_E_INVALID_DATA          40
#define SIE_E_DIMENSION_MISMATCH    41
#define SIE_E_INDEX_OUT_OF_BOUNDS   42
#define SIE_E_NOT_IMPLEMENTED       50
#define SIE_E_OPERATION_FAILED      51
#define SIE_E_STREAM_ENDED          52
#define SIE_E_UNKNOWN               99

/* Output dimension types (returned by sie_output_type). */
#define SIE_OUTPUT_NONE      0
#define SIE_OUTPUT_FLOAT64   1
#define SIE_OUTPUT_RAW       2

/* ── Opaque handles ────────────────────────────────────────────────────── */
typedef struct sie_File      sie_File;
typedef struct sie_Channel   sie_Channel;
typedef struct sie_Test      sie_Test;
typedef struct sie_Tag       sie_Tag;
typedef struct sie_Dimension sie_Dimension;
typedef struct sie_Spigot    sie_Spigot;
typedef struct sie_Output    sie_Output;
typedef struct sie_Stream    sie_Stream;
typedef struct sie_Histogram sie_Histogram;

/* ── Library info ──────────────────────────────────────────────────────── */
const char *sie_version(void);
const char *sie_status_message(int status);

/* ── SieFile ───────────────────────────────────────────────────────────── */
int      sie_file_open(const char *path, sie_File **out_handle);
void     sie_file_close(sie_File *handle);

size_t   sie_file_num_channels(sie_File *handle);
size_t   sie_file_num_tests(sie_File *handle);
size_t   sie_file_num_tags(sie_File *handle);

sie_Channel   *sie_file_channel(sie_File *handle, size_t index);
sie_Test      *sie_file_test(sie_File *handle, size_t index);
const sie_Tag *sie_file_tag(sie_File *handle, size_t index);

sie_Channel *sie_file_find_channel(sie_File *handle, uint32_t id);
sie_Test    *sie_file_find_test(sie_File *handle, uint32_t id);
sie_Test    *sie_file_containing_test(sie_File *handle, sie_Channel *ch);

/* ── Test ──────────────────────────────────────────────────────────────── */
uint32_t       sie_test_id(sie_Test *handle);
void           sie_test_name(sie_Test *handle, const char **out_ptr, size_t *out_len);
size_t         sie_test_num_channels(sie_Test *handle);
sie_Channel   *sie_test_channel(sie_Test *handle, size_t index);
size_t         sie_test_num_tags(sie_Test *handle);
const sie_Tag *sie_test_tag(sie_Test *handle, size_t index);
const sie_Tag *sie_test_find_tag(sie_Test *handle, const char *key);

/* ── Channel ───────────────────────────────────────────────────────────── */
uint32_t              sie_channel_id(sie_Channel *handle);
uint32_t              sie_channel_test_id(sie_Channel *handle);
void                  sie_channel_name(sie_Channel *handle, const char **out_ptr, size_t *out_len);
size_t                sie_channel_num_dims(sie_Channel *handle);
const sie_Dimension  *sie_channel_dimension(sie_Channel *handle, size_t index);
size_t                sie_channel_num_tags(sie_Channel *handle);
const sie_Tag        *sie_channel_tag(sie_Channel *handle, size_t index);
const sie_Tag        *sie_channel_find_tag(sie_Channel *handle, const char *key);

/* ── Dimension ─────────────────────────────────────────────────────────── */
uint32_t       sie_dimension_index(const sie_Dimension *handle);
void           sie_dimension_name(const sie_Dimension *handle, const char **out_ptr, size_t *out_len);
size_t         sie_dimension_num_tags(const sie_Dimension *handle);
const sie_Tag *sie_dimension_tag(const sie_Dimension *handle, size_t index);
const sie_Tag *sie_dimension_find_tag(const sie_Dimension *handle, const char *key);

/* ── Tag ───────────────────────────────────────────────────────────────── */
void     sie_tag_key(const sie_Tag *handle, const char **out_ptr, size_t *out_len);
void     sie_tag_value(const sie_Tag *handle, const char **out_ptr, size_t *out_len);
size_t   sie_tag_value_size(const sie_Tag *handle);
int      sie_tag_is_string(const sie_Tag *handle);
int      sie_tag_is_binary(const sie_Tag *handle);
uint32_t sie_tag_group(const sie_Tag *handle);
int      sie_tag_is_from_group(const sie_Tag *handle);

/* ── Spigot ────────────────────────────────────────────────────────────── */
int      sie_spigot_attach(sie_File *file, sie_Channel *channel, sie_Spigot **out);
void     sie_spigot_free(sie_Spigot *handle);

/* On exhaustion, *out_output is set to NULL and SIE_OK is returned. */
int      sie_spigot_get(sie_Spigot *handle, sie_Output **out_output);

uint64_t sie_spigot_tell(sie_Spigot *handle);
uint64_t sie_spigot_seek(sie_Spigot *handle, uint64_t target);
void     sie_spigot_reset(sie_Spigot *handle);
int      sie_spigot_is_done(sie_Spigot *handle);
size_t   sie_spigot_num_blocks(sie_Spigot *handle);

void     sie_spigot_disable_transforms(sie_Spigot *handle, int disable);
int      sie_spigot_transform_output(sie_Spigot *handle, sie_Output *output);
void     sie_spigot_set_scan_limit(sie_Spigot *handle, uint64_t limit);

int      sie_spigot_lower_bound(sie_Spigot *handle, size_t dim, double value,
                                uint64_t *out_block, uint64_t *out_scan, int *out_found);
int      sie_spigot_upper_bound(sie_Spigot *handle, size_t dim, double value,
                                uint64_t *out_block, uint64_t *out_scan, int *out_found);

/* ── Output (borrowed from spigot) ─────────────────────────────────────── */
size_t   sie_output_num_dims(sie_Output *handle);
size_t   sie_output_num_rows(sie_Output *handle);
size_t   sie_output_block(sie_Output *handle);
int      sie_output_type(sie_Output *handle, size_t dim);
int      sie_output_get_float64(sie_Output *handle, size_t dim, size_t row, double *out_value);
int      sie_output_get_raw(sie_Output *handle, size_t dim, size_t row,
                            const uint8_t **out_ptr, uint32_t *out_size);

/* ── Stream (incremental ingest) ───────────────────────────────────────── */
int      sie_stream_new(sie_Stream **out_handle);
void     sie_stream_free(sie_Stream *handle);
int      sie_stream_add_data(sie_Stream *handle, const uint8_t *data, size_t size,
                             size_t *out_consumed);
uint32_t sie_stream_num_groups(sie_Stream *handle);
size_t   sie_stream_group_num_blocks(sie_Stream *handle, uint32_t group_id);
uint64_t sie_stream_group_num_bytes(sie_Stream *handle, uint32_t group_id);
int      sie_stream_is_group_closed(sie_Stream *handle, uint32_t group_id);

/* ── Histogram ─────────────────────────────────────────────────────────── */
int      sie_histogram_from_channel(sie_File *file, sie_Channel *channel,
                                    sie_Histogram **out_handle);
void     sie_histogram_free(sie_Histogram *handle);
size_t   sie_histogram_num_dims(sie_Histogram *handle);
size_t   sie_histogram_total_size(sie_Histogram *handle);
size_t   sie_histogram_num_bins(sie_Histogram *handle, size_t dim);
int      sie_histogram_get_bin(sie_Histogram *handle, const size_t *indices, double *out_value);
int      sie_histogram_get_bounds(sie_Histogram *handle, size_t dim,
                                  double *lower, double *upper, size_t capacity);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBSIE_SIE_H */
