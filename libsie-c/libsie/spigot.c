/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>

#include "sie_spigot.h"

void sie_spigot_init(sie_Spigot *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

void sie_spigot_destroy(sie_Spigot *self)
{
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_spigot_spigot_prep(sie_Spigot *self)
{
}

sie_Output *sie_spigot_spigot_get(sie_Spigot *self)
{
    sie_Output *output;
    if (!self->prepped) {
        sie_spigot_prep(self);
        self->prepped = 1;
    }
    if (self->scans_limit && (self->scans >= self->scans_limit))
        return NULL;
    output = sie_spigot_get_inner(self);
    if (!output)
        return NULL;
    if (self->scans_limit && (self->scans + output->num_scans >
                              (size_t)self->scans_limit)) {
        sie_uint64 over =
            output->num_scans - (self->scans_limit - self->scans);
        sie_uint64 trim_to = output->num_scans - over;
        sie_output_trim(output, 0, (size_t)trim_to);
        self->scans += output->num_scans;
    }
    return output;
}

void sie_spigot_set_scan_limit(void *self, sie_uint64 limit)
{
    SIE_SPIGOT(self)->scans_limit = limit;
}

SIE_API_METHOD(sie_spigot_get, sie_Output *, NULL, self,
               (void *self), (self));

static size_t no_seek(sie_Spigot *self, size_t target)
{
    sie_errorf((self, "sie_spigot_seek not support for spigot type %s.\n",
                sie_object_class_name(self)));
    return SIE_SPIGOT_SEEK_END;
}

SIE_API_METHOD(sie_spigot_seek, size_t, SIE_SPIGOT_SEEK_END, self,
               (void *self, size_t target), (self, target));

static size_t no_tell(sie_Spigot *self)
{
    sie_errorf((self, "sie_spigot_tell not support for spigot type %s.\n",
                sie_object_class_name(self)));
    return SIE_SPIGOT_SEEK_END;
}

SIE_API_METHOD(sie_spigot_tell, size_t, SIE_SPIGOT_SEEK_END, self,
               (void *self), (self));

SIE_VOID_METHOD(sie_spigot_prep, self, (void *self), (self));
SIE_METHOD(sie_spigot_get_inner, sie_Output *, self, (void *self), (self));

SIE_API_METHOD(sie_spigot_done, int, 0, self, (void *self), (self));

SIE_VOID_API_METHOD(sie_spigot_clear_output, self, (void *self), (self));

enum {
    BEFORE = -1,
    AFTER = -2,
};

static ssize_t find_value_lower(sie_Output *output, size_t dim,
                                sie_float64 value)
{
    ssize_t low = 0;
    ssize_t high = output->num_scans - 1;
    ssize_t mid;
    if (value <= output->v[dim].float64[low])
        return BEFORE;
    if (value > output->v[dim].float64[high])
        return AFTER;
    for (;;) {
        if (low > high)
            return low;
        mid = (high + low) / 2;
        if (output->v[dim].float64[mid] < value) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
}

static ssize_t find_value_upper(sie_Output *output, size_t dim,
                                sie_float64 value)
{
    ssize_t low = 0;
    ssize_t high = output->num_scans - 1;
    ssize_t mid;
    if (value < output->v[dim].float64[low])
        return BEFORE;
    if (value >= output->v[dim].float64[high])
        return AFTER;
    for (;;) {
        if (low > high)
            return high;
        mid = (high + low) / 2;
        if (output->v[dim].float64[mid] > value) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
}

static int binary_search(sie_Spigot *self, size_t dim, sie_float64 value,
                         size_t *block, size_t *scan, int upper_bound)
{
    size_t orig = sie_spigot_tell(self);
    ssize_t num = sie_spigot_seek(self, SIE_SPIGOT_SEEK_END);
    ssize_t low = 0;
    ssize_t high = num - 1;
    ssize_t mid;
    int retval = 0;
    sie_Output *output;
    ssize_t pos;

    sie_debug((self, 4, "sie_spigot_binary_search(dim = %"APR_SIZE_T_FMT", "
               "value = %g)\n", dim, value));
    for (;;) {
        sie_debug((self, 4, "  low %"APR_SSIZE_T_FMT" high %"APR_SSIZE_T_FMT" "
                   "mid %"APR_SSIZE_T_FMT" value %f\n",
                   low, high, (high + low) / 2, value));
        if (low > high) {
            if (upper_bound) {
                if (high >= 0) {
                    *block = high;
                    sie_spigot_seek(self, high);
                    output = sie_spigot_get(self);
                    sie_assert(output, self);
                    *scan = output->num_scans - 1;
                    retval = 1;
                }
            } else {
                if (low < num) {
                    *block = low;
                    *scan = 0;
                    retval = 1;
                }
            }
            goto out;
        }
        mid = (high + low) / 2;
        sie_spigot_seek(self, mid);
        output = sie_spigot_get(self);
        sie_assert(output, self);
        if (upper_bound)
            pos = find_value_upper(output, dim, value);
        else
            pos = find_value_lower(output, dim, value);
        if (pos >= 0) {
            /* Found in block */
            *block = mid;
            *scan = pos;
            retval = 1;
            goto out;
        } else if (pos == AFTER) {
            low = mid + 1;
        } else if (pos == BEFORE) {
            high = mid - 1;
        } else {
            sie_errorf((self, "sie_spigot_binary_search failure"));
        }
    }
out:
    if (retval)
        sie_debug((self, 4, "  next sample block %"APR_SIZE_T_FMT" "
                   "scan %"APR_SIZE_T_FMT"\n", *block, *scan));
    else
        sie_debug((self, 4, "  not found\n"));
        
    sie_spigot_seek(self, orig);
    return retval;
}

int sie_spigot_lower_bound(sie_Spigot *self, size_t dim, sie_float64 value,
                           size_t *block, size_t *scan)
{
    return binary_search(self, dim, value, block, scan, 0);
}

int sie_spigot_upper_bound(sie_Spigot *self, size_t dim, sie_float64 value,
                           size_t *block, size_t *scan)
{
    return binary_search(self, dim, value, block, scan, 1);
}

SIE_API_METHOD(sie_binary_search, int, 0, self,
               (void *self, size_t dim, sie_float64 value,
                size_t *block, size_t *scan),
               (self, dim, value, block, scan));

SIE_API_METHOD(sie_lower_bound, int, 0, self,
               (void *self, size_t dim, sie_float64 value,
                size_t *block, size_t *scan),
               (self, dim, value, block, scan));

SIE_API_METHOD(sie_upper_bound, int, 0, self,
               (void *self, size_t dim, sie_float64 value,
                size_t *block, size_t *scan),
               (self, dim, value, block, scan));

static void no_disable_transforms(sie_Spigot *self, int disable)
{
    sie_errorf((self, "Tried to disable transforms on a spigot (of type %s) "
                "that does not use transforms.",
                sie_object_class_name(self)));
}

SIE_VOID_API_METHOD(sie_spigot_disable_transforms, self,
                    (void *self, int disable),
                    (self, disable));

static void no_transform_output(sie_Spigot *self, sie_Output *output)
{
    sie_errorf((self, "Tried to transform output with a spigot (of type %s) "
                "that does not use transforms.",
                sie_object_class_name(self)));
}

SIE_VOID_API_METHOD(sie_spigot_transform_output, self,
                    (void *self, sie_Output *output),
                    (self, output));

SIE_CLASS(sie_Spigot, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_spigot_destroy)
          SIE_MDEF(sie_spigot_prep, sie_spigot_spigot_prep)
          SIE_MDEF(sie_spigot_get, sie_spigot_spigot_get)
          SIE_MDEF(sie_spigot_get_inner, sie_abstract_method)
          SIE_MDEF(sie_spigot_clear_output, sie_abstract_method)
          SIE_MDEF(sie_spigot_done, sie_abstract_method)
          SIE_MDEF(sie_spigot_seek, no_seek)
          SIE_MDEF(sie_spigot_tell, no_tell)
          SIE_MDEF(sie_binary_search, sie_spigot_lower_bound)
          SIE_MDEF(sie_lower_bound, sie_spigot_lower_bound)
          SIE_MDEF(sie_upper_bound, sie_spigot_upper_bound)
          SIE_MDEF(sie_spigot_disable_transforms, no_disable_transforms)
          SIE_MDEF(sie_spigot_transform_output, no_transform_output));
