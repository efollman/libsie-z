/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_histogram.h"

SIE_CONTEXT_OBJECT_API_NEW_FN(
    sie_histogram_new, sie_Histogram, self, channel,
    (void *channel), sie_histogram_init(self, channel));

static size_t not_found = (size_t)-1;

static int compare_bounds(const void *a_v, const void *b_v)
{
    const sie_Histogram_Bound *a = a_v;
    const sie_Histogram_Bound *b = b_v;
    if (a->lower < b->lower)
        return -1;
    else if (a->lower > b->lower)
        return 1;
    else if (a->upper < b->upper)
        return -1;
    else if (a->upper > b->upper)
        return 1;
    else
        return 0;
}

static size_t binary_bound_search(sie_Histogram_Bound *haystack,
                                  sie_Histogram_Bound *needle)
{
    ssize_t low = 0;
    ssize_t high = sie_vec_size(haystack) - 1;
    ssize_t mid;
    int comparison;
    for (;;) {
        if (low > high)
            return not_found;
        mid = (high + low) / 2;
        comparison = compare_bounds(&haystack[mid], needle);
        if (comparison == 0) {
            return mid;
        } else if (comparison < 0) {
            low = mid + 1;
        } else  {
            high = mid - 1;
        }
    }
}

static void sort_bounds(sie_Histogram_Bound *bounds)
{
    qsort(bounds, sie_vec_size(bounds), sizeof(*bounds), compare_bounds);
}

static void maybe_add_bound(sie_Histogram *self, size_t dim,
                            sie_float64 lower, sie_float64 upper)
{
    sie_Histogram_Bound bound;
    sie_Histogram_Dim *dim_s;
    bound.lower = lower;
    bound.upper = upper;
    sie_assert(dim < sie_vec_size(self->dims), self);
    dim_s = &self->dims[dim];
    sie_assert(dim_s, self);
    if (binary_bound_search(dim_s->bounds, &bound) == not_found) {
        sie_vec_push_back(dim_s->bounds, bound);
        sort_bounds(dim_s->bounds);
    }
}

static size_t find_bound(sie_Histogram *self, size_t dim,
                         sie_float64 lower, sie_float64 upper)
{
    sie_Histogram_Bound bound;
    bound.lower = lower;
    bound.upper = upper;
    sie_assert(dim < sie_vec_size(self->dims), self);
    return binary_bound_search(self->dims[dim].bounds, &bound);
}

static size_t flat(sie_Histogram *self, size_t *indices)
{
    size_t result = 0;
    size_t mul = 1;
    ssize_t dim;
    
    for (dim = sie_vec_size(self->dims) - 1; dim >= 0; dim--) {
        result += indices[dim] * mul;
        mul *= sie_vec_size(self->dims[dim].bounds);
    }

    return result;
}

static void unflat(sie_Histogram *self, size_t index, size_t *indices)
{
    ssize_t dim;
    
    for (dim = sie_vec_size(self->dims) - 1; dim >= 0; dim--) {
        indices[dim] = index % sie_vec_size(self->dims[dim].bounds);
        index /= sie_vec_size(self->dims[dim].bounds);
    }
}

void sie_histogram_init(sie_Histogram *self, void *channel)
{
    sie_Spigot *spigot;
    sie_Output *output;
    size_t num_dims;
    size_t dim;
    size_t *indices;
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), channel);
    sie_error_context_push(self, "Parsing histogram for channel id %d",
                           sie_get_id(channel));

    /* Find number of dimensions. */
    spigot = sie_autorelease(sie_attach_spigot(channel));
    output = sie_spigot_get(spigot);
    if (output) {
        sie_assert(output->num_vs >= 3, self);
        sie_assert((output->num_vs % 2) == 1, self);
        num_dims = (output->num_vs - 1) / 2;
        sie_cleanup_pop(self, spigot, 1);

        sie_vec_reserve(self->dims, num_dims);
        memset(self->dims, 0, num_dims * sizeof(*self->dims));
        sie_vec_raw_size(self->dims) = num_dims;

        /* Find bin bounds. */
        spigot = sie_autorelease(sie_attach_spigot(channel));
        while ( (output = sie_spigot_get(spigot)) ) {
            size_t scan;
            for (dim = 0; dim < output->num_vs; dim++)
                sie_assert(output->v[dim].type == SIE_OUTPUT_FLOAT64, self);
            for (scan = 0; scan < output->num_scans; scan++) {
                for (dim = 0; dim < num_dims; dim++)
                    maybe_add_bound(self, dim,
                                    output->v[dim * 2 + 1].float64[scan],
                                    output->v[dim * 2 + 2].float64[scan]);
            }
        }
        sie_cleanup_pop(self, spigot, 1);

        self->total_size = 1;
        for (dim = 0; dim < num_dims; dim++)
            self->total_size *= sie_vec_size(self->dims[dim].bounds);
        self->bins = sie_calloc(self, sizeof(*self->bins) * self->total_size);
        sie_assert(self->bins, self);

        /* Load up data. */
        spigot = sie_autorelease(sie_attach_spigot(channel));
        indices = sie_malloc(self, sizeof(*indices) * num_dims);
        sie_cleanup_push(self, free, indices);
        while ( (output = sie_spigot_get(spigot)) ) {
            size_t scan;
            for (scan = 0; scan < output->num_scans; scan++) {
                size_t index;
                for (dim = 0; dim < num_dims; dim++) {
                    indices[dim] = 
                        find_bound(self, dim,
                                   output->v[dim * 2 + 1].float64[scan],
                                   output->v[dim * 2 + 2].float64[scan]);
                    sie_assert(indices[dim] != not_found, self);
                }
                index = flat(self, indices);
                sie_assert(index < self->total_size, self);
                self->bins[index] = output->v[0].float64[scan];
            }
        }
        sie_cleanup_pop(self, indices, 1);
    }

    sie_cleanup_pop(self, spigot, 1);
    sie_error_context_pop(self);
}

void sie_histogram_destroy(sie_Histogram *self)
{
    sie_Histogram_Dim *dim;
    sie_vec_forall(self->dims, dim) {
        sie_vec_free(dim->bounds);
    }
    sie_vec_free(self->dims);
    free(self->bins);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

size_t sie_histogram_get_num_dims(sie_Histogram *self)
{
    if (!self) return 0;
    return sie_vec_size(self->dims);
}

size_t sie_histogram_get_num_bins(sie_Histogram *self, size_t dim)
{
    if (!self || dim >= sie_vec_size(self->dims)) return 0;
    return sie_vec_size(self->dims[dim].bounds);
}

void sie_histogram_get_bin_bounds(sie_Histogram *self, size_t dim,
                                  sie_float64 *lower, sie_float64 *upper)
{
    size_t i;
    sie_Histogram_Bound *bounds;
    if (!self || dim >= sie_vec_size(self->dims)) return;
    bounds = self->dims[dim].bounds;
    for (i = 0; i < sie_vec_size(bounds); i++) {
        lower[i] = bounds[i].lower;
        upper[i] = bounds[i].upper;
    }
}

size_t sie_histogram_flatten_indices(sie_Histogram *self, size_t *indices)
{
    if (!self || !self->bins) return 0;
    return flat(self, indices);
}

sie_float64 sie_histogram_get_bin(sie_Histogram *self, size_t *indices)
{
    size_t index;
    if (!self || !self->bins) return 0.0;
    index = flat(self, indices);
    return self->bins[index];
}

sie_float64 sie_histogram_get_next_nonzero_bin(sie_Histogram *self,
                                               size_t *start,
                                               size_t *indices)
{
    sie_float64 bin;
    if (!self || !self->bins) return 0.0;
    while (*start < self->total_size) {
        if ((bin = self->bins[*start]) != 0.0) {
            unflat(self, *start, indices);
            (*start)++;
            return bin;
        }
        (*start)++;
    }
    return 0.0;
}

SIE_CLASS(sie_Histogram, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_histogram_destroy));
