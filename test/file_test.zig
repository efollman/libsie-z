// File I/O tests
// Based on t_file.c

const std = @import("std");
const libsie = @import("libsie");
const File = libsie.file.File;
const Block = libsie.block;
const xml_mod = libsie.xml;

const testing = std.testing;

const test_sie_path = "test/data/sie_min_timhis_a_19EFAA61.sie";
const test_xml_path = "test/data/sie_min_timhis_a_19EFAA61.xml";
const comp_sie_path = "test/data/sie_comprehensive_VBM_DE81A7BA.sie";

test "file: open and close SIE file" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();

    // File should have non-zero size
    try testing.expect(file.size() > 0);
    try testing.expectEqual(@as(i64, 11122), file.size());
}

test "file: is SIE magic" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();

    try testing.expect(try file.isSie());
}

test "file: non-SIE file" {
    // XML file is not a SIE file
    var file = File.init(testing.allocator, test_xml_path);
    defer file.deinit();
    try file.open();

    try testing.expect(!try file.isSie());
}

test "file: build index" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    // Should have at least group 0 (XML) and some data groups
    const num_groups = file.getNumGroups();
    try testing.expect(num_groups > 0);

    // Group 0 (XML) should exist
    const xml_idx = file.getGroupIndex(0);
    try testing.expect(xml_idx != null);
    if (xml_idx) |idx| {
        try testing.expect(idx.getNumBlocks() > 0);
    }
}

test "file: read first block" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();

    // Read first block (should be XML block)
    try file.seek(0);
    var blk = try file.readBlock();
    defer blk.deinit();

    // First block should be group 0 (XML)
    try testing.expectEqual(@as(u32, Block.SIE_XML_GROUP), blk.getGroup());
    try testing.expect(blk.getPayloadSize() > 0);
}

test "file: read block at offset" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    // Read the first XML block using the index
    const xml_idx = file.getGroupIndex(0) orelse return error.TestUnexpectedResult;
    if (xml_idx.entries.items.len > 0) {
        const entry = xml_idx.entries.items[0];
        var blk = try file.readBlockAt(@intCast(entry.offset));
        defer blk.deinit();
        try testing.expectEqual(@as(u32, 0), blk.getGroup());
    }
}

test "file: comprehensive file groups" {
    var file = File.init(testing.allocator, comp_sie_path);
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    try testing.expect(try file.isSie());
    const num_groups = file.getNumGroups();
    // Comprehensive file should have multiple groups
    try testing.expect(num_groups >= 2);

    // Get highest group
    const highest = file.getHighestGroup();
    try testing.expect(highest >= 2);
}

test "file: seek and tell" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();

    try file.seek(0);
    try testing.expectEqual(@as(i64, 0), file.tell());

    try file.seek(100);
    try testing.expectEqual(@as(i64, 100), file.tell());

    try file.seekBy(-50);
    try testing.expectEqual(@as(i64, 50), file.tell());
}

test "file: XML block contains valid XML" {
    var file = File.init(testing.allocator, test_sie_path);
    defer file.deinit();
    try file.open();

    // Read first block (XML)
    try file.seek(0);
    var blk = try file.readBlock();
    defer blk.deinit();

    // Should be XML group and start with <?xml
    try testing.expectEqual(@as(u32, 0), blk.getGroup());
    const payload = blk.getPayload();
    try testing.expect(payload.len > 5);
    try testing.expect(std.mem.startsWith(u8, payload, "<?xml"));
}
