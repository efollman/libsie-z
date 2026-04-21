// FileStream tests
// Tests for writing SIE blocks to a file via the file stream interface

const std = @import("std");
const libsie = @import("libsie");
const FileStream = libsie.FileStream;
const FileGroupIndex = libsie.advanced.file.FileGroupIndex;
const Block = libsie.advanced.block;
const Writer = libsie.advanced.writer.Writer;

const testing = std.testing;

const tmp_path = "zig-out/test_file_stream.sie";

/// Build a raw SIE block in Writer format: [block_size_be, group_be, magic_be, payload, crc_be, block_size_be]
fn buildRawBlock(allocator: std.mem.Allocator, group: u32, payload: []const u8) ![]u8 {
    const block_size: u32 = @intCast(payload.len + Block.SIE_OVERHEAD_SIZE);
    const buf = try allocator.alloc(u8, block_size);

    std.mem.writeInt(u32, buf[0..4], block_size, .big);
    std.mem.writeInt(u32, buf[4..8], group, .big);
    std.mem.writeInt(u32, buf[8..12], Block.SIE_MAGIC, .big);
    @memcpy(buf[12 .. 12 + payload.len], payload);

    const crc = Block.crc32(buf[0 .. 12 + payload.len]);
    std.mem.writeInt(u32, buf[12 + payload.len ..][0..4], crc, .big);
    std.mem.writeInt(u32, buf[16 + payload.len ..][0..4], block_size, .big);

    return buf;
}

test "file_stream: init and deinit" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();

    try testing.expectEqual(@as(u32, 0), fs.numGroups());
    try testing.expectEqual(@as(u32, 0), fs.highestGroup());
}

test "file_stream: open creates file" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    // File should exist
    const stat = try std.fs.cwd().statFile(tmp_path);
    try testing.expectEqual(@as(u64, 0), stat.size);
}

test "file_stream: write single block" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const payload = "hello world";
    const raw = try buildRawBlock(testing.allocator, 5, payload);
    defer testing.allocator.free(raw);

    const consumed = try fs.addStreamData(raw);
    try testing.expectEqual(raw.len, consumed);

    // Should have one group with one block
    try testing.expectEqual(@as(u32, 1), fs.numGroups());
    try testing.expectEqual(@as(u32, 5), fs.highestGroup());

    const idx = fs.groupIndex(5).?;
    try testing.expectEqual(@as(usize, 1), idx.numBlocks());
    try testing.expectEqual(@as(u64, payload.len), idx.numBytes());
}

test "file_stream: write multiple blocks" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const payload1 = "block one";
    const payload2 = "block two data";
    const raw1 = try buildRawBlock(testing.allocator, 3, payload1);
    defer testing.allocator.free(raw1);
    const raw2 = try buildRawBlock(testing.allocator, 3, payload2);
    defer testing.allocator.free(raw2);

    _ = try fs.addStreamData(raw1);
    _ = try fs.addStreamData(raw2);

    const idx = fs.groupIndex(3).?;
    try testing.expectEqual(@as(usize, 2), idx.numBlocks());
    try testing.expectEqual(@as(u64, payload1.len + payload2.len), idx.numBytes());
}

test "file_stream: write blocks to different groups" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const raw_g2 = try buildRawBlock(testing.allocator, 2, "group two");
    defer testing.allocator.free(raw_g2);
    const raw_g7 = try buildRawBlock(testing.allocator, 7, "group seven");
    defer testing.allocator.free(raw_g7);

    _ = try fs.addStreamData(raw_g2);
    _ = try fs.addStreamData(raw_g7);

    try testing.expectEqual(@as(u32, 2), fs.numGroups());
    try testing.expectEqual(@as(u32, 7), fs.highestGroup());

    try testing.expectEqual(@as(usize, 1), fs.groupIndex(2).?.numBlocks());
    try testing.expectEqual(@as(usize, 1), fs.groupIndex(7).?.numBlocks());
}

test "file_stream: read back written blocks" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const payload = "readback test payload";
    const raw = try buildRawBlock(testing.allocator, 4, payload);
    defer testing.allocator.free(raw);

    _ = try fs.addStreamData(raw);

    // Read back through the index
    const idx = fs.groupIndex(4).?;
    const entry = idx.entries.items[0];
    var blk = try fs.readBlockAt(entry.offset);
    defer blk.deinit();

    try testing.expectEqual(@as(u32, 4), blk.group);
    try testing.expectEqual(@as(u32, payload.len), blk.payload_size);
    try testing.expectEqualSlices(u8, payload, blk.payload());
}

test "file_stream: incremental data feeding" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const payload = "incremental test";
    const raw = try buildRawBlock(testing.allocator, 2, payload);
    defer testing.allocator.free(raw);

    // Feed data one byte at a time
    for (raw) |byte| {
        _ = try fs.addStreamData(&[_]u8{byte});
    }

    // Block should have been parsed and written
    try testing.expectEqual(@as(u32, 1), fs.numGroups());
    const idx = fs.groupIndex(2).?;
    try testing.expectEqual(@as(usize, 1), idx.numBlocks());

    // Verify readback
    var blk = try fs.readBlockAt(idx.entries.items[0].offset);
    defer blk.deinit();
    try testing.expectEqualSlices(u8, payload, blk.payload());
}

test "file_stream: multiple blocks in single addStreamData call" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const raw1 = try buildRawBlock(testing.allocator, 3, "first");
    defer testing.allocator.free(raw1);
    const raw2 = try buildRawBlock(testing.allocator, 3, "second");
    defer testing.allocator.free(raw2);

    // Concatenate both blocks into one buffer
    var combined = try testing.allocator.alloc(u8, raw1.len + raw2.len);
    defer testing.allocator.free(combined);
    @memcpy(combined[0..raw1.len], raw1);
    @memcpy(combined[raw1.len..], raw2);

    _ = try fs.addStreamData(combined);

    const idx = fs.groupIndex(3).?;
    try testing.expectEqual(@as(usize, 2), idx.numBlocks());

    // Verify both blocks
    var blk1 = try fs.readBlockAt(idx.entries.items[0].offset);
    defer blk1.deinit();
    try testing.expectEqualSlices(u8, "first", blk1.payload());

    var blk2 = try fs.readBlockAt(idx.entries.items[1].offset);
    defer blk2.deinit();
    try testing.expectEqualSlices(u8, "second", blk2.payload());
}

test "file_stream: intake vtable interface" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const raw = try buildRawBlock(testing.allocator, 5, "vtable test");
    defer testing.allocator.free(raw);
    _ = try fs.addStreamData(raw);

    var intake = fs.asIntake();

    // Get group handle
    const handle = intake.getGroupHandle(5).?;
    try testing.expectEqual(@as(usize, 1), intake.getGroupNumBlocks(handle));
    try testing.expectEqual(@as(u64, 11), intake.getGroupNumBytes(handle));
    try testing.expectEqual(@as(u32, 11), intake.getGroupBlockSize(handle, 0));

    // Read block through vtable
    var blk = Block.Block.init(testing.allocator);
    defer blk.deinit();
    try intake.readGroupBlock(handle, 0, &blk);
    try testing.expectEqualSlices(u8, "vtable test", blk.payload());

    // Non-existent group
    try testing.expectEqual(@as(?libsie.advanced.intake.GroupHandle, null), intake.getGroupHandle(99));
}

test "file_stream: writer integration roundtrip" {
    // Use Writer to produce SIE stream data, feed it to FileStream, read back
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    // Writer callback that feeds data into FileStream
    const Ctx = struct {
        fs: *FileStream,

        fn writerFn(user: ?*anyopaque, data: []const u8) usize {
            const self: *@This() = @ptrCast(@alignCast(user.?));
            return self.fs.addStreamData(data) catch 0;
        }
    };

    var ctx = Ctx{ .fs = &fs };
    var writer = Writer.init(testing.allocator, Ctx.writerFn, @ptrCast(&ctx));
    defer writer.deinit();

    // Write XML and data blocks through the Writer
    try writer.xmlString("<test>data</test>");
    writer.flushXml();

    try writer.writeBlock(5, &[_]u8{ 0xAA, 0xBB, 0xCC, 0xDD });

    // FileStream should have group 0 (XML) and group 5
    try testing.expect(fs.groupIndex(0) != null);
    try testing.expect(fs.groupIndex(5) != null);

    // Read back group-5 block
    const idx5 = fs.groupIndex(5).?;
    var blk = try fs.readBlockAt(idx5.entries.items[0].offset);
    defer blk.deinit();
    try testing.expectEqual(@as(u32, 4), blk.payload_size);
    try testing.expectEqualSlices(u8, &[_]u8{ 0xAA, 0xBB, 0xCC, 0xDD }, blk.payload());

    // Read back XML block
    const idx0 = fs.groupIndex(0).?;
    var xml_blk = try fs.readBlockAt(idx0.entries.items[0].offset);
    defer xml_blk.deinit();
    try testing.expectEqualStrings("<test>data</test>", xml_blk.payload());
}

test "file_stream: isGroupClosed defaults to false" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const raw = try buildRawBlock(testing.allocator, 2, "data");
    defer testing.allocator.free(raw);
    _ = try fs.addStreamData(raw);

    try testing.expect(!fs.isGroupClosed(2));
    try testing.expect(!fs.isGroupClosed(99)); // non-existent
}

test "file_stream: empty addStreamData" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const consumed = try fs.addStreamData(&[_]u8{});
    try testing.expectEqual(@as(usize, 0), consumed);
    try testing.expectEqual(@as(u32, 0), fs.numGroups());
}

test "file_stream: file persistence and correct offsets" {
    // Write blocks, close, verify file size matches expectations
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const payload1 = "alpha";
    const payload2 = "beta";
    const raw1 = try buildRawBlock(testing.allocator, 2, payload1);
    defer testing.allocator.free(raw1);
    const raw2 = try buildRawBlock(testing.allocator, 3, payload2);
    defer testing.allocator.free(raw2);

    _ = try fs.addStreamData(raw1);
    _ = try fs.addStreamData(raw2);

    // Second block should start right after the first
    const idx2 = fs.groupIndex(2).?;
    const idx3 = fs.groupIndex(3).?;
    try testing.expectEqual(@as(u64, 0), idx2.entries.items[0].offset);
    try testing.expectEqual(@as(u64, raw1.len), idx3.entries.items[0].offset);

    // Verify file size
    const handle = fs.handle.?;
    const file_size = try handle.getEndPos();
    try testing.expectEqual(@as(u64, raw1.len + raw2.len), file_size);
}

test "file_stream: groupForEach iterates all groups" {
    var fs = FileStream.init(testing.allocator, tmp_path);
    defer fs.deinit();
    try fs.open();

    const raw_g2 = try buildRawBlock(testing.allocator, 2, "group two");
    defer testing.allocator.free(raw_g2);
    const raw_g5 = try buildRawBlock(testing.allocator, 5, "group five");
    defer testing.allocator.free(raw_g5);
    const raw_g5b = try buildRawBlock(testing.allocator, 5, "group five again");
    defer testing.allocator.free(raw_g5b);

    _ = try fs.addStreamData(raw_g2);
    _ = try fs.addStreamData(raw_g5);
    _ = try fs.addStreamData(raw_g5b);

    const State = struct {
        group_count: u32 = 0,
        total_blocks: usize = 0,

        fn callback(group_id: u32, index: *libsie.advanced.file.FileGroupIndex, extra: ?*anyopaque) void {
            _ = group_id;
            const self: *@This() = @ptrCast(@alignCast(extra.?));
            self.group_count += 1;
            self.total_blocks += index.numBlocks();
        }
    };

    var state = State{};
    fs.groupForEach(State.callback, @ptrCast(&state));

    try testing.expectEqual(@as(u32, 2), state.group_count);
    try testing.expectEqual(@as(usize, 3), state.total_blocks);
}
