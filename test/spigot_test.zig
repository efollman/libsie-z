// Spigot streaming tests
// Based on t_spigot.c

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const Spigot = libsie.Spigot;
const Output = libsie.Output;

test "spigot: init and basic state" {
    var spigot = Spigot.init(testing.allocator, 100);
    defer spigot.deinit();

    try testing.expectEqual(@as(u64, 0), spigot.tell());
    try testing.expect(!spigot.isDone());
    try testing.expectEqual(@as(u64, 100), spigot.remaining());
}

test "spigot: scan limit" {
    var spigot = Spigot.init(testing.allocator, 100);
    defer spigot.deinit();

    spigot.setScanLimit(50);
    // Without an impl, get() returns null
    try testing.expect(spigot.get() == null);
}

test "spigot: seek to end" {
    var spigot = Spigot.init(testing.allocator, 100);
    defer spigot.deinit();

    try spigot.seekTo(libsie.advanced.spigot.SEEK_END);
    try testing.expect(spigot.isDone());
}

test "spigot: output create trim" {
    var out = try Output.init(testing.allocator, 2);
    defer out.deinit();

    out.setType(0, .Float64);
    out.setType(1, .Float64);
    try out.resize(0, 100);
    try out.resize(1, 100);

    // Populate some rows
    if (out.dimensions[0].float64_data) |data| {
        for (0..50) |i| data[i] = @floatFromInt(i);
    }
    if (out.dimensions[1].float64_data) |data| {
        for (0..50) |i| data[i] = @as(f64, @floatFromInt(i)) * 10.0;
    }
    out.num_rows = 50;

    // Trim to rows 10-20
    out.trim(10, 20);
    // After trim, first row should be what was at index 10
    const first_val = out.float64(0, 0);
    try testing.expect(first_val != null);
}
