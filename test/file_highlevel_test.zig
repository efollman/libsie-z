// High-level file tests — ports of t_file.c tests using SieFile API
// Tests: open_read_close, get_name, get_tag, get_dimension, scan_limit

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;

const test_sie_path = "test/data/sie_min_timhis_a_19EFAA61.sie";

test "file: open read close via SieFile" {
    // Port of test_open_read_close
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const channels = sf.channels();
    try testing.expect(channels.len > 0);

    // Find channel 1 and read all output through spigot
    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var row_count: usize = 0;
    while (try spigot.get()) |out| {
        row_count += out.num_rows;
    }
    try testing.expect(row_count > 0);
}

test "file: get channel name" {
    // Port of test_get_name
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    const name = ch.name;
    try testing.expectEqualStrings("timhis@Tri_10Hz.RN_1", name);
}

test "file: get tag values" {
    // Port of test_get_tag — verifies tag lookup on channel and file
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;

    // Channel tags
    const dm_tag = ch.findTag("DataMode") orelse return error.TestUnexpectedResult;
    try testing.expectEqualStrings("timhis", dm_tag.string().?);

    const desc_tag = ch.findTag("Description") orelse return error.TestUnexpectedResult;
    try testing.expectEqualStrings("Sim FG Tri_10Hz", desc_tag.string().?);

    // Nonexistent tag
    try testing.expect(ch.findTag("nonexistent") == null);

    // File-level tags
    const file_tags = sf.fileTags();
    var found_setup_name = false;
    for (file_tags) |t| {
        if (std.mem.eql(u8, t.key, "SIE:TCE_SetupName")) {
            try testing.expectEqualStrings("sie_min_timhis_a", t.string().?);
            found_setup_name = true;
        }
    }
    try testing.expect(found_setup_name);
}

test "file: get dimensions" {
    // Port of test_get_dimension — verifies dimension iteration
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    const dims = ch.dimensions();

    try testing.expectEqual(@as(usize, 2), dims.len);

    // Check dimension indices match
    for (dims, 0..) |dim, i| {
        try testing.expectEqual(@as(u32, @intCast(i)), dim.index);

        // getDimension by index should return same info
        const odim = ch.dimension(@intCast(i)) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(dim.index, odim.index);
        try testing.expectEqual(dim.decoder_id, odim.decoder_id);
    }

    // Out-of-bounds dimension should be null
    try testing.expect(ch.dimension(2) == null);
}

test "file: dimension decoder and group info" {
    // Verify dimension decoder_id, decoder_version, and group
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    const dims = ch.dimensions();

    // Both dimensions use decoder 2
    try testing.expectEqual(@as(u32, 2), dims[0].decoder_id);
    try testing.expectEqual(@as(u32, 2), dims[1].decoder_id);

    // v register indices: dim0=v0, dim1=v1
    try testing.expectEqual(@as(usize, 0), dims[0].decoder_version);
    try testing.expectEqual(@as(usize, 1), dims[1].decoder_version);

    // Group should be 4
    try testing.expectEqual(@as(u32, 4), dims[0].toplevel_group);
    try testing.expectEqual(@as(u32, 4), dims[1].toplevel_group);
}

test "file: dimension tags" {
    // Verify dimension tags match reference
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    const dims = ch.dimensions();

    // dim[0] should have SIE:units = sec
    var found_units = false;
    for (dims[0].tags()) |t| {
        if (std.mem.eql(u8, t.key, "SIE:units")) {
            try testing.expectEqualStrings("sec", t.string().?);
            found_units = true;
        }
    }
    try testing.expect(found_units);

    // dim[1] should have SIE:units = millivolts
    found_units = false;
    for (dims[1].tags()) |t| {
        if (std.mem.eql(u8, t.key, "SIE:units")) {
            try testing.expectEqualStrings("millivolts", t.string().?);
            found_units = true;
        }
    }
    try testing.expect(found_units);
}

test "file: two tests with one channel each" {
    // Verify the hierarchical structure matches expected
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const tests = sf.tests();
    try testing.expectEqual(@as(usize, 2), tests.len);

    // Test 0 has channel 1
    const test0_channels = tests[0].channels();
    try testing.expectEqual(@as(usize, 1), test0_channels.len);
    try testing.expectEqual(@as(u32, 1), test0_channels[0].id);

    // Test 1 has channel 2
    const test1_channels = tests[1].channels();
    try testing.expectEqual(@as(usize, 1), test1_channels.len);
    try testing.expectEqual(@as(u32, 2), test1_channels[0].id);
}

test "file: test tags" {
    // Verify test-level tags
    var sf = try SieFile.open(testing.allocator, test_sie_path);
    defer sf.deinit();

    const tests = sf.tests();

    // Test 0 should have StartTime, Run, etc
    const t0 = &tests[0];
    var found_run = false;
    for (t0.tags()) |t| {
        if (std.mem.eql(u8, t.key, "Run")) {
            try testing.expectEqualStrings("1", t.string().?);
            found_run = true;
        }
    }
    try testing.expect(found_run);

    // Test 1 should have Run = 2
    const t1 = &tests[1];
    found_run = false;
    for (t1.tags()) |t| {
        if (std.mem.eql(u8, t.key, "Run")) {
            try testing.expectEqualStrings("2", t.string().?);
            found_run = true;
        }
    }
    try testing.expect(found_run);
}

test "file: open nonexistent error" {
    // Port of test_open_nonexistent_error
    const result = SieFile.open(testing.allocator, "nonexistent_file.sie");
    try testing.expectError(error.FileNotFound, result);
}

test "file: open xml file error" {
    // Port of test_open_xml_error — XML file is not a valid SIE binary
    const result = SieFile.open(testing.allocator, "test/data/sie_comprehensive2_VBM_20050908.xml");
    try testing.expectError(error.InvalidBlock, result);
}
