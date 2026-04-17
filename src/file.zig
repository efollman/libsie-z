// File-based data intake
// Replaces sie_file.h and sie_apr.c file I/O functions
//
// The File type provides both basic file I/O and the Intake interface
// for reading SIE files. It maintains a group index built by scanning
// the file for blocks, and supports random access to group data.

const std = @import("std");
const error_mod = @import("error.zig");
const block_mod = @import("block.zig");
const ref_mod = @import("ref.zig");
const intake_mod = @import("intake.zig");

/// Entry in a file group index - records offset and size of a block in the file
pub const FileGroupIndexEntry = struct {
    offset: u64, // byte offset of block header in file
    size: u32, // payload size
};

/// Index for a single group within a file
pub const FileGroupIndex = struct {
    group_id: u32,
    entries: std.ArrayList(FileGroupIndexEntry),
    payload_size: u64 = 0,
    is_closed: bool = false,

    pub fn init(group_id: u32) FileGroupIndex {
        return FileGroupIndex{
            .group_id = group_id,
            .entries = .{},
        };
    }

    pub fn deinit(self: *FileGroupIndex, allocator: std.mem.Allocator) void {
        self.entries.deinit(allocator);
    }

    pub fn addEntry(self: *FileGroupIndex, allocator: std.mem.Allocator, offset: u64, size: u32) !void {
        try self.entries.append(allocator, .{ .offset = offset, .size = size });
        self.payload_size += size;
    }

    pub fn getNumBlocks(self: *const FileGroupIndex) usize {
        return self.entries.items.len;
    }

    pub fn getNumBytes(self: *const FileGroupIndex) u64 {
        return self.payload_size;
    }
};

/// File handle wrapper for SIE file reading
pub const File = struct {
    path: []const u8,
    handle: ?std.fs.File = null,
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,
    position: i64 = 0,
    file_size: i64 = 0,

    // Group index: built by scanning the file
    group_indexes: std.AutoHashMap(u32, FileGroupIndex),
    is_indexed: bool = false,
    first_unindexed: i64 = 0,
    highest_group: u32 = 0,

    /// Create a file handle for the given path
    pub fn init(allocator: std.mem.Allocator, path: []const u8) File {
        return File{
            .path = path,
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.File),
            .group_indexes = std.AutoHashMap(u32, FileGroupIndex).init(allocator),
        };
    }

    /// Open the file for reading
    pub fn open(self: *File) !void {
        if (self.handle != null) {
            return error_mod.Error.FileOpenError;
        }

        self.handle = std.fs.cwd().openFile(self.path, .{
            .mode = .read_only,
        }) catch {
            return error_mod.Error.FileNotFound;
        };

        // Get file size
        if (self.handle) |h| {
            const stat = try h.stat();
            self.file_size = @as(i64, @intCast(stat.size));
        }
    }

    /// Close the file
    pub fn close(self: *File) void {
        if (self.handle) |h| {
            h.close();
            self.handle = null;
        }
    }

    /// Deinitialize and close
    pub fn deinit(self: *File) void {
        self.close();
        var iter = self.group_indexes.valueIterator();
        while (iter.next()) |idx| {
            var idx_mut = idx;
            idx_mut.deinit(self.allocator);
        }
        self.group_indexes.deinit();
    }

    /// Read bytes from current position
    pub fn read(self: *File, buffer: []u8) !usize {
        if (self.handle) |h| {
            const n = try h.read(buffer);
            self.position += @as(i64, @intCast(n));
            return n;
        }
        return error_mod.Error.FileReadError;
    }

    /// Seek to absolute position
    pub fn seek(self: *File, offset: i64) !void {
        if (self.handle) |h| {
            try h.seekTo(@as(u64, @intCast(offset)));
            self.position = offset;
        } else {
            return error_mod.Error.FileSeekError;
        }
    }

    /// Seek relative to current position
    pub fn seekBy(self: *File, delta: i64) !void {
        const new_pos = self.position + delta;
        if (new_pos < 0 or new_pos > self.file_size) {
            return error_mod.Error.FileSeekError;
        }
        try self.seek(new_pos);
    }

    /// Get current position
    pub fn tell(self: *const File) i64 {
        return self.position;
    }

    /// Get file size
    pub fn size(self: *const File) i64 {
        return self.file_size;
    }

    /// Check if at end of file
    pub fn isEof(self: *const File) bool {
        return self.position >= self.file_size;
    }

    /// Read entire file into memory
    pub fn readAll(self: *File) ![]u8 {
        if (self.handle == null) {
            return error_mod.Error.FileReadError;
        }

        const file_len = @as(usize, @intCast(self.file_size));
        const buffer = try self.allocator.alloc(u8, file_len);
        errdefer self.allocator.free(buffer);

        try self.seek(0);
        const n = try self.read(buffer);
        if (n != file_len) {
            return error_mod.Error.FileTruncated;
        }

        return buffer;
    }

    // --- Block reading ---

    /// Read a block at the given file offset
    pub fn readBlockAt(self: *File, offset: i64) !block_mod.Block {
        try self.seek(offset);

        var header_buf: [block_mod.SIE_HEADER_SIZE]u8 = undefined;
        const header_read = try self.read(&header_buf);
        if (header_read != block_mod.SIE_HEADER_SIZE) {
            return error_mod.Error.UnexpectedEof;
        }

        const block_size = std.mem.readInt(u32, header_buf[0..4], .big);
        const group = std.mem.readInt(u32, header_buf[4..8], .big);
        const magic = std.mem.readInt(u32, header_buf[8..12], .big);

        if (magic != block_mod.SIE_MAGIC) {
            return error_mod.Error.InvalidBlock;
        }

        if (block_size < block_mod.SIE_OVERHEAD_SIZE) {
            return error_mod.Error.InvalidBlock;
        }

        const payload_size = block_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE);

        // Read payload
        const payload = try self.allocator.alloc(u8, payload_size);
        errdefer self.allocator.free(payload);
        const payload_read = try self.read(payload);
        if (payload_read != payload_size) {
            return error_mod.Error.UnexpectedEof;
        }

        // Read trailer
        var trailer_buf: [block_mod.SIE_TRAILER_SIZE]u8 = undefined;
        const trailer_read = try self.read(&trailer_buf);
        if (trailer_read != block_mod.SIE_TRAILER_SIZE) {
            return error_mod.Error.UnexpectedEof;
        }

        const checksum = std.mem.readInt(u32, trailer_buf[0..4], .big);
        const trailer_size = std.mem.readInt(u32, trailer_buf[4..8], .big);

        // Trailer size field should match the original block size
        if (trailer_size < block_mod.SIE_OVERHEAD_SIZE) {
            return error_mod.Error.InvalidBlock;
        }
        if (trailer_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE) != payload_size) {
            return error_mod.Error.InvalidBlock;
        }

        return block_mod.Block{
            .allocator = self.allocator,
            .group = group,
            .payload_size = payload_size,
            .max_size = payload_size,
            .checksum = checksum,
            .payload = payload,
        };
    }

    /// Read the next block from current position
    pub fn readBlock(self: *File) !block_mod.Block {
        return self.readBlockAt(self.position);
    }

    /// Check if a file appears to be an SIE file (has valid magic at start)
    pub fn isSie(self: *File) !bool {
        if (self.file_size < block_mod.SIE_HEADER_SIZE) return false;

        try self.seek(0);
        var header_buf: [block_mod.SIE_HEADER_SIZE]u8 = undefined;
        const n = try self.read(&header_buf);
        if (n != block_mod.SIE_HEADER_SIZE) return false;

        const magic = std.mem.readInt(u32, header_buf[8..12], .big);
        return magic == block_mod.SIE_MAGIC;
    }

    // --- Group index management ---

    /// Scan the file and build group indexes
    pub fn buildIndex(self: *File) !void {
        if (self.is_indexed) return;

        try self.seek(0);

        while (self.position < self.file_size) {
            const block_offset = self.position;

            // Try to read header
            var header_buf: [block_mod.SIE_HEADER_SIZE]u8 = undefined;
            const n = self.read(&header_buf) catch break;
            if (n != block_mod.SIE_HEADER_SIZE) break;

            const block_size = std.mem.readInt(u32, header_buf[0..4], .big);
            const group = std.mem.readInt(u32, header_buf[4..8], .big);
            const magic = std.mem.readInt(u32, header_buf[8..12], .big);

            if (magic != block_mod.SIE_MAGIC) break;

            if (block_size < block_mod.SIE_OVERHEAD_SIZE) break;
            const payload_size = block_size - @as(u32, block_mod.SIE_OVERHEAD_SIZE);

            // Record in group index
            const result = try self.group_indexes.getOrPut(group);
            if (!result.found_existing) {
                result.value_ptr.* = FileGroupIndex.init(group);
            }
            try result.value_ptr.addEntry(self.allocator, @intCast(block_offset), payload_size);

            if (group > self.highest_group) {
                self.highest_group = group;
            }

            // Skip payload + trailer
            const skip = @as(i64, @intCast(payload_size + block_mod.SIE_TRAILER_SIZE));
            self.seekBy(skip) catch break;
        }

        self.is_indexed = true;
    }

    /// Get a group index by ID
    pub fn getGroupIndex(self: *File, group_id: u32) ?*FileGroupIndex {
        return self.group_indexes.getPtr(group_id);
    }

    /// Get number of groups in the file
    pub fn getNumGroups(self: *const File) u32 {
        return @as(u32, @intCast(self.group_indexes.count()));
    }

    /// Get the highest group ID seen
    pub fn getHighestGroup(self: *const File) u32 {
        return self.highest_group;
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

    fn ptrFromOpaque(ctx: *anyopaque) *File {
        return @ptrCast(@alignCast(ctx));
    }

    fn vtableGetGroupHandle(ctx: *anyopaque, group: u32) ?intake_mod.GroupHandle {
        const self = ptrFromOpaque(ctx);
        if (self.getGroupIndex(group)) |ptr| {
            return @ptrCast(ptr);
        }
        return null;
    }

    fn vtableGetGroupNumBlocks(ctx: *anyopaque, handle: intake_mod.GroupHandle) usize {
        const idx: *FileGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        return idx.getNumBlocks();
    }

    fn vtableGetGroupNumBytes(ctx: *anyopaque, handle: intake_mod.GroupHandle) u64 {
        const idx: *FileGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        return idx.getNumBytes();
    }

    fn vtableGetGroupBlockSize(ctx: *anyopaque, handle: intake_mod.GroupHandle, entry: usize) u32 {
        const idx: *FileGroupIndex = @ptrCast(@alignCast(handle));
        _ = ctx;
        if (entry < idx.entries.items.len) {
            return idx.entries.items[entry].size;
        }
        return 0;
    }

    fn vtableReadGroupBlock(ctx: *anyopaque, handle: intake_mod.GroupHandle, entry: usize, blk: *block_mod.Block) error_mod.Error!void {
        const self = ptrFromOpaque(ctx);
        const idx: *FileGroupIndex = @ptrCast(@alignCast(handle));

        if (entry >= idx.entries.items.len) {
            return error_mod.Error.IndexOutOfBounds;
        }

        const e = idx.entries.items[entry];
        const block_read = self.readBlockAt(@intCast(e.offset)) catch {
            return error_mod.Error.FileReadError;
        };

        blk.group = block_read.group;
        blk.payload_size = block_read.payload_size;
        blk.checksum = block_read.checksum;

        // Transfer payload ownership
        if (blk.max_size > 0) {
            self.allocator.free(blk.payload);
        }
        blk.payload = block_read.payload;
        blk.max_size = block_read.max_size;
    }

    fn vtableIsGroupClosed(_: *anyopaque, _: intake_mod.GroupHandle) bool {
        return true; // File groups are always "closed" (complete)
    }

    fn vtableAddStreamData(_: *anyopaque, _: []const u8) usize {
        return 0; // File doesn't accept stream data
    }

    fn vtableGetPtr(ctx: *anyopaque) *anyopaque {
        return ctx;
    }

    /// Create an Intake interface backed by this file
    pub fn asIntake(self: *File) intake_mod.Intake {
        return intake_mod.Intake.init(self.allocator, &vtable, @ptrCast(self));
    }
};

test "file open and read" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Test the initialization
    var file = File.init(allocator, "test.bin");
    defer file.deinit();

    try std.testing.expectEqual(file.position, 0);
    try std.testing.expectEqual(file.tell(), 0);
}

test "file position tracking" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var file = File.init(allocator, "test.bin");
    defer file.deinit();

    try std.testing.expectEqual(file.tell(), 0);
    try std.testing.expectEqual(file.size(), 0);
    try std.testing.expectEqual(@as(u32, 0), file.getNumGroups());
}

test "file group index structure" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var idx = FileGroupIndex.init(2);
    defer idx.deinit(allocator);

    try idx.addEntry(allocator, 0, 100);
    try idx.addEntry(allocator, 200, 50);

    try std.testing.expectEqual(@as(usize, 2), idx.getNumBlocks());
    try std.testing.expectEqual(@as(u64, 150), idx.getNumBytes());
}
