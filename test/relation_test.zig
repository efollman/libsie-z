// Relation tests — ports of t_relation.c
// Tests: set/get, clone, integer value parsing

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const Relation = libsie.relation.Relation;

test "relation: set get and clone" {
    // Port of test_simple — set values, get values, clone
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("foo", "bar");
    try rel.setValue("bar", "baz");

    try testing.expectEqualStrings("bar", rel.value("foo").?);
    try testing.expectEqualStrings("baz", rel.value("bar").?);

    // Clone
    var clone = try rel.clone();
    defer clone.deinit();

    try testing.expectEqualStrings("bar", clone.value("foo").?);
    try testing.expectEqualStrings("baz", clone.value("bar").?);

    // Modify original — clone should be independent
    try rel.setValue("foo", "modified");
    try testing.expectEqualStrings("modified", rel.value("foo").?);
    try testing.expectEqualStrings("bar", clone.value("foo").?);
}

test "relation: integer value parsing" {
    // Port of test_amd64_stdarg — set formatted value, parse as int
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("foo", "42");

    const val = rel.intValue("foo");
    try testing.expect(val != null);
    try testing.expectEqual(@as(i64, 42), val.?);
}

test "relation: float value parsing" {
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("pi", "3.14159");

    const val = rel.floatValue("pi");
    try testing.expect(val != null);
    try testing.expectApproxEqAbs(@as(f64, 3.14159), val.?, 0.00001);
}

test "relation: overwrite existing value" {
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("key", "first");
    try testing.expectEqualStrings("first", rel.value("key").?);

    try rel.setValue("key", "second");
    try testing.expectEqualStrings("second", rel.value("key").?);
    try testing.expectEqual(@as(usize, 1), rel.count());
}

test "relation: nonexistent key returns null" {
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try testing.expect(rel.value("missing") == null);
    try testing.expect(rel.intValue("missing") == null);
    try testing.expect(rel.floatValue("missing") == null);
}

test "relation: index-based access" {
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("alpha", "1");
    try rel.setValue("beta", "2");
    try rel.setValue("gamma", "3");

    try testing.expectEqual(@as(usize, 3), rel.count());

    try testing.expectEqualStrings("alpha", rel.getName(0).?);
    try testing.expectEqualStrings("1", rel.getValue(0).?);
    try testing.expectEqualStrings("beta", rel.getName(1).?);
    try testing.expectEqualStrings("2", rel.getValue(1).?);
    try testing.expectEqualStrings("gamma", rel.getName(2).?);

    // Out of bounds
    try testing.expect(rel.getName(3) == null);
    try testing.expect(rel.getValue(3) == null);
}

test "relation: delete at index" {
    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("a", "1");
    try rel.setValue("b", "2");
    try rel.setValue("c", "3");

    rel.deleteAtIndex(1); // remove "b"
    try testing.expectEqual(@as(usize, 2), rel.count());
    try testing.expectEqualStrings("a", rel.getName(0).?);
    try testing.expectEqualStrings("c", rel.getName(1).?);
}

test "relation: merge" {
    var rel1 = Relation.init(testing.allocator);
    defer rel1.deinit();
    var rel2 = Relation.init(testing.allocator);
    defer rel2.deinit();

    try rel1.setValue("a", "1");
    try rel1.setValue("b", "2");

    try rel2.setValue("b", "override");
    try rel2.setValue("c", "3");

    try rel1.merge(&rel2);

    try testing.expectEqualStrings("1", rel1.value("a").?);
    // merge keeps self's values for duplicate keys
    try testing.expectEqualStrings("2", rel1.value("b").?);
    try testing.expectEqualStrings("3", rel1.value("c").?);
}

test "relation: split string" {
    // Test parsing "key=val&key2=val2" style strings
    var rel = try Relation.splitString(testing.allocator, "foo=bar&baz=qux", '=', '&');
    defer rel.deinit();

    try testing.expectEqual(@as(usize, 2), rel.count());
    try testing.expectEqualStrings("bar", rel.value("foo").?);
    try testing.expectEqualStrings("qux", rel.value("baz").?);
}
