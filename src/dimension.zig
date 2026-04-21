// Dimension - axis metadata for a channel
// Replaces sie_dimension.h
//
// A Dimension represents one axis of data in a channel (e.g. time, amplitude).
// In the C version, Dimension holds references to intake, XML node, group,
// decoder ID, and transform info.

const std = @import("std");
const ref_mod = @import("ref.zig");
const tag_mod = @import("tag.zig");

/// Dimension represents a single axis/column of data in a channel
pub const Dimension = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,

    // Identity
    name: []const u8,
    index: u32,

    // Group association
    group: u32 = 0,
    toplevel_group: u32 = 0,

    // Decoder information
    decoder_id: u32 = 0,
    decoder_version: usize = 0,

    // Raw XML content (owned if allocated)
    raw_xml: ?[]const u8 = null,
    raw_xml_owned: bool = false,

    // Transform node XML (owned if allocated)
    xform_xml: ?[]const u8 = null,
    xform_xml_owned: bool = false,

    // Linear transform parameters (parsed from <transform scale="..." offset="..."/>)
    has_linear_xform: bool = false,
    xform_scale: f64 = 1.0,
    xform_offset: f64 = 0.0,

    // Tags associated with this dimension
    tag_list: std.ArrayList(tag_mod.Tag),

    /// Create a new dimension
    pub fn init(
        allocator: std.mem.Allocator,
        name: []const u8,
        index: u32,
        toplevel_group: u32,
    ) Dimension {
        return Dimension{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Dimension),
            .name = name,
            .index = index,
            .toplevel_group = toplevel_group,
            .group = toplevel_group,
            .tag_list = .{},
        };
    }

    /// Clean up dimension
    pub fn deinit(self: *Dimension) void {
        for (self.tag_list.items) |*t| {
            t.deinit();
        }
        self.tag_list.deinit(self.allocator);

        if (self.raw_xml_owned) {
            if (self.raw_xml) |xml| {
                self.allocator.free(xml);
            }
        }
        if (self.xform_xml_owned) {
            if (self.xform_xml) |xml| {
                self.allocator.free(xml);
            }
        }
    }

    /// Set decoder information
    pub fn setDecoder(self: *Dimension, decoder_id: u32, version: usize) void {
        self.decoder_id = decoder_id;
        self.decoder_version = version;
    }

    /// Set raw XML content (takes ownership if owned=true)
    pub fn setRawXml(self: *Dimension, xml: []const u8, owned: bool) void {
        if (self.raw_xml_owned) {
            if (self.raw_xml) |old| {
                self.allocator.free(old);
            }
        }
        self.raw_xml = xml;
        self.raw_xml_owned = owned;
    }

    /// Set transform node XML
    pub fn setTransformXml(self: *Dimension, xml: []const u8, owned: bool) void {
        if (self.xform_xml_owned) {
            if (self.xform_xml) |old| {
                self.allocator.free(old);
            }
        }
        self.xform_xml = xml;
        self.xform_xml_owned = owned;
    }

    /// Add a tag to this dimension
    pub fn addTag(self: *Dimension, t: tag_mod.Tag) !void {
        try self.tag_list.append(self.allocator, t);
    }

    /// Get all tags
    pub fn tags(self: *const Dimension) []const tag_mod.Tag {
        return self.tag_list.items;
    }

    /// Find a tag by key
    pub fn findTag(self: *const Dimension, key: []const u8) ?*const tag_mod.Tag {
        for (self.tag_list.items) |*t| {
            if (std.mem.eql(u8, t.key, key)) return t;
        }
        return null;
    }

    /// Format for debug output
    pub fn format(self: *const Dimension, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.print("Dimension(index={d}, name=\"{s}\", tags={d})", .{
            self.index, self.name, self.tag_list.items.len,
        });
    }
};

test "dimension creation" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var dim = Dimension.init(allocator, "Time", 0, 2);
    defer dim.deinit();

    try std.testing.expectEqualSlices(u8, "Time", dim.name);
    try std.testing.expectEqual(@as(u32, 0), dim.index);
    try std.testing.expectEqual(@as(u32, 2), dim.group);
}

test "dimension decoder and tags" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var dim = Dimension.init(allocator, "Amplitude", 1, 3);
    defer dim.deinit();

    dim.setDecoder(42, 1);
    try std.testing.expectEqual(@as(u32, 42), dim.decoder_id);

    const t = try tag_mod.Tag.initString(allocator, "units", "mV");
    try dim.addTag(t);

    try std.testing.expectEqual(@as(usize, 1), dim.tags().len);
    const found = dim.findTag("units");
    try std.testing.expect(found != null);
    try std.testing.expectEqualSlices(u8, "mV", found.?.string().?);
}

test "dimension xml" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var dim = Dimension.init(allocator, "Freq", 2, 1);
    defer dim.deinit();

    const xml = "<dim name=\"Freq\" index=\"2\"/>";
    dim.setRawXml(xml, false); // non-owned (static string)

    try std.testing.expectEqualSlices(u8, xml, dim.raw_xml.?);
}
