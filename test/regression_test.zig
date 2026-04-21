// Regression tests — port of t_regression.c
// test_double_merge_corruption: verifies XML merge produces correct output
// for CAN raw test file, and re-opening doesn't corrupt the merge.

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;

const can_file = "test/data/can_raw_test-v-1-5-0-129-build-1218.sie";

test "regression: CAN raw file opens and has channels" {
    // Basic structural test for the CAN raw test file
    var sf = try SieFile.open(testing.allocator, can_file);
    defer sf.deinit();

    const channels = sf.channels();
    try testing.expect(channels.len > 0);

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    try testing.expectEqualStrings("raw_can1_cx23r_ff0027", ch.name);

    const dims = ch.dimensions();
    try testing.expectEqual(@as(usize, 2), dims.len);
    try testing.expectEqual(@as(u32, 2), dims[0].decoder_id);
    try testing.expectEqual(@as(usize, 0), dims[0].decoder_version);
    try testing.expectEqual(@as(u32, 2), dims[1].decoder_id);
    try testing.expectEqual(@as(usize, 1), dims[1].decoder_version);
}

test "regression: CAN raw channel tags" {
    var sf = try SieFile.open(testing.allocator, can_file);
    defer sf.deinit();
    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;

    if (ch.findTag("Description")) |desc| {
        try testing.expectEqualStrings("Raw CAN messages", desc.string().?);
    }
    if (ch.findTag("data_type")) |dt| {
        try testing.expectEqualStrings("message_can", dt.string().?);
    }
}

test "regression: CAN raw dimension transform" {
    var sf = try SieFile.open(testing.allocator, can_file);
    defer sf.deinit();
    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    const dims = ch.dimensions();

    try testing.expect(dims[0].has_linear_xform);
    try testing.expectApproxEqAbs(@as(f64, 2.5e-08), dims[0].xform_scale, 1e-15);
    try testing.expectApproxEqAbs(@as(f64, 0.0), dims[0].xform_offset, 1e-15);
    try testing.expect(!dims[1].has_linear_xform);
}

test "regression: CAN raw file can be opened twice" {
    var sf1 = try SieFile.open(testing.allocator, can_file);
    defer sf1.deinit();
    const ch1 = sf1.findChannel(1) orelse return error.TestUnexpectedResult;

    var sf2 = try SieFile.open(testing.allocator, can_file);
    defer sf2.deinit();
    const ch2 = sf2.findChannel(1) orelse return error.TestUnexpectedResult;

    try testing.expectEqualStrings(ch1.name, ch2.name);
    try testing.expectEqual(ch1.dimensions().len, ch2.dimensions().len);
}
