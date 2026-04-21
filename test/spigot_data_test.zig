// Spigot data tests — ports of t_spigot.c
// Tests seek/tell with real SIE data, disable transforms, etc.

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;
const File = libsie.File;
const Block = libsie.advanced.block;
const GroupSpigot = libsie.GroupSpigot;

const min_sie = "test/data/sie_min_timhis_a_19EFAA61.sie";
const seek_sie = "test/data/sie_seek_test.sie";

test "spigot: group spigot seek and tell" {
    // Port of test_group_spigot_seek_tell
    // Reads 7 blocks forward, then seeks backward verifying data matches.
    var file = File.init(testing.allocator, min_sie);
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    const n: usize = 7;

    // First pass: read all blocks forward, save copies
    var spigot = GroupSpigot.init(testing.allocator, &file, 0);
    defer spigot.deinit();

    var saved: [7][]u8 = undefined;
    var saved_count: usize = 0;

    for (0..n) |i| {
        const payload = try spigot.get() orelse return error.TestUnexpectedResult;
        saved[i] = try testing.allocator.dupe(u8, payload);
        saved_count += 1;
    }
    defer for (saved[0..saved_count]) |s| testing.allocator.free(s);

    // Should be at end
    try testing.expect(try spigot.get() == null);

    // Seek back to start
    try testing.expectEqual(@as(u64, 0), spigot.seek(0));
    try testing.expectEqual(@as(u64, 0), spigot.tell());

    // Seek to end
    const end = spigot.seek(std.math.maxInt(u64));
    try testing.expectEqual(@as(u64, n), end);
    try testing.expectEqual(@as(u64, n), spigot.tell());
    try testing.expect(try spigot.get() == null);

    // Seek backward, verify data matches
    var i: usize = n;
    while (i > 0) {
        i -= 1;
        _ = spigot.seek(@intCast(i));
        const payload = try spigot.get() orelse return error.TestUnexpectedResult;
        try testing.expectEqual(saved[i].len, payload.len);
        try testing.expectEqualSlices(u8, saved[i], payload);
    }
}

test "spigot: channel spigot seek and tell" {
    // Port of test_channel_spigot_seek_tell
    // Uses seek test file with 82 blocks
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    const n: usize = spigot.numBlocks();

    // Seek to start
    try testing.expectEqual(@as(u64, 0), spigot.tell());
    try testing.expectEqual(@as(u64, 0), spigot.seek(0));

    // Seek to end
    const end_pos = spigot.seek(std.math.maxInt(u64));
    try testing.expectEqual(@as(u64, n), end_pos);
    try testing.expectEqual(@as(u64, n), spigot.tell());
    try testing.expect(try spigot.get() == null);

    // Seek to block 10, verify first value
    _ = spigot.seek(10);
    try testing.expectEqual(@as(u64, 10), spigot.tell());
    const out10 = try spigot.get() orelse return error.TestUnexpectedResult;
    try testing.expectEqual(@as(usize, 10), out10.block);
    // v[0] first row should be 32 (= 10 * 3.2 scaled by transform)
    const val10 = out10.float64(0, 0) orelse return error.TestUnexpectedResult;
    try testing.expectApproxEqAbs(@as(f64, 32), val10, 0.01);

    // Seek to last block
    _ = spigot.seek(@intCast(n - 1));
    try testing.expectEqual(@as(u64, n - 1), spigot.tell());
    const out_last = try spigot.get() orelse return error.TestUnexpectedResult;
    try testing.expectEqual(n - 1, out_last.block);
    const val_last = out_last.float64(0, 0) orelse return error.TestUnexpectedResult;
    try testing.expectApproxEqAbs(@as(f64, 259.2), val_last, 0.01);
}

test "spigot: channel spigot disable transforms" {
    // Port of test_channel_spigot_disable_transforms
    // With transforms disabled, time dim should have raw integer values (0, 1, 2, ...)
    // With transforms enabled, time dim should have scaled values (0, 0.001, 0.002, ...)
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;

    // Read with transforms disabled
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();
    spigot.disableTransforms(true);

    const raw_out = try spigot.get() orelse return error.TestUnexpectedResult;
    // With transforms disabled, v[0] should be raw integer counters: 0, 1, 2, ...
    for (0..@min(raw_out.num_rows, 20)) |i| {
        const val = raw_out.float64(0, i) orelse continue;
        try testing.expectEqual(@as(f64, @floatFromInt(i)), val);
    }

    // Re-read with transforms enabled
    _ = spigot.seek(0);
    spigot.disableTransforms(false);
    const xform_out = try spigot.get() orelse return error.TestUnexpectedResult;

    // With transforms enabled, v[0] should be: i * scale
    // The scale from the seek test file — each integer step * 0.001 (or whatever the transform is)
    // Verify values are NOT raw integers (they should be scaled)
    for (0..@min(xform_out.num_rows, 20)) |i| {
        const val = xform_out.float64(0, i) orelse continue;
        // floor(val * 1000 + 0.5) should equal i (C test checks this)
        const rounded: i64 = @intFromFloat(@floor(val * 1000 + 0.5));
        try testing.expectEqual(@as(i64, @intCast(i)), rounded);
    }
}

test "spigot: channel spigot read all blocks" {
    // Read all blocks from min_timhis channel 1, verify non-empty output
    var sf = try SieFile.open(testing.allocator, min_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var total_rows: usize = 0;
    var block_count: usize = 0;

    while (try spigot.get()) |out| {
        try testing.expect(out.num_rows > 0);
        try testing.expect(out.num_dims >= 2);
        total_rows += out.num_rows;
        block_count += 1;
    }

    try testing.expectEqual(@as(usize, 1), block_count);
    try testing.expectEqual(@as(usize, 520), total_rows);
}

test "spigot: channel spigot data values match expected" {
    // Verify first and last few values of min_timhis channel 1
    var sf = try SieFile.open(testing.allocator, min_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    const out = try spigot.get() orelse return error.TestUnexpectedResult;
    try testing.expectEqual(@as(usize, 520), out.num_rows);

    // v[0] = time: 0, 0.02, 0.04, ...
    try testing.expectApproxEqAbs(@as(f64, 0.0), out.float64(0, 0).?, 1e-10);
    try testing.expectApproxEqAbs(@as(f64, 0.02), out.float64(0, 1).?, 1e-10);
    try testing.expectApproxEqAbs(@as(f64, 0.04), out.float64(0, 2).?, 1e-10);

    // v[1] = amplitude: -1000, -920, -840, ...
    try testing.expectApproxEqAbs(@as(f64, -1000.0), out.float64(1, 0).?, 0.01);
    try testing.expectApproxEqAbs(@as(f64, -920.0), out.float64(1, 1).?, 0.01);
    try testing.expectApproxEqAbs(@as(f64, -840.0), out.float64(1, 2).?, 0.01);

    // Last row: v[0] = 10.38, v[1] = 519.986
    try testing.expectApproxEqAbs(@as(f64, 10.38), out.float64(0, 519).?, 0.01);
    try testing.expectApproxEqAbs(@as(f64, 519.986), out.float64(1, 519).?, 0.01);
}

test "spigot: both test channels produce identical structure" {
    // Channels 1 and 2 are from test 0 and test 1, both should have 520 rows
    var sf = try SieFile.open(testing.allocator, min_sie);
    defer sf.deinit();

    for ([_]u32{ 1, 2 }) |ch_id| {
        const ch = sf.findChannel(ch_id) orelse return error.TestUnexpectedResult;
        var spigot = try sf.attachSpigot(ch);
        defer spigot.deinit();

        var total: usize = 0;
        while (try spigot.get()) |out| {
            total += out.num_rows;
            try testing.expectEqual(@as(usize, 2), out.num_dims);
        }
        try testing.expectEqual(@as(usize, 520), total);
    }
}

test "spigot: lower bound basic" {
    // Port of test_channel_spigot_lower_bound from t_spigot.c
    // lower_bound finds the first scan >= target value
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    // Searching for -1 finds block=0, scan=0 (first element >= -1)
    const r1 = try spigot.lowerBound(0, -1) orelse return error.TestUnexpectedResult;
    try testing.expectEqual(@as(usize, 0), r1.block);
    try testing.expectEqual(@as(usize, 0), r1.scan);

    // Searching for 4000 (beyond all data) returns null
    try testing.expect(try spigot.lowerBound(0, 4000) == null);
}

test "spigot: lower bound exact values" {
    // Port of test_channel_spigot_lower_bound loop tests
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    // Loop i=0,26,52,...,286 — verify exact match
    var i: i32 = 0;
    while (i < 300) : (i += 26) {
        const d: f64 = @floatFromInt(i);
        const result = try spigot.lowerBound(0, d);
        if (d < 262.4) {
            const r = result orelse {
                std.debug.print("didn't find time value {d}\n", .{d});
                return error.TestUnexpectedResult;
            };
            _ = spigot.seek(r.block);
            const output = try spigot.get() orelse return error.TestUnexpectedResult;
            const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
            try testing.expectEqual(d, val);
        }
    }
}

test "spigot: lower bound fine-grained" {
    // Port of the fine-grained loop: d=-1 to 264 step 0.125 (skips 7→260)
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var d: f64 = -1;
    while (d < 264) : (d += 0.125) {
        const result = try spigot.lowerBound(0, d);
        if (d < 262.4) {
            const r = result orelse {
                std.debug.print("didn't find time value {d}\n", .{d});
                return error.TestUnexpectedResult;
            };
            if (d >= 0) {
                _ = spigot.seek(r.block);
                const output = try spigot.get() orelse return error.TestUnexpectedResult;
                const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
                try testing.expectEqual(d, val);
            }
        }
        if (d == 7) d = 260;
    }
}

test "spigot: lower bound just-below values" {
    // Port of just-below loop: d = i*3.2 - 0.000001
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var i: usize = 0;
    var d: f64 = 0;
    while (d < 264) : ({
        i += 1;
        d = @as(f64, @floatFromInt(i)) * 3.2 - 0.000001;
    }) {
        const result = try spigot.lowerBound(0, d);
        if (d <= 262.399) {
            const r = result orelse {
                std.debug.print("didn't find time value {d}\n", .{d});
                return error.TestUnexpectedResult;
            };
            _ = spigot.seek(r.block);
            const output = try spigot.get() orelse return error.TestUnexpectedResult;
            const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
            try testing.expectApproxEqAbs(d, val, 0.00001);
        }
    }
}

test "spigot: lower bound edge cases" {
    // Port of edge case tests around 1.999
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    // lower_bound(1.9989) → value at scan == 1.999
    {
        const r = try spigot.lowerBound(0, 1.9989) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 1.999), val);
    }

    // lower_bound(1.999) → value == 1.999
    {
        const r = try spigot.lowerBound(0, 1.999) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 1.999), val);
    }

    // lower_bound(1.9991) → value == 2.0
    {
        const r = try spigot.lowerBound(0, 1.9991) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 2.0), val);
    }
}

test "spigot: upper bound basic" {
    // Port of test_channel_spigot_upper_bound from t_spigot.c
    // upper_bound finds the last scan <= target value
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    // Searching for 4000 finds last element: block=81, scan=3199
    const r1 = try spigot.upperBound(0, 4000) orelse return error.TestUnexpectedResult;
    try testing.expectEqual(@as(usize, 81), r1.block);
    // Verify this is the last block
    const end_pos = spigot.seek(std.math.maxInt(u64));
    try testing.expectEqual(@as(u64, r1.block + 1), end_pos);
    try testing.expectEqual(@as(usize, 3199), r1.scan);

    // Searching for -1 returns null (no element <= -1)
    try testing.expect(try spigot.upperBound(0, -1) == null);
}

test "spigot: upper bound exact values" {
    // Port of upper_bound loop tests
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var i: i32 = 0;
    while (i < 300) : (i += 26) {
        const d: f64 = @floatFromInt(i);
        const result = try spigot.upperBound(0, d);
        const r = result orelse {
            std.debug.print("didn't find time value {d}\n", .{d});
            return error.TestUnexpectedResult;
        };
        if (d < 263) {
            _ = spigot.seek(r.block);
            const output = try spigot.get() orelse return error.TestUnexpectedResult;
            const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
            try testing.expectEqual(d, val);
        }
    }
}

test "spigot: upper bound fine-grained" {
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var d: f64 = -1;
    while (d < 264) : (d += 0.125) {
        const result = try spigot.upperBound(0, d);
        if (d >= 0) {
            const r = result orelse {
                std.debug.print("didn't find time value {d}\n", .{d});
                return error.TestUnexpectedResult;
            };
            if (d < 262.4) {
                _ = spigot.seek(r.block);
                const output = try spigot.get() orelse return error.TestUnexpectedResult;
                const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
                try testing.expectEqual(d, val);
            }
        }
        if (d == 7) d = 260;
    }
}

test "spigot: upper bound just-above values" {
    // Port of just-above loop: d = i*3.2 + 0.000001
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    var i: usize = 0;
    var d: f64 = 0;
    while (d < 264) : ({
        i += 1;
        d = @as(f64, @floatFromInt(i)) * 3.2 + 0.000001;
    }) {
        const result = try spigot.upperBound(0, d);
        const r = result orelse {
            std.debug.print("didn't find time value {d}\n", .{d});
            return error.TestUnexpectedResult;
        };
        if (d < 262.4) {
            _ = spigot.seek(r.block);
            const output = try spigot.get() orelse return error.TestUnexpectedResult;
            const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
            try testing.expectApproxEqAbs(d, val, 0.00001);
        }
    }
}

test "spigot: upper bound edge cases" {
    // Port of edge case tests around 1.999
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    // upper_bound(1.9989) → value at scan == 1.998
    {
        const r = try spigot.upperBound(0, 1.9989) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 1.998), val);
    }

    // upper_bound(1.999) → value == 1.999
    {
        const r = try spigot.upperBound(0, 1.999) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 1.999), val);
    }

    // upper_bound(1.9991) → value == 1.999
    {
        const r = try spigot.upperBound(0, 1.9991) orelse return error.TestUnexpectedResult;
        _ = spigot.seek(r.block);
        const output = try spigot.get() orelse return error.TestUnexpectedResult;
        const val = output.float64(0, r.scan) orelse return error.TestUnexpectedResult;
        try testing.expectEqual(@as(f64, 1.999), val);
    }
}

test "spigot: transform output manually" {
    // Port of test_channel_spigot_transform_output
    // Reads raw data with transforms disabled, then calls transformOutput manually
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot = try sf.attachSpigot(ch);
    defer spigot.deinit();

    spigot.disableTransforms(true);
    const output = try spigot.get() orelse return error.TestUnexpectedResult;

    // Raw values should be integers: 0, 1, 2, ...
    for (0..@min(output.num_rows, 20)) |i| {
        const val = output.float64(0, i) orelse continue;
        try testing.expectEqual(@as(f64, @floatFromInt(i)), val);
    }

    // Apply transforms manually
    spigot.transformOutput(output);

    // Now values should be scaled: floor(val * 1000 + 0.5) == i
    for (0..@min(output.num_rows, 20)) |i| {
        const val = output.float64(0, i) orelse continue;
        const rounded: i64 = @intFromFloat(@floor(val * 1000 + 0.5));
        try testing.expectEqual(@as(i64, @intCast(i)), rounded);
    }
}

test "spigot: clear output" {
    // Port of test_spigot_clear_output
    // Two spigots on same channel, every other iteration clear one's output.
    // Both should still produce matching data.
    var sf = try SieFile.open(testing.allocator, seek_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    var spigot1 = try sf.attachSpigot(ch);
    defer spigot1.deinit();
    var spigot2 = try sf.attachSpigot(ch);
    defer spigot2.deinit();

    var i: usize = 0;
    while (try spigot1.get()) |out1| {
        const out2 = try spigot2.get() orelse return error.TestUnexpectedResult;
        try testing.expect(out1.compare(out2));
        i += 1;
        if (i % 2 == 1) {
            spigot2.clearOutput();
        }
    }
    try testing.expect(i > 0);
}

test "spigot: scan limit" {
    // Port of test_scan_limit from t_file.c
    // Without limit, blocks have >= 12 rows. With limit of 12, exactly 12.
    var sf = try SieFile.open(testing.allocator, min_sie);
    defer sf.deinit();

    const ch = sf.findChannel(1) orelse return error.TestUnexpectedResult;

    // Without limit
    {
        var spigot = try sf.attachSpigot(ch);
        defer spigot.deinit();
        while (try spigot.get()) |out| {
            try testing.expect(out.num_rows >= 12);
        }
    }

    // With limit of 12
    {
        var spigot = try sf.attachSpigot(ch);
        defer spigot.deinit();
        spigot.setScanLimit(12);
        while (try spigot.get()) |out| {
            try testing.expectEqual(@as(usize, 12), out.num_rows);
        }
    }
}
