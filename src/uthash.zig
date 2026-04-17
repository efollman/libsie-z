// Hash table utilities
// Replaces sie_uthash.h (Troy Hanson's uthash library)

const std = @import("std");

/// Generic hash map wrapper around std.AutoHashMap
pub fn HashMap(comptime K: type, comptime V: type) type {
    return struct {
        map: std.AutoHashMap(K, V),

        const Self = @This();
        pub const MapType = std.AutoHashMap(K, V);

        pub fn init(allocator: std.mem.Allocator) Self {
            return Self{
                .map = std.AutoHashMap(K, V).init(allocator),
            };
        }

        pub fn deinit(self: *Self) void {
            self.map.deinit();
        }

        pub fn put(self: *Self, key: K, value: V) !void {
            _ = try self.map.put(key, value);
        }

        pub fn get(self: *const Self, key: K) ?V {
            return self.map.get(key);
        }

        pub fn contains(self: *const Self, key: K) bool {
            return self.map.contains(key);
        }

        pub fn remove(self: *Self, key: K) bool {
            return self.map.remove(key);
        }

        pub fn clear(self: *Self) void {
            self.map.clearRetainingCapacity();
        }

        pub fn count(self: *const Self) usize {
            return self.map.count();
        }

        pub fn iterator(self: *const Self) std.AutoHashMap(K, V).Iterator {
            return self.map.iterator();
        }

        pub fn keyIterator(self: *const Self) std.AutoHashMap(K, V).KeyIterator {
            return self.map.keyIterator();
        }

        pub fn valueIterator(self: *const Self) std.AutoHashMap(K, V).ValueIterator {
            return self.map.valueIterator();
        }
    };
}

test "hash map operations" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var map = HashMap(u32, []const u8).init(allocator);
    defer map.deinit();

    try map.put(1, "one");
    try map.put(2, "two");
    try map.put(3, "three");

    try std.testing.expectEqual(map.count(), 3);
    try std.testing.expectEqualSlices(u8, map.get(1).?, "one");
    try std.testing.expectEqualSlices(u8, map.get(2).?, "two");
    try std.testing.expectEqual(map.get(4), null);

    _ = map.remove(2);
    try std.testing.expectEqual(map.count(), 2);
    try std.testing.expectEqual(map.get(2), null);
}
