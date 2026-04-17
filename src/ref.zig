// Reference-counted polymorphic base class
// Replaces sie_ref.h

const std = @import("std");

pub const RefType = enum {
    Context,
    File,
    Stream,
    Test,
    Channel,
    Dimension,
    Tag,
    Group,
    Spigot,
    Iterator,
    Other,
};

/// Base reference type with reference counting
pub const Ref = struct {
    ref_count: std.atomic.Value(u32),
    ref_type: RefType,

    pub fn init(ref_type: RefType) Ref {
        return Ref{
            .ref_count = std.atomic.Value(u32).init(1),
            .ref_type = ref_type,
        };
    }

    pub fn retain(self: *Ref) *Ref {
        _ = self.ref_count.fetchAdd(1, .release);
        return self;
    }

    pub fn release(self: *Ref) u32 {
        return self.ref_count.fetchSub(1, .release);
    }

    pub fn refCount(self: *const Ref) u32 {
        return self.ref_count.load(.acquire);
    }
};

test "reference counting" {
    var ref = Ref.init(.Context);
    try std.testing.expectEqual(ref.refCount(), 1);

    _ = ref.retain();
    try std.testing.expectEqual(ref.refCount(), 2);

    _ = ref.release();
    try std.testing.expectEqual(ref.refCount(), 1);
}
