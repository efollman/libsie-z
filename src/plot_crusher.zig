// Plot crusher - data reduction for plotting
// Replaces sie_plot_crusher.h
//
// Reduces data to a manageable number of scans by tracking min/max pairs.
// Output has 2x dimensions (min, max for each input dimension).
// When output is full, halves by picking min-of-mins / max-of-maxes.

const std = @import("std");
const output_mod = @import("output.zig");

pub const PlotCrusher = struct {
    allocator: std.mem.Allocator,
    ideal_scans: usize,
    max_scans: usize,
    per_scan: usize,
    cur: usize,
    crushed: ?output_mod.Output,

    pub fn init(allocator: std.mem.Allocator, ideal_scans: usize) PlotCrusher {
        return .{
            .allocator = allocator,
            .ideal_scans = ideal_scans,
            .max_scans = ideal_scans * 2,
            .per_scan = 1,
            .cur = 0,
            .crushed = null,
        };
    }

    pub fn deinit(self: *PlotCrusher) void {
        if (self.crushed) |*c| {
            c.deinit();
        }
    }

    /// Process one block of input data. Returns true if data was consumed.
    pub fn work(self: *PlotCrusher, input: *const output_mod.Output) !bool {
        // Initialize crushed output on first call
        if (self.crushed == null) {
            self.crushed = try output_mod.Output.init(self.allocator, input.num_dims * 2);
            var c = &self.crushed.?;
            for (0..input.num_dims * 2) |v| {
                c.setType(v, .Float64);
                try c.resize(v, self.max_scans);
            }
        }

        var c = &self.crushed.?;

        for (0..input.num_rows) |i| {
            const oi = c.num_rows;
            for (0..input.num_dims) |v| {
                const ov = v * 2;
                const value = input.dimensions[v].float64_data.?[i];
                if (self.cur == 0) {
                    // First value in this scan — init both min and max
                    c.dimensions[ov].float64_data.?[oi] = value;
                    c.dimensions[ov + 1].float64_data.?[oi] = value;
                } else {
                    // Update min/max
                    const cur_min = c.dimensions[ov].float64_data.?[oi];
                    const cur_max = c.dimensions[ov + 1].float64_data.?[oi];
                    if (value < cur_min) c.dimensions[ov].float64_data.?[oi] = value;
                    if (value > cur_max) c.dimensions[ov + 1].float64_data.?[oi] = value;
                }
            }

            self.cur += 1;
            if (self.cur == self.per_scan) {
                // Commit this scan
                c.num_rows += 1;
                self.cur = 0;
            }

            if (c.num_rows == self.max_scans) {
                self.crush();
                self.per_scan *= 2;
            }
        }

        return true;
    }

    /// Finalize — close the last partial scan if any.
    pub fn finalize(self: *PlotCrusher) void {
        if (self.crushed != null and self.cur > 0) {
            var c = &self.crushed.?;
            c.num_rows += 1;
        }
    }

    /// Get the crushed output.
    pub fn getOutput(self: *const PlotCrusher) ?*const output_mod.Output {
        if (self.crushed != null) return &self.crushed.?;
        return null;
    }

    /// Internal: halve the data by picking min-of-mins / max-of-maxes.
    fn crush(self: *PlotCrusher) void {
        var c = &self.crushed.?;
        for (0..self.ideal_scans) |i| {
            var v: usize = 0;
            while (v < c.num_dims) : (v += 2) {
                // Min dimension: pick smaller of the two
                const a_min = c.dimensions[v].float64_data.?[i * 2];
                const b_min = c.dimensions[v].float64_data.?[i * 2 + 1];
                c.dimensions[v].float64_data.?[i] = @min(a_min, b_min);

                // Max dimension: pick larger of the two
                const a_max = c.dimensions[v + 1].float64_data.?[i * 2];
                const b_max = c.dimensions[v + 1].float64_data.?[i * 2 + 1];
                c.dimensions[v + 1].float64_data.?[i] = @max(a_max, b_max);
            }
        }
        // Shrink to ideal_scans
        c.num_rows = self.ideal_scans;
    }
};

const testing = std.testing;

test "plot_crusher basic reduction" {
    var pc = PlotCrusher.init(testing.allocator, 4);
    defer pc.deinit();

    // Create an input with 1 dimension, 3 rows
    var input = try output_mod.Output.init(testing.allocator, 1);
    defer input.deinit();
    input.setType(0, .Float64);
    try input.resize(0, 10);
    // Values: 5.0, 2.0, 8.0
    input.dimensions[0].float64_data.?[0] = 5.0;
    input.dimensions[0].float64_data.?[1] = 2.0;
    input.dimensions[0].float64_data.?[2] = 8.0;
    input.num_rows = 3;

    _ = try pc.work(&input);
    pc.finalize();

    const out = pc.getOutput().?;
    // Output should have 2 dimensions (min, max)
    try testing.expectEqual(@as(usize, 2), out.num_dims);
    // 3 rows (each input row = 1 scan since per_scan=1)
    try testing.expectEqual(@as(usize, 3), out.num_rows);

    // Each scan should have min==max (single value per scan)
    try testing.expectApproxEqAbs(@as(f64, 5.0), out.dimensions[0].float64_data.?[0], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 5.0), out.dimensions[1].float64_data.?[0], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 2.0), out.dimensions[0].float64_data.?[1], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 2.0), out.dimensions[1].float64_data.?[1], 0.001);
}

test "plot_crusher crush when full" {
    // ideal=2, max=4. Feed 4 values, should trigger crush → 2 scans, then per_scan=2
    var pc = PlotCrusher.init(testing.allocator, 2);
    defer pc.deinit();

    var input = try output_mod.Output.init(testing.allocator, 1);
    defer input.deinit();
    input.setType(0, .Float64);
    try input.resize(0, 10);
    // 4 values: 10, 20, 30, 5
    input.dimensions[0].float64_data.?[0] = 10.0;
    input.dimensions[0].float64_data.?[1] = 20.0;
    input.dimensions[0].float64_data.?[2] = 30.0;
    input.dimensions[0].float64_data.?[3] = 5.0;
    input.num_rows = 4;

    _ = try pc.work(&input);

    const out = pc.getOutput().?;
    // After crush: 2 scans, per_scan=2
    // Scan 0: min(10,20)=10, max(10,20)=20
    // Scan 1: min(30,5)=5, max(30,5)=30
    try testing.expectEqual(@as(usize, 2), out.num_rows);
    try testing.expectApproxEqAbs(@as(f64, 10.0), out.dimensions[0].float64_data.?[0], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 20.0), out.dimensions[1].float64_data.?[0], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 5.0), out.dimensions[0].float64_data.?[1], 0.001);
    try testing.expectApproxEqAbs(@as(f64, 30.0), out.dimensions[1].float64_data.?[1], 0.001);
}

test "plot_crusher multiple blocks" {
    var pc = PlotCrusher.init(testing.allocator, 4);
    defer pc.deinit();

    // First block: 2 values
    var input1 = try output_mod.Output.init(testing.allocator, 1);
    defer input1.deinit();
    input1.setType(0, .Float64);
    try input1.resize(0, 10);
    input1.dimensions[0].float64_data.?[0] = 1.0;
    input1.dimensions[0].float64_data.?[1] = 2.0;
    input1.num_rows = 2;
    _ = try pc.work(&input1);

    // Second block: 2 values
    var input2 = try output_mod.Output.init(testing.allocator, 1);
    defer input2.deinit();
    input2.setType(0, .Float64);
    try input2.resize(0, 10);
    input2.dimensions[0].float64_data.?[0] = 3.0;
    input2.dimensions[0].float64_data.?[1] = 4.0;
    input2.num_rows = 2;
    _ = try pc.work(&input2);

    pc.finalize();

    const out = pc.getOutput().?;
    try testing.expectEqual(@as(usize, 4), out.num_rows);
}
