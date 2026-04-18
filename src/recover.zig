// Error recovery - file corruption recovery tool
// Replaces sie_recover.h / recover.c
//
// Implements a 3-pass file recovery algorithm:
//  Pass 1: Scan for SIE magic bytes, extract contiguous block runs
//  Pass 2: Try to glue adjacent parts together (ordered pairs)
//  Pass 3: Try all part pairs for glue blocks
//
// Replaces APR file I/O with Zig std.fs.

const std = @import("std");
const block_mod = @import("block.zig");

/// Summary of an index block within a recovered part
pub const IndexSummary = struct {
    offset: u64,
    size: u64,
    first_offset: u64,
    last_offset: u64,
};

/// A contiguous run of valid SIE blocks found during recovery
pub const RecoverPart = struct {
    offset: u64,
    size: u64,
    before_size: u32,
    after_size: u32,
    before_glue_ids: std.ArrayList(usize),
    after_glue_ids: std.ArrayList(usize),
    indexes: std.ArrayList(IndexSummary),

    pub fn init(allocator: std.mem.Allocator) RecoverPart {
        _ = allocator;
        return .{
            .offset = 0,
            .size = 0,
            .before_size = 0,
            .after_size = 0,
            .before_glue_ids = .{},
            .after_glue_ids = .{},
            .indexes = .{},
        };
    }

    pub fn deinit(self: *RecoverPart, allocator: std.mem.Allocator) void {
        self.before_glue_ids.deinit(allocator);
        self.after_glue_ids.deinit(allocator);
        self.indexes.deinit(allocator);
    }
};

/// A block that bridges two recovered parts
pub const GlueEntry = struct {
    before_part: usize,
    after_part: usize,
    block_size: u64,
    glue_id: ?usize,
    indexes: std.ArrayList(IndexSummary),

    pub fn deinit(self: *GlueEntry, allocator: std.mem.Allocator) void {
        self.indexes.deinit(allocator);
    }
};

/// GlueKey for dedup
const GlueKey = struct {
    before_part: usize,
    after_part: usize,
    block_size: u64,
};

/// SIE magic constant in big-endian for scanning
const SIE_MAGIC_BE = std.mem.nativeToBig(u32, block_mod.SIE_MAGIC);

/// Recovery result
pub const RecoverResult = struct {
    parts: std.ArrayList(RecoverPart),
    glue_entries: std.ArrayList(GlueEntry),

    pub fn deinit(self: *RecoverResult, allocator: std.mem.Allocator) void {
        for (self.parts.items) |*p| p.deinit(allocator);
        self.parts.deinit(allocator);
        for (self.glue_entries.items) |*g| g.deinit(allocator);
        self.glue_entries.deinit(allocator);
    }

    /// Serialize recovery results to JSON
    pub fn toJson(self: *const RecoverResult, allocator: std.mem.Allocator) ![]u8 {
        var buf = std.ArrayList(u8){};
        defer buf.deinit(allocator);
        const writer = buf.writer(allocator);

        try writer.writeAll("{\n  \"parts\": [");
        for (self.parts.items, 0..) |part, pi| {
            if (pi > 0) try writer.writeAll(",");
            try writer.print("\n    {{\"offset\": {d}, \"size\": {d}, \"before_size\": {d}, \"after_size\": {d}}}", .{
                part.offset, part.size, part.before_size, part.after_size,
            });
        }
        try writer.writeAll("\n  ],\n  \"glue\": [");
        for (self.glue_entries.items, 0..) |entry, gi| {
            if (gi > 0) try writer.writeAll(",");
            if (entry.glue_id) |gid| {
                try writer.print("\n    {{\"glue_id\": {d}, \"size\": {d}}}", .{ gid, entry.block_size });
            }
        }
        try writer.writeAll("\n  ]\n}\n");

        return allocator.dupe(u8, buf.items);
    }
};

/// Try to glue two buffer halves together to form a valid SIE block.
/// Returns the split point if successful, null otherwise.
pub fn tryGlue(left: []const u8, right: []const u8, size: usize, offset: u64, mod: u64) ?usize {
    if (size < block_mod.SIE_OVERHEAD_SIZE or size > 1048576) return null;

    var block_buf: [1048576 + 20]u8 = undefined;
    if (size > block_buf.len) return null;
    const data = block_buf[0..size];

    var split: usize = 1;
    while (split < size - 1) : (split += 1) {
        if (mod > 0 and (offset + split) % mod != 0) continue;

        // Assemble candidate block
        @memcpy(data[0..split], left[0..split]);
        @memcpy(data[split..size], right[split..size]);

        // Check header: [size_be | group_be | magic_be | payload...]
        if (size < 12) continue;
        const block_size = std.mem.readInt(u32, data[0..4], .big);
        if (block_size != size) continue;
        const magic = std.mem.readInt(u32, data[8..12], .big);
        if (magic != block_mod.SIE_MAGIC) continue;

        // Check trailer
        const payload_size = block_size - block_mod.SIE_OVERHEAD_SIZE;
        const trailer_offset = 12 + payload_size;
        if (trailer_offset + 8 > size) continue;
        const trailer_crc = std.mem.readInt(u32, data[trailer_offset..][0..4], .big);
        const trailer_size = std.mem.readInt(u32, data[trailer_offset + 4 ..][0..4], .big);
        if (trailer_size != block_size) continue;

        // CRC check
        const computed_crc = block_mod.crc32(data[0 .. 12 + payload_size]);
        if (computed_crc != trailer_crc) continue;

        return split;
    }
    return null;
}

/// Scan a file for SIE magic bytes and find contiguous block runs.
/// This is a simplified version of the C 3-pass recovery.
pub fn scanForMagic(allocator: std.mem.Allocator, data: []const u8) !std.ArrayList(u64) {
    var offsets = std.ArrayList(u64){};
    if (data.len < 12) return offsets;

    // Look for SIE_MAGIC at every 4-byte alignment (magic is at offset 8 in block)
    var i: usize = 0;
    while (i + 4 <= data.len) : (i += 1) {
        if (i + 4 <= data.len) {
            const word = std.mem.readInt(u32, data[i..][0..4], .big);
            if (word == block_mod.SIE_MAGIC and i >= 8) {
                // Magic found at position i, block starts at i-8
                try offsets.append(allocator, i - 8);
            }
        }
    }
    return offsets;
}

/// Top-level file recovery: scans a file for magic bytes, extracts contiguous
/// block runs, and attempts to glue adjacent parts together.
/// Returns a RecoverResult with all discovered parts and glue entries.
pub fn recover(allocator: std.mem.Allocator, path: []const u8, mod: u64) !RecoverResult {
    // Open and read the file
    const cwd = std.fs.cwd();
    const file_handle = cwd.openFile(path, .{ .mode = .read_only }) catch {
        return error.FileNotFound;
    };
    defer file_handle.close();

    const stat = try file_handle.stat();
    const file_size = stat.size;
    if (file_size == 0) {
        return RecoverResult{ .parts = .{}, .glue_entries = .{} };
    }

    const data = try allocator.alloc(u8, file_size);
    defer allocator.free(data);
    const n = try file_handle.readAll(data);
    if (n != file_size) {
        return error.UnexpectedEof;
    }

    var result = RecoverResult{ .parts = .{}, .glue_entries = .{} };
    errdefer result.deinit(allocator);

    // Pass 1: Find all magic byte positions and extract contiguous block runs
    var magic_offsets = try scanForMagic(allocator, data);
    defer magic_offsets.deinit(allocator);

    // Group consecutive valid blocks into parts
    var part_start: ?u64 = null;
    var part_end: u64 = 0;
    var before_size: u32 = 0;

    for (magic_offsets.items) |block_offset| {
        // Validate this is a real block
        const off = @as(usize, @intCast(block_offset));
        if (off + block_mod.SIE_HEADER_SIZE > data.len) continue;

        const block_size = std.mem.readInt(u32, data[off..][0..4], .big);
        if (block_size < block_mod.SIE_OVERHEAD_SIZE) continue;
        if (off + block_size > data.len) continue;

        const magic = std.mem.readInt(u32, data[off + 8 ..][0..4], .big);
        if (magic != block_mod.SIE_MAGIC) continue;

        // Verify CRC
        const payload_size = block_size - block_mod.SIE_OVERHEAD_SIZE;
        const trailer_off = off + 12 + payload_size;
        if (trailer_off + 8 > data.len) continue;
        const trailer_crc = std.mem.readInt(u32, data[trailer_off..][0..4], .big);
        const trailer_size = std.mem.readInt(u32, data[trailer_off + 4 ..][0..4], .big);
        if (trailer_size != block_size) continue;
        const computed_crc = block_mod.crc32(data[off .. off + 12 + payload_size]);
        if (computed_crc != trailer_crc) continue;

        const block_end = block_offset + block_size;

        if (part_start == null) {
            part_start = block_offset;
            part_end = block_end;
            // Read before_size: 4 bytes before the block start
            if (block_offset >= 4) {
                before_size = std.mem.readInt(u32, data[@intCast(block_offset - 4)..][0..4], .big);
            } else {
                before_size = 0;
            }
        } else if (block_offset == part_end) {
            // Contiguous with current part
            part_end = block_end;
        } else {
            // Gap: finalize current part and start a new one
            var after_size: u32 = 0;
            if (part_end + 4 <= data.len) {
                after_size = std.mem.readInt(u32, data[@intCast(part_end)..][0..4], .big);
            }
            try result.parts.append(allocator, .{
                .offset = part_start.?,
                .size = part_end - part_start.?,
                .before_size = before_size,
                .after_size = after_size,
                .before_glue_ids = .{},
                .after_glue_ids = .{},
                .indexes = .{},
            });
            part_start = block_offset;
            part_end = block_end;
            if (block_offset >= 4) {
                before_size = std.mem.readInt(u32, data[@intCast(block_offset - 4)..][0..4], .big);
            } else {
                before_size = 0;
            }
        }
    }

    // Finalize last part
    if (part_start != null) {
        var after_size: u32 = 0;
        if (part_end + 4 <= data.len) {
            after_size = std.mem.readInt(u32, data[@intCast(part_end)..][0..4], .big);
        }
        try result.parts.append(allocator, .{
            .offset = part_start.?,
            .size = part_end - part_start.?,
            .before_size = before_size,
            .after_size = after_size,
            .before_glue_ids = .{},
            .after_glue_ids = .{},
            .indexes = .{},
        });
    }

    // Pass 2 & 3: Try to glue adjacent parts
    var next_glue_id: usize = 0;
    const num_parts = result.parts.items.len;

    if (num_parts >= 2) {
        // Pass 2: Ordered pairs (adjacent parts)
        for (0..num_parts - 1) |pi| {
            const p1 = &result.parts.items[pi];
            const p2 = &result.parts.items[pi + 1];
            const sz = p1.after_size;
            if (sz < block_mod.SIE_OVERHEAD_SIZE or sz > 1048576) continue;

            const p1_end = @as(usize, @intCast(p1.offset + p1.size));
            const p2_start = @as(usize, @intCast(p2.offset));
            if (p1_end + sz > data.len or p2_start + sz > data.len) continue;

            const left = data[p1_end .. p1_end + sz];
            const right = data[p2_start - @min(p2_start, sz) ..][0..sz];

            if (tryGlue(left, right, sz, p1.offset + p1.size, mod) != null) {
                try p1.after_glue_ids.append(allocator, next_glue_id);
                try p2.before_glue_ids.append(allocator, next_glue_id);
                try result.glue_entries.append(allocator, .{
                    .before_part = pi,
                    .after_part = pi + 1,
                    .block_size = sz,
                    .glue_id = next_glue_id,
                    .indexes = .{},
                });
                next_glue_id += 1;
            }
        }

        // Pass 3: All pairs
        for (0..num_parts) |pi| {
            for (0..num_parts) |qi| {
                if (pi == qi) continue;
                // Skip already-glued ordered pairs
                if (qi == pi + 1 and result.parts.items[pi].after_glue_ids.items.len > 0) continue;

                const p1 = &result.parts.items[pi];
                const p2 = &result.parts.items[qi];
                const sz = p1.after_size;
                if (sz < block_mod.SIE_OVERHEAD_SIZE or sz > 1048576) continue;

                const p1_end = @as(usize, @intCast(p1.offset + p1.size));
                if (p1_end + sz > data.len) continue;
                const p2_start = @as(usize, @intCast(p2.offset));
                if (p2_start < sz and p2_start > 0) continue;
                if (p2_start + sz > data.len) continue;

                const left = data[p1_end .. p1_end + sz];
                const right = if (p2_start >= sz) data[p2_start - sz ..][0..sz] else continue;

                if (tryGlue(left, right, sz, p1.offset + p1.size, mod) != null) {
                    try p1.after_glue_ids.append(allocator, next_glue_id);
                    try p2.before_glue_ids.append(allocator, next_glue_id);
                    try result.glue_entries.append(allocator, .{
                        .before_part = pi,
                        .after_part = qi,
                        .block_size = sz,
                        .glue_id = next_glue_id,
                        .indexes = .{},
                    });
                    next_glue_id += 1;
                }
            }
        }
    }

    return result;
}

const error_set = error{
    FileNotFound,
    UnexpectedEof,
};

// ---- Tests ----

const testing = std.testing;

test "recover scan for magic" {
    const allocator = testing.allocator;

    // Build a minimal valid SIE block:
    // [size_be(4) | group_be(4) | magic_be(4) | payload | crc_be(4) | size_be(4)]
    const payload = "hello";
    const total_size: u32 = @intCast(block_mod.SIE_OVERHEAD_SIZE + payload.len);
    var buf: [100]u8 = undefined;
    std.mem.writeInt(u32, buf[0..4], total_size, .big);
    std.mem.writeInt(u32, buf[4..8], 2, .big); // group
    std.mem.writeInt(u32, buf[8..12], block_mod.SIE_MAGIC, .big);
    @memcpy(buf[12..17], payload);
    const crc = block_mod.crc32(buf[0..17]);
    std.mem.writeInt(u32, buf[17..21], crc, .big);
    std.mem.writeInt(u32, buf[21..25], total_size, .big);

    var offsets = try scanForMagic(allocator, buf[0..25]);
    defer offsets.deinit(allocator);

    try testing.expectEqual(@as(usize, 1), offsets.items.len);
    try testing.expectEqual(@as(u64, 0), offsets.items[0]);
}

test "recover try glue" {
    // Build a valid block, split it, and verify glue works
    const payload = "test_data";
    const total_size: u32 = @intCast(block_mod.SIE_OVERHEAD_SIZE + payload.len);
    var block_data: [100]u8 = undefined;
    std.mem.writeInt(u32, block_data[0..4], total_size, .big);
    std.mem.writeInt(u32, block_data[4..8], 2, .big);
    std.mem.writeInt(u32, block_data[8..12], block_mod.SIE_MAGIC, .big);
    @memcpy(block_data[12..21], payload);
    const crc = block_mod.crc32(block_data[0..21]);
    std.mem.writeInt(u32, block_data[21..25], crc, .big);
    std.mem.writeInt(u32, block_data[25..29], total_size, .big);

    const size = total_size;

    // Split at position 10
    const left = block_data[0..size];
    const right = block_data[0..size];

    // With mod=0 (no alignment constraint), should find the correct split
    const result = tryGlue(left, right, size, 0, 0);
    // The split point doesn't matter since left==right (same block), but
    // any split should reconstruct the same block. Actually with mod=0 the
    // loop skips because (offset+split)%0 is division by zero. Use mod=1.
    _ = result;
    const result2 = tryGlue(left, right, size, 0, 1);
    // Since left and right are the same complete block, every split should work.
    // The first valid split should be 1.
    try testing.expect(result2 != null);
}

test "recover result json" {
    const allocator = testing.allocator;
    var result = RecoverResult{
        .parts = .{},
        .glue_entries = .{},
    };
    defer result.deinit(allocator);

    var part = RecoverPart{
        .offset = 0,
        .size = 100,
        .before_size = 4,
        .after_size = 8,
        .before_glue_ids = .{},
        .after_glue_ids = .{},
        .indexes = .{},
    };
    try result.parts.append(allocator, part);
    _ = &part;

    const json = try result.toJson(allocator);
    defer allocator.free(json);

    try testing.expect(std.mem.indexOf(u8, json, "\"offset\": 0") != null);
    try testing.expect(std.mem.indexOf(u8, json, "\"size\": 100") != null);
}
