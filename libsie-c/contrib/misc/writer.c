#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sie_internal.h"

static const double pi = 3.1415926535897932384626;

static const char * const xml_sections[] = {
    " <decoder id=\"2\">\n"
    "  <loop var=\"v0\">\n"
    "   <read var=\"v1\" bits=\"16\" type=\"int\" endian=\"big\"/>\n"
    "   <sample/>\n"
    "  </loop>\n"
    " </decoder>\n"
    "\n"
    " <test id=\"0\">\n"
    "  <ch id=\"0\" name=\"", /* ____ */ "\" group=\"2\">\n"
    "   <tag id=\"core:schema\">somat:sequential</tag>\n"
    "   <tag id=\"core:output_samples\">", /* ____ */ "</tag>\n"
    "   <tag id=\"core:sample_rate\">", /* ____ */ "</tag>\n"
    "   <dim index=\"0\">\n"
    "    <tag id=\"core:label\">time</tag>\n"
    "    <tag id=\"core:units\">seconds</tag>\n"
    "    <data decoder=\"2\" v=\"0\"/>\n"
    "    <xform scale=\"", /* ____ */ "\" offset=\"0\"/>\n"
    "   </dim>\n"
    "   <dim index=\"1\">\n"
    "    <tag id=\"core:label\">measurement</tag>\n"
    "    <tag id=\"core:units\">volts</tag>\n"
    "    <data decoder=\"2\" v=\"1\"/>\n"
    "    <xform scale=\"", /* ____ */ "\" offset=\"", /* ____ */ "\"/>\n"
    "   </dim>\n"
    "  </ch>\n"
    " </test>\n"
};

static const int num_xml_sections = 
    sizeof(xml_sections) / sizeof(*xml_sections);

sie_Writer *writer;

static size_t write_block(void *file, const char *data, size_t size)
{
    if (!fwrite(data, size, 1, file))
        return 0;
    return size;
}

static void output_metadata(const char *buf, uint32_t len)
{
    sie_writer_xml_string(writer, buf, len);
}

static void output_xml(int num)
{
    uint32_t len;

    assert(num < num_xml_sections);
    len = strlen(xml_sections[num]);
    output_metadata(xml_sections[num], len);
}

static void output_sie(const char *filename, const char *channame,
                       double rate, double period, int samples)
{
    FILE *out;
    char buf[32];
    int i;
    short foobar[1024];
    sie_Context *ctx;

    ctx = sie_context_new();
    out = fopen(filename, "wb");
    writer = sie_writer_new(ctx, write_block, out);
    assert(ctx && out && writer);

    sie_writer_xml_header(writer);

    output_xml(0);
    output_metadata(channame, strlen(channame));
    output_xml(1);
    sprintf(buf, "%d", samples);
    output_metadata(buf, strlen(buf));
    output_xml(2);
    sprintf(buf, "%f", rate);
    output_metadata(buf, strlen(buf));
    output_xml(3);
    sprintf(buf, "%f", 1.0 / rate);
    output_metadata(buf, strlen(buf));
    output_xml(4);
    sprintf(buf, "%f", 0.001);
    output_metadata(buf, strlen(buf));
    output_xml(5);
    sprintf(buf, "%f", 0.0);
    output_metadata(buf, strlen(buf));
    output_xml(6);

    sie_writer_flush_xml(writer);

    for (i = 0; i < 1024; ++i)
        foobar[i] = i;
    for (i = 0; i < 16; ++i)
        sie_writer_write_block(writer, 2, (char *)foobar, sizeof(foobar));

    sie_release(writer);
    fclose(out);
    sie_context_done(ctx);
}

int main(int argc, char **argv)
{
    output_sie(argv[1], argv[2], atof(argv[3]), atof(argv[4]), atoi(argv[5]));
    return 0;
}

