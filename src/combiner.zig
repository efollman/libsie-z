// Combiner - remaps output dimensions from one layout to another
// Replaces sie_combiner.h / combiner.c
//
// Maps input dimension indices to output dimension indices.
// Used when multiple decoders produce data that needs to be
// combined into a single output with a unified dimension layout.

const std = @import("std");
const output_mod = @import("output.zig");
const Output = output_mod.Output;
const OutputType = output_mod.OutputType;

/// Sentinel for unmapped dimensions
const UNMAPPED: usize = std.math.maxInt(usize);

/// Combines multiple decoder outputs into a single output layout
pub const Combiner = struct {
    allocator: std.mem.Allocator,
    num_dims: usize,
    map: []usize,
    output: ?Output = null,

    /// Create a new combiner with num_dims output dimensions
    pub fn init(allocator: std.mem.Allocator, num_dims: usize) !Combiner {
        const map = try allocator.alloc(usize, num_dims);
        @memset(map, UNMAPPED);
        return .{
            .allocator = allocator,
            .num_dims = num_dims,
            .map = map,
        };
    }

    /// Clean up combiner
    pub fn deinit(self: *Combiner) void {
        if (self.output) |*out| out.deinit();
        self.allocator.free(self.map);
    }

    /// Map input dimension in_dim to output dimension out_dim
    pub fn addMapping(self: *Combiner, in_dim: usize, out_dim: usize) void {
        if (out_dim < self.num_dims) {
            self.map[out_dim] = in_dim;
        }
    }

    /// Combine input into the output layout, returning the remapped output
    pub fn combine(self: *Combiner, input: *const Output) !*Output {
        // Create or reuse output
        if (self.output == null) {
            self.output = try Output.init(self.allocator, self.num_dims);
        }
        var out = &self.output.?;

        out.num_rows = input.num_rows;
        out.block = input.block;
        out.scan_offset = input.scan_offset;

        for (0..self.num_dims) |v| {
            const in_v = self.map[v];
            if (in_v == UNMAPPED or in_v >= input.num_dims) continue;

            const src = &input.dimensions[in_v];
            const dst = &out.dimensions[v];

            // Copy type
            dst.dim_type = src.dim_type;

            switch (src.dim_type) {
                .Float64 => {
                    if (src.float64_data) |src_data| {
                        // Ensure output is big enough
                        if (dst.guts.capacity < input.num_rows) {
                            try out.resize(v, input.num_rows);
                        }
                        const copy_len = @min(src_data.len, input.num_rows);
                        if (dst.float64_data) |dst_data| {
                            @memcpy(dst_data[0..copy_len], src_data[0..copy_len]);
                        }
                    }
                },
                .Raw => {
                    if (src.raw_data) |src_data| {
                        if (dst.guts.capacity < input.num_rows) {
                            try out.resize(v, input.num_rows);
                        }
                        const copy_len = @min(src_data.len, input.num_rows);
                        if (dst.raw_data) |dst_data| {
                            @memcpy(dst_data[0..copy_len], src_data[0..copy_len]);
                            // Clear owned flag - combiner borrows raw ptrs from decoder output
                            for (dst_data[0..copy_len]) |*r| {
                                r.owned = false;
                            }
                        }
                    }
                },
                .None => {},
            }
        }

        return out;
    }
};

// ─── Tests ──────────────────────────────────────────────────────

test "combiner basic mapping" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Input has 3 dims, we want to remap to 2 dims
    var input = try Output.init(allocator, 3);
    defer input.deinit();

    input.setType(0, .Float64);
    input.setType(1, .Float64);
    input.setType(2, .Float64);
    try input.resize(0, 4);
    try input.resize(1, 4);
    try input.resize(2, 4);
    input.num_rows = 2;

    input.dimensions[0].float64_data.?[0] = 10.0;
    input.dimensions[0].float64_data.?[1] = 20.0;
    input.dimensions[1].float64_data.?[0] = 30.0;
    input.dimensions[1].float64_data.?[1] = 40.0;
    input.dimensions[2].float64_data.?[0] = 50.0;
    input.dimensions[2].float64_data.?[1] = 60.0;

    // Combiner maps: out[0] = in[2], out[1] = in[0]
    var combiner = try Combiner.init(allocator, 2);
    defer combiner.deinit();
    combiner.addMapping(2, 0);
    combiner.addMapping(0, 1);

    const result = try combiner.combine(&input);

    try std.testing.expectEqual(@as(usize, 2), result.num_dims);
    try std.testing.expectEqual(@as(usize, 2), result.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 50.0), result.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 60.0), result.dimensions[0].float64_data.?[1], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 10.0), result.dimensions[1].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 20.0), result.dimensions[1].float64_data.?[1], 0.001);
}

test "combiner unmapped dimensions" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var input = try Output.init(allocator, 2);
    defer input.deinit();
    input.setType(0, .Float64);
    try input.resize(0, 4);
    input.num_rows = 1;
    input.dimensions[0].float64_data.?[0] = 99.0;

    // 3 output dims, only map dim 1
    var combiner = try Combiner.init(allocator, 3);
    defer combiner.deinit();
    combiner.addMapping(0, 1);

    const result = try combiner.combine(&input);
    try std.testing.expectEqual(@as(usize, 3), result.num_dims);
    // Dim 0 and 2 are unmapped, dim 1 has data
    try std.testing.expectEqual(OutputType.None, result.dimensions[0].dim_type);
    try std.testing.expectEqual(OutputType.Float64, result.dimensions[1].dim_type);
    try std.testing.expectEqual(OutputType.None, result.dimensions[2].dim_type);
}

test "combiner reuse output" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var input = try Output.init(allocator, 1);
    defer input.deinit();
    input.setType(0, .Float64);
    try input.resize(0, 4);
    input.num_rows = 1;
    input.dimensions[0].float64_data.?[0] = 1.0;

    var combiner = try Combiner.init(allocator, 1);
    defer combiner.deinit();
    combiner.addMapping(0, 0);

    // First combine
    const r1 = try combiner.combine(&input);
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), r1.dimensions[0].float64_data.?[0], 0.001);

    // Update input and combine again (reuses output)
    input.dimensions[0].float64_data.?[0] = 2.0;
    const r2 = try combiner.combine(&input);
    try std.testing.expectApproxEqAbs(@as(f64, 2.0), r2.dimensions[0].float64_data.?[0], 0.001);
}
