// String table tests — ports of t_stringtable.c
// Tests: interning/deduplication, empty strings, binary-safe strings

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const StringTable = libsie.stringtable.StringTable;

test "stringtable: interning deduplication" {
    // Port of test_stringtable — same string returns same pointer
    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    const null_string = try st.intern("");
    const test_string = try st.intern("test");
    const ch_string = try st.intern("ch");

    const null2_string = try st.intern("");
    const test2_string = try st.intern("test");
    const ch2_string = try st.intern("ch");

    // All interned strings should be non-null (zero-length slice is not null)
    try testing.expect(test_string.len > 0);
    try testing.expect(ch_string.len > 0);

    // Different strings are different pointers
    try testing.expect(null_string.ptr != test_string.ptr);
    try testing.expect(null_string.ptr != ch_string.ptr);

    // Same strings return the same pointer (deduplication)
    try testing.expect(null_string.ptr == null2_string.ptr);
    try testing.expect(test_string.ptr == test2_string.ptr);
    try testing.expect(ch_string.ptr == ch2_string.ptr);
}

test "stringtable: count tracks unique entries" {
    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    try testing.expectEqual(@as(usize, 0), st.count());

    _ = try st.intern("alpha");
    try testing.expectEqual(@as(usize, 1), st.count());

    _ = try st.intern("beta");
    try testing.expectEqual(@as(usize, 2), st.count());

    // Re-interning same string doesn't increase count
    _ = try st.intern("alpha");
    try testing.expectEqual(@as(usize, 2), st.count());

    _ = try st.intern("gamma");
    try testing.expectEqual(@as(usize, 3), st.count());
}

test "stringtable: contains and get" {
    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    try testing.expect(!st.contains("hello"));
    try testing.expect(st.get("hello") == null);

    _ = try st.intern("hello");

    try testing.expect(st.contains("hello"));
    try testing.expect(st.get("hello") != null);
    try testing.expectEqualStrings("hello", st.get("hello").?);

    try testing.expect(!st.contains("world"));
}

test "stringtable: empty string interning" {
    // Port of test_stringtable — empty string handling
    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    const empty1 = try st.intern("");
    const empty2 = try st.intern("");

    try testing.expectEqual(@as(usize, 0), empty1.len);
    try testing.expectEqual(@as(usize, 0), empty2.len);
    try testing.expect(empty1.ptr == empty2.ptr);
}

test "stringtable: many strings" {
    // Port of test_grow concept — many entries
    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    var buf: [32]u8 = undefined;
    var i: usize = 0;
    while (i < 200) : (i += 1) {
        const s = std.fmt.bufPrint(&buf, "string_{d}", .{i}) catch unreachable;
        _ = try st.intern(s);
    }

    try testing.expectEqual(@as(usize, 200), st.count());

    // Verify deduplication still works
    const s0 = try st.intern("string_0");
    const s0_again = try st.intern("string_0");
    try testing.expect(s0.ptr == s0_again.ptr);
    try testing.expectEqual(@as(usize, 200), st.count());
}
