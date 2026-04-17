// Dynamic array utilities
// Replaces sie_vec.h
// In Zig 0.15, we use std.ArrayList directly

const std = @import("std");

/// Wrapper type for ArrayList
pub fn Vec(comptime T: type) type {
    return std.ArrayList(T);
}
