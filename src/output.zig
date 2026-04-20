// Output data structure - represents a block of data output
// Replaces sie_output.h / output.c
//
// A 2D table with typed columns (dimensions). Each column holds either
// float64 values or raw binary data. Includes capacity management
// (resize, grow, trim) and owned data buffers.

const std = @import("std");

/// Output value types
pub const OutputType = enum(u8) {
    None = 0,
    Float64 = 1,
    Raw = 2,
};

/// Raw binary data for a single value
pub const RawData = struct {
    ptr: []const u8 = &.{},
    size: u32 = 0,
    owned: bool = false,
};

/// Internal bookkeeping per dimension
const DimGuts = struct {
    capacity: usize = 0,
    element_size: usize = 0,
};

/// A single dimension in the output (column of data)
pub const OutputDimension = struct {
    dim_type: OutputType = .None,
    float64_data: ?[]f64 = null,
    raw_data: ?[]RawData = null,
    guts: DimGuts = .{},
    owned: bool = false,
};

/// Output data block - represents one complete data output
pub const Output = struct {
    allocator: std.mem.Allocator,
    block: usize = 0,
    scan_offset: usize = 0,
    num_dims: usize,
    num_rows: usize = 0,
    dimensions: []OutputDimension,

    /// Create a new output block with num_dims columns
    pub fn init(allocator: std.mem.Allocator, num_dims: usize) !Output {
        const dims = try allocator.alloc(OutputDimension, num_dims);
        for (dims) |*d| {
            d.* = .{};
        }
        return Output{
            .allocator = allocator,
            .num_dims = num_dims,
            .dimensions = dims,
        };
    }

    /// Clean up output and all owned data
    pub fn deinit(self: *Output) void {
        for (self.dimensions) |*dim| {
            if (dim.owned) {
                if (dim.float64_data) |data| self.allocator.free(data);
                if (dim.raw_data) |data| {
                    for (data) |*r| {
                        if (r.owned) self.allocator.free(@constCast(r.ptr));
                    }
                    self.allocator.free(data);
                }
            }
        }
        self.allocator.free(self.dimensions);
    }

    /// Set the type for a dimension
    pub fn setType(self: *Output, dim: usize, dim_type: OutputType) void {
        if (dim >= self.num_dims) return;
        self.dimensions[dim].dim_type = dim_type;
        self.dimensions[dim].guts.element_size = switch (dim_type) {
            .Float64 => @sizeOf(f64),
            .Raw => @sizeOf(RawData),
            .None => 0,
        };
    }

    /// Resize a dimension to hold max_size elements (allocates owned buffer)
    pub fn resize(self: *Output, dim: usize, max_size: usize) !void {
        if (dim >= self.num_dims) return;
        const d = &self.dimensions[dim];
        switch (d.dim_type) {
            .Float64 => {
                const new_data = try self.allocator.alloc(f64, max_size);
                @memset(new_data, 0);
                // Preserve existing data
                if (d.float64_data) |old_data| {
                    const copy_len = @min(old_data.len, max_size);
                    @memcpy(new_data[0..copy_len], old_data[0..copy_len]);
                    if (d.owned) self.allocator.free(old_data);
                }
                d.float64_data = new_data;
                d.owned = true;
                d.guts.capacity = max_size;
            },
            .Raw => {
                const new_data = try self.allocator.alloc(RawData, max_size);
                for (new_data) |*r| r.* = .{};
                // Preserve existing data
                if (d.raw_data) |old_data| {
                    const copy_len = @min(old_data.len, max_size);
                    @memcpy(new_data[0..copy_len], old_data[0..copy_len]);
                    if (d.owned) self.allocator.free(old_data);
                }
                d.raw_data = new_data;
                d.owned = true;
                d.guts.capacity = max_size;
            },
            .None => {},
        }
    }

    /// Grow a dimension's capacity (double it, min 64)
    pub fn grow(self: *Output, dim: usize) !void {
        if (dim >= self.num_dims) return;
        const d = &self.dimensions[dim];
        const new_size = if (d.guts.capacity < 64) @as(usize, 64) else d.guts.capacity * 2;
        try self.resize(dim, new_size);
    }

    /// Grow a dimension's capacity to at least target size
    pub fn growTo(self: *Output, dim: usize, target: usize) !void {
        if (dim >= self.num_dims) return;
        if (self.dimensions[dim].guts.capacity >= target) return;
        try self.resize(dim, target);
    }

    /// Trim all dimensions to keep only rows [start, start+size)
    pub fn trim(self: *Output, start: usize, size: usize) void {
        for (self.dimensions) |*d| {
            switch (d.dim_type) {
                .Float64 => {
                    if (d.float64_data) |data| {
                        if (start + size <= data.len and start > 0) {
                            std.mem.copyForwards(f64, data[0..size], data[start..][0..size]);
                        }
                    }
                },
                .Raw => {
                    if (d.raw_data) |data| {
                        if (start + size <= data.len) {
                            // Free owned entries before the kept range
                            for (data[0..start]) |*r| {
                                if (r.owned) self.allocator.free(@constCast(r.ptr));
                                r.* = .{};
                            }
                            // Free owned entries after the kept range
                            for (data[start + size ..]) |*r| {
                                if (r.owned) self.allocator.free(@constCast(r.ptr));
                                r.* = .{};
                            }
                            // Shift kept range to the front
                            if (start > 0) {
                                std.mem.copyForwards(RawData, data[0..size], data[start..][0..size]);
                                // Clear the vacated tail (already freed above)
                                for (data[size .. start + size]) |*r| {
                                    r.* = .{};
                                }
                            }
                        }
                    }
                },
                .None => {},
            }
        }
        self.num_rows = size;
        self.scan_offset += start;
    }

    /// Clear all row data (keep allocations)
    pub fn clear(self: *Output) void {
        self.num_rows = 0;
    }

    /// Clear and free all allocations
    pub fn clearAndShrink(self: *Output) void {
        for (self.dimensions) |*d| {
            if (d.owned) {
                if (d.float64_data) |data| self.allocator.free(data);
                if (d.raw_data) |data| self.allocator.free(data);
            }
            d.float64_data = null;
            d.raw_data = null;
            d.guts.capacity = 0;
            d.owned = false;
        }
        self.num_rows = 0;
    }

    /// Set float64 data for a dimension (borrows pointer, does NOT own)
    pub fn setFloat64Dimension(self: *Output, dim: usize, data: []const f64) !void {
        if (dim >= self.num_dims) return error.IndexOutOfBounds;
        self.dimensions[dim].dim_type = .Float64;
        self.dimensions[dim].float64_data = @constCast(data);
        self.dimensions[dim].owned = false;
    }

    /// Set raw data for a dimension (borrows pointer, does NOT own)
    pub fn setRawDimension(self: *Output, dim: usize, data: []const RawData) !void {
        if (dim >= self.num_dims) return error.IndexOutOfBounds;
        self.dimensions[dim].dim_type = .Raw;
        self.dimensions[dim].raw_data = @constCast(data);
        self.dimensions[dim].owned = false;
    }

    /// Set a single raw data cell
    pub fn setRaw(self: *Output, dim: usize, scan: usize, data: []const u8) !void {
        if (dim >= self.num_dims) return error.IndexOutOfBounds;
        const d = &self.dimensions[dim];
        if (d.raw_data) |raw_arr| {
            if (scan < raw_arr.len) {
                const owned_data = try self.allocator.dupe(u8, data);
                if (raw_arr[scan].owned) self.allocator.free(@constCast(raw_arr[scan].ptr));
                raw_arr[scan] = .{
                    .ptr = owned_data,
                    .size = @intCast(data.len),
                    .owned = true,
                };
            }
        }
    }

    /// Get float64 value at position
    pub fn float64(self: *const Output, dim: usize, row: usize) ?f64 {
        if (dim >= self.num_dims or row >= self.num_rows) return null;
        if (self.dimensions[dim].float64_data) |data| {
            if (row < data.len) return data[row];
        }
        return null;
    }

    /// Get raw data at position
    pub fn raw(self: *const Output, dim: usize, row: usize) ?RawData {
        if (dim >= self.num_dims or row >= self.num_rows) return null;
        if (self.dimensions[dim].raw_data) |data| {
            if (row < data.len) return data[row];
        }
        return null;
    }

    /// Get dimension type
    pub fn dimensionType(self: *const Output, dim: usize) ?OutputType {
        if (dim >= self.num_dims) return null;
        return self.dimensions[dim].dim_type;
    }

    /// Deep copy this output, cloning all dimension data into new owned buffers
    pub fn deepCopy(self: *const Output, allocator: std.mem.Allocator) !Output {
        var copy = try Output.init(allocator, self.num_dims);
        copy.block = self.block;
        copy.scan_offset = self.scan_offset;
        copy.num_rows = self.num_rows;

        for (0..self.num_dims) |v| {
            const src = &self.dimensions[v];
            copy.dimensions[v].dim_type = src.dim_type;
            copy.dimensions[v].guts.element_size = src.guts.element_size;

            switch (src.dim_type) {
                .Float64 => {
                    if (src.float64_data) |data| {
                        const size = @max(self.num_rows, src.guts.capacity);
                        const new_data = try allocator.alloc(f64, size);
                        const copy_len = @min(data.len, size);
                        @memcpy(new_data[0..copy_len], data[0..copy_len]);
                        if (copy_len < size) @memset(new_data[copy_len..], 0);
                        copy.dimensions[v].float64_data = new_data;
                        copy.dimensions[v].owned = true;
                        copy.dimensions[v].guts.capacity = size;
                    }
                },
                .Raw => {
                    if (src.raw_data) |data| {
                        const size = @max(self.num_rows, src.guts.capacity);
                        const new_data = try allocator.alloc(RawData, size);
                        for (new_data) |*r| r.* = .{};
                        for (0..@min(data.len, size)) |row| {
                            if (data[row].size > 0) {
                                const owned_copy = try allocator.dupe(u8, data[row].ptr);
                                new_data[row] = .{
                                    .ptr = owned_copy,
                                    .size = data[row].size,
                                    .owned = true,
                                };
                            }
                        }
                        copy.dimensions[v].raw_data = new_data;
                        copy.dimensions[v].owned = true;
                        copy.dimensions[v].guts.capacity = size;
                    }
                },
                .None => {},
            }
        }

        return copy;
    }

    /// Format for debug output
    pub fn format(self: *const Output, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.print("Output(dims={d}, rows={d}, block={d})", .{
            self.num_dims, self.num_rows, self.block,
        });
    }

    /// Compare two outputs for equality
    pub fn compare(self: *const Output, other: *const Output) bool {
        if (self.num_dims != other.num_dims or self.num_rows != other.num_rows)
            return false;
        for (0..self.num_dims) |d| {
            if (self.dimensions[d].dim_type != other.dimensions[d].dim_type)
                return false;
            switch (self.dimensions[d].dim_type) {
                .Float64 => {
                    const a = self.dimensions[d].float64_data orelse continue;
                    const b = other.dimensions[d].float64_data orelse return false;
                    for (0..self.num_rows) |r| {
                        if (r >= a.len or r >= b.len) return false;
                        if (a[r] != b[r]) return false;
                    }
                },
                .Raw => {
                    const a = self.dimensions[d].raw_data orelse continue;
                    const b = other.dimensions[d].raw_data orelse return false;
                    for (0..self.num_rows) |r| {
                        if (r >= a.len or r >= b.len) return false;
                        if (!std.mem.eql(u8, a[r].ptr, b[r].ptr)) return false;
                    }
                },
                .None => {},
            }
        }
        return true;
    }
};

// ─── Tests ──────────────────────────────────────────────────────

test "output initialization" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 2);
    defer output.deinit();

    try std.testing.expectEqual(@as(usize, 2), output.num_dims);
    try std.testing.expectEqual(@as(usize, 0), output.num_rows);
}

test "output float64 data" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();

    const data = try allocator.alloc(f64, 3);
    defer allocator.free(data);
    data[0] = 1.5;
    data[1] = 2.5;
    data[2] = 3.5;

    output.num_rows = 3;
    try output.setFloat64Dimension(0, data);

    try std.testing.expectEqual(OutputType.Float64, output.dimensionType(0).?);
    try std.testing.expectApproxEqAbs(@as(f64, 1.5), output.float64(0, 0).?, 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 2.5), output.float64(0, 1).?, 0.001);
}

test "output resize and grow" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();

    output.setType(0, .Float64);
    try output.resize(0, 10);

    try std.testing.expectEqual(@as(usize, 10), output.dimensions[0].guts.capacity);
    try std.testing.expect(output.dimensions[0].float64_data != null);

    // Write and read
    output.dimensions[0].float64_data.?[0] = 42.0;
    output.num_rows = 1;
    try std.testing.expectApproxEqAbs(@as(f64, 42.0), output.float64(0, 0).?, 0.001);

    // Grow
    try output.grow(0);
    try std.testing.expect(output.dimensions[0].guts.capacity >= 20);
}

test "output trim" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();

    output.setType(0, .Float64);
    try output.resize(0, 10);
    output.num_rows = 5;

    // Write values 0-4
    for (0..5) |i| {
        output.dimensions[0].float64_data.?[i] = @floatFromInt(i);
    }

    // Trim to keep rows [2, 2+2) = rows 2,3
    output.trim(2, 2);
    try std.testing.expectEqual(@as(usize, 2), output.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 2.0), output.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 3.0), output.dimensions[0].float64_data.?[1], 0.001);
}

test "output compare" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var out1 = try Output.init(allocator, 1);
    defer out1.deinit();
    var out2 = try Output.init(allocator, 1);
    defer out2.deinit();

    out1.setType(0, .Float64);
    out2.setType(0, .Float64);
    try out1.resize(0, 3);
    try out2.resize(0, 3);

    out1.num_rows = 2;
    out2.num_rows = 2;
    out1.dimensions[0].float64_data.?[0] = 1.0;
    out1.dimensions[0].float64_data.?[1] = 2.0;
    out2.dimensions[0].float64_data.?[0] = 1.0;
    out2.dimensions[0].float64_data.?[1] = 2.0;

    try std.testing.expect(out1.compare(&out2));

    out2.dimensions[0].float64_data.?[1] = 9.0;
    try std.testing.expect(!out1.compare(&out2));
}

test "output clear and shrink" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var output = try Output.init(allocator, 1);
    defer output.deinit();

    output.setType(0, .Float64);
    try output.resize(0, 100);
    output.num_rows = 50;

    output.clear();
    try std.testing.expectEqual(@as(usize, 0), output.num_rows);
    // Capacity still there
    try std.testing.expectEqual(@as(usize, 100), output.dimensions[0].guts.capacity);

    // Now shrink
    output.clearAndShrink();
    try std.testing.expectEqual(@as(usize, 0), output.dimensions[0].guts.capacity);
    try std.testing.expect(output.dimensions[0].float64_data == null);
}
