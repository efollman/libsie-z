// Data-driven decoder tests
// Based on t_decoder.c
//
// Reads decoder test files from test/data/decoders/, compiles XML definitions
// to bytecode, runs the decoder VM, and compares output against expected values.

const std = @import("std");
const libsie = @import("libsie");
const xml_mod = libsie.xml;
const compiler_mod = libsie.compiler;
const decoder_mod = libsie.decoder;
const output_mod = libsie.output;

const testing = std.testing;

/// Parsed expected output for a test case
const ExpectedOutput = struct {
    allocator: std.mem.Allocator,
    output: ?output_mod.Output, // null means failure expected
    is_failure: bool,

    fn deinit(self: *ExpectedOutput) void {
        if (self.output) |*o| o.deinit();
    }
};

/// Parse a hex data section from the test file
fn parseHexData(allocator: std.mem.Allocator, lines: []const []const u8) ![]u8 {
    var data = std.ArrayList(u8){};
    for (lines) |line| {
        var it = std.mem.tokenizeAny(u8, line, " \t\r\n");
        while (it.next()) |token| {
            const byte = std.fmt.parseInt(u8, token, 16) catch continue;
            try data.append(allocator, byte);
        }
    }
    return data.toOwnedSlice(allocator);
}

/// Parse an expected output number value
/// Supports: raw hex (xDEADBEEF), hex integer (0x...), float/int
const ParsedNumber = union(enum) {
    float64: f64,
    raw: []u8,
};

fn parseNumber(allocator: std.mem.Allocator, num: []const u8) !ParsedNumber {
    if (num.len == 0) return error.InvalidData;

    // Raw data: xDEADBEEF
    if (num[0] == 'x') {
        var raw_data = std.ArrayList(u8){};
        var i: usize = 1;
        while (i + 1 < num.len) : (i += 2) {
            const byte = std.fmt.parseInt(u8, num[i..][0..2], 16) catch break;
            try raw_data.append(allocator, byte);
        }
        return .{ .raw = try raw_data.toOwnedSlice(allocator) };
    }

    // Hex integer: 0x01020304
    if (num.len > 2 and num[0] == '0' and num[1] == 'x') {
        const val = std.fmt.parseInt(u64, num, 0) catch return error.InvalidData;
        return .{ .float64 = @floatFromInt(val) };
    }

    // Regular number
    const val = std.fmt.parseFloat(f64, num) catch return error.InvalidData;
    return .{ .float64 = val };
}

/// Parse expected output section
fn parseExpected(allocator: std.mem.Allocator, lines: []const []const u8) !ExpectedOutput {
    if (lines.len == 0) return error.InvalidData;

    // Check for "fail"
    const first = std.mem.trim(u8, lines[0], " \t\r\n");
    if (std.mem.startsWith(u8, first, "fail")) {
        return .{ .allocator = allocator, .output = null, .is_failure = true };
    }

    // First line: number of dimensions
    const num_dims = std.fmt.parseInt(usize, first, 10) catch return error.InvalidData;

    var output = try output_mod.Output.init(allocator, num_dims);
    errdefer output.deinit();

    // Parse data rows
    var dim: usize = 0;
    var row: usize = 0;
    for (lines[1..]) |line| {
        var it = std.mem.tokenizeAny(u8, line, " \t\r\n");
        while (it.next()) |token| {
            const num = try parseNumber(allocator, token);
            switch (num) {
                .float64 => |val| {
                    if (row == 0 and dim < num_dims) {
                        output.setType(dim, .Float64);
                    }
                    // Ensure capacity
                    while (output.dimensions[dim].guts.capacity <= row) {
                        try output.grow(dim);
                    }
                    if (output.dimensions[dim].float64_data) |data| {
                        data[row] = val;
                    }
                },
                .raw => |raw_bytes| {
                    defer allocator.free(raw_bytes);
                    if (row == 0 and dim < num_dims) {
                        output.setType(dim, .Raw);
                    }
                    while (output.dimensions[dim].guts.capacity <= row) {
                        try output.grow(dim);
                    }
                    try output.setRaw(dim, row, raw_bytes);
                },
            }
            dim += 1;
            if (dim >= num_dims) {
                row += 1;
                output.num_rows = row;
                dim = 0;
            }
        }
    }

    return .{ .allocator = allocator, .output = output, .is_failure = false };
}

/// Test case data parsed from a test file
const TestCase = struct {
    data_start: usize,
    data_end: usize,
    expected_start: usize,
    expected_end: usize,
};

/// Complete parsed test file
const TestFile = struct {
    allocator: std.mem.Allocator,
    xml_text: []u8,
    cases: []TestCase,
    all_lines: [][]u8,

    fn getLines(self: *const TestFile, start: usize, end: usize) []const []const u8 {
        return @ptrCast(self.all_lines[start..end]);
    }

    fn deinit(self: *TestFile) void {
        self.allocator.free(self.xml_text);
        self.allocator.free(self.cases);
        for (self.all_lines) |line| self.allocator.free(line);
        self.allocator.free(self.all_lines);
    }
};

fn readTestFile(allocator: std.mem.Allocator, path: []const u8) !TestFile {
    const data = try std.fs.cwd().readFileAlloc(allocator, path, 1024 * 1024);
    defer allocator.free(data);

    // Split into lines, skip comments
    var all_lines = std.ArrayList([]u8){};
    var it = std.mem.splitScalar(u8, data, '\n');
    while (it.next()) |line_raw| {
        const line = std.mem.trimRight(u8, line_raw, "\r");
        // Skip comment lines
        if (line.len > 0 and line[0] == '#') continue;
        try all_lines.append(allocator, try allocator.dupe(u8, line));
    }

    // First section (up to first blank line) is XML
    var xml_buf = std.ArrayList(u8){};
    var line_idx: usize = 0;
    while (line_idx < all_lines.items.len) : (line_idx += 1) {
        if (all_lines.items[line_idx].len == 0) {
            line_idx += 1;
            break;
        }
        try xml_buf.appendSlice(allocator, all_lines.items[line_idx]);
        try xml_buf.append(allocator, '\n');
    }

    // Remaining sections: alternate data / expected, separated by blank lines
    var cases = std.ArrayList(TestCase){};
    while (line_idx < all_lines.items.len) {
        // Data section
        const data_start = line_idx;
        while (line_idx < all_lines.items.len and all_lines.items[line_idx].len > 0) : (line_idx += 1) {}
        const data_end = line_idx;
        line_idx += 1; // skip blank

        // Expected section
        const exp_start = line_idx;
        while (line_idx < all_lines.items.len and all_lines.items[line_idx].len > 0) : (line_idx += 1) {}
        const exp_end = line_idx;
        line_idx += 1; // skip blank

        if (exp_start < exp_end) {
            try cases.append(allocator, .{
                .data_start = data_start,
                .data_end = data_end,
                .expected_start = exp_start,
                .expected_end = exp_end,
            });
        }
    }

    return .{
        .allocator = allocator,
        .xml_text = try xml_buf.toOwnedSlice(allocator),
        .cases = try cases.toOwnedSlice(allocator),
        .all_lines = try all_lines.toOwnedSlice(allocator),
    };
}

/// Compare output against expected, handling failure cases
fn compareOutput(actual: ?*output_mod.Output, expected: *const ExpectedOutput, had_error: bool) !void {
    if (expected.is_failure) {
        try testing.expect(had_error);
        return;
    }

    try testing.expect(!had_error);
    const exp_out = expected.output orelse return error.InvalidData;
    const act_out = actual orelse return error.InvalidData;

    try testing.expectEqual(exp_out.num_dims, act_out.num_dims);
    try testing.expectEqual(exp_out.num_rows, act_out.num_rows);

    for (0..exp_out.num_dims) |d| {
        const exp_type = exp_out.dimensions[d].dim_type;
        const act_type = act_out.dimensions[d].dim_type;
        try testing.expectEqual(exp_type, act_type);

        for (0..exp_out.num_rows) |r| {
            switch (exp_type) {
                .Float64 => {
                    const exp_val = exp_out.dimensions[d].float64_data.?[r];
                    const act_val = act_out.dimensions[d].float64_data.?[r];
                    if (exp_val != act_val) {
                        std.debug.print("Mismatch at dim={d}, row={d}: expected {d}, got {d}\n", .{ d, r, exp_val, act_val });
                        return error.TestUnexpectedResult;
                    }
                },
                .Raw => {
                    const exp_raw = exp_out.dimensions[d].raw_data.?[r];
                    const act_raw = act_out.dimensions[d].raw_data.?[r];
                    if (!std.mem.eql(u8, exp_raw.ptr, act_raw.ptr)) {
                        std.debug.print("Raw mismatch at dim={d}, row={d}\n", .{ d, r });
                        return error.TestUnexpectedResult;
                    }
                },
                .None => {},
            }
        }
    }
}

/// Run a single decoder test file
fn runDecoderTestFile(allocator: std.mem.Allocator, filename: []const u8) !void {
    var path_buf: [256]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buf, "test/data/decoders/{s}", .{filename}) catch return error.InvalidData;

    var tf = try readTestFile(allocator, path);
    defer tf.deinit();

    // Parse XML
    var doc = xml_mod.parseString(allocator, tf.xml_text) catch |err| {
        std.debug.print("XML parse error for {s}: {}\n", .{ filename, err });
        return err;
    };
    defer doc.deinit();

    // Compile
    var comp = compiler_mod.Compiler.init(allocator);
    defer comp.deinit();
    var result = comp.compile(doc) catch |err| {
        std.debug.print("Compile error for {s}: {}\n", .{ filename, err });
        return err;
    };
    defer result.deinit(allocator);

    // Create decoder
    const vs_usize = try allocator.alloc(usize, result.vs.len);
    defer allocator.free(vs_usize);
    for (result.vs, 0..) |v, i| vs_usize[i] = @intCast(v);

    var decoder = try decoder_mod.Decoder.init(
        allocator,
        result.bytecode,
        vs_usize,
        result.num_registers,
        result.initial_registers,
    );
    defer decoder.deinit();

    // Create machine
    var machine = try decoder_mod.DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    // Run each test case
    for (tf.cases, 0..) |tc, case_idx| {
        const data_lines = tf.getLines(tc.data_start, tc.data_end);
        const exp_lines = tf.getLines(tc.expected_start, tc.expected_end);

        const hex_data = try parseHexData(allocator, data_lines);
        defer allocator.free(hex_data);

        var expected = try parseExpected(allocator, exp_lines);
        defer expected.deinit();

        // Prep and run
        machine.prep(hex_data);
        var had_error = false;
        machine.run() catch {
            had_error = true;
        };

        // Compare
        compareOutput(if (machine.output) |*o| o else null, &expected, had_error) catch |err| {
            std.debug.print("FAIL: {s} case {d}\n", .{ filename, case_idx });
            return err;
        };
    }
}

// ---- Individual test functions for each decoder test file ----

test "decoder: simple" {
    try runDecoderTestFile(testing.allocator, "simple");
}

test "decoder: th16-little" {
    try runDecoderTestFile(testing.allocator, "th16-little");
}

test "decoder: th16-big" {
    try runDecoderTestFile(testing.allocator, "th16-big");
}

test "decoder: fibonacci" {
    try runDecoderTestFile(testing.allocator, "fibonacci");
}

test "decoder: big-expr-1" {
    try runDecoderTestFile(testing.allocator, "big-expr-1");
}

test "decoder: big-expr-2" {
    try runDecoderTestFile(testing.allocator, "big-expr-2");
}

test "decoder: old-message" {
    try runDecoderTestFile(testing.allocator, "old-message");
}

test "decoder: new-message" {
    try runDecoderTestFile(testing.allocator, "new-message");
}

test "decoder: bit-decoder-1" {
    try runDecoderTestFile(testing.allocator, "bit-decoder-1");
}

test "decoder: bit-decoder-2" {
    try runDecoderTestFile(testing.allocator, "bit-decoder-2");
}

test "decoder: read-no-var" {
    try runDecoderTestFile(testing.allocator, "read-no-var");
}

test "decoder: read-assert" {
    try runDecoderTestFile(testing.allocator, "read-assert");
}

test "decoder: seek" {
    try runDecoderTestFile(testing.allocator, "seek");
}

test "decoder: equal signature" {
    const allocator = testing.allocator;

    const xml1 =
        "<decoder>" ++
        " <read var='v0' bits='64' type='uint' endian='little'/>" ++
        " <loop var='v0'>" ++
        "  <set var='foo' value='{$v0}'/>" ++
        "  <read var='v1' bits='32' type='float' endian='little'/>" ++
        "  <sample/>" ++
        " </loop>" ++
        "</decoder>";
    const xml2 =
        "<decoder>" ++
        " <read var='v0' bits='64' type='uint' endian='little'/>" ++
        " <loop var='v0'>" ++
        "  <set var='bar' value='{$v0}'/>" ++
        "  <read var='v1' bits='32' type='float' endian='little'/>" ++
        "  <sample/>" ++
        " </loop>" ++
        "</decoder>";
    const xml3 =
        "<decoder>" ++
        " <read var='v0' bits='64' type='uint' endian='little'/>" ++
        " <loop var='v0'>" ++
        "  <set var='foo' value='{$v1}'/>" ++
        "  <read var='v1' bits='32' type='float' endian='little'/>" ++
        "  <sample/>" ++
        " </loop>" ++
        "</decoder>";

    var doc1 = try xml_mod.parseString(allocator, xml1);
    defer doc1.deinit();
    var doc2 = try xml_mod.parseString(allocator, xml2);
    defer doc2.deinit();
    var doc3 = try xml_mod.parseString(allocator, xml3);
    defer doc3.deinit();

    var c1 = compiler_mod.Compiler.init(allocator);
    defer c1.deinit();
    var r1 = try c1.compile(doc1);
    defer r1.deinit(allocator);

    var c2 = compiler_mod.Compiler.init(allocator);
    defer c2.deinit();
    var r2 = try c2.compile(doc2);
    defer r2.deinit(allocator);

    var c3 = compiler_mod.Compiler.init(allocator);
    defer c3.deinit();
    var r3 = try c3.compile(doc3);
    defer r3.deinit(allocator);

    const vs1 = try allocator.alloc(usize, r1.vs.len);
    defer allocator.free(vs1);
    for (r1.vs, 0..) |v, i| vs1[i] = @intCast(v);
    var d1 = try decoder_mod.Decoder.init(allocator, r1.bytecode, vs1, r1.num_registers, r1.initial_registers);
    defer d1.deinit();

    const vs2 = try allocator.alloc(usize, r2.vs.len);
    defer allocator.free(vs2);
    for (r2.vs, 0..) |v, i| vs2[i] = @intCast(v);
    var d2 = try decoder_mod.Decoder.init(allocator, r2.bytecode, vs2, r2.num_registers, r2.initial_registers);
    defer d2.deinit();

    const vs3 = try allocator.alloc(usize, r3.vs.len);
    defer allocator.free(vs3);
    for (r3.vs, 0..) |v, i| vs3[i] = @intCast(v);
    var d3 = try decoder_mod.Decoder.init(allocator, r3.bytecode, vs3, r3.num_registers, r3.initial_registers);
    defer d3.deinit();

    // d1 and d2 should have same signature (same structure, different var names)
    try testing.expectEqual(d1.signature(), d2.signature());
    // d1 and d3 should differ (different expression: $v0 vs $v1)
    try testing.expect(d1.signature() != d3.signature());

    try testing.expect(d1.isEqual(&d2));
    try testing.expect(!d1.isEqual(&d3));
}
