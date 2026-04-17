/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

SIE_CONTEXT_OBJECT_API_NEW_FN(
    sie_plot_crusher_new, sie_Plot_Crusher, self, spigot,
    (void *spigot, size_t scans), sie_plot_crusher_init(self, spigot, scans));

void sie_plot_crusher_init(sie_Plot_Crusher *self, void *spigot, size_t scans)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), spigot);
    self->spigot = sie_retain(spigot);
    self->ideal_scans = scans;
    self->max_scans = scans * 2;
    self->per_scan = 1;
}

void sie_plot_crusher_destroy(sie_Plot_Crusher *self)
{
    sie_release(self->spigot);
    sie_release(self->crushed);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

#define pick(op, a, b) (((a) op (b)) ? (a) : (b))
#define max_v(v) ((v) & 1)

static void crush(sie_Plot_Crusher *self)
{
    size_t i, v;
    for (i = 0; i < self->ideal_scans; i++) {
        for (v = 0; v < self->crushed->num_vs; v += 2) {
            self->crushed->v[v].float64[i] =
                pick(<,
                     self->crushed->v[v].float64[i * 2],
                     self->crushed->v[v].float64[i * 2 + 1]);
            self->crushed->v[v + 1].float64[i] =
                pick(>,
                     self->crushed->v[v + 1].float64[i * 2],
                     self->crushed->v[v + 1].float64[i * 2 + 1]);
        }
    }
    for (v = 0; v < self->crushed->num_vs; v++)
        self->crushed->v_guts[v].size = self->ideal_scans;
    self->crushed->num_scans = self->ideal_scans;
}

static void work(sie_Plot_Crusher *self, sie_Output *input)
{
    size_t v, i;
    for (i = 0; i < input->num_scans; i++) {
        size_t oi = self->crushed->num_scans;
        for (v = 0; v < input->num_vs; v++) {
            size_t ov = v * 2;
            sie_float64 value = input->v[v].float64[i];
            if (self->cur == 0) {
                self->crushed->v[ov].float64[oi] = value;
                self->crushed->v[ov + 1].float64[oi] = value;
            } else {
                self->crushed->v[ov].float64[oi] = 
                    pick(<, self->crushed->v[ov].float64[oi], value);
                self->crushed->v[ov + 1].float64[oi] =
                    pick(>, self->crushed->v[ov + 1].float64[oi], value);
            }
        }

        self->cur++;
        if (self->cur == self->per_scan) {
            for (v = 0; v < self->crushed->num_vs; v++)
                self->crushed->v_guts[v].size++;
            self->crushed->num_scans++;
            self->cur = 0;
        }
            
        if (self->crushed->num_scans == self->max_scans) {
            crush(self);
            self->per_scan *= 2;
        }
    }
}

static void finalize(sie_Plot_Crusher *self)
{
    if (self->crushed && self->cur) {
        size_t v;
        for (v = 0; v < self->crushed->num_vs; v++)
            self->crushed->v_guts[v].size++;
        self->crushed->num_scans++;
    }        
}

int sie_plot_crusher_work(sie_Plot_Crusher *self)
{
    sie_Output *output = sie_spigot_get(self->spigot);
    if (!output) {
        finalize(self);
        return 0;
    }
    if (!self->crushed) {
        size_t v;
        self->crushed = sie_output_new(self, output->num_vs * 2);
        for (v = 0; v < output->num_vs * 2; v++) {
            sie_assertf(output->v[v / 2].type == SIE_OUTPUT_FLOAT64,
                        (self, "Tried to use Plot Crusher on raw data "
                         "(v=%"APR_SIZE_T_FMT")\n",
                         v));
            sie_output_set_type(self->crushed, v, SIE_OUTPUT_FLOAT64);
            sie_output_resize(self->crushed, v, self->max_scans);
        }
    }
    work(self, output);
    return 1;
}

sie_Output *sie_plot_crusher_output(sie_Plot_Crusher *self)
{
    return sie_retain(self->crushed);
}

sie_Output *sie_plot_crusher_finish(sie_Plot_Crusher *self)
{
    while (sie_plot_crusher_work(self))
        /* do nothing */ ;
    return sie_plot_crusher_output(self);
}

SIE_CLASS(sie_Plot_Crusher, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_plot_crusher_destroy));
