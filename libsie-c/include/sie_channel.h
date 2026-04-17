/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Channel sie_Channel;
typedef struct _sie_Channel_Spigot sie_Channel_Spigot;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_CHANNEL_H
#define SIE_CHANNEL_H

struct _sie_Channel {
    sie_Ref parent;
    sie_XML *raw_xml;
    sie_XML *expanded_xml;
    sie_Intake *intake;
    sie_id id;
    sie_String *name;
    sie_id toplevel_group;
    sie_Dimension **dimensions;
};
SIE_CLASS_DECL(sie_Channel);
#define SIE_CHANNEL(p) SIE_SAFE_CAST(p, sie_Channel)

SIE_DECLARE(sie_Channel *) sie_channel_new(void *intake, sie_id id);
SIE_DECLARE(void) sie_channel_init(sie_Channel *self, void *intake, sie_id id);
SIE_DECLARE(void) sie_channel_destroy(sie_Channel *self);

SIE_DECLARE(sie_Spigot *) sie_channel_attach_spigot(sie_Channel *self);
SIE_DECLARE(void) sie_channel_dump(sie_Channel *self, FILE *stream);

SIE_DECLARE(sie_Iterator *) sie_channel_get_tags(sie_Channel *self);
SIE_DECLARE(sie_Iterator *) sie_channel_get_dimensions(sie_Channel *self);
SIE_DECLARE(sie_Dimension *) sie_channel_get_dimension(sie_Channel *self,
                                                       sie_id index);
SIE_DECLARE(sie_String *) sie_channel_get_name_s(sie_Channel *self);
SIE_DECLARE(sie_id) sie_channel_get_id(sie_Channel *self);
SIE_DECLARE(sie_Test *) sie_channel_get_containing_test(sie_Channel *self);


struct _sie_Channel_Spigot {
    sie_Spigot parent;
    sie_id decoder_id;
    sie_Decoder_Machine *machine;
    sie_id group;
    sie_Group *group_ref;
    sie_Spigot *group_spigot;
    sie_Combiner *combiner;
    sie_Transform *transform;
    int transforms_disabled;
};
SIE_CLASS_DECL(sie_Channel_Spigot);
#define SIE_CHANNEL_SPIGOT(p) SIE_SAFE_CAST(p, sie_Channel_Spigot);

SIE_DECLARE(sie_Channel_Spigot *) sie_channel_spigot_new(sie_Channel *channel);
SIE_DECLARE(void) sie_channel_spigot_init(sie_Channel_Spigot *self,
                                          sie_Channel *channel);
SIE_DECLARE(void) sie_channel_spigot_destroy(sie_Channel_Spigot *self);

SIE_DECLARE(sie_Output *) sie_channel_spigot_get_inner(
    sie_Channel_Spigot *self);

SIE_DECLARE(void) sie_channel_spigot_clear_output(sie_Channel_Spigot *self);

SIE_DECLARE(size_t) sie_channel_spigot_seek(sie_Channel_Spigot *self,
                                            size_t target);
SIE_DECLARE(size_t) sie_channel_spigot_tell(sie_Channel_Spigot *self);

SIE_DECLARE(void) sie_channel_spigot_disable_transforms(
    sie_Channel_Spigot *self, int disable);

#endif

#endif
