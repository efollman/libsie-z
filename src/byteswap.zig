// Byte swapping and endianness utilities
// Replaces sie_byteswap.h

const std = @import("std");
const config = @import("config.zig");

/// Convert 16-bit value from network byte order (big-endian) to host byte order
pub fn ntoh16(value: u16) u16 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Convert 16-bit value from host byte order to network byte order (big-endian)
pub fn hton16(value: u16) u16 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Convert 32-bit value from network byte order (big-endian) to host byte order
pub fn ntoh32(value: u32) u32 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Convert 32-bit value from host byte order to network byte order (big-endian)
pub fn hton32(value: u32) u32 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Convert 64-bit value from network byte order (big-endian) to host byte order
pub fn ntoh64(value: u64) u64 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Convert 64-bit value from host byte order to network byte order (big-endian)
pub fn hton64(value: u64) u64 {
    if (config.IS_LITTLE_ENDIAN) {
        return @byteSwap(value);
    } else {
        return value;
    }
}

/// Byte swap for floating point (used for f64 endianness)
pub fn swapF64Bytes(value: f64) f64 {
    const bits = @as(u64, @bitCast(value));
    const swapped = @byteSwap(bits);
    return @as(f64, @bitCast(swapped));
}

/// Convert f64 from big-endian bytes to host byte order
pub fn ntohF64(value: f64) f64 {
    if (config.IS_LITTLE_ENDIAN) {
        return swapF64Bytes(value);
    } else {
        return value;
    }
}

/// Convert f64 from host byte order to big-endian bytes
pub fn htonF64(value: f64) f64 {
    if (config.IS_LITTLE_ENDIAN) {
        return swapF64Bytes(value);
    } else {
        return value;
    }
}

test "byte swap 16-bit" {
    const std_import = std;
    try std_import.testing.expectEqual(hton16(0x1234), if (config.IS_LITTLE_ENDIAN) 0x3412 else 0x1234);
    try std_import.testing.expectEqual(ntoh16(0x1234), if (config.IS_LITTLE_ENDIAN) 0x3412 else 0x1234);
}

test "byte swap 32-bit" {
    const std_import = std;
    try std_import.testing.expectEqual(hton32(0x12345678), if (config.IS_LITTLE_ENDIAN) 0x78563412 else 0x12345678);
    try std_import.testing.expectEqual(ntoh32(0x12345678), if (config.IS_LITTLE_ENDIAN) 0x78563412 else 0x12345678);
}

test "byte swap 64-bit" {
    const std_import = std;
    try std_import.testing.expectEqual(hton64(0x123456789ABCDEF0), if (config.IS_LITTLE_ENDIAN) 0xF0DEBC9A78563412 else 0x123456789ABCDEF0);
}
