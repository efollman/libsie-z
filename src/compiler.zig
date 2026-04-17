// Compiler - XML to bytecode compiler for decoder programs
// Replaces sie_compiler.h / compiler.c
//
// Compiles XML decoder descriptions into bytecode programs that can
// be executed by the Decoder virtual machine. Supports control flow
// (loops, conditionals, branches), typed reads (int/uint/float in
// various sizes/endians), register operations, and expression trees.

const std = @import("std");
const decoder_mod = @import("decoder.zig");
const xml_mod = @import("xml.zig");
const relation_mod = @import("relation.zig");
const Opcode = decoder_mod.Opcode;

/// Value type classification for compiler arguments
pub const ValueType = enum {
    Name,
    Number,
    Expression,
};

/// Classify a value string
pub fn classifyValue(name: []const u8) ?ValueType {
    if (name.len == 0) return null;
    const c1 = std.ascii.toUpper(name[0]);
    if (c1 == '{') return .Expression;
    if ((c1 >= 'A' and c1 <= 'Z') or c1 == '_') return .Name;
    if ((c1 >= '0' and c1 <= '9') or c1 == '.' or c1 == '-') return .Number;
    return null;
}

/// Parse a number from a string value, returning f64 register value
pub fn parseNumber(name: []const u8) ?f64 {
    // Try float parse first
    return std.fmt.parseFloat(f64, name) catch return null;
}

/// Dump register bytes as hex string "0X..."
pub fn dumpRegisterBytes(reg: f64, buf: *[18]u8) void {
    const bytes = std.mem.asBytes(&reg);
    buf[0] = '0';
    buf[1] = 'X';
    for (bytes, 0..) |b, i| {
        const hex = "0123456789ABCDEF";
        buf[2 + i * 2] = hex[b >> 4];
        buf[2 + i * 2 + 1] = hex[b & 0xF];
    }
}

/// Compiler - compiles XML decoder descriptions into bytecode
pub const Compiler = struct {
    allocator: std.mem.Allocator,
    bytecode: std.ArrayList(i32),
    label_locations: std.ArrayList(i32),
    label_fixups: std.ArrayList(i32),
    initial_registers: std.ArrayList(f64),
    register_names: relation_mod.Relation,
    temp_register_names: std.ArrayList([]u8),
    next_temp_register: usize,

    pub fn init(allocator: std.mem.Allocator) Compiler {
        return Compiler{
            .allocator = allocator,
            .bytecode = .{},
            .label_locations = .{},
            .label_fixups = .{},
            .initial_registers = .{},
            .register_names = relation_mod.Relation.init(allocator),
            .temp_register_names = .{},
            .next_temp_register = 0,
        };
    }

    pub fn deinit(self: *Compiler) void {
        for (self.temp_register_names.items) |name| {
            self.allocator.free(name);
        }
        self.temp_register_names.deinit(self.allocator);
        self.register_names.deinit();
        self.initial_registers.deinit(self.allocator);
        self.label_fixups.deinit(self.allocator);
        self.label_locations.deinit(self.allocator);
        self.bytecode.deinit(self.allocator);
    }

    /// Create a new label, returning its index
    pub fn nextLabel(self: *Compiler) !usize {
        const next = self.label_locations.items.len;
        try self.label_locations.append(self.allocator, -1);
        return next;
    }

    /// Save temp register state for later restoration
    pub fn saveTempRegisters(self: *const Compiler) usize {
        return self.next_temp_register;
    }

    /// Restore temp register state
    pub fn restoreTempRegisters(self: *Compiler, saved: usize) void {
        self.next_temp_register = saved;
    }

    /// Get next temp register name (e.g., "r0", "r1", ...)
    pub fn nextTempRegister(self: *Compiler) ![]const u8 {
        const next = self.next_temp_register;
        self.next_temp_register += 1;

        if (next >= self.temp_register_names.items.len) {
            // Create new name "rN"
            var buf: [12]u8 = undefined;
            const name = std.fmt.bufPrint(&buf, "r{d}", .{next}) catch unreachable;
            const owned = try self.allocator.dupe(u8, name);
            try self.temp_register_names.append(self.allocator, owned);
            return owned;
        }
        return self.temp_register_names.items[next];
    }

    /// Resolve a register name to a register index.
    /// Numbers are converted to their hex byte representation.
    /// New names are assigned the next register index.
    pub fn resolveRegister(self: *Compiler, name: []const u8) !i32 {
        const vtype = classifyValue(name) orelse return error.InvalidValue;

        var name_used: []const u8 = undefined;
        var initial_value: f64 = 0.0;
        var hex_buf: [18]u8 = undefined;

        switch (vtype) {
            .Name => {
                name_used = name;
            },
            .Number => {
                initial_value = parseNumber(name) orelse return error.InvalidValue;
                dumpRegisterBytes(initial_value, &hex_buf);
                name_used = &hex_buf;
            },
            .Expression => {
                // Expression: compile and return the register
                return self.compileTextExprToRegister(null, name);
            },
        }

        // Look up existing register
        if (self.register_names.intValue(name_used)) |reg| {
            return @intCast(reg);
        }

        // Create new register
        const reg: i32 = @intCast(self.initial_registers.items.len);
        try self.initial_registers.append(self.allocator, initial_value);
        var reg_buf: [16]u8 = undefined;
        const reg_str = std.fmt.bufPrint(&reg_buf, "{d}", .{reg}) catch unreachable;
        try self.register_names.setValue(name_used, reg_str);
        return reg;
    }

    /// Emit a label at the current bytecode position
    pub fn emitLabel(self: *Compiler, label: usize) void {
        self.label_locations.items[label] = @intCast(self.bytecode.items.len);
    }

    /// Emit a 0-argument opcode
    pub fn emit0(self: *Compiler, op: Opcode) !void {
        try self.bytecode.append(self.allocator, @intFromEnum(op));
    }

    /// Emit a 1-argument opcode
    pub fn emit1(self: *Compiler, op: Opcode, a1: []const u8) !void {
        const r1 = try self.resolveRegister(a1);
        try self.bytecode.append(self.allocator, @intFromEnum(op));
        try self.bytecode.append(self.allocator, r1);
    }

    /// Emit a 2-argument opcode
    pub fn emit2(self: *Compiler, op: Opcode, a1: []const u8, a2: []const u8) !void {
        const r1 = try self.resolveRegister(a1);
        const r2 = try self.resolveRegister(a2);
        try self.bytecode.append(self.allocator, @intFromEnum(op));
        try self.bytecode.append(self.allocator, r1);
        try self.bytecode.append(self.allocator, r2);
    }

    /// Emit a 3-argument opcode
    pub fn emit3(self: *Compiler, op: Opcode, a1: []const u8, a2: []const u8, a3: []const u8) !void {
        const r1 = try self.resolveRegister(a1);
        const r2 = try self.resolveRegister(a2);
        const r3 = try self.resolveRegister(a3);
        try self.bytecode.append(self.allocator, @intFromEnum(op));
        try self.bytecode.append(self.allocator, r1);
        try self.bytecode.append(self.allocator, r2);
        try self.bytecode.append(self.allocator, r3);
    }

    pub const CompileError = error{ InvalidValue, InvalidData, OutOfMemory };

    /// Emit a MR (move register) instruction, with expression optimization
    pub fn emitMr(self: *Compiler, dest: []const u8, src: []const u8) CompileError!void {
        const r1 = try self.resolveRegister(dest);
        if (classifyValue(src) == .Expression) {
            _ = try self.compileTextExprToRegister(dest, src);
            return;
        }
        const r2 = try self.resolveRegister(src);
        try self.bytecode.append(self.allocator, @intFromEnum(Opcode.MoveReg));
        try self.bytecode.append(self.allocator, r1);
        try self.bytecode.append(self.allocator, r2);
    }

    /// Emit a branch instruction with label fixup
    pub fn emitBranch(self: *Compiler, op: Opcode, label: usize) !void {
        try self.bytecode.append(self.allocator, @intFromEnum(op));
        const fixup_loc: i32 = @intCast(self.bytecode.items.len);
        try self.bytecode.append(self.allocator, 0); // placeholder
        try self.label_fixups.append(self.allocator, @intCast(label));
        try self.label_fixups.append(self.allocator, fixup_loc);
    }

    /// Fixup all label references after compilation
    pub fn fixupLabels(self: *Compiler) void {
        while (self.label_fixups.items.len > 0) {
            const fixup_loc: usize = @intCast(self.label_fixups.pop().?);
            const label: usize = @intCast(self.label_fixups.pop().?);
            const target = self.label_locations.items[label];
            self.bytecode.items[fixup_loc] = target - @as(i32, @intCast(fixup_loc)) + 1;
        }
    }

    /// Compile a text expression (in braces) to a register
    fn compileTextExprToRegister(self: *Compiler, return_reg: ?[]const u8, text: []const u8) CompileError!i32 {
        // Strip braces: "{expr}" -> "expr"
        const had_braces = text.len >= 2 and text[0] == '{' and text[text.len - 1] == '}';
        const inner = if (had_braces) text[1 .. text.len - 1] else text;

        // For non-braced text, try simple resolution first
        if (!had_braces) {
            if (classifyValue(inner) == .Name) {
                if (return_reg) |ret| {
                    if (!std.mem.eql(u8, inner, ret)) {
                        try self.emitMr(ret, inner);
                    }
                }
                return self.resolveRegister(inner);
            }

            if (classifyValue(inner) == .Number) {
                if (return_reg) |ret| {
                    try self.emitMr(ret, inner);
                }
                return self.resolveRegister(inner);
            }
        }

        // Parse the expression using recursive descent
        var parser = ExprParser{
            .compiler = self,
            .input = inner,
            .pos = 0,
        };
        parser.skipWhitespace();
        const result_reg = try parser.parseExpr();

        // Move result to return_reg if needed
        if (return_reg) |ret| {
            const dest = try self.resolveRegister(ret);
            if (dest != result_reg) {
                try self.bytecode.append(self.allocator, @intFromEnum(Opcode.MoveReg));
                try self.bytecode.append(self.allocator, dest);
                try self.bytecode.append(self.allocator, result_reg);
            }
            return dest;
        }
        return result_reg;
    }

    // ---- Expression Parser ----
    // Recursive descent parser for text expressions like "{$v1 + $last}", "{($vb >> $shift) & 1}"

    const ExprParser = struct {
        compiler: *Compiler,
        input: []const u8,
        pos: usize,

        fn skipWhitespace(self: *ExprParser) void {
            while (self.pos < self.input.len and (self.input[self.pos] == ' ' or self.input[self.pos] == '\t' or self.input[self.pos] == '\n' or self.input[self.pos] == '\r')) {
                self.pos += 1;
            }
        }

        fn peek(self: *ExprParser) ?u8 {
            if (self.pos < self.input.len) return self.input[self.pos];
            return null;
        }

        fn advance(self: *ExprParser) void {
            if (self.pos < self.input.len) self.pos += 1;
        }

        fn matchChar(self: *ExprParser, c: u8) bool {
            if (self.pos < self.input.len and self.input[self.pos] == c) {
                self.pos += 1;
                return true;
            }
            return false;
        }

        // Precedence levels (low to high):
        // ||  &&  |  &  ==!=  <><=>= <<>>  +-  */%  unary  primary

        fn parseExpr(self: *ExprParser) CompileError!i32 {
            return self.parseLOr();
        }

        fn parseLOr(self: *ExprParser) CompileError!i32 {
            var left = try self.parseLAnd();
            while (true) {
                self.skipWhitespace();
                if (self.pos + 1 < self.input.len and self.input[self.pos] == '|' and self.input[self.pos + 1] == '|') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseLAnd();
                    left = try self.emitBinOp(.LOr, left, right);
                } else break;
            }
            return left;
        }

        fn parseLAnd(self: *ExprParser) CompileError!i32 {
            var left = try self.parseBitOr();
            while (true) {
                self.skipWhitespace();
                if (self.pos + 1 < self.input.len and self.input[self.pos] == '&' and self.input[self.pos + 1] == '&') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseBitOr();
                    left = try self.emitBinOp(.LAnd, left, right);
                } else break;
            }
            return left;
        }

        fn parseBitOr(self: *ExprParser) CompileError!i32 {
            var left = try self.parseBitAnd();
            while (true) {
                self.skipWhitespace();
                if (self.pos < self.input.len and self.input[self.pos] == '|' and (self.pos + 1 >= self.input.len or self.input[self.pos + 1] != '|')) {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseBitAnd();
                    left = try self.emitBinOp(.Or, left, right);
                } else break;
            }
            return left;
        }

        fn parseBitAnd(self: *ExprParser) CompileError!i32 {
            var left = try self.parseCompare();
            while (true) {
                self.skipWhitespace();
                if (self.pos < self.input.len and self.input[self.pos] == '&' and (self.pos + 1 >= self.input.len or self.input[self.pos + 1] != '&')) {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseCompare();
                    left = try self.emitBinOp(.And, left, right);
                } else break;
            }
            return left;
        }

        fn parseCompare(self: *ExprParser) CompileError!i32 {
            var left = try self.parseShift();
            self.skipWhitespace();
            if (self.pos + 1 < self.input.len) {
                const c0 = self.input[self.pos];
                const c1 = self.input[self.pos + 1];
                if (c0 == '=' and c1 == '=') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Eq, left, right);
                } else if (c0 == '!' and c1 == '=') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Ne, left, right);
                } else if (c0 == '<' and c1 == '=') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Le, left, right);
                } else if (c0 == '>' and c1 == '=') {
                    self.pos += 2;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Ge, left, right);
                } else if (c0 == '<' and c1 != '<') {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Lt, left, right);
                } else if (c0 == '>' and c1 != '>') {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Gt, left, right);
                }
            } else if (self.pos < self.input.len) {
                const c0 = self.input[self.pos];
                if (c0 == '<') {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Lt, left, right);
                } else if (c0 == '>') {
                    self.pos += 1;
                    self.skipWhitespace();
                    const right = try self.parseShift();
                    left = try self.emitBinOp(.Gt, left, right);
                }
            }
            return left;
        }

        fn parseShift(self: *ExprParser) CompileError!i32 {
            var left = try self.parseAdditive();
            while (true) {
                self.skipWhitespace();
                if (self.pos + 1 < self.input.len) {
                    const c0 = self.input[self.pos];
                    const c1 = self.input[self.pos + 1];
                    if (c0 == '<' and c1 == '<') {
                        self.pos += 2;
                        self.skipWhitespace();
                        const right = try self.parseAdditive();
                        left = try self.emitBinOp(.Lsl, left, right);
                    } else if (c0 == '>' and c1 == '>') {
                        self.pos += 2;
                        self.skipWhitespace();
                        const right = try self.parseAdditive();
                        left = try self.emitBinOp(.Lsr, left, right);
                    } else break;
                } else break;
            }
            return left;
        }

        fn parseAdditive(self: *ExprParser) CompileError!i32 {
            var left = try self.parseMultiplicative();
            while (true) {
                self.skipWhitespace();
                const c = self.peek() orelse break;
                if (c == '+') {
                    self.advance();
                    self.skipWhitespace();
                    const right = try self.parseMultiplicative();
                    left = try self.emitBinOp(.Add, left, right);
                } else if (c == '-') {
                    // Check if this is a binary minus or start of negative number
                    // It's binary minus because we already have a left operand
                    self.advance();
                    self.skipWhitespace();
                    const right = try self.parseMultiplicative();
                    left = try self.emitBinOp(.Sub, left, right);
                } else break;
            }
            return left;
        }

        fn parseMultiplicative(self: *ExprParser) CompileError!i32 {
            var left = try self.parseUnary();
            while (true) {
                self.skipWhitespace();
                const c = self.peek() orelse break;
                if (c == '*') {
                    self.advance();
                    self.skipWhitespace();
                    const right = try self.parseUnary();
                    left = try self.emitBinOp(.Mul, left, right);
                } else if (c == '/') {
                    self.advance();
                    self.skipWhitespace();
                    const right = try self.parseUnary();
                    left = try self.emitBinOp(.Div, left, right);
                } else if (c == '%') {
                    self.advance();
                    self.skipWhitespace();
                    // Modulo — no opcode available, skip for now
                    _ = try self.parseUnary();
                } else break;
            }
            return left;
        }

        fn parseUnary(self: *ExprParser) CompileError!i32 {
            self.skipWhitespace();
            const c = self.peek() orelse return error.InvalidValue;
            if (c == '!') {
                self.advance();
                self.skipWhitespace();
                const operand = try self.parseUnary();
                const tmp = try self.compiler.resolveRegister(try self.compiler.nextTempRegister());
                try self.compiler.bytecode.append(self.compiler.allocator, @intFromEnum(Opcode.LNot));
                try self.compiler.bytecode.append(self.compiler.allocator, tmp);
                try self.compiler.bytecode.append(self.compiler.allocator, operand);
                return tmp;
            }
            if (c == '-') {
                // Could be unary minus or negative number
                // Check if next char is digit — if so, parse as negative number
                if (self.pos + 1 < self.input.len and (self.input[self.pos + 1] >= '0' and self.input[self.pos + 1] <= '9')) {
                    return self.parsePrimary();
                }
                self.advance();
                self.skipWhitespace();
                const operand = try self.parseUnary();
                const zero = try self.compiler.resolveRegister("0");
                const tmp = try self.compiler.resolveRegister(try self.compiler.nextTempRegister());
                try self.compiler.bytecode.append(self.compiler.allocator, @intFromEnum(Opcode.Sub));
                try self.compiler.bytecode.append(self.compiler.allocator, tmp);
                try self.compiler.bytecode.append(self.compiler.allocator, zero);
                try self.compiler.bytecode.append(self.compiler.allocator, operand);
                return tmp;
            }
            return self.parsePrimary();
        }

        fn parsePrimary(self: *ExprParser) CompileError!i32 {
            self.skipWhitespace();
            const c = self.peek() orelse return error.InvalidValue;

            // Parenthesized expression
            if (c == '(') {
                self.advance();
                self.skipWhitespace();
                const result = try self.parseExpr();
                self.skipWhitespace();
                if (!self.matchChar(')')) return error.InvalidValue;
                return result;
            }

            // Variable reference: $name
            if (c == '$') {
                self.advance();
                const start = self.pos;
                while (self.pos < self.input.len) {
                    const ch = self.input[self.pos];
                    if ((ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or (ch >= '0' and ch <= '9') or ch == '_') {
                        self.pos += 1;
                    } else break;
                }
                if (self.pos == start) return error.InvalidValue;
                const name = self.input[start..self.pos];
                return self.compiler.resolveRegister(name);
            }

            // Number (including negative)
            if ((c >= '0' and c <= '9') or c == '.' or c == '-') {
                const start = self.pos;
                if (c == '-') self.pos += 1;
                // Accept hex 0x... or 0X...
                if (self.pos + 1 < self.input.len and self.input[self.pos] == '0' and (self.input[self.pos + 1] == 'x' or self.input[self.pos + 1] == 'X')) {
                    self.pos += 2;
                    while (self.pos < self.input.len) {
                        const ch = self.input[self.pos];
                        if ((ch >= '0' and ch <= '9') or (ch >= 'a' and ch <= 'f') or (ch >= 'A' and ch <= 'F')) {
                            self.pos += 1;
                        } else break;
                    }
                } else {
                    while (self.pos < self.input.len) {
                        const ch = self.input[self.pos];
                        if ((ch >= '0' and ch <= '9') or ch == '.') {
                            self.pos += 1;
                        } else break;
                    }
                }
                const num_str = self.input[start..self.pos];
                return self.compiler.resolveRegister(num_str);
            }

            // Bare name (variable without $)
            if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or c == '_') {
                const start = self.pos;
                while (self.pos < self.input.len) {
                    const ch = self.input[self.pos];
                    if ((ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or (ch >= '0' and ch <= '9') or ch == '_') {
                        self.pos += 1;
                    } else break;
                }
                const name = self.input[start..self.pos];
                return self.compiler.resolveRegister(name);
            }

            return error.InvalidValue;
        }

        fn emitBinOp(self: *ExprParser, op: Opcode, left: i32, right: i32) CompileError!i32 {
            const tmp = try self.compiler.resolveRegister(try self.compiler.nextTempRegister());
            try self.compiler.bytecode.append(self.compiler.allocator, @intFromEnum(op));
            try self.compiler.bytecode.append(self.compiler.allocator, tmp);
            try self.compiler.bytecode.append(self.compiler.allocator, left);
            try self.compiler.bytecode.append(self.compiler.allocator, right);
            return tmp;
        }
    };

    // ---- Node Compilers ----

    /// Compile an XML node based on its element name
    pub fn compileNode(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        if (node.node_type != .Element) return;
        const name = node.name orelse return;

        if (std.mem.eql(u8, name, "decoder")) {
            try self.compileChildren(node);
        } else if (std.mem.eql(u8, name, "set")) {
            try self.compileSet(node);
        } else if (std.mem.eql(u8, name, "loop")) {
            try self.compileLoop(node);
        } else if (std.mem.eql(u8, name, "read")) {
            try self.compileRead(node);
        } else if (std.mem.eql(u8, name, "seek")) {
            try self.compileSeek(node);
        } else if (std.mem.eql(u8, name, "sample")) {
            try self.emit0(.Sample);
        } else if (std.mem.eql(u8, name, "if")) {
            try self.compileIf(node);
        } else {
            return error.InvalidData;
        }
    }

    /// Compile all children of a node
    pub fn compileChildren(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        var child = node.child;
        while (child) |c| {
            try self.compileNode(c);
            child = c.next;
        }
    }

    /// Compile <set var="x" value="y"/>
    fn compileSet(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        const var_name = node.getAttribute("var") orelse return error.InvalidData;
        const value = node.getAttribute("value") orelse return error.InvalidData;
        try self.emitMr(var_name, value);
    }

    /// Compile <loop var="x" start="s" end="e" increment="i">...</loop>
    fn compileLoop(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        const var_name = node.getAttribute("var");
        const start = node.getAttribute("start");
        const end = node.getAttribute("end");
        const increment_s = node.getAttribute("increment");
        const increment = increment_s orelse "1";

        const const_increment_p = classifyValue(increment) == .Number;
        var const_increment: f64 = 0;
        var zero_increment_p = false;

        const loop_end = try self.nextLabel();
        const loop_up_start = try self.nextLabel();
        const loop_down_start = try self.nextLabel();

        if (var_name == null) {
            // Simple infinite loop
            self.emitLabel(loop_up_start);
            try self.compileChildren(node);
            try self.emitBranch(.Branch, loop_up_start);
            return;
        }

        const var_n = var_name.?;

        // Complex loop
        if (start) |s| {
            try self.emitMr(var_n, s);
        }

        if (const_increment_p) {
            const_increment = parseNumber(increment) orelse 0;
            if (const_increment == 0) zero_increment_p = true;
        } else {
            try self.emit2(.Cmp, increment, "0");
            try self.emitBranch(.BranchLt, loop_down_start);
        }

        // Upward counting loop
        if (!const_increment_p or const_increment >= 0) {
            self.emitLabel(loop_up_start);
            if (end) |e| {
                try self.emit2(.Cmp, var_n, e);
                try self.emitBranch(.BranchGe, loop_end);
            }
            try self.compileChildren(node);
            if (!zero_increment_p)
                try self.emit3(.Add, var_n, var_n, increment);
            try self.emitBranch(.Branch, loop_up_start);
        }

        // Downward counting loop
        if (!const_increment_p or const_increment < 0) {
            self.emitLabel(loop_down_start);
            if (end) |e| {
                try self.emit2(.Cmp, var_n, e);
                try self.emitBranch(.BranchLe, loop_end);
            }
            try self.compileChildren(node);
            if (!zero_increment_p)
                try self.emit3(.Add, var_n, var_n, increment);
            try self.emitBranch(.Branch, loop_down_start);
        }

        if (end != null)
            self.emitLabel(loop_end);
    }

    /// Compile <read var="x" bits="N" type="T" endian="E"/>
    fn compileRead(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        const var_name = node.getAttribute("var");
        const bits_s = node.getAttribute("bits");
        const octets_s = node.getAttribute("octets");
        const type_s = node.getAttribute("type") orelse "raw";
        const endian_s = node.getAttribute("endian");
        const value_s = node.getAttribute("value");

        var op: i32 = 1;

        if (bits_s != null and octets_s != null)
            return error.InvalidData;
        if (bits_s == null and octets_s == null)
            return error.InvalidData;

        // Raw read
        if (std.mem.eql(u8, type_s, "raw")) {
            const oc = octets_s orelse return error.InvalidData;
            try self.emit2(.ReadRaw, var_name orelse "_raw", oc);
            return;
        }

        const var_n = var_name orelse blk: {
            break :blk try self.nextTempRegister();
        };
        const saved = self.saveTempRegisters();
        defer self.restoreTempRegisters(saved);

        // Calculate bits
        var bits: i32 = 0;
        if (bits_s) |bs| {
            bits = std.fmt.parseInt(i32, bs, 10) catch return error.InvalidData;
        }
        if (octets_s) |os| {
            bits = (std.fmt.parseInt(i32, os, 10) catch return error.InvalidData) * 8;
        }

        switch (bits) {
            8 => {},
            16 => op += 1,
            32 => op += 2,
            64 => op += 3,
            else => return error.InvalidData,
        }

        // Endian
        if (endian_s) |endian| {
            if (std.mem.eql(u8, endian, "little")) {
                // op += 0; (little-endian is base)
            } else if (std.mem.eql(u8, endian, "big")) {
                op += 10;
            } else {
                return error.InvalidData;
            }
        } else {
            if (bits == 8) {
                op += 10; // default to big for 8-bit
            } else {
                return error.InvalidData;
            }
        }

        // Type
        if (std.mem.eql(u8, type_s, "uint")) {
            op += 0;
        } else if (std.mem.eql(u8, type_s, "int")) {
            op += 4;
        } else if (std.mem.eql(u8, type_s, "float")) {
            op += 6;
            if (bits != 32 and bits != 64) return error.InvalidData;
        } else {
            return error.InvalidData;
        }

        const opcode: Opcode = @enumFromInt(@as(u8, @intCast(op)));
        try self.emit1(opcode, var_n);
        if (value_s) |v| {
            try self.emit2(.Assert, var_n, v);
        }
    }

    /// Compile <seek offset="N" from="start|current|end"/>
    fn compileSeek(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        const offset = node.getAttribute("offset") orelse return error.InvalidData;
        const from = node.getAttribute("from") orelse return error.InvalidData;

        var whence: []const u8 = undefined;
        if (std.mem.eql(u8, from, "start")) {
            whence = "0";
        } else if (std.mem.eql(u8, from, "current")) {
            whence = "1";
        } else if (std.mem.eql(u8, from, "end")) {
            whence = "2";
        } else {
            return error.InvalidData;
        }

        try self.emit2(.Seek, offset, whence);
    }

    /// Compile <if condition="expr">...</if>
    fn compileIf(self: *Compiler, node: *xml_mod.Node) CompileError!void {
        const cond = node.getAttribute("condition") orelse return error.InvalidData;
        const skip = try self.nextLabel();
        try self.emit2(.Cmp, cond, "0");
        try self.emitBranch(.BranchEq, skip);
        try self.compileChildren(node);
        self.emitLabel(skip);
    }

    // ---- Expression Compilers ----

    /// Compile an expression node, returning the register name holding the result
    pub fn compileExprNode(self: *Compiler, return_reg: ?[]const u8, node: *xml_mod.Node) CompileError![]const u8 {
        if (node.node_type != .Element) return error.InvalidData;
        const name = node.name orelse return error.InvalidData;
        const saved = self.saveTempRegisters();
        defer self.restoreTempRegisters(saved);

        const ret = return_reg orelse try self.nextTempRegister();

        // Value reference: <v n="name"/>
        if (std.mem.eql(u8, name, "v")) {
            return node.getAttribute("n") orelse error.InvalidData;
        }

        // Binary operations
        if (std.mem.eql(u8, name, "+")) return self.compileBinaryExpr(.Add, ret, node);
        if (std.mem.eql(u8, name, "-")) return self.compileBinaryExpr(.Sub, ret, node);
        if (std.mem.eql(u8, name, "*")) return self.compileBinaryExpr(.Mul, ret, node);
        if (std.mem.eql(u8, name, "/")) return self.compileBinaryExpr(.Div, ret, node);
        if (std.mem.eql(u8, name, "&")) return self.compileBinaryExpr(.And, ret, node);
        if (std.mem.eql(u8, name, "|")) return self.compileBinaryExpr(.Or, ret, node);
        if (std.mem.eql(u8, name, "<<")) return self.compileBinaryExpr(.Lsl, ret, node);
        if (std.mem.eql(u8, name, ">>")) return self.compileBinaryExpr(.Lsr, ret, node);
        if (std.mem.eql(u8, name, "<")) return self.compileBinaryExpr(.Lt, ret, node);
        if (std.mem.eql(u8, name, "<=")) return self.compileBinaryExpr(.Le, ret, node);
        if (std.mem.eql(u8, name, ">")) return self.compileBinaryExpr(.Gt, ret, node);
        if (std.mem.eql(u8, name, ">=")) return self.compileBinaryExpr(.Ge, ret, node);
        if (std.mem.eql(u8, name, "==")) return self.compileBinaryExpr(.Eq, ret, node);
        if (std.mem.eql(u8, name, "!=")) return self.compileBinaryExpr(.Ne, ret, node);
        if (std.mem.eql(u8, name, "&&")) return self.compileBinaryExpr(.LAnd, ret, node);
        if (std.mem.eql(u8, name, "||")) return self.compileBinaryExpr(.LOr, ret, node);

        // Unary operations
        if (std.mem.eql(u8, name, "!")) return self.compileUnaryExpr(.LNot, ret, node);

        return error.InvalidData;
    }

    /// Compile a binary expression node
    fn compileBinaryExpr(self: *Compiler, op: Opcode, return_reg: []const u8, node: *xml_mod.Node) CompileError![]const u8 {
        const saved = self.saveTempRegisters();
        defer self.restoreTempRegisters(saved);
        const child1 = node.child orelse return error.InvalidData;
        const child2 = child1.next orelse return error.InvalidData;
        const a1 = try self.compileExprNode(try self.nextTempRegister(), child1);
        const a2 = try self.compileExprNode(try self.nextTempRegister(), child2);
        try self.emit3(op, return_reg, a1, a2);
        return return_reg;
    }

    /// Compile a unary expression node
    fn compileUnaryExpr(self: *Compiler, op: Opcode, return_reg: []const u8, node: *xml_mod.Node) CompileError![]const u8 {
        const child = node.child orelse return error.InvalidData;
        const a = try self.compileExprNode(null, child);
        try self.emit2(op, return_reg, a);
        return return_reg;
    }

    /// Compile a full decoder XML and return bytecode, registers, and register names
    pub fn compile(self: *Compiler, source: *xml_mod.Node) !CompileResult {
        try self.compileNode(source);
        self.fixupLabels();

        // Collect v-register mappings (v0, v1, v2, ...)
        var vs = std.ArrayList(i32){};
        defer vs.deinit(self.allocator);

        var v: usize = 0;
        var name_buf: [32]u8 = undefined;
        while (true) {
            const v_name = std.fmt.bufPrint(&name_buf, "v{d}", .{v}) catch break;
            if (self.register_names.intValue(v_name)) |v_reg| {
                try vs.append(self.allocator, @intCast(v_reg));
                v += 1;
            } else break;
        }

        // Copy bytecode
        const bytecode = try self.allocator.dupe(i32, self.bytecode.items);
        const registers = try self.allocator.dupe(f64, self.initial_registers.items);
        const vs_copy = try self.allocator.dupe(i32, vs.items);
        const names = try self.register_names.clone();

        return CompileResult{
            .bytecode = bytecode,
            .initial_registers = registers,
            .vs = vs_copy,
            .register_names = names,
            .num_bytecodes = self.bytecode.items.len,
            .num_registers = self.initial_registers.items.len,
        };
    }
};

/// Result of compilation
pub const CompileResult = struct {
    bytecode: []i32,
    initial_registers: []f64,
    vs: []i32,
    register_names: relation_mod.Relation,
    num_bytecodes: usize,
    num_registers: usize,

    pub fn deinit(self: *CompileResult, allocator: std.mem.Allocator) void {
        allocator.free(self.bytecode);
        allocator.free(self.initial_registers);
        allocator.free(self.vs);
        self.register_names.deinit();
    }
};

// ---- Tests ----

const testing = std.testing;

test "compiler value classification" {
    try testing.expectEqual(ValueType.Name, classifyValue("x").?);
    try testing.expectEqual(ValueType.Name, classifyValue("_temp").?);
    try testing.expectEqual(ValueType.Number, classifyValue("42").?);
    try testing.expectEqual(ValueType.Number, classifyValue("3.14").?);
    try testing.expectEqual(ValueType.Number, classifyValue("-1").?);
    try testing.expectEqual(ValueType.Expression, classifyValue("{x+1}").?);
    try testing.expect(classifyValue("") == null);
}

test "compiler register resolution" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    // First resolution creates register 0
    const r0 = try compiler.resolveRegister("x");
    try testing.expectEqual(@as(i32, 0), r0);
    try testing.expectEqual(@as(f64, 0.0), compiler.initial_registers.items[0]);

    // Same name returns same register
    const r0_again = try compiler.resolveRegister("x");
    try testing.expectEqual(r0, r0_again);

    // Different name creates register 1
    const r1 = try compiler.resolveRegister("y");
    try testing.expectEqual(@as(i32, 1), r1);

    // Number creates register with initial value
    const r2 = try compiler.resolveRegister("42");
    try testing.expectEqual(@as(i32, 2), r2);
    // Number should have created register with value 42
    // (actually stored under hex key, but value is 42.0)
    try testing.expectEqual(@as(f64, 42.0), compiler.initial_registers.items[2]);
}

test "compiler emit basic opcodes" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    try compiler.emit0(.Sample);
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.Sample)), compiler.bytecode.items[0]);

    try compiler.emit1(.ReadU32BE, "x");
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.ReadU32BE)), compiler.bytecode.items[1]);
    try testing.expectEqual(@as(i32, 0), compiler.bytecode.items[2]); // x = register 0

    try compiler.emit3(.Add, "result", "x", "y");
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.Add)), compiler.bytecode.items[3]);
    try testing.expectEqual(@as(i32, 1), compiler.bytecode.items[4]); // result = register 1
    try testing.expectEqual(@as(i32, 0), compiler.bytecode.items[5]); // x = register 0
    try testing.expectEqual(@as(i32, 2), compiler.bytecode.items[6]); // y = register 2
}

test "compiler label fixup" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    const label = try compiler.nextLabel();
    try compiler.emitBranch(.Branch, label);
    // Emit some code
    try compiler.emit0(.Sample);
    // Set label here
    compiler.emitLabel(label);

    compiler.fixupLabels();

    // Branch target should be relative offset: target - fixup_loc + 1
    // fixup_loc = 1 (second bytecode), target = 3 (after sample)
    // offset = 3 - 1 + 1 = 3
    try testing.expectEqual(@as(i32, 3), compiler.bytecode.items[1]);
}

test "compiler compile set node" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    // Build <set var="x" value="42"/>
    const node = try xml_mod.Node.newElement(allocator, "set");
    defer node.deinit();
    try node.setAttribute("var", "x");
    try node.setAttribute("value", "42");

    try compiler.compileNode(node);

    // Should emit MR x, 42
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.MoveReg)), compiler.bytecode.items[0]);
}

test "compiler compile sample node" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    const node = try xml_mod.Node.newElement(allocator, "sample");
    defer node.deinit();

    try compiler.compileNode(node);
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.Sample)), compiler.bytecode.items[0]);
    try testing.expectEqual(@as(usize, 1), compiler.bytecode.items.len);
}

test "compiler compile seek node" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    const node = try xml_mod.Node.newElement(allocator, "seek");
    defer node.deinit();
    try node.setAttribute("offset", "100");
    try node.setAttribute("from", "start");

    try compiler.compileNode(node);
    try testing.expectEqual(@as(i32, @intFromEnum(Opcode.Seek)), compiler.bytecode.items[0]);
}

test "compiler temp registers" {
    const allocator = testing.allocator;
    var compiler = Compiler.init(allocator);
    defer compiler.deinit();

    const r0 = try compiler.nextTempRegister();
    try testing.expect(std.mem.eql(u8, "r0", r0));
    const r1 = try compiler.nextTempRegister();
    try testing.expect(std.mem.eql(u8, "r1", r1));

    const saved = compiler.saveTempRegisters();
    _ = try compiler.nextTempRegister(); // r2
    compiler.restoreTempRegisters(saved);

    // After restore, next temp should reuse r2
    const r2_again = try compiler.nextTempRegister();
    try testing.expect(std.mem.eql(u8, "r2", r2_again));
}

test "compiler number register bytes" {
    var buf: [18]u8 = undefined;
    dumpRegisterBytes(0.0, &buf);
    // Should produce "0X" followed by hex digits
    try testing.expect(buf[0] == '0');
    try testing.expect(buf[1] == 'X');
}
