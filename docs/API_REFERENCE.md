# libsie-zig API Reference

> This document is the Zig equivalent of the C API reference originally
> documented in `sie.h.in`.  The original C library is available at
> [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference)
> or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).
> Where the C API documentation is quoted, it appears in blockquotes.
> Zig-specific behavior follows immediately after.

---

## Overview

> *From the C reference:*
>
> "The SIE reader library (libsie) exposes an object-oriented C API for reading
> SIE data.
>
> SIE is a flexible and robust data storage and transmission format.  It is
> designed to be self-describing — looking at an SIE file in a text editor
> should present enough information to extract data from it.  Flexibility comes
> in part from the binary data formats being defined in the file, not in the
> specification.  Safety and streamability comes from always writing in an
> append-only fashion (with no forward pointers) when possible."

The Zig port provides the same functionality through a native Zig API with
idiomatic error handling, memory management, and type safety.  All types are
imported from a single `libsie` module:

```zig
const libsie = @import("libsie");
const SieFile = libsie.SieFile;
const Channel = libsie.Channel;
const Output = libsie.Output;
// ... etc.
```

> **C / FFI consumers:** the library also exposes a stable C ABI in
> [`src/c_api.zig`](../src/c_api.zig), declared in
> [`include/sie.h`](../include/sie.h). All `sie_*` functions follow the
> conventions described there (opaque handles, `int` status codes,
> `(ptr, len)` strings). See the project README's *C ABI / FFI* section.

The basic metadata structure of an SIE file:

```
FILE
  TAG toplevel_tag = value
  TAG ... = ...
  TEST
    TAG core:start_time = 2005-04-15T17:16:23-0500
    TAG core:test_count = 1
    CHANNEL timhis:Plus8.RN_1
      TAG core:description = Unsigned 8 Bit Value
      TAG core:sample_rate = 2500
      DIMENSION 0
        TAG core:units = sec
      DIMENSION 1
        TAG core:range_min = 0
        TAG core:range_max = 255
        TAG core:units = bits
    CHANNEL ...
  TEST ...
```

For a verbose tutorial on using libsie-zig to read SIE files, see
[examples/sie_dump.zig](../examples/sie_dump.zig).

---

## Library context → Allocator

> *From the C reference:*
>
> "The first step towards using the library is to get a library context.  This
> serves to keep global resources around that are used in library operations.
> It is also used for error handling.  libsie is thread-safe as long as code
> referencing a context is only running in a single thread at a time.  Multiple
> contexts can be created, but objects cannot be shared between them."

**C API:**

```c
sie_Context *context = sie_context_new();
// ... use library ...
int leaked = sie_context_done(context);
```

**Zig equivalent:** There is no separate context object.  A standard
`std.mem.Allocator` serves the same purpose.  All library objects accept an
allocator at construction time.

```zig
var gpa = std.heap.GeneralPurposeAllocator(.{}){};
defer {
    const check = gpa.deinit();
    if (check == .leak) std.debug.print("Memory leaks detected!\n", .{});
}
const allocator = gpa.allocator();
```

The GPA's `deinit()` serves the same role as `sie_context_done()` — it reports
whether any allocations were leaked.

---

## Memory management

> *From the C reference:*
>
> "libsie uses reference counting for its memory handling.  There are two
> generic methods to manage an object's reference count:
>
> `sie_retain(object)` — Retains object by raising its reference count by one.
>
> `sie_release(object)` — Releases object, lowering its reference count by one.
> If its count reaches zero, the object is freed from memory."

**Zig equivalent:** There is no reference counting in the public API.  Objects
follow Zig's standard ownership model:

- Objects are created with `init()` or `open()` and destroyed with `deinit()`.
- The caller owns the returned object and is responsible for calling `deinit()`.
- Use `defer obj.deinit()` immediately after creation.
- Objects returned from iteration (slices, optionals) are *borrowed* — the
  parent object owns them.  Do not free them.
- To keep a copy beyond the parent's lifetime, use `deepCopy()` where
  available (e.g. `Output.deepCopy()`).

```zig
var sf = try SieFile.open(allocator, "data.sie");
defer sf.deinit();  // Cleans up everything — file, channels, decoders, etc.
```

> *From the C reference:*
>
> "Some SIE API functions return plain pointers to memory, not objects.
> `sie_free` must be called on these pointers to free them."

**Zig equivalent:** No equivalent needed.  String slices returned from tag
accessors, channel names, etc. are borrows from the owning object — no
allocation or freeing occurs.

---

## Opening a file

> *From the C reference:*
>
> "Opening a file is as easy as:
>
> `sie_File *file = sie_file_open(context, filename);`"

**Zig equivalent:**

```zig
var sf = try SieFile.open(allocator, "data.sie");
defer sf.deinit();
```

`SieFile.open()` performs all initialization in one call:

1. Opens and validates the file (SIE magic bytes)
2. Reads and parses XML metadata (group 0)
3. Merges XML definitions (merge/replace rules)
4. Compiles bytecode decoders
5. Builds the test → channel → dimension hierarchy
6. Builds the file index (group → block offset mapping)

On failure, an error from the `Error` enum is returned rather than `NULL` +
exception.

For lower-level file access without the full parse pipeline, use `File`
directly:

```zig
var file = libsie.File.init(allocator, "data.sie");
try file.open();
defer file.deinit();
```

---

## Navigating the metadata hierarchy

> *From the C reference:*
>
> "`sie_get_tests(reference)` — For files, returns an iterator containing all
> tests in the file.
>
> `sie_get_channels(reference)` — For files, returns all channels. For tests,
> returns all channels in the test.
>
> `sie_get_dimensions(reference)` — For channels, returns all dimensions in the
> channel.
>
> `sie_get_tags(reference)` — For files, returns all toplevel tags. For tests,
> channels, and dimensions, returns all tags in the requested object."

**Zig equivalent:** All of these return slices directly — no iterator
allocation or cleanup needed.

```zig
// Tests
const tests = sf.tests();            // []test_mod.Test
for (tests) |*test_obj| { ... }

// Channels (from a test)
const channels = test_obj.channels(); // []const Channel
for (channels) |*ch| { ... }

// All channels in the file (flat)
const all_ch = sf.channels();       // []*Channel
for (all_ch) |ch| { ... }

// Dimensions
const dims = ch.dimensions();         // []const Dimension
for (dims) |*dim| { ... }

// Tags (on any level)
const tags = sf.fileTags();           // []const Tag  (file-level)
const tags2 = ch.tags();              // []const Tag  (channel-level)
const tags3 = dim.tags();             // []const Tag  (dimension-level)
```

> *From the C reference:*
>
> "`sie_get_test(reference, id)` — Returns the test with id `id`.
>
> `sie_get_channel(reference, id)` — Returns the channel with id `id`.
>
> `sie_get_dimension(reference, index)` — Returns the dimension at `index`.
>
> `sie_get_tag(reference, id)` — Returns the tag with id `id`."

**Zig equivalent:**

```zig
const test_obj = sf.findTest(42);            // ?*Test
const ch = sf.findChannel(7);               // ?*Channel
const dim = ch.dimension(0);             // ?*const Dimension
const tag = ch.findTag("core:schema");      // ?*const Tag
```

All return optionals — `null` if not found.

> *From the C reference:*
>
> "`sie_get_containing_test(reference)` — For channels, returns the test the
> channel is a member of, if any."

**Zig equivalent:**

```zig
if (sf.containingTest(ch)) |test_obj| {
    // Channel belongs to this test
} else {
    // Channel is not in a test
}
```

> *From the C reference:*
>
> "`sie_get_name(reference)` — For channels, returns the name.
>
> `sie_get_id(reference)` — For tests and channels, returns the id.
>
> `sie_get_index(reference)` — For dimensions, returns the index."

**Zig equivalent:** Each type has its own specific accessor methods and direct
public fields:

```zig
// Channels
ch.id       // u32
ch.name     // []const u8
ch.test_id   // u32

// Tests
test_obj.id      // u32 (direct field)
test_obj.name    // []const u8 (direct field)

// Dimensions
dim.index   // u32
dim.name    // []const u8
```

---

## Tags

> *From the C reference:*
>
> "A tag is a key to value pairing and is used for almost all metadata.
>
> `sie_tag_get_id(tag)` — Returns the id (key) of tag. The returned string is
> valid for the lifetime of the tag object.
>
> `sie_tag_get_value(tag)` — Returns a newly-allocated string containing the
> tag value. Must be freed with `sie_free`.
>
> `sie_tag_get_value_b(tag, &value, &size)` — Sets value pointer and size for
> binary-safe access. Must be freed with `sie_free`."

**Zig equivalent:** No allocation or freeing needed.  Tag accessors return
borrowed slices:

```zig
const key = tag.key;            // []const u8 — the tag key
const str = tag.string();        // ?[]const u8 — null for binary tags
const bin = tag.binary();        // ?[]const u8 — null for string tags
const size = tag.valueSize();    // usize
const is_str = tag.isString();      // bool
const is_bin = tag.isBinary();      // bool
const group = tag.group;       // u32
const from_grp = tag.isFromGroup(); // bool
```

**Key difference from C:** In the C API, `sie_tag_get_value()` and
`sie_tag_get_value_b()` allocate new memory that must be freed.  In Zig, the
`Tag` owns its data directly and the returned slices are borrows — no
allocation or freeing occurs.  This is both faster and eliminates a class of
memory leak bugs.

---

## Spigots and data

> *From the C reference:*
>
> "A spigot is the interface used to get data out of the library.  A spigot can
> be attached to several kinds of references (currently channels and tags), and
> can be read from repeatedly, returning the data contained in the reference.
>
> `sie_attach_spigot(reference)` — Attaches a spigot to reference.
>
> `sie_spigot_get(spigot)` — Reads the next output record. Returns NULL when
> all data has been read. The output record is owned by the spigot."

**Zig equivalent:**

```zig
var spig = try sf.attachSpigot(ch);
defer spig.deinit();

while (try spig.get()) |output| {
    // output is *Output — owned by the spigot
    // Valid until the next call to get() or spig.deinit()
    const num_dims = output.num_dims;
    const num_rows = output.num_rows;
    // ...
}
```

**Key difference:** In Zig, `get()` returns `!?*Output` — an error union of an
optional.  The `try` propagates errors, and `null` signals end of data.  In C,
both error and end-of-data returned `NULL`, requiring a separate
`sie_check_exception()` call.

### Spigot operations

> *From the C reference:*
>
> "`sie_spigot_seek(spigot, target)` — Seeks to block `target`.
>
> `sie_spigot_tell(spigot)` — Returns the current block position.
>
> `sie_spigot_disable_transforms(spigot, disable)` — If disable is true, raw
> decoder output will be returned instead of engineering values.
>
> `sie_spigot_transform_output(spigot, output)` — Transform output after the
> fact."

**Zig equivalent:**

```zig
const pos = spig.seek(target);     // u64 — actual position after seek
const cur = spig.tell();           // u64 — current block position
spig.disableTransforms(true);      // bool — raw vs. engineering
spig.transformOutput(&output);     // apply transforms after the fact
const done = spig.isDone();        // bool — all data read?
spig.reset();                      // reset to beginning
spig.setScanLimit(1000);           // limit max scans returned
```

### Binary search

> *From the C reference:*
>
> "`sie_lower_bound(spigot, dim, value, &block, &scan)` — Find the first value
> in dimension `dim` greater than or equal to `value`.
>
> `sie_upper_bound(spigot, dim, value, &block, &scan)` — Find the last value
> less than or equal to `value`."

**Zig equivalent:**

```zig
if (try spig.lowerBound(0, 5.0)) |result| {
    // result.block, result.scan
}

if (try spig.upperBound(0, 10.0)) |result| {
    // result.block, result.scan
}
```

Returns an optional `BoundResult` struct instead of out-parameters.

---

## Output

> *From the C reference:*
>
> "The data that comes out of a spigot is arranged in scans of vectors.  Each
> column can currently be one of two datatypes: 64-bit float or 'raw', which is
> a string of octets.
>
> `sie_output_get_block(output)` — Returns the block number.
>
> `sie_output_get_num_dims(output)` — Returns the number of dimensions.
>
> `sie_output_get_num_rows(output)` — Returns the number of rows.
>
> `sie_output_get_type(output, dim)` — Returns the type of dimension `dim`.
> One of: SIE_OUTPUT_NONE (0), SIE_OUTPUT_FLOAT64 (1), SIE_OUTPUT_RAW (2).
>
> `sie_output_get_float64(output, dim)` — Returns a pointer to an array of
> float64 data for the dimension.
>
> `sie_output_get_raw(output, dim)` — Returns a pointer to an array of
> sie_Output_Raw for the dimension."

**Zig equivalent:**

```zig
// Direct field access (no function call overhead):
output.block       // usize — block number
output.num_dims    // usize — number of dimensions
output.num_rows    // usize — number of rows

// Per-cell accessors:
output.float64(dim, row)       // ?f64
output.raw(dim, row)           // ?Output.RawData
output.dimensionType(dim)      // ?Output.OutputType (.None, .Float64, .Raw)
```

**Key difference from C:** The C API provided two access patterns:
per-dimension arrays (`sie_output_get_float64`) and a C struct
(`sie_output_get_struct`).  The Zig API provides per-cell accessors that return
optionals.  This is safer (bounds-checked) and more ergonomic:

```zig
// C style (sie_output_get_struct):
//   os = sie_output_get_struct(output);
//   value = os->dim[d].float64[row];

// Zig style:
if (output.float64(d, row)) |value| {
    // use value
}
```

> *From the C reference (on raw data):*
>
> "The `ptr` member of the `sie_Output_Raw` struct is a pointer to the actual
> data, `size` is the size of the data pointed at by `ptr`, in bytes."

**Zig equivalent:**

```zig
if (output.raw(dim, row)) |raw| {
    raw.ptr   // []const u8 — the raw data bytes
    raw.size  // u32 — size in bytes
}
```

**Deep copy:** To retain output data beyond the spigot's lifetime:

```zig
var copy = try output.deepCopy(allocator);
defer copy.deinit();
```

---

## Error handling

> *From the C reference:*
>
> "`sie_check_exception(ctx_obj)` — Returns NULL if no exception has happened.
>
> `sie_get_exception(ctx_obj)` — Returns the exception object. The caller is
> responsible for releasing it.
>
> `sie_report(exception)` — Returns a string describing an exception.
>
> `sie_verbose_report(exception)` — Returns a string with extra context."

**Zig equivalent:** Errors are returned directly via Zig's error union mechanism.
There is no separate exception object to check or release:

```zig
// C pattern:
//   file = sie_file_open(context, name);
//   if (!file) {
//       exception = sie_get_exception(context);
//       fprintf(stderr, "%s\n", sie_verbose_report(exception));
//       sie_release(exception);
//   }

// Zig pattern:
var sf = SieFile.open(allocator, name) catch |err| {
    std.debug.print("Error: {}\n", .{err});
    return err;
};
defer sf.deinit();
```

The `Error` enum in `src/error.zig` defines 22 error variants covering all
failure modes (file not found, invalid format, CRC mismatch, XML parse errors,
decoder failures, etc.).

---

## Progress information

> *From the C reference:*
>
> "Some operations, such as `sie_file_open` on very large SIE files, can take
> enough time that a GUI may want to provide progress information.
>
> `sie_set_progress_callbacks(ctx_obj, data, set_message_callback,
> percent_callback)` — Configures progress callbacks.
>
> If a callback returns non-zero, the current API function will be aborted."

**Zig equivalent:** The `Context` object supports progress callbacks:

```zig
var ctx = try libsie.Context.init(.{});
defer ctx.deinit();
ctx.setProgressCallbacks(.{
    .message_fn = myMessageCallback,
    .percent_fn = myPercentCallback,
    .data = &my_state,
});
```

---

## Streaming

> *From the C reference:*
>
> "`sie_stream_new(context_object)` — Creates a new SIE stream. The stream
> object can be used in all places a file object can be.
>
> `sie_add_stream_data(stream, data, size)` — Adds data to the stream."

**Zig equivalent:**

```zig
var stream = libsie.Stream.init(allocator);
defer stream.deinit();

const consumed = try stream.addStreamData(data_slice);

// Use the Intake interface for polymorphic access:
var intake = stream.asIntake();
```

The `Stream` implements the same `Intake` interface as `File`, so downstream
code (spigots, decoders) works identically with both.

---

## Miscellaneous

> *From the C reference:*
>
> "`sie_file_is_sie(ctx_obj, name)` — Quickly tests to see if a file looks
> like an SIE file.
>
> `sie_ignore_trailing_garbage(ctx_obj, amount)` — Tells the library to open
> the file anyway as long as it finds a valid block in the last `amount`
> bytes."

**Zig equivalent:**

```zig
// Check if file is SIE:
var file = libsie.File.init(allocator, path);
try file.open();
const is_sie = try file.isSie();
file.deinit();

// Backward search for valid blocks (replaces sie_ignore_trailing_garbage):
const offset = try file.findBlockBackward(max_search_bytes);
```

---

## Histogram access

> *From the C reference:*
>
> "Histograms are presented with a data schema which is comprehensive but
> somewhat inconvenient to access.  libsie provides a utility for
> reconstructing a more traditional representation.
>
> The SoMat histogram data schema for each bin is:
>
>     dim 0: count
>     dim 1: dimension 0 lower bound
>     dim 2: dimension 0 upper bound
>     dim 3: dimension 1 lower bound
>     dim 4: dimension 1 upper bound
>     ...
>
> `sie_histogram_new(channel)` — Create a histogram from a channel.
>
> `sie_histogram_get_num_dims(hist)` — Number of dimensions.
>
> `sie_histogram_get_num_bins(hist, dim)` — Number of bins in dimension.
>
> `sie_histogram_get_bin_bounds(hist, dim, lower, upper)` — Fill arrays with
> bin bounds.
>
> `sie_histogram_get_bin(hist, indices)` — Get bin value at multi-dim indices.
>
> `sie_histogram_get_next_nonzero_bin(hist, start, indices)` — Iterate
> non-zero bins."

**Zig equivalent:**

```zig
var hist = try libsie.Histogram.fromChannel(allocator, &sf, ch);
defer hist.deinit();

const num_dims = hist.getNumDims();
const num_bins = hist.getNumBins(0);

// Get bin bounds for dimension 0:
var lower: [256]f64 = undefined;
var upper: [256]f64 = undefined;
hist.getBinBounds(0, &lower, &upper);

// Access a specific bin:
var indices = [_]usize{ 2, 5 };
const count = hist.getBin(&indices);

// Set a bin:
hist.setBin(&indices, 42.0);

// Iterate non-zero bins:
var start: usize = 0;
var idx: [2]usize = undefined;
while (hist.getNextNonzeroBin(&start, &idx) != 0.0) |count| {
    // Process bin at idx with count
}
```

**Key difference:** The Zig API also supports `Histogram.init()` for manual
construction (without a channel) and `setBinByBounds()` for setting bins by
their engineering-unit boundaries.

---

## Complete type reference

For the full list of all 462 public functions across 36 modules, see the
**Public API** section in [README_ZIG.md](../README_ZIG.md).

---

## Deprecated C functions not ported

The following C API functions have no Zig equivalent:

| C function | Reason |
|------------|--------|
| `sie_retain()` / `sie_release()` | Zig uses allocator-based ownership, not reference counting. |
| `sie_context_new()` / `sie_context_done()` | Replaced by `std.mem.Allocator`. |
| `sie_free()` / `sie_system_free()` | No allocations returned to caller. |
| `sie_binary_search()` | Deprecated alias for `sie_lower_bound()`; use `lowerBound()`. |
| `sie_iterator_next()` | Replaced by slice iteration. |
| `sie_output_get_struct()` | Replaced by direct field access + per-cell accessors. |

---

## License

> Copyright (C) 2005-2015 HBM Inc., SoMat Products
>
> This library is free software; you can redistribute it and/or modify it
> under the terms of version 2.1 of the GNU Lesser General Public License as
> published by the Free Software Foundation.
