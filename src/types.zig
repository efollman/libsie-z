// Type definitions for LibSIE
// Replaces sie_types.h and APR type definitions

pub const Types = struct {
    pub const i8_t = i8;
    pub const i16_t = i16;
    pub const i32_t = i32;
    pub const i64_t = i64;
    pub const u8_t = u8;
    pub const u16_t = u16;
    pub const u32_t = u32;
    pub const u64_t = u64;
    pub const f32_t = f32;
    pub const f64_t = f64;
};

// Core integer types (matching C library)
pub const sie_int8 = i8;
pub const sie_int16 = i16;
pub const sie_int32 = i32;
pub const sie_int64 = i64;
pub const sie_uint8 = u8;
pub const sie_uint16 = u16;
pub const sie_uint32 = u32;
pub const sie_uint64 = u64;

// Floating point types
pub const sie_float32 = f32;
pub const sie_float64 = f64;

// File offset type (replaces apr_off_t)
pub const sie_off_t = i64;

// Size types (replaces apr_size_t, apr_ssize_t)
pub const sie_size_t = usize;
pub const sie_ssize_t = isize;

// Status type (replacing apr_status_t with error union approach)
pub const sie_status_t = u32;

test "type sizes" {
    const std = @import("std");
    try std.testing.expectEqual(@sizeOf(i8), 1);
    try std.testing.expectEqual(@sizeOf(i32), 4);
    try std.testing.expectEqual(@sizeOf(i64), 8);
    try std.testing.expectEqual(@sizeOf(f64), 8);
}
