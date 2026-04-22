// Utility functions
// Replaces sie_utils.h

const std = @import("std");

/// Convert string to double
pub fn strToDouble(str: []const u8) !f64 {
    return try std.fmt.parseFloat(f64, str);
}

/// Check if a string is a valid number
pub fn isNumeric(str: []const u8) bool {
    if (std.fmt.parseFloat(f64, str)) |_| {
        return true;
    } else |_| {
        return false;
    }
}

/// Trim whitespace from string
pub fn trim(str: []const u8) []const u8 {
    return std.mem.trim(u8, str, " \t\n\r");
}

/// Find substring in string
pub fn indexOf(haystack: []const u8, needle: []const u8) ?usize {
    return std.mem.indexOf(u8, haystack, needle);
}

/// Compare strings case-insensitively
pub fn strcasecmp(a: []const u8, b: []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |ca, cb| {
        if (std.ascii.toLower(ca) != std.ascii.toLower(cb)) return false;
    }
    return true;
}

/// Convert integer to string
pub fn intToString(allocator: std.mem.Allocator, value: i64) ![]const u8 {
    return try std.fmt.allocPrint(allocator, "{}", .{value});
}

test "string to double" {
    const value = try strToDouble("3.14");
    try std.testing.expectApproxEqAbs(value, 3.14, 0.01);
}

test "is numeric" {
    try std.testing.expect(isNumeric("123"));
    try std.testing.expect(isNumeric("3.14"));
    try std.testing.expect(!isNumeric("abc"));
}

test "trim string" {
    const result = trim("  hello world  ");
    try std.testing.expectEqualSlices(u8, result, "hello world");
}

test "find substring" {
    try std.testing.expectEqual(indexOf("hello world", "world"), 6);
    try std.testing.expectEqual(indexOf("hello world", "xyz"), null);
}
