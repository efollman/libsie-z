// GroupSpigot - reads raw block data from a file group
//
// Provides sequential access to raw block payloads for a specific
// group in an SIE file. Each get() call returns one block's raw payload.
//
// Used internally by ChannelSpigot to feed raw data into the decoder.

const std = @import("std");
const file_mod = @import("file.zig");
const block_mod = @import("block.zig");
const spigot_mod = @import("spigot.zig");
const error_mod = @import("error.zig");

/// Reads raw block payloads from a file group
pub const GroupSpigot = struct {
    allocator: std.mem.Allocator,
    file: *file_mod.File,
    group_id: u32,
    entry: usize = 0,
    // Cache the current block's payload
    current_payload: ?[]u8 = null,

    pub fn init(allocator: std.mem.Allocator, file: *file_mod.File, group_id: u32) GroupSpigot {
        return .{
            .allocator = allocator,
            .file = file,
            .group_id = group_id,
        };
    }

    pub fn deinit(self: *GroupSpigot) void {
        if (self.current_payload) |p| self.allocator.free(p);
    }

    /// Get the number of blocks in this group
    pub fn numBlocks(self: *const GroupSpigot) usize {
        if (self.file.getGroupIndex(self.group_id)) |idx| {
            return idx.getNumBlocks();
        }
        return 0;
    }

    /// Get next raw block payload. Returns null when all blocks consumed.
    pub fn get(self: *GroupSpigot) !?[]const u8 {
        const idx = self.file.getGroupIndex(self.group_id) orelse return null;
        if (self.entry >= idx.getNumBlocks()) return null;

        // Read the block
        const file_entry = idx.entries.items[self.entry];
        var blk = try self.file.readBlockAt(@intCast(file_entry.offset));
        defer blk.deinit();

        const payload = blk.getPayload();

        // Free previous payload
        if (self.current_payload) |p| self.allocator.free(p);

        // Copy payload (block will be freed)
        self.current_payload = try self.allocator.dupe(u8, payload);

        self.entry += 1;
        return self.current_payload.?;
    }

    /// Get block at specific index
    pub fn getAt(self: *GroupSpigot, index: usize) !?[]const u8 {
        const idx = self.file.getGroupIndex(self.group_id) orelse return null;
        if (index >= idx.getNumBlocks()) return null;

        const file_entry = idx.entries.items[index];
        var blk = try self.file.readBlockAt(@intCast(file_entry.offset));
        defer blk.deinit();

        const payload = blk.getPayload();

        if (self.current_payload) |p| self.allocator.free(p);
        self.current_payload = try self.allocator.dupe(u8, payload);
        return self.current_payload.?;
    }

    /// Seek to a block index
    pub fn seek(self: *GroupSpigot, target: u64) u64 {
        const total = self.numBlocks();
        if (target >= total or target == spigot_mod.SEEK_END) {
            self.entry = total;
            return total;
        }
        self.entry = @intCast(target);
        return @intCast(self.entry);
    }

    /// Tell current position
    pub fn tell(self: *const GroupSpigot) u64 {
        return @intCast(self.entry);
    }

    /// Check if done
    pub fn isDone(self: *const GroupSpigot) bool {
        return self.entry >= self.numBlocks();
    }

    /// Reset to beginning
    pub fn reset(self: *GroupSpigot) void {
        self.entry = 0;
    }
};

// ── Tests ──

test "GroupSpigot: reads blocks from group" {
    const testing = std.testing;
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var file = file_mod.File.init(allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer file.deinit();
    file.open() catch return;
    try file.buildIndex();

    // Group 0 is XML metadata — should have blocks
    var gs = GroupSpigot.init(allocator, &file, 0);
    defer gs.deinit();

    try testing.expect(gs.numBlocks() > 0);

    // Read first block — should return some data
    if (try gs.get()) |payload| {
        try testing.expect(payload.len > 0);
    }
}
