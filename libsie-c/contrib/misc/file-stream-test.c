/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "sie_internal.h" /* sie_File_Stream is not an exposed interface yet */

/* KLUDGE - library-internal routine for data output */
void sie_output_dump(sie_Output *output, FILE *stream);

struct stream_data {
    size_t num;
    size_t max;
    CURL *handle;
    sie_Context *ctx;
    void *stream;
    sie_Channel **channels;
    sie_Spigot **spigots;
};

static void grow(struct stream_data *sd, size_t to)
{
    size_t old_max = sd->max;
    if (to <= sd->max)
        return;
    sd->max = to * 2;
    sd->channels = realloc(sd->channels, sizeof(*sd->channels) * sd->max);
    memset(sd->channels + old_max, 0,
           (sd->max - old_max) * sizeof(*sd->channels));
    sd->spigots = realloc(sd->spigots, sizeof(*sd->spigots) * sd->max);
    memset(sd->spigots + old_max, 0,
           (sd->max - old_max) * sizeof(*sd->spigots));
}

static size_t data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    struct stream_data *sd = userp;
    long code;
    size_t ret;
    size_t i;

    size *= nmemb;
    curl_easy_getinfo(sd->handle, CURLINFO_RESPONSE_CODE, &code);
    printf("code %ld buffer %p size %lu\n", code, buffer, size);
    if (size == 0 || code != 200)
        return size;
    ret = sie_add_stream_data(sd->stream, buffer, size);
    printf("add_stream_data returned %lu\n", ret);
    if (!ret) {
        printf("%s\n", sie_verbose_report(sie_get_exception(sd->stream)));
        exit(1);
    }
    for (;;) {
        i = sd->num;
        grow(sd, i + 1);
        if (!sd->spigots[i]) {
            if (!sd->channels[i])
                sd->channels[i] = sie_get_channel(sd->stream, i);
            if (sd->channels[i])
                sd->spigots[i] = sie_attach_spigot(sd->channels[i]);
            if (sd->spigots[i])
                ++sd->num;
        }
        printf("i = %lu, channels[i] = %p, spigots[i] = %p\n",
               i, sd->channels[i], sd->spigots[i]);
        if (i == sd->num)
            break;
    }
    for (i = 0; i < sd->num; i++) {
        sie_Output *output;
        if (!sd->spigots[i])
            continue;
        printf("Channel %s:\n", sie_get_name(sd->channels[i]));
        while ((output = sie_spigot_get(sd->spigots[i])))
            sie_output_dump(output, stdout);
        if (sie_spigot_done(sd->spigots[i])) {
            printf("Channel %s is done.\n", sie_get_name(sd->channels[i]));
            sie_release(sd->channels[i]);
            sie_release(sd->spigots[i]);
            sd->channels[i] = NULL;
            sd->spigots[i] = NULL;
        }
    }
    return size;
}

int main(int argc, char *argv[])
{
    struct stream_data sd;
    size_t i;
    char url[16384];            /* KLUDGE */
    char *urlp = url;
    if (argc < 2) {
        fprintf(stderr, "usage: %s edaq [-o outfile.sie] [channel...]\n",
                argv[0]);
        return 1;
    }
    memset(&sd, 0, sizeof(sd));
    sd.handle = curl_easy_init();
    sd.ctx = sie_context_new();
    urlp += sprintf(urlp, "http://%s/-/test/_DEFAULT_/data/stream.sie",
                    argv[1]);
    argc -= 2;
    argv += 2;
    if (argc >= 2 && !strcmp(argv[0], "-o")) {
        sd.stream = sie_file_stream_new(sd.ctx, argv[1]);
        argc -= 2;
        argv += 2;
    } else {
        sd.stream = sie_stream_new(sd.ctx);
    }
    if (!sd.stream) {
        printf("%s\n", sie_verbose_report(sie_get_exception(sd.ctx)));
        exit(1);
    }
    if (argc) {
        urlp += sprintf(urlp, "?channels=");
        for (i = 0; i < argc; i++)
            urlp += sprintf(urlp, "%s%s", i > 0 ? "," : "", argv[i]);
    }
    curl_easy_setopt(sd.handle, CURLOPT_URL, url);
    curl_easy_setopt(sd.handle, CURLOPT_WRITEFUNCTION, data);
    curl_easy_setopt(sd.handle, CURLOPT_WRITEDATA, &sd);
    curl_easy_perform(sd.handle);
    curl_easy_cleanup(sd.handle);
    for (i = 0; i < sd.num; i++)
        sie_release(sd.spigots[i]);
    for (i = 0; i < sd.num; i++)
        sie_release(sd.channels[i]);
    free(sd.spigots);
    free(sd.channels);
    sie_release(sd.stream);
    printf("%d leaks\n", sie_context_done(sd.ctx));
    return 0;
}
