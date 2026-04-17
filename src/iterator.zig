// Iterator types for collections
// Replaces sie_iterator.h
//
// Zig has idiomatic iterator patterns (struct with next() -> ?T).
// These iterators wrap the various collection types used in libsie.

const std = @import("std");
const xml_mod = @import("xml.zig");
const uthash = @import("uthash.zig");
const Node = xml_mod.Node;

/// Iterator over a slice of pointers
pub fn SliceIterator(comptime T: type) type {
    return struct {
        items: []const T,
        index: usize = 0,

        const Self = @This();

        pub fn init(items: []const T) Self {
            return .{ .items = items };
        }

        pub fn next(self: *Self) ?T {
            if (self.index >= self.items.len) return null;
            const item = self.items[self.index];
            self.index += 1;
            return item;
        }

        pub fn reset(self: *Self) void {
            self.index = 0;
        }

        pub fn remaining(self: *const Self) usize {
            return self.items.len - self.index;
        }
    };
}

/// Iterator over HashMap values (uses uthash.HashMap wrapper)
pub fn HashMapValueIterator(comptime K: type, comptime V: type) type {
    return struct {
        inner: uthash.HashMap(K, V).MapType.Iterator,

        const Self = @This();

        pub fn init(map: *const uthash.HashMap(K, V)) Self {
            return .{ .inner = map.iterator() };
        }

        pub fn next(self: *Self) ?V {
            if (self.inner.next()) |entry| {
                return entry.value_ptr.*;
            }
            return null;
        }

        pub fn nextEntry(self: *Self) ?struct { key: K, value: V } {
            if (self.inner.next()) |entry| {
                return .{ .key = entry.key_ptr.*, .value = entry.value_ptr.* };
            }
            return null;
        }
    };
}

/// Iterator over XML child elements, optionally filtering by name and/or attribute
pub const XmlIterator = struct {
    current: ?*Node,
    name: ?[]const u8,
    attr: ?[]const u8,
    attr_value: ?[]const u8,

    pub fn init(
        parent: *Node,
        name: ?[]const u8,
        attr: ?[]const u8,
        attr_value: ?[]const u8,
    ) XmlIterator {
        return .{
            .current = parent.child,
            .name = name,
            .attr = attr,
            .attr_value = attr_value,
        };
    }

    pub fn next(self: *XmlIterator) ?*Node {
        while (self.current) |node| {
            self.current = node.next;

            if (node.node_type != .Element) continue;

            // Filter by name
            if (self.name) |n| {
                if (!std.mem.eql(u8, node.getName(), n)) continue;
            }

            // Filter by attribute value
            if (self.attr) |a| {
                const val = node.getAttribute(a) orelse continue;
                if (self.attr_value) |expected| {
                    if (!std.mem.eql(u8, val, expected)) continue;
                }
            }

            return node;
        }
        return null;
    }
};

// ─── Tests ──────────────────────────────────────────────────────

test "slice iterator" {
    const items = [_]i32{ 10, 20, 30 };
    var iter = SliceIterator(i32).init(&items);

    try std.testing.expectEqual(@as(i32, 10), iter.next().?);
    try std.testing.expectEqual(@as(i32, 20), iter.next().?);
    try std.testing.expectEqual(@as(usize, 1), iter.remaining());
    try std.testing.expectEqual(@as(i32, 30), iter.next().?);
    try std.testing.expect(iter.next() == null);

    iter.reset();
    try std.testing.expectEqual(@as(i32, 10), iter.next().?);
}

test "hash map iterator" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var map = uthash.HashMap(i32, []const u8).init(allocator);
    defer map.deinit();
    try map.put(1, "one");
    try map.put(2, "two");

    var iter = HashMapValueIterator(i32, []const u8).init(&map);
    var found: usize = 0;
    while (iter.next()) |_| found += 1;
    try std.testing.expectEqual(@as(usize, 2), found);
}

test "xml iterator" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var parser = xml_mod.XmlParser.init(allocator);
    defer parser.deinit();
    try parser.parse("<root><ch id=\"1\"/><dim index=\"0\"/><ch id=\"2\"/></root>");

    const doc = parser.getDocument().?;

    // Iterate all children
    {
        var iter = XmlIterator.init(doc, null, null, null);
        var count: usize = 0;
        while (iter.next()) |_| count += 1;
        try std.testing.expectEqual(@as(usize, 3), count);
    }

    // Filter by name
    {
        var iter = XmlIterator.init(doc, "ch", null, null);
        var count: usize = 0;
        while (iter.next()) |_| count += 1;
        try std.testing.expectEqual(@as(usize, 2), count);
    }

    // Filter by attribute value
    {
        var iter = XmlIterator.init(doc, "ch", "id", "2");
        const node = iter.next();
        try std.testing.expect(node != null);
        try std.testing.expectEqualStrings("2", node.?.getAttribute("id").?);
        try std.testing.expect(iter.next() == null);
    }
}

test "xml iterator empty" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var parser = xml_mod.XmlParser.init(allocator);
    defer parser.deinit();
    try parser.parse("<empty/>");

    const doc = parser.getDocument().?;
    var iter = XmlIterator.init(doc, null, null, null);
    try std.testing.expect(iter.next() == null);
}
