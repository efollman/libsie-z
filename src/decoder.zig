// Decoder - register-based bytecode virtual machine for binary data decoding
// Replaces sie_decoder.h / decoder.c
//
// Compiled bytecode programs read typed values from binary data,
// perform arithmetic/logic operations on registers, and emit
// output rows via SAMPLE instructions.
//
// Register file uses f64 for numeric values. Raw data registers
// use separate byte buffers.

const std = @import("std");
const output_mod = @import("output.zig");
const Output = output_mod.Output;
const OutputType = output_mod.OutputType;
const byteswap = @import("byteswap.zig");

/// Bytecode opcodes
pub const Opcode = enum(u8) {
    Crash = 0,
    // Read unsigned little-endian
    ReadU8LE = 1,
    ReadU16LE = 2,
    ReadU32LE = 3,
    ReadU64LE = 4,
    // Read signed little-endian
    ReadS8LE = 5,
    ReadS16LE = 6,
    ReadS32LE = 7,
    ReadS64LE = 8,
    // Read float little-endian
    ReadF32LE = 9,
    ReadF64LE = 10,
    // Read unsigned big-endian
    ReadU8BE = 11,
    ReadU16BE = 12,
    ReadU32BE = 13,
    ReadU64BE = 14,
    // Read signed big-endian
    ReadS8BE = 15,
    ReadS16BE = 16,
    ReadS32BE = 17,
    ReadS64BE = 18,
    // Read float big-endian
    ReadF32BE = 19,
    ReadF64BE = 20,
    // Data access
    ReadRaw = 21,
    Seek = 22,
    Sample = 23,
    // Register ops
    MoveReg = 24,
    // Arithmetic
    Add = 25,
    Sub = 26,
    Mul = 27,
    Div = 28,
    // Bitwise
    And = 29,
    Or = 30,
    Lsl = 31,
    Lsr = 32,
    // Logical
    LNot = 33,
    // Comparison
    Lt = 34,
    Le = 35,
    Gt = 36,
    Ge = 37,
    Eq = 38,
    Ne = 39,
    LAnd = 40,
    LOr = 41,
    // Compare and branch
    Cmp = 42,
    Branch = 43,
    BranchEq = 44,
    BranchNe = 45,
    BranchLt = 46,
    BranchGt = 47,
    BranchLe = 48,
    BranchGe = 49,
    // Debug
    Assert = 50,
};

/// Comparison flags
const FLAG_EQUAL: u8 = 0x1;
const FLAG_NOT_EQUAL: u8 = 0x2;
const FLAG_LESS: u8 = 0x4;
const FLAG_LESS_EQUAL: u8 = 0x8;
const FLAG_GREATER: u8 = 0x10;
const FLAG_GREATER_EQUAL: u8 = 0x20;

/// Number of arguments per opcode
fn opcodeArgs(op: Opcode) usize {
    return switch (op) {
        .Crash => 0,
        .ReadU8LE, .ReadU16LE, .ReadU32LE, .ReadU64LE => 1,
        .ReadS8LE, .ReadS16LE, .ReadS32LE, .ReadS64LE => 1,
        .ReadF32LE, .ReadF64LE => 1,
        .ReadU8BE, .ReadU16BE, .ReadU32BE, .ReadU64BE => 1,
        .ReadS8BE, .ReadS16BE, .ReadS32BE, .ReadS64BE => 1,
        .ReadF32BE, .ReadF64BE => 1,
        .ReadRaw => 2,
        .Seek => 2,
        .Sample => 0,
        .MoveReg => 2,
        .Add, .Sub, .Mul, .Div => 3,
        .And, .Or, .Lsl, .Lsr => 3,
        .LNot => 2,
        .Lt, .Le, .Gt, .Ge, .Eq, .Ne => 3,
        .LAnd, .LOr => 3,
        .Cmp => 2,
        .Branch => 1,
        .BranchEq, .BranchNe, .BranchLt, .BranchGt, .BranchLe, .BranchGe => 1,
        .Assert => 2,
    };
}

/// Raw data register (for ReadRaw opcode)
pub const RawRegister = struct {
    data: std.ArrayList(u8) = .{},

    pub fn deinit(self: *RawRegister, allocator: std.mem.Allocator) void {
        self.data.deinit(allocator);
    }

    pub fn clear(self: *RawRegister) void {
        self.data.clearRetainingCapacity();
    }
};

/// Decoder definition - compiled bytecode program
pub const Decoder = struct {
    allocator: std.mem.Allocator,
    bytecode: []const i32,
    num_dims: usize,
    dim_registers: []const usize,
    num_registers: usize,
    initial_registers: []f64,
    owned: bool = true,

    /// Create a decoder from bytecode arrays
    pub fn init(
        allocator: std.mem.Allocator,
        bytecode: []const i32,
        dim_registers: []const usize,
        num_registers: usize,
        initial_values: ?[]const f64,
    ) !Decoder {
        const bc = try allocator.dupe(i32, bytecode);
        const dims = try allocator.dupe(usize, dim_registers);
        const regs = try allocator.alloc(f64, num_registers);
        @memset(regs, 0);
        if (initial_values) |iv| {
            const copy_len = @min(iv.len, num_registers);
            @memcpy(regs[0..copy_len], iv[0..copy_len]);
        }
        return .{
            .allocator = allocator,
            .bytecode = bc,
            .num_dims = dim_registers.len,
            .dim_registers = dims,
            .num_registers = num_registers,
            .initial_registers = regs,
        };
    }

    pub fn deinit(self: *Decoder) void {
        if (self.owned) {
            self.allocator.free(@constCast(self.bytecode));
            self.allocator.free(@constCast(self.dim_registers));
            self.allocator.free(self.initial_registers);
        }
    }

    /// Compare two decoders for equality (same bytecode + registers)
    pub fn isEqual(self: *const Decoder, other: *const Decoder) bool {
        if (self.bytecode.len != other.bytecode.len) return false;
        if (self.num_registers != other.num_registers) return false;
        if (!std.mem.eql(i32, self.bytecode, other.bytecode)) return false;
        if (!std.mem.eql(f64, self.initial_registers, other.initial_registers)) return false;
        return true;
    }

    /// Compute a CRC-32 signature of the decoder
    pub fn signature(self: *const Decoder) u32 {
        var crc = std.hash.Crc32.init();
        crc.update(std.mem.sliceAsBytes(self.bytecode));
        crc.update(std.mem.sliceAsBytes(self.dim_registers));
        crc.update(std.mem.sliceAsBytes(self.initial_registers));
        return crc.final();
    }

    /// Disassemble bytecode to human-readable text
    pub fn disassemble(self: *const Decoder, allocator: std.mem.Allocator) ![]u8 {
        var buf: std.ArrayList(u8) = .{};
        errdefer buf.deinit(allocator);

        var pc: usize = 0;
        while (pc < self.bytecode.len) {
            const op_val = self.bytecode[pc];
            const op: Opcode = if (op_val >= 0 and op_val <= 50)
                @enumFromInt(@as(u8, @intCast(op_val)))
            else
                .Crash;

            // Format: "pc: OPNAME args...\n"
            var line_buf: [128]u8 = undefined;
            const args = opcodeArgs(op);
            const line = switch (args) {
                0 => std.fmt.bufPrint(&line_buf, "{d:>4}: {s}\n", .{ pc, @tagName(op) }) catch break,
                1 => std.fmt.bufPrint(&line_buf, "{d:>4}: {s} {d}\n", .{
                    pc,
                    @tagName(op),
                    if (pc + 1 < self.bytecode.len) self.bytecode[pc + 1] else 0,
                }) catch break,
                2 => std.fmt.bufPrint(&line_buf, "{d:>4}: {s} {d} {d}\n", .{
                    pc,
                    @tagName(op),
                    if (pc + 1 < self.bytecode.len) self.bytecode[pc + 1] else 0,
                    if (pc + 2 < self.bytecode.len) self.bytecode[pc + 2] else 0,
                }) catch break,
                3 => std.fmt.bufPrint(&line_buf, "{d:>4}: {s} {d} {d} {d}\n", .{
                    pc,
                    @tagName(op),
                    if (pc + 1 < self.bytecode.len) self.bytecode[pc + 1] else 0,
                    if (pc + 2 < self.bytecode.len) self.bytecode[pc + 2] else 0,
                    if (pc + 3 < self.bytecode.len) self.bytecode[pc + 3] else 0,
                }) catch break,
                else => break,
            };
            try buf.appendSlice(allocator, line);
            pc += 1 + args;
        }

        return buf.toOwnedSlice(allocator);
    }
};

/// Decoder execution machine - runs bytecode programs
pub const DecoderMachine = struct {
    allocator: std.mem.Allocator,
    decoder: *const Decoder,
    registers: []f64,
    raw_registers: []RawRegister,
    done: bool = false,
    pc: usize = 0,
    flags: u8 = 0,
    data: []const u8 = &.{},
    data_index: usize = 0,
    output: ?Output = null,

    /// Create a new machine for a decoder
    pub fn init(allocator: std.mem.Allocator, decoder: *const Decoder) !DecoderMachine {
        const regs = try allocator.alloc(f64, decoder.num_registers);
        @memcpy(regs, decoder.initial_registers);
        const raw_regs = try allocator.alloc(RawRegister, decoder.num_registers);
        for (raw_regs) |*r| r.* = .{};
        return .{
            .allocator = allocator,
            .decoder = decoder,
            .registers = regs,
            .raw_registers = raw_regs,
        };
    }

    pub fn deinit(self: *DecoderMachine) void {
        self.allocator.free(self.registers);
        for (self.raw_registers) |*r| r.deinit(self.allocator);
        self.allocator.free(self.raw_registers);
        if (self.output) |*out| out.deinit();
    }

    /// Prepare the machine to decode a new data block
    pub fn prep(self: *DecoderMachine, data: []const u8) void {
        @memcpy(self.registers, self.decoder.initial_registers);
        for (self.raw_registers) |*r| r.clear();
        self.pc = 0;
        self.flags = 0;
        self.data = data;
        self.data_index = 0;
        self.done = false;
        if (self.output) |*out| {
            out.num_rows = 0;
        }
    }

    /// Run the bytecode program to completion
    pub fn run(self: *DecoderMachine) !void {
        const bc = self.decoder.bytecode;
        const regs = self.registers;

        while (self.pc < bc.len) {
            const op_val = bc[self.pc];
            const op: Opcode = if (op_val >= 0 and op_val <= 50)
                @enumFromInt(@as(u8, @intCast(op_val)))
            else
                return error.InvalidData;

            switch (op) {
                .Crash => return error.OperationFailed,

                // --- Read unsigned LE ---
                .ReadU8LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(1)) break;
                    regs[dst] = @floatFromInt(self.data[self.data_index]);
                    self.data_index += 1;
                },
                .ReadU16LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(2)) break;
                    const v = std.mem.readInt(u16, self.data[self.data_index..][0..2], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 2;
                },
                .ReadU32LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const v = std.mem.readInt(u32, self.data[self.data_index..][0..4], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 4;
                },
                .ReadU64LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const v = std.mem.readInt(u64, self.data[self.data_index..][0..8], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 8;
                },

                // --- Read signed LE ---
                .ReadS8LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(1)) break;
                    const v: i8 = @bitCast(self.data[self.data_index]);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 1;
                },
                .ReadS16LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(2)) break;
                    const v = std.mem.readInt(i16, self.data[self.data_index..][0..2], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 2;
                },
                .ReadS32LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const v = std.mem.readInt(i32, self.data[self.data_index..][0..4], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 4;
                },
                .ReadS64LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const v = std.mem.readInt(i64, self.data[self.data_index..][0..8], .little);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 8;
                },

                // --- Read float LE ---
                .ReadF32LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const bits = std.mem.readInt(u32, self.data[self.data_index..][0..4], .little);
                    regs[dst] = @as(f64, @floatCast(@as(f32, @bitCast(bits))));
                    self.data_index += 4;
                },
                .ReadF64LE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const bits = std.mem.readInt(u64, self.data[self.data_index..][0..8], .little);
                    regs[dst] = @bitCast(bits);
                    self.data_index += 8;
                },

                // --- Read unsigned BE ---
                .ReadU8BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(1)) break;
                    regs[dst] = @floatFromInt(self.data[self.data_index]);
                    self.data_index += 1;
                },
                .ReadU16BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(2)) break;
                    const v = std.mem.readInt(u16, self.data[self.data_index..][0..2], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 2;
                },
                .ReadU32BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const v = std.mem.readInt(u32, self.data[self.data_index..][0..4], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 4;
                },
                .ReadU64BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const v = std.mem.readInt(u64, self.data[self.data_index..][0..8], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 8;
                },

                // --- Read signed BE ---
                .ReadS8BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(1)) break;
                    const v: i8 = @bitCast(self.data[self.data_index]);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 1;
                },
                .ReadS16BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(2)) break;
                    const v = std.mem.readInt(i16, self.data[self.data_index..][0..2], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 2;
                },
                .ReadS32BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const v = std.mem.readInt(i32, self.data[self.data_index..][0..4], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 4;
                },
                .ReadS64BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const v = std.mem.readInt(i64, self.data[self.data_index..][0..8], .big);
                    regs[dst] = @floatFromInt(v);
                    self.data_index += 8;
                },

                // --- Read float BE ---
                .ReadF32BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(4)) break;
                    const bits = std.mem.readInt(u32, self.data[self.data_index..][0..4], .big);
                    regs[dst] = @as(f64, @floatCast(@as(f32, @bitCast(bits))));
                    self.data_index += 4;
                },
                .ReadF64BE => {
                    const dst = arg(bc, self.pc, 1);
                    if (!self.readAvailable(8)) break;
                    const bits = std.mem.readInt(u64, self.data[self.data_index..][0..8], .big);
                    regs[dst] = @bitCast(bits);
                    self.data_index += 8;
                },

                // --- Data access ---
                .ReadRaw => {
                    const dst = arg(bc, self.pc, 1);
                    const len_reg = arg(bc, self.pc, 2);
                    const count = @as(usize, @intFromFloat(regs[len_reg]));
                    if (!self.readAvailable(count)) break;
                    var raw = &self.raw_registers[dst];
                    raw.clear();
                    try raw.data.appendSlice(self.allocator, self.data[self.data_index..][0..count]);
                    self.data_index += count;
                    // Use NaN sentinel to mark as raw
                    regs[dst] = std.math.nan(f64);
                },
                .Seek => {
                    const off_reg = arg(bc, self.pc, 1);
                    const whence_reg = arg(bc, self.pc, 2);
                    const offset = @as(i64, @intFromFloat(regs[off_reg]));
                    const whence = @as(i64, @intFromFloat(regs[whence_reg]));
                    switch (whence) {
                        0 => self.data_index = @intCast(@max(0, offset)), // SEEK_SET
                        1 => { // SEEK_CUR
                            const new = @as(i64, @intCast(self.data_index)) + offset;
                            self.data_index = @intCast(@max(0, new));
                        },
                        2 => { // SEEK_END
                            const new = @as(i64, @intCast(self.data.len)) + offset;
                            self.data_index = @intCast(@max(0, new));
                        },
                        else => {},
                    }
                },
                .Sample => {
                    try self.emitSample();
                },

                // --- Register ops ---
                .MoveReg => {
                    const dst = arg(bc, self.pc, 1);
                    const src = arg(bc, self.pc, 2);
                    regs[dst] = regs[src];
                },

                // --- Arithmetic ---
                .Add => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = regs[lhs] + regs[rhs];
                },
                .Sub => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = regs[lhs] - regs[rhs];
                },
                .Mul => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = regs[lhs] * regs[rhs];
                },
                .Div => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    if (regs[rhs] != 0) {
                        regs[dst] = regs[lhs] / regs[rhs];
                    } else {
                        regs[dst] = 0;
                    }
                },

                // --- Bitwise (cast to u64) ---
                .And => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    const a = toU64(regs[lhs]);
                    const b = toU64(regs[rhs]);
                    regs[dst] = @floatFromInt(a & b);
                },
                .Or => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    const a = toU64(regs[lhs]);
                    const b = toU64(regs[rhs]);
                    regs[dst] = @floatFromInt(a | b);
                },
                .Lsl => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    const a = toU64(regs[lhs]);
                    const shift: u6 = @intCast(@min(63, toU64(regs[rhs])));
                    regs[dst] = @floatFromInt(a << shift);
                },
                .Lsr => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    const a = toU64(regs[lhs]);
                    const shift: u6 = @intCast(@min(63, toU64(regs[rhs])));
                    regs[dst] = @floatFromInt(a >> shift);
                },

                // --- Logical ---
                .LNot => {
                    const dst = arg(bc, self.pc, 1);
                    const src = arg(bc, self.pc, 2);
                    regs[dst] = if (regs[src] == 0) 1.0 else 0.0;
                },
                .Lt => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] < regs[rhs]) 1.0 else 0.0;
                },
                .Le => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] <= regs[rhs]) 1.0 else 0.0;
                },
                .Gt => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] > regs[rhs]) 1.0 else 0.0;
                },
                .Ge => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] >= regs[rhs]) 1.0 else 0.0;
                },
                .Eq => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] == regs[rhs]) 1.0 else 0.0;
                },
                .Ne => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] != regs[rhs]) 1.0 else 0.0;
                },
                .LAnd => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] != 0 and regs[rhs] != 0) 1.0 else 0.0;
                },
                .LOr => {
                    const dst = arg(bc, self.pc, 1);
                    const lhs = arg(bc, self.pc, 2);
                    const rhs = arg(bc, self.pc, 3);
                    regs[dst] = if (regs[lhs] != 0 or regs[rhs] != 0) 1.0 else 0.0;
                },

                // --- Compare and branch ---
                .Cmp => {
                    const lhs = arg(bc, self.pc, 1);
                    const rhs = arg(bc, self.pc, 2);
                    self.flags = 0;
                    if (regs[lhs] == regs[rhs]) {
                        self.flags |= FLAG_EQUAL | FLAG_LESS_EQUAL | FLAG_GREATER_EQUAL;
                    } else {
                        self.flags |= FLAG_NOT_EQUAL;
                    }
                    if (regs[lhs] < regs[rhs]) {
                        self.flags |= FLAG_LESS | FLAG_LESS_EQUAL;
                    }
                    if (regs[lhs] > regs[rhs]) {
                        self.flags |= FLAG_GREATER | FLAG_GREATER_EQUAL;
                    }
                },
                .Branch => {
                    const offset = bc[self.pc + 1];
                    self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                    continue; // Don't advance pc normally
                },
                .BranchEq => {
                    if (self.flags & FLAG_EQUAL != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },
                .BranchNe => {
                    if (self.flags & FLAG_NOT_EQUAL != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },
                .BranchLt => {
                    if (self.flags & FLAG_LESS != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },
                .BranchGt => {
                    if (self.flags & FLAG_GREATER != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },
                .BranchLe => {
                    if (self.flags & FLAG_LESS_EQUAL != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },
                .BranchGe => {
                    if (self.flags & FLAG_GREATER_EQUAL != 0) {
                        const offset = bc[self.pc + 1];
                        self.pc = @intCast(@as(i64, @intCast(self.pc)) + offset);
                        continue;
                    }
                },

                // --- Debug ---
                .Assert => {
                    const lhs = arg(bc, self.pc, 1);
                    const rhs = arg(bc, self.pc, 2);
                    if (regs[lhs] != regs[rhs]) {
                        return error.OperationFailed;
                    }
                },
            }

            // Advance PC past opcode + args
            self.pc += 1 + opcodeArgs(op);
        }

        self.done = true;
    }

    /// Get the output
    pub fn getOutput(self: *const DecoderMachine) ?*const Output {
        if (self.output != null) return &self.output.?;
        return null;
    }

    // --- Internal helpers ---

    fn readAvailable(self: *const DecoderMachine, n: usize) bool {
        return self.data_index + n <= self.data.len;
    }

    fn emitSample(self: *DecoderMachine) !void {
        const decoder = self.decoder;

        // Create output on first sample
        if (self.output == null) {
            self.output = try Output.init(self.allocator, decoder.num_dims);
            for (0..decoder.num_dims) |v| {
                const reg = decoder.dim_registers[v];
                if (std.math.isNan(self.registers[reg])) {
                    self.output.?.setType(v, .Raw);
                } else {
                    self.output.?.setType(v, .Float64);
                }
                try self.output.?.resize(v, 64);
            }
        }

        var out = &self.output.?;
        const row = out.num_rows;

        // Grow if needed
        for (0..decoder.num_dims) |v| {
            if (out.dimensions[v].guts.capacity <= row) {
                try out.growTo(v, row + 64);
            }
        }

        // Copy register values to output
        for (0..decoder.num_dims) |v| {
            const reg = decoder.dim_registers[v];
            if (std.math.isNan(self.registers[reg])) {
                // Raw data — copy from raw register
                const raw = &self.raw_registers[reg];
                // Switch dimension to Raw if needed
                if (out.dimensions[v].dim_type != .Raw) {
                    out.setType(v, .Raw);
                    try out.resize(v, out.dimensions[v].guts.capacity);
                }
                try out.setRaw(v, row, raw.data.items);
            } else {
                if (out.dimensions[v].float64_data) |data| {
                    if (row < data.len) {
                        data[row] = self.registers[reg];
                    }
                }
            }
        }

        out.num_rows = row + 1;
    }
};

/// Extract argument from bytecode at pc+offset, as usize register index
inline fn arg(bc: []const i32, pc: usize, offset: usize) usize {
    return @intCast(bc[pc + offset]);
}

/// Cast f64 to u64 for bitwise ops
inline fn toU64(v: f64) u64 {
    if (v < 0 or std.math.isNan(v)) return 0;
    if (v > @as(f64, @floatFromInt(std.math.maxInt(u64)))) return std.math.maxInt(u64);
    return @intFromFloat(v);
}

// ─── Tests ──────────────────────────────────────────────────────

test "decoder simple read and sample" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Program: read u16 LE into reg 0, read u16 LE into reg 1, sample, (loop)
    const bytecode = [_]i32{
        @intFromEnum(Opcode.ReadU16LE), 0, // read u16 LE into R0
        @intFromEnum(Opcode.ReadU16LE), 1, // read u16 LE into R1
        @intFromEnum(Opcode.Sample), // emit row
    };
    const dim_regs = [_]usize{ 0, 1 };

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 2, null);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    // Data: two u16 LE values: 0x0100 (=256), 0x0200 (=512)
    const data = [_]u8{ 0x00, 0x01, 0x00, 0x02 };
    machine.prep(&data);
    try machine.run();

    try std.testing.expect(machine.done);
    const out = machine.getOutput().?;
    try std.testing.expectEqual(@as(usize, 1), out.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 256.0), out.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 512.0), out.dimensions[1].float64_data.?[0], 0.001);
}

test "decoder arithmetic ops" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // R0 = 10, R1 = 3 (initial), R2 = R0 + R1 = 13, R3 = R0 * R1 = 30
    const init_regs = [_]f64{ 10.0, 3.0, 0.0, 0.0 };
    const bytecode = [_]i32{
        @intFromEnum(Opcode.Add), 2, 0, 1, // R2 = R0 + R1
        @intFromEnum(Opcode.Mul),    3, 0, 1, // R3 = R0 * R1
        @intFromEnum(Opcode.Sample),
    };
    const dim_regs = [_]usize{ 2, 3 };

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 4, &init_regs);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    machine.prep(&.{});
    try machine.run();

    const out = machine.getOutput().?;
    try std.testing.expectEqual(@as(usize, 1), out.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 13.0), out.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 30.0), out.dimensions[1].float64_data.?[0], 0.001);
}

test "decoder branch and loop" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Count from 0 to 2: R0 = counter, R1 = limit(3), R2 = one(1)
    // Loop: sample, R0 += R2, cmp R0 R1, branch-lt back
    const init_regs = [_]f64{ 0.0, 3.0, 1.0 };
    const bytecode = [_]i32{
        // pc=0: sample
        @intFromEnum(Opcode.Sample),
        // pc=1: R0 = R0 + R2
        @intFromEnum(Opcode.Add),
        0,
        0,
        2,
        // pc=5: cmp R0, R1
        @intFromEnum(Opcode.Cmp),
        0,
        1,
        // pc=8: branch lt to pc=0 (offset = -8)
        @intFromEnum(Opcode.BranchLt),
        -8,
    };
    const dim_regs = [_]usize{0};

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 3, &init_regs);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    machine.prep(&.{});
    try machine.run();

    const out = machine.getOutput().?;
    try std.testing.expectEqual(@as(usize, 3), out.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 0.0), out.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), out.dimensions[0].float64_data.?[1], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 2.0), out.dimensions[0].float64_data.?[2], 0.001);
}

test "decoder big endian reads" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const bytecode = [_]i32{
        @intFromEnum(Opcode.ReadU32BE), 0,
        @intFromEnum(Opcode.Sample),
    };
    const dim_regs = [_]usize{0};

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 1, null);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    // 0x00000100 in big-endian = 256
    const data = [_]u8{ 0x00, 0x00, 0x01, 0x00 };
    machine.prep(&data);
    try machine.run();

    const out = machine.getOutput().?;
    try std.testing.expectApproxEqAbs(@as(f64, 256.0), out.dimensions[0].float64_data.?[0], 0.001);
}

test "decoder bitwise ops" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // R0 = 0xFF, R1 = 0x0F, R2 = R0 AND R1, R3 = 4 (shift amount)
    const init_regs = [_]f64{ 0xFF, 0x0F, 0, 4.0 };
    const bytecode = [_]i32{
        @intFromEnum(Opcode.And), 2, 0, 1, // R2 = 0xFF & 0x0F = 0x0F
        @intFromEnum(Opcode.Lsr),    0, 0, 3, // R0 = 0xFF >> 4 = 0x0F
        @intFromEnum(Opcode.Sample),
    };
    const dim_regs = [_]usize{ 0, 2 };

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 4, &init_regs);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    machine.prep(&.{});
    try machine.run();

    const out = machine.getOutput().?;
    try std.testing.expectApproxEqAbs(@as(f64, 15.0), out.dimensions[0].float64_data.?[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 15.0), out.dimensions[1].float64_data.?[0], 0.001);
}

test "decoder disassemble" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const bytecode = [_]i32{
        @intFromEnum(Opcode.ReadU16LE), 0,
        @intFromEnum(Opcode.Sample),
    };

    var decoder = try Decoder.init(allocator, &bytecode, &[_]usize{0}, 1, null);
    defer decoder.deinit();

    const dis = try decoder.disassemble(allocator);
    defer allocator.free(dis);

    try std.testing.expect(std.mem.indexOf(u8, dis, "ReadU16LE") != null);
    try std.testing.expect(std.mem.indexOf(u8, dis, "Sample") != null);
}

test "decoder signature and equality" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const bytecode = [_]i32{@intFromEnum(Opcode.Sample)};
    var d1 = try Decoder.init(allocator, &bytecode, &[_]usize{0}, 1, null);
    defer d1.deinit();
    var d2 = try Decoder.init(allocator, &bytecode, &[_]usize{0}, 1, null);
    defer d2.deinit();

    try std.testing.expect(d1.isEqual(&d2));
    try std.testing.expectEqual(d1.signature(), d2.signature());
}

test "decoder multiple data blocks" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const bytecode = [_]i32{
        @intFromEnum(Opcode.ReadU8LE), 0,
        @intFromEnum(Opcode.Sample),
    };
    const dim_regs = [_]usize{0};

    var decoder = try Decoder.init(allocator, &bytecode, &dim_regs, 1, null);
    defer decoder.deinit();

    var machine = try DecoderMachine.init(allocator, &decoder);
    defer machine.deinit();

    // First block - one byte, reads 1 byte, samples once = 1 row
    machine.prep(&[_]u8{10});
    try machine.run();
    try std.testing.expectEqual(@as(usize, 1), machine.getOutput().?.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 10.0), machine.getOutput().?.dimensions[0].float64_data.?[0], 0.001);

    // Second block (prep resets output)
    machine.prep(&[_]u8{30});
    try machine.run();
    try std.testing.expectEqual(@as(usize, 1), machine.getOutput().?.num_rows);
    try std.testing.expectApproxEqAbs(@as(f64, 30.0), machine.getOutput().?.dimensions[0].float64_data.?[0], 0.001);
}
