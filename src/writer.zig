// SIE block writer
// Replaces sie_writer.h

const std = @import("std");
const block_mod = @import("block.zig");
const xml_mod = @import("xml.zig");

const FLUSH_SIZE: usize = 65536;
const SIE_OVERHEAD_SIZE: usize = block_mod.SIE_OVERHEAD_SIZE;
const SIE_MAGIC: u32 = block_mod.SIE_MAGIC;
const SIE_XML_GROUP: u32 = block_mod.SIE_XML_GROUP;
const SIE_INDEX_GROUP: u32 = block_mod.SIE_INDEX_GROUP;

pub const IdType = enum(u2) {
    Group = 0,
    Test = 1,
    Channel = 2,
    Decoder = 3,
};

pub const WriterFn = *const fn (user: ?*anyopaque, data: []const u8) usize;

const header_text =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" ++
    "<sie version=\"1.0\" xmlns=\"http://www.somat.com/SIE\">\n" ++
    "<!-- SIE format standard definitions: -->\n" ++
    " <!-- SIE stream decoder: -->\n" ++
    " <decoder id=\"0\">\n" ++
    "  <loop>\n" ++
    "   <read var=\"size\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n" ++
    "   <read var=\"group\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n" ++
    "   <read var=\"syncword\" bits=\"32\" type=\"uint\"\n" ++
    "         endian=\"big\" value=\"0x51EDA7A0\"/>\n" ++
    "   <read var=\"payload\" octets=\"{$size - 20}\" type=\"raw\"/>\n" ++
    "   <read var=\"checksum\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n" ++
    "   <read var=\"size2\" bits=\"32\" type=\"uint\"\n" ++
    "         endian=\"big\" value=\"{$size}\"/>\n" ++
    "  </loop>\n" ++
    " </decoder>\n" ++
    " <tag id=\"sie:xml_metadata\" group=\"0\" format=\"text/xml\"/>\n" ++
    "\n" ++
    " <!-- SIE index block decoder:  v0=offset, v1=group -->\n" ++
    " <decoder id=\"1\">\n" ++
    "  <loop>\n" ++
    "   <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"big\"/>\n" ++
    "   <read var=\"v1\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n" ++
    "   <sample/>\n" ++
    "  </loop>\n" ++
    " </decoder>\n" ++
    " <tag id=\"sie:block_index\" group=\"1\" decoder=\"1\"/>\n" ++
    "\n" ++
    "<!-- Stream-specific definitions begin here: -->\n" ++
    "\n";

pub const Writer = struct {
    allocator: std.mem.Allocator,
    writer_fn: ?WriterFn,
    user: ?*anyopaque,
    xml_buf: std.ArrayList(u8),
    index_buf: std.ArrayList(u8),
    offset: u64,
    do_index: bool,
    next_ids: [4]u32,

    pub fn init(allocator: std.mem.Allocator, writer_fn: ?WriterFn, user: ?*anyopaque) Writer {
        return .{
            .allocator = allocator,
            .writer_fn = writer_fn,
            .user = user,
            .xml_buf = .{},
            .index_buf = .{},
            .offset = 0,
            .do_index = true,
            .next_ids = .{ 2, 0, 0, 2 }, // groups=2, tests=0, channels=0, decoders=2
        };
    }

    pub fn deinit(self: *Writer) void {
        self.flushXml();
        self.flushIndex();
        self.xml_buf.deinit(self.allocator);
        self.index_buf.deinit(self.allocator);
    }

    /// Write a complete SIE block (header + payload + trailer).
    pub fn writeBlock(self: *Writer, group: u32, data: []const u8) !void {
        const block_size: u32 = @intCast(data.len + SIE_OVERHEAD_SIZE);

        if (self.writer_fn) |wfn| {
            // Build the block: [size_be][group_be][magic_be][payload][crc_be][size_be]
            var buf = try self.allocator.alloc(u8, block_size);
            defer self.allocator.free(buf);

            std.mem.writeInt(u32, buf[0..4], block_size, .big);
            std.mem.writeInt(u32, buf[4..8], group, .big);
            std.mem.writeInt(u32, buf[8..12], SIE_MAGIC, .big);
            @memcpy(buf[12 .. 12 + data.len], data);

            const crc = block_mod.crc32(buf[0 .. 12 + data.len]);
            std.mem.writeInt(u32, buf[12 + data.len ..][0..4], crc, .big);
            std.mem.writeInt(u32, buf[16 + data.len ..][0..4], block_size, .big);

            const out = wfn(self.user, buf[0..block_size]);
            if (out != block_size) return error.FileWriteError;
        }

        // Record index entry (except for index blocks themselves)
        if (self.do_index and group != SIE_INDEX_GROUP) {
            const group_be_bytes: [4]u8 = @bitCast(std.mem.nativeToBig(u32, group));
            const offset_be_bytes: [8]u8 = @bitCast(std.mem.nativeToBig(u64, self.offset));
            try self.index_buf.appendSlice(self.allocator, &offset_be_bytes);
            try self.index_buf.appendSlice(self.allocator, &group_be_bytes);
            self.maybeFlushIndex();
        }
        self.offset += block_size;
    }

    /// Append raw XML string to the XML buffer.
    pub fn xmlString(self: *Writer, data: []const u8) !void {
        try self.xml_buf.appendSlice(self.allocator, data);
        self.maybeFlushXml();
    }

    /// Serialize an XML node and append to the XML buffer.
    pub fn xmlNode(self: *Writer, node: *const xml_mod.Node) !void {
        try node.outputXml(self.allocator, &self.xml_buf, 0);
        self.maybeFlushXml();
    }

    /// Write the standard SIE XML header.
    pub fn xmlHeader(self: *Writer) !void {
        try self.xmlString(header_text);
    }

    /// Flush buffered XML as a group-0 block.
    pub fn flushXml(self: *Writer) void {
        if (self.xml_buf.items.len > 0) {
            self.writeBlock(SIE_XML_GROUP, self.xml_buf.items) catch {};
        }
        self.xml_buf.clearRetainingCapacity();
    }

    /// Get the next available ID for a given type.
    pub fn nextId(self: *Writer, id_type: IdType) u32 {
        const idx = @intFromEnum(id_type);
        const id = self.next_ids[idx];
        self.next_ids[idx] += 1;
        return id;
    }

    /// Calculate total size including pending buffers and additional data.
    pub fn totalSize(self: *const Writer, addl_bytes: u64, addl_blocks: u64) u64 {
        var total: u64 = self.offset;
        var index_size: u64 = self.index_buf.items.len;

        // Pending XML buffer
        if (self.xml_buf.items.len > 0) {
            total += self.xml_buf.items.len + SIE_OVERHEAD_SIZE;
            if (self.do_index) {
                index_size += 12;
                if (index_size > FLUSH_SIZE) {
                    total += index_size + SIE_OVERHEAD_SIZE;
                    index_size = 0;
                }
            }
        }

        // Additional data blocks
        total += addl_bytes + addl_blocks * SIE_OVERHEAD_SIZE;

        // Index blocks for additional blocks
        if (self.do_index) {
            var remaining_blocks = addl_blocks;
            while (remaining_blocks > 0) : (remaining_blocks -= 1) {
                index_size += 12;
                if (index_size > FLUSH_SIZE) {
                    total += index_size + SIE_OVERHEAD_SIZE;
                    index_size = 0;
                }
            }
            if (index_size > 0) {
                total += index_size + SIE_OVERHEAD_SIZE;
            }
        }

        return total;
    }

    fn flushIndex(self: *Writer) void {
        if (self.index_buf.items.len > 0) {
            self.writeBlock(SIE_INDEX_GROUP, self.index_buf.items) catch {};
        }
        self.index_buf.clearRetainingCapacity();
    }

    fn maybeFlushIndex(self: *Writer) void {
        if (self.index_buf.items.len > FLUSH_SIZE) {
            self.flushIndex();
        }
    }

    fn maybeFlushXml(self: *Writer) void {
        if (self.xml_buf.items.len > FLUSH_SIZE) {
            self.flushXml();
        }
    }
};

// ─── Tests ──────────────────────────────────────────────────────────

const testing = std.testing;

const TestContext = struct {
    data: std.ArrayList(u8),

    fn init(_: std.mem.Allocator) TestContext {
        return .{ .data = .{} };
    }

    fn deinit(self: *TestContext, allocator: std.mem.Allocator) void {
        self.data.deinit(allocator);
    }
};

fn testWriterFn(user: ?*anyopaque, data: []const u8) usize {
    const allocator = std.testing.allocator;
    const ctx: *TestContext = @ptrCast(@alignCast(user.?));
    ctx.data.appendSlice(allocator, data) catch return 0;
    return data.len;
}

test "writer basic block writing" {
    var ctx = TestContext.init(testing.allocator);
    defer ctx.deinit(testing.allocator);

    var writer = Writer.init(testing.allocator, testWriterFn, @ptrCast(&ctx));
    defer writer.deinit();

    // Write a block with 4 bytes of payload
    try writer.writeBlock(5, &[_]u8{ 1, 2, 3, 4 });

    // Block size = 4 + 20 = 24 bytes
    try testing.expectEqual(@as(u64, 24), writer.offset);
    try testing.expectEqual(@as(usize, 24), ctx.data.items.len);

    // Verify header: size=24 big-endian
    try testing.expectEqual(@as(u8, 0), ctx.data.items[0]);
    try testing.expectEqual(@as(u8, 0), ctx.data.items[1]);
    try testing.expectEqual(@as(u8, 0), ctx.data.items[2]);
    try testing.expectEqual(@as(u8, 24), ctx.data.items[3]);

    // Verify magic at offset 8
    const magic = std.mem.readInt(u32, ctx.data.items[8..12], .big);
    try testing.expectEqual(SIE_MAGIC, magic);

    // Verify payload at offset 12
    try testing.expectEqual(@as(u8, 1), ctx.data.items[12]);
    try testing.expectEqual(@as(u8, 2), ctx.data.items[13]);

    // Verify trailing size
    const trail_size = std.mem.readInt(u32, ctx.data.items[20..24], .big);
    try testing.expectEqual(@as(u32, 24), trail_size);

    // Index buf should have 12 bytes (one entry)
    try testing.expectEqual(@as(usize, 12), writer.index_buf.items.len);
}

test "writer xml buffering and flush" {
    var ctx = TestContext.init(testing.allocator);
    defer ctx.deinit(testing.allocator);

    var writer = Writer.init(testing.allocator, testWriterFn, @ptrCast(&ctx));
    defer writer.deinit();

    try writer.xmlString("<test>hello</test>");
    // Not yet flushed (under FLUSH_SIZE)
    try testing.expectEqual(@as(usize, 18), writer.xml_buf.items.len);
    try testing.expectEqual(@as(usize, 0), ctx.data.items.len);

    // Manual flush
    writer.flushXml();
    try testing.expectEqual(@as(usize, 0), writer.xml_buf.items.len);
    // Should have written one block (18 + 20 = 38 bytes)
    try testing.expectEqual(@as(usize, 38), ctx.data.items.len);

    // Verify it's a group-0 block
    const group = std.mem.readInt(u32, ctx.data.items[4..8], .big);
    try testing.expectEqual(SIE_XML_GROUP, group);
}

test "writer next_id generation" {
    var writer = Writer.init(testing.allocator, null, null);
    defer writer.deinit();

    // Groups start at 2
    try testing.expectEqual(@as(u32, 2), writer.nextId(.Group));
    try testing.expectEqual(@as(u32, 3), writer.nextId(.Group));

    // Decoders start at 2
    try testing.expectEqual(@as(u32, 2), writer.nextId(.Decoder));
    try testing.expectEqual(@as(u32, 3), writer.nextId(.Decoder));

    // Tests/Channels start at 0
    try testing.expectEqual(@as(u32, 0), writer.nextId(.Test));
    try testing.expectEqual(@as(u32, 0), writer.nextId(.Channel));
}

test "writer total_size calculation" {
    var writer = Writer.init(testing.allocator, null, null);
    defer writer.deinit();

    // Empty writer, no additional data
    try testing.expectEqual(@as(u64, 0), writer.totalSize(0, 0));

    // Add some xml to buffer
    try writer.xmlString("<hello/>");
    // total = 0 (offset) + 8+20 (xml block) + 12+20 (index block) = 60
    const size = writer.totalSize(0, 0);
    try testing.expect(size > 0);

    // With additional 100 bytes in 2 blocks
    const size2 = writer.totalSize(100, 2);
    try testing.expect(size2 > size);
}

test "writer xml header" {
    var ctx = TestContext.init(testing.allocator);
    defer ctx.deinit(testing.allocator);

    var writer = Writer.init(testing.allocator, testWriterFn, @ptrCast(&ctx));
    defer writer.deinit();

    try writer.xmlHeader();
    // Header should be buffered
    try testing.expect(writer.xml_buf.items.len > 100);
    try testing.expect(std.mem.indexOf(u8, writer.xml_buf.items, "<?xml") != null);
    try testing.expect(std.mem.indexOf(u8, writer.xml_buf.items, "0x51EDA7A0") != null);
}

test "writer index entries for non-index blocks" {
    var ctx = TestContext.init(testing.allocator);
    defer ctx.deinit(testing.allocator);

    var writer = Writer.init(testing.allocator, testWriterFn, @ptrCast(&ctx));
    defer writer.deinit();

    // Write a data block (group=5) - should create index entry
    try writer.writeBlock(5, &[_]u8{0xAA});
    try testing.expectEqual(@as(usize, 12), writer.index_buf.items.len);

    // Write an index block (group=1) - should NOT create index entry
    const prev_index_len = writer.index_buf.items.len;
    try writer.writeBlock(SIE_INDEX_GROUP, &[_]u8{0xBB});
    try testing.expectEqual(prev_index_len, writer.index_buf.items.len);
}

test "writer crc integrity" {
    var ctx = TestContext.init(testing.allocator);
    defer ctx.deinit(testing.allocator);

    var writer = Writer.init(testing.allocator, testWriterFn, @ptrCast(&ctx));
    defer writer.deinit();

    try writer.writeBlock(2, &[_]u8{ 0xDE, 0xAD });

    // Verify CRC: compute over header (12 bytes) + payload (2 bytes)
    const expected_crc = block_mod.crc32(ctx.data.items[0..14]);
    const stored_crc = std.mem.readInt(u32, ctx.data.items[14..18], .big);
    try testing.expectEqual(expected_crc, stored_crc);
}
