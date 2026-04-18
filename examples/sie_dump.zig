// LibSIE SIE File Dumper — Zig Port
// Zig port of libsie-demo.c from the original C library
//
// Welcome.  This extremely verbose tutorial code reads all data and
// metadata out of an SIE file, while demonstrating most of the
// libsie-zig API.  It is written far more linearly than code like
// this would usually be to allow a better narrative comment flow
// with less jumping around.
//
// After compiling this program, run it with an SIE file on the
// command line:
//
//   zig-out/bin/sie_dump myfile.sie
//

const std = @import("std");
const libsie = @import("libsie");

// Import the types we'll use from libsie.  The root module re-exports
// all public types so they can be accessed via a single import.
const SieFile = libsie.SieFile;
const Output = libsie.Output;
const Tag = libsie.Tag;

// ---------------------------------------------------------------
// Helper: printTag
// ---------------------------------------------------------------
// This function prints a tag to stdout, prefixed by a label string.
// It is used many times in the main print loop below.
//
// As described in the SIE format, a tag is a relation between a
// textual key and an arbitrary value.  The value can be either a
// human-readable string or arbitrary-length binary data.
fn printTag(writer: anytype, tag: *const Tag, prefix: []const u8) !void {
    // Getting the key is straightforward — it is always a string.
    const name = tag.getId();

    // Tags can contain arbitrary-length binary data in the value.
    // To check whether the value is a string or binary, we use
    // isString() / isBinary().  getString() returns null for binary
    // tags, so we can also just try getString() and handle the
    // null case.
    if (tag.isString()) {
        const value = tag.getString() orelse "";

        // As tags are occasionally long, let's only print the
        // length here if the value is over 50 bytes.
        if (value.len > 50) {
            try writer.print("{s}'{s}': long tag of {d} bytes.\n", .{
                prefix, name, value.len,
            });
        } else {
            try writer.print("{s}'{s}': '{s}'\n", .{ prefix, name, value });
        }
    } else {
        // Binary tag — just report the size.  In a real application
        // you might hex-dump the contents or attach a spigot to
        // read the value piecewise (as with channel data).
        try writer.print("{s}'{s}': binary tag of {d} bytes.\n", .{
            prefix, name, tag.getValueSize(),
        });
    }

    // Note: unlike the C API, we don't need to free the returned
    // key or value strings.  In Zig, tags directly own their data
    // and the slices returned are borrows — no allocations occur.
}

// ---------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------
pub fn main() !void {
    // The first step in using libsie is to get an allocator.
    //
    // In C libsie, you create a "context" object that holds
    // internal library state, buffers, and error handling.  In the
    // Zig port, the standard library allocator takes this role.
    // We use a GeneralPurposeAllocator in debug mode because it
    // catches leaks and double-frees during development.
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer {
        // Unlike the C API's sie_context_done() which returns a
        // leaked-object count, the Zig GPA reports leaks via its
        // deinit() return value.  In debug builds this also prints
        // a diagnostic if any allocations were not freed.
        const check = gpa.deinit();
        if (check == .leak) {
            std.debug.print("Warning: memory leaks detected!\n", .{});
        }
    }
    const allocator = gpa.allocator();

    // Get handles to stdout and stderr for output.
    const stdout = std.io.getStdOut().writer();
    const stderr = std.io.getStdErr().writer();

    try stdout.print("LibSIE {s} - SIE file dumper (Zig port)\n\n", .{libsie.version});

    // ---------------------------------------------------------------
    // Parse command line arguments
    // ---------------------------------------------------------------
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        try stderr.print("Please enter an SIE file name on the command line.\n", .{});
        std.process.exit(1);
    }

    const filename = args[1];

    // ---------------------------------------------------------------
    // Open the SIE file
    // ---------------------------------------------------------------
    // SieFile.open() is the high-level entry point.  It:
    //   1. Opens the file and validates the SIE magic bytes
    //   2. Reads the XML definition block (group 0)
    //   3. Merges default + file XML to build the full definition
    //   4. Compiles bytecode decoders for each data format
    //   5. Builds the test → channel → dimension hierarchy
    //   6. Builds the file index (group → block offset mapping)
    //
    // In the C API this was split across sie_context_new(),
    // sie_file_open(), and various configuration calls.  The Zig
    // port collapses these into a single call.
    var sf = SieFile.open(allocator, filename) catch |err| {
        // In the C API you would pull the exception out of the
        // context with sie_get_exception().  In Zig, the error is
        // returned directly as a tagged union (error set), and we
        // can switch on it or print it.
        try stderr.print("Error: Could not open '{s}': {}\n", .{ filename, err });
        std.process.exit(1);
    };
    defer sf.deinit();

    // ---------------------------------------------------------------
    // Print file summary
    // ---------------------------------------------------------------
    // Now we have successfully opened our file.  Let's print some
    // basic information about it.
    const file = sf.getFile();
    try stdout.print("File '{s}':\n", .{filename});
    try stdout.print("  Size: {d} bytes\n", .{@as(u64, @intCast(file.file_size))});
    try stdout.print("  Groups: {d}\n", .{file.getNumGroups()});
    try stdout.print("  Decoders compiled: {d}\n", .{sf.compiled_decoders.count()});
    try stdout.print("\n", .{});

    // ---------------------------------------------------------------
    // File-level tags
    // ---------------------------------------------------------------
    // As you may know from the SIE format document, the generic form
    // of metadata in an SIE file is called a "tag."  This is simply
    // a relation between an arbitrary textual key and an arbitrary
    // value.  Tags can exist at any level in the SIE metadata
    // hierarchy, including at the file level.
    //
    // In the C API, you would get an iterator via sie_get_tags(file)
    // and call sie_iterator_next() in a loop.  In Zig, tags are
    // returned as a slice — no iterator allocation or cleanup needed.
    const file_tags = sf.getFileTags();
    if (file_tags.len > 0) {
        try stdout.print("File tags:\n", .{});
        for (file_tags) |*tag| {
            try printTag(stdout, tag, "  ");
        }
        try stdout.print("\n", .{});
    }

    // ---------------------------------------------------------------
    // Test / Channel / Dimension hierarchy
    // ---------------------------------------------------------------
    // SIE "tests" are grouped collections of channels, representing
    // a single test run or acquisition session.  Within each test,
    // channels represent individual data series (e.g. a strain gauge
    // signal).  Each channel has one or more dimensions that define
    // the "axes" or "columns" of data — for a typical time series,
    // dimension 0 is time and dimension 1 is the engineering value.
    //
    // In the C API, you would call sie_get_tests(file) to get an
    // iterator of tests, then sie_get_channels(test) for channels,
    // and sie_get_dimensions(channel) for dimensions.  In Zig, all
    // of these return slices directly.
    const tests = sf.getTests();
    try stdout.print("Tests: {d}\n", .{tests.len});

    for (tests) |*test_obj| {
        // Each test has a numeric ID that uniquely identifies it
        // within the SIE file.
        try stdout.print("\n  Test id {d}:\n", .{test_obj.id});

        // Tests also have tags.  Print them just like file tags.
        const test_tags = test_obj.getTags();
        for (test_tags) |*tag| {
            try printTag(stdout, tag, "    Test tag ");
        }

        // Now iterate over the channels contained in this test.
        // In the C API: sie_get_channels(test) → iterator.
        // In Zig: test_obj.getChannels() → slice.
        const channels = test_obj.getChannels();
        try stdout.print("    Channels: {d}\n", .{channels.len});

        for (channels) |*ch| {
            // Channels have an SIE-internal ID and may have a name,
            // accessible with getId() and getName().
            try stdout.print("\n    Channel id {d}, '{s}':\n", .{
                ch.getId(), ch.getName(),
            });

            // Channel tags — in real code you'd probably factor this
            // into a helper, but it's inline here for the narrative.
            const ch_tags = ch.getTags();
            for (ch_tags) |*tag| {
                try printTag(stdout, tag, "      Channel tag ");
            }

            // Channels contain dimensions, which define an "axis"
            // or "column" of data.
            const dims = ch.getDimensions();
            for (dims) |*dim| {
                // Dimensions have an "index" — for a typical time
                // series, dimension index 0 is time and index 1 is
                // the engineering value.
                try stdout.print("      Dimension index {d}:\n", .{dim.getIndex()});

                // Dimension tags...
                const dim_tags = dim.getTags();
                for (dim_tags) |*tag| {
                    try printTag(stdout, tag, "        Dimension tag ");
                }
            }

            // -------------------------------------------------------
            // Read channel data via the spigot pipeline
            // -------------------------------------------------------
            // Channels can have a "spigot" attached to them to get
            // the data out.  libsie presents data as a matrix where
            // each column is a dimension as specified above.  Each
            // column can be either 64-bit floats or "raw" octet
            // strings.
            //
            // For example, a time series may look like:
            //
            //      dimension 0   dimension 1
            //     ------------- -------------
            //      0.0           0.0
            //      1.0           0.25
            //      2.0           0.5
            //      3.0           0.25
            //      4.0           0.0
            //
            // To see which type of data is stored, look at the
            // channel tag "core:schema".  For a sequential time
            // series (e.g. "somat:sequential"), dimension 0 is time
            // and dimension 1 is the data value.  All numbers come
            // out scaled to their engineering values.
            //
            // Because interpreting the data is separate from reading
            // it, we can write a single routine to print any type of
            // data stored in an SIE file.
            //
            // In the C API: sie_attach_spigot(channel) → spigot.
            // In Zig: sf.attachSpigot(ch) which wires up the file,
            // decoder, and transform pipeline automatically.
            var spig = sf.attachSpigot(ch) catch |err| {
                try stdout.print("      (could not attach spigot: {})\n", .{err});
                continue;
            };
            defer spig.deinit();

            try stdout.print("      Data blocks: {d}\n", .{spig.numBlocks()});

            // Now, as with C iterators, we pull out sequential
            // sections of the channel's data.  The data is arranged
            // into "blocks" in the SIE file and we get one block at
            // a time via spigot.get().  The data comes out in an
            // Output object.
            //
            // There are spigot operations that allow seeking around
            // to read data blocks out of order (seek, tell, etc.),
            // but those are beyond the scope of this demonstration.
            var total_rows: usize = 0;
            var block_count: usize = 0;

            while (try spig.get()) |out| {
                // The Output object provides accessor functions to
                // pull properties out.
                const num_dims = out.num_dims;
                const num_rows = out.num_rows;

                try stdout.print("      Data block {d}, {d} dimensions, {d} rows:\n", .{
                    out.block, num_dims, num_rows,
                });

                // Now iterate through the output, printing all the
                // data.  In the C API you could either use
                // sie_output_get_struct() to get a raw C struct, or
                // use sie_output_get_float64() / sie_output_get_raw()
                // per-dimension.  In Zig, we use getFloat64() and
                // getRaw() which return optionals — no struct casting
                // or null checking needed.
                for (0..num_rows) |row| {
                    try stdout.print("        Row {d}: ", .{row});
                    for (0..num_dims) |dim| {
                        if (dim != 0) try stdout.print(", ", .{});

                        // The type can be Float64 or Raw.  We try
                        // getFloat64 first; if it returns null then
                        // the column is raw data.
                        if (out.getFloat64(dim, row)) |val| {
                            try stdout.print("{d:.15}", .{val});
                        } else if (out.getRaw(dim, row)) |raw| {
                            // Raw data has a pointer (ptr) and size.
                            // Unlike the C API where you could set a
                            // "claimed" flag to take ownership, in
                            // Zig the Output owns the data and it is
                            // cleaned up automatically when the
                            // spigot advances or is deinitialized.
                            if (raw.size > 16) {
                                try stdout.print("(raw data of size {d})", .{raw.size});
                            } else {
                                for (raw.ptr) |byte| {
                                    try stdout.print("{x:0>2}", .{byte});
                                }
                            }
                        }
                    }
                    try stdout.print("\n", .{});
                }

                // Note: just like with C iterators, we don't need to
                // release the output object — the spigot still "owns"
                // it.  The next call to get() will overwrite the
                // output buffer.  If we needed to keep a copy, we
                // would call out.deepCopy(allocator).
                total_rows += num_rows;
                block_count += 1;
            }

            try stdout.print("      Total: {d} rows in {d} blocks\n", .{
                total_rows, block_count,
            });
        }
    }

    // ---------------------------------------------------------------
    // Flat channel listing
    // ---------------------------------------------------------------
    // We can also skip the test level of the hierarchy and directly
    // get all channels in the file.  In the C API this was:
    //   channel_iterator = sie_get_channels(file);
    // In Zig:
    const all_channels = sf.getAllChannels();
    try stdout.print("\nAll channels ({d}):\n", .{all_channels.len});

    for (all_channels) |ch| {
        try stdout.print("  Channel id {d}, '{s}' ", .{ ch.getId(), ch.getName() });

        // We can also go "backwards" and find which test contains a
        // channel.  Not all channels must be in a test, though most
        // containing actual user data will be.
        //
        // In the C API: sie_get_containing_test(channel).
        // In Zig: sf.getContainingTest(ch).
        if (sf.getContainingTest(ch)) |test_obj| {
            try stdout.print("is contained in test id {d}.\n", .{test_obj.id});
        } else {
            try stdout.print("is not in a test.\n", .{});
        }
    }

    // ---------------------------------------------------------------
    // Cleanup
    // ---------------------------------------------------------------
    // In the C API, you had to sie_release() each object, then call
    // sie_context_done() and check for leaked objects.  In Zig, the
    // SieFile owns all parsed data and `defer sf.deinit()` handles
    // cleanup.  The GPA's deferred deinit (at top of main) will
    // report any leaks, serving the same purpose as
    // sie_context_done()'s leak count.

    try stdout.print("\nDone.\n", .{});
}

// I hope this demonstration has been instructive in how to use
// libsie-zig.  For more information, look at:
//
// * README_ZIG.md — full public API reference for all modules.
//
// * "The SIE Format", a white paper describing the SIE format
//   in detail.
//
// * SIE schema documentation for the "core" and "somat" namespaces.
