// Histogram tests — ports of t_histogram.c
// Tests histogram reconstruction from SIE channel data.

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;
const Histogram = libsie.advanced.histogram.Histogram;

const comprehensive2_sie = "test/data/sie_comprehensive2_VBM_20050908.sie";

test "histogram: open 1D histogram from channel" {
    // Port of test_histogram_open — channel 49 is a 1D histogram
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(49) orelse return error.TestUnexpectedResult;
    var hist = try Histogram.fromChannel(testing.allocator, &sf, ch);
    defer hist.deinit();

    try testing.expect(hist.getNumDims() > 0);
    try testing.expect(hist.total_size > 0);
}

test "histogram: non-histogram channel fails" {
    // Port of test_histogram_open_timhis — channel 32 is a timhis, not a histogram
    // A timhis channel has 2 dimensions (even number), so fromChannel should return InvalidData
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(32) orelse return error.TestUnexpectedResult;
    const result = Histogram.fromChannel(testing.allocator, &sf, ch);
    try testing.expectError(error.InvalidData, result);
}

test "histogram: 1D basic accessors" {
    // Port of test_histogram_basic_accessors
    // Channel 49: 1D histogram with 12 bins
    // Bounds from dim=0 should match the spigot output v[1] (lower) and v[2] (upper)
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(49) orelse return error.TestUnexpectedResult;
    var hist = try Histogram.fromChannel(testing.allocator, &sf, ch);
    defer hist.deinit();

    try testing.expectEqual(@as(usize, 1), hist.getNumDims());
    try testing.expectEqual(@as(usize, 12), hist.getNumBins(0));

    // Get bounds and verify against raw spigot data
    var lower: [12]f64 = undefined;
    var upper: [12]f64 = undefined;
    hist.getBinBounds(0, &lower, &upper);

    // Also read spigot data to cross-reference
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    const output = try spigot.get() orelse return error.TestUnexpectedResult;
    // Histogram output: v[0]=value, v[1]=lower, v[2]=upper
    try testing.expectEqual(@as(usize, 3), output.num_dims);

    // The first scan's lower/upper bounds should match one of our histogram bounds
    const first_lower = output.float64(1, 0) orelse return error.TestUnexpectedResult;
    const first_upper = output.float64(2, 0) orelse return error.TestUnexpectedResult;

    // Verify these bounds exist in our histogram
    const bound_idx = hist.findBound(0, first_lower, first_upper);
    try testing.expect(bound_idx != std.math.maxInt(usize));
}

test "histogram: 2D basic accessors" {
    // Port of test_histogram_basic_accessors_2d
    // Channel 54: 2D histogram with 9×13 bins
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(54) orelse return error.TestUnexpectedResult;
    var hist = try Histogram.fromChannel(testing.allocator, &sf, ch);
    defer hist.deinit();

    try testing.expectEqual(@as(usize, 2), hist.getNumDims());
    try testing.expectEqual(@as(usize, 9), hist.getNumBins(0));
    try testing.expectEqual(@as(usize, 13), hist.getNumBins(1));

    // Verify bounds from spigot data
    // 2D histogram: v[0]=value, v[1]=dim0_lower, v[2]=dim0_upper, v[3]=dim1_lower, v[4]=dim1_upper
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    const output = try spigot.get() orelse return error.TestUnexpectedResult;
    try testing.expectEqual(@as(usize, 5), output.num_dims);

    // Dim 0 bounds: check first scan's bound matches something in our histogram
    var d0_lower: [9]f64 = undefined;
    var d0_upper: [9]f64 = undefined;
    hist.getBinBounds(0, &d0_lower, &d0_upper);

    // All dim0 bounds at stride 13 in the spigot output
    // v[1][i*13] = dim0 lower for i-th dim0 bin
    for (0..9) |i| {
        const scan = i * 13;
        if (scan < output.num_rows) {
            const sp_lower = output.float64(1, scan) orelse continue;
            const sp_upper = output.float64(2, scan) orelse continue;
            try testing.expectApproxEqAbs(d0_lower[i], sp_lower, 1e-10);
            try testing.expectApproxEqAbs(d0_upper[i], sp_upper, 1e-10);
        }
    }

    // Dim 1 bounds: first 13 scans have all dim1 bounds
    var d1_lower: [13]f64 = undefined;
    var d1_upper: [13]f64 = undefined;
    hist.getBinBounds(1, &d1_lower, &d1_upper);

    for (0..13) |i| {
        if (i < output.num_rows) {
            const sp_lower = output.float64(3, i) orelse continue;
            const sp_upper = output.float64(4, i) orelse continue;
            try testing.expectApproxEqAbs(d1_lower[i], sp_lower, 1e-10);
            try testing.expectApproxEqAbs(d1_upper[i], sp_upper, 1e-10);
        }
    }
}

test "histogram: 1D bin values" {
    // Port of test_histogram_bins
    // Channel 49: getBin([5]) == 460.0, getNextNonzeroBin works
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(49) orelse return error.TestUnexpectedResult;
    var hist = try Histogram.fromChannel(testing.allocator, &sf, ch);
    defer hist.deinit();

    // getBin([5]) == 460.0
    try testing.expectApproxEqAbs(@as(f64, 460.0), hist.getBin(&[_]usize{5}), 0.001);

    // getNextNonzeroBin from 0 — should find first nonzero bin
    var start: usize = 0;
    var indices: [1]usize = undefined;
    const val1 = hist.getNextNonzeroBin(&start, &indices);
    try testing.expectApproxEqAbs(@as(f64, 460.0), val1, 0.001);
    try testing.expectEqual(@as(usize, 5), indices[0]);

    // Next nonzero bin should return 0.0 (no more nonzero bins)
    const val2 = hist.getNextNonzeroBin(&start, &indices);
    try testing.expectApproxEqAbs(@as(f64, 0.0), val2, 0.001);
}

test "histogram: 2D bin values" {
    // Port of test_histogram_bins_2d
    // Channel 54: getBin([0,2]) == 10.0
    var sf = try SieFile.open(testing.allocator, comprehensive2_sie);
    defer sf.deinit();

    const ch = sf.findChannel(54) orelse return error.TestUnexpectedResult;
    var hist = try Histogram.fromChannel(testing.allocator, &sf, ch);
    defer hist.deinit();

    try testing.expectApproxEqAbs(@as(f64, 10.0), hist.getBin(&[_]usize{ 0, 2 }), 0.001);
}
