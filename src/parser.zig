// Expression/tag parsing
// Replaces sie_parser.h

const std = @import("std");
const error_mod = @import("error.zig");

/// Parse result from parsing operations
pub const ParseResult = struct {
    value: []const u8,
    remaining: []const u8,
};

/// Parse an XML-like tag name
pub fn parseTagName(input: []const u8) !ParseResult {
    if (input.len == 0) {
        return error_mod.Error.ParseError;
    }

    var end: usize = 0;
    for (input, 0..) |char, i| {
        if (char == ' ' or char == '\t' or char == '\n' or char == '>' or char == '/') {
            end = i;
            break;
        }
    } else {
        end = input.len;
    }

    if (end == 0) {
        return error_mod.Error.ParseError;
    }

    return ParseResult{
        .value = input[0..end],
        .remaining = if (end < input.len) input[end..] else "",
    };
}

/// Skip whitespace
pub fn skipWhitespace(input: []const u8) []const u8 {
    var i: usize = 0;
    while (i < input.len and (input[i] == ' ' or input[i] == '\t' or input[i] == '\n' or input[i] == '\r')) {
        i += 1;
    }
    return input[i..];
}

/// Parse a quoted string value
pub fn parseQuotedString(input: []const u8) !ParseResult {
    if (input.len < 2 or input[0] != '"') {
        return error_mod.Error.ParseError;
    }

    var end: usize = 1;
    while (end < input.len and input[end] != '"') {
        end += 1;
    }

    if (end >= input.len) {
        return error_mod.Error.ParseError;
    }

    return ParseResult{
        .value = input[1..end],
        .remaining = if (end + 1 < input.len) input[end + 1 ..] else "",
    };
}

/// Parse a number
pub fn parseNumber(input: []const u8) !ParseResult {
    if (input.len == 0) {
        return error_mod.Error.ParseError;
    }

    var end: usize = 0;
    const start_char = input[0];

    // Check for sign
    if (start_char == '-' or start_char == '+') {
        end = 1;
    }

    if (end >= input.len) {
        return error_mod.Error.ParseError;
    }

    // Consume digits
    while (end < input.len and std.ascii.isDigit(input[end])) {
        end += 1;
    }

    const min_digits: usize = if (start_char == '-' or start_char == '+') 1 else 0;
    if (end == min_digits) {
        return error_mod.Error.ParseError;
    }

    // Handle decimal point
    if (end < input.len and input[end] == '.') {
        end += 1;
        while (end < input.len and std.ascii.isDigit(input[end])) {
            end += 1;
        }
    }

    return ParseResult{
        .value = input[0..end],
        .remaining = if (end < input.len) input[end..] else "",
    };
}

test "parse tag name" {
    const result = try parseTagName("channel type=\"time\"");
    try std.testing.expectEqualSlices(u8, result.value, "channel");
    try std.testing.expectEqualSlices(u8, result.remaining, " type=\"time\"");
}

test "skip whitespace" {
    const input = "   \t\n  hello";
    const result = skipWhitespace(input);
    try std.testing.expectEqualSlices(u8, result, "hello");
}

test "parse quoted string" {
    const result = try parseQuotedString("\"hello world\" rest");
    try std.testing.expectEqualSlices(u8, result.value, "hello world");
    try std.testing.expectEqualSlices(u8, result.remaining, " rest");
}

test "parse number" {
    const result = try parseNumber("123.45 more");
    try std.testing.expectEqualSlices(u8, result.value, "123.45");
    try std.testing.expectEqualSlices(u8, result.remaining, " more");
}
