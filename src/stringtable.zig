// String table / string interning
// Replaces sie_stringtable.h

const std = @import("std");

/// String table for interning strings (deduplication)
pub const StringTable = struct {
    strings: std.StringHashMap([]const u8),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) StringTable {
        return StringTable{
            .strings = std.StringHashMap([]const u8).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *StringTable) void {
        var iter = self.strings.valueIterator();
        while (iter.next()) |value| {
            self.allocator.free(value.*);
        }
        self.strings.deinit();
    }

    /// Intern a string - returns deduplicated pointer
    pub fn intern(self: *StringTable, str: []const u8) ![]const u8 {
        if (self.strings.get(str)) |existing| {
            return existing;
        }

        const copy = try self.allocator.dupe(u8, str);
        try self.strings.put(copy, copy);
        return copy;
    }

    /// Get an interned string, or null if not found
    pub fn get(self: *const StringTable, str: []const u8) ?[]const u8 {
        return self.strings.get(str);
    }

    /// Check if a string is interned
    pub fn contains(self: *const StringTable, str: []const u8) bool {
        return self.strings.contains(str);
    }

    pub fn count(self: *const StringTable) usize {
        return self.strings.count();
    }
};

test "string interning" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var table = StringTable.init(allocator);
    defer table.deinit();

    const str1 = try table.intern("hello");
    const str2 = try table.intern("hello");
    const str3 = try table.intern("world");

    // Same string should return same pointer
    try std.testing.expectEqual(str1.ptr, str2.ptr);
    try std.testing.expect(str1.ptr != str3.ptr);
    try std.testing.expectEqual(table.count(), 2);
}
