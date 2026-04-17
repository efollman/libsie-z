#include "sie_internal.h"

#define BUFSIZE 65536

void usage(void)
{
    fprintf(stderr,
            "Usage:\n"
            "  sie-attach <sie-file> -attach <file> <name>\n"
            "  sie-attach <sie-file> -list\n"
            "  sie-attach <sie-file> -extract <name> <file>\n");
    exit(1);
}

static size_t write_block(void *file, const char *data, size_t size)
{
    if (!fwrite(data, size, 1, file))
        return 0;
    return size;
}

char *arg(int argc, char **argv, int n)
{
    if (n >= argc)
        usage();
    return argv[n];
}

int main(int argc, char **argv)
{
    char *filename = arg(argc, argv, 1);
    sie_Context *ctx = sie_context_new();
    sie_File *file = sie_file_open(ctx, filename);
    char *buf = malloc(BUFSIZE);

    if (!strcmp(arg(argc, argv, 2), "-attach")) {
        FILE *handle = fopen(filename, "ab");
        sie_Writer *writer = sie_writer_new(ctx, write_block, handle);
        FILE *attachment = fopen(arg(argc, argv, 3), "rb");
        sie_XML *tag = sie_xml_new_element(ctx, "tag");
        sie_id next;
        size_t got;
        sie_writer_prepare_append(writer, file);
        next = sie_writer_next_id(writer, SIE_WRITER_ID_GROUP);
        sprintf(buf, "attachment:%s", arg(argc, argv, 4));
        sie_xml_set_attribute(tag, "id", buf);
        sprintf(buf, "%u", next);
        sie_xml_set_attribute(tag, "group", buf);
        sie_writer_xml_node(writer, tag);
        sie_release(tag);
        sie_writer_flush_xml(writer);
        while ((got = fread(buf, 1, BUFSIZE, attachment)))
            sie_writer_write_block(writer, next, buf, got);
        fclose(attachment);
        sie_release(writer);
        fclose(handle);
    } else if (!strcmp(arg(argc, argv, 2), "-list")) {
        sie_Iterator *tags = sie_get_tags(file);
        sie_Tag *tag;
        while ((tag = sie_iterator_next(tags))) {
            const char *id = sie_tag_get_id(tag);
            if (!strncmp(id, "attachment:", 11)) {
                printf("%s\n", id + 11);
            }
        }
        sie_release(tags);
    } else if (!strcmp(arg(argc, argv, 2), "-extract")) {
        FILE *attachment = fopen(arg(argc, argv, 4), "wb");
        sie_Tag *tag;
        sie_Spigot *spigot;
        sie_Output *output;
        sie_Output_Struct *os;
        sprintf(buf, "attachment:%s", arg(argc, argv, 3));
        tag = sie_get_tag(file, buf);
        spigot = sie_attach_spigot(tag);
        while ((output = sie_spigot_get(spigot))) {
            os = sie_output_get_struct(output);
            fwrite(os->dim[0].raw[0].ptr, 1, os->dim[0].raw[0].size,
                   attachment);
        }
        sie_release(tag);
        sie_release(spigot);
        fclose(attachment);
    }

    free(buf);
    sie_release(file);
    sie_context_done(ctx);

    return 0;
}
