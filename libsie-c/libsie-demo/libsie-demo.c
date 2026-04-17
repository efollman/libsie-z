/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/* Welcome.  This extremely verbose tutorial code reads all data and
 * metadata out of an SIE file, while demonstrating most of the libsie
 * API.  It is written far more linearly than code like this would
 * usually be to allow a better narrative comment flow with less
 * jumping around.
 *
 * After compiling this program, run it with an SIE file on the
 * command line. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sie.h>

/* First, we'll prototype our functions for this demonstration. */
static void print_sie_file(const char *filename);
static void print_tag(sie_Tag *tag, const char *prefix);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Please enter an SIE file name on the command line.\n");
        return EXIT_FAILURE;
    }

    print_sie_file(argv[1]);

    return EXIT_SUCCESS;
}

/* This function prints an SIE file's entire contents to standard
 * output. */
void print_sie_file(const char *filename)
{
    sie_Context *context;
    sie_File *file;
    sie_Exception *exception;
    sie_Iterator *tag_iterator;
    sie_Iterator *test_iterator;
    sie_Iterator *channel_iterator;
    sie_Iterator *dimension_iterator;
    sie_Tag *tag;
    sie_Test *test;
    sie_Channel *channel;
    sie_Dimension *dimension;
    sie_Spigot *spigot;
    sie_Output *output;
    sie_uint32 id;
    int leaked_objects;

    /* The first step in using libsie is to get a library context.
     * This is used to hold internal library information about such
     * things as buffers and error handling.  Whenever a library
     * context reference is required, either a direct pointer to the
     * context or a pointer to any other object that has been spawned
     * from the context can be used.
     *
     * libsie is thread-safe so long as code referencing a context or
     * objects created from that context is only running in one thread
     * at once.  Objects cannot be shared among contexts. */
    context = sie_context_new();

    /* If we fail to get a context, a problem has occurred at a very
     * low level. */
    if (!context) {
        fprintf(stderr, "Error:  Failed to acquire context!\n");
        exit(EXIT_FAILURE);
    }
    /* However, all functions in the SIE API accept NULL pointers
     * where they normally expect an SIE object, and will simply
     * return an error value (usually NULL, if the function would
     * normally return an object pointer) when it is called with NULL.
     * This means that one can often "cascade" through valid code once
     * an error is reached, and just check it at the end of the
     * process.  (Obviously, non-SIE-object pointers must be checked
     * for NULL before being dereferenced or otherwise used, as is
     * typical in normal C code.) */

    /* Now we'll open the file we're interested in. */
    file = sie_file_open(context, filename);
    /* Notice that we passed the context into sie_file_open.  The SIE
     * file object that is returned references the same context, and
     * can be used to refer to it as an argument to other functions
     * that want a context (with one exception; see below).  For
     * instance, if we had another file to open,
     *     file2 = sie_file_open(context, filename2);
     * and
     *     file2 = sie_file_open(file, filename2);
     * would both do the same thing. */

    /* However, sie_file_open may have failed, due to a non-existent
     * file, a corrupted file, or other reasons.  To demonstrate the
     * error-handling tools, we're going to catch a possible error
     * here, print the exception, and then exit. */
    if (!file) {
        /* Functions that return SIE objects return NULL on failure,
         * so if "file" is NULL, we failed to open the requested file.
         * To see what went wrong, we'll pull the exception out of the
         * library context: */
        exception = sie_get_exception(context);

        /* To get a string describing the exception, use either
         * sie_report(exception) or sie_verbose_report(exception).
         * The verbose version contains, in addition to the exception
         * message, what the library was doing when the exception
         * happened. */
        fprintf(stderr, "Something bad happened:\n  %s\n",
                sie_verbose_report(exception));

        exit(EXIT_FAILURE);
    }

    /* Now we have successfully opened our file. */
    printf("File '%s':\n", filename);

    /* As you may know from reading the libsie library reference or
     * the SIE format document, the generic form of metadata in an SIE
     * file is called a "tag."  This is simply a reference between an
     * arbitrary textual key and an arbitrary value.  Tags can exist
     * at any level in the SIE metadata hierarchy, including at the
     * file level.  Next, we're going to print out the file-level tags
     * in our file.
     *
     * libsie functions that return more than one object do so via an
     * object called an "iterator."  The iterator returns the other
     * objects one at a time when the sie_iterator_next(iterator)
     * method is called.  When the iterator is empty,
     * sie_iterator_next returns NULL instead of an object.
     *
     * To get an iterator containing all of an object's tags, we call
     * sie_get_tags() on the object. */
    tag_iterator = sie_get_tags(file);
    /* Now we'll pull all the tags out of the iterator, one at a
     * time. */
    while ((tag = sie_iterator_next(tag_iterator))) {
        /* Finally, we'll print out the tag with a function
         * "print_tag", defined below.  To follow the narrative, you
         * may wish to go read this function definition now and come
         * back here when you're done. */
        print_tag(tag, "  File tag ");
    }
    /* We're now done with the iterator.  We need to "release" it to
     * tell libsie that we're through with it.  This is done by simply
     * calling: */
    sie_release(tag_iterator);
    /* We didn't release the tag objects we were getting above because
     * objects returned by an iterator are still "owned" by it, and
     * are cleaned up by it as well.  This makes the usual iterating
     * pattern shorter.  If we need to keep iterator-returned objects
     * around after getting the next object out of or releasing the
     * iterator, we simply call sie_retain() on the object.  This is
     * the opposite of sie_release -- it says we're interested in the
     * object and promise to sie_release it when we're done with it.
     * If you're familiar with referencing counting memory management,
     * sie_retain() raises the reference count, and sie_release lowers
     * it(). */

    /* Now let's get all the test runs contained in this SIE file.
     * SIE "tests" are grouped collections of channels.  To get this,
     * we simply do: */
    test_iterator = sie_get_tests(file);
    /* Again, we have an iterator, this time of SIE tests. */
    while ((test = sie_iterator_next(test_iterator))) {
        /* Next, we'll print out the test.
         *
         * Within an SIE file, each test has a numeric ID, which is a
         * unique identifier for that test within that SIE file.  To
         * demonstrate getting this, we'll print the test ID here. */
        id = sie_get_id(test);
        printf("  Test id %lu:\n", (unsigned long)id);

        /* Tests also have tags.  We'll print these just like we did
         * the channel tags above. */
        tag_iterator = sie_get_tags(test);
        while ((tag = sie_iterator_next(tag_iterator)))
            print_tag(tag, "    Test tag ");
        sie_release(tag_iterator);

        /* Finally, tests contain the channels that were collected as
         * part of the test run.  To get these, we follow the familiar
         * pattern: */
        channel_iterator = sie_get_channels(test);
        while ((channel = sie_iterator_next(channel_iterator))) {
            /* Channels have an SIE-internal ID, just like tests.
             * They also may have a name, accessible with
             * sie_get_name. */
            printf("    Channel id %lu, '%s':\n",
                   (unsigned long)sie_get_id(channel), sie_get_name(channel));

            /* Channels also contain tags.  In real code, you'd
             * probably want a function to print groups of tags, but
             * here it's inline so as to not disrupt the narrative. */
            tag_iterator = sie_get_tags(channel);
            while ((tag = sie_iterator_next(tag_iterator)))
                print_tag(tag, "      Channel tag ");
            sie_release(tag_iterator);

            /* Channels contain dimensions, which define an "axis" or
             * "column" of data. */
            dimension_iterator = sie_get_dimensions(channel);
            while ((dimension = sie_iterator_next(dimension_iterator))) {
                /* Dimensions have an "index" -- for instance, for a
                 * typical time series, dimension index 0 is time, and
                 * index 1 is the engineering value of the data. */
                printf("      Dimension index %lu:\n",
                       (unsigned long)sie_get_index(dimension));

                /* Tags... */
                tag_iterator = sie_get_tags(dimension);
                while ((tag = sie_iterator_next(tag_iterator)))
                    print_tag(tag, "        Dimension tag ");
                sie_release(tag_iterator);
            }
            sie_release(dimension_iterator);

            /* Finally, channels can have a "spigot" attached to them
             * to get the data out.
             *
             * libsie presents data as a array or matrix where each
             * column is a dimension as specified above.  In libsie
             * currently, each column can either consist of 64-bit
             * floats or of "raw" octet strings.
             *
             * For example, a time series may look like this:
             * 
             *      dimension 0   dimension 1
             *     ------------- -------------
             *      0.0           0.0
             *      1.0           0.25
             *      2.0           0.5
             *      3.0           0.25
             *      4.0           0.0
             *
             * While all data comes out in this general form, there
             * are multiple ways to interpret the output.  To see
             * which type of data is stored, and how the channel
             * output should be interpreted, look at the channel tag
             * "core:schema".  For the above example, if the time
             * series was generated by an eDAQ, an appropriate schema
             * would be "somat:sequential".  In this case, dimension 0
             * is time and dimension 1 is the data value of the
             * channel.  All numbers come out scaled to their
             * engineering values.  There is documentation describing
             * each SoMat data schema used.
             *
             * However, because interpreting the data is separate from
             * reading it, we can write a single routine to print out
             * any type of data that can be stored in an SIE file.
             *
             * First, as mentioned above, we need to attach a spigot
             * to the channel we want to read data out of: */
            spigot = sie_attach_spigot(channel);

            /* Now, as with iterators, we can pull out sequential
             * sections of the channel's data.  The data is arranged
             * into "blocks" in the SIE file, and we get one block at
             * a time.  The data comes out in an sie_Output object,
             * and we read the spigot with sie_spigot_get().
             *
             * There are spigot operations that allow seeking around
             * to read data blocks out of order, but those are beyond
             * the scope of this demonstration. */
            while ((output = sie_spigot_get(spigot))) {
                sie_Output_Struct *os;
                size_t dim, row, num_dims, num_rows, byte, size;
                unsigned char *uchar_p;

                /* There are several accessor functions to pull
                 * properties out of an sie_Output object. */
                num_dims = sie_output_get_num_dims(output);
                num_rows = sie_output_get_num_rows(output);
                printf("      Data block %lu, %lu dimensions, %lu rows:\n",
                       (unsigned long)sie_output_get_block(output),
                       (unsigned long)num_dims,
                       (unsigned long)num_rows);

                /* libsie offers several ways to get the data out of
                 * the output object.  One way, more suitable for
                 * languages that don't have easy access to C structs,
                 * is to use sie_output_get_float64() and
                 * sie_output_get_raw() to retrieve an array
                 * containing one dimension's data.
                 * sie_output_get_type() returns what kind of data a
                 * column/dimension contains.
                 *
                 * However, in languages that can interpret C structs,
                 * we can pull out one struct that contains all of the
                 * data: */
                os = sie_output_get_struct(output);
                /* Note that this is simply a struct, not an SIE
                 * object, so we can't call sie_release on it.  It is
                 * owned by the output object and will go away when
                 * that object does. */

                /* Now we can iterate through this C struct, printing
                 * all the data. */
                for (row = 0; row < num_rows; row++) {
                    printf("        Row %lu: ", (unsigned long)row);
                    for (dim = 0; dim < num_dims; dim++) {
                        if (dim != 0)
                            printf(", ");
                        switch (os->dim[dim].type) {
                            /* The type can be SIE_OUTPUT_FLOAT64, or
                             * SIE_OUTPUT_RAW, as described above. */
                        case SIE_OUTPUT_FLOAT64:
                            printf("%.15g", os->dim[dim].float64[row]);
                            break;
                        case SIE_OUTPUT_RAW:
                            /* Raw data has three parts:  "ptr", which
                             * is a pointer to the data; "size", which
                             * is the size of the data; and "claimed",
                             * which should be set to 1 if you wish to
                             * keep the data and clean up the memory
                             * associated with it yourself.  If you
                             * don't set the "claimed" field, the raw
                             * data will be cleaned up with the rest
                             * of the output with the output object
                             * goes away. */
                            uchar_p = os->dim[dim].raw[row].ptr;
                            size = os->dim[dim].raw[row].size;
                            if (size > 16) {
                                printf("(raw data of size %lu.)",
                                       (unsigned long)size);
                            } else {
                                for (byte = 0; byte < size; byte++)
                                    printf("%02x", uchar_p[byte]);
                            }
                            break;
                        }
                    }
                    printf("\n");
                }

                /* Note that, just like with iterators, we don't need
                 * to release the output object here -- the spigot
                 * still "owns" it.  Also as with iterators we could
                 * retain it if we wanted. */
            }
            sie_release(spigot);
        }
        sie_release(channel_iterator);
    }
    sie_release(test_iterator);

    /* Finally, we can also skip the test level of the hierarchy, and
     * directly get all the channels in a file.  To do this: */
    channel_iterator = sie_get_channels(file);
    /* Note this is just like getting all the channels out of a test. */
    while ((channel = sie_iterator_next(channel_iterator))) {
        printf("  Channel id %lu, '%s' ",
               (unsigned long)sie_get_id(channel), sie_get_name(channel));

        /* Also, we can go "backwards" and get the test that contains
         * a channel.  Note that not all channels must be contained by
         * tests (though most containing actual user data will). */
        test = sie_get_containing_test(channel);
        if (test)
            printf("is contained in test id %lu.\n",
                   (unsigned long)sie_get_id(test));
        else
            printf("is not in a test.\n");
        sie_release(test);
    }
    sie_release(channel_iterator);

    /* Now, to close the file, we simply release it like any other
     * object. */
    sie_release(file);

    /* Finally, we'll check to see if any exceptions happened during
     * our run: */
    if (sie_check_exception(context)) {
        exception = sie_get_exception(context);
        fprintf(stderr, "Something bad happened:\n  %s\n",
                sie_verbose_report(exception));
        sie_release(exception);
    }

    /* Unlike other SIE objects, a context must be disposed of in a
     * special way, to release internal data structures and break
     * circular references. */
    leaked_objects = sie_context_done(context);
    /* sie_context_done() attempts to dispose of the specified
     * context.  If successful, it returns 0.  Otherwise, other
     * objects are still alive and referencing the context.  The
     * number returned is the number of objects that are still alive.
     * It is good coding practice to check that this returns zero to
     * ensure all SIE objects are being cleaned up properly.
     *
     * Unlike in other places that want a context, you must use the
     * actual context object here.  If the context were referred to
     * via another live object as described above, this call would
     * fail as that object would by definition still be referencing
     * the context! */

    if (leaked_objects != 0)
        fprintf(stderr, "Warning:  Leaked %d SIE objects!\n", leaked_objects);
}

/* This function prints a tag to standard output, prefixed by a
 * string.  It is used many times in the print_sie_file function
 * above. */
static void print_tag(sie_Tag *tag, const char *prefix)
{
    const char *name;
    char *value = NULL;
    size_t value_size = 0;
    int dont_print = 0;

    /* We have a tag object, and we want to get something useful out
     * of it.  As described above, a tag is a relation between a
     * textual key and an arbitrary value.  Getting the id ("key") is
     * easy: */
    name = sie_tag_get_id(tag);

    /* However, tags can contain arbitrary-length binary data in the
     * value.  To get the entire contents of the value of a tag in a
     * binary-safe way, use: */
    sie_tag_get_value_b(tag, &value, &value_size);
    /* This sets "value" to a pointer pointing to newly allocated
     * memory of size "value_size" containing the tag value.  The
     * value is guaranteed to be null-terminated by the library, so it
     * can safely be treated as a C string as well.  However, the null
     * added by libsie is not included in the size returned, so as to
     * not disturb the size information of real binary data.
     * sie_tag_get_value_b returns true if it successfully got the tag
     * value, or 0 if it didn't, but in the interests of brevity we're
     * not checking that here.
     *
     * Because a tag value can be arbitrarily long, sucking the
     * entirety of it into memory (as the function used above does)
     * may not be wise.  You can also attach a "spigot" to a tag to
     * get the value out piecewise, just like getting the data out of
     * a channel.  This technique is beyond the scope of this
     * demonstration, however.
     *
     * As tags are occasionally long, let's only print the length of
     * the tag here if it is over 50 bytes, or if it contains any
     * nulls: */
    if (value_size > 50 || memchr(value, 0, value_size)) {
        printf("%s'%s': long tag of %lu bytes.\n",
               prefix, name, (unsigned long)value_size);
        dont_print = 1;
    }

    /* The value returned by sie_tag_get_value_b must be freed as any
     * other allocated raw memory in C.  To free plain pointers to
     * allocated memory returned from libsie, call sie_free(). */
    sie_free(value);

    if (dont_print)
        return;

    /* Obviously we could have printed out the value above, but this
     * gives us a chance to expose another interface.  If we are
     * willing to not know the binary size of a tag's value and just
     * want to treat the value as a NULL-terminated string, we can
     * call: */
    value = sie_tag_get_value(tag);

    printf("%s'%s': '%s'\n", prefix, name, value);

    /* Just as with sie_tag_get_value_b, we have to free the returned
     * value. */
    sie_free(value);

    /* Note that we don't have to free the value returned by
     * sie_tag_get_id -- it is cleaned up with the tag object. */
}

/* I hope this demonstration has been instructive in how to use
 * libsie.  For more information, look at the following:
 *
 * * Reference documentation for libsie.  (Available in "sie.h", the
 *   libsie header file.)
 *
 * * "The SIE Format", a white paper describing the SIE format in
 *   detail.
 *
 * * SIE schema documentation for the "core" and "somat" namespaces.
 *
 */
