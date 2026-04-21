// Functional end-to-end tests
// Based on t_functional.c

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const File = libsie.File;
const Block = libsie.advanced.block;
const xml_mod = libsie.advanced.xml;
const compiler_mod = libsie.advanced.compiler;
const decoder_mod = libsie.advanced.decoder;

test "functional: open SIE file, read XML, parse" {
    var file = File.init(testing.allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer file.deinit();
    try file.open();

    try testing.expect(try file.isSie());

    // Read first block (XML)
    try file.seek(0);
    var blk = try file.readBlock();
    defer blk.deinit();

    try testing.expectEqual(@as(u32, Block.SIE_XML_GROUP), blk.group);

    // First block payload should start with <?xml
    const payload = blk.payload();
    try testing.expect(payload.len > 5);
    try testing.expect(std.mem.startsWith(u8, payload, "<?xml"));
}

test "functional: build index and count groups" {
    var file = File.init(testing.allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    const num_groups = file.numGroups();
    try testing.expect(num_groups >= 2);

    // XML group should have exactly 1 block
    if (file.groupIndex(0)) |xml_idx| {
        try testing.expect(xml_idx.numBlocks() >= 1);
    }
}

test "functional: read all blocks sequentially" {
    var file = File.init(testing.allocator, "test/data/sie_min_timhis_a_19EFAA61.sie");
    defer file.deinit();
    try file.open();

    var count: usize = 0;
    try file.seek(0);
    while (!file.isEof()) {
        var blk = file.readBlock() catch break;
        defer blk.deinit();
        try testing.expect(blk.payloadSize() > 0);
        count += 1;
        if (count > 1000) break; // safety limit
    }

    try testing.expect(count > 0);
}

test "functional: compile decoder from XML and run on data" {
    // This is the full pipeline test: XML → Compile → Decode → Output
    const xml_text =
        \\<decoder>
        \\  <loop>
        \\  <read var="v0" type="uint" bits="16" endian="little"/>
        \\  <read var="v1" type="int" bits="16" endian="little"/>
        \\  <sample/>
        \\  </loop>
        \\</decoder>
    ;

    var doc = try xml_mod.parseString(testing.allocator, xml_text);
    defer doc.deinit();

    var comp = compiler_mod.Compiler.init(testing.allocator);
    defer comp.deinit();

    var result = try comp.compile(doc);
    defer result.deinit(testing.allocator);

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

    var machine = try decoder_mod.DecoderMachine.init(testing.allocator, &decoder);
    defer machine.deinit();

    // 2 samples: (0x0100, 0xFF7F) and (0x0200, 0x0080)
    const data = [_]u8{ 0x00, 0x01, 0x7F, 0xFF, 0x00, 0x02, 0x80, 0x00 };
    machine.prep(&data);
    try machine.run();

    if (machine.output) |out| {
        try testing.expectEqual(@as(usize, 2), out.num_rows);
        try testing.expectEqual(@as(usize, 2), out.num_dims);

        // v0: uint16 LE: 0x0100=256, 0x0200=512
        try testing.expectEqual(@as(f64, 256.0), out.dimensions[0].float64_data.?[0]);
        try testing.expectEqual(@as(f64, 512.0), out.dimensions[0].float64_data.?[1]);

        // v1: int16 LE: 0xFF7F=-129, 0x0080=32768 doesn't fit i16...
        // Actually 0x7FFF=32767 as i16, 0x8000=-32768
        // Our data: bytes 7F FF in LE = 0xFF7F = -129 as i16
        //           bytes 80 00 in LE = 0x0080 = 128 as i16
        try testing.expectEqual(@as(f64, -129.0), out.dimensions[1].float64_data.?[0]);
        try testing.expectEqual(@as(f64, 128.0), out.dimensions[1].float64_data.?[1]);
    } else return error.TestUnexpectedResult;
}

test "functional: comprehensive file has multiple groups" {
    var file = File.init(testing.allocator, "test/data/sie_comprehensive_VBM_DE81A7BA.sie");
    defer file.deinit();
    try file.open();
    try file.buildIndex();

    try testing.expect(try file.isSie());

    const num_groups = file.numGroups();
    try testing.expect(num_groups >= 3);
    try testing.expect(file.highestGroup() >= 2);
}
