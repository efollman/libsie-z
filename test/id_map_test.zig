// ID map / hash table tests — ports of t_id_map.c
// Tests: creation/set/get, growing beyond capacity, iteration

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const HashMap = libsie.uthash.HashMap;

test "id_map: creation set get" {
    // Port of test_creation_set_get
    var map = HashMap(u32, usize).init(testing.allocator);
    defer map.deinit();

    try map.put(0, 0);
    try map.put(1, 1);
    try map.put(2, 2);
    try map.put(42, 42);

    try testing.expectEqual(@as(usize, 0), map.get(0).?);
    try testing.expectEqual(@as(usize, 1), map.get(1).?);
    try testing.expectEqual(@as(usize, 2), map.get(2).?);
    try testing.expectEqual(@as(usize, 42), map.get(42).?);

    // Non-existent key
    try testing.expect(map.get(100) == null);
}

test "id_map: grow beyond initial capacity" {
    // Port of test_grow — store 100 entries, verify old entries survive
    var map = HashMap(u32, usize).init(testing.allocator);
    defer map.deinit();

    // Insert key 42 first
    try map.put(42, 42);

    // Insert 0..99 (skipping 42 to avoid overwrite)
    var i: u32 = 0;
    while (i < 100) : (i += 1) {
        if (i != 42) try map.put(i, i);
    }

    // Old entry still present
    try testing.expectEqual(@as(usize, 42), map.get(42).?);

    // Spot checks
    try testing.expectEqual(@as(usize, 0), map.get(0).?);
    try testing.expectEqual(@as(usize, 50), map.get(50).?);
    try testing.expectEqual(@as(usize, 99), map.get(99).?);

    try testing.expectEqual(@as(usize, 100), map.count());
}

test "id_map: iteration with accumulation" {
    // Port of test_foreach — iterate all entries and sum id*42 + value
    var map = HashMap(u32, usize).init(testing.allocator);
    defer map.deinit();

    var expected_sum: usize = 0;

    try map.put(420, 19);
    expected_sum += 420 * 42 + 19;

    var i: u32 = 0;
    while (i < 100) : (i += 1) {
        try map.put(i, i);
        expected_sum += @as(usize, i) * 42 + @as(usize, i);
    }

    try map.put(900, 2501);
    expected_sum += 900 * 42 + 2501;

    // Iterate and accumulate
    var sum: usize = 0;
    var iter = map.iterator();
    while (iter.next()) |entry| {
        const id: usize = entry.key_ptr.*;
        const value: usize = entry.value_ptr.*;
        sum += id * 42 + value;
    }

    try testing.expectEqual(expected_sum, sum);
}

test "id_map: contains and remove" {
    var map = HashMap(u32, []const u8).init(testing.allocator);
    defer map.deinit();

    try map.put(1, "hello");
    try map.put(2, "world");

    try testing.expect(map.contains(1));
    try testing.expect(map.contains(2));
    try testing.expect(!map.contains(3));

    try testing.expect(map.remove(1));
    try testing.expect(!map.contains(1));
    try testing.expectEqual(@as(usize, 1), map.count());

    try testing.expect(!map.remove(99)); // removing non-existent returns false
}

test "id_map: clear" {
    var map = HashMap(u32, u32).init(testing.allocator);
    defer map.deinit();

    try map.put(1, 10);
    try map.put(2, 20);
    try map.put(3, 30);
    try testing.expectEqual(@as(usize, 3), map.count());

    map.clear();
    try testing.expectEqual(@as(usize, 0), map.count());
    try testing.expect(map.get(1) == null);
}
