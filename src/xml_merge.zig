// XML Definition & Merge Engine
// Replaces sie_xml_merge.h / xml_merge.c
//
// Provides an XML definition store that accumulates XML definition
// strings (from SIE file XML blocks), merges them into a unified
// DOM tree, and supports template expansion via "base" attribute
// inheritance. Maintains channel, test, and decoder ID maps.

const std = @import("std");
const xml_mod = @import("xml.zig");
const error_mod = @import("error.zig");

const Node = xml_mod.Node;
const NodeType = xml_mod.NodeType;

/// Merge style for comparing two XML elements
const MergeStyle = enum {
    NotEqual,
    Replace,
    Merge,
};

/// Legacy element name translations
const NameTranslation = struct {
    old_name: []const u8,
    new_name: []const u8,
};

const name_translations = [_]NameTranslation{
    .{ .old_name = "channel", .new_name = "ch" },
    .{ .old_name = "dimension", .new_name = "dim" },
    .{ .old_name = "transform", .new_name = "xform" },
};

/// XML Definition - accumulates and merges XML definition blocks
pub const XmlDefinition = struct {
    allocator: std.mem.Allocator,
    parser: xml_mod.IncrementalParser,
    sie_node: ?*Node = null,
    channel_map: std.AutoHashMap(i32, *Node),
    test_map: std.AutoHashMap(i32, *Node),
    decoder_map: std.AutoHashMap(i32, *Node),
    sie_node_started: bool = false,
    any_private_attrs: bool = false,
    max_recursion: i32 = 100,

    pub fn init(allocator: std.mem.Allocator) XmlDefinition {
        var self = XmlDefinition{
            .allocator = allocator,
            .parser = undefined,
            .channel_map = std.AutoHashMap(i32, *Node).init(allocator),
            .test_map = std.AutoHashMap(i32, *Node).init(allocator),
            .decoder_map = std.AutoHashMap(i32, *Node).init(allocator),
        };
        self.parser = xml_mod.IncrementalParser.init(
            allocator,
            parserStarted,
            parserText,
            parserComplete,
            @ptrCast(&self),
        );
        return self;
    }

    pub fn deinit(self: *XmlDefinition) void {
        if (self.sie_node) |node| node.deinit();
        self.channel_map.deinit();
        self.test_map.deinit();
        self.decoder_map.deinit();
        self.parser.deinit();
    }

    /// Feed XML text into the definition parser
    pub fn addString(self: *XmlDefinition, data: []const u8) !void {
        self.parser.callback_data = @ptrCast(self);
        try self.parser.parse(data);
    }

    /// Get the root <sie> node
    pub fn getSieNode(self: *const XmlDefinition) ?*Node {
        return self.sie_node;
    }

    /// Get a channel definition by ID
    pub fn getChannel(self: *const XmlDefinition, id: i32) ?*Node {
        return self.channel_map.get(id);
    }

    /// Get a test definition by ID
    pub fn getTest(self: *const XmlDefinition, id: i32) ?*Node {
        return self.test_map.get(id);
    }

    /// Get a decoder definition by ID
    pub fn getDecoder(self: *const XmlDefinition, id: i32) ?*Node {
        return self.decoder_map.get(id);
    }

    /// Expand a channel or test definition by type name and ID.
    /// If the definition has a "base" attribute, recursively expands
    /// the base first, then merges the current definition on top.
    pub fn expand(self: *XmlDefinition, type_name: []const u8, id: i32) !*Node {
        return self.expandInternal(type_name, id, 0);
    }

    fn expandInternal(self: *XmlDefinition, type_name: []const u8, id: i32, level: i32) !*Node {
        if (level > self.max_recursion) return error_mod.Error.OperationFailed;

        const map = self.getMapForType(type_name) orelse return error_mod.Error.InvalidData;
        const node = map.get(id) orelse return error_mod.Error.InvalidData;

        const base_s = node.getAttribute("base");
        if (base_s) |base_str| {
            const base_id = std.fmt.parseInt(i32, base_str, 10) catch return error_mod.Error.ParseError;
            const base_node = try self.expandInternal(type_name, base_id, level + 1);
            // Remove "private" attribute from expanded base
            base_node.clearAttribute("private");
            // Merge current node's attributes and children onto the base
            try mergeElements(self.allocator, base_node, node, true);
            return base_node;
        } else {
            if (level == 0) {
                // Top-level call: return the node as-is (caller's reference)
                return node;
            } else {
                // Non-top-level: copy the node for merging
                return try copyTree(self.allocator, node);
            }
        }
    }

    fn getMapForType(self: *XmlDefinition, type_name: []const u8) ?*std.AutoHashMap(i32, *Node) {
        if (std.mem.eql(u8, type_name, "ch") or std.mem.eql(u8, type_name, "channel"))
            return &self.channel_map;
        if (std.mem.eql(u8, type_name, "test"))
            return &self.test_map;
        if (std.mem.eql(u8, type_name, "decoder"))
            return &self.decoder_map;
        return null;
    }

    fn isIdNode(name: []const u8) bool {
        return std.mem.eql(u8, name, "ch") or std.mem.eql(u8, name, "test") or std.mem.eql(u8, name, "decoder");
    }

    fn maybeStore(self: *XmlDefinition, node: *Node, expansion: bool) !void {
        if (expansion) return;
        const name = node.getName();
        if (!isIdNode(name)) return;
        const id_str = node.getAttribute("id") orelse return;
        const id = std.fmt.parseInt(i32, id_str, 10) catch return;

        if (!self.any_private_attrs and node.getAttribute("private") != null) {
            self.any_private_attrs = true;
        }

        const map = self.getMapForType(name) orelse return;
        try map.put(id, node);
    }

    // ── Parser callbacks ──

    fn parserStarted(node: *Node, _: ?*Node, _: usize, data: ?*anyopaque) void {
        if (data) |d| {
            const self: *XmlDefinition = @ptrCast(@alignCast(d));
            if (!self.sie_node_started) {
                if (node.node_type == .Element and std.mem.eql(u8, node.getName(), "sie")) {
                    // Create our own <sie> element to accumulate into.
                    // The parser's copy will be destroyed when </sie> closes.
                    self.sie_node = Node.newElementOwned(self.allocator, "sie") catch return;
                    // Copy attributes from the parser's node
                    for (node.attrs.items) |attr| {
                        self.sie_node.?.setAttribute(attr.name, attr.value) catch {};
                    }
                    self.sie_node_started = true;
                }
            }
        }
    }

    fn parserText(node_type: NodeType, _: []const u8, parent: ?*Node, _: usize, data: ?*anyopaque) bool {
        if (data) |d| {
            const self: *XmlDefinition = @ptrCast(@alignCast(d));
            _ = self;
            if (node_type == .Text) {
                // Only text in <tag> elements is kept
                if (parent) |p| {
                    if (!std.mem.eql(u8, p.getName(), "tag")) return false;
                }
            }
        }
        return true;
    }

    fn parserComplete(node: *Node, _: ?*Node, level: usize, data: ?*anyopaque) bool {
        if (data) |d| {
            const self: *XmlDefinition = @ptrCast(@alignCast(d));
            if (self.sie_node_started and node.node_type == .Element) {
                if (level > 1) {
                    return true; // Link to parent on the stack
                } else if (level == 1) {
                    // Top-level element: merge into our sie_node (copies data)
                    if (self.sie_node) |sie| {
                        _ = mergeElementInto(self.allocator, self, sie, node, false) catch null;
                    }
                    // We took ownership by returning false — must free
                    node.deinit();
                    return false;
                } else {
                    // Level 0: <sie> closing tag.
                    // We already created our own sie_node in parserStarted.
                    // Free the parser's version.
                    node.deinit();
                    return false;
                }
            }
        }
        return true; // Default: link to parent
    }
};

// ─── Merge Logic ─────────────────────────────────────────────────

/// Determine merge style for two nodes
fn equalForMerge(a: *Node, b: *Node) MergeStyle {
    if (a.node_type != .Element or b.node_type != .Element) return .NotEqual;
    if (!std.mem.eql(u8, a.getName(), b.getName())) return .NotEqual;

    const name = a.getName();

    // data, xform, units → replace
    if (std.mem.eql(u8, name, "data") or
        std.mem.eql(u8, name, "xform") or
        std.mem.eql(u8, name, "units"))
        return .Replace;

    // tag with matching id → replace
    if (std.mem.eql(u8, name, "tag")) {
        if (attributeEqual(a, b, "id")) return .Replace;
        return .NotEqual;
    }

    // ch, test, or decoder with matching id → merge
    if (std.mem.eql(u8, name, "ch") or std.mem.eql(u8, name, "test") or std.mem.eql(u8, name, "decoder")) {
        if (attributeEqual(a, b, "id")) return .Merge;
        return .NotEqual;
    }

    // dim with matching index → merge
    if (std.mem.eql(u8, name, "dim")) {
        if (attributeEqual(a, b, "index")) return .Merge;
        return .NotEqual;
    }

    return .NotEqual;
}

/// Check if two nodes have equal values for a given attribute
fn attributeEqual(a: *Node, b: *Node, attr_name: []const u8) bool {
    const av = a.getAttribute(attr_name) orelse return false;
    const bv = b.getAttribute(attr_name) orelse return false;
    return std.mem.eql(u8, av, bv);
}

/// Apply legacy name translations
fn translateName(name: []const u8) []const u8 {
    for (&name_translations) |tr| {
        if (std.mem.eql(u8, name, tr.old_name)) return tr.new_name;
    }
    return name;
}

/// Merge a patch element into a target parent, finding the right position
fn mergeElementInto(
    allocator: std.mem.Allocator,
    defn: *XmlDefinition,
    top: *Node,
    patch: *Node,
    expansion: bool,
) !*Node {
    // Apply legacy name translations
    if (!expansion) {
        const translated = translateName(patch.getName());
        if (!std.mem.eql(u8, translated, patch.getName())) {
            try patch.setNameOwned(translated);
        }
    }

    // Find matching node in top's children
    var base: ?*Node = null;
    var style: MergeStyle = .NotEqual;

    // Fast path for ch/test/decoder using ID maps
    if (!expansion and (std.mem.eql(u8, patch.getName(), "ch") or std.mem.eql(u8, patch.getName(), "test") or std.mem.eql(u8, patch.getName(), "decoder"))) {
        if (patch.getAttribute("id")) |id_str| {
            if (std.fmt.parseInt(i32, id_str, 10)) |id| {
                const map = defn.getMapForType(patch.getName());
                if (map) |m| {
                    if (m.get(id)) |existing| {
                        base = existing;
                        style = .Merge;
                    }
                }
            } else |_| {}
        }
    }

    // Slow path: scan children for match
    if (base == null) {
        var cur = top.child;
        while (cur) |c| {
            const s = equalForMerge(c, patch);
            if (s != .NotEqual) {
                base = c;
                style = s;
                break;
            }
            cur = c.next;
        }
    }

    if (base == null) {
        // No match: copy and append
        const name = patch.getName();
        var merged: *Node = undefined;
        if (std.mem.eql(u8, name, "ch") or std.mem.eql(u8, name, "test") or std.mem.eql(u8, name, "dim") or std.mem.eql(u8, name, "decoder")) {
            merged = try mergeTree(allocator, defn, patch, expansion);
        } else {
            merged = try copyTree(allocator, patch);
        }
        top.appendChild(merged);
        return merged;
    }

    switch (style) {
        .Merge => {
            try mergeElements(allocator, base.?, patch, expansion);
            return base.?;
        },
        .Replace => {
            const replacement = try copyTree(allocator, patch);
            top.insertAfter(replacement, base.?);
            base.?.unlink();
            base.?.deinit();
            return replacement;
        },
        .NotEqual => unreachable,
    }
}

const MergeError = std.mem.Allocator.Error || error_mod.Error;

/// Merge attributes and children from patch into base.
fn mergeElements(
    allocator: std.mem.Allocator,
    base: *Node,
    patch: *Node,
    expansion: bool,
) MergeError!void {
    // Copy attributes from patch to base
    for (patch.attrs.items) |attr| {
        try base.setAttribute(attr.name, attr.value);
    }
    // Merge children
    var cur = patch.child;
    while (cur) |c| {
        const next = c.next;
        if (c.node_type == .Element) {
            try mergeElementChild(allocator, base, c, expansion);
        } else {
            // Text/comment/PI: copy and append
            if (expansion) {
                const copy = try copyNode(allocator, c);
                base.appendChild(copy);
            }
        }
        cur = next;
    }
}

/// Merge a single child element into base by scanning base's children
fn mergeElementChild(
    allocator: std.mem.Allocator,
    base: *Node,
    patch: *Node,
    expansion: bool,
) MergeError!void {
    // Find matching child in base
    var existing: ?*Node = null;
    var style: MergeStyle = .NotEqual;
    var cur = base.child;
    while (cur) |c| {
        const s = equalForMerge(c, patch);
        if (s != .NotEqual) {
            existing = c;
            style = s;
            break;
        }
        cur = c.next;
    }

    if (existing == null) {
        // No match: copy and append
        const copy = try copyTree(allocator, patch);
        base.appendChild(copy);
        return;
    }

    switch (style) {
        .Merge => {
            try mergeElements(allocator, existing.?, patch, expansion);
        },
        .Replace => {
            const replacement = try copyTree(allocator, patch);
            base.insertAfter(replacement, existing.?);
            existing.?.unlink();
            existing.?.deinit();
        },
        .NotEqual => unreachable,
    }
}

/// Create a new element and merge tree into it, then maybe store in ID map
fn mergeTree(
    allocator: std.mem.Allocator,
    defn: *XmlDefinition,
    tree: *Node,
    expansion: bool,
) !*Node {
    if (tree.node_type != .Element) {
        if (expansion) return try copyNode(allocator, tree);
        return tree;
    }
    const new_node = try Node.newElementOwned(allocator, tree.getName());
    try mergeElements(allocator, new_node, tree, expansion);
    try defn.maybeStore(new_node, expansion);
    return new_node;
}

/// Deep copy a single node (without children)
fn copyNode(allocator: std.mem.Allocator, src: *Node) !*Node {
    switch (src.node_type) {
        .Element => {
            const copy = try Node.newElementOwned(allocator, src.getName());
            for (src.attrs.items) |attr| {
                try copy.setAttribute(attr.name, attr.value);
            }
            return copy;
        },
        .Text => return try Node.newTextOwned(allocator, src.text orelse ""),
        .Comment => return try Node.newComment(allocator, src.text orelse ""),
        .ProcessingInstruction => return try Node.newPI(allocator, src.text orelse ""),
    }
}

/// Deep copy a node and all its children
fn copyTree(allocator: std.mem.Allocator, src: *Node) !*Node {
    const copy = try copyNode(allocator, src);
    var cur = src.child;
    while (cur) |c| {
        const child_copy = try copyTree(allocator, c);
        copy.appendChild(child_copy);
        cur = c.next;
    }
    return copy;
}

// ─── Tests ──────────────────────────────────────────────────────

test "xml definition basic" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var defn = XmlDefinition.init(allocator);
    defer defn.deinit();

    try defn.addString("<sie><ch id=\"1\"><dim index=\"0\"/></ch></sie>");

    try std.testing.expect(defn.sie_node != null);
    try std.testing.expectEqualSlices(u8, "sie", defn.sie_node.?.getName());

    // Channel 1 should be registered
    const ch = defn.getChannel(1);
    try std.testing.expect(ch != null);
    try std.testing.expectEqualSlices(u8, "ch", ch.?.getName());
    try std.testing.expectEqualSlices(u8, "1", ch.?.getAttribute("id").?);
}

test "xml definition merge" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var defn = XmlDefinition.init(allocator);
    defer defn.deinit();

    // First definition block
    try defn.addString("<sie><ch id=\"1\"><dim index=\"0\" units=\"V\"/></ch></sie>");
    // Second definition block - should merge
    try defn.addString("<sie><ch id=\"1\"><dim index=\"1\" units=\"A\"/></ch></sie>");

    const ch = defn.getChannel(1);
    try std.testing.expect(ch != null);

    // Should have 2 dims merged
    var dim_count: usize = 0;
    var cur = ch.?.child;
    while (cur) |c| {
        if (c.node_type == .Element and std.mem.eql(u8, c.getName(), "dim"))
            dim_count += 1;
        cur = c.next;
    }
    try std.testing.expectEqual(@as(usize, 2), dim_count);
}

test "xml definition multiple channels" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var defn = XmlDefinition.init(allocator);
    defer defn.deinit();

    try defn.addString(
        \\<sie>
        \\  <ch id="1"><dim index="0" units="V"/></ch>
        \\  <ch id="2"><dim index="0" units="A"/></ch>
        \\  <test id="1"><ch id="1"/></test>
        \\</sie>
    );

    try std.testing.expect(defn.getChannel(1) != null);
    try std.testing.expect(defn.getChannel(2) != null);
    try std.testing.expect(defn.getTest(1) != null);
}

test "xml definition base expansion" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var defn = XmlDefinition.init(allocator);
    defer defn.deinit();

    try defn.addString(
        \\<sie>
        \\  <ch id="1"><dim index="0" units="V"/><dim index="1" units="A"/></ch>
        \\  <ch id="2" base="1"><dim index="0" units="mV"/></ch>
        \\</sie>
    );

    // Channel 2 inherits from 1, overrides dim 0 units
    const expanded = try defn.expand("ch", 2);
    defer expanded.deinit();

    // Should have dim 0 with units="mV" (overridden) and dim 1 with units="A" (inherited)
    const dim0 = expanded.findElement(expanded, "dim", "index", "0", .Descend);
    try std.testing.expect(dim0 != null);
    try std.testing.expectEqualSlices(u8, "mV", dim0.?.getAttribute("units").?);

    const dim1 = expanded.findElement(expanded, "dim", "index", "1", .Descend);
    try std.testing.expect(dim1 != null);
    try std.testing.expectEqualSlices(u8, "A", dim1.?.getAttribute("units").?);
}

test "xml copyTree" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElementOwned(allocator, "root");
    defer root.deinit();

    const child = try Node.newElementOwned(allocator, "child");
    try child.setAttribute("key", "val");
    root.appendChild(child);

    const text = try Node.newTextOwned(allocator, "hello");
    child.appendChild(text);

    const copy = try copyTree(allocator, root);
    defer copy.deinit();

    try std.testing.expectEqualSlices(u8, "root", copy.getName());
    try std.testing.expect(copy != root);
    const copy_child = copy.child.?;
    try std.testing.expectEqualSlices(u8, "child", copy_child.getName());
    try std.testing.expectEqualSlices(u8, "val", copy_child.getAttribute("key").?);
    try std.testing.expectEqualSlices(u8, "hello", copy_child.child.?.text.?);
}
