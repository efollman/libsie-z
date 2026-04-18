// Output tests — ports of t_output.c
// Tests: trim (float64+raw), accessors, compare, copy (deep clone)

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const Output = libsie.output.Output;
const OutputType = libsie.output.OutputType;

/// Create a sample output matching the C test's sample_output()
/// 2 dims: dim0=Float64, dim1=Raw, 5 rows each
fn sampleOutput() !Output {
    var out = try Output.init(testing.allocator, 2);
    out.setType(0, .Float64);
    out.setType(1, .Raw);
    try out.grow(0);
    try out.grow(1);

    // Set float64 values: 1000, 2000, 3000, 4000, 5000
    if (out.dimensions[0].float64_data) |data| {
        data[0] = 1000.0;
        data[1] = 2000.0;
        data[2] = 3000.0;
        data[3] = 4000.0;
        data[4] = 5000.0;
    }

    // Set raw values: "ab", "ra", "ca", "da", "bra"
    try out.setRaw(1, 0, "ab");
    try out.setRaw(1, 1, "ra");
    try out.setRaw(1, 2, "ca");
    try out.setRaw(1, 3, "da");
    try out.setRaw(1, 4, "bra");

    out.num_rows = 5;
    return out;
}

test "output: trim float64 and raw from start" {
    // Port of test_output_trim — first half: trim(0, 2) keeps first 2 rows
    var out = try sampleOutput();
    defer out.deinit();

    try testing.expectEqual(@as(usize, 5), out.num_rows);

    out.trim(0, 2);

    try testing.expectEqual(@as(usize, 2), out.num_rows);
    try testing.expectEqual(@as(f64, 1000.0), out.getFloat64(0, 0).?);
    try testing.expectEqual(@as(f64, 2000.0), out.getFloat64(0, 1).?);

    const r0 = out.getRaw(1, 0).?;
    try testing.expectEqualStrings("ab", r0.ptr);
    const r1 = out.getRaw(1, 1).?;
    try testing.expectEqualStrings("ra", r1.ptr);
}

test "output: trim float64 and raw with offset" {
    // Port of test_output_trim — second half: trim(1, 2) keeps rows 1..2
    var out = try sampleOutput();
    defer out.deinit();

    out.trim(1, 2);

    try testing.expectEqual(@as(usize, 2), out.num_rows);
    try testing.expectEqual(@as(f64, 2000.0), out.getFloat64(0, 0).?);
    try testing.expectEqual(@as(f64, 3000.0), out.getFloat64(0, 1).?);

    const r0 = out.getRaw(1, 0).?;
    try testing.expectEqualStrings("ra", r0.ptr);
    const r1 = out.getRaw(1, 1).?;
    try testing.expectEqualStrings("ca", r1.ptr);
}

test "output: accessors" {
    // Port of test_output_accessors
    var out = try sampleOutput();
    defer out.deinit();

    try testing.expectEqual(@as(usize, 2), out.num_dims);
    try testing.expectEqual(@as(usize, 5), out.num_rows);
    try testing.expectEqual(OutputType.Float64, out.getDimensionType(0).?);
    try testing.expectEqual(OutputType.Raw, out.getDimensionType(1).?);

    // Float64 data accessible
    try testing.expect(out.getFloat64(0, 0) != null);
    // Raw data accessible
    try testing.expect(out.getRaw(1, 0) != null);

    // Out-of-bounds dimension returns null/None
    try testing.expect(out.getDimensionType(2) == null);
    try testing.expect(out.getFloat64(2, 0) == null);
    try testing.expect(out.getRaw(2, 0) == null);
}

test "output: compare equal" {
    // Port of test_output_compare
    var out1 = try sampleOutput();
    defer out1.deinit();
    var out2 = try sampleOutput();
    defer out2.deinit();

    try testing.expect(out1.compare(&out2));

    // Modify one value — should no longer be equal
    out1.dimensions[0].float64_data.?[0] = 42.0;
    try testing.expect(!out1.compare(&out2));
}

test "output: deep copy preserves independence" {
    // Port of test_output_copy — clone then modify original, verify independent
    var out1 = try sampleOutput();
    defer out1.deinit();

    // Deep copy
    var out2 = try Output.init(testing.allocator, out1.num_dims);
    defer out2.deinit();

    // Copy dimension types and data
    for (0..out1.num_dims) |d| {
        out2.setType(d, out1.dimensions[d].dim_type);
        switch (out1.dimensions[d].dim_type) {
            .Float64 => {
                if (out1.dimensions[d].float64_data) |src| {
                    try out2.resize(d, src.len);
                    @memcpy(out2.dimensions[d].float64_data.?[0..out1.num_rows], src[0..out1.num_rows]);
                }
            },
            .Raw => {
                if (out1.dimensions[d].raw_data) |src| {
                    try out2.resize(d, src.len);
                    for (0..out1.num_rows) |r| {
                        try out2.setRaw(d, r, src[r].ptr);
                    }
                }
            },
            .None => {},
        }
    }
    out2.num_rows = out1.num_rows;

    // Should be equal
    try testing.expect(out1.compare(&out2));

    // Modify original — copy should be unaffected
    out1.dimensions[0].float64_data.?[0] = 42.0;
    try testing.expect(!out1.compare(&out2));
}
