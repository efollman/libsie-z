/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Histogram_Bound sie_Histogram_Bound;
typedef struct _sie_Histogram_Dim sie_Histogram_Dim;
typedef struct _sie_Histogram sie_Histogram;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_HISTOGRAM_H
#define SIE_HISTOGRAM_H

struct _sie_Histogram_Bound {
    sie_float64 lower;
    sie_float64 upper;
};
    
struct _sie_Histogram_Dim {
    size_t num_bins;
    sie_Histogram_Bound *bounds;
};

struct _sie_Histogram {
    sie_Context_Object parent;
    sie_Histogram_Dim *dims;
    size_t total_size;
    sie_float64 *bins;
};
SIE_CLASS_DECL(sie_Histogram);
#define SIE_HISTOGRAM(p) SIE_SAFE_CAST(p, sie_Histogram)

SIE_DECLARE(sie_Histogram *) sie_histogram_new(void *channel);
SIE_DECLARE(void) sie_histogram_init(sie_Histogram *self, void *channel);
SIE_DECLARE(void) sie_histogram_destroy(sie_Histogram *self);

/*
SIE_DECLARE(size_t) sie_histogram_get_total_size(sie_Histogram *self);
SIE_DECLARE(sie_float64) sie_histogram_get_row_major_bin(
    sie_Histogram *self, size_t index, size_t *indices);
*/

SIE_DECLARE(size_t) sie_histogram_flatten_indices(sie_Histogram *self,
                                                  size_t *indices);

SIE_DECLARE(size_t) sie_histogram_get_num_dims(sie_Histogram *self);
SIE_DECLARE(size_t) sie_histogram_get_num_bins(sie_Histogram *self,
                                               size_t dim);
SIE_DECLARE(void) sie_histogram_get_bin_bounds(
    sie_Histogram *self, size_t dim,
    sie_float64 *lower, sie_float64 *upper);
SIE_DECLARE(sie_float64) sie_histogram_get_bin(
    sie_Histogram *self, size_t *indices);
SIE_DECLARE(sie_float64) sie_histogram_get_next_nonzero_bin(
    sie_Histogram *self, size_t *start, size_t *indices);

#endif

#endif
