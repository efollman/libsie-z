// LibSIE - Zig port of the SIE file reading library
// Removes dependencies on autotools and Apache Portable Runtime (APR)

pub const version = "1.1.6";

// Pull in the C ABI surface so that `export fn` symbols defined there are
// linked into the shared library. The `comptime` reference forces Zig to
// analyze every declaration in c_api.zig, which is what causes the
// `export fn` symbols to actually be emitted into the object file.
pub const c_api = @import("c_api.zig");
comptime {
    _ = c_api;
    @import("std").testing.refAllDecls(c_api);
}

// ── Stable Public API ──────────────────────────────────────────
//
// These are the recommended types for reading SIE files. They form
// a small, stable surface that is unlikely to change across versions.

pub const SieFile = @import("sie_file.zig").SieFile;
pub const Channel = @import("channel.zig").Channel;
pub const Test = @import("test.zig").Test;
pub const Dimension = @import("dimension.zig").Dimension;
pub const Tag = @import("tag.zig").Tag;
pub const Output = @import("output.zig").Output;
pub const output = @import("output.zig");
pub const ChannelSpigot = @import("channel_spigot.zig").ChannelSpigot;
pub const GroupSpigot = @import("group_spigot.zig").GroupSpigot;
pub const Block = @import("block.zig").Block;
pub const File = @import("file.zig").File;
pub const FileStream = @import("file_stream.zig").FileStream;
pub const Stream = @import("stream.zig").Stream;
pub const Group = @import("group.zig").Group;
pub const Error = @import("error.zig").Error;
pub const Exception = @import("error.zig").Exception;
pub const Types = @import("types.zig").Types;
pub const Context = @import("context.zig").Context;
pub const Spigot = @import("spigot.zig").Spigot;
pub const Intake = @import("intake.zig").Intake;

// ── Advanced / Internal Modules ────────────────────────────────
//
// Available for advanced use cases (custom pipelines, recovery,
// compiler, etc.). Not part of the stable API surface — these may
// change between versions without notice.

pub const advanced = struct {
    pub const types = @import("types.zig");
    pub const config = @import("config.zig");
    pub const byteswap = @import("byteswap.zig");
    pub const vec = @import("vec.zig");
    pub const stringtable = @import("stringtable.zig");
    pub const uthash = @import("uthash.zig");
    pub const utils = @import("utils.zig");
    pub const err = @import("error.zig");
    pub const ref = @import("ref.zig");
    pub const object = @import("object.zig");
    pub const context = @import("context.zig");
    pub const parser = @import("parser.zig");
    pub const decoder = @import("decoder.zig");
    pub const combiner = @import("combiner.zig");
    pub const transform = @import("transform.zig");
    pub const histogram = @import("histogram.zig");
    pub const xml = @import("xml.zig");
    pub const xml_merge = @import("xml_merge.zig");
    pub const relation = @import("relation.zig");
    pub const iterator = @import("iterator.zig");
    pub const recover = @import("recover.zig");
    pub const writer = @import("writer.zig");
    pub const plot_crusher = @import("plot_crusher.zig");
    pub const compiler = @import("compiler.zig");
    pub const sifter = @import("sifter.zig");
    pub const output = @import("output.zig");
    pub const spigot = @import("spigot.zig");
    pub const block = @import("block.zig");
    pub const file = @import("file.zig");
    pub const file_stream = @import("file_stream.zig");
    pub const stream = @import("stream.zig");
    pub const channel = @import("channel.zig");
    pub const test_ = @import("test.zig");
    pub const dimension = @import("dimension.zig");
    pub const tag = @import("tag.zig");
    pub const group = @import("group.zig");
    pub const intake = @import("intake.zig");
    pub const channel_spigot = @import("channel_spigot.zig");
    pub const group_spigot = @import("group_spigot.zig");
    pub const sie_file = @import("sie_file.zig");
};

test {
    @import("std").testing.refAllDeclsRecursive(@This());
}
