// Base object implementation
// Replaces sie_object.h
//
// The C version uses a complex vtable/class system with runtime type checking.
// In Zig, we use tagged unions for type-safe polymorphism and comptime generics.
// Reference counting is handled by the Ref type.

const std = @import("std");
const ref_mod = @import("ref.zig");
const tag_mod = @import("tag.zig");
const channel_mod = @import("channel.zig");
const dimension_mod = @import("dimension.zig");
const test_mod = @import("test.zig");
const group_mod = @import("group.zig");

/// Object type enumeration
pub const ObjectType = enum {
    File,
    Stream,
    Test,
    Channel,
    Dimension,
    Tag,
    Group,
    Spigot,
    Iterator,
    Context,
};

/// Tagged union for type-safe polymorphic access to SIE objects
pub const TypedObject = union(ObjectType) {
    File: void,
    Stream: void,
    Test: *test_mod.Test,
    Channel: *channel_mod.Channel,
    Dimension: *dimension_mod.Dimension,
    Tag: *tag_mod.Tag,
    Group: *group_mod.Group,
    Spigot: void,
    Iterator: void,
    Context: void,

    /// Get the name of the object type
    pub fn typeName(self: TypedObject) []const u8 {
        return @tagName(self);
    }

    /// Get name from the underlying object (if it has one)
    pub fn getName(self: TypedObject) ?[]const u8 {
        return switch (self) {
            .Test => |t| t.getName(),
            .Channel => |c| c.getName(),
            .Dimension => |d| d.getName(),
            .Tag => |t| t.getId(),
            else => null,
        };
    }

    /// Get ID from the underlying object (if it has one)
    pub fn getId(self: TypedObject) ?u32 {
        return switch (self) {
            .Test => |t| t.getId(),
            .Channel => |c| c.getId(),
            .Dimension => |d| d.getIndex(),
            .Group => |g| g.getId(),
            else => null,
        };
    }

    /// Get tags from the underlying object (if it supports them)
    pub fn getTags(self: TypedObject) ?[]const tag_mod.Tag {
        return switch (self) {
            .Test => |t| t.getTags(),
            .Channel => |c| c.getTags(),
            .Dimension => |d| d.getTags(),
            else => null,
        };
    }
};

/// Base object fields common to most SIE objects (for simple use cases)
pub const Object = struct {
    name: ?[]const u8 = null,
    id: ?u32 = null,
    index: u32 = 0,
    obj_type: ObjectType,
    ref: ref_mod.Ref,

    pub fn init(obj_type: ObjectType) Object {
        return Object{
            .obj_type = obj_type,
            .ref = ref_mod.Ref.init(switch (obj_type) {
                .File => .File,
                .Stream => .Other,
                .Test => .Test,
                .Channel => .Channel,
                .Dimension => .Dimension,
                .Tag => .Tag,
                .Group => .Group,
                .Spigot => .Spigot,
                .Iterator => .Iterator,
                .Context => .Context,
            }),
        };
    }

    pub fn retain(self: *Object) *Object {
        _ = self.ref.retain();
        return self;
    }

    pub fn release(self: *Object) u32 {
        return self.ref.release();
    }

    pub fn refCount(self: *const Object) u32 {
        return self.ref.refCount();
    }
};

test "object creation and ref counting" {
    var obj = Object.init(.Channel);
    try std.testing.expectEqual(ObjectType.Channel, obj.obj_type);
    try std.testing.expectEqual(@as(u32, 1), obj.refCount());

    _ = obj.retain();
    try std.testing.expectEqual(@as(u32, 2), obj.refCount());

    _ = obj.release();
    try std.testing.expectEqual(@as(u32, 1), obj.refCount());
}

test "typed object dispatch" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var test_obj = test_mod.Test.init(allocator, 1, "My Test");
    defer test_obj.deinit();

    const typed = TypedObject{ .Test = &test_obj };
    try std.testing.expectEqualSlices(u8, "Test", typed.typeName());
    try std.testing.expectEqualSlices(u8, "My Test", typed.getName().?);
    try std.testing.expectEqual(@as(u32, 1), typed.getId().?);
}

test "typed object tags" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ch = channel_mod.Channel.init(allocator, 5, "Temp");
    defer ch.deinit();

    const t = try tag_mod.Tag.initString(allocator, "units", "C");
    try ch.addTag(t);

    const typed = TypedObject{ .Channel = &ch };
    const tags = typed.getTags();
    try std.testing.expect(tags != null);
    try std.testing.expectEqual(@as(usize, 1), tags.?.len);
}
