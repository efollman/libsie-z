/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Group sie_Group;
typedef struct _sie_Group_Spigot sie_Group_Spigot;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_GROUP_H
#define SIE_GROUP_H

struct _sie_Group {
    sie_Ref parent;
    sie_Intake *intake;
    sie_id group;
};
SIE_CLASS_DECL(sie_Group);
#define SIE_GROUP(p) SIE_SAFE_CAST(p, sie_Group)

SIE_DECLARE(sie_Group *) sie_group_new(void *intake, sie_id group);
SIE_DECLARE(void) sie_group_init(sie_Group *self, void *intake, sie_id group);
SIE_DECLARE(void) sie_group_destroy(sie_Group *self);

sie_Spigot *sie_group_attach_spigot(sie_Group *self);

struct _sie_Group_Spigot {
    sie_Spigot parent;
    sie_Group *group;
    sie_Block *block;
    sie_Output *output;
    void *group_handle;
    size_t entry;
};
SIE_CLASS_DECL(sie_Group_Spigot);
#define SIE_GROUP_SPIGOT(p) SIE_SAFE_CAST(p, sie_Group_Spigot);

SIE_DECLARE(sie_Group_Spigot *) sie_group_spigot_new(sie_Group *group);
SIE_DECLARE(void) sie_group_spigot_init(sie_Group_Spigot *self,
                                        sie_Group *group);
SIE_DECLARE(void) sie_group_spigot_destroy(sie_Group_Spigot *self);

SIE_DECLARE(sie_Output *) sie_group_spigot_get_inner(sie_Group_Spigot *self);

SIE_DECLARE(void) sie_group_spigot_clear_output(sie_Group_Spigot *self);

SIE_DECLARE(size_t) sie_group_spigot_seek(sie_Group_Spigot *self,
                                          size_t target);
SIE_DECLARE(size_t) sie_group_spigot_tell(sie_Group_Spigot *self);

#endif

#endif
