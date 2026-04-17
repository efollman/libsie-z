// LibSIE - Zig port of the SIE file reading library
// Removes dependencies on autotools and Apache Portable Runtime (APR)

pub const version = "1.1.6";

// Foundation modules
pub const types = @import("types.zig");
pub const config = @import("config.zig");
pub const byteswap = @import("byteswap.zig");
pub const vec = @import("vec.zig");
pub const stringtable = @import("stringtable.zig");
pub const uthash = @import("uthash.zig");
pub const utils = @import("utils.zig");
pub const err = @import("error.zig");

// Object system
pub const ref = @import("ref.zig");
pub const object = @import("object.zig");
pub const context = @import("context.zig");

// Data structures
pub const file = @import("file.zig");
pub const stream = @import("stream.zig");
pub const test_ = @import("test.zig");
pub const channel = @import("channel.zig");
pub const dimension = @import("dimension.zig");
pub const tag = @import("tag.zig");
pub const group = @import("group.zig");
pub const intake = @import("intake.zig");

// I/O and data access
pub const block = @import("block.zig");
pub const parser = @import("parser.zig");
pub const spigot = @import("spigot.zig");
pub const output = @import("output.zig");

// Data pipeline
pub const decoder = @import("decoder.zig");
pub const combiner = @import("combiner.zig");
pub const transform = @import("transform.zig");
pub const histogram = @import("histogram.zig");
pub const xml = @import("xml.zig");
pub const xml_merge = @import("xml_merge.zig");
pub const recover = @import("recover.zig");
pub const relation = @import("relation.zig");
pub const iterator = @import("iterator.zig");

// Higher-level features
pub const writer = @import("writer.zig");
pub const plot_crusher = @import("plot_crusher.zig");
pub const compiler = @import("compiler.zig");
pub const sifter = @import("sifter.zig");

// High-level API
pub const sie_file = @import("sie_file.zig");
pub const group_spigot = @import("group_spigot.zig");
pub const channel_spigot = @import("channel_spigot.zig");

// Re-export commonly used types
pub const Types = types.Types;
pub const Error = err.Error;
pub const Exception = err.Exception;
pub const Context = context.Context;
pub const File = file.File;
pub const Stream = stream.Stream;
pub const Channel = channel.Channel;
pub const Test = test_.Test;
pub const Dimension = dimension.Dimension;
pub const Tag = tag.Tag;
pub const Spigot = spigot.Spigot;
pub const Output = output.Output;
pub const Block = block.Block;
pub const Group = group.Group;
pub const Intake = intake.Intake;
pub const Object = object.Object;
pub const SieFile = sie_file.SieFile;
pub const GroupSpigot = group_spigot.GroupSpigot;
pub const ChannelSpigot = channel_spigot.ChannelSpigot;

test {
    @import("std").testing.refAllDeclsRecursive(@This());
}
