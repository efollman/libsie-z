// Histogram reconstruction
// Replaces sie_histogram.h

const std = @import("std");
const sie_file_mod = @import("sie_file.zig");
const channel_mod = @import("channel.zig");
const output_mod = @import("output.zig");

pub const Bound = struct {
    lower: f64,
    upper: f64,

    pub fn compare(a: Bound, b: Bound) std.math.Order {
        if (a.lower < b.lower) return .lt;
        if (a.lower > b.lower) return .gt;
        if (a.upper < b.upper) return .lt;
        if (a.upper > b.upper) return .gt;
        return .eq;
    }

    pub fn lessThan(_: void, a: Bound, b: Bound) bool {
        return compare(a, b) == .lt;
    }
};

pub const HistogramDim = struct {
    bounds: std.ArrayList(Bound),

    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) HistogramDim {
        return .{ .bounds = .{}, .allocator = allocator };
    }

    pub fn deinit(self: *HistogramDim) void {
        self.bounds.deinit(self.allocator);
    }

    pub fn numBins(self: *const HistogramDim) usize {
        return self.bounds.items.len;
    }
};

const NOT_FOUND: usize = std.math.maxInt(usize);

pub const Histogram = struct {
    allocator: std.mem.Allocator,
    dims: []HistogramDim,
    total_size: usize,
    bins: []f64,

    /// Create a histogram with the given number of dimensions.
    /// Dimensions start empty; use addBound to populate them,
    /// then call finalize to allocate the bin storage.
    pub fn init(allocator: std.mem.Allocator, num_dims: usize) !Histogram {
        const dims = try allocator.alloc(HistogramDim, num_dims);
        for (dims) |*d| {
            d.* = HistogramDim.init(allocator);
        }
        return .{
            .allocator = allocator,
            .dims = dims,
            .total_size = 0,
            .bins = &[_]f64{},
        };
    }

    /// Build a histogram from a channel's spigot data.
    /// The channel must have an odd number of dimensions >= 3:
    ///   v[0] = bin value, v[1]/v[2] = dim0 lower/upper, v[3]/v[4] = dim1 lower/upper, ...
    pub fn fromChannel(allocator: std.mem.Allocator, sf: *sie_file_mod.SieFile, ch: *const channel_mod.Channel) !Histogram {
        // First pass: determine dimensionality from first block
        var spigot = try sf.attachSpigot(ch);
        defer spigot.deinit();

        const first_output = try spigot.get() orelse return error.InvalidData;
        const num_vs = first_output.num_dims;
        if (num_vs < 3 or (num_vs % 2) != 1) return error.InvalidData;
        const num_dims = (num_vs - 1) / 2;

        var hist = try Histogram.init(allocator, num_dims);
        errdefer hist.deinit();

        // Collect bounds from first block
        try collectBounds(&hist, first_output, num_dims);

        // Continue collecting bounds from remaining blocks
        while (try spigot.get()) |output| {
            try collectBounds(&hist, output, num_dims);
        }

        // Finalize (allocate bin storage)
        try hist.finalize();

        // Second pass: load bin values
        spigot.reset();
        while (try spigot.get()) |output| {
            for (0..output.num_rows) |scan| {
                var indices_buf: [16]usize = undefined;
                const indices = indices_buf[0..num_dims];
                var valid = true;
                for (0..num_dims) |dim| {
                    const lower_data = output.dimensions[dim * 2 + 1].float64_data orelse {
                        valid = false;
                        break;
                    };
                    const upper_data = output.dimensions[dim * 2 + 2].float64_data orelse {
                        valid = false;
                        break;
                    };
                    const idx = hist.findBound(dim, lower_data[scan], upper_data[scan]);
                    if (idx == NOT_FOUND) {
                        valid = false;
                        break;
                    }
                    indices[dim] = idx;
                }
                if (valid) {
                    const value_data = output.dimensions[0].float64_data orelse continue;
                    hist.setBin(indices, value_data[scan]);
                }
            }
        }

        return hist;
    }

    fn collectBounds(hist: *Histogram, output: *const output_mod.Output, num_dims: usize) !void {
        for (0..output.num_rows) |scan| {
            for (0..num_dims) |dim| {
                const lower_data = output.dimensions[dim * 2 + 1].float64_data orelse continue;
                const upper_data = output.dimensions[dim * 2 + 2].float64_data orelse continue;
                try hist.addBound(dim, lower_data[scan], upper_data[scan]);
            }
        }
    }

    pub fn deinit(self: *Histogram) void {
        for (self.dims) |*d| {
            d.deinit();
        }
        self.allocator.free(self.dims);
        if (self.bins.len > 0) {
            self.allocator.free(self.bins);
        }
    }

    /// Add a bound to a dimension, maintaining sorted order and uniqueness.
    pub fn addBound(self: *Histogram, dim: usize, lower: f64, upper: f64) !void {
        const bound = Bound{ .lower = lower, .upper = upper };
        const bounds = &self.dims[dim].bounds;
        // Check if already exists
        if (binaryBoundSearch(bounds.items, bound) != NOT_FOUND) return;
        try bounds.append(self.allocator, bound);
        std.mem.sort(Bound, bounds.items, {}, Bound.lessThan);
    }

    /// Allocate bin storage after all bounds have been added.
    pub fn finalize(self: *Histogram) !void {
        if (self.bins.len > 0) {
            self.allocator.free(self.bins);
        }
        self.total_size = 1;
        for (self.dims) |*d| {
            if (d.numBins() == 0) {
                self.total_size = 0;
                break;
            }
            self.total_size *= d.numBins();
        }
        if (self.total_size > 0) {
            self.bins = try self.allocator.alloc(f64, self.total_size);
            @memset(self.bins, 0.0);
        } else {
            self.bins = &[_]f64{};
        }
    }

    /// Convert multi-dimensional indices to a flat index (row-major).
    pub fn flattenIndices(self: *const Histogram, indices: []const usize) usize {
        var result: usize = 0;
        var mul: usize = 1;
        var dim_i: usize = self.dims.len;
        while (dim_i > 0) {
            dim_i -= 1;
            result += indices[dim_i] * mul;
            mul *= self.dims[dim_i].numBins();
        }
        return result;
    }

    /// Convert a flat index to multi-dimensional indices.
    pub fn unflattenIndex(self: *const Histogram, index: usize, indices: []usize) void {
        var remaining = index;
        var dim_i: usize = self.dims.len;
        while (dim_i > 0) {
            dim_i -= 1;
            const num_bins = self.dims[dim_i].numBins();
            indices[dim_i] = remaining % num_bins;
            remaining /= num_bins;
        }
    }

    /// Find the bin index for a bound in a given dimension.
    pub fn findBound(self: *const Histogram, dim: usize, lower: f64, upper: f64) usize {
        const bound = Bound{ .lower = lower, .upper = upper };
        return binaryBoundSearch(self.dims[dim].bounds.items, bound);
    }

    /// Get number of dimensions.
    pub fn getNumDims(self: *const Histogram) usize {
        return self.dims.len;
    }

    /// Get number of bins in a dimension.
    pub fn getNumBins(self: *const Histogram, dim: usize) usize {
        if (dim >= self.dims.len) return 0;
        return self.dims[dim].numBins();
    }

    /// Get bin value by multi-dimensional indices.
    pub fn getBin(self: *const Histogram, indices: []const usize) f64 {
        if (self.bins.len == 0) return 0.0;
        const index = self.flattenIndices(indices);
        if (index >= self.total_size) return 0.0;
        return self.bins[index];
    }

    /// Set bin value by multi-dimensional indices.
    pub fn setBin(self: *Histogram, indices: []const usize, value: f64) void {
        const index = self.flattenIndices(indices);
        if (index < self.total_size) {
            self.bins[index] = value;
        }
    }

    /// Set bin value by bound lookup (lower/upper pairs per dimension).
    /// bounds should have 2 * num_dims entries: [lower0, upper0, lower1, upper1, ...]
    pub fn setBinByBounds(self: *Histogram, bound_pairs: []const f64, value: f64) !void {
        const num_dims = self.dims.len;
        var indices_buf: [16]usize = undefined;
        const indices = indices_buf[0..num_dims];
        for (0..num_dims) |dim_idx| {
            const lower = bound_pairs[dim_idx * 2];
            const upper = bound_pairs[dim_idx * 2 + 1];
            const idx = self.findBound(dim_idx, lower, upper);
            if (idx == NOT_FOUND) return error.InvalidData;
            indices[dim_idx] = idx;
        }
        self.setBin(indices, value);
    }

    /// Find the next nonzero bin starting from flat index *start.
    /// Returns the bin value and fills indices. Advances *start past
    /// the found bin. Returns 0.0 if no more nonzero bins.
    pub fn getNextNonzeroBin(self: *const Histogram, start: *usize, indices: []usize) f64 {
        while (start.* < self.total_size) {
            const bin = self.bins[start.*];
            if (bin != 0.0) {
                self.unflattenIndex(start.*, indices);
                start.* += 1;
                return bin;
            }
            start.* += 1;
        }
        return 0.0;
    }

    /// Get bounds (lower and upper arrays) for a dimension.
    pub fn getBinBounds(self: *const Histogram, dim: usize, lower: []f64, upper: []f64) void {
        if (dim >= self.dims.len) return;
        const bounds = self.dims[dim].bounds.items;
        for (bounds, 0..) |b, i| {
            if (i < lower.len) lower[i] = b.lower;
            if (i < upper.len) upper[i] = b.upper;
        }
    }
};

fn binaryBoundSearch(haystack: []const Bound, needle: Bound) usize {
    var low: usize = 0;
    var high: usize = haystack.len;
    while (low < high) {
        const mid = low + (high - low) / 2;
        const cmp = Bound.compare(haystack[mid], needle);
        if (cmp == .eq) return mid;
        if (cmp == .lt) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return NOT_FOUND;
}

const testing = std.testing;

test "histogram basic 1D" {
    var h = try Histogram.init(testing.allocator, 1);
    defer h.deinit();

    // Add bounds for 3 bins
    try h.addBound(0, 0.0, 10.0);
    try h.addBound(0, 10.0, 20.0);
    try h.addBound(0, 20.0, 30.0);
    try h.finalize();

    try testing.expectEqual(@as(usize, 1), h.getNumDims());
    try testing.expectEqual(@as(usize, 3), h.getNumBins(0));
    try testing.expectEqual(@as(usize, 3), h.total_size);

    // Set and get bins
    h.setBin(&[_]usize{0}, 5.0);
    h.setBin(&[_]usize{1}, 15.0);
    h.setBin(&[_]usize{2}, 25.0);

    try testing.expectApproxEqAbs(@as(f64, 5.0), h.getBin(&[_]usize{0}), 0.001);
    try testing.expectApproxEqAbs(@as(f64, 15.0), h.getBin(&[_]usize{1}), 0.001);
    try testing.expectApproxEqAbs(@as(f64, 25.0), h.getBin(&[_]usize{2}), 0.001);
}

test "histogram 2D flatten and unflatten" {
    var h = try Histogram.init(testing.allocator, 2);
    defer h.deinit();

    // 2x3 histogram
    try h.addBound(0, 0.0, 1.0);
    try h.addBound(0, 1.0, 2.0);
    try h.addBound(1, 0.0, 10.0);
    try h.addBound(1, 10.0, 20.0);
    try h.addBound(1, 20.0, 30.0);
    try h.finalize();

    try testing.expectEqual(@as(usize, 6), h.total_size);

    // Test flatten: [1, 2] in a 2x3 grid = 1*3 + 2 = 5
    try testing.expectEqual(@as(usize, 5), h.flattenIndices(&[_]usize{ 1, 2 }));

    // Test unflatten: 5 -> [1, 2]
    var indices: [2]usize = undefined;
    h.unflattenIndex(5, &indices);
    try testing.expectEqual(@as(usize, 1), indices[0]);
    try testing.expectEqual(@as(usize, 2), indices[1]);

    // Set by indices and verify
    h.setBin(&[_]usize{ 0, 1 }, 42.0);
    try testing.expectApproxEqAbs(@as(f64, 42.0), h.getBin(&[_]usize{ 0, 1 }), 0.001);
}

test "histogram bound uniqueness and sorting" {
    var h = try Histogram.init(testing.allocator, 1);
    defer h.deinit();

    // Add bounds out of order with duplicates
    try h.addBound(0, 20.0, 30.0);
    try h.addBound(0, 0.0, 10.0);
    try h.addBound(0, 10.0, 20.0);
    try h.addBound(0, 0.0, 10.0); // duplicate
    try h.finalize();

    try testing.expectEqual(@as(usize, 3), h.getNumBins(0));

    // Verify sorted order
    var lower: [3]f64 = undefined;
    var upper: [3]f64 = undefined;
    h.getBinBounds(0, &lower, &upper);
    try testing.expectApproxEqAbs(@as(f64, 0.0), lower[0], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 10.0), lower[1], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 20.0), lower[2], 0.001);
}

test "histogram setBinByBounds" {
    var h = try Histogram.init(testing.allocator, 2);
    defer h.deinit();

    try h.addBound(0, 0.0, 5.0);
    try h.addBound(0, 5.0, 10.0);
    try h.addBound(1, 0.0, 100.0);
    try h.addBound(1, 100.0, 200.0);
    try h.finalize();

    try h.setBinByBounds(&[_]f64{ 5.0, 10.0, 100.0, 200.0 }, 99.5);
    // That's indices [1, 1] in a 2x2 grid = flat index 3
    try testing.expectApproxEqAbs(@as(f64, 99.5), h.getBin(&[_]usize{ 1, 1 }), 0.001);
}

test "histogram getNextNonzeroBin" {
    var h = try Histogram.init(testing.allocator, 1);
    defer h.deinit();

    try h.addBound(0, 0.0, 10.0);
    try h.addBound(0, 10.0, 20.0);
    try h.addBound(0, 20.0, 30.0);
    try h.addBound(0, 30.0, 40.0);
    try h.finalize();

    h.setBin(&[_]usize{1}, 7.0);
    h.setBin(&[_]usize{3}, 9.0);

    var start: usize = 0;
    var indices: [1]usize = undefined;

    const val1 = h.getNextNonzeroBin(&start, &indices);
    try testing.expectApproxEqAbs(@as(f64, 7.0), val1, 0.001);
    try testing.expectEqual(@as(usize, 1), indices[0]);

    const val2 = h.getNextNonzeroBin(&start, &indices);
    try testing.expectApproxEqAbs(@as(f64, 9.0), val2, 0.001);
    try testing.expectEqual(@as(usize, 3), indices[0]);

    const val3 = h.getNextNonzeroBin(&start, &indices);
    try testing.expectApproxEqAbs(@as(f64, 0.0), val3, 0.001);
}
