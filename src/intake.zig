// Abstract interface for data intake (File or Stream)
// Replaces sie_intake.h
//
// In C, Intake uses virtual dispatch via the class system.
// In Zig, we use a vtable pattern (interface with function pointers).

const std = @import("std");
const block_mod = @import("block.zig");
const error_mod = @import("error.zig");
const ref_mod = @import("ref.zig");

/// Opaque handle to a group within an intake source
pub const GroupHandle = *anyopaque;

/// Virtual method table for Intake implementations
pub const IntakeVTable = struct {
    /// Get a group handle for the given group ID. Returns null if not found.
    getGroupHandle: *const fn (ctx: *anyopaque, group: u32) ?GroupHandle,

    /// Get number of blocks in a group
    getGroupNumBlocks: *const fn (ctx: *anyopaque, handle: GroupHandle) usize,

    /// Get total payload bytes in a group
    getGroupNumBytes: *const fn (ctx: *anyopaque, handle: GroupHandle) u64,

    /// Get payload size of a specific block entry
    getGroupBlockSize: *const fn (ctx: *anyopaque, handle: GroupHandle, entry: usize) u32,

    /// Read a block from a group by entry index
    readGroupBlock: *const fn (ctx: *anyopaque, handle: GroupHandle, entry: usize, blk: *block_mod.Block) error_mod.Error!void,

    /// Check if a group is closed (no more data expected)
    isGroupClosed: *const fn (ctx: *anyopaque, handle: GroupHandle) bool,

    /// Add streaming data (for Stream type; File may return 0)
    addStreamData: *const fn (ctx: *anyopaque, data: []const u8) usize,

    /// Get the implementation pointer (for downcasting)
    getPtr: *const fn (ctx: *anyopaque) *anyopaque,
};

/// Intake - abstract interface for SIE data sources (File or Stream)
pub const Intake = struct {
    ref: ref_mod.Ref,
    vtable: *const IntakeVTable,
    impl: *anyopaque,
    allocator: std.mem.Allocator,

    // Cached data from XML definition
    tests: std.ArrayList(TestEntry),
    channels: std.ArrayList(ChannelEntry),
    tags: std.ArrayList(TagEntry),

    /// Lightweight entry for test lookup
    pub const TestEntry = struct {
        id: u32,
        name: []const u8,
    };

    /// Lightweight entry for channel lookup
    pub const ChannelEntry = struct {
        id: u32,
        name: []const u8,
        test_id: u32,
    };

    /// Lightweight entry for tag lookup
    pub const TagEntry = struct {
        key: []const u8,
        value: []const u8,
        group: u32,
    };

    /// Initialize an Intake interface wrapping an implementation
    pub fn init(
        allocator: std.mem.Allocator,
        vtable: *const IntakeVTable,
        impl: *anyopaque,
    ) Intake {
        return Intake{
            .ref = ref_mod.Ref.init(.Other),
            .vtable = vtable,
            .impl = impl,
            .allocator = allocator,
            .tests = .{},
            .channels = .{},
            .tags = .{},
        };
    }

    pub fn deinit(self: *Intake) void {
        self.tests.deinit(self.allocator);
        self.channels.deinit(self.allocator);
        self.tags.deinit(self.allocator);
    }

    // --- Group data access (delegated to vtable) ---

    pub fn getGroupHandle(self: *Intake, group: u32) ?GroupHandle {
        return self.vtable.getGroupHandle(self.impl, group);
    }

    pub fn getGroupNumBlocks(self: *Intake, handle: GroupHandle) usize {
        return self.vtable.getGroupNumBlocks(self.impl, handle);
    }

    pub fn getGroupNumBytes(self: *Intake, handle: GroupHandle) u64 {
        return self.vtable.getGroupNumBytes(self.impl, handle);
    }

    pub fn getGroupBlockSize(self: *Intake, handle: GroupHandle, entry: usize) u32 {
        return self.vtable.getGroupBlockSize(self.impl, handle, entry);
    }

    pub fn readGroupBlock(self: *Intake, handle: GroupHandle, entry: usize, blk: *block_mod.Block) !void {
        return self.vtable.readGroupBlock(self.impl, handle, entry, blk);
    }

    pub fn isGroupClosed(self: *Intake, handle: GroupHandle) bool {
        return self.vtable.isGroupClosed(self.impl, handle);
    }

    pub fn addStreamData(self: *Intake, data: []const u8) usize {
        return self.vtable.addStreamData(self.impl, data);
    }

    // --- Test/Channel/Tag registration ---

    pub fn addTest(self: *Intake, id: u32, name: []const u8) !void {
        try self.tests.append(self.allocator, .{ .id = id, .name = name });
    }

    pub fn addChannel(self: *Intake, id: u32, name: []const u8, test_id: u32) !void {
        try self.channels.append(self.allocator, .{ .id = id, .name = name, .test_id = test_id });
    }

    pub fn addTag(self: *Intake, key: []const u8, value: []const u8, group: u32) !void {
        try self.tags.append(self.allocator, .{ .key = key, .value = value, .group = group });
    }

    /// Find a test entry by ID
    pub fn findTest(self: *const Intake, id: u32) ?TestEntry {
        for (self.tests.items) |entry| {
            if (entry.id == id) return entry;
        }
        return null;
    }

    /// Find a channel entry by ID
    pub fn findChannel(self: *const Intake, id: u32) ?ChannelEntry {
        for (self.channels.items) |entry| {
            if (entry.id == id) return entry;
        }
        return null;
    }

    /// Get channels belonging to a specific test
    pub fn getChannelsForTest(self: *const Intake, test_id: u32, buf: []ChannelEntry) usize {
        var count: usize = 0;
        for (self.channels.items) |entry| {
            if (entry.test_id == test_id) {
                if (count < buf.len) {
                    buf[count] = entry;
                }
                count += 1;
            }
        }
        return count;
    }

    /// Get tags for a given group (0 = top-level)
    pub fn getTagsForGroup(self: *const Intake, group: u32, buf: []TagEntry) usize {
        var count: usize = 0;
        for (self.tags.items) |entry| {
            if (entry.group == group) {
                if (count < buf.len) {
                    buf[count] = entry;
                }
                count += 1;
            }
        }
        return count;
    }
};

// --- Null intake (for testing) ---

const null_vtable = IntakeVTable{
    .getGroupHandle = nullGetGroupHandle,
    .getGroupNumBlocks = nullGetGroupNumBlocks,
    .getGroupNumBytes = nullGetGroupNumBytes,
    .getGroupBlockSize = nullGetGroupBlockSize,
    .readGroupBlock = nullReadGroupBlock,
    .isGroupClosed = nullIsGroupClosed,
    .addStreamData = nullAddStreamData,
    .getPtr = nullGetPtr,
};

fn nullGetGroupHandle(_: *anyopaque, _: u32) ?GroupHandle {
    return null;
}
fn nullGetGroupNumBlocks(_: *anyopaque, _: GroupHandle) usize {
    return 0;
}
fn nullGetGroupNumBytes(_: *anyopaque, _: GroupHandle) u64 {
    return 0;
}
fn nullGetGroupBlockSize(_: *anyopaque, _: GroupHandle, _: usize) u32 {
    return 0;
}
fn nullReadGroupBlock(_: *anyopaque, _: GroupHandle, _: usize, _: *block_mod.Block) error_mod.Error!void {
    return error_mod.Error.OperationFailed;
}
fn nullIsGroupClosed(_: *anyopaque, _: GroupHandle) bool {
    return true;
}
fn nullAddStreamData(_: *anyopaque, _: []const u8) usize {
    return 0;
}
fn nullGetPtr(ctx: *anyopaque) *anyopaque {
    return ctx;
}

/// Create a null/dummy intake for testing
pub fn nullIntake(allocator: std.mem.Allocator) Intake {
    // Use a sentinel pointer for the null implementation
    return Intake.init(allocator, &null_vtable, @ptrFromInt(1));
}

test "intake interface basics" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var intake = nullIntake(allocator);
    defer intake.deinit();

    // Null intake returns null for group handles
    try std.testing.expectEqual(@as(?GroupHandle, null), intake.getGroupHandle(0));

    // Can add tests and channels
    try intake.addTest(1, "Test 1");
    try intake.addTest(2, "Test 2");
    try intake.addChannel(10, "Temp", 1);
    try intake.addChannel(11, "Pressure", 1);
    try intake.addChannel(20, "Voltage", 2);

    try std.testing.expectEqual(@as(usize, 2), intake.tests.items.len);
    try std.testing.expectEqual(@as(usize, 3), intake.channels.items.len);

    // Find test by ID
    const t = intake.findTest(1);
    try std.testing.expect(t != null);
    try std.testing.expectEqualSlices(u8, "Test 1", t.?.name);

    // Find channels for test
    var chan_buf: [10]Intake.ChannelEntry = undefined;
    const count = intake.getChannelsForTest(1, &chan_buf);
    try std.testing.expectEqual(@as(usize, 2), count);
}

test "intake tag management" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var intake = nullIntake(allocator);
    defer intake.deinit();

    try intake.addTag("author", "John", 0);
    try intake.addTag("date", "2024-01-01", 0);
    try intake.addTag("sensor", "TC-1", 5);

    var buf: [10]Intake.TagEntry = undefined;
    const top_count = intake.getTagsForGroup(0, &buf);
    try std.testing.expectEqual(@as(usize, 2), top_count);

    const grp_count = intake.getTagsForGroup(5, &buf);
    try std.testing.expectEqual(@as(usize, 1), grp_count);
}
