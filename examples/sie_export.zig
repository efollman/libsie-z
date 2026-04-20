// LibSIE SIE-to-ASCII Exporter — Zig Port
//
// This example reads an SIE file and exports all metadata and
// channel data to a plain ASCII text file.  It mirrors the
// sie_dump example but writes to a file instead of the terminal.
//
// Usage:
//
//   zig-out/bin/sie_export myfile.sie output.txt
//

const std = @import("std");
const libsie = @import("libsie");

const SieFile = libsie.SieFile;
const Output = libsie.Output;
const Tag = libsie.Tag;

// ---------------------------------------------------------------
// Helper: writeTag
// ---------------------------------------------------------------
fn writeTag(writer: anytype, tag: *const Tag, prefix: []const u8) !void {
    const name = tag.key;

    if (tag.isString()) {
        const value = tag.string() orelse "";

        if (value.len > 50) {
            try writer.print("{s}'{s}': long tag of {d} bytes.\n", .{
                prefix, name, value.len,
            });
        } else {
            try writer.print("{s}'{s}': '{s}'\n", .{ prefix, name, value });
        }
    } else {
        try writer.print("{s}'{s}': binary tag of {d} bytes.\n", .{
            prefix, name, tag.valueSize(),
        });
    }
}

// ---------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------
pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer {
        const check = gpa.deinit();
        if (check == .leak) {
            std.debug.print("Warning: memory leaks detected!\n", .{});
        }
    }
    const allocator = gpa.allocator();

    const stderr = std.fs.File.stderr().deprecatedWriter();

    // ---------------------------------------------------------------
    // Parse command line arguments
    // ---------------------------------------------------------------
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 3) {
        try stderr.print("Usage: sie_export <input.sie> <output.txt>\n", .{});
        std.process.exit(1);
    }

    const filename = args[1];
    const output_path = args[2];

    // ---------------------------------------------------------------
    // Open the SIE file
    // ---------------------------------------------------------------
    var sf = SieFile.open(allocator, filename) catch |err| {
        try stderr.print("Error: Could not open '{s}': {}\n", .{ filename, err });
        std.process.exit(1);
    };
    defer sf.deinit();

    // ---------------------------------------------------------------
    // Open the output file for writing
    // ---------------------------------------------------------------
    const out_file = std.fs.cwd().createFile(output_path, .{}) catch |err| {
        try stderr.print("Error: Could not create '{s}': {}\n", .{ output_path, err });
        std.process.exit(1);
    };
    defer out_file.close();
    const writer = out_file.deprecatedWriter();

    // ---------------------------------------------------------------
    // Print file summary
    // ---------------------------------------------------------------
    const file = sf.file;
    try writer.print("LibSIE {s} - SIE file export\n\n", .{libsie.version});
    try writer.print("File '{s}':\n", .{filename});
    try writer.print("  Size: {d} bytes\n", .{@as(u64, @intCast(file.file_size))});
    try writer.print("  Groups: {d}\n", .{file.numGroups()});
    try writer.print("  Decoders compiled: {d}\n", .{sf.numDecoders()});
    try writer.print("\n", .{});

    // ---------------------------------------------------------------
    // Section 1: All metadata (file tags, test/channel/dimension tags)
    // ---------------------------------------------------------------
    const file_tags = sf.fileTags();
    if (file_tags.len > 0) {
        try writer.print("File tags:\n", .{});
        for (file_tags) |*tag| {
            try writeTag(writer, tag, "  ");
        }
        try writer.print("\n", .{});
    }

    const tests = sf.tests();
    try writer.print("Tests: {d}\n", .{tests.len});

    for (tests) |*test_obj| {
        try writer.print("\n  Test id {d}:\n", .{test_obj.id});

        const test_tags = test_obj.tags();
        for (test_tags) |*tag| {
            try writeTag(writer, tag, "    Test tag ");
        }

        const channels = test_obj.channels();
        try writer.print("    Channels: {d}\n", .{channels.len});

        for (channels) |*ch| {
            try writer.print("\n    Channel id {d}, '{s}':\n", .{
                ch.id, ch.name,
            });

            const ch_tags = ch.tags();
            for (ch_tags) |*tag| {
                try writeTag(writer, tag, "      Channel tag ");
            }

            const dims = ch.dimensions();
            for (dims) |*dim| {
                try writer.print("      Dimension index {d}:\n", .{dim.index});

                const dim_tags = dim.tags();
                for (dim_tags) |*tag| {
                    try writeTag(writer, tag, "        Dimension tag ");
                }
            }
        }
    }

    // ---------------------------------------------------------------
    // Section 2: Raw channel data
    // ---------------------------------------------------------------
    try writer.print("\n", .{});

    for (tests) |*test_obj| {
        const channels = test_obj.channels();

        for (channels) |*ch| {
            try writer.print("Channel {d}\n", .{ch.id});

            var spig = sf.attachSpigot(ch) catch {
                continue;
            };
            defer spig.deinit();

            while (try spig.get()) |out| {
                const num_dims = out.num_dims;
                const num_rows = out.num_rows;

                for (0..num_rows) |row| {
                    for (0..num_dims) |dim| {
                        if (dim != 0) try writer.print("\t", .{});

                        if (out.float64(dim, row)) |val| {
                            try writer.print("{d:.15}", .{val});
                        } else if (out.raw(dim, row)) |raw_val| {
                            for (raw_val.ptr) |byte| {
                                try writer.print("{x:0>2}", .{byte});
                            }
                        }
                    }
                    try writer.print("\n", .{});
                }
            }

            try writer.print("\n", .{});
        }
    }

    // Report success to stderr so the user sees feedback on the terminal.
    try stderr.print("Exported '{s}' to '{s}'\n", .{ filename, output_path });
}
