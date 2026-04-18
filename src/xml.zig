// XML DOM tree and incremental parser
// Replaces sie_xml.h / xml.c
//
// Provides a DOM tree (nodes with element/text types, attributes,
// parent/child/sibling links) and an incremental XML parser that
// builds the tree character by character. The parser supports
// entity references, comments, processing instructions, and
// callback hooks for streaming use.

const std = @import("std");
const error_mod = @import("error.zig");

// ─── Node types ──────────────────────────────────────────────────

pub const NodeType = enum {
    Element,
    Text,
    Comment,
    ProcessingInstruction,
};

pub const DescendType = enum {
    NoDescend,
    Descend,
    DescendOnce,
};

// ─── Attribute ───────────────────────────────────────────────────

pub const Attr = struct {
    name: []const u8,
    value: []const u8,
};

// ─── XML Node ────────────────────────────────────────────────────

pub const Node = struct {
    allocator: std.mem.Allocator,
    node_type: NodeType,

    // Element-specific
    name: ?[]const u8 = null,
    attrs: std.ArrayList(Attr) = .{},

    // Text/Comment/PI content
    text: ?[]const u8 = null,

    // Tree links
    parent: ?*Node = null,
    child: ?*Node = null,
    last_child: ?*Node = null,
    next: ?*Node = null,
    prev: ?*Node = null,

    // Memory ownership tracking
    owns_name: bool = false,
    owns_text: bool = false,

    // ── Constructors ──

    /// Create a new element node
    pub fn newElement(allocator: std.mem.Allocator, name: []const u8) !*Node {
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .Element,
            .name = name,
        };
        return node;
    }

    /// Create a new element with an owned (duped) name
    pub fn newElementOwned(allocator: std.mem.Allocator, name: []const u8) !*Node {
        const owned = try allocator.dupe(u8, name);
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .Element,
            .name = owned,
            .owns_name = true,
        };
        return node;
    }

    /// Create a new text node
    pub fn newText(allocator: std.mem.Allocator, text: []const u8) !*Node {
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .Text,
            .text = text,
        };
        return node;
    }

    /// Create a text node with owned (duped) content
    pub fn newTextOwned(allocator: std.mem.Allocator, text: []const u8) !*Node {
        const owned = try allocator.dupe(u8, text);
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .Text,
            .text = owned,
            .owns_text = true,
        };
        return node;
    }

    /// Create a comment node with owned content
    pub fn newComment(allocator: std.mem.Allocator, text: []const u8) !*Node {
        const owned = try allocator.dupe(u8, text);
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .Comment,
            .text = owned,
            .owns_text = true,
        };
        return node;
    }

    /// Create a processing instruction node with owned content
    pub fn newPI(allocator: std.mem.Allocator, text: []const u8) !*Node {
        const owned = try allocator.dupe(u8, text);
        const node = try allocator.create(Node);
        node.* = Node{
            .allocator = allocator,
            .node_type = .ProcessingInstruction,
            .text = owned,
            .owns_text = true,
        };
        return node;
    }

    // ── Clone ──

    /// Deep-copy this node and all its children into a new tree.
    pub fn clone(self: *const Node, allocator: std.mem.Allocator) !*Node {
        const copy = switch (self.node_type) {
            .Element => blk: {
                const n = try newElementOwned(allocator, self.name orelse "");
                for (self.attrs.items) |attr| {
                    try n.setAttribute(attr.name, attr.value);
                }
                break :blk n;
            },
            .Text => try newTextOwned(allocator, self.text orelse ""),
            .Comment => try newComment(allocator, self.text orelse ""),
            .ProcessingInstruction => try newPI(allocator, self.text orelse ""),
        };
        var cur = self.child;
        while (cur) |c| {
            const child_copy = try c.clone(allocator);
            copy.appendChild(child_copy);
            cur = c.next;
        }
        return copy;
    }

    /// Pack (compact copy) of this node and all its children.
    /// This is the Zig equivalent of the C library's `sie_xml_pack()`.
    /// In C, pack allocated all nodes in a single contiguous block for
    /// memory efficiency. In Zig, this is a deep copy (identical to `clone`)
    /// since the allocator handles memory layout.
    pub fn pack(self: *const Node, allocator: std.mem.Allocator) !*Node {
        return self.clone(allocator);
    }

    // ── Destructor ──

    /// Destroy this node and all children recursively
    pub fn deinit(self: *Node) void {
        // Recursively destroy children
        var cur = self.child;
        while (cur) |c| {
            const nxt = c.next;
            c.parent = null;
            c.prev = null;
            c.next = null;
            c.deinit();
            cur = nxt;
        }

        // Free attributes
        for (self.attrs.items) |attr| {
            self.allocator.free(attr.name);
            self.allocator.free(attr.value);
        }
        self.attrs.deinit(self.allocator);

        // Free owned strings
        if (self.owns_name) {
            if (self.name) |n| self.allocator.free(n);
        }
        if (self.owns_text) {
            if (self.text) |t| self.allocator.free(t);
        }

        self.allocator.destroy(self);
    }

    // ── Attributes ──

    /// Set an attribute (overwrites if exists)
    pub fn setAttribute(self: *Node, name: []const u8, value: []const u8) !void {
        // Check for existing
        for (self.attrs.items) |*attr| {
            if (std.mem.eql(u8, attr.name, name)) {
                self.allocator.free(attr.value);
                attr.value = try self.allocator.dupe(u8, value);
                return;
            }
        }
        // Add new
        try self.attrs.append(self.allocator, .{
            .name = try self.allocator.dupe(u8, name),
            .value = try self.allocator.dupe(u8, value),
        });
    }

    /// Get an attribute value by name
    pub fn getAttribute(self: *const Node, name: []const u8) ?[]const u8 {
        for (self.attrs.items) |attr| {
            if (std.mem.eql(u8, attr.name, name)) return attr.value;
        }
        return null;
    }

    /// Remove an attribute by name
    pub fn clearAttribute(self: *Node, name: []const u8) void {
        for (self.attrs.items, 0..) |attr, i| {
            if (std.mem.eql(u8, attr.name, name)) {
                self.allocator.free(attr.name);
                self.allocator.free(attr.value);
                _ = self.attrs.orderedRemove(i);
                return;
            }
        }
    }

    /// Get number of attributes
    pub fn numAttrs(self: *const Node) usize {
        return self.attrs.items.len;
    }

    // ── Element name ──

    /// Get element name
    pub fn getName(self: *const Node) []const u8 {
        return self.name orelse "";
    }

    /// Set element name (takes ownership)
    pub fn setNameOwned(self: *Node, new_name: []const u8) !void {
        if (self.owns_name) {
            if (self.name) |old| self.allocator.free(old);
        }
        self.name = try self.allocator.dupe(u8, new_name);
        self.owns_name = true;
    }

    // ── Tree manipulation ──

    /// Append a child node at the end
    pub fn appendChild(self: *Node, child_node: *Node) void {
        child_node.parent = self;
        child_node.prev = self.last_child;
        child_node.next = null;
        if (self.last_child) |lc| {
            lc.next = child_node;
        } else {
            self.child = child_node;
        }
        self.last_child = child_node;
    }

    /// Insert a child node at the beginning
    pub fn prependChild(self: *Node, child_node: *Node) void {
        child_node.parent = self;
        child_node.next = self.child;
        child_node.prev = null;
        if (self.child) |fc| {
            fc.prev = child_node;
        } else {
            self.last_child = child_node;
        }
        self.child = child_node;
    }

    /// Insert a node after a target sibling
    pub fn insertAfter(self: *Node, new_node: *Node, target: *Node) void {
        new_node.parent = self;
        new_node.prev = target;
        new_node.next = target.next;
        if (target.next) |tn| {
            tn.prev = new_node;
        } else {
            self.last_child = new_node;
        }
        target.next = new_node;
    }

    /// Insert a node before a target sibling
    pub fn insertBefore(self: *Node, new_node: *Node, target: *Node) void {
        new_node.parent = self;
        new_node.next = target;
        new_node.prev = target.prev;
        if (target.prev) |tp| {
            tp.next = new_node;
        } else {
            self.child = new_node;
        }
        target.prev = new_node;
    }

    /// Remove this node from its parent (does not destroy it)
    pub fn unlink(self: *Node) void {
        if (self.parent) |p| {
            if (self.prev) |pv| {
                pv.next = self.next;
            } else {
                p.child = self.next;
            }
            if (self.next) |nx| {
                nx.prev = self.prev;
            } else {
                p.last_child = self.prev;
            }
        }
        self.parent = null;
        self.prev = null;
        self.next = null;
    }

    // ── Tree walking ──

    /// Walk to the next node in document order
    pub fn walkNext(self: *Node, top: ?*Node, descend: DescendType) ?*Node {
        // Descend into children
        if (self.child != null and descend != .NoDescend) {
            return self.child;
        }
        // Sibling
        if (self.next) |nx| {
            if (top != null and self == top.?) return null;
            return nx;
        }
        // Walk up to find next ancestor sibling
        var cur = self.parent;
        while (cur) |c| {
            if (top != null and c == top.?) return null;
            if (c.next) |nx| return nx;
            cur = c.parent;
        }
        return null;
    }

    /// Walk to the previous node in document order
    pub fn walkPrev(self: *Node, top: ?*Node, descend: DescendType) ?*Node {
        if (self.prev) |pv| {
            if (pv.last_child != null and descend != .NoDescend) {
                var cur_node = pv.last_child.?;
                while (cur_node.last_child) |lc| {
                    cur_node = lc;
                }
                return cur_node;
            }
            return pv;
        }
        if (self.parent) |p| {
            if (top != null and p == top.?) return null;
            return p;
        }
        return null;
    }

    /// Find the next element matching criteria
    pub fn findElement(
        self: *Node,
        top: ?*Node,
        name: ?[]const u8,
        attr_name: ?[]const u8,
        attr_value: ?[]const u8,
        descend: DescendType,
    ) ?*Node {
        var cur: ?*Node = self.walkNext(top, descend);
        const subsequent_descend: DescendType = if (descend == .DescendOnce) .NoDescend else descend;
        while (cur) |c| {
            if (c.node_type == .Element) {
                const name_match = if (name) |n|
                    std.mem.eql(u8, c.getName(), n)
                else
                    true;
                if (name_match) {
                    if (attr_name) |an| {
                        if (c.getAttribute(an)) |av| {
                            if (attr_value) |ev| {
                                if (std.mem.eql(u8, av, ev)) return c;
                            } else {
                                return c;
                            }
                        }
                    } else {
                        return c;
                    }
                }
            }
            cur = c.walkNext(top, subsequent_descend);
        }
        return null;
    }

    /// Get first child element with given name
    pub fn getChildElement(self: *Node, name: []const u8) ?*Node {
        var cur = self.child;
        while (cur) |c| {
            if (c.node_type == .Element and std.mem.eql(u8, c.getName(), name))
                return c;
            cur = c.next;
        }
        return null;
    }

    /// Count child elements
    pub fn countChildren(self: *const Node) usize {
        var count: usize = 0;
        var cur = self.child;
        while (cur) |_| {
            count += 1;
            cur = cur.?.next;
        }
        return count;
    }

    // ── Output ──

    /// Serialize this node (and children) to XML string
    pub fn toXml(self: *const Node, allocator: std.mem.Allocator) ![]u8 {
        var buf: std.ArrayList(u8) = .{};
        defer buf.deinit(allocator);
        try self.outputXml(allocator, &buf, 0);
        return try buf.toOwnedSlice(allocator);
    }

    pub fn outputXml(self: *const Node, allocator: std.mem.Allocator, buf: *std.ArrayList(u8), indent: i32) !void {
        switch (self.node_type) {
            .Element => {
                // Indent
                if (indent > 0) {
                    try buf.appendNTimes(allocator, ' ', @intCast(indent));
                }
                try buf.append(allocator, '<');
                try buf.appendSlice(allocator, self.getName());
                for (self.attrs.items) |attr| {
                    try buf.append(allocator, ' ');
                    try buf.appendSlice(allocator, attr.name);
                    try buf.appendSlice(allocator, "=\"");
                    try outputQuoted(allocator, buf, attr.value, true);
                    try buf.append(allocator, '"');
                }
                if (self.child != null) {
                    // Check if any child is text (if so, no indentation inside)
                    var has_text_child = false;
                    var cur = self.child;
                    while (cur) |c| {
                        if (c.node_type == .Text) {
                            has_text_child = true;
                            break;
                        }
                        cur = c.next;
                    }
                    const inner_indent: i32 = if (has_text_child) -1 else if (indent >= 0) indent + 1 else -1;
                    try buf.append(allocator, '>');
                    if (!has_text_child and indent >= 0) try buf.append(allocator, '\n');

                    cur = self.child;
                    while (cur) |c| {
                        try c.outputXml(allocator, buf, inner_indent);
                        cur = c.next;
                    }

                    if (!has_text_child and indent >= 0) {
                        if (indent > 0) try buf.appendNTimes(allocator, ' ', @intCast(indent));
                    }
                    try buf.appendSlice(allocator, "</");
                    try buf.appendSlice(allocator, self.getName());
                    try buf.append(allocator, '>');
                    if (indent >= 0) try buf.append(allocator, '\n');
                } else {
                    try buf.appendSlice(allocator, "/>");
                    if (indent >= 0) try buf.append(allocator, '\n');
                }
            },
            .Text => {
                if (self.text) |t| {
                    try outputQuoted(allocator, buf, t, false);
                }
            },
            .Comment => {
                if (self.text) |t| {
                    try buf.appendSlice(allocator, "<");
                    try buf.appendSlice(allocator, t);
                    try buf.append(allocator, '>');
                    if (indent >= 0) try buf.append(allocator, '\n');
                }
            },
            .ProcessingInstruction => {
                if (self.text) |t| {
                    try buf.appendSlice(allocator, "<");
                    try buf.appendSlice(allocator, t);
                    try buf.append(allocator, '>');
                    if (indent >= 0) try buf.append(allocator, '\n');
                }
            },
        }
    }
};

fn outputQuoted(allocator: std.mem.Allocator, buf: *std.ArrayList(u8), src: []const u8, escape_quot: bool) !void {
    for (src) |c| {
        switch (c) {
            '&' => try buf.appendSlice(allocator, "&amp;"),
            '<' => try buf.appendSlice(allocator, "&lt;"),
            '"' => {
                if (escape_quot)
                    try buf.appendSlice(allocator, "&quot;")
                else
                    try buf.append(allocator, '"');
            },
            else => try buf.append(allocator, c),
        }
    }
}

// ─── Character classification ────────────────────────────────────

fn isNameStartChar(ch: u8) bool {
    return ch == ':' or ch == '_' or
        (ch >= 'A' and ch <= 'Z') or
        (ch >= 'a' and ch <= 'z') or
        ch >= 128;
}

fn isNameChar(ch: u8) bool {
    return isNameStartChar(ch) or ch == '-' or ch == '.' or
        (ch >= '0' and ch <= '9');
}

fn isWhitespace(ch: u8) bool {
    return ch == ' ' or ch == '\t' or ch == '\r' or ch == '\n';
}

// ─── Entity resolution ──────────────────────────────────────────

fn resolveEntity(entity_name: []const u8) ?u8 {
    if (std.mem.eql(u8, entity_name, "lt")) return '<';
    if (std.mem.eql(u8, entity_name, "gt")) return '>';
    if (std.mem.eql(u8, entity_name, "amp")) return '&';
    if (std.mem.eql(u8, entity_name, "apos")) return '\'';
    if (std.mem.eql(u8, entity_name, "quot")) return '"';
    // Numeric character references
    if (entity_name.len > 0 and entity_name[0] == '#') {
        if (entity_name.len > 1 and entity_name[1] == 'x') {
            // Hex
            const val = std.fmt.parseInt(u8, entity_name[2..], 16) catch return null;
            return val;
        } else {
            // Decimal
            const val = std.fmt.parseInt(u8, entity_name[1..], 10) catch return null;
            return val;
        }
    }
    return null;
}

// ─── Incremental Parser ─────────────────────────────────────────

const ParserState = enum {
    GetText,
    GetElementName,
    GetComment,
    GetProcessingInstruction,
    GotElementName,
    MaybeGetAttribute,
    EnsureEmptyTag,
    GetAttributeEquals,
    GetAttributeEquals2,
    GetAttributeValue,
    GetAttributeValue2,
    GotAttributeValue,
    GotClosingElementName,
    GotClosingElementName2,
    GetName,
    GetName2,
    EatWhitespace,
    GetEntity,
    GotEntity,
};

/// Callback types for incremental parsing
pub const ElementStartedFn = *const fn (node: *Node, parent: ?*Node, level: usize, data: ?*anyopaque) void;
pub const TextProcessFn = *const fn (node_type: NodeType, text: []const u8, parent: ?*Node, level: usize, data: ?*anyopaque) bool;
pub const ElementCompleteFn = *const fn (node: *Node, parent: ?*Node, level: usize, data: ?*anyopaque) bool;

/// Incremental XML parser - parses character by character
pub const IncrementalParser = struct {
    allocator: std.mem.Allocator,
    state: ParserState = .GetText,
    return_state: ParserState = .GetText,
    buf: std.ArrayList(u8) = .{},
    entity_buf: std.ArrayList(u8) = .{},
    attr_name: ?[]u8 = null,
    quote: u8 = 0,
    node_stack: std.ArrayList(*Node) = .{},

    // Callbacks
    element_started: ?ElementStartedFn = null,
    text_process: ?TextProcessFn = null,
    element_complete: ?ElementCompleteFn = null,
    callback_data: ?*anyopaque = null,

    pub fn init(
        allocator: std.mem.Allocator,
        element_started: ?ElementStartedFn,
        text_process: ?TextProcessFn,
        element_complete: ?ElementCompleteFn,
        callback_data: ?*anyopaque,
    ) IncrementalParser {
        return .{
            .allocator = allocator,
            .element_started = element_started,
            .text_process = text_process,
            .element_complete = element_complete,
            .callback_data = callback_data,
        };
    }

    pub fn deinit(self: *IncrementalParser) void {
        // Release remaining nodes on stack
        for (self.node_stack.items) |node| {
            node.deinit();
        }
        self.node_stack.deinit(self.allocator);
        self.buf.deinit(self.allocator);
        self.entity_buf.deinit(self.allocator);
        if (self.attr_name) |an| self.allocator.free(an);
    }

    fn currentParent(self: *const IncrementalParser) ?*Node {
        if (self.node_stack.items.len > 0)
            return self.node_stack.items[self.node_stack.items.len - 1];
        return null;
    }

    fn notifyStarted(self: *IncrementalParser, node: *Node) void {
        if (self.element_started) |cb| {
            cb(node, self.currentParent(), self.node_stack.items.len -| 1, self.callback_data);
        }
    }

    fn maybeLink(self: *IncrementalParser, node: ?*Node, node_type: NodeType, text: ?[]const u8) void {
        const parent = self.currentParent();

        // For non-element types, check text_process callback
        if (node_type != .Element) {
            if (self.text_process) |tp| {
                if (!tp(node_type, text orelse "", parent, self.node_stack.items.len, self.callback_data))
                    return;
            }
        }

        // Create text/comment/PI node if not an element
        var the_node: *Node = undefined;
        if (node_type != .Element) {
            const t = text orelse "";
            the_node = switch (node_type) {
                .Text => Node.newTextOwned(self.allocator, t) catch return,
                .Comment => Node.newComment(self.allocator, t) catch return,
                .ProcessingInstruction => Node.newPI(self.allocator, t) catch return,
                .Element => unreachable,
            };
        } else {
            the_node = node orelse return;
        }

        // Check element_complete callback - if it returns true, link to parent
        var should_link = true;
        if (self.element_complete) |cb| {
            should_link = cb(the_node, parent, self.node_stack.items.len, self.callback_data);
        }

        if (should_link and parent != null) {
            parent.?.appendChild(the_node);
        } else if (node_type != .Element) {
            // No parent to link to, or callback declined — free the node we created
            the_node.deinit();
        }
        // For elements: callback is responsible for ownership when returning false
    }

    /// Feed data to the parser
    pub fn parse(self: *IncrementalParser, data: []const u8) !void {
        var i: usize = 0;
        while (i < data.len) {
            const ch = data[i];
            switch (self.state) {
                .GetText => {
                    switch (ch) {
                        '<' => {
                            if (self.buf.items.len > 0) {
                                self.maybeLink(null, .Text, self.buf.items);
                                self.buf.clearRetainingCapacity();
                            }
                            self.state = .GetElementName;
                        },
                        '&' => {
                            self.state = .GetEntity;
                            self.return_state = .GetText;
                        },
                        else => try self.buf.append(self.allocator, ch),
                    }
                    i += 1;
                },

                .GetElementName => {
                    self.state = .GetName;
                    switch (ch) {
                        '!' => {
                            self.state = .GetComment;
                            i += 1;
                        },
                        '?' => {
                            self.state = .GetProcessingInstruction;
                            i += 1;
                        },
                        '/' => {
                            self.return_state = .GotClosingElementName;
                            i += 1;
                        },
                        else => {
                            self.return_state = .GotElementName;
                            // Don't increment i - reprocess this char in GetName
                        },
                    }
                },

                .GetComment => {
                    try self.buf.append(self.allocator, ch);
                    i += 1;
                    if (ch == '>' and self.buf.items.len > 4) {
                        const len = self.buf.items.len;
                        if (self.buf.items[len - 3] == '-' and self.buf.items[len - 2] == '-') {
                            // End of comment: "...-->" in buf (but '>' just added is at end)
                            // Actually buf has "!-- content -->" at this point
                            // The C code checks buf[len-2..len] == "--" and buf[len-3] != '-'
                            // Our buf includes the '>' we just appended, so check items[len-3] == '-' and items[len-2] == '-'
                            // Wait - we appended ch='>' so buf ends with "-->", check items[len-3]=='-', items[len-2]=='-'
                            // Actually len-1 is '>', len-2 is '-', len-3 is '-'
                            // So check items[len-2]=='-' and items[len-3]=='-'
                            // But we already checked above - let me re-examine:
                            // buf has: "!--...-->"
                            // len-1 = '>', len-2 = '-', len-3 = '-'
                            // We're checking items[len-3]=='-' && items[len-2]=='-' which is correct
                            self.maybeLink(null, .Comment, self.buf.items);
                            self.buf.clearRetainingCapacity();
                            self.state = .GetText;
                        }
                    }
                },

                .GetProcessingInstruction => {
                    try self.buf.append(self.allocator, ch);
                    i += 1;
                    if (ch == '>' and self.buf.items.len > 1) {
                        if (self.buf.items[self.buf.items.len - 2] == '?') {
                            self.maybeLink(null, .ProcessingInstruction, self.buf.items);
                            self.buf.clearRetainingCapacity();
                            self.state = .GetText;
                        }
                    }
                },

                .GotElementName => {
                    // Create element from buf content
                    const element_name = try self.allocator.dupe(u8, self.buf.items);
                    self.buf.clearRetainingCapacity();
                    const new_node = try Node.newElement(self.allocator, element_name);
                    new_node.owns_name = true;
                    try self.node_stack.append(self.allocator, new_node);
                    self.state = .EatWhitespace;
                    self.return_state = .MaybeGetAttribute;
                    // Don't increment i - reprocess
                },

                .MaybeGetAttribute => {
                    switch (ch) {
                        '/' => {
                            self.state = .EnsureEmptyTag;
                            i += 1;
                        },
                        '>' => {
                            // Notify element started
                            if (self.node_stack.items.len > 0) {
                                const top_node = self.node_stack.items[self.node_stack.items.len - 1];
                                self.notifyStarted(top_node);
                            }
                            self.state = .GetText;
                            i += 1;
                        },
                        else => {
                            self.state = .GetName;
                            self.return_state = .GetAttributeEquals;
                            // Don't increment - reprocess
                        },
                    }
                },

                .EnsureEmptyTag => {
                    if (ch != '>') return error_mod.Error.ParseError;
                    self.state = .GotClosingElementName2;
                    // Don't increment - reprocess in GotClosingElementName2
                },

                .GetAttributeEquals => {
                    // Store attribute name
                    if (self.attr_name) |old| self.allocator.free(old);
                    self.attr_name = try self.allocator.dupe(u8, self.buf.items);
                    self.buf.clearRetainingCapacity();
                    self.state = .EatWhitespace;
                    self.return_state = .GetAttributeEquals2;
                    // Don't increment - reprocess
                },

                .GetAttributeEquals2 => {
                    if (ch != '=') return error_mod.Error.ParseError;
                    self.state = .EatWhitespace;
                    self.return_state = .GetAttributeValue;
                    i += 1;
                },

                .GetAttributeValue => {
                    if (ch == '"' or ch == '\'') {
                        self.quote = ch;
                    } else {
                        return error_mod.Error.ParseError;
                    }
                    self.state = .GetAttributeValue2;
                    i += 1;
                },

                .GetAttributeValue2 => {
                    if (ch == '<') {
                        return error_mod.Error.ParseError;
                    } else if (ch == self.quote) {
                        self.state = .GotAttributeValue;
                        i += 1;
                    } else if (ch == '&') {
                        self.state = .GetEntity;
                        self.return_state = .GetAttributeValue2;
                        i += 1;
                    } else {
                        try self.buf.append(self.allocator, ch);
                        i += 1;
                    }
                },

                .GotAttributeValue => {
                    // Set attribute on current element
                    if (self.node_stack.items.len > 0) {
                        const top_node = self.node_stack.items[self.node_stack.items.len - 1];
                        if (self.attr_name) |an| {
                            try top_node.setAttribute(an, self.buf.items);
                        }
                    }
                    if (self.attr_name) |an| {
                        self.allocator.free(an);
                        self.attr_name = null;
                    }
                    self.buf.clearRetainingCapacity();
                    self.state = .EatWhitespace;
                    self.return_state = .MaybeGetAttribute;
                    // Don't increment - reprocess
                },

                .GotClosingElementName => {
                    // Validate closing tag matches
                    if (self.node_stack.items.len == 0) return error_mod.Error.ParseError;
                    self.buf.clearRetainingCapacity();
                    self.state = .EatWhitespace;
                    self.return_state = .GotClosingElementName2;
                    // Don't increment - reprocess
                },

                .GotClosingElementName2 => {
                    if (ch != '>') return error_mod.Error.ParseError;
                    if (self.node_stack.pop()) |popped_node| {
                        self.maybeLink(popped_node, .Element, null);
                    }
                    self.state = .GetText;
                    i += 1;
                },

                .GetName => {
                    if (isNameStartChar(ch)) {
                        try self.buf.append(self.allocator, ch);
                        self.state = .GetName2;
                        i += 1;
                    } else {
                        return error_mod.Error.ParseError;
                    }
                },

                .GetName2 => {
                    if (isNameChar(ch)) {
                        try self.buf.append(self.allocator, ch);
                        i += 1;
                    } else {
                        self.state = self.return_state;
                        // Don't increment - reprocess
                    }
                },

                .EatWhitespace => {
                    if (!isWhitespace(ch)) {
                        self.state = self.return_state;
                        // Don't increment - reprocess
                    } else {
                        i += 1;
                    }
                },

                .GetEntity => {
                    if (isNameChar(ch) or ch == '#') {
                        try self.entity_buf.append(self.allocator, ch);
                        i += 1;
                    } else {
                        self.state = .GotEntity;
                        // Don't increment - reprocess
                    }
                },

                .GotEntity => {
                    if (ch != ';') return error_mod.Error.ParseError;
                    if (resolveEntity(self.entity_buf.items)) |resolved| {
                        try self.buf.append(self.allocator, resolved);
                    }
                    self.entity_buf.clearRetainingCapacity();
                    self.state = self.return_state;
                    i += 1;
                },
            }
        }
    }
};

// ─── Convenience Parser ─────────────────────────────────────────

/// Simple parser that builds a full DOM tree
pub const XmlParser = struct {
    allocator: std.mem.Allocator,
    incremental: IncrementalParser,
    document: ?*Node = null,

    pub fn init(allocator: std.mem.Allocator) XmlParser {
        return .{
            .allocator = allocator,
            .incremental = IncrementalParser.init(allocator, null, null, parserNodeComplete, null),
        };
    }

    pub fn deinit(self: *XmlParser) void {
        self.incremental.deinit();
        if (self.document) |doc| {
            doc.deinit();
        }
    }

    pub fn parse(self: *XmlParser, data: []const u8) !void {
        // Set callback data to self
        self.incremental.callback_data = @ptrCast(self);
        try self.incremental.parse(data);
    }

    pub fn getDocument(self: *XmlParser) ?*Node {
        return self.document;
    }

    fn parserNodeComplete(node: *Node, _: ?*Node, level: usize, data: ?*anyopaque) bool {
        if (level == 0 and node.node_type == .Element) {
            if (data) |d| {
                const self: *XmlParser = @ptrCast(@alignCast(d));
                if (self.document == null) {
                    self.document = node;
                    return false; // Don't link to parent (there is none)
                }
            }
        }
        return true; // Link to parent
    }
};

/// Parse an XML string and return the document root
pub fn parseString(allocator: std.mem.Allocator, input: []const u8) !*Node {
    var p = XmlParser.init(allocator);
    p.incremental.callback_data = @ptrCast(&p);
    defer {
        p.document = null; // Prevent deinit from destroying the document
        p.deinit();
    }
    try p.parse(input);
    return p.document orelse error_mod.Error.ParseError;
}

// ─── Tests ──────────────────────────────────────────────────────

test "xml node creation" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElement(allocator, "root");
    defer root.deinit();

    try std.testing.expectEqualSlices(u8, "root", root.getName());
    try std.testing.expectEqual(NodeType.Element, root.node_type);
}

test "xml attributes" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const node = try Node.newElementOwned(allocator, "test");
    defer node.deinit();

    try node.setAttribute("id", "42");
    try node.setAttribute("name", "hello");

    try std.testing.expectEqualSlices(u8, "42", node.getAttribute("id").?);
    try std.testing.expectEqualSlices(u8, "hello", node.getAttribute("name").?);
    try std.testing.expectEqual(@as(usize, 2), node.numAttrs());

    // Overwrite
    try node.setAttribute("id", "99");
    try std.testing.expectEqualSlices(u8, "99", node.getAttribute("id").?);
    try std.testing.expectEqual(@as(usize, 2), node.numAttrs());

    // Clear
    node.clearAttribute("id");
    try std.testing.expect(node.getAttribute("id") == null);
    try std.testing.expectEqual(@as(usize, 1), node.numAttrs());
}

test "xml tree structure" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElementOwned(allocator, "root");
    defer root.deinit();

    const a = try Node.newElementOwned(allocator, "a");
    const b = try Node.newElementOwned(allocator, "b");
    const c = try Node.newElementOwned(allocator, "c");

    root.appendChild(a);
    root.appendChild(b);
    root.appendChild(c);

    try std.testing.expectEqual(@as(usize, 3), root.countChildren());
    try std.testing.expectEqualSlices(u8, "a", root.child.?.getName());
    try std.testing.expectEqualSlices(u8, "c", root.last_child.?.getName());
    try std.testing.expectEqualSlices(u8, "b", root.child.?.next.?.getName());

    // Parent links
    try std.testing.expect(a.parent == root);
    try std.testing.expect(b.parent == root);

    // Unlink middle child
    b.unlink();
    try std.testing.expectEqual(@as(usize, 2), root.countChildren());
    try std.testing.expect(a.next == c);
    try std.testing.expect(c.prev == a);
    b.deinit();
}

test "xml tree walking" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElementOwned(allocator, "root");
    defer root.deinit();

    const a = try Node.newElementOwned(allocator, "a");
    const b = try Node.newElementOwned(allocator, "b");
    const a1 = try Node.newElementOwned(allocator, "a1");

    root.appendChild(a);
    root.appendChild(b);
    a.appendChild(a1);

    // Walk with descend
    const first = root.walkNext(root, .Descend);
    try std.testing.expect(first != null);
    try std.testing.expectEqualSlices(u8, "a", first.?.getName());

    const second = first.?.walkNext(root, .Descend);
    try std.testing.expect(second != null);
    try std.testing.expectEqualSlices(u8, "a1", second.?.getName());

    const third = second.?.walkNext(root, .Descend);
    try std.testing.expect(third != null);
    try std.testing.expectEqualSlices(u8, "b", third.?.getName());

    const fourth = third.?.walkNext(root, .Descend);
    try std.testing.expect(fourth == null);
}

test "xml findElement" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElementOwned(allocator, "root");
    defer root.deinit();

    const ch1 = try Node.newElementOwned(allocator, "channel");
    try ch1.setAttribute("id", "1");
    root.appendChild(ch1);

    const ch2 = try Node.newElementOwned(allocator, "channel");
    try ch2.setAttribute("id", "2");
    root.appendChild(ch2);

    const dim = try Node.newElementOwned(allocator, "dim");
    ch1.appendChild(dim);

    // Find by name
    const found = root.findElement(root, "channel", null, null, .Descend);
    try std.testing.expect(found != null);
    try std.testing.expectEqualSlices(u8, "1", found.?.getAttribute("id").?);

    // Find by name + attr value
    const found2 = root.findElement(root, "channel", "id", "2", .Descend);
    try std.testing.expect(found2 != null);
    try std.testing.expect(found2.? == ch2);

    // Find nested
    const found3 = root.findElement(root, "dim", null, null, .Descend);
    try std.testing.expect(found3 != null);
    try std.testing.expect(found3.? == dim);
}

test "xml parse simple" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const doc = try parseString(allocator, "<root><child name=\"hello\"/></root>");
    defer doc.deinit();

    try std.testing.expectEqualSlices(u8, "root", doc.getName());
    try std.testing.expectEqual(@as(usize, 1), doc.countChildren());

    const child_node = doc.child.?;
    try std.testing.expectEqualSlices(u8, "child", child_node.getName());
    try std.testing.expectEqualSlices(u8, "hello", child_node.getAttribute("name").?);
}

test "xml parse with text" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const doc = try parseString(allocator, "<p>Hello &amp; world</p>");
    defer doc.deinit();

    try std.testing.expectEqualSlices(u8, "p", doc.getName());
    const text_node = doc.child.?;
    try std.testing.expectEqual(NodeType.Text, text_node.node_type);
    try std.testing.expectEqualSlices(u8, "Hello & world", text_node.text.?);
}

test "xml parse nested" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const doc = try parseString(allocator,
        \\<sie>
        \\  <test id="1" name="T1">
        \\    <channel id="10"/>
        \\    <channel id="11"/>
        \\  </test>
        \\</sie>
    );
    defer doc.deinit();

    try std.testing.expectEqualSlices(u8, "sie", doc.getName());

    const test_node = doc.findElement(doc, "test", null, null, .Descend);
    try std.testing.expect(test_node != null);
    try std.testing.expectEqualSlices(u8, "1", test_node.?.getAttribute("id").?);

    const ch2 = doc.findElement(doc, "channel", "id", "11", .Descend);
    try std.testing.expect(ch2 != null);
}

test "xml output" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const root = try Node.newElementOwned(allocator, "root");
    defer root.deinit();

    const child_elem = try Node.newElementOwned(allocator, "item");
    try child_elem.setAttribute("key", "val");
    root.appendChild(child_elem);

    const xml_str = try root.toXml(allocator);
    defer allocator.free(xml_str);

    // Should contain the element
    try std.testing.expect(std.mem.indexOf(u8, xml_str, "<root>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml_str, "<item key=\"val\"/>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml_str, "</root>") != null);
}

test "xml entity references" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const doc = try parseString(allocator, "<r a=\"1&amp;2\">&lt;tag&gt;</r>");
    defer doc.deinit();

    // Attribute value should have decoded entity
    try std.testing.expectEqualSlices(u8, "1&2", doc.getAttribute("a").?);
    // Text content should have decoded entities
    const text_node = doc.child.?;
    try std.testing.expectEqualSlices(u8, "<tag>", text_node.text.?);
}

test "xml pack" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Build a tree: <ch id="1"><dim index="0"/><tag id="core:units">volts</tag></ch>
    const ch = try Node.newElement(allocator, "ch");
    defer ch.deinit();
    try ch.setAttribute("id", "1");

    const dim = try Node.newElement(allocator, "dim");
    try dim.setAttribute("index", "0");
    ch.appendChild(dim);

    const tag_el = try Node.newElement(allocator, "tag");
    try tag_el.setAttribute("id", "core:units");
    const tag_text = try Node.newTextOwned(allocator, "volts");
    tag_el.appendChild(tag_text);
    ch.appendChild(tag_el);

    // Pack (compact copy) the tree
    const pack_copy = try ch.pack(allocator);
    defer pack_copy.deinit();

    // Packed tree is structurally identical but independent
    try std.testing.expectEqualSlices(u8, "ch", pack_copy.getName());
    try std.testing.expectEqualSlices(u8, "1", pack_copy.getAttribute("id").?);

    // Children are preserved
    const packed_dim = pack_copy.child.?;
    try std.testing.expectEqualSlices(u8, "dim", packed_dim.getName());
    try std.testing.expectEqualSlices(u8, "0", packed_dim.getAttribute("index").?);

    const packed_tag = packed_dim.next.?;
    try std.testing.expectEqualSlices(u8, "tag", packed_tag.getName());
    try std.testing.expectEqualSlices(u8, "core:units", packed_tag.getAttribute("id").?);
    try std.testing.expectEqualSlices(u8, "volts", packed_tag.child.?.text.?);

    // Serialization matches
    const orig_xml = try ch.toXml(allocator);
    defer allocator.free(orig_xml);
    const pack_xml = try pack_copy.toXml(allocator);
    defer allocator.free(pack_xml);
    try std.testing.expectEqualSlices(u8, orig_xml, pack_xml);

    // Mutation of packed tree does not affect original
    try pack_copy.setAttribute("id", "99");
    try std.testing.expectEqualSlices(u8, "1", ch.getAttribute("id").?);
    try std.testing.expectEqualSlices(u8, "99", pack_copy.getAttribute("id").?);
}
