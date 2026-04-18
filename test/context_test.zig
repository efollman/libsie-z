// Context tests — ports of t_exception.c and t_progress.c
// Tests: exception handling, error contexts, cleanup stack, progress callbacks

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const Context = libsie.context.Context;
const ContextConfig = libsie.context.ContextConfig;
const ProgressCallbacks = libsie.context.ProgressCallbacks;

test "context: exception set and clear" {
    // Port of test_exception — basic exception lifecycle
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    try testing.expect(!ctx.hasException());

    try ctx.setException("Test error occurred");
    try testing.expect(ctx.hasException());

    const exc = ctx.getException().?;
    try testing.expectEqualStrings("Test error occurred", exc.message);

    ctx.clearException();
    try testing.expect(!ctx.hasException());
    try testing.expect(ctx.getException() == null);
}

test "context: exception overwrite" {
    // Port of test_exception_2 — setting multiple exceptions
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    try ctx.setException("First error");
    try testing.expect(ctx.hasException());

    // Setting a new exception replaces the old one
    try ctx.setException("Second error");
    try testing.expect(ctx.hasException());
    try testing.expectEqualStrings("Second error", ctx.getException().?.message);

    ctx.clearException();
    try testing.expect(!ctx.hasException());
}

test "context: cleanup stack basic" {
    // Port of test_exception_cleanup_deep concept — cleanup functions fire on pop
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var counter: usize = 0;

    const Fns = struct {
        fn increment(target: *anyopaque) void {
            const ptr: *usize = @ptrCast(@alignCast(target));
            ptr.* += 1;
        }
    };

    // Push cleanup
    try ctx.cleanupPush(Fns.increment, @ptrCast(&counter));

    try testing.expectEqual(@as(usize, 0), counter);

    // Pop with fire=true should invoke
    ctx.cleanupPop(true);
    try testing.expectEqual(@as(usize, 1), counter);
}

test "context: cleanup stack pop without fire" {
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var counter: usize = 0;
    const Fns = struct {
        fn increment(target: *anyopaque) void {
            const ptr: *usize = @ptrCast(@alignCast(target));
            ptr.* += 1;
        }
    };

    try ctx.cleanupPush(Fns.increment, @ptrCast(&counter));
    ctx.cleanupPop(false); // don't fire
    try testing.expectEqual(@as(usize, 0), counter);
}

test "context: cleanup stack mark and pop to mark" {
    // Port of test_exception_uwp_deep concept — mark/popMark
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var counter: usize = 0;
    const Fns = struct {
        fn increment(target: *anyopaque) void {
            const ptr: *usize = @ptrCast(@alignCast(target));
            ptr.* += 1;
        }
    };

    const mark = ctx.cleanupMark();

    // Push 100 cleanups
    var i: usize = 0;
    while (i < 100) : (i += 1) {
        try ctx.cleanupPush(Fns.increment, @ptrCast(&counter));
    }

    // Pop back to mark — should fire all 100
    ctx.cleanupPopMark(mark);
    try testing.expectEqual(@as(usize, 100), counter);
}

test "context: error context stack" {
    // Port of test_exception_report concept — error context push/pop
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    try testing.expectEqual(@as(usize, 0), ctx.errorContextDepth());

    try ctx.errorContextPush("Opening file");
    try testing.expectEqual(@as(usize, 1), ctx.errorContextDepth());

    try ctx.errorContextPush("Parsing header");
    try testing.expectEqual(@as(usize, 2), ctx.errorContextDepth());

    ctx.errorContextPop();
    try testing.expectEqual(@as(usize, 1), ctx.errorContextDepth());

    ctx.errorContextPop();
    try testing.expectEqual(@as(usize, 0), ctx.errorContextDepth());
}

test "context: string interning via context" {
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    const s1 = try ctx.internString("test_string");
    const s2 = try ctx.internString("test_string");

    // Same string → same pointer
    try testing.expect(s1.ptr == s2.ptr);
    try testing.expectEqualStrings("test_string", s1);

    // Different string → different pointer
    const s3 = try ctx.internString("other");
    try testing.expect(s3.ptr != s1.ptr);
}

test "context: recursion limit" {
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    ctx.max_recursion = 5;

    // Should succeed up to limit
    var i: i32 = 0;
    while (i < 5) : (i += 1) {
        try ctx.recursionEnter();
    }

    // Next should fail
    try testing.expectError(error.OperationFailed, ctx.recursionEnter());

    // Leave
    while (i > 0) : (i -= 1) {
        ctx.recursionLeave();
    }
}

test "context: progress message callback" {
    // Port of test_progress — progress callbacks receive calls
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var message_count: usize = 0;

    const Fns = struct {
        fn setMessage(data: ?*anyopaque, _: []const u8) bool {
            const count: *usize = @ptrCast(@alignCast(data.?));
            count.* += 1;
            return false; // don't abort
        }
    };

    // Without callbacks, should be fine
    ctx.progressMessage("test");
    try testing.expectEqual(@as(usize, 0), message_count);

    // Set callback
    ctx.setProgressCallbacks(.{
        .data = @ptrCast(&message_count),
        .set_message = Fns.setMessage,
    });

    ctx.progressMessage("Opening file...");
    try testing.expectEqual(@as(usize, 1), message_count);

    ctx.progressMessage("Reading data...");
    try testing.expectEqual(@as(usize, 2), message_count);

    // Clear callbacks
    ctx.setProgressCallbacks(.{});
    ctx.progressMessage("After clear");
    try testing.expectEqual(@as(usize, 2), message_count);
}

test "context: progress percent callback" {
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var percent_count: usize = 0;
    var last_percent: f64 = -1.0;

    const Fns = struct {
        fn percent(data: ?*anyopaque, pct: f64) bool {
            const state: *struct { count: *usize, last: *f64 } = @ptrCast(@alignCast(data.?));
            state.count.* += 1;
            state.last.* = pct;
            return false;
        }
    };

    var state = .{ .count = &percent_count, .last = &last_percent };

    ctx.setProgressCallbacks(.{
        .data = @ptrCast(&state),
        .percent = Fns.percent,
    });

    // Report via progressCount which converts to percent
    ctx.progressCount(0, 100);
    ctx.progressCount(50, 100);
    ctx.progressCount(100, 100);

    try testing.expect(percent_count > 0);
    try testing.expectApproxEqAbs(@as(f64, 100.0), last_percent, 0.1);
}

test "context: progress count callback" {
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    var count_calls: usize = 0;
    var last_done: u64 = 0;
    var last_total: u64 = 0;

    const Fns = struct {
        fn count(data: ?*anyopaque, done: u64, total: u64) bool {
            const state: *struct { calls: *usize, done: *u64, total: *u64 } = @ptrCast(@alignCast(data.?));
            state.calls.* += 1;
            state.done.* = done;
            state.total.* = total;
            return false;
        }
    };

    var state = .{ .calls = &count_calls, .done = &last_done, .total = &last_total };

    ctx.setProgressCallbacks(.{
        .data = @ptrCast(&state),
        .count = Fns.count,
    });

    ctx.progressCount(10, 200);
    try testing.expectEqual(@as(usize, 1), count_calls);
    try testing.expectEqual(@as(u64, 10), last_done);
    try testing.expectEqual(@as(u64, 200), last_total);

    ctx.progressCount(200, 200);
    try testing.expectEqual(@as(usize, 2), count_calls);
    try testing.expectEqual(@as(u64, 200), last_done);
}
