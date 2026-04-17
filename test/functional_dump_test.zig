// Functional dump comparison tests — port of t_functional.c
// test_spigot_xml: reads all XML blocks, compares against .xml reference
// test_dump_file: dumps channel data, compares against per-channel reference files
//
// These are the most critical data-correctness tests.

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;
const File = libsie.file.File;
const Block = libsie.block;
const GroupSpigot = libsie.GroupSpigot;

const test_files = [_][]const u8{
    "sie_min_timhis_a_19EFAA61",
    "sie_comprehensive_VBM_DE81A7BA",
    "sie_comprehensive2_VBM_20050908",
    "sie_float_conversions_20050908",
};

const test_data = "test/data";

// --- Helpers ---

/// Replace characters that are invalid in Windows filenames
fn safeFn(name: []const u8, out: []u8) []const u8 {
    const bad_chars = "/\\:*?\"><|";
    var len: usize = 0;
    for (name) |c| {
        if (len >= out.len) break;
        var replaced = false;
        for (bad_chars) |bad| {
            if (c == bad) {
                out[len] = '_';
                replaced = true;
                break;
            }
        }
        if (!replaced) out[len] = c;
        len += 1;
    }
    return out[0..len];
}

// --- test_spigot_xml: Read XML group and compare to reference ---

fn testSpigotXml(comptime file_base: []const u8) !void {
    const sie_path = test_data ++ "/" ++ file_base ++ ".sie";
    const ref_path = test_data ++ "/" ++ file_base ++ ".xml";

    // Open file and build index
    var file = File.init(testing.allocator, sie_path);
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    // Read all XML group blocks via GroupSpigot
    var xml_spigot = GroupSpigot.init(testing.allocator, &file, Block.SIE_XML_GROUP);
    defer xml_spigot.deinit();

    var xml_buf: std.ArrayList(u8) = .{};
    defer xml_buf.deinit(testing.allocator);

    while (try xml_spigot.get()) |payload| {
        try xml_buf.appendSlice(testing.allocator, payload);
    }

    // Append closing tag (matches C behavior)
    try xml_buf.appendSlice(testing.allocator, "</sie>\n");

    // Read reference file
    const reference = try std.fs.cwd().readFileAlloc(testing.allocator, ref_path, 10 * 1024 * 1024);
    defer testing.allocator.free(reference);

    // Compare
    try testing.expectEqualSlices(u8, reference, xml_buf.items);
}

test "functional: spigot XML - sie_min_timhis_a" {
    try testSpigotXml("sie_min_timhis_a_19EFAA61");
}

test "functional: spigot XML - sie_comprehensive_VBM" {
    try testSpigotXml("sie_comprehensive_VBM_DE81A7BA");
}

test "functional: spigot XML - sie_comprehensive2_VBM" {
    try testSpigotXml("sie_comprehensive2_VBM_20050908");
}

test "functional: spigot XML - sie_float_conversions" {
    try testSpigotXml("sie_float_conversions_20050908");
}

// --- test_dump_file: Dump channel data and compare to per-channel reference ---

/// Generate a channel dump matching C's sie_channel_dump + sie_output_dump format
fn dumpChannel(
    allocator: std.mem.Allocator,
    sf: *SieFile,
    ch: *const libsie.channel.Channel,
    full_name: []const u8,
) ![]u8 {
    var buf: std.ArrayList(u8) = .{};
    errdefer buf.deinit(allocator);

    // Channel header: Channel id N, "name":
    try appendFmt(allocator, &buf, "Channel id {d}, \"{s}\":\n", .{ ch.getId(), full_name });

    // Group
    try appendFmt(allocator, &buf, "  group {d}\n", .{ch.toplevel_group});

    // Tags
    for (ch.getTags()) |t| {
        try dumpTag(allocator, &buf, t, "  ");
    }

    // Dimensions
    const dims = ch.getDimensions();
    for (dims) |dim| {
        try appendFmt(allocator, &buf, "  Dimension index {d}:\n", .{dim.getIndex()});
        try appendFmt(allocator, &buf, "    group {d}\n", .{dim.toplevel_group});
        try appendFmt(allocator, &buf, "    decoder_id {d}\n", .{dim.decoder_id});
        try appendFmt(allocator, &buf, "    decoder_v {d}\n", .{dim.decoder_version});
        for (dim.tags.items) |t| {
            try dumpTag(allocator, &buf, t, "    ");
        }
    }

    // Output data
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    while (try spigot.get()) |out| {
        try dumpOutput(allocator, &buf, out);
    }

    return buf.toOwnedSlice(allocator);
}

fn appendFmt(allocator: std.mem.Allocator, buf: *std.ArrayList(u8), comptime fmt: []const u8, args: anytype) !void {
    var tmp: [4096]u8 = undefined;
    const result = std.fmt.bufPrint(&tmp, fmt, args) catch {
        // Fallback for large strings
        const s = try std.fmt.allocPrint(allocator, fmt, args);
        defer allocator.free(s);
        try buf.appendSlice(allocator, s);
        return;
    };
    try buf.appendSlice(allocator, result);
}

fn dumpTag(allocator: std.mem.Allocator, buf: *std.ArrayList(u8), t: libsie.tag.Tag, prefix: []const u8) !void {
    const value = t.getString() orelse "";
    if (value.len > 0) {
        try appendFmt(allocator, buf, "{s}Tag \"{s}\": \"{s}\"\n", .{ prefix, t.getId(), value });
    } else if (t.group != 0) {
        try appendFmt(allocator, buf, "{s}Tag \"{s}\": external in group {d}\n", .{ prefix, t.getId(), t.group });
    } else {
        try appendFmt(allocator, buf, "{s}Tag \"{s}\": \"\"\n", .{ prefix, t.getId() });
    }
}

fn dumpOutput(allocator: std.mem.Allocator, buf: *std.ArrayList(u8), out: *const libsie.output.Output) !void {
    for (0..out.num_rows) |scan| {
        try appendFmt(allocator, buf, "scan[{d}]: ", .{scan});
        for (0..out.num_dims) |v| {
            const dim_type = out.getDimensionType(v) orelse continue;
            switch (dim_type) {
                .Float64 => {
                    if (out.getFloat64(v, scan)) |val| {
                        try appendFmt(allocator, buf, "v[{d}] = {d}; ", .{ v, val });
                    }
                },
                .Raw => {
                    if (out.getRaw(v, scan)) |raw| {
                        try appendFmt(allocator, buf, "v[{d}] =", .{v});
                        for (0..raw.size) |bi| {
                            try appendFmt(allocator, buf, " {x:0>2}", .{raw.ptr[bi]});
                        }
                        try buf.appendSlice(allocator, "; ");
                    }
                },
                .None => {},
            }
        }
        try buf.appendSlice(allocator, "\n");
    }
}

fn testDumpFile(comptime file_base: []const u8) !void {
    const sie_path = test_data ++ "/" ++ file_base ++ ".sie";

    var sf = try SieFile.open(testing.allocator, sie_path);
    defer sf.deinit();

    const channels = sf.getAllChannels();
    var safe_buf: [256]u8 = undefined;

    for (channels) |ch| {
        const safe_name = safeFn(ch.getName(), &safe_buf);

        // Generate dump
        const dump = try dumpChannel(testing.allocator, &sf, ch, ch.getName());
        defer testing.allocator.free(dump);

        // Build reference path
        var ref_path_buf: [512]u8 = undefined;
        const ref_path = std.fmt.bufPrint(&ref_path_buf, "{s}/{s}/{s}", .{
            test_data, file_base, safe_name,
        }) catch return error.TestUnexpectedResult;

        // Read reference
        const reference = std.fs.cwd().readFileAlloc(testing.allocator, ref_path, 10 * 1024 * 1024) catch |err| {
            std.debug.print("Could not read reference file '{s}': {}\n", .{ ref_path, err });
            return err;
        };
        defer testing.allocator.free(reference);

        // Compare
        if (!std.mem.eql(u8, dump, reference)) {
            // Find first difference for debugging
            const min_len = @min(dump.len, reference.len);
            var diff_pos: usize = min_len;
            for (0..min_len) |i| {
                if (dump[i] != reference[i]) {
                    diff_pos = i;
                    break;
                }
            }

            // Find the line containing the difference
            var line_start: usize = diff_pos;
            while (line_start > 0 and dump[line_start - 1] != '\n') line_start -= 1;
            var line_end = diff_pos;
            while (line_end < dump.len and dump[line_end] != '\n') line_end += 1;
            var ref_line_end = diff_pos;
            while (ref_line_end < reference.len and reference[ref_line_end] != '\n') ref_line_end += 1;

            std.debug.print(
                "\nDump mismatch for channel '{s}' in {s}.sie at byte {d}:\n" ++
                    "  got:      '{s}'\n" ++
                    "  expected: '{s}'\n" ++
                    "  dump_len={d} ref_len={d}\n",
                .{
                    ch.getName(),
                    file_base,
                    diff_pos,
                    dump[line_start..line_end],
                    reference[line_start..ref_line_end],
                    dump.len,
                    reference.len,
                },
            );
            return error.TestExpectedEqual;
        }
    }
}

test "functional: dump file - sie_min_timhis_a" {
    try testDumpFile("sie_min_timhis_a_19EFAA61");
}

test "functional: dump file - sie_comprehensive_VBM" {
    try testDumpFile("sie_comprehensive_VBM_DE81A7BA");
}

test "functional: dump file - sie_comprehensive2_VBM" {
    try testDumpFile("sie_comprehensive2_VBM_20050908");
}

test "functional: dump file - sie_float_conversions" {
    try testDumpFile("sie_float_conversions_20050908");
}
