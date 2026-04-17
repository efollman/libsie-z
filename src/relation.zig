// Relation - key/value string store
// Replaces sie_relation.h / sie_relation.c
//
// The C implementation is a flat packed buffer with offset-based pointers.
// This Zig version uses a simple ordered list of key-value pairs with
// proper string ownership.

const std = @import("std");

pub const Parameter = struct {
    name: []const u8,
    value: []const u8,
};

/// Ordered key-value string store
pub const Relation = struct {
    allocator: std.mem.Allocator,
    params: std.ArrayList(OwnedParam) = .{},

    const OwnedParam = struct {
        name: []u8,
        value: []u8,
    };

    pub fn init(allocator: std.mem.Allocator) Relation {
        return .{
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *Relation) void {
        for (self.params.items) |p| {
            self.allocator.free(p.name);
            self.allocator.free(p.value);
        }
        self.params.deinit(self.allocator);
    }

    /// Number of key-value pairs
    pub fn count(self: *const Relation) usize {
        return self.params.items.len;
    }

    /// Get name at index
    pub fn getName(self: *const Relation, idx: usize) ?[]const u8 {
        if (idx >= self.params.items.len) return null;
        return self.params.items[idx].name;
    }

    /// Get value at index
    pub fn getValue(self: *const Relation, idx: usize) ?[]const u8 {
        if (idx >= self.params.items.len) return null;
        return self.params.items[idx].value;
    }

    /// Find index of a name (linear scan)
    pub fn nameIndex(self: *const Relation, name: []const u8) ?usize {
        for (self.params.items, 0..) |p, i| {
            if (std.mem.eql(u8, p.name, name)) return i;
        }
        return null;
    }

    /// Look up value by name
    pub fn value(self: *const Relation, name: []const u8) ?[]const u8 {
        if (self.nameIndex(name)) |idx| {
            return self.params.items[idx].value;
        }
        return null;
    }

    /// Set value for a name (updates existing or appends)
    pub fn setValue(self: *Relation, name: []const u8, val: []const u8) !void {
        if (self.nameIndex(name)) |idx| {
            self.allocator.free(self.params.items[idx].value);
            self.params.items[idx].value = try self.allocator.dupe(u8, val);
        } else {
            try self.params.append(self.allocator, .{
                .name = try self.allocator.dupe(u8, name),
                .value = try self.allocator.dupe(u8, val),
            });
        }
    }

    /// Set name at index
    pub fn setNameAtIndex(self: *Relation, idx: usize, name: []const u8) !void {
        if (idx >= self.params.items.len) return;
        self.allocator.free(self.params.items[idx].name);
        self.params.items[idx].name = try self.allocator.dupe(u8, name);
    }

    /// Set value at index
    pub fn setValueAtIndex(self: *Relation, idx: usize, val: []const u8) !void {
        if (idx >= self.params.items.len) return;
        self.allocator.free(self.params.items[idx].value);
        self.params.items[idx].value = try self.allocator.dupe(u8, val);
    }

    /// Delete entry at index
    pub fn deleteAtIndex(self: *Relation, idx: usize) void {
        if (idx >= self.params.items.len) return;
        const p = self.params.items[idx];
        self.allocator.free(p.name);
        self.allocator.free(p.value);
        _ = self.params.orderedRemove(idx);
    }

    /// Remove all entries
    pub fn clear(self: *Relation) void {
        for (self.params.items) |p| {
            self.allocator.free(p.name);
            self.allocator.free(p.value);
        }
        self.params.clearRetainingCapacity();
    }

    /// Clone the relation
    pub fn clone(self: *const Relation) !Relation {
        var new = Relation.init(self.allocator);
        errdefer new.deinit();
        try new.params.ensureTotalCapacity(self.allocator, self.params.items.len);
        for (self.params.items) |p| {
            new.params.appendAssumeCapacity(.{
                .name = try self.allocator.dupe(u8, p.name),
                .value = try self.allocator.dupe(u8, p.value),
            });
        }
        return new;
    }

    /// Merge two relations (self takes priority for duplicate keys)
    pub fn merge(self: *Relation, other: *const Relation) !void {
        for (other.params.items) |p| {
            if (self.nameIndex(p.name) == null) {
                try self.params.append(self.allocator, .{
                    .name = try self.allocator.dupe(u8, p.name),
                    .value = try self.allocator.dupe(u8, p.value),
                });
            }
        }
    }

    /// Scan value as integer
    pub fn intValue(self: *const Relation, name: []const u8) ?i64 {
        const v = self.value(name) orelse return null;
        return std.fmt.parseInt(i64, v, 10) catch null;
    }

    /// Scan value as float
    pub fn floatValue(self: *const Relation, name: []const u8) ?f64 {
        const v = self.value(name) orelse return null;
        return std.fmt.parseFloat(f64, v) catch null;
    }

    /// Split a string into a relation using delimiters
    pub fn splitString(
        allocator: std.mem.Allocator,
        input: []const u8,
        assign: u8,
        delimit: u8,
    ) !Relation {
        var rel = Relation.init(allocator);
        errdefer rel.deinit();

        var rest = input;
        while (rest.len > 0) {
            // Find delimiter
            const end = std.mem.indexOfScalar(u8, rest, delimit) orelse rest.len;
            const pair = rest[0..end];
            rest = if (end < rest.len) rest[end + 1 ..] else &.{};

            // Find assignment
            if (std.mem.indexOfScalar(u8, pair, assign)) |eq| {
                const name = std.mem.trim(u8, pair[0..eq], " \t");
                const val = std.mem.trim(u8, pair[eq + 1 ..], " \t");
                try rel.params.append(allocator, .{
                    .name = try allocator.dupe(u8, name),
                    .value = try allocator.dupe(u8, val),
                });
            } else {
                const name = std.mem.trim(u8, pair, " \t");
                if (name.len > 0) {
                    try rel.params.append(allocator, .{
                        .name = try allocator.dupe(u8, name),
                        .value = try allocator.dupe(u8, ""),
                    });
                }
            }
        }
        return rel;
    }

    /// Decode a CGI-style query string (key=value&key2=value2)
    pub fn decodeQueryString(allocator: std.mem.Allocator, query: []const u8) !Relation {
        return splitString(allocator, query, '=', '&');
    }
};

// ─── Tests ──────────────────────────────────────────────────────

test "relation basic operations" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var rel = Relation.init(allocator);
    defer rel.deinit();

    try rel.setValue("name", "alice");
    try rel.setValue("age", "30");

    try std.testing.expectEqual(@as(usize, 2), rel.count());
    try std.testing.expectEqualStrings("alice", rel.value("name").?);
    try std.testing.expectEqualStrings("30", rel.value("age").?);

    // Update existing
    try rel.setValue("name", "bob");
    try std.testing.expectEqual(@as(usize, 2), rel.count());
    try std.testing.expectEqualStrings("bob", rel.value("name").?);

    // Missing key
    try std.testing.expect(rel.value("missing") == null);
}

test "relation index access" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var rel = Relation.init(allocator);
    defer rel.deinit();

    try rel.setValue("x", "1");
    try rel.setValue("y", "2");

    try std.testing.expectEqualStrings("x", rel.getName(0).?);
    try std.testing.expectEqualStrings("1", rel.getValue(0).?);
    try std.testing.expectEqualStrings("y", rel.getName(1).?);

    // Delete first
    rel.deleteAtIndex(0);
    try std.testing.expectEqual(@as(usize, 1), rel.count());
    try std.testing.expectEqualStrings("y", rel.getName(0).?);
}

test "relation clone and merge" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var r1 = Relation.init(allocator);
    defer r1.deinit();
    try r1.setValue("a", "1");
    try r1.setValue("b", "2");

    // Clone
    var r2 = try r1.clone();
    defer r2.deinit();
    try std.testing.expectEqual(@as(usize, 2), r2.count());
    try std.testing.expectEqualStrings("1", r2.value("a").?);

    // Merge — r1 keeps its values, gets new keys from r3
    var r3 = Relation.init(allocator);
    defer r3.deinit();
    try r3.setValue("b", "overridden");
    try r3.setValue("c", "3");

    try r1.merge(&r3);
    try std.testing.expectEqual(@as(usize, 3), r1.count());
    try std.testing.expectEqualStrings("2", r1.value("b").?); // kept original
    try std.testing.expectEqualStrings("3", r1.value("c").?); // added new
}

test "relation split string" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var rel = try Relation.splitString(allocator, "color=red;size=10;bold", '=', ';');
    defer rel.deinit();

    try std.testing.expectEqual(@as(usize, 3), rel.count());
    try std.testing.expectEqualStrings("red", rel.value("color").?);
    try std.testing.expectEqualStrings("10", rel.value("size").?);
    try std.testing.expectEqualStrings("", rel.value("bold").?);
}

test "relation typed value access" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var rel = Relation.init(allocator);
    defer rel.deinit();

    try rel.setValue("count", "42");
    try rel.setValue("rate", "3.14");
    try rel.setValue("bad", "xyz");

    try std.testing.expectEqual(@as(i64, 42), rel.intValue("count").?);
    try std.testing.expectApproxEqAbs(@as(f64, 3.14), rel.floatValue("rate").?, 0.001);
    try std.testing.expect(rel.intValue("bad") == null);
    try std.testing.expect(rel.intValue("missing") == null);
}

test "relation query string decode" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var rel = try Relation.decodeQueryString(allocator, "foo=bar&x=1&empty=");
    defer rel.deinit();

    try std.testing.expectEqual(@as(usize, 3), rel.count());
    try std.testing.expectEqualStrings("bar", rel.value("foo").?);
    try std.testing.expectEqualStrings("1", rel.value("x").?);
    try std.testing.expectEqualStrings("", rel.value("empty").?);
}
