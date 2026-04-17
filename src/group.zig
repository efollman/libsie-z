// Group - grouping structure for binary data blocks
// Replaces sie_group.h
//
// A Group represents a set of binary data blocks that share a group ID.
// Groups are used to organize related data (e.g. all blocks for a particular
// decoder/channel combination). Group 0 = XML, Group 1 = Index.

const std = @import("std");
const ref_mod = @import("ref.zig");
const block_mod = @import("block.zig");

/// Group represents a set of binary data blocks sharing a group ID
pub const Group = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,

    // Identity
    id: u32,

    // Block tracking
    num_blocks: usize = 0,
    total_payload_bytes: u64 = 0,
    is_closed: bool = false,

    /// Create a new group
    pub fn init(allocator: std.mem.Allocator, id: u32) Group {
        return Group{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Group),
            .id = id,
        };
    }

    /// Clean up group
    pub fn deinit(self: *Group) void {
        _ = self;
    }

    /// Get group ID
    pub fn getId(self: *const Group) u32 {
        return self.id;
    }

    /// Check if this is the XML group (group 0)
    pub fn isXmlGroup(self: *const Group) bool {
        return self.id == block_mod.SIE_XML_GROUP;
    }

    /// Check if this is the index group (group 1)
    pub fn isIndexGroup(self: *const Group) bool {
        return self.id == block_mod.SIE_INDEX_GROUP;
    }

    /// Get number of blocks
    pub fn getNumBlocks(self: *const Group) usize {
        return self.num_blocks;
    }

    /// Get total payload bytes across all blocks
    pub fn getTotalPayloadBytes(self: *const Group) u64 {
        return self.total_payload_bytes;
    }

    /// Check if the group is closed (no more blocks expected)
    pub fn isClosed(self: *const Group) bool {
        return self.is_closed;
    }

    /// Mark the group as closed
    pub fn close(self: *Group) void {
        self.is_closed = true;
    }

    /// Record that a block was added to this group
    pub fn recordBlock(self: *Group, payload_size: u32) void {
        self.num_blocks += 1;
        self.total_payload_bytes += payload_size;
    }
};

test "group creation" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var group = Group.init(allocator, 5);
    defer group.deinit();

    try std.testing.expectEqual(@as(u32, 5), group.getId());
    try std.testing.expect(!group.isXmlGroup());
    try std.testing.expect(!group.isIndexGroup());
    try std.testing.expect(!group.isClosed());
    try std.testing.expectEqual(@as(usize, 0), group.getNumBlocks());
}

test "group block tracking" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var group = Group.init(allocator, 2);
    defer group.deinit();

    group.recordBlock(100);
    group.recordBlock(200);

    try std.testing.expectEqual(@as(usize, 2), group.getNumBlocks());
    try std.testing.expectEqual(@as(u64, 300), group.getTotalPayloadBytes());

    group.close();
    try std.testing.expect(group.isClosed());
}

test "special group IDs" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var xml_group = Group.init(allocator, 0);
    defer xml_group.deinit();
    try std.testing.expect(xml_group.isXmlGroup());

    var idx_group = Group.init(allocator, 1);
    defer idx_group.deinit();
    try std.testing.expect(idx_group.isIndexGroup());
}
