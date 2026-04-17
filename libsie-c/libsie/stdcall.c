/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#define WVOID
#define WRET return

#define PP_CAT(a, b) PP_PRIMITIVE_CAT(a, b)
#define PP_PRIMITIVE_CAT(a, b) a ## b

#define sfx _stdcall

#define WRAP(ret, name) SIE_DECLARE_STD(ret) PP_CAT(name, sfx)

#define WRAP_0(t, r, n)                         \
    WRAP(r, n)(void) { t n(); }
#define WRAP_1(t, r, n, ta)                     \
    WRAP(r, n)(ta a) { t n(a); }
#define WRAP_2(t, r, n, ta, tb)                 \
    WRAP(r, n)(ta a, tb b) { t n(a, b); }
#define WRAP_3(t, r, n, ta, tb, tc)             \
    WRAP(r, n)(ta a, tb b, tc c) { t n(a, b, c); }
#define WRAP_4(t, r, n, ta, tb, tc, td)         \
    WRAP(r, n)(ta a, tb b, tc c, td d) { t n(a, b, c, d); }
#define WRAP_5(t, r, n, ta, tb, tc, td, te)     \
    WRAP(r, n)(ta a, tb b, tc c, td d, te e) { t n(a, b, c, d, e); }

WRAP_0(WRET, sie_Context *, sie_context_new)
WRAP_1(WRET, void *, sie_retain, void *)
WRAP_1(WVOID, void, sie_release, void *)
WRAP_1(WRET, int, sie_context_done, sie_Context *)
WRAP_2(WRET, sie_File *, sie_file_open, void *, const char *)

WRAP_1(WRET, sie_Iterator *, sie_get_tests, void *)
WRAP_1(WRET, sie_Iterator *, sie_get_channels, void *)
WRAP_1(WRET, sie_Iterator *, sie_get_dimensions, void *)
WRAP_1(WRET, sie_Iterator *, sie_get_tags, void *)
WRAP_2(WRET, sie_Test *, sie_get_test, void *, sie_id)
WRAP_2(WRET, sie_Channel *, sie_get_channel, void *, sie_id)
WRAP_2(WRET, sie_Dimension *, sie_get_dimension, void *, sie_id)
WRAP_2(WRET, sie_Tag *, sie_get_tag, void *, const char *)
WRAP_1(WRET, sie_Test *, sie_get_containing_test, void *)
WRAP_1(WRET, const char *, sie_get_name, void *)
WRAP_1(WRET, int, sie_get_index, void *)

WRAP_1(WRET, void *, sie_iterator_next, sie_Iterator *)

WRAP_1(WRET, const char *, sie_tag_get_id, sie_Tag *)
WRAP_1(WRET, char *, sie_tag_get_value, sie_Tag *)
WRAP_3(WRET, int, sie_tag_get_value_b, sie_Tag *, char **, size_t *)

WRAP_1(WVOID, void, sie_free, void *)
WRAP_1(WVOID, void, sie_system_free, void *)

WRAP_1(WRET, sie_Spigot *, sie_attach_spigot, void *)
WRAP_1(WRET, sie_Output *, sie_spigot_get, void *)
WRAP_2(WVOID, void, sie_spigot_disable_transforms, void *, int);
WRAP_2(WVOID, void, sie_spigot_transform_output, void *, sie_Output *);
WRAP_2(WRET, size_t, sie_spigot_seek, void *, size_t)
WRAP_1(WRET, size_t, sie_spigot_tell, void *)

WRAP_5(WRET, int, sie_lower_bound, void *,
       size_t, sie_float64, size_t *, size_t *)
WRAP_5(WRET, int, sie_upper_bound, void *,
       size_t, sie_float64, size_t *, size_t *)
WRAP_5(WRET, int, sie_binary_search, void *,
       size_t, sie_float64, size_t *, size_t *)

WRAP_1(WRET, size_t, sie_output_get_block, sie_Output *)
WRAP_1(WRET, size_t, sie_output_get_num_dims, sie_Output *)
WRAP_1(WRET, size_t, sie_output_get_num_rows, sie_Output *)
WRAP_2(WRET, int, sie_output_get_type, sie_Output *, size_t)
WRAP_2(WRET, sie_float64 *, sie_output_get_float64, sie_Output *, size_t)
WRAP_2(WRET, sie_Output_Raw *, sie_output_get_raw, sie_Output *, size_t)
WRAP_1(WRET, sie_Output_Struct *, sie_output_get_struct, sie_Output *)

WRAP_1(WRET, sie_Histogram *, sie_histogram_new, void *)
WRAP_1(WRET, size_t, sie_histogram_get_num_dims, sie_Histogram *)
WRAP_2(WRET, size_t, sie_histogram_get_num_bins, sie_Histogram *, size_t)
WRAP_4(WVOID, void, sie_histogram_get_bin_bounds,
       sie_Histogram *, size_t, sie_float64 *, sie_float64 *)
WRAP_2(WRET, sie_float64, sie_histogram_get_bin, sie_Histogram *, size_t *)
WRAP_3(WRET, sie_float64, sie_histogram_get_next_nonzero_bin,
       sie_Histogram *, size_t *, size_t *)

WRAP_1(WRET, sie_Exception *, sie_check_exception, void *)
WRAP_1(WRET, sie_Exception *, sie_get_exception, void *)
WRAP_1(WRET, char *, sie_report, void *)
WRAP_1(WRET, char *, sie_verbose_report, void *)
WRAP_2(WVOID, void, sie_ignore_trailing_garbage, void *, size_t)

WRAP_4(WVOID, void, sie_set_progress_callbacks,
       void *, void *, sie_Progress_Set_Message *, sie_Progress_Percent *)

WRAP_2(WRET, int, sie_file_is_sie, void *, const char *)

WRAP_1(WRET, sie_Stream *, sie_stream_new, void *)
WRAP_3(WRET, size_t, sie_add_stream_data, void *, const void *, size_t)
WRAP_1(WRET, int, sie_spigot_done, void *)
