/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_group.h"

void sie_group_init(sie_Group *self, void *intake, sie_id group)
{
    sie_ref_init(SIE_REF(self), intake);
    self->intake = intake;
    sie_retain(self->intake);
    self->group = group;
}

void sie_group_destroy(sie_Group *self)
{
    sie_release(self->intake);
    sie_ref_destroy(SIE_REF(self));
}

sie_Spigot *sie_group_attach_spigot(sie_Group *self)
{
    return SIE_SPIGOT(sie_group_spigot_new(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_group_new, sie_Group, self, intake,
                          (void *intake, sie_id group),
                          sie_group_init(self, intake, group));

SIE_CLASS(sie_Group, sie_Ref,
          SIE_MDEF(sie_destroy, sie_group_destroy)
          SIE_MDEF(sie_attach_spigot, sie_group_attach_spigot));



void sie_group_spigot_init(sie_Group_Spigot *self, sie_Group *group)
{
    sie_spigot_init(SIE_SPIGOT(self), group);
    self->group = group;
    sie_retain(self->group);
    self->output = sie_output_new(self, 1);
    sie_output_set_type(self->output, 0, SIE_OUTPUT_RAW);
}

void sie_group_spigot_destroy(sie_Group_Spigot *self)
{
    sie_release(self->output);
    sie_release(self->block);
    sie_release(self->group);
    sie_spigot_destroy(SIE_SPIGOT(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_group_spigot_new, sie_Group_Spigot,
                          self, group, (sie_Group *group),
                          sie_group_spigot_init(self, group));

static void *get_group_handle(sie_Group_Spigot *self)
{
    if (!self->group_handle)
        self->group_handle =
            sie_get_group_handle(self->group->intake, self->group->group);
    return self->group_handle;
}

sie_Output *sie_group_spigot_get_inner(sie_Group_Spigot *self)
{
    void *group_handle = get_group_handle(self);
    sie_debug((self, 10, "sie_group_spigot_get_inner, "
               "entry = %"APR_SIZE_T_FMT"\n", self->entry));

    if (!group_handle)
        return NULL;
    if (self->entry >= sie_get_group_num_blocks(self->group->intake,
                                                group_handle))
        return NULL;

    if (!self->block)
        self->block = sie_block_new(self);
    self->output = sie_output_maybe_reuse(self->output);
    sie_read_group_block(self->group->intake, group_handle,
                         self->entry, self->block);
    sie_output_grow_to(self->output, 0, 1);
    sie_output_set_raw(self->output, 0, 0,
                       self->block->data->payload, self->block->size);
    self->output->v_guts[0].size = 1;
    self->output->num_scans = 1;
    self->output->block = self->entry;
    self->entry++;

    return self->output;
}

void sie_group_spigot_clear_output(sie_Group_Spigot *self)
{
    sie_release(self->block);
    self->block = NULL;
    self->output = sie_output_maybe_reuse(self->output);
    sie_output_clear_and_shrink(self->output);
}

size_t sie_group_spigot_seek(sie_Group_Spigot *self, size_t target)
{
    void *group_handle = get_group_handle(self);
    if (group_handle) {
        size_t end =
            sie_get_group_num_blocks(self->group->intake, group_handle);
        self->entry = (target > end) ? end : target;
        return self->entry;
    } else {
        return 0;
    }
}

size_t sie_group_spigot_tell(sie_Group_Spigot *self)
{
    return self->entry;
}

int sie_group_spigot_done(sie_Group_Spigot *self)
{
    void *group_handle = get_group_handle(self);
    if (group_handle) {
        size_t end =
            sie_get_group_num_blocks(self->group->intake, group_handle);
        if (self->entry >= end)
            return sie_is_group_closed(self->group->intake, group_handle);
        else
            return 0;
    } else {
        return sie_is_group_closed(self->group->intake, group_handle);
    }
}

SIE_CLASS(sie_Group_Spigot, sie_Spigot,
          SIE_MDEF(sie_destroy, sie_group_spigot_destroy)
          SIE_MDEF(sie_spigot_get_inner, sie_group_spigot_get_inner)
          SIE_MDEF(sie_spigot_clear_output, sie_group_spigot_clear_output)
          SIE_MDEF(sie_spigot_seek, sie_group_spigot_seek)
          SIE_MDEF(sie_spigot_tell, sie_group_spigot_tell)
          SIE_MDEF(sie_spigot_done, sie_group_spigot_done));
