// SieFile - high-level SIE file reader
//
// Orchestrates the full pipeline: opens a file, reads XML metadata,
// builds the Test → Channel → Dimension hierarchy, compiles decoders,
// and provides navigation APIs matching the C demo (sie_file_open).
//
// Usage:
//   var sf = try SieFile.open(allocator, "data.sie");
//   defer sf.deinit();
//   for (sf.tests()) |*test_obj| { ... }

const std = @import("std");
const file_mod = @import("file.zig");
const block_mod = @import("block.zig");
const xml_mod = @import("xml.zig");
const xml_merge_mod = @import("xml_merge.zig");
const test_mod = @import("test.zig");
const channel_mod = @import("channel.zig");
const dimension_mod = @import("dimension.zig");
const tag_mod = @import("tag.zig");
const compiler_mod = @import("compiler.zig");
const decoder_mod = @import("decoder.zig");
const error_mod = @import("error.zig");
const channel_spigot_mod = @import("channel_spigot.zig");
const spigot_mod = @import("spigot.zig");

const Node = xml_mod.Node;

/// High-level SIE file reader with full test/channel/dimension hierarchy
pub const SieFile = struct {
    allocator: std.mem.Allocator,
    file: file_mod.File,
    xml_def: xml_merge_mod.XmlDefinition,

    test_list: std.ArrayList(test_mod.Test),
    all_channels: std.ArrayList(*channel_mod.Channel), // pointers into tests' channels
    file_tags: std.ArrayList(tag_mod.Tag),

    // Compiled decoders keyed by decoder ID
    compiled_decoders: std.AutoHashMap(i32, decoder_mod.Decoder),

    // Owned strings that need to be freed (names duped from XML)
    owned_strings: std.ArrayList([]const u8),

    /// Open an SIE file and build the full hierarchy
    pub fn open(allocator: std.mem.Allocator, path: []const u8) !SieFile {
        var self = SieFile{
            .allocator = allocator,
            .file = file_mod.File.init(allocator, path),
            .xml_def = xml_merge_mod.XmlDefinition.init(allocator),
            .test_list = .{},
            .all_channels = .{},
            .file_tags = .{},
            .compiled_decoders = std.AutoHashMap(i32, decoder_mod.Decoder).init(allocator),
            .owned_strings = .{},
        };
        errdefer self.deinit();

        // Open and validate
        try self.file.open();
        if (!try self.file.isSie()) return error_mod.Error.InvalidBlock;
        try self.file.buildIndex();

        // Read XML from group 0 and feed to XmlDefinition
        try self.parseXml();

        // Compile decoders
        try self.compileDecoders();

        // Build test/channel/dimension hierarchy from XML
        try self.buildHierarchy();

        return self;
    }

    pub fn deinit(self: *SieFile) void {
        // Free all_channels list (pointers only, data owned by tests)
        self.all_channels.deinit(self.allocator);

        for (self.file_tags.items) |*t| t.deinit();
        self.file_tags.deinit(self.allocator);

        for (self.test_list.items) |*t| t.deinit();
        self.test_list.deinit(self.allocator);

        var dec_iter = self.compiled_decoders.valueIterator();
        while (dec_iter.next()) |d| {
            @constCast(d).deinit();
        }
        self.compiled_decoders.deinit();

        for (self.owned_strings.items) |s| self.allocator.free(@constCast(s));
        self.owned_strings.deinit(self.allocator);

        self.xml_def.deinit();
        self.file.deinit();
    }

    // ── Navigation API ──

    /// Get all tests
    pub fn tests(self: *SieFile) []test_mod.Test {
        return self.test_list.items;
    }

    /// Get all channels (across all tests)
    pub fn channels(self: *SieFile) []*channel_mod.Channel {
        return self.all_channels.items;
    }

    /// Get file-level tags (from the <sie> element)
    pub fn fileTags(self: *SieFile) []const tag_mod.Tag {
        return self.file_tags.items;
    }

    /// Find a test by ID
    pub fn findTest(self: *SieFile, id: u32) ?*test_mod.Test {
        for (self.test_list.items) |*t| {
            if (t.id == id) return t;
        }
        return null;
    }

    /// Find a channel by ID (across all tests)
    pub fn findChannel(self: *SieFile, id: u32) ?*channel_mod.Channel {
        for (self.all_channels.items) |ch| {
            if (ch.id == id) return ch;
        }
        return null;
    }

    /// Get the containing test for a channel
    pub fn containingTest(self: *SieFile, ch: *const channel_mod.Channel) ?*test_mod.Test {
        return self.findTest(ch.test_id);
    }

    /// Attach a spigot to a channel for reading data
    pub fn attachSpigot(self: *SieFile, ch: *const channel_mod.Channel) !channel_spigot_mod.ChannelSpigot {
        return channel_spigot_mod.ChannelSpigot.init(
            self.allocator,
            &self.file,
            ch,
            &self.compiled_decoders,
        );
    }

    /// Get a compiled decoder by ID
    pub fn decoder(self: *SieFile, id: i32) ?*const decoder_mod.Decoder {
        return self.compiled_decoders.getPtr(id);
    }

    /// Get the number of compiled decoders
    pub fn numDecoders(self: *const SieFile) usize {
        return self.compiled_decoders.count();
    }

    // ── Internal: XML parsing ──

    fn parseXml(self: *SieFile) !void {
        const idx = self.file.groupIndex(block_mod.SIE_XML_GROUP) orelse return;

        for (idx.entries.items) |entry| {
            var blk = try self.file.readBlockAt(@intCast(entry.offset));
            defer blk.deinit();
            const pl = blk.payload();
            try self.xml_def.addString(pl);
        }

        // Force close the root <sie> element (SIE files don't include </sie>)
        try self.xml_def.addString("</sie>");
    }

    // ── Internal: compile decoders ──

    fn compileDecoders(self: *SieFile) !void {
        var iter = self.xml_def.decoder_map.iterator();
        while (iter.next()) |entry| {
            const id = entry.key_ptr.*;
            const node = entry.value_ptr.*;

            var comp = compiler_mod.Compiler.init(self.allocator);
            defer comp.deinit();

            var result = comp.compile(node) catch continue;
            defer result.deinit(self.allocator);

            // Convert vs (i32 register indices) to usize
            const dim_regs = try self.allocator.alloc(usize, result.vs.len);
            defer self.allocator.free(dim_regs);
            for (result.vs, 0..) |v, i| {
                dim_regs[i] = @intCast(v);
            }

            var dec = try decoder_mod.Decoder.init(
                self.allocator,
                result.bytecode,
                dim_regs,
                result.num_registers,
                result.initial_registers,
            );
            // Store — decoder owns its duped data
            try self.compiled_decoders.put(id, dec);
            _ = &dec;
        }
    }

    // ── Internal: build hierarchy ──

    fn buildHierarchy(self: *SieFile) !void {
        const sie_node = self.xml_def.getSieNode() orelse return;

        // Collect file-level tags from <sie> children
        var child = sie_node.child;
        while (child) |node| : (child = node.next) {
            if (node.node_type != .Element) continue;
            if (std.mem.eql(u8, node.getName(), "tag")) {
                try self.buildFileTag(node);
            }
        }

        // Walk <test> elements to build tests and their channels
        // Tests may appear multiple times in the merged XML (for start/end tags)
        // so we merge into existing test objects
        child = sie_node.child;
        while (child) |node| : (child = node.next) {
            if (node.node_type != .Element) continue;
            if (!std.mem.eql(u8, node.getName(), "test")) continue;
            if (node.getAttribute("private") != null) continue;

            const id_str = node.getAttribute("id") orelse continue;
            const test_id = std.fmt.parseInt(u32, id_str, 10) catch continue;

            // Find or create test
            var test_obj = self.findTest(test_id);
            if (test_obj == null) {
                const test_name = node.getAttribute("name") orelse "";
                const new_test = test_mod.Test.init(self.allocator, test_id, test_name);
                try self.test_list.append(self.allocator, new_test);
                test_obj = &self.test_list.items[self.test_list.items.len - 1];
            }

            // Add tags and channels from this <test> element
            var tc = node.child;
            while (tc) |c| : (tc = c.next) {
                if (c.node_type != .Element) continue;
                const cname = c.getName();

                if (std.mem.eql(u8, cname, "tag")) {
                    if (try self.buildTag(c)) |t| {
                        try test_obj.?.addTag(t);
                    }
                } else if (std.mem.eql(u8, cname, "channel") or std.mem.eql(u8, cname, "ch")) {
                    const cid_str = c.getAttribute("id") orelse continue;
                    const ch_id = std.fmt.parseInt(i32, cid_str, 10) catch continue;
                    if (c.getAttribute("private") != null) continue;

                    // Check if this channel already exists in this test
                    var already_exists = false;
                    for (test_obj.?.channelsMut()) |*existing_ch| {
                        if (existing_ch.id == @as(u32, @intCast(ch_id))) {
                            // Merge additional tags into existing channel
                            var merge_child = c.child;
                            while (merge_child) |mc| : (merge_child = mc.next) {
                                if (mc.node_type != .Element) continue;
                                if (std.mem.eql(u8, mc.getName(), "tag")) {
                                    if (try self.buildTag(mc)) |t| {
                                        try existing_ch.addTag(t);
                                    }
                                }
                            }
                            already_exists = true;
                            break;
                        }
                    }
                    if (!already_exists) {
                        try self.buildChannel(ch_id, c, test_id, test_obj.?);
                    }
                }
            }
        }

        // Build the all_channels index (pointers into tests' channel arrays)
        for (self.test_list.items) |*t| {
            for (t.channel_list.items) |*ch| {
                try self.all_channels.append(self.allocator, ch);
            }
        }
    }

    fn buildChannel(self: *SieFile, ch_id: i32, ch_node: *Node, test_id: u32, test_obj: *test_mod.Test) !void {
        // Resolve channel definition: if this channel has a base, expand the base
        // to get the full definition with dimensions. Channels inside <test> elements
        // may reference a base template (e.g. <channel id="1" base="0" group="4"/>)
        var expanded: *Node = ch_node;
        if (ch_node.getAttribute("base")) |base_str| {
            const base_id = std.fmt.parseInt(i32, base_str, 10) catch null;
            if (base_id) |bid| {
                expanded = self.xml_def.expand("ch", bid) catch ch_node;
            }
        } else {
            // Try expanding via channel_map (handles base inheritance recursively)
            expanded = self.xml_def.expand("ch", ch_id) catch ch_node;
        }

        // Get name — prefer ch_node (specific reference), fall back to expanded (base template)
        const name_str = ch_node.getAttribute("name") orelse expanded.getAttribute("name") orelse "";
        const name_owned = try self.allocator.dupe(u8, name_str);
        try self.owned_strings.append(self.allocator, name_owned);

        var ch = channel_mod.Channel.init(self.allocator, @intCast(ch_id), name_owned);
        ch.test_id = test_id;

        // Get group from the channel node (the one inside <test>) — this overrides base
        if (ch_node.getAttribute("group")) |group_str| {
            ch.toplevel_group = std.fmt.parseInt(u32, group_str, 10) catch 0;
        } else if (expanded.getAttribute("group")) |group_str| {
            ch.toplevel_group = std.fmt.parseInt(u32, group_str, 10) catch 0;
        }

        // Build dimensions and tags from expanded XML
        var dim_child = expanded.child;
        while (dim_child) |dc| : (dim_child = dc.next) {
            if (dc.node_type != .Element) continue;
            const dc_name = dc.getName();

            if (std.mem.eql(u8, dc_name, "dimension") or std.mem.eql(u8, dc_name, "dim")) {
                if (self.buildDimension(dc, ch.toplevel_group)) |dim| {
                    try ch.addDimension(dim);
                }
            } else if (std.mem.eql(u8, dc_name, "tag")) {
                if (try self.buildTag(dc)) |t| {
                    try ch.addTag(t);
                }
            }
        }

        // Also add tags and dimensions from the ch_node itself (if different from expanded).
        // This is needed for channels using shorthand path notation (e.g. <ch test="0">)
        // where the channel node carries its own dimension and tag definitions alongside
        // a base attribute that points to a template without dimensions.
        if (ch_node != expanded) {
            var cn_child = ch_node.child;
            while (cn_child) |cnc| : (cn_child = cnc.next) {
                if (cnc.node_type != .Element) continue;
                const cn_name = cnc.getName();
                if (std.mem.eql(u8, cn_name, "tag")) {
                    if (try self.buildTag(cnc)) |t| {
                        try ch.addTag(t);
                    }
                } else if (std.mem.eql(u8, cn_name, "dimension") or std.mem.eql(u8, cn_name, "dim")) {
                    if (self.buildDimension(cnc, ch.toplevel_group)) |dim| {
                        // Only add if this dimension index isn't already present from expanded
                        var already_present = false;
                        for (ch.dim_list.items) |d| {
                            if (d.index == dim.index) {
                                already_present = true;
                                break;
                            }
                        }
                        if (!already_present) {
                            try ch.addDimension(dim);
                        }
                    }
                }
            }
        }

        try test_obj.addChannel(ch);
    }

    fn buildDimension(self: *SieFile, node: *Node, toplevel_group: u32) ?dimension_mod.Dimension {
        const index_str = node.getAttribute("index") orelse return null;
        const index = std.fmt.parseInt(u32, index_str, 10) catch return null;

        // Dimension group: inherit from parent channel unless overridden
        var group = toplevel_group;
        if (node.getAttribute("group")) |g| {
            group = std.fmt.parseInt(u32, g, 10) catch toplevel_group;
        }

        // Look for a name — use empty string as fallback
        const name_owned = self.allocator.dupe(u8, "") catch return null;
        self.owned_strings.append(self.allocator, name_owned) catch return null;

        var dim = dimension_mod.Dimension.init(self.allocator, name_owned, index, group);

        // Parse <data> child for decoder info
        var child = node.child;
        while (child) |c| : (child = c.next) {
            if (c.node_type != .Element) continue;
            const cname = c.getName();

            if (std.mem.eql(u8, cname, "data")) {
                if (c.getAttribute("decoder")) |dec_str| {
                    dim.decoder_id = std.fmt.parseInt(u32, dec_str, 10) catch 0;
                }
                // "v" attribute is the output variable index in the decoder
                if (c.getAttribute("v")) |v_str| {
                    dim.decoder_version = std.fmt.parseInt(usize, v_str, 10) catch 0;
                }
            } else if (std.mem.eql(u8, cname, "transform") or std.mem.eql(u8, cname, "xform")) {
                // Parse linear transform: <transform scale="S" offset="O"/>
                if (c.getAttribute("scale")) |scale_str| {
                    const scale = std.fmt.parseFloat(f64, scale_str) catch 1.0;
                    const offset = if (c.getAttribute("offset")) |off_str|
                        std.fmt.parseFloat(f64, off_str) catch 0.0
                    else
                        0.0;
                    dim.has_linear_xform = true;
                    dim.xform_scale = scale;
                    dim.xform_offset = offset;
                }
            }
        }

        // Parse <tag> children
        var tag_child = node.child;
        while (tag_child) |tc| : (tag_child = tc.next) {
            if (tc.node_type != .Element) continue;
            if (std.mem.eql(u8, tc.getName(), "tag")) {
                if (self.buildTagNoError(tc)) |t| {
                    dim.addTag(t) catch {};
                }
            }
        }

        return dim;
    }

    fn buildTag(self: *SieFile, node: *Node) !?tag_mod.Tag {
        return self.buildTagNoError(node);
    }

    fn buildTagNoError(self: *SieFile, node: *Node) ?tag_mod.Tag {
        const id_attr = node.getAttribute("id") orelse return null;

        // Get text content of the tag node
        var text: []const u8 = "";
        if (node.child) |c| {
            if (c.node_type == .Text) {
                text = c.text orelse "";
            }
        }

        const key = self.allocator.dupe(u8, id_attr) catch return null;
        self.owned_strings.append(self.allocator, key) catch {
            self.allocator.free(@constCast(key));
            return null;
        };
        const val = self.allocator.dupe(u8, text) catch return null;
        self.owned_strings.append(self.allocator, val) catch {
            self.allocator.free(@constCast(val));
            return null;
        };

        return tag_mod.Tag.initString(self.allocator, key, val) catch null;
    }

    fn buildFileTag(self: *SieFile, node: *Node) !void {
        if (try self.buildTag(node)) |t| {
            try self.file_tags.append(self.allocator, t);
        }
    }
};

// ── Tests ──

test "SieFile: open and read hierarchy" {
    const testing = std.testing;
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var sf = SieFile.open(allocator, "test/data/sie_min_timhis_a_19EFAA61.sie") catch |e| {
        // If file not found, skip test gracefully
        if (e == error_mod.Error.FileNotFound) return;
        return e;
    };
    defer sf.deinit();

    // Should have at least one test
    const tests = sf.tests();
    try testing.expect(tests.len >= 1);

    // Should have channels
    const channels = sf.channels();
    try testing.expect(channels.len >= 1);

    // First channel should have dimensions
    if (channels.len > 0) {
        const ch = channels[0];
        try testing.expect(ch.dimensions().len >= 1);
    }
}

test "SieFile: compiled decoders exist" {
    const testing = std.testing;
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var sf = SieFile.open(allocator, "test/data/sie_min_timhis_a_19EFAA61.sie") catch |e| {
        if (e == error_mod.Error.FileNotFound) return;
        return e;
    };
    defer sf.deinit();

    // Should have at least one compiled decoder
    try testing.expect(sf.numDecoders() >= 1);
}

test "SieFile: channel has decoder info" {
    const testing = std.testing;
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var sf = SieFile.open(allocator, "test/data/sie_min_timhis_a_19EFAA61.sie") catch |e| {
        if (e == error_mod.Error.FileNotFound) return;
        return e;
    };
    defer sf.deinit();

    const channels = sf.channels();
    if (channels.len > 0) {
        const ch = channels[0];
        const dims = ch.dimensions();
        if (dims.len > 0) {
            // Dimension should reference a decoder
            try testing.expect(dims[0].decoder_id > 0);
        }
    }
}
