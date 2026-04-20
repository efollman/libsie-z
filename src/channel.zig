// Channel - data series within a test
// Replaces sie_channel.h
//
// A Channel represents a data series (e.g. a sensor measurement) within a Test.
// It contains Dimensions (axes) and Tags (metadata), and can produce a Spigot
// for streaming data access.

const std = @import("std");
const ref_mod = @import("ref.zig");
const dimension_mod = @import("dimension.zig");
const tag_mod = @import("tag.zig");

/// Channel represents a data series within a test
pub const Channel = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,

    // Identity
    id: u32,
    name: []const u8,
    index: u32 = 0,

    // Group association
    toplevel_group: u32 = 0,

    // Containing test ID
    test_id: u32 = 0,

    // Dimensions (axes of data)
    dim_list: std.ArrayList(dimension_mod.Dimension),

    // Tags (metadata key-value pairs)
    tag_list: std.ArrayList(tag_mod.Tag),

    // Raw and expanded XML definitions
    raw_xml: ?[]const u8 = null,
    raw_xml_owned: bool = false,
    expanded_xml: ?[]const u8 = null,
    expanded_xml_owned: bool = false,

    /// Create a new channel
    pub fn init(allocator: std.mem.Allocator, id: u32, name: []const u8) Channel {
        return Channel{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Channel),
            .id = id,
            .name = name,
            .dim_list = .{},
            .tag_list = .{},
        };
    }

    /// Clean up channel and all owned sub-objects
    pub fn deinit(self: *Channel) void {
        for (self.dim_list.items) |*dim| {
            dim.deinit();
        }
        self.dim_list.deinit(self.allocator);

        for (self.tag_list.items) |*t| {
            t.deinit();
        }
        self.tag_list.deinit(self.allocator);

        if (self.raw_xml_owned) {
            if (self.raw_xml) |xml| {
                self.allocator.free(xml);
            }
        }
        if (self.expanded_xml_owned) {
            if (self.expanded_xml) |xml| {
                self.allocator.free(xml);
            }
        }
    }

    // --- Identity ---

    // --- Dimensions ---

    /// Add a dimension to this channel
    pub fn addDimension(self: *Channel, dim: dimension_mod.Dimension) !void {
        try self.dim_list.append(self.allocator, dim);
    }

    /// Get all dimensions
    pub fn dimensions(self: *const Channel) []const dimension_mod.Dimension {
        return self.dim_list.items;
    }

    /// Get dimension by index
    pub fn dimension(self: *const Channel, idx: u32) ?*const dimension_mod.Dimension {
        for (self.dim_list.items) |*dim| {
            if (dim.index == idx) return dim;
        }
        return null;
    }

    // --- Tags ---

    /// Add a tag to this channel
    pub fn addTag(self: *Channel, t: tag_mod.Tag) !void {
        try self.tag_list.append(self.allocator, t);
    }

    /// Get all tags
    pub fn tags(self: *const Channel) []const tag_mod.Tag {
        return self.tag_list.items;
    }

    /// Find a tag by key
    pub fn findTag(self: *const Channel, key: []const u8) ?*const tag_mod.Tag {
        for (self.tag_list.items) |*t| {
            if (std.mem.eql(u8, t.key, key)) return t;
        }
        return null;
    }

    // --- XML ---

    /// Set raw XML definition
    pub fn setRawXml(self: *Channel, xml: []const u8, owned: bool) void {
        if (self.raw_xml_owned) {
            if (self.raw_xml) |old| self.allocator.free(old);
        }
        self.raw_xml = xml;
        self.raw_xml_owned = owned;
    }

    /// Set expanded XML definition
    pub fn setExpandedXml(self: *Channel, xml: []const u8, owned: bool) void {
        if (self.expanded_xml_owned) {
            if (self.expanded_xml) |old| self.allocator.free(old);
        }
        self.expanded_xml = xml;
        self.expanded_xml_owned = owned;
    }

    /// Format for debug output
    pub fn format(self: *const Channel, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.print("Channel(id={d}, name=\"{s}\", dims={d}, tags={d})", .{
            self.id, self.name, self.dim_list.items.len, self.tag_list.items.len,
        });
    }
};

test "channel creation" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var channel = Channel.init(allocator, 1, "Temperature");
    defer channel.deinit();

    try std.testing.expectEqual(@as(u32, 1), channel.id);
    try std.testing.expectEqualSlices(u8, "Temperature", channel.name);
    try std.testing.expectEqual(@as(usize, 0), channel.dimensions().len);
}

test "channel with dimensions" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var channel = Channel.init(allocator, 5, "Vibration");
    defer channel.deinit();

    const dim_time = dimension_mod.Dimension.init(allocator, "Time", 0, 2);
    const dim_amp = dimension_mod.Dimension.init(allocator, "Amplitude", 1, 2);

    try channel.addDimension(dim_time);
    try channel.addDimension(dim_amp);

    try std.testing.expectEqual(@as(usize, 2), channel.dimensions().len);

    const d0 = channel.dimension(0);
    try std.testing.expect(d0 != null);
    try std.testing.expectEqualSlices(u8, "Time", d0.?.name);

    const d1 = channel.dimension(1);
    try std.testing.expect(d1 != null);
    try std.testing.expectEqualSlices(u8, "Amplitude", d1.?.name);
}

test "channel with tags" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var channel = Channel.init(allocator, 3, "Pressure");
    defer channel.deinit();

    const t1 = try tag_mod.Tag.initString(allocator, "units", "kPa");
    const t2 = try tag_mod.Tag.initString(allocator, "sensor", "PT-100");
    try channel.addTag(t1);
    try channel.addTag(t2);

    try std.testing.expectEqual(@as(usize, 2), channel.tags().len);

    const found = channel.findTag("units");
    try std.testing.expect(found != null);
    try std.testing.expectEqualSlices(u8, "kPa", found.?.string().?);
}
