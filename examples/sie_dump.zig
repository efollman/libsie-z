// LibSIE SIE file dumper demo
// Replaces src/libsie-demo/libsie-demo.c
//
// Usage: sie_dump <file.sie>
//
// Opens an SIE file using the high-level SieFile API, prints the
// test/channel/dimension hierarchy and tag metadata, then reads
// and prints data values through the channel spigot pipeline.

const std = @import("std");
const libsie = @import("libsie");

const SieFile = libsie.SieFile;
const Output = libsie.Output;

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdout = std.fs.File.stdout().deprecatedWriter();
    const stderr = std.fs.File.stderr().deprecatedWriter();

    try stdout.print("LibSIE {s} - SIE file dumper (Zig port)\n\n", .{libsie.version});

    // Parse command line
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        try stderr.print("Usage: sie_dump <file.sie>\n", .{});
        std.process.exit(1);
    }

    const filename = args[1];

    // Open the SIE file (reads XML, compiles decoders, builds hierarchy)
    var sf = SieFile.open(allocator, filename) catch {
        try stderr.print("Error: Could not open '{s}'\n", .{filename});
        std.process.exit(1);
    };
    defer sf.deinit();

    const file = sf.getFile();
    try stdout.print("File: {s}\n", .{filename});
    try stdout.print("Size: {} bytes\n", .{@as(u64, @intCast(file.file_size))});
    try stdout.print("Groups: {}\n", .{file.getNumGroups()});
    try stdout.print("Decoders compiled: {}\n\n", .{sf.compiled_decoders.count()});

    // Print file-level tags
    const file_tags = sf.getFileTags();
    if (file_tags.len > 0) {
        try stdout.print("=== File Tags ===\n", .{});
        for (file_tags) |tag| {
            try stdout.print("  {s} = {s}\n", .{ tag.getId(), tag.getString() orelse "(binary)" });
        }
        try stdout.print("\n", .{});
    }

    // Print test/channel/dimension hierarchy
    const tests = sf.getTests();
    try stdout.print("=== Tests: {} ===\n", .{tests.len});

    for (tests) |*test_obj| {
        try stdout.print("\nTest {}\n", .{test_obj.id});

        // Test tags
        const test_tags = test_obj.getTags();
        for (test_tags) |tag| {
            try stdout.print("  tag: {s} = {s}\n", .{ tag.getId(), tag.getString() orelse "(binary)" });
        }

        // Channels
        const channels = test_obj.getChannels();
        try stdout.print("  Channels: {}\n", .{channels.len});

        for (channels) |*ch| {
            try stdout.print("\n  Channel {} \"{s}\" (group={}, test={})\n", .{
                ch.id, ch.name, ch.toplevel_group, ch.test_id,
            });

            // Channel tags
            const ch_tags = ch.getTags();
            for (ch_tags) |tag| {
                try stdout.print("    tag: {s} = {s}\n", .{ tag.getId(), tag.getString() orelse "(binary)" });
            }

            // Dimensions
            const dims = ch.getDimensions();
            try stdout.print("    Dimensions: {}\n", .{dims.len});

            for (dims) |*dim| {
                try stdout.print("      dim[{}] decoder={} v={}\n", .{
                    dim.index, dim.decoder_id, dim.decoder_version,
                });

                // Dimension tags
                const dim_tags = dim.getTags();
                for (dim_tags) |tag| {
                    try stdout.print("        tag: {s} = {s}\n", .{ tag.getId(), tag.getString() orelse "(binary)" });
                }
            }

            // Attach spigot and read data
            var spig = sf.attachSpigot(ch) catch |e| {
                try stdout.print("    (could not attach spigot: {})\n", .{e});
                continue;
            };
            defer spig.deinit();

            try stdout.print("    Data blocks: {}\n", .{spig.numBlocks()});

            var total_rows: usize = 0;
            var block_count: usize = 0;
            while (try spig.get()) |out| {
                if (block_count < 3) {
                    // Print first few blocks in detail
                    try stdout.print("    Block {}: {} rows, {} dims\n", .{
                        out.block, out.num_rows, out.num_dims,
                    });
                    // Print first few rows
                    const max_rows = @min(out.num_rows, 5);
                    for (0..max_rows) |row| {
                        try stdout.print("      [{}]", .{row});
                        for (0..out.num_dims) |d| {
                            if (out.getFloat64(d, row)) |val| {
                                try stdout.print(" {d:.6}", .{val});
                            } else {
                                try stdout.print(" (raw)", .{});
                            }
                        }
                        try stdout.print("\n", .{});
                    }
                    if (out.num_rows > max_rows) {
                        try stdout.print("      ... ({} more rows)\n", .{out.num_rows - max_rows});
                    }
                }
                total_rows += out.num_rows;
                block_count += 1;
            }
            if (block_count > 3) {
                try stdout.print("    ... ({} more blocks)\n", .{block_count - 3});
            }
            try stdout.print("    Total: {} rows in {} blocks\n", .{ total_rows, block_count });
        }
    }

    try stdout.print("\nDone.\n", .{});
}
