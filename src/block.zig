// Binary block structure - represents a SIE data block
// Replaces sie_block.h
//
// SIE block format (from C header):
//   Header: 12 bytes (size:u32, group:u32, magic:u32)
//   Payload: variable length
//   Trailer: 8 bytes (checksum:u32, magic:u32)
//   Total overhead = 20 bytes

const std = @import("std");
const error_mod = @import("error.zig");
const byteswap = @import("byteswap.zig");

/// SIE magic number identifying valid blocks
pub const SIE_MAGIC: u32 = 0x51EDA7A0;

/// Block header size in bytes
pub const SIE_HEADER_SIZE: usize = 12;

/// Block trailer size in bytes
pub const SIE_TRAILER_SIZE: usize = 8;

/// Total overhead per block (header + trailer)
pub const SIE_OVERHEAD_SIZE: usize = SIE_HEADER_SIZE + SIE_TRAILER_SIZE;

/// Well-known group IDs
pub const SIE_XML_GROUP: u32 = 0;
pub const SIE_INDEX_GROUP: u32 = 1;

/// On-disk block header (12 bytes, big-endian)
pub const BlockHeader = struct {
    size: u32, // payload size (or block size on output)
    group: u32,
    magic: u32,
};

/// On-disk block trailer (8 bytes)
pub const BlockTrailer = struct {
    checksum: u32,
    magic: u32,
};

/// CRC-32 lookup table (same polynomial as C implementation)
const crc_table: [256]u32 = blk: {
    @setEvalBranchQuota(10000);
    var table: [256]u32 = undefined;
    for (0..256) |i| {
        var crc: u32 = @intCast(i);
        for (0..8) |_| {
            if (crc & 1 != 0) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
        table[i] = crc;
    }
    break :blk table;
};

/// Compute CRC-32 checksum
pub fn crc32(buf: []const u8) u32 {
    var crc: u32 = 0xFFFFFFFF;
    for (buf) |byte| {
        crc = (crc >> 8) ^ crc_table[@as(u8, @truncate(crc ^ byte))];
    }
    return crc ^ 0xFFFFFFFF;
}

/// Data block - represents a single SIE binary block
pub const Block = struct {
    allocator: std.mem.Allocator,
    group: u32,
    payload_size: u32,
    max_size: u32,
    checksum: u32,
    payload: []u8,

    /// Create a new empty block with given capacity
    pub fn init(allocator: std.mem.Allocator) Block {
        return Block{
            .allocator = allocator,
            .group = 0,
            .payload_size = 0,
            .max_size = 0,
            .checksum = 0,
            .payload = &.{},
        };
    }

    /// Ensure the block can hold at least `size` bytes of payload
    pub fn expand(self: *Block, size: u32) !void {
        if (size > self.max_size) {
            const new_buf = try self.allocator.alloc(u8, size);
            if (self.max_size > 0) {
                const copy_len = @min(self.payload_size, size);
                @memcpy(new_buf[0..copy_len], self.payload[0..copy_len]);
                self.allocator.free(self.payload);
            }
            self.payload = new_buf;
            self.max_size = size;
        }
    }

    /// Parse a block from raw on-disk data
    pub fn parseFromData(allocator: std.mem.Allocator, data: []const u8) !Block {
        if (data.len < SIE_HEADER_SIZE) {
            return error_mod.Error.InvalidBlock;
        }

        // Read header fields (big-endian on disk)
        const raw_size = std.mem.readInt(u32, data[0..4], .big);
        const raw_group = std.mem.readInt(u32, data[4..8], .big);
        const raw_magic = std.mem.readInt(u32, data[8..12], .big);

        if (raw_magic != SIE_MAGIC) {
            return error_mod.Error.InvalidBlock;
        }

        const payload_size = raw_size;
        const total_size = SIE_HEADER_SIZE + payload_size + SIE_TRAILER_SIZE;

        if (data.len < total_size) {
            return error_mod.Error.InvalidBlock;
        }

        // Read trailer
        const trailer_start = SIE_HEADER_SIZE + payload_size;
        const stored_checksum = std.mem.readInt(u32, data[trailer_start..][0..4], .big);
        const trailer_magic = std.mem.readInt(u32, data[trailer_start + 4 ..][0..4], .big);

        if (trailer_magic != SIE_MAGIC) {
            return error_mod.Error.InvalidBlock;
        }

        // Copy payload
        const payload_data = data[SIE_HEADER_SIZE .. SIE_HEADER_SIZE + payload_size];
        const payload = try allocator.alloc(u8, payload_size);
        @memcpy(payload, payload_data);

        return Block{
            .allocator = allocator,
            .group = raw_group,
            .payload_size = payload_size,
            .max_size = payload_size,
            .checksum = stored_checksum,
            .payload = payload,
        };
    }

    /// Serialize block to writer (big-endian)
    pub fn writeTo(self: *const Block, writer: anytype) !void {
        // Header
        try writer.writeInt(u32, self.payload_size, .big);
        try writer.writeInt(u32, self.group, .big);
        try writer.writeInt(u32, SIE_MAGIC, .big);

        // Payload
        try writer.writeAll(self.getPayload());

        // Trailer
        const checksum = crc32(self.getPayload());
        try writer.writeInt(u32, checksum, .big);
        try writer.writeInt(u32, SIE_MAGIC, .big);
    }

    /// Get the group ID
    pub fn getGroup(self: *const Block) u32 {
        return self.group;
    }

    /// Get payload size
    pub fn getPayloadSize(self: *const Block) u32 {
        return self.payload_size;
    }

    /// Get total on-disk size (header + payload + trailer)
    pub fn getTotalSize(self: *const Block) u32 {
        return @as(u32, @intCast(SIE_OVERHEAD_SIZE)) + self.payload_size;
    }

    /// Get payload data
    pub fn getPayload(self: *const Block) []const u8 {
        if (self.payload_size == 0) return &.{};
        return self.payload[0..self.payload_size];
    }

    /// Get mutable payload data
    pub fn getPayloadMut(self: *Block) []u8 {
        if (self.payload_size == 0) return &.{};
        return self.payload[0..self.payload_size];
    }

    /// Validate block checksum
    pub fn validateChecksum(self: *const Block) bool {
        if (self.checksum == 0) return true; // no checksum stored
        const computed = crc32(self.getPayload());
        return computed == self.checksum;
    }

    /// Check if this is an XML block (group 0)
    pub fn isXml(self: *const Block) bool {
        return self.group == SIE_XML_GROUP;
    }

    /// Check if this is an index block (group 1)
    pub fn isIndex(self: *const Block) bool {
        return self.group == SIE_INDEX_GROUP;
    }

    /// Free allocated payload
    pub fn deinit(self: *Block) void {
        if (self.max_size > 0) {
            self.allocator.free(self.payload);
            self.payload = &.{};
            self.max_size = 0;
            self.payload_size = 0;
        }
    }
};

test "block create and serialize" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var block = Block.init(allocator);
    defer block.deinit();

    // Expand and fill payload
    try block.expand(10);
    @memcpy(block.payload[0..5], "hello");
    block.payload_size = 5;
    block.group = 2;

    // Serialize
    var buf: [100]u8 = undefined;
    var fbs = std.io.fixedBufferStream(&buf);
    try block.writeTo(fbs.writer());
    const written = fbs.getWritten();

    // Total should be header(12) + payload(5) + trailer(8) = 25
    try std.testing.expectEqual(@as(usize, 25), written.len);

    // Parse it back
    var parsed = try Block.parseFromData(allocator, written);
    defer parsed.deinit();

    try std.testing.expectEqual(@as(u32, 2), parsed.group);
    try std.testing.expectEqual(@as(u32, 5), parsed.payload_size);
    try std.testing.expectEqualSlices(u8, "hello", parsed.getPayload());
    try std.testing.expect(parsed.validateChecksum());
}

test "block invalid magic" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var invalid_data: [20]u8 = undefined;
    for (&invalid_data) |*b| {
        b.* = 0xFF;
    }

    const result = Block.parseFromData(allocator, &invalid_data);
    try std.testing.expectError(error_mod.Error.InvalidBlock, result);
}

test "crc32 basic" {
    const checksum = crc32("hello");
    try std.testing.expect(checksum != 0);

    // Same input should produce same checksum
    const checksum2 = crc32("hello");
    try std.testing.expectEqual(checksum, checksum2);

    // Different input should produce different checksum
    const checksum3 = crc32("world");
    try std.testing.expect(checksum != checksum3);
}
