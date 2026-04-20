// Sifter pipeline tests — ports of t_sifter.c
// Tests: sifter_basic, sifter_partial

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const SieFile = libsie.SieFile;
const Writer = libsie.advanced.writer.Writer;
const Sifter = libsie.advanced.sifter.Sifter;

/// Writer callback that accumulates total bytes written.
fn accumulateFn(user: ?*anyopaque, data: []const u8) usize {
    const acc: *usize = @ptrCast(@alignCast(user.?));
    acc.* += data.len;
    return data.len;
}

test "sifter: add channel basic" {
    // Port of test_sifter_basic
    var sf = try SieFile.open(testing.allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer sf.deinit();

    var accumulator: usize = 0;
    var writer = Writer.init(testing.allocator, accumulateFn, @ptrCast(&accumulator));

    try writer.xmlHeader();

    var sifter = Sifter.init(testing.allocator, &writer);

    const channel = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    try sifter.addChannel(&sf, channel, 0, std.math.maxInt(u64));

    const nbytes = sifter.sifterTotalSize(&sf.file);

    // Finish writes data blocks, then deinit flushes remaining index
    try sifter.finish(&sf.file);
    sifter.deinit();
    writer.deinit();

    // totalSize prediction must match actual bytes written
    try testing.expectEqual(nbytes, accumulator);
    try testing.expect(nbytes > 0);
}

test "sifter: add channel partial range" {
    // Port of test_sifter_partial
    var sf = try SieFile.open(testing.allocator, "test/data/sie_seek_test.sie");
    defer sf.deinit();

    var accumulator: usize = 0;
    var writer = Writer.init(testing.allocator, accumulateFn, @ptrCast(&accumulator));

    try writer.xmlHeader();

    var sifter = Sifter.init(testing.allocator, &writer);

    const channel = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    try sifter.addChannel(&sf, channel, 42, 63);

    const nbytes = sifter.sifterTotalSize(&sf.file);

    try sifter.finish(&sf.file);
    sifter.deinit();
    writer.deinit();

    // totalSize prediction must match actual bytes written
    try testing.expectEqual(nbytes, accumulator);
    try testing.expect(nbytes > 0);
}

test "sifter: id mapping and remapping" {
    // Verify the core ID mapping and XML remapping work together
    var sf = try SieFile.open(testing.allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer sf.deinit();

    var accumulator: usize = 0;
    var writer = Writer.init(testing.allocator, accumulateFn, @ptrCast(&accumulator));
    defer writer.deinit();

    var sifter = Sifter.init(testing.allocator, &writer);
    defer sifter.deinit();

    // Before adding anything, no entries
    try testing.expectEqual(@as(usize, 0), sifter.totalEntries());

    const channel = sf.findChannel(1) orelse return error.TestUnexpectedResult;
    try sifter.addChannel(&sf, channel, 0, std.math.maxInt(u64));

    // After adding a channel, we should have entries for groups, channel, test, etc.
    try testing.expect(sifter.totalEntries() > 0);

    // The channel should now be mapped
    try testing.expect(sifter.findId(.Channel, 0, channel.id) != null);

    // Adding the same channel again should be a no-op
    const entries_before = sifter.totalEntries();
    try sifter.addChannel(&sf, channel, 0, std.math.maxInt(u64));
    try testing.expectEqual(entries_before, sifter.totalEntries());
}
