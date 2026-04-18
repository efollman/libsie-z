// File stream writing - writes SIE blocks to a file from a byte stream
// Replaces C sie_File_Stream (sie_file_stream_init, sie_file_stream_add_stream_data, etc.)
//
// FileStream accepts raw SIE byte stream data, incrementally reassembles
// complete blocks, writes them to a file, and maintains a group index.
// It implements the Intake interface for reading back written data.

const std = @import("std");
const block_mod = @import("block.zig");
const error_mod = @import("error.zig");
const file_mod = @import("file.zig");
const intake_mod = @import("intake.zig");
const xml_merge_mod = @import("xml_merge.zig");

pub const FileStream = struct {
    allocator: std.mem.Allocator,
    handle: ?std.fs.File = null,
    path: []const u8,
    group_indexes: std.AutoHashMap(u32, file_mod.FileGroupIndex),
    highest_group: u32 = 0,

    // Incremental block reassembly buffer
    recv_buf: std.ArrayList(u8),

    // Optional XML definition for auto-parsing group-0 blocks
    xml_definition: ?*xml_merge_mod.XmlDefinition = null,

    /// Create a new FileStream
    pub fn init(allocator: std.mem.Allocator, path: []const u8) FileStream {
        return .{
            .allocator = allocator,
            .handle = null,
            .path = path,
            .group_indexes = std.AutoHashMap(u32, file_mod.FileGroupIndex).init(allocator),
            .recv_buf = .{},
        };
    }

    /// Open the file for writing (create + truncate). Also opens for reading
    /// so blocks can be read back through the Intake interface.
    pub fn open(self: *FileStream) !void {
        self.handle = try std.fs.cwd().createFile(self.path, .{ .truncate = true, .read = true });
    }

    /// Close the file handle
    pub fn close(self: *FileStream) void {
        if (self.handle) |h| {
            h.close();
            self.handle = null;
        }
    }

    /// Clean up all resources
    pub fn deinit(self: *FileStream) void {
        self.close();
        var iter = self.group_indexes.valueIterator();
        while (iter.next()) |idx| {
            var idx_mut = idx;
            idx_mut.deinit(self.allocator);
        }
        self.group_indexes.deinit();
        self.recv_buf.deinit(self.allocator);
    }

    /// Add raw SIE stream data. Parses blocks incrementally and writes
    /// each complete block to the file. Returns number of bytes consumed
    /// (always equals data.len, matching C behavior).
    pub fn addStreamData(self: *FileStream, data: []const u8) !usize {
        if (data.len == 0) return 0;

        try self.recv_buf.appendSlice(self.allocator, data);

        var pos: usize = 0;
        while (true) {
            const remaining = self.recv_buf.items[pos..];
            if (remaining.len < block_mod.SIE_HEADER_SIZE) break;

            const block_size = std.mem.readInt(u32, remaining[0..4], .big);
            const magic = std.mem.readInt(u32, remaining[8..12], .big);

            if (magic != block_mod.SIE_MAGIC) {
                pos += 1;
                continue;
            }

            if (block_size < block_mod.SIE_OVERHEAD_SIZE) {
                pos += 1;
                continue;
            }

            // Wait for complete block
            if (remaining.len < block_size) break;

            // Validate trailer size field
            const payload_size = block_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE);
            const trailer_offset = block_mod.SIE_HEADER_SIZE + payload_size + 4;
            const trailer_size = std.mem.readInt(u32, remaining[trailer_offset..][0..4], .big);
            if (trailer_size != block_size) {
                pos += 1;
                continue;
            }

            const group = std.mem.readInt(u32, remaining[4..8], .big);
            try self.writeBlockToFile(remaining[0..block_size], group, block_size);

            pos += block_size;
        }

        // Compact: remove processed bytes
        if (pos > 0) {
            const leftover = self.recv_buf.items.len - pos;
            if (leftover > 0) {
                std.mem.copyForwards(u8, self.recv_buf.items[0..leftover], self.recv_buf.items[pos..]);
            }
            self.recv_buf.items.len = leftover;
        }

        return data.len;
    }

    /// Write a completed block to the file, index it, and optionally feed XML.
    fn writeBlockToFile(self: *FileStream, raw_data: []const u8, group: u32, block_size: u32) !void {
        const handle = self.handle orelse return error_mod.Error.FileWriteError;

        // Seek to end of file and write
        const offset = try handle.getEndPos();
        try handle.seekTo(offset);
        try handle.writeAll(raw_data);

        // Add to group index
        const payload_size = block_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE);
        const result = try self.group_indexes.getOrPut(group);
        if (!result.found_existing) {
            result.value_ptr.* = file_mod.FileGroupIndex.init(group);
        }
        try result.value_ptr.addEntry(self.allocator, offset, payload_size);

        if (group > self.highest_group) {
            self.highest_group = group;
        }

        // Feed group-0 (XML metadata) payloads to the XML definition
        if (group == block_mod.SIE_XML_GROUP) {
            if (self.xml_definition) |defn| {
                const payload = raw_data[block_mod.SIE_HEADER_SIZE .. block_mod.SIE_HEADER_SIZE + payload_size];
                try defn.addString(payload);
            }
        }
    }

    /// Check if a group has been marked closed
    pub fn isGroupClosed(self: *const FileStream, group: u32) bool {
        if (self.group_indexes.getPtr(group)) |idx| {
            return idx.is_closed;
        }
        return false;
    }

    /// Get a group index by ID
    pub fn getGroupIndex(self: *FileStream, group_id: u32) ?*file_mod.FileGroupIndex {
        return self.group_indexes.getPtr(group_id);
    }

    /// Get number of groups
    pub fn getNumGroups(self: *const FileStream) u32 {
        return @as(u32, @intCast(self.group_indexes.count()));
    }

    /// Get highest group ID seen
    pub fn getHighestGroup(self: *const FileStream) u32 {
        return self.highest_group;
    }

    /// Iterate over all groups, calling `callback` for each one.
    pub fn groupForEach(self: *FileStream, callback: *const fn (group_id: u32, index: *file_mod.FileGroupIndex, extra: ?*anyopaque) void, extra: ?*anyopaque) void {
        var iter = self.group_indexes.iterator();
        while (iter.next()) |entry| {
            callback(entry.key_ptr.*, entry.value_ptr, extra);
        }
    }

    /// Read a block at the given file offset (for reading back written data)
    pub fn readBlockAt(self: *FileStream, offset: u64) !block_mod.Block {
        const handle = self.handle orelse return error_mod.Error.FileReadError;

        try handle.seekTo(offset);

        var header_buf: [block_mod.SIE_HEADER_SIZE]u8 = undefined;
        const header_read = try handle.readAll(&header_buf);
        if (header_read != block_mod.SIE_HEADER_SIZE) {
            return error_mod.Error.UnexpectedEof;
        }

        const block_size = std.mem.readInt(u32, header_buf[0..4], .big);
        const group = std.mem.readInt(u32, header_buf[4..8], .big);
        const magic = std.mem.readInt(u32, header_buf[8..12], .big);

        if (magic != block_mod.SIE_MAGIC) return error_mod.Error.InvalidBlock;
        if (block_size < block_mod.SIE_OVERHEAD_SIZE) return error_mod.Error.InvalidBlock;

        const payload_size = block_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE);

        const payload = try self.allocator.alloc(u8, payload_size);
        errdefer self.allocator.free(payload);
        const payload_read = try handle.readAll(payload);
        if (payload_read != payload_size) return error_mod.Error.UnexpectedEof;

        var trailer_buf: [block_mod.SIE_TRAILER_SIZE]u8 = undefined;
        const trailer_read = try handle.readAll(&trailer_buf);
        if (trailer_read != block_mod.SIE_TRAILER_SIZE) return error_mod.Error.UnexpectedEof;

        const checksum = std.mem.readInt(u32, trailer_buf[0..4], .big);

        return block_mod.Block{
            .allocator = self.allocator,
            .group = group,
            .payload_size = payload_size,
            .max_size = payload_size,
            .checksum = checksum,
            .payload = payload,
        };
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

    fn ptrFromOpaque(ctx: *anyopaque) *FileStream {
        return @ptrCast(@alignCast(ctx));
    }

    fn vtableGetGroupHandle(ctx: *anyopaque, group: u32) ?intake_mod.GroupHandle {
        const self = ptrFromOpaque(ctx);
        if (self.getGroupIndex(group)) |ptr| {
            return @ptrCast(ptr);
        }
        return null;
    }

    fn vtableGetGroupNumBlocks(_: *anyopaque, handle: intake_mod.GroupHandle) usize {
        const idx: *file_mod.FileGroupIndex = @ptrCast(@alignCast(handle));
        return idx.getNumBlocks();
    }

    fn vtableGetGroupNumBytes(_: *anyopaque, handle: intake_mod.GroupHandle) u64 {
        const idx: *file_mod.FileGroupIndex = @ptrCast(@alignCast(handle));
        return idx.getNumBytes();
    }

    fn vtableGetGroupBlockSize(_: *anyopaque, handle: intake_mod.GroupHandle, entry: usize) u32 {
        const idx: *file_mod.FileGroupIndex = @ptrCast(@alignCast(handle));
        if (entry < idx.entries.items.len) {
            return idx.entries.items[entry].size;
        }
        return 0;
    }

    fn vtableReadGroupBlock(ctx: *anyopaque, handle: intake_mod.GroupHandle, entry: usize, blk: *block_mod.Block) error_mod.Error!void {
        const self = ptrFromOpaque(ctx);
        const idx: *file_mod.FileGroupIndex = @ptrCast(@alignCast(handle));

        if (entry >= idx.entries.items.len) {
            return error_mod.Error.IndexOutOfBounds;
        }

        const e = idx.entries.items[entry];
        const block_read = self.readBlockAt(e.offset) catch {
            return error_mod.Error.FileReadError;
        };

        blk.group = block_read.group;
        blk.payload_size = block_read.payload_size;
        blk.checksum = block_read.checksum;

        if (blk.max_size > 0) {
            self.allocator.free(blk.payload);
        }
        blk.payload = block_read.payload;
        blk.max_size = block_read.max_size;
    }

    fn vtableIsGroupClosed(_: *anyopaque, handle: intake_mod.GroupHandle) bool {
        const idx: *file_mod.FileGroupIndex = @ptrCast(@alignCast(handle));
        return idx.is_closed;
    }

    fn vtableAddStreamData(ctx: *anyopaque, data: []const u8) usize {
        const self = ptrFromOpaque(ctx);
        return self.addStreamData(data) catch 0;
    }

    fn vtableGetPtr(ctx: *anyopaque) *anyopaque {
        return ctx;
    }

    /// Create an Intake interface backed by this FileStream
    pub fn asIntake(self: *FileStream) intake_mod.Intake {
        return intake_mod.Intake.init(self.allocator, &vtable, @ptrCast(self));
    }
};
