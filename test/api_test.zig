// API contract tests
// Based on t_api.c — cross-module integration tests

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

test "api: channel create and query" {
    const Channel = libsie.channel.Channel;
    var ch = Channel.init(testing.allocator, 42, "TestChannel");
    defer ch.deinit();

    try testing.expectEqual(@as(u32, 42), ch.getId());
    try testing.expect(std.mem.eql(u8, "TestChannel", ch.getName()));
    try testing.expectEqual(@as(usize, 0), ch.getNumDimensions());
}

test "api: channel with dimensions and tags" {
    const Channel = libsie.channel.Channel;
    const Dimension = libsie.dimension.Dimension;
    const Tag = libsie.tag.Tag;

    var ch = Channel.init(testing.allocator, 1, "Speed");
    defer ch.deinit();

    // Add dimensions
    var dim0 = Dimension.init(testing.allocator, "Time", 0, 0);
    defer dim0.deinit();
    try ch.addDimension(dim0);

    var dim1 = Dimension.init(testing.allocator, "Value", 1, 0);
    defer dim1.deinit();
    try ch.addDimension(dim1);

    try testing.expectEqual(@as(usize, 2), ch.getNumDimensions());

    // Add tags
    var t1 = try Tag.initString(testing.allocator, "unit", "km/h");
    defer t1.deinit();
    try ch.addTag(t1);

    const found = ch.findTag("unit");
    try testing.expect(found != null);
    if (found) |tag| {
        try testing.expect(std.mem.eql(u8, "km/h", tag.getString() orelse ""));
    }
}

test "api: output create and populate" {
    const Output = libsie.output.Output;

    var out = try Output.init(testing.allocator, 2);
    defer out.deinit();

    out.setType(0, .Float64);
    out.setType(1, .Float64);
    try out.resize(0, 10);
    try out.resize(1, 10);

    // Set values
    if (out.dimensions[0].float64_data) |data| {
        data[0] = 1.0;
        data[1] = 2.0;
        data[2] = 3.0;
    }
    if (out.dimensions[1].float64_data) |data| {
        data[0] = 10.0;
        data[1] = 20.0;
        data[2] = 30.0;
    }
    out.num_rows = 3;

    try testing.expectEqual(@as(usize, 2), out.num_dims);
    try testing.expectEqual(@as(usize, 3), out.num_rows);
    try testing.expectEqual(@as(f64, 2.0), out.getFloat64(0, 1).?);
    try testing.expectEqual(@as(f64, 30.0), out.getFloat64(1, 2).?);
}

test "api: block create and validate" {
    const block_mod = libsie.block;

    var blk = block_mod.Block.init(testing.allocator);
    defer blk.deinit();

    try blk.expand(100);
    try testing.expect(blk.max_size >= 100);

    // After expand, payload buffer exists but payload_size is still 0
    try testing.expectEqual(@as(u32, 0), blk.payload_size);
    blk.payload_size = 5;
    const payload = blk.getPayloadMut();
    @memcpy(payload[0..5], "hello");
}

test "api: xml parse and query" {
    const xml_mod = libsie.xml;

    const xml_text = "<root attr=\"value\"><child>text</child></root>";
    var doc = try xml_mod.parseString(testing.allocator, xml_text);
    defer doc.deinit();

    try testing.expect(doc.name != null);
    try testing.expect(std.mem.eql(u8, "root", doc.name.?));

    const attr = doc.getAttribute("attr");
    try testing.expect(attr != null);
    try testing.expect(std.mem.eql(u8, "value", attr.?));

    // Child element
    try testing.expect(doc.child != null);
    if (doc.child) |child| {
        try testing.expect(std.mem.eql(u8, "child", child.name.?));
    }
}

test "api: compiler compile simple decoder" {
    const compiler_mod = libsie.compiler;
    const decoder_mod = libsie.decoder;
    const xml_mod = libsie.xml;

    const xml_text = "<decoder><loop><read var=\"v0\" type=\"uint\" bits=\"8\"/><sample/></loop></decoder>";
    var doc = try xml_mod.parseString(testing.allocator, xml_text);
    defer doc.deinit();

    var comp = compiler_mod.Compiler.init(testing.allocator);
    defer comp.deinit();

    var result = try comp.compile(doc);
    defer result.deinit(testing.allocator);

    try testing.expect(result.bytecode.len > 0);
    try testing.expect(result.num_registers > 0);
    try testing.expectEqual(@as(usize, 1), result.vs.len); // v0

    // Create decoder from result
    const vs_usize = try testing.allocator.alloc(usize, result.vs.len);
    defer testing.allocator.free(vs_usize);
    for (result.vs, 0..) |v, i| vs_usize[i] = @intCast(v);

    var decoder = try decoder_mod.Decoder.init(
        testing.allocator,
        result.bytecode,
        vs_usize,
        result.num_registers,
        result.initial_registers,
    );
    defer decoder.deinit();

    // Run with some data
    var machine = try decoder_mod.DecoderMachine.init(testing.allocator, &decoder);
    defer machine.deinit();

    machine.prep(&[_]u8{ 0x0A, 0x14, 0x1E }); // 10, 20, 30
    try machine.run();

    // Should have output with 3 samples
    if (machine.output) |out| {
        try testing.expectEqual(@as(usize, 3), out.num_rows);
        try testing.expectEqual(@as(f64, 10.0), out.dimensions[0].float64_data.?[0]);
        try testing.expectEqual(@as(f64, 20.0), out.dimensions[0].float64_data.?[1]);
        try testing.expectEqual(@as(f64, 30.0), out.dimensions[0].float64_data.?[2]);
    } else return error.TestUnexpectedResult;
}

test "api: context lifecycle" {
    const Context = libsie.context.Context;
    var ctx = try Context.init(.{ .allocator = testing.allocator });
    defer ctx.deinit();

    // Intern strings
    const s1 = try ctx.internString("hello");
    const s2 = try ctx.internString("hello");
    try testing.expectEqual(s1.ptr, s2.ptr); // same pointer (interned)

    // Error context
    try ctx.errorContextPush("test error");
    try testing.expectEqual(@as(usize, 1), ctx.errorContextDepth());
    ctx.errorContextPop();
    try testing.expectEqual(@as(usize, 0), ctx.errorContextDepth());
}

test "api: relation create and query" {
    const Relation = libsie.relation.Relation;

    var rel = Relation.init(testing.allocator);
    defer rel.deinit();

    try rel.setValue("key1", "100");
    try rel.setValue("key2", "200");

    try testing.expectEqual(@as(?i64, 100), rel.intValue("key1"));
    try testing.expectEqual(@as(?i64, 200), rel.intValue("key2"));
    try testing.expectEqual(@as(?i64, null), rel.intValue("key3"));
}

test "api: stringtable interning" {
    const StringTable = libsie.stringtable.StringTable;

    var st = StringTable.init(testing.allocator);
    defer st.deinit();

    const a = try st.intern("hello");
    const b = try st.intern("hello");
    const c = try st.intern("world");

    try testing.expectEqual(a.ptr, b.ptr); // same pointer
    try testing.expect(a.ptr != c.ptr); // different strings
    try testing.expect(std.mem.eql(u8, "hello", a));
}
