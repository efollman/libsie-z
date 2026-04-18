// Stream-based data intake
// Replaces sie_stream.h
//
// Stream implements the Intake interface for consuming SIE data from
// a byte stream (e.g. network, pipe, or incremental file reading).
// It maintains a group index that tracks blocks as they arrive.

const std = @import("std");
const block_mod = @import("block.zig");
const error_mod = @import("error.zig");
const ref_mod = @import("ref.zig");
const intake_mod = @import("intake.zig");

/// Entry in a stream group index - points to a block payload in the buffer
pub const StreamGroupIndexEntry = struct {
    offset: usize, // offset into the data buffer
    size: u32, // payload size
};

/// Index for a single group within a stream
pub const StreamGroupIndex = struct {
    group_id: u32,
    entries: std.ArrayList(StreamGroupIndexEntry),
    payload_size: u64 = 0,
    is_closed: bool = false,

    pub fn init(group_id: u32) StreamGroupIndex {
        return StreamGroupIndex{
            .group_id = group_id,
            .entries = .{},
        };
    }

    pub fn deinit(self: *StreamGroupIndex, allocator: std.mem.Allocator) void {
        self.entries.deinit(allocator);
    }

    pub fn addEntry(self: *StreamGroupIndex, allocator: std.mem.Allocator, offset: usize, size: u32) !void {
        try self.entries.append(allocator, .{ .offset = offset, .size = size });
        self.payload_size += size;
    }

    pub fn getNumBlocks(self: *const StreamGroupIndex) usize {
        return self.entries.items.len;
    }

    pub fn getNumBytes(self: *const StreamGroupIndex) u64 {
        return self.payload_size;
    }
};

/// Stream - SIE data intake from a byte stream
pub const Stream = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,

    // Accumulated data buffer
    data: std.ArrayList(u8),

    // Partial block read state
    read_pos: usize = 0,

    // Group indexes: map group_id -> StreamGroupIndex
    group_indexes: std.AutoHashMap(u32, StreamGroupIndex),

    // Working block for parsing
    current_block: block_mod.Block,

    /// Create a new stream
    pub fn init(allocator: std.mem.Allocator) Stream {
        return Stream{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Other),
            .data = .{},
            .group_indexes = std.AutoHashMap(u32, StreamGroupIndex).init(allocator),
            .current_block = block_mod.Block.init(allocator),
        };
    }

    /// Clean up stream
    pub fn deinit(self: *Stream) void {
        var iter = self.group_indexes.valueIterator();
        while (iter.next()) |idx| {
            var idx_mut = idx;
            idx_mut.deinit(self.allocator);
        }
        self.group_indexes.deinit();
        self.data.deinit(self.allocator);
        self.current_block.deinit();
    }

    /// Add data to the stream and parse any complete blocks
    /// Returns number of bytes consumed
    pub fn addStreamData(self: *Stream, new_data: []const u8) !usize {
        try self.data.appendSlice(self.allocator, new_data);
        var consumed: usize = 0;

        // Try to parse complete blocks from the buffer
        while (self.read_pos + block_mod.SIE_HEADER_SIZE <= self.data.items.len) {
            const remaining = self.data.items[self.read_pos..];

            if (remaining.len < block_mod.SIE_HEADER_SIZE) break;

            // Peek at header to get payload size
            const payload_size = std.mem.readInt(u32, remaining[0..4], .big);
            const magic = std.mem.readInt(u32, remaining[8..12], .big);

            if (magic != block_mod.SIE_MAGIC) {
                // Not at a valid block boundary - skip a byte and retry
                self.read_pos += 1;
                consumed += 1;
                continue;
            }

            const total_size = block_mod.SIE_HEADER_SIZE + payload_size + block_mod.SIE_TRAILER_SIZE;
            if (remaining.len < total_size) break; // Need more data

            // Parse the complete block
            const block_data = remaining[0..total_size];
            const parsed = block_mod.Block.parseFromData(self.allocator, block_data) catch {
                self.read_pos += 1;
                consumed += 1;
                continue;
            };

            try self.addBlockToIndex(parsed.group, self.read_pos + block_mod.SIE_HEADER_SIZE, parsed.payload_size);

            // Clean up parsed block (we've recorded the index entry)
            var parsed_mut = parsed;
            parsed_mut.deinit();

            self.read_pos += total_size;
            consumed += total_size;
        }

        return consumed;
    }

    /// Add a block entry to the appropriate group index
    fn addBlockToIndex(self: *Stream, group_id: u32, offset: usize, size: u32) !void {
        const result = try self.group_indexes.getOrPut(group_id);
        if (!result.found_existing) {
            result.value_ptr.* = StreamGroupIndex.init(group_id);
        }
        try result.value_ptr.addEntry(self.allocator, offset, size);
    }

    /// Get a group index by ID (returns as opaque handle)
    pub fn getGroupHandle(self: *Stream, group_id: u32) ?*StreamGroupIndex {
        return self.group_indexes.getPtr(group_id);
    }

    /// Get number of blocks in a group
    pub fn getGroupNumBlocks(self: *const Stream, group_id: u32) usize {
        if (self.group_indexes.getPtr(group_id)) |idx| {
            return idx.getNumBlocks();
        }
        return 0;
    }

    /// Get total payload bytes in a group
    pub fn getGroupNumBytes(self: *const Stream, group_id: u32) u64 {
        if (self.group_indexes.getPtr(group_id)) |idx| {
            return idx.getNumBytes();
        }
        return 0;
    }

    /// Check if a group is closed
    pub fn isGroupClosed(self: *const Stream, group_id: u32) bool {
        if (self.group_indexes.getPtr(group_id)) |idx| {
            return idx.is_closed;
        }
        return false;
    }

    /// Get the raw data buffer
    pub fn getData(self: *const Stream) []const u8 {
        return self.data.items;
    }

    /// Get number of groups seen so far
    pub fn getNumGroups(self: *const Stream) u32 {
        return @as(u32, @intCast(self.group_indexes.count()));
    }

    /// Iterate over all groups, calling `callback` for each one.
    pub fn groupForEach(self: *Stream, callback: *const fn (group_id: u32, index: *StreamGroupIndex, extra: ?*anyopaque) void, extra: ?*anyopaque) void {
        var iter = self.group_indexes.iterator();
        while (iter.next()) |entry| {
            callback(entry.key_ptr.*, entry.value_ptr, extra);
        }
    }

    /// Get the payload size of a specific block within a group
    pub fn getGroupBlockSize(self: *const Stream, group_id: u32, entry: usize) u32 {
        if (self.group_indexes.getPtr(group_id)) |idx| {
            if (entry < idx.entries.items.len) {
                return idx.entries.items[entry].size;
            }
        }
        return 0;
    }

    /// Read a specific block from a group by entry index.
    /// Returns a copy of the block payload in a new Block.
    pub fn readGroupBlock(self: *Stream, group_id: u32, entry: usize) !block_mod.Block {
        const idx = self.group_indexes.getPtr(group_id) orelse
            return error_mod.Error.IndexOutOfBounds;

        if (entry >= idx.entries.items.len)
            return error_mod.Error.IndexOutOfBounds;

        const e = idx.entries.items[entry];
        if (e.offset + e.size > self.data.items.len)
            return error_mod.Error.InvalidBlock;

        var blk = block_mod.Block.init(self.allocator);
        blk.expand(e.size) catch return error_mod.Error.OutOfMemory;
        @memcpy(blk.payload[0..e.size], self.data.items[e.offset .. e.offset + e.size]);
        blk.payload_size = e.size;
        blk.group = idx.group_id;
        return blk;
    }

    // --- Intake vtable implementation ---

    pub const vtable = intake_mod.IntakeVTable{
        .getGroupHandle = vtableGetGroupHandle,
        .getGroupNumBlocks = vtableGetGroupNumBlocks,
        .getGroupNumBytes = vtableGetGroupNumBytes,
        .getGroupBlockSize = vtableGetGroupBlockSize,
        .readGroupBlock = vtableReadGroupBlock,
        .isGroupClosed = vtableIsGroupClosed,
        .addStreamData = vtableAddStreamData,
        .getPtr = vtableGetPtr,
    };

    fn vtableGetGroupHandle(ctx: *anyopaque, group: u32) ?intake_mod.GroupHandle {
        const self = ptrFromOpaque(ctx);
        if (self.getGroupHandle(group)) |ptr| {
            return @ptrCast(ptr);
        }
        return null;
    }

    fn vtableGetGroupNumBlocks(ctx: *anyopaque, handle: intake_mod.GroupHandle) usize {
        const idx: *StreamGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        return idx.getNumBlocks();
    }

    fn vtableGetGroupNumBytes(ctx: *anyopaque, handle: intake_mod.GroupHandle) u64 {
        const idx: *StreamGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        return idx.getNumBytes();
    }

    fn vtableGetGroupBlockSize(ctx: *anyopaque, handle: intake_mod.GroupHandle, entry: usize) u32 {
        const idx: *StreamGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        if (entry < idx.entries.items.len) {
            return idx.entries.items[entry].size;
        }
        return 0;
    }

    fn vtableReadGroupBlock(ctx: *anyopaque, handle: intake_mod.GroupHandle, entry: usize, blk: *block_mod.Block) error_mod.Error!void {
        const self = ptrFromOpaque(ctx);
        const idx: *StreamGroupIndex = @ptrCast(@alignCast(handle));

        if (entry >= idx.entries.items.len) {
            return error_mod.Error.IndexOutOfBounds;
        }

        const e = idx.entries.items[entry];
        // The payload is at offset e.offset in the data buffer
        if (e.offset + e.size > self.data.items.len) {
            return error_mod.Error.InvalidBlock;
        }

        blk.expand(e.size) catch return error_mod.Error.OutOfMemory;
        @memcpy(blk.payload[0..e.size], self.data.items[e.offset .. e.offset + e.size]);
        blk.payload_size = e.size;
        blk.group = idx.group_id;
    }

    fn vtableIsGroupClosed(ctx: *anyopaque, handle: intake_mod.GroupHandle) bool {
        const idx: *StreamGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        return idx.is_closed;
    }

    fn vtableAddStreamData(ctx: *anyopaque, data: []const u8) usize {
        const self = ptrFromOpaque(ctx);
        return self.addStreamData(data) catch 0;
    }

    fn vtableGetPtr(ctx: *anyopaque) *anyopaque {
        return ctx;
    }

    fn ptrFromOpaque(ctx: *anyopaque) *Stream {
        return @ptrCast(@alignCast(ctx));
    }

    /// Create an Intake interface backed by this stream
    pub fn asIntake(self: *Stream) intake_mod.Intake {
        return intake_mod.Intake.init(self.allocator, &vtable, @ptrCast(self));
    }
};

test "stream initialization" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var stream = Stream.init(allocator);
    defer stream.deinit();

    try std.testing.expectEqual(@as(u32, 0), stream.getNumGroups());
    try std.testing.expectEqual(@as(usize, 0), stream.getData().len);
}

test "stream add data with valid block" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var stream = Stream.init(allocator);
    defer stream.deinit();

    // Build a valid SIE block: header(12) + payload(5) + trailer(8)
    var buf: [25]u8 = undefined;
    var fbs = std.io.fixedBufferStream(&buf);
    const writer = fbs.writer();
    writer.writeInt(u32, 5, .big) catch unreachable; // payload size
    writer.writeInt(u32, 2, .big) catch unreachable; // group
    writer.writeInt(u32, block_mod.SIE_MAGIC, .big) catch unreachable; // magic
    _ = writer.write("hello") catch unreachable; // payload
    const checksum = block_mod.crc32("hello");
    writer.writeInt(u32, checksum, .big) catch unreachable; // checksum
    writer.writeInt(u32, block_mod.SIE_MAGIC, .big) catch unreachable; // trailer magic

    _ = try stream.addStreamData(&buf);

    try std.testing.expectEqual(@as(u32, 1), stream.getNumGroups());
    try std.testing.expectEqual(@as(usize, 1), stream.getGroupNumBlocks(2));
    try std.testing.expectEqual(@as(u64, 5), stream.getGroupNumBytes(2));
}

test "stream groupForEach" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var stream = Stream.init(allocator);
    defer stream.deinit();

    // Build two blocks in different groups
    inline for ([_]struct { group: u32, payload: []const u8 }{
        .{ .group = 2, .payload = "hello" },
        .{ .group = 7, .payload = "world" },
    }) |item| {
        var buf: [25]u8 = undefined;
        var fbs = std.io.fixedBufferStream(&buf);
        const writer = fbs.writer();
        writer.writeInt(u32, @intCast(item.payload.len), .big) catch unreachable;
        writer.writeInt(u32, item.group, .big) catch unreachable;
        writer.writeInt(u32, block_mod.SIE_MAGIC, .big) catch unreachable;
        _ = writer.write(item.payload) catch unreachable;
        writer.writeInt(u32, block_mod.crc32(item.payload), .big) catch unreachable;
        writer.writeInt(u32, block_mod.SIE_MAGIC, .big) catch unreachable;
        _ = try stream.addStreamData(&buf);
    }

    const State = struct {
        count: u32 = 0,
        fn callback(_: u32, _: *StreamGroupIndex, extra: ?*anyopaque) void {
            const self: *@This() = @ptrCast(@alignCast(extra.?));
            self.count += 1;
        }
    };

    var state = State{};
    stream.groupForEach(State.callback, @ptrCast(&state));
    try std.testing.expectEqual(@as(u32, 2), state.count);
}
