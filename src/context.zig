// Library context - central hub for memory, error handling, and state
// Replaces sie_context.h
//
// The Context is the root object for all libsie operations. It owns:
// - Allocator for all memory
// - String table for interning
// - Exception state
// - Cleanup stack (for RAII-like resource cleanup)
// - Error context stack (for nested error messages)
// - Progress callbacks
// - Reference tracking (leak detection)

const std = @import("std");
const stringtable_mod = @import("stringtable.zig");
const error_mod = @import("error.zig");

/// Cleanup function type
pub const CleanupFn = *const fn (target: *anyopaque) void;

/// Cleanup stack entry
pub const CleanupEntry = struct {
    func: CleanupFn,
    target: *anyopaque,
};

/// Error context entry for nested error messages
pub const ErrorContext = struct {
    message: []const u8,
    owned: bool,
};

/// Progress callback types
pub const ProgressSetMessage = *const fn (data: ?*anyopaque, message: []const u8) bool;
pub const ProgressPercent = *const fn (data: ?*anyopaque, percent: f64) bool;
pub const ProgressCount = *const fn (data: ?*anyopaque, done: u64, total: u64) bool;

/// Progress callback configuration
pub const ProgressCallbacks = struct {
    data: ?*anyopaque = null,
    set_message: ?ProgressSetMessage = null,
    percent: ?ProgressPercent = null,
    count: ?ProgressCount = null,
};

/// Context configuration
pub const ContextConfig = struct {
    allocator: std.mem.Allocator,
    track_references: bool = true,
    max_open_files: usize = 100,
    debug_level: i32 = 0,
};

/// Main library context
pub const Context = struct {
    allocator: std.mem.Allocator,
    string_table: stringtable_mod.StringTable,
    exception: ?error_mod.Exception = null,
    ref_count: u32 = 1,
    tracked_refs: std.AutoHashMap(usize, u32) = undefined,
    track_references: bool,

    // Cleanup stack
    cleanup_stack: std.ArrayList(CleanupEntry),

    // Error context stack
    error_contexts: std.ArrayList(ErrorContext),

    // Progress
    progress: ProgressCallbacks = .{},
    progress_enabled: bool = false,
    progress_percent_last: i32 = -1,

    // Debug
    debug_level: i32 = 0,

    // Recursion limit
    recursion_count: i32 = 0,
    max_recursion: i32 = 100,

    // Counters
    num_inits: u32 = 0,
    num_destroys: u32 = 0,

    // Trailing garbage tolerance
    ignore_trailing_garbage: usize = 0,

    /// Initialize a new context
    pub fn init(config: ContextConfig) !Context {
        const context = Context{
            .allocator = config.allocator,
            .string_table = stringtable_mod.StringTable.init(config.allocator),
            .track_references = config.track_references,
            .tracked_refs = std.AutoHashMap(usize, u32).init(config.allocator),
            .cleanup_stack = .{},
            .error_contexts = .{},
            .debug_level = config.debug_level,
        };
        return context;
    }

    /// Clean up context and check for memory leaks
    pub fn deinit(self: *Context) void {
        // Fire remaining cleanup entries
        self.cleanupPopAll();

        // Free error contexts
        for (self.error_contexts.items) |entry| {
            if (entry.owned) {
                self.allocator.free(entry.message);
            }
        }
        self.error_contexts.deinit(self.allocator);
        self.cleanup_stack.deinit(self.allocator);

        if (self.exception) |*exc| {
            exc.deinit(self.allocator);
        }

        if (self.track_references) {
            var iter = self.tracked_refs.iterator();
            while (iter.next()) |entry| {
                if (entry.value_ptr.* > 0) {
                    // Leak detected
                }
            }
        }

        self.tracked_refs.deinit();
        self.string_table.deinit();
    }

    // --- Exception handling ---

    /// Record an exception
    pub fn setException(self: *Context, message: []const u8) !void {
        if (self.exception) |*exc| {
            exc.deinit(self.allocator);
        }
        self.exception = try error_mod.Exception.init(self.allocator, message);
    }

    /// Check if an exception occurred
    pub fn hasException(self: *const Context) bool {
        return self.exception != null;
    }

    /// Get current exception (if any)
    pub fn getException(self: *const Context) ?error_mod.Exception {
        return self.exception;
    }

    /// Clear exception
    pub fn clearException(self: *Context) void {
        if (self.exception) |*exc| {
            exc.deinit(self.allocator);
            self.exception = null;
        }
    }

    // --- String interning ---

    /// Intern a string using the context's string table
    pub fn internString(self: *Context, str: []const u8) ![]const u8 {
        return try self.string_table.intern(str);
    }

    // --- Reference tracking ---

    /// Track a reference for leak detection
    pub fn trackRef(self: *Context, ref_id: usize) !void {
        if (!self.track_references) return;

        const entry = try self.tracked_refs.getOrPut(ref_id);
        if (entry.found_existing) {
            entry.value_ptr.* += 1;
        } else {
            entry.value_ptr.* = 1;
        }
    }

    /// Untrack a reference
    pub fn untrackRef(self: *Context, ref_id: usize) void {
        if (!self.track_references) return;
        if (self.tracked_refs.get(ref_id)) |count| {
            if (count > 0) {
                _ = self.tracked_refs.put(ref_id, count - 1) catch {};
            }
        }
    }

    // --- Cleanup stack ---

    /// Get current cleanup stack position (for later pop_mark)
    pub fn cleanupMark(self: *const Context) usize {
        return self.cleanup_stack.items.len;
    }

    /// Push a cleanup function onto the stack
    pub fn cleanupPush(self: *Context, func: CleanupFn, target: *anyopaque) !void {
        try self.cleanup_stack.append(self.allocator, .{ .func = func, .target = target });
    }

    /// Pop the top cleanup entry, optionally firing it
    pub fn cleanupPop(self: *Context, fire: bool) void {
        if (self.cleanup_stack.pop()) |entry| {
            if (fire) {
                entry.func(entry.target);
            }
        }
    }

    /// Pop all cleanup entries down to a mark, firing each one
    pub fn cleanupPopMark(self: *Context, mark: usize) void {
        while (self.cleanup_stack.items.len > mark) {
            if (self.cleanup_stack.pop()) |entry| {
                entry.func(entry.target);
            }
        }
    }

    /// Pop all remaining cleanup entries, firing each one
    pub fn cleanupPopAll(self: *Context) void {
        self.cleanupPopMark(0);
    }

    // --- Error context stack ---

    /// Push an error context message
    pub fn errorContextPush(self: *Context, message: []const u8) !void {
        const owned_msg = try self.allocator.dupe(u8, message);
        try self.error_contexts.append(self.allocator, .{ .message = owned_msg, .owned = true });
    }

    /// Pop the top error context
    pub fn errorContextPop(self: *Context) void {
        if (self.error_contexts.pop()) |entry| {
            if (entry.owned) {
                self.allocator.free(entry.message);
            }
        }
    }

    /// Get the current error context stack depth
    pub fn errorContextDepth(self: *const Context) usize {
        return self.error_contexts.items.len;
    }

    // --- Progress callbacks ---

    /// Set progress callbacks
    pub fn setProgressCallbacks(self: *Context, callbacks: ProgressCallbacks) void {
        self.progress = callbacks;
        self.progress_enabled = callbacks.set_message != null or
            callbacks.percent != null or
            callbacks.count != null;
        self.progress_percent_last = -1;
    }

    /// Report progress message
    pub fn progressMessage(self: *Context, message: []const u8) void {
        if (self.progress_enabled) {
            if (self.progress.set_message) |cb| {
                _ = cb(self.progress.data, message);
            }
        }
    }

    /// Report progress as count
    pub fn progressCount(self: *Context, done: u64, total: u64) void {
        if (self.progress_enabled) {
            if (self.progress.count) |cb| {
                _ = cb(self.progress.data, done, total);
            } else if (self.progress.percent) |cb| {
                if (total > 0) {
                    const percent = @as(f64, @floatFromInt(done)) / @as(f64, @floatFromInt(total)) * 100.0;
                    const int_percent: i32 = @intFromFloat(percent);
                    if (int_percent != self.progress_percent_last) {
                        self.progress_percent_last = int_percent;
                        _ = cb(self.progress.data, percent);
                    }
                }
            }
        }
    }

    // --- Recursion limit ---

    /// Check and increment recursion counter
    pub fn recursionEnter(self: *Context) !void {
        self.recursion_count += 1;
        if (self.recursion_count > self.max_recursion) {
            return error_mod.Error.OperationFailed;
        }
    }

    /// Decrement recursion counter
    pub fn recursionLeave(self: *Context) void {
        if (self.recursion_count > 0) {
            self.recursion_count -= 1;
        }
    }

    // --- Trailing garbage ---

    /// Set the amount of trailing garbage to tolerate
    pub fn setIgnoreTrailingGarbage(self: *Context, amount: usize) void {
        self.ignore_trailing_garbage = amount;
    }
};

test "context initialization" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ctx = try Context.init(.{
        .allocator = allocator,
        .track_references = true,
    });
    defer ctx.deinit();

    try std.testing.expect(!ctx.hasException());
}

test "context exception handling" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ctx = try Context.init(.{ .allocator = allocator });
    defer ctx.deinit();

    try ctx.setException("Test error");
    try std.testing.expect(ctx.hasException());

    ctx.clearException();
    try std.testing.expect(!ctx.hasException());
}

test "context string interning" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ctx = try Context.init(.{ .allocator = allocator });
    defer ctx.deinit();

    const str1 = try ctx.internString("hello");
    const str2 = try ctx.internString("hello");

    try std.testing.expectEqual(str1.ptr, str2.ptr);
}

test "context cleanup stack" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ctx = try Context.init(.{ .allocator = allocator });
    defer ctx.deinit();

    var counter: u32 = 0;

    const S = struct {
        fn cleanup(target: *anyopaque) void {
            const p: *u32 = @ptrCast(@alignCast(target));
            p.* += 1;
        }
    };

    const mark = ctx.cleanupMark();
    try ctx.cleanupPush(S.cleanup, @ptrCast(&counter));
    try ctx.cleanupPush(S.cleanup, @ptrCast(&counter));

    try std.testing.expectEqual(@as(usize, 2), ctx.cleanup_stack.items.len);

    ctx.cleanupPopMark(mark);
    try std.testing.expectEqual(@as(u32, 2), counter);
    try std.testing.expectEqual(@as(usize, 0), ctx.cleanup_stack.items.len);
}

test "context error context stack" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var ctx = try Context.init(.{ .allocator = allocator });
    defer ctx.deinit();

    try ctx.errorContextPush("reading file");
    try ctx.errorContextPush("parsing block");

    try std.testing.expectEqual(@as(usize, 2), ctx.errorContextDepth());

    ctx.errorContextPop();
    try std.testing.expectEqual(@as(usize, 1), ctx.errorContextDepth());
}
