// Metadata key-value pair
// Replaces sie_tag.h
//
// A Tag represents a metadata key-value pair in an SIE file.
// Tags can be associated with a group (for group-level metadata)
// or be top-level (group = 0). Values can be string or binary.

const std = @import("std");
const ref_mod = @import("ref.zig");

/// Tag value can be either string or binary
pub const TagValue = union(enum) {
    String: []const u8,
    Binary: []const u8,
};

/// Metadata tag with string or binary value
pub const Tag = struct {
    allocator: std.mem.Allocator,
    ref: ref_mod.Ref,
    key: []const u8,
    value: TagValue,

    // Group association (0 = top-level / no group)
    group: u32 = 0,

    /// Create a string-valued tag
    pub fn initString(allocator: std.mem.Allocator, key: []const u8, value: []const u8) !Tag {
        return Tag{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Tag),
            .key = key,
            .value = .{ .String = value },
        };
    }

    /// Create a binary-valued tag
    pub fn initBinary(allocator: std.mem.Allocator, key: []const u8, value: []const u8) !Tag {
        return Tag{
            .allocator = allocator,
            .ref = ref_mod.Ref.init(.Tag),
            .key = key,
            .value = .{ .Binary = value },
        };
    }

    /// Create a string-valued tag with group association
    pub fn initWithGroup(
        allocator: std.mem.Allocator,
        key: []const u8,
        value: []const u8,
        group: u32,
    ) !Tag {
        var tag = try initString(allocator, key, value);
        tag.group = group;
        return tag;
    }

    /// Check if this is a string tag
    pub fn isString(self: *const Tag) bool {
        return self.value == .String;
    }

    /// Check if this is a binary tag
    pub fn isBinary(self: *const Tag) bool {
        return self.value == .Binary;
    }

    /// Get string value (returns null if not a string)
    pub fn string(self: *const Tag) ?[]const u8 {
        if (self.value == .String) {
            return self.value.String;
        }
        return null;
    }

    /// Get binary value (returns null if not binary)
    pub fn binary(self: *const Tag) ?[]const u8 {
        if (self.value == .Binary) {
            return self.value.Binary;
        }
        return null;
    }

    /// Get value size in bytes regardless of type
    pub fn valueSize(self: *const Tag) usize {
        return switch (self.value) {
            .String => |s| s.len,
            .Binary => |b| b.len,
        };
    }

    /// Check if this tag is associated with a specific group
    pub fn isFromGroup(self: *const Tag) bool {
        return self.group != 0;
    }

    /// Deinit (would free allocated memory if needed)
    pub fn deinit(self: *Tag) void {
        _ = self;
    }

    /// Format for debug output
    pub fn format(self: *const Tag, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        switch (self.value) {
            .String => |s| try writer.print("Tag(\"{s}\" = \"{s}\", group={d})", .{ self.key, s, self.group }),
            .Binary => |b| try writer.print("Tag(\"{s}\" = <{d} bytes binary>, group={d})", .{ self.key, b.len, self.group }),
        }
    }
};

test "string tag" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var tag = try Tag.initString(allocator, "author", "John Doe");
    defer tag.deinit();

    try std.testing.expectEqualSlices(u8, "author", tag.key);
    try std.testing.expect(tag.isString());
    try std.testing.expect(!tag.isBinary());
    try std.testing.expectEqualSlices(u8, "John Doe", tag.string().?);
    try std.testing.expectEqual(@as(usize, 8), tag.valueSize());
    try std.testing.expect(!tag.isFromGroup());
}

test "binary tag" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const data = "binary data";
    var tag = try Tag.initBinary(allocator, "data", data);
    defer tag.deinit();

    try std.testing.expect(!tag.isString());
    try std.testing.expect(tag.isBinary());
    try std.testing.expectEqualSlices(u8, data, tag.binary().?);
}

test "tag with group" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var tag = try Tag.initWithGroup(allocator, "sensor", "TC-1", 5);
    defer tag.deinit();

    try std.testing.expectEqual(@as(u32, 5), tag.group);
    try std.testing.expect(tag.isFromGroup());
    try std.testing.expectEqualSlices(u8, "TC-1", tag.string().?);
}
