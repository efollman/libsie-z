// Data stream interface - provides sequential access to SIE data
// Replaces sie_spigot.h / spigot.c
//
// Spigot is a base class with virtual method dispatch for streaming
// data access. Concrete implementations (e.g., group spigot, combiner
// spigot) override the vtable methods.
//
// Supports scan limits, block-level seeking, binary search over
// sorted data, and transform pipeline integration.

const std = @import("std");
const output_mod = @import("output.zig");
const error_mod = @import("error.zig");

/// Sentinel value for seek-to-end
pub const SEEK_END: u64 = std.math.maxInt(u64);

/// Result of a binary search: block index and scan offset within block
pub const SearchResult = struct {
    block: u64,
    scan: u64,
};

/// Virtual method table for Spigot implementations
pub const VTable = struct {
    /// Prepare the spigot for reading (called once before first get)
    prepFn: ?*const fn (ctx: *anyopaque) void = null,

    /// Get the next output block (inner implementation)
    getInnerFn: ?*const fn (ctx: *anyopaque) ?*output_mod.Output = null,

    /// Seek to a block index. Returns the block count (SEEK_END for seek-to-end).
    seekFn: ?*const fn (ctx: *anyopaque, target: u64) u64 = null,

    /// Tell current block position
    tellFn: ?*const fn (ctx: *anyopaque) u64 = null,

    /// Check if stream is done
    doneFn: ?*const fn (ctx: *anyopaque) bool = null,

    /// Clear cached output
    clearOutputFn: ?*const fn (ctx: *anyopaque) void = null,

    /// Disable/enable transforms
    disableTransformsFn: ?*const fn (ctx: *anyopaque, disable: bool) void = null,

    /// Apply transforms to output
    transformOutputFn: ?*const fn (ctx: *anyopaque, output: *output_mod.Output) void = null,
};

/// Spigot - data streaming interface for reading channel data
pub const Spigot = struct {
    allocator: std.mem.Allocator,
    vtable: VTable = .{},
    impl: *anyopaque = undefined,
    prepped: bool = false,
    scans: u64 = 0,
    scans_limit: u64 = 0,
    position: u64 = 0,
    size: u64 = 0,

    /// Create a new spigot with default (no-op) vtable
    pub fn init(allocator: std.mem.Allocator, size: u64) Spigot {
        return Spigot{
            .allocator = allocator,
            .size = size,
        };
    }

    /// Clean up spigot
    pub fn deinit(self: *Spigot) void {
        _ = self;
    }

    /// Set implementation context and vtable
    pub fn setImpl(self: *Spigot, impl_ptr: *anyopaque, vtable: VTable) void {
        self.impl = impl_ptr;
        self.vtable = vtable;
    }

    /// Set scan limit (0 = unlimited)
    pub fn setScanLimit(self: *Spigot, limit: u64) void {
        self.scans_limit = limit;
    }

    /// Prepare the spigot (called automatically on first get)
    pub fn prep(self: *Spigot) void {
        if (self.vtable.prepFn) |f| f(self.impl);
    }

    /// Get next output block, respecting scan limits
    pub fn get(self: *Spigot) ?*output_mod.Output {
        if (!self.prepped) {
            self.prep();
            self.prepped = true;
        }
        if (self.scans_limit > 0 and self.scans >= self.scans_limit)
            return null;

        const output = self.getInner() orelse return null;

        // Apply scan limit trimming
        if (self.scans_limit > 0 and
            self.scans + output.num_rows > self.scans_limit)
        {
            const over = output.num_rows - (self.scans_limit - self.scans);
            const trim_to = output.num_rows - over;
            output.trim(0, trim_to);
        }
        self.scans += output.num_rows;
        return output;
    }

    /// Get inner output (dispatches to vtable)
    fn getInner(self: *Spigot) ?*output_mod.Output {
        if (self.vtable.getInnerFn) |f| return f(self.impl);
        return null;
    }

    /// Seek to block index. Returns block count.
    pub fn seek(self: *Spigot, target: u64) u64 {
        if (self.vtable.seekFn) |f| return f(self.impl, target);
        return SEEK_END;
    }

    /// Tell current block position
    pub fn tell(self: *const Spigot) u64 {
        if (self.vtable.tellFn) |f| return f(@constCast(self.impl));
        return self.position;
    }

    /// Check if stream is done
    pub fn isDone(self: *const Spigot) bool {
        if (self.vtable.doneFn) |f| return f(@constCast(self.impl));
        return self.position >= self.size;
    }

    /// Clear cached output
    pub fn clearOutput(self: *Spigot) void {
        if (self.vtable.clearOutputFn) |f| f(self.impl);
    }

    /// Seek to absolute position (convenience)
    pub fn seekTo(self: *Spigot, position: u64) !void {
        if (position > self.size and position != SEEK_END) {
            return error_mod.Error.FileSeekError;
        }
        _ = self.seek(position);
        self.position = position;
    }

    /// Seek relative to current position (convenience)
    pub fn seekBy(self: *Spigot, delta: i64) !void {
        const new_pos = if (delta < 0)
            self.position -| @as(u64, @intCast(-delta))
        else
            self.position +| @as(u64, @intCast(delta));

        if (new_pos > self.size) {
            return error_mod.Error.FileSeekError;
        }
        _ = self.seek(new_pos);
        self.position = new_pos;
    }

    /// Get remaining bytes
    pub fn remaining(self: *const Spigot) u64 {
        if (self.position >= self.size) return 0;
        return self.size - self.position;
    }

    /// Binary search for lower bound in sorted dimension
    pub fn lowerBound(self: *Spigot, dim: usize, value: f64) ?SearchResult {
        return self.binarySearch(dim, value, false);
    }

    /// Binary search for upper bound in sorted dimension
    pub fn upperBound(self: *Spigot, dim: usize, value: f64) ?SearchResult {
        return self.binarySearch(dim, value, true);
    }

    /// Internal binary search implementation
    fn binarySearch(self: *Spigot, dim: usize, value: f64, upper_bound: bool) ?SearchResult {
        const orig = self.tell();
        const num = self.seek(SEEK_END);
        if (num == 0 or num == SEEK_END) {
            _ = self.seek(orig);
            return null;
        }

        var low: i64 = 0;
        var high: i64 = @intCast(num - 1);
        var result: ?SearchResult = null;

        while (low <= high) {
            const mid: u64 = @intCast(@divFloor(high + low, 2));
            _ = self.seek(mid);
            const output = self.get() orelse break;

            if (output.num_rows == 0) break;
            if (dim >= output.num_dims) break;

            const data = output.dimensions[dim].float64_data orelse break;

            const pos = if (upper_bound)
                findValueUpper(data[0..output.num_rows], value)
            else
                findValueLower(data[0..output.num_rows], value);

            switch (pos) {
                .found => |scan| {
                    result = .{ .block = mid, .scan = scan };
                    break;
                },
                .after => {
                    low = @as(i64, @intCast(mid)) + 1;
                },
                .before => {
                    high = @as(i64, @intCast(mid)) - 1;
                },
            }
        }

        // Boundary cases when value is at edge of range
        if (result == null) {
            if (upper_bound) {
                if (high >= 0) {
                    const h: u64 = @intCast(high);
                    _ = self.seek(h);
                    if (self.get()) |output| {
                        if (output.num_rows > 0) {
                            result = .{ .block = h, .scan = output.num_rows - 1 };
                        }
                    }
                }
            } else {
                if (low < @as(i64, @intCast(num))) {
                    result = .{ .block = @intCast(low), .scan = 0 };
                }
            }
        }

        _ = self.seek(orig);
        return result;
    }
};

/// Search position result
const FindResult = union(enum) {
    found: u64,
    before: void,
    after: void,
};

/// Find lower bound position in sorted array
fn findValueLower(data: []const f64, value: f64) FindResult {
    if (data.len == 0) return .before;
    if (value <= data[0]) return .before;
    if (value > data[data.len - 1]) return .after;

    var low: usize = 0;
    var high: usize = data.len - 1;
    while (low <= high) {
        if (low > high) return .{ .found = low };
        const mid = (high + low) / 2;
        if (data[mid] < value) {
            low = mid + 1;
        } else {
            if (mid == 0) return .{ .found = 0 };
            high = mid - 1;
        }
    }
    return .{ .found = low };
}

/// Find upper bound position in sorted array
/// Returns the highest index where data[index] <= value
fn findValueUpper(data: []const f64, value: f64) FindResult {
    if (data.len == 0) return .before;
    if (value < data[0]) return .before;
    if (value >= data[data.len - 1]) return .after;

    var low: usize = 0;
    var high: usize = data.len - 1;
    while (low <= high) {
        const mid = (high + low) / 2;
        if (data[mid] > value) {
            if (mid == 0) return .{ .found = 0 };
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    // high is the last index where data[high] <= value
    return .{ .found = high + 1 };
}

test "spigot initialization" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var spigot = Spigot.init(allocator, 1000);
    defer spigot.deinit();

    try std.testing.expectEqual(spigot.tell(), 0);
    try std.testing.expectEqual(spigot.size, 1000);
    try std.testing.expectEqual(spigot.remaining(), 1000);
    try std.testing.expect(!spigot.isDone());
}

test "spigot seeking" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var spigot = Spigot.init(allocator, 1000);
    defer spigot.deinit();

    try spigot.seekTo(500);
    try std.testing.expectEqual(spigot.tell(), 500);
    try std.testing.expectEqual(spigot.remaining(), 500);

    try spigot.seekBy(100);
    try std.testing.expectEqual(spigot.tell(), 600);
    try std.testing.expectEqual(spigot.remaining(), 400);
}

test "spigot vtable dispatch" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const TestImpl = struct {
        prep_called: bool = false,
        done_val: bool = false,

        fn prepFn(ctx: *anyopaque) void {
            const self: *@This() = @ptrCast(@alignCast(ctx));
            self.prep_called = true;
        }

        fn doneFn(ctx: *anyopaque) bool {
            const self: *@This() = @ptrCast(@alignCast(ctx));
            return self.done_val;
        }
    };

    var impl = TestImpl{};
    var spigot = Spigot.init(allocator, 100);
    defer spigot.deinit();

    spigot.setImpl(@ptrCast(&impl), .{
        .prepFn = TestImpl.prepFn,
        .doneFn = TestImpl.doneFn,
    });

    // First get triggers prep
    _ = spigot.get();
    try std.testing.expect(impl.prep_called);

    // isDone delegates to vtable
    try std.testing.expect(!spigot.isDone());
    impl.done_val = true;
    try std.testing.expect(spigot.isDone());
}

test "spigot scan limit" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var spigot = Spigot.init(allocator, 100);
    defer spigot.deinit();

    spigot.setScanLimit(50);
    try std.testing.expectEqual(@as(u64, 50), spigot.scans_limit);

    // Without a getInner implementation, get returns null
    try std.testing.expect(spigot.get() == null);
}

test "find value lower" {
    const data = [_]f64{ 1.0, 2.0, 3.0, 4.0, 5.0 };

    // Value in range
    const r1 = findValueLower(&data, 2.5);
    switch (r1) {
        .found => |pos| try std.testing.expectEqual(@as(u64, 2), pos),
        else => return error.TestUnexpectedResult,
    }

    // Value before range
    const r2 = findValueLower(&data, 0.5);
    try std.testing.expect(r2 == .before);

    // Value after range
    const r3 = findValueLower(&data, 6.0);
    try std.testing.expect(r3 == .after);
}

test "find value upper" {
    const data = [_]f64{ 1.0, 2.0, 3.0, 4.0, 5.0 };

    // Value in range
    const r1 = findValueUpper(&data, 3.5);
    switch (r1) {
        .found => |pos| try std.testing.expectEqual(@as(u64, 3), pos),
        else => return error.TestUnexpectedResult,
    }

    // Value before range
    const r2 = findValueUpper(&data, 0.5);
    try std.testing.expect(r2 == .before);

    // Value at/after last element
    const r3 = findValueUpper(&data, 5.0);
    try std.testing.expect(r3 == .after);
}
