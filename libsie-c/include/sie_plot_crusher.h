/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Plot_Crusher sie_Plot_Crusher;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_PLOT_CRUSHER_H
#define SIE_PLOT_CRUSHER_H

struct _sie_Plot_Crusher {
    sie_Context_Object parent;
    sie_Spigot *spigot;
    size_t ideal_scans;
    size_t max_scans;
    size_t cur;
    size_t per_scan;
    sie_Output *crushed;
};
SIE_CLASS_DECL(sie_Plot_Crusher);
#define SIE_PLOT_CRUSHER(p) SIE_SAFE_CAST(p, sie_Plot_Crusher)

SIE_DECLARE(sie_Plot_Crusher *) sie_plot_crusher_new(void *spigot,
                                                     size_t scans);
SIE_DECLARE(void) sie_plot_crusher_init(sie_Plot_Crusher *self,
                                        void *spigot, size_t scans);
SIE_DECLARE(void) sie_plot_crusher_destroy(sie_Plot_Crusher *self);

SIE_DECLARE(int) sie_plot_crusher_work(sie_Plot_Crusher *self);
SIE_DECLARE(sie_Output *) sie_plot_crusher_output(sie_Plot_Crusher *self);
SIE_DECLARE(sie_Output *) sie_plot_crusher_finish(sie_Plot_Crusher *self);

#endif

#endif
