// Platform configuration
// Replaces sie_config.h autoconf generation

const std = @import("std");
const builtin = @import("builtin");

/// Detect platform endianness at compile time
pub const ENDIANNESS = builtin.cpu.arch.endian();

/// Platform-specific defines
pub const IS_LITTLE_ENDIAN = ENDIANNESS == .little;
pub const IS_BIG_ENDIAN = ENDIANNESS == .big;

/// Platform detection
pub const IS_WINDOWS = builtin.os.tag == .windows;
pub const IS_UNIX = !IS_WINDOWS;
pub const IS_LINUX = builtin.os.tag == .linux;
pub const IS_MACOS = builtin.os.tag == .macos;

/// Determine 32-bit unsigned integer type
pub fn getUint32Type() type {
    return u32;
}

/// Determine 64-bit float type (double)
pub fn getFloat64Type() type {
    return f64;
}

test "endianness detection" {
    // This test verifies compile-time endianness detection works
    _ = IS_LITTLE_ENDIAN;
    _ = IS_BIG_ENDIAN;
}

test "platform detection" {
    _ = IS_WINDOWS;
    _ = IS_UNIX;
    _ = IS_LINUX;
    _ = IS_MACOS;
}
