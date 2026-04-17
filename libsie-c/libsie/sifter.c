/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

const static struct {
    const char *element;
    const char *attribute;
    sie_Sifter_Map_Type type;
} remappings[] = {
    { "ch", "base", SIE_SIFTER_CH },
    { "ch", "group", SIE_SIFTER_GROUP },
    { "ch", "id", SIE_SIFTER_CH },
    { "data", "decoder", SIE_SIFTER_DECODER },
    { "decoder", "id", SIE_SIFTER_DECODER },
    { "dim", "group", SIE_SIFTER_GROUP },
    { "tag", "decoder", SIE_SIFTER_DECODER },
    { "tag", "group", SIE_SIFTER_GROUP },
    { "xform", "index_ch", SIE_SIFTER_CH },
};
const static size_t num_remappings = sizeof(remappings) / sizeof(*remappings);

SIE_CONTEXT_OBJECT_API_NEW_FN(sie_sifter_new, sie_Sifter, self, writer,
                              (sie_Writer *writer),
                              sie_sifter_init(self, writer));

static sie_String *str(sie_Sifter *self, const char *str)
{
    return sie_string_get(self, str, strlen(str));
}

static void test_sig(sie_Sifter *self, sie_Test *test, void *user);

void sie_sifter_init(sie_Sifter *self, sie_Writer *writer)
{
    size_t i;
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), writer);
    self->test_sig_fn = test_sig;
    self->writer = sie_retain(writer);
    self->remappings = sie_malloc(self, sizeof(*self->remappings) * num_remappings);
    for (i = 0; i < num_remappings; ++i) {
        self->remappings[i].element = str(self, remappings[i].element);
        self->remappings[i].attribute = str(self, remappings[i].attribute);
        self->remappings[i].type = remappings[i].type;
    }
}

static sie_id next_id(sie_Sifter *self, sie_Sifter_Map_Type type)
{
    switch (type) {
    case SIE_SIFTER_GROUP:
        return sie_writer_next_id(self->writer, SIE_WRITER_ID_GROUP);
    case SIE_SIFTER_TEST_BY_SIG:
        return sie_writer_next_id(self->writer, SIE_WRITER_ID_TEST);
    case SIE_SIFTER_CH:
        return sie_writer_next_id(self->writer, SIE_WRITER_ID_CH);
    case SIE_SIFTER_DECODER_BY_SIG:
        return sie_writer_next_id(self->writer, SIE_WRITER_ID_DECODER);
    default:
        sie_errorf((self, "Bad type: %d", type));
    }
    return SIE_NULL_ID; /* not reached */
}

void sie_sifter_destroy(sie_Sifter *self)
{
    size_t i;
    int type;
    for (i = 0; i < num_remappings; ++i) {
        sie_release(self->remappings[i].element);
        sie_release(self->remappings[i].attribute);
    }
    free(self->remappings);
    sie_sifter_finish(self);
    for (type = 0; type < SIE_SIFTER_NUM_TYPES; ++type) {
        sie_Sifter_Map_Entry *map, *next;
        for (map = self->maps[type]; map; map = next) {
            if (type == SIE_SIFTER_GROUP) {
                sie_Sifter_Map_Key *key = map->key;
                sie_release(key->intake);
            }
            next = map->hh.next;
            HASH_DELETE(hh, self->maps[type], map);
            free(map->key);
            free(map);
        }
    }
    sie_release(self->writer);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

static sie_Sifter_Map_Entry *find_entry(sie_Sifter *self,
                                        sie_Sifter_Map_Type type,
                                        const void *key, size_t key_size)
{
    sie_Sifter_Map_Entry *entry;
    HASH_FIND(hh, self->maps[type], key, (int)key_size, entry);
    return entry;
}

static sie_id find_id(sie_Sifter *self, sie_Sifter_Map_Type type,
                      const void *key, size_t key_size)
{
    sie_Sifter_Map_Entry *entry = find_entry(self, type, key, key_size);
    return entry ? entry->id : SIE_NULL_ID;
}

static sie_Sifter_Map_Entry *set_entry(sie_Sifter *self,
                                       sie_Sifter_Map_Type type,
                                       const void *key, size_t key_size,
                                       sie_id id)
{
    sie_Sifter_Map_Entry *entry = sie_calloc(self, sizeof(*entry));
    entry->key = sie_malloc(self, key_size);
    memcpy(entry->key, key, key_size);
    entry->key_size = key_size;
    entry->id = id;
    entry->start_block = ~entry->start_block;
    HASH_ADD_KEYPTR(hh, self->maps[type], entry->key,
                    (int)entry->key_size, entry);
    return entry;
}

static sie_Sifter_Map_Entry *map_entry(sie_Sifter *self,
                                       sie_Sifter_Map_Type type,
                                       const void *key, size_t key_size)
{
    sie_Sifter_Map_Entry *entry = find_entry(self, type, key, key_size);
    if (!entry)
        entry = set_entry(self, type, key, key_size, next_id(self, type));
    return entry;
}

static sie_id map_id(sie_Sifter *self, sie_Sifter_Map_Type type,
                     const void *key, size_t key_size)
{
    sie_Sifter_Map_Entry *entry = map_entry(self, type, key, key_size);
    return entry ? entry->id : SIE_NULL_ID;
}

/* KLUDGE to make sure that there is no uninitialized data, as this
 * gets hashed. */
#define STD_KEY(key, in, id)                    \
    sie_Sifter_Map_Key key;                     \
    memset(&key, 0, sizeof(key));               \
    key.intake = (in);                          \
    key.from_id = (id)

static sie_id find_id_std(sie_Sifter *self, sie_Sifter_Map_Type type,
                          sie_Intake *intake, sie_id id)
{
    STD_KEY(key, intake, id);
    return find_id(self, type, &key, sizeof(key));
}

static sie_id set_id_std(sie_Sifter *self, sie_Sifter_Map_Type type,
                         sie_Intake *intake, sie_id id, sie_id to_id)
{
    STD_KEY(key, intake, id);
    return set_entry(self, type, &key, sizeof(key), to_id)->id;
}

static sie_id map_id_std(sie_Sifter *self, sie_Sifter_Map_Type type,
                         sie_Intake *intake, sie_id id)
{
    STD_KEY(key, intake, id);
    return map_id(self, type, &key, sizeof(key));
}

static sie_id map_id_group_start_end(sie_Sifter *self, sie_Intake *intake,
                                     sie_id id,
                                     size_t start_block, size_t end_block)
{
    sie_Sifter_Map_Entry *entry;
    STD_KEY(key, intake, id);
    if (find_id_std(self, SIE_SIFTER_GROUP, intake, id) == SIE_NULL_ID)
        sie_retain(intake);
    entry = map_entry(self, SIE_SIFTER_GROUP, &key, sizeof(key));
    if (start_block < entry->start_block)
        entry->start_block = start_block;
    if (end_block > entry->end_block)
        entry->end_block = end_block;
    return entry->id;
}

static sie_id map_id_group(sie_Sifter *self, sie_Intake *intake, sie_id id)
{
    return map_id_group_start_end(self, intake, id, 0, ~(size_t)0);
}

static sie_id map_test_id(sie_Sifter *self, sie_Test *test)
{
    sie_id id = find_id_std(self, SIE_SIFTER_TEST, test->intake, test->id);
    if (id != SIE_NULL_ID)
        return id;
    self->test_sig_fn(self, test, self->user);
    return find_id_std(self, SIE_SIFTER_TEST, test->intake, test->id);
}

void sie_sifter_test_sig_fn(sie_Sifter *self,
                            sie_Sifter_Test_Sig_Fn *fn,
                            void *user)
{
    self->test_sig_fn = fn ? fn : test_sig;
    self->user = user;
}

void sie_sifter_register_test(sie_Sifter *self, sie_Test *test,
                              const void *key, size_t key_size)
{
    sie_id id = map_id(self, SIE_SIFTER_TEST_BY_SIG, key, key_size);
    if (find_id_std(self, SIE_SIFTER_TEST,
                    test->intake, test->id) == SIE_NULL_ID)
        set_id_std(self, SIE_SIFTER_TEST, test->intake, test->id, id);
}

static void test_sig(sie_Sifter *self, sie_Test *test, void *user)
{
    STD_KEY(key, test->intake, test->id);
    sie_sifter_register_test(self, test, &key, sizeof(key));
}

static void xml_set_attribute_id(sie_XML *node, const char *name, sie_id id)
{
    char id_s[20];
    sprintf(id_s, "%u", id);
    sie_xml_set_attribute(node, name, id_s);
}

static void remap_xml(sie_Sifter *self, sie_Intake *intake, sie_XML *top)
{
    sie_XML *cur = top;
    do {
        size_t a;
        if (cur->type != SIE_XML_ELEMENT)
            continue;
        for (a = 0; a < cur->value.element.num_attrs; ++a) {
            size_t r;
            for (r = 0; r < num_remappings; ++r) {
                sie_id from_id, id;
                sie_XML_Element *el = &cur->value.element;
                if (!el->name || !el->attrs[a].name ||
                    (self->remappings[r].element != el->name) ||
                    (self->remappings[r].attribute != el->attrs[a].name))
                    continue;
                from_id =
                    sie_strtoid(self, sie_string_value(el->attrs[a].value));
                id = find_id_std(self, self->remappings[r].type,
                                 intake, from_id);
                sie_assert(id != SIE_NULL_ID, self);
                xml_set_attribute_id(cur, sie_string_value(el->attrs[a].name),
                                     id);
            }
        }
    } while ( (cur = sie_xml_walk_next(cur, top, SIE_XML_DESCEND)) );
}

static sie_id map_decoder_id(sie_Sifter *self, sie_Intake *intake,
                             sie_id from_id)
{
    sie_Decoder *decoder =
        sie_id_map_get(intake->xml->compiled_decoder_map, from_id);
    sie_id id;
    unsigned char *buf;
    size_t len;
    sie_decoder_sigbuf(decoder, &buf, &len);
    id = find_id(self, SIE_SIFTER_DECODER_BY_SIG, (char *)buf, len);
    if (id == SIE_NULL_ID) {
        sie_XML *node = sie_id_map_get(intake->xml->decoder_map,
                                       from_id);
        sie_XML *copy = sie_xml_pack(node);
        id = map_id(self, SIE_SIFTER_DECODER_BY_SIG, (char *)buf, len);
        set_id_std(self, SIE_SIFTER_DECODER, intake, from_id, id);
        remap_xml(self, intake, copy);
        sie_writer_xml_node(self->writer, copy);
        sie_release(copy);
    }
    free(buf);
    return id;
}

int sie_sifter_add_channel(sie_Sifter *self, void *ref,
                           size_t start_block, size_t end_block)
{
    sie_Channel *channel = ref;
    if (find_id_std(self, SIE_SIFTER_CH,
                    channel->intake, channel->id) == SIE_NULL_ID) {
        sie_Test *test = sie_get_containing_test(channel);
        sie_XML *node = channel->raw_xml;
        sie_XML *copy = sie_xml_pack(node);
        sie_Iterator *iter = sie_get_dimensions(channel);
        sie_Dimension *dim;
        sie_Tag *tag;
        sie_Iterator *tag_iter;
        const char *base_s = sie_xml_get_attribute(node, "base");

        if (base_s) {
            sie_Channel *base = sie_get_channel(channel->intake,
                                                sie_strtoid(self, base_s));
            sie_sifter_add(self, base);
            sie_release(base);
        }

        if (channel->toplevel_group != SIE_NULL_ID)
            map_id_group_start_end(self,
                                   channel->intake, channel->toplevel_group,
                                   start_block, end_block);

        tag_iter = sie_get_tags(channel);
        while ( (tag = sie_iterator_next(tag_iter)) ) {
            if (tag->group != SIE_NULL_ID)
                map_id_group(self, tag->intake, tag->group);
        }
        sie_release(tag_iter);

        while ( (dim = sie_iterator_next(iter)) ) {
            if (dim->xform_node) {
                const char *index_ch_s =
                    sie_xml_get_attribute(dim->xform_node, "index_ch");
                if (index_ch_s) {
                    sie_Channel *sub =
                        sie_get_channel(channel->intake,
                                        sie_strtoid(self, index_ch_s));
                    sie_sifter_add(self, sub);
                    sie_release(sub);
                }
            }
            if (dim->group != SIE_NULL_ID)
                map_id_group_start_end(self, channel->intake, dim->group,
                                       start_block, end_block);
            if (dim->decoder_id != SIE_NULL_ID)
                map_decoder_id(self, channel->intake, dim->decoder_id);

            tag_iter = sie_get_tags(dim);
            while ( (tag = sie_iterator_next(tag_iter)) ) {
                if (tag->group != SIE_NULL_ID)
                    map_id_group(self, tag->intake, tag->group);
            }
            sie_release(tag_iter);
        }

        map_id_std(self, SIE_SIFTER_CH, channel->intake, channel->id);

        remap_xml(self, channel->intake, copy);

        if (test) {
            sie_id id = map_test_id(self, test);
            xml_set_attribute_id(copy, "test", id);
        }

        sie_writer_xml_node(self->writer, copy);
        sie_release(copy);
        sie_release(iter);
        sie_release(test);
    }
    return 1; /* KLUDGE */
}

int sie_sifter_add(sie_Sifter *self, void *ref)
{
    sie_Class *ref_class;
    sie_assert(ref, self);
    ref_class = sie_class_of(ref);

    if (ref_class == SIE_CLASS_FOR_TYPE(sie_Tag)) {
        sie_Tag *tag = ref;
        sie_XML *node = tag->node;
        sie_XML *parent = node->parent;
        sie_XML *copy = sie_xml_pack(node);

        sie_assert(parent, self);
        if (parent->value.element.name == sie_literal(self, test)) {
            sie_String *id_s = sie_xml_get_attribute_literal(parent, id);
            sie_id from_id = sie_strtoid(self, sie_string_value(id_s));
            sie_Test *test = sie_get_test(tag->intake, from_id);
            sie_id id = map_test_id(self, test);
            xml_set_attribute_id(copy, "test", id);
            sie_release(test);
        } else if (parent->value.element.name == sie_literal(self, sie)) {
            if (!strcmp(sie_tag_get_id(tag), "sie:xml_metadata") ||
                !strcmp(sie_tag_get_id(tag), "sie:block_index")) {
                sie_release(copy);
                return 1; /* KLUDGE */
            }
        } else {
            sie_errorf((self, "Tried to sift non-file-or-test tag"));
        }

        if (tag->group != SIE_NULL_ID)
            map_id_group(self, tag->intake, tag->group);

        remap_xml(self, tag->intake, copy);
        sie_writer_xml_node(self->writer, copy);
        sie_release(copy);
    } else if (ref_class == SIE_CLASS_FOR_TYPE(sie_Test)) {
        sie_Test *test = ref;
        sie_XML *copy = sie_xml_new_element(self, "test");
        sie_id id = map_test_id(self, test);
        xml_set_attribute_id(copy, "test", id);
        sie_writer_xml_node(self->writer, copy);
        sie_release(copy);
    } else if (ref_class == SIE_CLASS_FOR_TYPE(sie_Channel)) {
        sie_sifter_add_channel(self, ref, 0, ~(size_t)0);
    }
    return 1; /* KLUDGE */
}

sie_uint64 sie_sifter_total_size(sie_Sifter *self)
{
    sie_uint64 num_blocks = 0;
    sie_uint64 num_bytes = 0;
    sie_Sifter_Map_Entry *entry;
    for (entry = self->maps[SIE_SIFTER_GROUP]; entry; entry = entry->hh.next) {
        sie_Sifter_Map_Key *key = entry->key;
        void *gh = sie_get_group_handle(key->intake, key->from_id);
        if (gh) {
            size_t nb = sie_get_group_num_blocks(key->intake, gh);
            if (entry->start_block > 0 || entry->end_block < nb) {
                size_t end_block =
                    entry->end_block < nb ? entry->end_block : nb;
                size_t i;
                num_blocks += end_block - entry->start_block;
                for (i = entry->start_block; i < end_block; ++i) {
                    num_bytes += sie_get_group_block_size(key->intake, gh, i);
                }
            } else {
                num_blocks += nb;
                num_bytes += sie_get_group_num_bytes(key->intake, gh);
            }
        }
    }
    return sie_writer_total_size(self->writer, num_bytes, num_blocks);
}

void sie_sifter_finish(sie_Sifter *self)
{
    sie_Sifter_Map_Entry *entry;
    sie_writer_flush_xml(self->writer);
    for (entry = self->maps[SIE_SIFTER_GROUP]; entry; entry = entry->hh.next) {
        sie_Sifter_Map_Key *key = entry->key;
        size_t block = entry->start_block;
        sie_Group *group = sie_group_new(key->intake, key->from_id);
        sie_Spigot *spigot = sie_attach_spigot(group);
        sie_Output *output;
        sie_spigot_seek(spigot, block);
        while ( block < entry->end_block &&
                (output = sie_spigot_get(spigot)) ) {
            sie_writer_write_block(self->writer, entry->id,
                                   output->v[0].raw[0].ptr,
                                   output->v[0].raw[0].size);
            ++block;
        }
        sie_release(spigot);
        sie_release(group);
    }
}

SIE_CLASS(sie_Sifter, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_sifter_destroy));
