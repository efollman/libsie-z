// Test container - represents a test run containing channels
// Replaces sie_test.h
//
// A Test represents a single test run or acquisition within an SIE file.
// It contains channels (data series) and tags (metadata).

const std = @import("std");
const ref_mod = @import("ref.zig");
const channel_mod = @import("channel.zig");
const tag_mod = @import("tag.zig");

/// Test represents a test run containing channels
pub const Test = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,

    // Identity
    id: u32,
    name: []const u8,
    index: u32 = 0,

    // Channels belonging to this test
    channels: std.ArrayList(channel_mod.Channel),

    // Tags (metadata)
    tags: std.ArrayList(tag_mod.Tag),

    // Raw XML node content
    raw_xml: ?[]const u8 = null,
    raw_xml_owned: bool = false,

    /// Create a new test
    pub fn init(allocator: std.mem.Allocator, id: u32, name: []const u8) Test {
        return Test{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Test),
            .id = id,
            .name = name,
            .channels = .{},
            .tags = .{},
        };
    }

    /// Clean up test and all owned sub-objects
    pub fn deinit(self: *Test) void {
        for (self.channels.items) |*ch| {
            ch.deinit();
        }
        self.channels.deinit(self.allocator);

        for (self.tags.items) |*t| {
            t.deinit();
        }
        self.tags.deinit(self.allocator);

        if (self.raw_xml_owned) {
            if (self.raw_xml) |xml| {
                self.allocator.free(xml);
            }
        }
    }

    // --- Identity ---

    /// Get test ID
    pub fn getId(self: *const Test) u32 {
        return self.id;
    }

    /// Get test name
    pub fn getName(self: *const Test) []const u8 {
        return self.name;
    }

    // --- Channels ---

    /// Add a channel to this test
    pub fn addChannel(self: *Test, channel: channel_mod.Channel) !void {
        try self.channels.append(self.allocator, channel);
    }

    /// Get all channels
    pub fn getChannels(self: *const Test) []const channel_mod.Channel {
        return self.channels.items;
    }

    /// Get mutable channels
    pub fn getChannelsMut(self: *Test) []channel_mod.Channel {
        return self.channels.items;
    }

    /// Find a channel by ID
    pub fn findChannel(self: *const Test, id: u32) ?*const channel_mod.Channel {
        for (self.channels.items) |*ch| {
            if (ch.id == id) return ch;
        }
        return null;
    }

    /// Get number of channels
    pub fn getNumChannels(self: *const Test) usize {
        return self.channels.items.len;
    }

    // --- Tags ---

    /// Add a tag to this test
    pub fn addTag(self: *Test, t: tag_mod.Tag) !void {
        try self.tags.append(self.allocator, t);
    }

    /// Get all tags
    pub fn getTags(self: *const Test) []const tag_mod.Tag {
        return self.tags.items;
    }

    /// Find a tag by key
    pub fn findTag(self: *const Test, key: []const u8) ?*const tag_mod.Tag {
        for (self.tags.items) |*t| {
            if (std.mem.eql(u8, t.key, key)) return t;
        }
        return null;
    }

    // --- XML ---

    /// Set raw XML content
    pub fn setRawXml(self: *Test, xml: []const u8, owned: bool) void {
        if (self.raw_xml_owned) {
            if (self.raw_xml) |old| self.allocator.free(old);
        }
        self.raw_xml = xml;
        self.raw_xml_owned = owned;
    }
};

test "test creation" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var test_obj = Test.init(allocator, 1, "My Test");
    defer test_obj.deinit();

    try std.testing.expectEqual(@as(u32, 1), test_obj.getId());
    try std.testing.expectEqualSlices(u8, "My Test", test_obj.getName());
    try std.testing.expectEqual(@as(usize, 0), test_obj.getNumChannels());
}

test "test with channels" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var test_obj = Test.init(allocator, 1, "Acquisition");
    defer test_obj.deinit();

    const ch1 = channel_mod.Channel.init(allocator, 10, "Temperature");
    const ch2 = channel_mod.Channel.init(allocator, 11, "Pressure");

    try test_obj.addChannel(ch1);
    try test_obj.addChannel(ch2);

    try std.testing.expectEqual(@as(usize, 2), test_obj.getNumChannels());

    const found = test_obj.findChannel(10);
    try std.testing.expect(found != null);
    try std.testing.expectEqualSlices(u8, "Temperature", found.?.getName());
}

test "test with tags" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var test_obj = Test.init(allocator, 1, "Tagged Test");
    defer test_obj.deinit();

    const t = try tag_mod.Tag.initString(allocator, "operator", "Jane");
    try test_obj.addTag(t);

    try std.testing.expectEqual(@as(usize, 1), test_obj.getTags().len);
    const found = test_obj.findTag("operator");
    try std.testing.expect(found != null);
    try std.testing.expectEqualSlices(u8, "Jane", found.?.getString().?);
}
