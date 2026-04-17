// ChannelSpigot - full decode pipeline for channel data
//
// Pipeline per block:
//   GroupSpigot.get() → raw bytes
//     → DecoderMachine.run() → decoded Output (v0, v1, ...)
//       → Combiner.combine() → remapped Output (dim0, dim1, ...)
//         → Transform.apply() → scaled/mapped Output
//
// Matches the C implementation in channel.c: channel_spigot_get_inner()

const std = @import("std");
const file_mod = @import("file.zig");
const channel_mod = @import("channel.zig");
const decoder_mod = @import("decoder.zig");
const combiner_mod = @import("combiner.zig");
const transform_mod = @import("transform.zig");
const output_mod = @import("output.zig");
const group_spigot_mod = @import("group_spigot.zig");
const error_mod = @import("error.zig");

/// Full decode pipeline for reading channel data
pub const ChannelSpigot = struct {
    allocator: std.mem.Allocator,
    group_spigot: group_spigot_mod.GroupSpigot,
    machine: decoder_mod.DecoderMachine,
    combiner: combiner_mod.Combiner,
    xform: transform_mod.Transform,
    transforms_disabled: bool = false,
    scan_limit: u64 = 0,

    pub const BoundResult = struct { block: usize, scan: usize };

    /// Create a channel spigot for reading decoded data
    pub fn init(
        allocator: std.mem.Allocator,
        file: *file_mod.File,
        ch: *const channel_mod.Channel,
        compiled_decoders: *const std.AutoHashMap(i32, decoder_mod.Decoder),
    ) !ChannelSpigot {
        const dims = ch.getDimensions();
        const num_dims = dims.len;

        if (num_dims == 0) return error_mod.Error.InvalidData;

        // All dimensions share the same group and decoder
        const group_id = ch.toplevel_group;
        const decoder_id: i32 = @intCast(dims[0].decoder_id);

        const decoder = compiled_decoders.getPtr(decoder_id) orelse
            return error_mod.Error.InvalidData;

        // Build combiner mappings: decoder v-register → channel dimension index
        var comb = try combiner_mod.Combiner.init(allocator, num_dims);
        errdefer comb.deinit();

        for (dims, 0..) |dim, i| {
            // decoder_version holds the "v" attribute (output variable index)
            comb.addMapping(dim.decoder_version, i);
        }

        // Build transforms from dimension xform info
        var xform = try transform_mod.Transform.init(allocator, num_dims);
        errdefer xform.deinit();

        for (dims, 0..) |dim, i| {
            if (dim.has_linear_xform) {
                xform.setLinear(i, dim.xform_scale, dim.xform_offset);
            }
        }

        var machine = try decoder_mod.DecoderMachine.init(allocator, decoder);
        errdefer machine.deinit();

        return .{
            .allocator = allocator,
            .group_spigot = group_spigot_mod.GroupSpigot.init(allocator, file, group_id),
            .machine = machine,
            .combiner = comb,
            .xform = xform,
        };
    }

    pub fn deinit(self: *ChannelSpigot) void {
        self.group_spigot.deinit();
        self.machine.deinit();
        self.combiner.deinit();
        self.xform.deinit();
    }

    /// Get the next decoded output block. Returns null when done.
    pub fn get(self: *ChannelSpigot) !?*output_mod.Output {
        // Get raw block from group
        const raw_data = try self.group_spigot.get() orelse return null;
        if (raw_data.len == 0) return null;

        const block_num = @as(usize, @intCast(self.group_spigot.tell())) - 1;

        // Prep decoder with raw data and run
        self.machine.prep(raw_data);
        try self.machine.run();

        // Get decoder output
        if (self.machine.output) |*dec_out| {
            if (dec_out.num_rows == 0) return null;

            // Remap through combiner
            var combined = try self.combiner.combine(dec_out);

            // Apply transforms
            if (!self.transforms_disabled) {
                self.xform.apply(combined);
            }

            combined.block = block_num;

            // Apply scan limit
            if (self.scan_limit > 0 and combined.num_rows > self.scan_limit) {
                combined.num_rows = @intCast(self.scan_limit);
            }

            return combined;
        }

        return null;
    }

    /// Get number of blocks available
    pub fn numBlocks(self: *const ChannelSpigot) usize {
        return self.group_spigot.numBlocks();
    }

    /// Seek to a block index
    pub fn seek(self: *ChannelSpigot, target: u64) u64 {
        return self.group_spigot.seek(target);
    }

    /// Tell current position
    pub fn tell(self: *const ChannelSpigot) u64 {
        return self.group_spigot.tell();
    }

    /// Check if done
    pub fn isDone(self: *const ChannelSpigot) bool {
        return self.group_spigot.isDone();
    }

    /// Reset to beginning
    pub fn reset(self: *ChannelSpigot) void {
        self.group_spigot.reset();
    }

    /// Disable/enable transforms
    pub fn disableTransforms(self: *ChannelSpigot, disable: bool) void {
        self.transforms_disabled = disable;
    }

    /// Manually apply transforms to an already-fetched output
    pub fn transformOutput(self: *ChannelSpigot, output: *output_mod.Output) void {
        self.xform.apply(output);
    }

    /// Clear combiner output, forcing reallocation on next get()
    pub fn clearOutput(self: *ChannelSpigot) void {
        if (self.combiner.output) |*out| {
            out.clearAndShrink();
        }
    }

    /// Set scan limit — each block's output will be trimmed to at most this many rows
    pub fn setScanLimit(self: *ChannelSpigot, limit: u64) void {
        self.scan_limit = limit;
    }

    const BEFORE: isize = -1;
    const AFTER: isize = -2;

    /// Find the first scan >= value in a single block's output for the given dimension
    fn findValueLower(output: *const output_mod.Output, dim: usize, value: f64) isize {
        const data = output.dimensions[dim].float64_data orelse return BEFORE;
        if (output.num_rows == 0) return BEFORE;
        const n = output.num_rows;
        if (value <= data[0]) return BEFORE;
        if (value > data[n - 1]) return AFTER;
        var low: isize = 0;
        var high: isize = @intCast(n - 1);
        while (true) {
            if (low > high) return low;
            const mid = @divTrunc(high + low, 2);
            const mid_u: usize = @intCast(mid);
            if (data[mid_u] < value) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
    }

    /// Find the last scan <= value in a single block's output for the given dimension
    fn findValueUpper(output: *const output_mod.Output, dim: usize, value: f64) isize {
        const data = output.dimensions[dim].float64_data orelse return BEFORE;
        if (output.num_rows == 0) return BEFORE;
        const n = output.num_rows;
        if (value < data[0]) return BEFORE;
        if (value >= data[n - 1]) return AFTER;
        var low: isize = 0;
        var high: isize = @intCast(n - 1);
        while (true) {
            if (low > high) return high;
            const mid = @divTrunc(high + low, 2);
            const mid_u: usize = @intCast(mid);
            if (data[mid_u] > value) {
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
    }

    /// Two-level binary search for the first scan >= value (lower_bound)
    /// or the last scan <= value (upper_bound).
    /// Returns .{ block, scan } or null if not found.
    /// Restores original seek position on return.
    fn binarySearch(self: *ChannelSpigot, dim: usize, value: f64, upper_bound: bool) !?BoundResult {
        const orig = self.tell();
        const num = self.seek(std.math.maxInt(u64));
        var low: isize = 0;
        var high: isize = @as(isize, @intCast(num)) - 1;

        while (true) {
            if (low > high) {
                if (upper_bound) {
                    if (high >= 0) {
                        const h: usize = @intCast(high);
                        _ = self.seek(h);
                        const output = try self.get() orelse {
                            _ = self.seek(orig);
                            return null;
                        };
                        _ = self.seek(orig);
                        return .{ .block = h, .scan = output.num_rows - 1 };
                    }
                } else {
                    const l: usize = @intCast(low);
                    if (l < num) {
                        _ = self.seek(orig);
                        return .{ .block = l, .scan = 0 };
                    }
                }
                _ = self.seek(orig);
                return null;
            }
            const mid = @divTrunc(high + low, 2);
            const mid_u: usize = @intCast(mid);
            _ = self.seek(mid_u);
            const output = try self.get() orelse {
                _ = self.seek(orig);
                return null;
            };
            const pos = if (upper_bound)
                findValueUpper(output, dim, value)
            else
                findValueLower(output, dim, value);

            if (pos >= 0) {
                // Found in block
                _ = self.seek(orig);
                return .{ .block = mid_u, .scan = @intCast(pos) };
            } else if (pos == AFTER) {
                low = mid + 1;
            } else if (pos == BEFORE) {
                high = mid - 1;
            }
        }
    }

    /// Find the first element >= value in the given dimension.
    /// Returns .{ block, scan } or null if not found.
    pub fn lowerBound(self: *ChannelSpigot, dim: usize, value: f64) !?BoundResult {
        return self.binarySearch(dim, value, false);
    }

    /// Find the last element <= value in the given dimension.
    /// Returns .{ block, scan } or null if not found.
    pub fn upperBound(self: *ChannelSpigot, dim: usize, value: f64) !?BoundResult {
        return self.binarySearch(dim, value, true);
    }
};

// ── Tests ──

test "ChannelSpigot: decode channel data" {
    const testing = std.testing;
    const sie_file_mod = @import("sie_file.zig");
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var sf = sie_file_mod.SieFile.open(allocator, "test/data/sie_min_timhis_a_19EFAA61.sie") catch |e| {
        if (e == error_mod.Error.FileNotFound) return;
        return e;
    };
    defer sf.deinit();

    const channels = sf.getAllChannels();
    if (channels.len == 0) return;

    // Try to attach a spigot to the first channel
    var spig = sf.attachSpigot(channels[0]) catch return;
    defer spig.deinit();

    // Read first block of data
    if (try spig.get()) |out| {
        try testing.expect(out.num_rows > 0);
        try testing.expect(out.num_dims > 0);
    }
}
