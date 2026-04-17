// Data transformations - applies per-dimension transforms to output data
// Replaces sie_transform.h / transform.c
//
// Three transform types:
//   None   - pass-through (no change)
//   Linear - scale * value + offset
//   Map    - lookup table: value → map[floor(value)]

const std = @import("std");
const output_mod = @import("output.zig");
const Output = output_mod.Output;

/// Transform type per dimension
pub const TransformType = enum {
    None,
    Linear,
    Map,
};

/// Parameters for a single dimension transform
pub const TransformParams = union(TransformType) {
    None: void,
    Linear: struct {
        scale: f64,
        offset: f64,
    },
    Map: struct {
        values: std.ArrayList(f64),
    },
};

/// Applies per-dimension transforms to output data in-place
pub const Transform = struct {
    allocator: std.mem.Allocator,
    num_dims: usize,
    xforms: []TransformParams,

    /// Create a new transform set for num_dims dimensions (all None)
    pub fn init(allocator: std.mem.Allocator, num_dims: usize) !Transform {
        const xforms = try allocator.alloc(TransformParams, num_dims);
        for (xforms) |*x| {
            x.* = .None;
        }
        return .{
            .allocator = allocator,
            .num_dims = num_dims,
            .xforms = xforms,
        };
    }

    /// Clean up transforms
    pub fn deinit(self: *Transform) void {
        for (self.xforms) |*x| {
            switch (x.*) {
                .Map => |*m| m.values.deinit(self.allocator),
                else => {},
            }
        }
        self.allocator.free(self.xforms);
    }

    /// Set dimension to pass-through
    pub fn setNone(self: *Transform, dim: usize) void {
        if (dim >= self.num_dims) return;
        switch (self.xforms[dim]) {
            .Map => |*m| m.values.deinit(self.allocator),
            else => {},
        }
        self.xforms[dim] = .None;
    }

    /// Set linear transform: result = scale * value + offset
    pub fn setLinear(self: *Transform, dim: usize, scale: f64, offset: f64) void {
        if (dim >= self.num_dims) return;
        switch (self.xforms[dim]) {
            .Map => |*m| m.values.deinit(self.allocator),
            else => {},
        }
        self.xforms[dim] = .{ .Linear = .{ .scale = scale, .offset = offset } };
    }

    /// Set map transform from a lookup table slice
    pub fn setMap(self: *Transform, dim: usize, values: []const f64) !void {
        if (dim >= self.num_dims) return;
        switch (self.xforms[dim]) {
            .Map => |*m| m.values.deinit(self.allocator),
            else => {},
        }
        var list: std.ArrayList(f64) = .{};
        try list.appendSlice(self.allocator, values);
        self.xforms[dim] = .{ .Map = .{ .values = list } };
    }

    /// Apply all transforms to the output data in-place
    pub fn apply(self: *const Transform, output: *Output) void {
        const count = @min(self.num_dims, output.num_dims);
        for (0..count) |v| {
            switch (self.xforms[v]) {
                .None => {},
                .Linear => |lin| {
                    if (output.dimensions[v].float64_data) |data| {
                        for (0..output.num_rows) |s| {
                            if (s < data.len) {
                                data[s] = data[s] * lin.scale + lin.offset;
                            }
                        }
                    }
                },
                .Map => |m| {
                    if (output.dimensions[v].float64_data) |data| {
                        for (0..output.num_rows) |s| {
                            if (s < data.len) {
                                const idx = @as(usize, @intFromFloat(@max(0, data[s])));
                                if (idx < m.values.items.len) {
                                    data[s] = m.values.items[idx];
                                }
                            }
                        }
                    }
                },
            }
        }
    }
};

// ─── Tests ──────────────────────────────────────────────────────

test "transform linear" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();
    output.setType(0, .Float64);
    try output.resize(0, 4);
    output.num_rows = 3;
    output.dimensions[0].float64_data.?[0] = 1.0;
    output.dimensions[0].float64_data.?[1] = 2.0;
    output.dimensions[0].float64_data.?[2] = 3.0;

    var xform = try Transform.init(allocator, 1);
    defer xform.deinit();
    xform.setLinear(0, 2.0, 10.0); // result = 2*x + 10

    xform.apply(&output);

    try std.testing.expectApproxEqAbs(@as(f64, 12.0), output.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 14.0), output.dimensions[0].float64_data.?[1], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 16.0), output.dimensions[0].float64_data.?[2], 0.001);
}

test "transform map lookup" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();
    output.setType(0, .Float64);
    try output.resize(0, 4);
    output.num_rows = 3;
    output.dimensions[0].float64_data.?[0] = 0.0; // maps to 100
    output.dimensions[0].float64_data.?[1] = 2.0; // maps to 300
    output.dimensions[0].float64_data.?[2] = 1.0; // maps to 200

    const lookup = [_]f64{ 100.0, 200.0, 300.0 };
    var xform = try Transform.init(allocator, 1);
    defer xform.deinit();
    try xform.setMap(0, &lookup);

    xform.apply(&output);

    try std.testing.expectApproxEqAbs(@as(f64, 100.0), output.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 300.0), output.dimensions[0].float64_data.?[1], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 200.0), output.dimensions[0].float64_data.?[2], 0.001);
}

test "transform mixed types" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 3);
    defer output.deinit();
    for (0..3) |i| {
        output.setType(i, .Float64);
        try output.resize(i, 4);
    }
    output.num_rows = 2;
    output.dimensions[0].float64_data.?[0] = 5.0;
    output.dimensions[0].float64_data.?[1] = 10.0;
    output.dimensions[1].float64_data.?[0] = 1.0;
    output.dimensions[1].float64_data.?[1] = 2.0;
    output.dimensions[2].float64_data.?[0] = 0.0;
    output.dimensions[2].float64_data.?[1] = 1.0;

    var xform = try Transform.init(allocator, 3);
    defer xform.deinit();
    // Dim 0: None (pass-through)
    // Dim 1: Linear (3*x + 1)
    xform.setLinear(1, 3.0, 1.0);
    // Dim 2: Map
    const lookup = [_]f64{ 50.0, 60.0 };
    try xform.setMap(2, &lookup);

    xform.apply(&output);

    try std.testing.expectApproxEqAbs(@as(f64, 5.0), output.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 4.0), output.dimensions[1].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 7.0), output.dimensions[1].float64_data.?[1], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 50.0), output.dimensions[2].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 60.0), output.dimensions[2].float64_data.?[1], 0.001);
}

test "transform set none resets" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var xform = try Transform.init(allocator, 1);
    defer xform.deinit();

    xform.setLinear(0, 2.0, 0.0);
    try std.testing.expectEqual(TransformType.Linear, std.meta.activeTag(xform.xforms[0]));

    xform.setNone(0);
    try std.testing.expectEqual(TransformType.None, std.meta.activeTag(xform.xforms[0]));
}
