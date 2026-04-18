// Sifter - subset extraction with ID remapping
// Replaces sie_sifter.h / sifter.c
//
// The Sifter extracts a subset of channels/tests from one or more SIE
// files and writes them to a new file via Writer, remapping all IDs
// (groups, tests, channels, decoders) to be contiguous in the output.
//
// Uses hash maps to track old→new ID mappings across 6 categories:
// Group, Test, Channel, Decoder, TestBySig, DecoderBySig.

const std = @import("std");
const writer_mod = @import("writer.zig");
const xml_mod = @import("xml.zig");
const file_mod = @import("file.zig");
const sie_file_mod = @import("sie_file.zig");
const channel_mod = @import("channel.zig");
const block_mod = @import("block.zig");

/// Map type categories for ID remapping
pub const MapType = enum(u3) {
    Group = 0,
    Test = 1,
    Channel = 2,
    Decoder = 3,
    TestBySig = 4,
    DecoderBySig = 5,

    pub const NUM_TYPES = 6;
};

/// Standard key for lookup: (intake_id, from_id) pair
pub const MapKey = struct {
    intake_id: u32,
    from_id: u32,
};

/// Map entry: maps an old key to a new ID
pub const MapEntry = struct {
    key: MapKey,
    id: u32,
    start_block: u64,
    end_block: u64,

    /// Pointer to arbitrary key bytes for sig-based lookups
    sig_key: ?[]const u8 = null,
};

/// Remapping rule: element/attribute combinations that need ID remapping
pub const RemapRule = struct {
    element: []const u8,
    attribute: []const u8,
    map_type: MapType,
};

/// Static remapping table matching the C code
pub const remapping_rules = [_]RemapRule{
    .{ .element = "ch", .attribute = "base", .map_type = .Channel },
    .{ .element = "ch", .attribute = "group", .map_type = .Group },
    .{ .element = "ch", .attribute = "id", .map_type = .Channel },
    .{ .element = "data", .attribute = "decoder", .map_type = .Decoder },
    .{ .element = "decoder", .attribute = "id", .map_type = .Decoder },
    .{ .element = "dim", .attribute = "group", .map_type = .Group },
    .{ .element = "tag", .attribute = "decoder", .map_type = .Decoder },
    .{ .element = "tag", .attribute = "group", .map_type = .Group },
    .{ .element = "xform", .attribute = "index_ch", .map_type = .Channel },
};

/// Null ID sentinel
pub const NULL_ID: u32 = 0xFFFFFFFF;

/// Sifter - extracts and remaps subsets of SIE data
pub const Sifter = struct {
    allocator: std.mem.Allocator,
    writer: *writer_mod.Writer,

    /// Hash maps for each MapType: key -> MapEntry
    maps: [MapType.NUM_TYPES]std.AutoHashMap(u64, MapEntry),

    /// Signature-based maps use byte slice keys
    sig_maps: [MapType.NUM_TYPES]std.StringHashMap(MapEntry),

    pub fn init(allocator: std.mem.Allocator, writer: *writer_mod.Writer) Sifter {
        var maps: [MapType.NUM_TYPES]std.AutoHashMap(u64, MapEntry) = undefined;
        var sig_maps: [MapType.NUM_TYPES]std.StringHashMap(MapEntry) = undefined;
        for (0..MapType.NUM_TYPES) |i| {
            maps[i] = std.AutoHashMap(u64, MapEntry).init(allocator);
            sig_maps[i] = std.StringHashMap(MapEntry).init(allocator);
        }
        return Sifter{
            .allocator = allocator,
            .writer = writer,
            .maps = maps,
            .sig_maps = sig_maps,
        };
    }

    pub fn deinit(self: *Sifter) void {
        for (0..MapType.NUM_TYPES) |i| {
            // Free owned sig keys
            var sig_iter = self.sig_maps[i].iterator();
            while (sig_iter.next()) |entry| {
                if (entry.value_ptr.sig_key) |k| {
                    self.allocator.free(k);
                }
            }
            self.sig_maps[i].deinit();
            self.maps[i].deinit();
        }
    }

    /// Compute a combined hash key from intake_id and from_id
    fn combineKey(key: MapKey) u64 {
        return (@as(u64, key.intake_id) << 32) | @as(u64, key.from_id);
    }

    /// Find an entry by standard key
    pub fn findId(self: *const Sifter, map_type: MapType, intake_id: u32, from_id: u32) ?u32 {
        const key = combineKey(.{ .intake_id = intake_id, .from_id = from_id });
        if (self.maps[@intFromEnum(map_type)].get(key)) |entry| {
            return entry.id;
        }
        return null;
    }

    /// Find an entry by signature key
    pub fn findIdBySig(self: *const Sifter, map_type: MapType, sig: []const u8) ?u32 {
        if (self.sig_maps[@intFromEnum(map_type)].get(sig)) |entry| {
            return entry.id;
        }
        return null;
    }

    /// Map (get or create) an ID by standard key
    pub fn mapId(self: *Sifter, map_type: MapType, intake_id: u32, from_id: u32) !u32 {
        if (self.findId(map_type, intake_id, from_id)) |id| {
            return id;
        }
        const new_id = self.nextId(map_type);
        try self.setId(map_type, intake_id, from_id, new_id);
        return new_id;
    }

    /// Map (get or create) an ID by signature
    pub fn mapIdBySig(self: *Sifter, map_type: MapType, sig: []const u8) !u32 {
        if (self.findIdBySig(map_type, sig)) |id| {
            return id;
        }
        const new_id = self.nextId(map_type);
        const owned_sig = try self.allocator.dupe(u8, sig);
        try self.sig_maps[@intFromEnum(map_type)].put(owned_sig, MapEntry{
            .key = .{ .intake_id = 0, .from_id = 0 },
            .id = new_id,
            .start_block = std.math.maxInt(u64),
            .end_block = 0,
            .sig_key = owned_sig,
        });
        return new_id;
    }

    /// Set an ID mapping
    pub fn setId(self: *Sifter, map_type: MapType, intake_id: u32, from_id: u32, to_id: u32) !void {
        const key = combineKey(.{ .intake_id = intake_id, .from_id = from_id });
        try self.maps[@intFromEnum(map_type)].put(key, MapEntry{
            .key = .{ .intake_id = intake_id, .from_id = from_id },
            .id = to_id,
            .start_block = std.math.maxInt(u64),
            .end_block = 0,
        });
    }

    /// Map a group ID with block range tracking
    pub fn mapIdGroup(self: *Sifter, intake_id: u32, from_id: u32, start_block: u64, end_block: u64) !u32 {
        const key = combineKey(.{ .intake_id = intake_id, .from_id = from_id });
        const mi = @intFromEnum(MapType.Group);
        if (self.maps[mi].getPtr(key)) |entry| {
            if (start_block < entry.start_block) entry.start_block = start_block;
            if (end_block > entry.end_block) entry.end_block = end_block;
            return entry.id;
        }
        const new_id = self.nextId(.Group);
        try self.maps[mi].put(key, MapEntry{
            .key = .{ .intake_id = intake_id, .from_id = from_id },
            .id = new_id,
            .start_block = start_block,
            .end_block = end_block,
        });
        return new_id;
    }

    /// Get next ID for a map type from the writer
    fn nextId(self: *Sifter, map_type: MapType) u32 {
        return switch (map_type) {
            .Group => self.writer.nextId(.Group),
            .TestBySig => self.writer.nextId(.Test),
            .Channel => self.writer.nextId(.Channel),
            .DecoderBySig => self.writer.nextId(.Decoder),
            else => 0, // Test, Decoder use indirect mapping
        };
    }

    /// Remap XML attributes according to remapping rules.
    /// Walks the XML tree and replaces old IDs with new mapped IDs.
    pub fn remapXml(self: *const Sifter, intake_id: u32, node: *xml_mod.Node) !void {
        var cur: ?*xml_mod.Node = node;
        while (cur) |n| {
            if (n.node_type == .Element) {
                const el_name = n.name orelse {
                    cur = walkNext(n, node);
                    continue;
                };
                for (n.attrs.items) |*attr| {
                    for (remapping_rules) |rule| {
                        if (std.mem.eql(u8, rule.element, el_name) and
                            std.mem.eql(u8, rule.attribute, attr.name))
                        {
                            // Parse old ID from attribute value
                            const from_id = std.fmt.parseInt(u32, attr.value, 10) catch continue;
                            // Look up new ID
                            if (self.findId(rule.map_type, intake_id, from_id)) |new_id| {
                                // Replace attribute value with new ID
                                var buf: [20]u8 = undefined;
                                const new_val = std.fmt.bufPrint(&buf, "{d}", .{new_id}) catch continue;
                                n.allocator.free(attr.value);
                                attr.value = n.allocator.dupe(u8, new_val) catch continue;
                            }
                        }
                    }
                }
            }
            cur = walkNext(n, node);
        }
    }

    /// Get total number of mapped entries across all map types
    pub fn totalEntries(self: *const Sifter) usize {
        var total: usize = 0;
        for (0..MapType.NUM_TYPES) |i| {
            total += self.maps[i].count();
            total += self.sig_maps[i].count();
        }
        return total;
    }

    // ── High-level pipeline: add channel and write data ──

    /// Add a channel to the sifter output, mapping all referenced IDs
    /// and writing the remapped channel XML definition to the writer.
    pub fn addChannel(
        self: *Sifter,
        sie_file: *sie_file_mod.SieFile,
        channel: *const channel_mod.Channel,
        start_block: u64,
        end_block: u64,
    ) !void {
        const intake_id: u32 = 0;

        // Already mapped — skip
        if (self.findId(.Channel, intake_id, channel.id) != null) return;

        // Look up the channel's raw XML node from the definition
        const ch_node = sie_file.xml_def.getChannel(@intCast(channel.id));

        // Handle base channel (recursively add the base first)
        if (ch_node) |node| {
            if (node.getAttribute("base")) |base_str| {
                const base_id = std.fmt.parseInt(u32, base_str, 10) catch null;
                if (base_id) |bid| {
                    if (sie_file.findChannel(bid)) |base_ch| {
                        try self.addChannel(sie_file, base_ch, 0, std.math.maxInt(u64));
                    }
                }
            }
        }

        // Map toplevel group with block range
        if (channel.toplevel_group != 0) {
            _ = try self.mapIdGroup(intake_id, channel.toplevel_group, start_block, end_block);
        }

        // Map channel tag groups
        for (channel.getTags()) |t| {
            if (t.group != 0) {
                _ = try self.mapId(.Group, intake_id, t.group);
            }
        }

        // Map dimension groups, decoders, and tag groups
        for (channel.getDimensions()) |dim| {
            if (dim.group != 0) {
                _ = try self.mapIdGroup(intake_id, dim.group, start_block, end_block);
            }
            if (dim.decoder_id != 0) {
                try self.mapDecoderId(sie_file, intake_id, dim.decoder_id);
            }
            for (dim.getTags()) |t| {
                if (t.group != 0) {
                    _ = try self.mapId(.Group, intake_id, t.group);
                }
            }
        }

        // Map the channel ID itself
        _ = try self.mapId(.Channel, intake_id, channel.id);

        // Clone channel XML, remap IDs, and write to output
        if (ch_node) |node| {
            const copy = try node.clone(self.allocator);
            errdefer copy.deinit();

            try self.remapXml(intake_id, copy);

            // Map containing test and set test attribute
            if (channel.test_id != 0) {
                const new_test_id = try self.mapTestId(intake_id, channel.test_id);
                var buf: [20]u8 = undefined;
                const test_str = std.fmt.bufPrint(&buf, "{d}", .{new_test_id}) catch unreachable;
                try copy.setAttribute("test", test_str);
            }

            try self.writer.xmlNode(copy);
            copy.deinit();
        }
    }

    /// Flush XML and copy all mapped group data blocks through the writer.
    pub fn finish(self: *Sifter, file: *file_mod.File) !void {
        self.writer.flushXml();

        const gi = @intFromEnum(MapType.Group);
        var iter = self.maps[gi].iterator();
        while (iter.next()) |entry| {
            const from_group = entry.value_ptr.key.from_id;
            const new_group = entry.value_ptr.id;
            const group_idx = file.getGroupIndex(from_group) orelse continue;
            const num_blocks = group_idx.entries.items.len;
            const end = if (entry.value_ptr.end_block < num_blocks)
                entry.value_ptr.end_block
            else
                num_blocks;
            var block_idx = entry.value_ptr.start_block;

            while (block_idx < end) : (block_idx += 1) {
                const file_entry = group_idx.entries.items[block_idx];
                var blk = try file.readBlockAt(@intCast(file_entry.offset));
                defer blk.deinit();
                try self.writer.writeBlock(new_group, blk.getPayload());
            }
        }
    }

    /// Calculate the total output size including pending XML and all
    /// mapped group data. Matches the C sie_sifter_total_size semantics.
    pub fn sifterTotalSize(self: *const Sifter, file: *file_mod.File) u64 {
        var num_blocks: u64 = 0;
        var num_bytes: u64 = 0;

        const gi = @intFromEnum(MapType.Group);
        var iter = self.maps[gi].iterator();
        while (iter.next()) |entry| {
            const from_group = entry.value_ptr.key.from_id;
            const group_idx = file.getGroupIndex(from_group) orelse continue;
            const nb = group_idx.entries.items.len;
            const start = entry.value_ptr.start_block;
            const end = if (entry.value_ptr.end_block < nb)
                entry.value_ptr.end_block
            else
                nb;

            if (start > 0 or end < nb) {
                num_blocks += end - start;
                var i = start;
                while (i < end) : (i += 1) {
                    num_bytes += group_idx.entries.items[i].size;
                }
            } else {
                num_blocks += nb;
                num_bytes += group_idx.getNumBytes();
            }
        }

        return self.writer.totalSize(num_bytes, num_blocks);
    }

    /// Map a decoder ID, writing the decoder XML definition on first encounter.
    fn mapDecoderId(self: *Sifter, sie_file: *sie_file_mod.SieFile, intake_id: u32, from_id: u32) !void {
        if (self.findId(.Decoder, intake_id, from_id) != null) return;

        const new_id = self.writer.nextId(.Decoder);
        try self.setId(.Decoder, intake_id, from_id, new_id);

        // Get the decoder XML definition, remap, and write
        if (sie_file.xml_def.getDecoder(@intCast(from_id))) |node| {
            const copy = try node.clone(self.allocator);
            defer copy.deinit();

            // Set the new ID before remapping (remap will handle child elements)
            var buf: [20]u8 = undefined;
            const id_str = std.fmt.bufPrint(&buf, "{d}", .{new_id}) catch unreachable;
            try copy.setAttribute("id", id_str);

            try self.writer.xmlNode(copy);
        }
    }

    /// Map a test ID, assigning a new contiguous ID on first encounter.
    fn mapTestId(self: *Sifter, intake_id: u32, from_id: u32) !u32 {
        if (self.findId(.Test, intake_id, from_id)) |id| return id;
        const new_id = self.writer.nextId(.Test);
        try self.setId(.Test, intake_id, from_id, new_id);
        return new_id;
    }

    /// Walk to next node in XML tree (depth-first)
    fn walkNext(node: *xml_mod.Node, top: *xml_mod.Node) ?*xml_mod.Node {
        // Try children first
        if (node.child) |child| return child;
        // Try next sibling
        if (node.next) |next_node| {
            if (node != top) return next_node;
        }
        // Walk up to find an ancestor's sibling
        var current = node.parent;
        while (current) |p| {
            if (p == top) return null;
            if (p.next) |next_node| return next_node;
            current = p.parent;
        }
        return null;
    }
};

// ---- Tests ----

const testing = std.testing;

fn testWriterFn(_: ?*anyopaque, _: []const u8) usize {
    return 0; // discard output
}

test "sifter initialization" {
    const allocator = testing.allocator;
    var writer = writer_mod.Writer.init(allocator, testWriterFn, null);
    defer writer.deinit();

    var sifter = Sifter.init(allocator, &writer);
    defer sifter.deinit();

    try testing.expectEqual(@as(usize, 0), sifter.totalEntries());
}

test "sifter id mapping" {
    const allocator = testing.allocator;
    var writer = writer_mod.Writer.init(allocator, testWriterFn, null);
    defer writer.deinit();

    var sifter = Sifter.init(allocator, &writer);
    defer sifter.deinit();

    // Map a channel ID
    const id1 = try sifter.mapId(.Channel, 1, 10);

    // Same key returns same ID
    const id1_again = try sifter.mapId(.Channel, 1, 10);
    try testing.expectEqual(id1, id1_again);

    // Different key gets different ID
    const id2 = try sifter.mapId(.Channel, 1, 20);
    try testing.expect(id2 != id1);

    // Find works
    try testing.expectEqual(id1, sifter.findId(.Channel, 1, 10).?);
    try testing.expect(sifter.findId(.Channel, 2, 10) == null);
}

test "sifter group mapping with block ranges" {
    const allocator = testing.allocator;
    var writer = writer_mod.Writer.init(allocator, testWriterFn, null);
    defer writer.deinit();

    var sifter = Sifter.init(allocator, &writer);
    defer sifter.deinit();

    // Map a group with range
    const id = try sifter.mapIdGroup(1, 5, 10, 100);

    // Map same group with wider range — should merge
    const id2 = try sifter.mapIdGroup(1, 5, 5, 200);
    try testing.expectEqual(id, id2);

    // Verify the entry has merged ranges
    const mi = @intFromEnum(MapType.Group);
    const key = Sifter.combineKey(.{ .intake_id = 1, .from_id = 5 });
    const entry = sifter.maps[mi].get(key).?;
    try testing.expectEqual(@as(u64, 5), entry.start_block);
    try testing.expectEqual(@as(u64, 200), entry.end_block);
}

test "sifter signature-based mapping" {
    const allocator = testing.allocator;
    var writer = writer_mod.Writer.init(allocator, testWriterFn, null);
    defer writer.deinit();

    var sifter = Sifter.init(allocator, &writer);
    defer sifter.deinit();

    const id1 = try sifter.mapIdBySig(.DecoderBySig, "sig_abc");
    const id2 = try sifter.mapIdBySig(.DecoderBySig, "sig_abc");
    try testing.expectEqual(id1, id2);

    const id3 = try sifter.mapIdBySig(.DecoderBySig, "sig_def");
    try testing.expect(id3 != id1);
}

test "sifter xml remapping" {
    const allocator = testing.allocator;
    var writer = writer_mod.Writer.init(allocator, testWriterFn, null);
    defer writer.deinit();

    var sifter = Sifter.init(allocator, &writer);
    defer sifter.deinit();

    // Pre-set a channel mapping: intake 1, old id 10 -> new id 42
    try sifter.setId(.Channel, 1, 10, 42);

    // Create a <ch id="10"/> node
    const node = try xml_mod.Node.newElement(allocator, "ch");
    defer node.deinit();
    try node.setAttribute("id", "10");

    // Remap
    try sifter.remapXml(1, node);

    // Attribute should now be "42"
    const val = node.getAttribute("id").?;
    try testing.expect(std.mem.eql(u8, val, "42"));
}

test "sifter remapping rules" {
    // Verify the static remapping table
    try testing.expectEqual(@as(usize, 9), remapping_rules.len);
    try testing.expect(std.mem.eql(u8, remapping_rules[0].element, "ch"));
    try testing.expect(std.mem.eql(u8, remapping_rules[0].attribute, "base"));
    try testing.expectEqual(MapType.Channel, remapping_rules[0].map_type);
}
