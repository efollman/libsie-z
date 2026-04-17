// Error handling and exception system
// Replaces apr_status_t and sie_Exception

const std = @import("std");

/// Error enumeration for LibSIE operations
pub const Error = error{
    // File I/O errors
    FileNotFound,
    PermissionDenied,
    FileOpenError,
    FileReadError,
    FileWriteError,
    FileSeekError,
    FileTruncated,

    // Format errors
    InvalidFormat,
    InvalidBlock,
    UnexpectedEof,
    CorruptedData,

    // Parsing errors
    InvalidXml,
    InvalidExpression,
    ParseError,

    // Memory errors
    OutOfMemory,

    // Data errors
    InvalidData,
    DimensionMismatch,
    IndexOutOfBounds,
    SizeMismatch,

    // Operation errors
    NotImplemented,
    OperationFailed,
    StreamEnded,

    // General errors
    UnknownError,
};

/// Exception structure for storing error information
pub const Exception = struct {
    error_code: u32,
    message: []const u8,
    source_file: []const u8 = "",
    source_line: u32 = 0,

    pub fn init(allocator: std.mem.Allocator, message: []const u8) !Exception {
        return Exception{
            .error_code = 1,
            .message = try allocator.dupe(u8, message),
        };
    }

    pub fn deinit(self: *Exception, allocator: std.mem.Allocator) void {
        allocator.free(self.message);
    }
};

/// Map Zig Error to LibSIE status code
pub fn errorToStatus(err: anyerror) u32 {
    return switch (err) {
        Error.FileNotFound => 1,
        Error.PermissionDenied => 2,
        Error.FileOpenError => 3,
        Error.FileReadError => 4,
        Error.FileWriteError => 5,
        Error.FileSeekError => 6,
        Error.FileTruncated => 7,
        Error.InvalidFormat => 10,
        Error.InvalidBlock => 11,
        Error.UnexpectedEof => 12,
        Error.CorruptedData => 13,
        Error.InvalidXml => 20,
        Error.InvalidExpression => 21,
        Error.ParseError => 22,
        Error.OutOfMemory => 30,
        Error.InvalidData => 40,
        Error.DimensionMismatch => 41,
        Error.IndexOutOfBounds => 42,
        Error.NotImplemented => 50,
        Error.OperationFailed => 51,
        Error.StreamEnded => 52,
        else => 99,
    };
}

/// Map status code back to error message
pub fn statusToMessage(status: u32) []const u8 {
    return switch (status) {
        1 => "File not found",
        2 => "Permission denied",
        3 => "Failed to open file",
        4 => "Failed to read from file",
        5 => "Failed to write to file",
        6 => "Failed to seek in file",
        7 => "File truncated unexpectedly",
        10 => "Invalid file format",
        11 => "Invalid data block",
        12 => "Unexpected end of file",
        13 => "Corrupted data detected",
        20 => "Invalid XML",
        21 => "Invalid expression",
        22 => "Parse error",
        30 => "Out of memory",
        40 => "Invalid data",
        41 => "Dimension mismatch",
        42 => "Index out of bounds",
        50 => "Not implemented",
        51 => "Operation failed",
        52 => "Stream ended",
        else => "Unknown error",
    };
}

test "error mapping" {
    const status = errorToStatus(Error.FileNotFound);
    try std.testing.expectEqual(status, 1);

    const msg = statusToMessage(status);
    try std.testing.expectEqualSlices(u8, msg, "File not found");
}
