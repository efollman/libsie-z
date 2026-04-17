/* 
 * libsie Reference Manual
 * =======================
 */

/*
 * ## License
 *
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef SIE_H_INCLUDED
#define SIE_H_INCLUDED

#include <stdlib.h>

/* 
 * ## Overview
 *
 * The SIE reader library (libsie) exposes an object-oriented C API
 * for reading SIE data.
 * 
 * SIE is a flexible and robust data storage and transmission format.
 * It is designed to be self-describing - looking at an SIE file in a
 * text editor should present enough information to extract data from
 * it.  Flexibility comes in part from the binary data formats being
 * defined in the file, not in the specification.  Safety and
 * streamability comes from always writing in an append-only fashion
 * (with no forward pointers) when possible.
 *
 * For more details, read "The SIE Format."
 * 
 * The basic metadata structure of an SIE file looks something like
 * this:
 * 
 *     FILE
 *       TAG toplevel_tag = value
 *       TAG ... = ...
 *       TEST
 *         TAG core:start_time = 2005-04-15T17:16:23-0500
 *         TAG core:test_count = 1
 *         TAG ... = ...
 *         CHANNEL timhis:Plus8.RN_1
 *           TAG core:description = Unsigned 8 Bit Value
 *           TAG core:sample_rate = 2500
 *           DIMENSION 0
 *             TAG core:units = sec
 *           DIMENSION 1
 *             TAG core:range_min = 0
 *             TAG core:range_max = 255
 *             TAG core:units = bits
 *         CHANNEL ...
 *           ...
 *       TEST
 *         ...
 *       ...
 * 
 * This is encoded in an XML description in the SIE file.  The file
 * also contains binary data, and an XML description of how to read
 * that data and turn it into a two-dimensional array of engineering
 * values.  libsie provides functions to open a file, traverse the
 * metadata tree as shown above, and attach to a channel and get the
 * data in a universal format.
 *
 * For a verbose tutorial on using libsie to read SIE files, please
 * see the `libsie-demo.c` file in your libsie distribution.
 */

#ifndef SIE_DOCUMENTATION
/* Please skip ahead if you are reading this as documentation - this
 * is all housekeeping stuff to make this header file work. */

# ifdef __cplusplus
#  define SIE_EXTERN_C extern "C"
# else
#  define SIE_EXTERN_C
# endif

/* The following typedefs are architecture-specific and have been
 * deduced by configure. */
# ifdef _WIN32
typedef double sie_float64;
typedef unsigned int sie_uint32;
# else
typedef @sie_float64_type@ sie_float64;
typedef @sie_uint32_type@ sie_uint32;
# endif
/* The rest of the file is architecture-independent. */

/* At a library user level the internals of these objects are
 * unimportant, so we just make them generic pointers. */
typedef void sie_Context;
typedef void sie_File;
typedef void sie_Iterator;
typedef void sie_Test;
typedef void sie_Tag;
typedef void sie_Spigot;
typedef void sie_Output;
typedef void sie_Channel;
typedef void sie_Dimension;
typedef void sie_Histogram;
typedef void sie_Exception;
typedef void sie_Stream;

# ifdef WIN32
#  ifdef SIE_DECLARE_STATIC
#   define SIE_DECLARE(type)  SIE_EXTERN_C type __cdecl
#  else
#   define SIE_DECLARE(type)  SIE_EXTERN_C __declspec(dllimport) type __cdecl
#  endif
# else
#  define SIE_DECLARE(type)   SIE_EXTERN_C type
# endif

#endif /* SIE_DOCUMENTATION */

/*
 * ## Library API notes
 * 
 * All functions are prefixed with `sie_`.
 * 
 * All of the `sie_`* object pointer types can be considered generic
 * pointers -- a normal library user should not have need to access
 * the internals.
 *
 * There are some special types used in these definitions.  Their C
 * equivalents for your local system are:
 *
 * * `sie_float64`: `@sie_float64_type@`
 * * `sie_uint32`: `@sie_uint32_type@`
 *
 * All string values passed to and from the libsie API are UTF-8
 * strings.  This includes the file name passed to sie_file_open.
 *
 * ## The library context
 * 
 * The first step towards using the library is to get a library
 * context.  This serves to keep global resources around that are used
 * in library operations.  It is also used for error handling.  libsie
 * is thread-safe as long as code referencing a context is only
 * running in a single thread at a time.  Multiple contexts can be
 * created, but objects cannot be shared between them.
 */

SIE_DECLARE(sie_Context *) sie_context_new(void);
/* > Returns a new library context. */

/*
 * Other API functions take a *context object* that specifies what
 * library context the function should operate in.  Any libsie object,
 * including the context itself, can function as a context object
 * referring to the context in which it was created.
 *
 * ## Memory management
 *
 * libsie uses reference counting for its memory handling.  There are
 * two generic methods to manage an object's reference count:
 */

SIE_DECLARE(void *) sie_retain(void *object);
/* > "Retains" `object` by raising its reference count by one. */

SIE_DECLARE(void) sie_release(void *object);
/* > "Releases" `object`, lowering its reference count by one.  If its
 * > count reaches zero, the object is freed from memory. */

/* 
 * `NULL` input to `sie_retain` and `sie_release` is safe, and does
 * nothing.  `sie_retain` returns the object pointer so as to allow
 * for chaining constructs like `return sie_retain(object);`.
 * 
 * The exception to retain and release methods is the context object:
 */

SIE_DECLARE(int) sie_context_done(sie_Context *context);
/* > Attempts to release the library context `context`.  If
 * > successful, zero is returned.  Otherwise, the number returned is
 * > the number of objects that still have dangling references to them
 * > and as such were not freed. */

/*
 * Some SIE API functions return plain pointers to memory, not objects.
 * `sie_free` must be called on these pointers to free them.
 */

SIE_DECLARE(void) sie_free(void *pointer);
/* > Frees the libsie-allocated memory pointed to by `pointer`. */

/*
 * ## Opening a file
 *
 * Opening a file is as easy as:
 */

SIE_DECLARE(sie_File *) sie_file_open(void *context_object, const char *name);
/* > Opens an SIE file, returning a file object. */

/*
 * ## References
 *
 * *References* are libsie objects that reference SIE files or parts
 * thereof.  A file is a reference, as are tests, channels,
 * dimensions, and tags.  There are a set of common methods for
 * references:
 */

SIE_DECLARE(sie_Iterator *) sie_get_tests(void *reference);
/* > For files, returns an iterator containing all tests (as
 * > `sie_Test` objects) in the file.  Not applicable for any other
 * > type - returns `NULL`. */

SIE_DECLARE(sie_Iterator *) sie_get_channels(void *reference);
/* > For files, returns an iterator containing all channels (as
 * > `sie_Channel` objects) in the file. For tests, returns an
 * > iterator containing all channels (as `sie_Channel` objects) in
 * > the test.  Not applicable for any other type - returns `NULL`. */

SIE_DECLARE(sie_Iterator *) sie_get_dimensions(void *reference);
/* > For channels, returns an iterator containing all dimensions (as
 * > `sie_Dimension` objects) in the channel.  Not applicable for any
 * > other type - returns `NULL`. */

SIE_DECLARE(sie_Iterator *) sie_get_tags(void *reference);
/* > For files, returns all toplevel tags in the file. For tests,
 * > channels, and dimensions, returns all tags in the requested
 * > object.  Not valid for tags - returns `NULL`. */

SIE_DECLARE(sie_Test *) sie_get_test(void *reference, sie_uint32 id);
/* > For files, returns the test in the file with id `id`, or `NULL` if
 * > no such id exists.  Not applicable for any other type - returns
 * > `NULL`. */

SIE_DECLARE(sie_Channel *) sie_get_channel(void *reference, sie_uint32 id);
/* > For files, returns the channel id `id`, or `NULL` if no such
 * > channel id exists.  Not applicable for any other type - returns
 * > `NULL`. */

SIE_DECLARE(sie_Dimension *) sie_get_dimension(void *reference, sie_uint32 index);
/* > For channels, returns the dimension index `index`, or `NULL` if
 * > no such dimension exists.  Not applicable for any other type -
 * > returns `NULL`. */

SIE_DECLARE(sie_Tag *) sie_get_tag(void *reference, const char *id);
/* > Returns the tag contained in `reference` with id `id`.  Returns
 * > `NULL` if no such tag id exists.  Not valid for tags - returns
 * > `NULL`. */

SIE_DECLARE(sie_Test *) sie_get_containing_test(void *reference);
/* > For channels, returns the test object representing the test the
 * > channel is a member of, if any.  Not applicable for any other
 * > type - returns `NULL`. */

SIE_DECLARE(const char *) sie_get_name(void *reference);
/* > For channels, returns the name of the channel.  Not applicable
 * > for any other type - returns `NULL`. */

#define SIE_NULL_ID   (~(sie_uint32)0)

SIE_DECLARE(sie_uint32) sie_get_id(void *reference);
/* > For tests and channels, returns the id.  Not applicable for any
 * > other type - returns `SIE_NULL_ID`. */

SIE_DECLARE(sie_uint32) sie_get_index(void *reference);
/* > For dimensions, returns the index of the dimension.  Not
 * > applicable for any other type - returns `SIE_NULL_ID`. */

/*
 * ## Iterators
 * 
 * The iterator interface is simple: 
 */

SIE_DECLARE(void *) sie_iterator_next(void *iter);
/* > Returns the next object from the iterator.  The returned object
 * > is "owned" by the spigot, is valid until the next call to
 * > `sie_iterator_next`, and does not need to be released.  If the
 * > object will be referenced after the next call to
 * > `sie_iterator_next` or the release of the iterator, it must be
 * > retained with `sie_retain` and later released. */

/*
 * ## Tags
 *
 * A tag is a key to value pairing and is used for almost all
 * metadata.  To get the parts of a tag object:
 */

SIE_DECLARE(const char *) sie_tag_get_id(sie_Tag *tag);
/* > Returns the id (key) of `tag`.  The returned string is valid for
 * > the lifetime of the tag object - if needed longer, it must be
 * > `strdup`'d or otherwise reallocated. */

SIE_DECLARE(char *) sie_tag_get_value(sie_Tag *tag);
/* > Returns a newly-allocated string containing the tag value of
 * > `tag`.  Because the returned string is a plain pointer, it must
 * > eventually be freed with the `sie_free` function.  Returns `NULL`
 * > on failure. */

SIE_DECLARE(int) sie_tag_get_value_b(sie_Tag *tag, char **value, size_t *size);
/* > Sets the `value` pointer to a pointer to newly-allocated string
 * > containing the tag value of `tag`, and the `size` pointer to the
 * > size of the data.  This function returns true if successful.  If
 * > an error occurs, the function returns false, and `value` and
 * > `size` are unchanged.  If successful, `value` must eventually be
 * > freed with the `sie_free` function. */

/* Because the amount of data in a tag value can be potentially huge,
 * tag data can also be read with a spigot, as described below. */

/*
 * ## Spigots and data
 *
 * A spigot is the interface used to get data out of the library.  A
 * spigot can be attached to several kinds of references (currently
 * channels and tags), and can be read from repeatedly, returning the
 * data contained in the reference.
 */

SIE_DECLARE(sie_Spigot *) sie_attach_spigot(void *reference);
/* > Attaches a spigot to `reference` in preparation for reading
 * > data. */

SIE_DECLARE(sie_Output *) sie_spigot_get(sie_Spigot *spigot);
/* > Reads the next output record out of `spigot`.  If it returns
 * > `NULL`, all data has been read.  The output record is "owned" by
 * > the spigot, is valid until the next call to `sie_spigot_get`, and
 * > does not need to be released.  If the output will be referenced
 * > after the next call to `sie_spigot_get` or the release of the
 * > spigot, it must be retained with `sie_retain` and later
 * > released. */

SIE_DECLARE(void) sie_spigot_disable_transforms(void *spigot, int disable);
/* > If `disable` is true, adjusts `spigot` such that data returned
 * > will not be transformed by SIE xform nodes.  This typically means
 * > that raw decoder output will be returned instead of engineering
 * > values.  This can be useful, as many data schemas have dimension
 * > 0 being "time" when scaled, and "sample count" when
 * > unscaled. Setting the spigot to unscaled and binary searching
 * > dimension 0 can be used to find a particular sample number with
 * > such schemas.  This is currently applicable only to channels. */

SIE_DECLARE(void) sie_spigot_transform_output(void *spigot, sie_Output *output);
/* > Transform `output` as it would be had it been an output from
 * > `spigot` and had transforms not been disabled.  This allows
 * > transforming spigot output after the fact, to get both
 * > transformed and non-transformed output without reading from the
 * > spigot twice. */

#define SIE_SPIGOT_SEEK_END (~(size_t)0)

SIE_DECLARE(size_t) sie_spigot_seek(void *spigot, size_t target);
/* > Prepares `spigot` such that the next call to `sie_spigot_get`
 * > will return the data in the block `target`.  If the target is
 * > past the end of the data in the file, it will be set to the end
 * > of the data - i.e., one block after the last one (and calling
 * > `sie_spigot_get` immediately after will return `NULL` indicating
 * > the end of the data).  Returns the block position that was set.
 * > `SIE_SPIGOT_SEEK_END`, defined as "all ones" (i.e., 0xffffffff on
 * > 32-bit platforms) is provided as a convenient way to seek to the
 * > end of a file. */

SIE_DECLARE(size_t) sie_spigot_tell(void *spigot);
/* > Returns the current block position of `spigot` - i.e. the block
 * > that the next call to `sie_spigot_get` will return. */

SIE_DECLARE(int) sie_lower_bound(void *spigot, size_t dim, sie_float64 value,
                    size_t *block, size_t *scan);
/* > Given a spigot `spigot` to data for which the dimension specified
 * > by `dim` is non-decreasing, find and return the block and scan
 * > within the block where the value of the specified dimension is
 * > first greater than or equal to `value`.  Returns true if a value
 * > is found (and sets `block` and `scan` to the found value), and
 * > returns false if the last point in the data is less than the
 * > value, or if some other error occurred.  If true, `block` and
 * > `scan` are always returned such that seeking to `block`, calling
 * > `sie_spigot_get`, and getting the scan number `scan` in that
 * > block will always be the first value in that dimension greater
 * > than or equal to the search value.  The block the spigot is
 * > currently pointing at (see `sie_spigot_tell`) will not be
 * > affected. */

SIE_DECLARE(int) sie_upper_bound(void *spigot, size_t dim, sie_float64 value,
                    size_t *block, size_t *scan);
/* > This is the opposite of `sie_lower_bound`.  Given a spigot
 * > `spigot` to data for which the dimension specified by `dim` is
 * > non-decreasing, find and return the block and scan within the
 * > block where the value of the specified dimension is last less
 * > than or equal to `value`.  Returns true if a value is found (and
 * > sets `block` and `scan` to the found value), and returns false if
 * > the first point in the data is greater than the value, or if some
 * > other error occurred.  If true, `block` and `scan` are always
 * > returned such that seeking to `block`, calling `sie_spigot_get`,
 * > and getting the scan number `scan` in that block will always be
 * > the last value in that dimension less than or equal to the search
 * > value.  The block the spigot is currently pointing at (see
 * > `sie_spigot_tell`) will not be affected. */

/*
 * ## Output
 * 
 * The data that comes out of a spigot is arranged in scans of
 * vectors.  Each column can currently be one of two datatypes:
 * 64-bit float or "raw", which is a string of octets.  The form of
 * the data will vary depending on the channel.
 *
 * The following methods exist to access the output:
 */

SIE_DECLARE(size_t) sie_output_get_block(sie_Output *output);
/* > Returns the block number from which the data originated (relative
 * > to the data source - i.e., the first data in a channel is always
 * > block 0.) */

SIE_DECLARE(size_t) sie_output_get_num_dims(sie_Output *output);
/* > Returns the number of dimensions in `output`. */

SIE_DECLARE(size_t) sie_output_get_num_rows(sie_Output *output);
/* > Returns the number of rows of data in `output`. */

SIE_DECLARE(int) sie_output_get_type(sie_Output *output, size_t dim);
/* > Returns the type of the specified dimension of `output`.  This
 * > is one of: */

#define SIE_OUTPUT_NONE    0
#define SIE_OUTPUT_FLOAT64 1
#define SIE_OUTPUT_RAW     2

SIE_DECLARE(sie_float64 *) sie_output_get_float64(sie_Output *output, size_t dim);
/* > Returns a pointer to an array of float64 (double) data for the
 * > specified dimension.  This array has a size equal to the number
 * > of scans in the output.  This is only valid if the type of the
 * > dimension is `SIE_OUTPUT_FLOAT64`.  The lifetime of the return
 * > value is managed by the `sie_Output` object. */

typedef struct _sie_Output_Raw {
    void *ptr;
    size_t size;
    int reserved_1;
} sie_Output_Raw;

SIE_DECLARE(sie_Output_Raw *) sie_output_get_raw(sie_Output *output, size_t dim);
/* > As `sie_output_get_float64`, but for raw data; returns a pointer
 * > to an array of sie_Output_Raw for the specified dimension.  This
 * > array has a size equal to the number of scans in the output.  The
 * > `ptr` member of the `sie_Output_Raw` struct is a pointer to the
 * > actual data, `size` is the size of the data pointed at by `ptr`,
 * > in bytes.  The lifetime of the return value is managed by the
 * > `sie_Output` object. */

typedef struct _sie_Output_Dim {
    int type;
    sie_float64 *float64;
    sie_Output_Raw *raw;
} sie_Output_Dim;

typedef struct _sie_Output_Struct {
    size_t num_dims;
    size_t num_rows;
    size_t reserved_1;
    size_t reserved_2;
    sie_Output_Dim *dim;
} sie_Output_Struct;

SIE_DECLARE(sie_Output_Struct *) sie_output_get_struct(sie_Output *output);
/* > Returns an `sie_Output_Struct` pointer containing information
 * > about the `sie_Output` object.  This struct can be used in
 * > C-compatible languages to access all data in the `sie_Output`
 * > object.  The lifetime of the return value is managed by the
 * > `sie_Output` object.  */

/*
 * For information about the SIE data model, read the appropriate
 * section of 'The SIE Format' paper and any applicable schema
 * documentation.
 */

/*
 * ## Error handling
 */

SIE_DECLARE(sie_Exception *) sie_check_exception(void *ctx_obj);
/* > `sie_check_exception` returns NULL if no exception has happened
 * > since library initialization or the last call to
 * > `sie_get_exception`.  Otherwise, it returns a non-NULL
 * > pointer. */

SIE_DECLARE(sie_Exception *) sie_get_exception(void *ctx_obj);
/* > `sie_get_exception` returns NULL if no exception has happened
 * > since library initialization or the last call to
 * > `sie_get_exception`.  Otherwise, it returns the exception object.
 * > The caller is responsible for releasing the exception object when
 * > they are done with it. */

SIE_DECLARE(char *) sie_report(void *exception);
/* > `sie_report` returns a string describing an exception.  The
 * > string returned is valid for the lifetime of the exception object
 * > passed to `sie_report`, and does not have to be freed by the
 * > user. */

SIE_DECLARE(char *) sie_verbose_report(void *exception);
/* > `sie_verbose_report` returns a string describing an exception,
 * > plus extra information describing what was happening when the
 * > exception occurred.  The string returned is valid for the
 * > lifetime of the exception object passed to `sie_report`, and does
 * > not have to be freed by the user. */

/*
 * ## Progress information
 *
 * Some operations, such as `sie_file_open` on very large SIE files,
 * can take enough time that a GUI may want to provide progress
 * information.  The following interface allows one to configure the
 * SIE library context to provide information on the progress of
 * libsie activities.
 *
 * If a callback returns non-zero, the current API function will be
 * aborted.  The API function will return a failure value and an
 * "operation aborted" exception.
 */

typedef int (sie_Progress_Set_Message)(void *data, const char *message);
typedef int (sie_Progress_Percent)(void *data, sie_float64 percent_done);

SIE_DECLARE(void) sie_set_progress_callbacks(void *ctx_obj, void *data, sie_Progress_Set_Message *set_message_callback, sie_Progress_Percent *percent_callback);

/*
 * ## Streaming
 *
 * See contrib/misc/stream-test.c for an example.
 */

SIE_DECLARE(sie_Stream *) sie_stream_new(void *context_object);
/* > Creates a new SIE stream.  The stream object can be used in all
 * > places a file object can be.  Data from the stream can only be
 * > read once. */

SIE_DECLARE(size_t) sie_add_stream_data(void *stream, const void *data, size_t size);
/* > Adds `size` bytes pointed to by `data` to the SIE stream
 * > `stream`.  After this function returns, open spigots can be
 * > queried for more data, and any new channels can be opened.  This
 * > function returns `size` if successful, or `0` if the stream was
 * > corrupt. */

SIE_DECLARE(int) sie_spigot_done(void *spigot);
/* > Returns true if `spigot` is "done"; i.e. if all data have been
 * > read, and no more data will ever appear on this spigot. */

/*
 * ## Miscellanea
 */

SIE_DECLARE(int) sie_file_is_sie(void *ctx_obj, const char *name);
/* > Quickly tests to see if the file specified by `name` looks like
 * > an SIE file.  Returns non-zero if it looks like an SIE file, or
 * > zero otherwise. */

SIE_DECLARE(void) sie_ignore_trailing_garbage(void *ctx_obj, size_t amount);
/* > By default, the library will refuse to open SIE files with any
 * > detectable corruption.  `sie_file_open` will return NULL and the
 * > library exception will be set with an explanation of the error.
 * > However, a somewhat common corruption is a file truncated in the
 * > middle of a block.  This can happen when reading a file that is
 * > being written at the same time.  `sie_ignore_trailing_garbage`
 * > tells the library to open the file anyway as long as it finds a
 * > valid block in the last `amount` bytes of the file. */

/*
 * ## Histogram access
 * 
 * Histograms are presented with a data schema which is comprehensive but
 * somewhat inconvenient to access.  However, libsie provides a utility
 * for reconstructing a more traditional representation.
 * 
 * The SoMat histogram data schema for each bin is:
 * 
 *     dim 0: count
 *     dim 1: dimension 0 lower bound
 *     dim 2: dimension 0 upper bound
 *     dim 3: dimension 1 lower bound
 *     dim 4: dimension 1 upper bound
 *     ...
 * 
 * with as many dimensions as are present in the histogram.  If a bin is
 * repeated, the new count replaces the old one.  This presents all the
 * data needed to reconstruct the histogram in one place.
 * 
 * In SoMat files, this schema is used whenever the "core:schema" tag is
 * "somat:histogram" (or "somat:rainflow", in which case this schema is
 * used with an additional tag of rainflow stack data).
 * 
 * To access a histogram in a more convenient way, use the following
 * interface:
 */

SIE_DECLARE(sie_Histogram *) sie_histogram_new(sie_Channel *channel);
/* > Create a new histogram convenience object from the specified
 * > channel.  This will read all data from the channel.  The
 * > following accessors can then be called on the histogram
 * > object. */

SIE_DECLARE(size_t) sie_histogram_get_num_dims(sie_Histogram *hist);
/* > Returns the number of dimensions in the histogram. */

SIE_DECLARE(size_t) sie_histogram_get_num_bins(sie_Histogram *hist, size_t dim);
/* > Returns the number of bins in the specified dimension of the
 * > histogram. */

SIE_DECLARE(void) sie_histogram_get_bin_bounds(sie_Histogram *hist, size_t dim,
                                  sie_float64 *lower, sie_float64 *upper);
/* > Fills the arrays of 64-bit floats `lower` and `upper` with the
 * > lower and upper bounds of the bins in the specified dimension.
 * > The arrays must have enough space for the number of bins in the
 * > dimension (see the `sie_histogram_get_num_bins` function). */

SIE_DECLARE(sie_float64) sie_histogram_get_bin(sie_Histogram *hist, size_t *indices);
/* > Get the bin value for the specified indices.  `indices` must
 * > point to an array of `size_t` of a size being the number of
 * > dimensions of the histogram. */

SIE_DECLARE(sie_float64) sie_histogram_get_next_nonzero_bin(sie_Histogram *hist,
                                               size_t *start,
                                               size_t *indices);
/* > Starting with absolute bin position `start`, find the next
 * > non-zero bin.  Returns the bin value, sets `indices` to the
 * > indices of the found bin, and `start` to a value such that it can
 * > be used in a future invocation of this function to continue the
 * > search with the bin after the found bin.  `start` should point to
 * > a zero value to start a new search.  `indices` must point to an
 * > array of size_t of a size being the number of dimensions of the
 * > histogram.  There are no more non-zero bins when this function
 * > returns 0.0. */

/*
 * ## Deprecated functions
 *
 * These functions will be removed in a future version of libsie.
 */

SIE_DECLARE(void) sie_system_free(void *pointer);
/* > This deprecated function is identical to `sie_free`. */

SIE_DECLARE(int) sie_binary_search(void *spigot, size_t dim, sie_float64 value,
                      size_t *block, size_t *scan);
/* > This deprecated function is identical to `sie_lower_bound`. */

#endif /* SIE_H_INCLUDED */
