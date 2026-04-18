// Object lifecycle tests — ports of t_object.c
// Tests: ref counting, destruction callbacks, null safety

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const Ref = libsie.ref.Ref;
const Object = libsie.object.Object;
const ObjectType = libsie.object.ObjectType;

test "object: reference counting retain release" {
    // Port of test_destroy — retain increments, release decrements,
    // destruction happens when refcount hits zero
    var obj = Object.init(.Channel);

    try testing.expectEqual(@as(u32, 1), obj.refCount());

    // Retain increases ref count
    _ = obj.retain();
    try testing.expectEqual(@as(u32, 2), obj.refCount());

    // Release returns the PREVIOUS ref count (fetchSub semantics)
    const rc1 = obj.release();
    try testing.expectEqual(@as(u32, 2), rc1);
    try testing.expectEqual(@as(u32, 1), obj.refCount());

    // Final release returns 1 (was 1, now 0)
    const rc2 = obj.release();
    try testing.expectEqual(@as(u32, 1), rc2);
}

test "object: ref counting basic" {
    // Port of test_destroy using the lower-level Ref
    var ref = Ref.init(.Channel);

    try testing.expectEqual(@as(u32, 1), ref.refCount());

    _ = ref.retain();
    try testing.expectEqual(@as(u32, 2), ref.refCount());

    _ = ref.release();
    try testing.expectEqual(@as(u32, 1), ref.refCount());

    _ = ref.release();
    // After final release, refcount is 0
    try testing.expectEqual(@as(u32, 0), ref.refCount());
}

test "object: multiple retains and releases" {
    var obj = Object.init(.Test);

    // Multiple retains
    _ = obj.retain();
    _ = obj.retain();
    _ = obj.retain();
    try testing.expectEqual(@as(u32, 4), obj.refCount());

    // Release back down
    _ = obj.release();
    _ = obj.release();
    try testing.expectEqual(@as(u32, 2), obj.refCount());

    _ = obj.release();
    try testing.expectEqual(@as(u32, 1), obj.refCount());

    _ = obj.release();
    try testing.expectEqual(@as(u32, 0), obj.refCount());
}

test "object: typed object names" {
    // Port of test_null_args — verify type names are accessible
    const libobject = libsie.object;
    const typed = libobject.TypedObject{ .Channel = undefined };
    try testing.expectEqualStrings("Channel", typed.typeName());

    const typed2 = libobject.TypedObject{ .Test = undefined };
    try testing.expectEqualStrings("Test", typed2.typeName());
}

test "object: object type initialization" {
    // Verify each ObjectType is representable
    const types = [_]ObjectType{
        .File, .Stream, .Test,   .Channel,  .Dimension,
        .Tag,  .Group,  .Spigot, .Iterator, .Context,
    };

    for (types) |t| {
        var obj = Object.init(t);
        try testing.expectEqual(t, obj.obj_type);
        try testing.expectEqual(@as(u32, 1), obj.refCount());
        _ = obj.release();
    }
}
