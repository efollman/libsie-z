// XML merge tests — port of t_xml_merge.c
// Tests: definition merging, base expansion, private attributes, tag merging

const std = @import("std");
const libsie = @import("libsie");
const testing = std.testing;

const XmlDefinition = libsie.advanced.xml_merge.XmlDefinition;
const Node = libsie.advanced.xml.Node;

test "xml_merge: definition merge with base expansion" {
    // Port of test_merge1 — comprehensive merge test
    // Input: multiple <ch> definitions with base inheritance, <tag> elements,
    // private attributes, and loose tag floating elements
    var defn = XmlDefinition.init(testing.allocator);
    defer defn.deinit();

    const data =
        "<sie>" ++
        "<ch id=\"1\" private=\"1\">" ++
        "<tag id=\"foo\">bar</tag>" ++
        "<tag id=\"bar\">bar-value</tag>" ++
        "</ch>" ++
        "<ch id=\"1\">" ++
        "<tag id=\"foo\">foo-value</tag>" ++
        "</ch>" ++
        "<ch id=\"2\" base=\"1\">" ++
        "<tag id=\"foo\">new-foo-value</tag>" ++
        "</ch>" ++
        "<ch id=\"3\" base=\"1\" private=\"1\">" ++
        "<tag id=\"foo\">new-foo-value-2</tag>" ++
        "</ch>" ++
        "<tag ch=\"1\" id=\"direct\"/>" ++
        "<tag test=\"0\" ch=\"4\" dim=\"0\" id=\"direct-4\"/>" ++
        "</sie>";

    try defn.addString(data);

    try testing.expect(defn.getSieNode() != null);
    const sie = defn.getSieNode().?;

    // Verify merged structure via serialization
    const merged_output = try sie.toXml(testing.allocator);
    defer testing.allocator.free(merged_output);

    // Channel 1 should exist with merged tags
    const ch1 = defn.getChannel(1);
    try testing.expect(ch1 != null);

    // After merge, ch1 should have tag foo=foo-value (overwritten) and tag bar=bar-value (kept)
    var foo_tag: ?*Node = null;
    var bar_tag: ?*Node = null;
    var direct_tag: ?*Node = null;
    var cur = ch1.?.child;
    while (cur) |c| {
        if (c.node_type == .Element and std.mem.eql(u8, c.getName(), "tag")) {
            if (c.getAttribute("id")) |id| {
                if (std.mem.eql(u8, id, "foo")) foo_tag = c;
                if (std.mem.eql(u8, id, "bar")) bar_tag = c;
                if (std.mem.eql(u8, id, "direct")) direct_tag = c;
            }
        }
        cur = c.next;
    }

    try testing.expect(foo_tag != null);
    try testing.expect(bar_tag != null);

    // foo should have been replaced with "foo-value" from second <ch id=1>
    if (foo_tag.?.child) |text_node| {
        try testing.expectEqualStrings("foo-value", text_node.text.?);
    }

    // bar-value should be preserved from first <ch id=1>
    if (bar_tag.?.child) |text_node| {
        try testing.expectEqualStrings("bar-value", text_node.text.?);
    }

    // Channel 2 exists with base="1"
    const ch2 = defn.getChannel(2);
    try testing.expect(ch2 != null);
    try testing.expectEqualStrings("1", ch2.?.getAttribute("base").?);

    // Channel 3 exists with private="1"
    const ch3 = defn.getChannel(3);
    try testing.expect(ch3 != null);
    try testing.expectEqualStrings("1", ch3.?.getAttribute("private").?);
}

test "xml_merge: expand with base inheritance" {
    // Port of test_merge1 expansion — expand(ch, 2) should inherit from base ch 1
    var defn = XmlDefinition.init(testing.allocator);
    defer defn.deinit();

    const data =
        "<sie>" ++
        "<ch id=\"1\" private=\"1\">" ++
        "<tag id=\"foo\">foo-value</tag>" ++
        "<tag id=\"bar\">bar-value</tag>" ++
        "<tag id=\"direct\"/>" ++
        "</ch>" ++
        "<ch id=\"2\" base=\"1\">" ++
        "<tag id=\"foo\">new-foo-value</tag>" ++
        "</ch>" ++
        "</sie>";

    try defn.addString(data);

    // Expand channel 2 — should get base ch1's attributes + ch2's overrides
    const expanded = try defn.expand("ch", 2);
    defer expanded.deinit();

    // Should have id="2" and base="1"
    try testing.expectEqualStrings("2", expanded.getAttribute("id").?);
    try testing.expectEqualStrings("1", expanded.getAttribute("base").?);

    // Should NOT have private attribute (cleared during expansion)
    try testing.expect(expanded.getAttribute("private") == null);

    // Check tags: foo should be overridden to "new-foo-value"
    var found_foo = false;
    var found_bar = false;
    var found_direct = false;
    var child = expanded.child;
    while (child) |c| {
        if (c.node_type == .Element and std.mem.eql(u8, c.getName(), "tag")) {
            if (c.getAttribute("id")) |id| {
                if (std.mem.eql(u8, id, "foo")) {
                    found_foo = true;
                    if (c.child) |text| {
                        try testing.expectEqualStrings("new-foo-value", text.text.?);
                    }
                }
                if (std.mem.eql(u8, id, "bar")) {
                    found_bar = true;
                    if (c.child) |text| {
                        try testing.expectEqualStrings("bar-value", text.text.?);
                    }
                }
                if (std.mem.eql(u8, id, "direct")) found_direct = true;
            }
        }
        child = c.next;
    }

    try testing.expect(found_foo);
    try testing.expect(found_bar);
    try testing.expect(found_direct);
}

test "xml_merge: channel and test maps populated" {
    var defn = XmlDefinition.init(testing.allocator);
    defer defn.deinit();

    try defn.addString("<sie>" ++
        "<ch id=\"1\"><tag id=\"a\"/></ch>" ++
        "<ch id=\"2\"><tag id=\"b\"/></ch>" ++
        "<test id=\"0\"><ch id=\"1\"/></test>" ++
        "<decoder id=\"5\"><loop/></decoder>" ++
        "</sie>");

    try testing.expect(defn.getChannel(1) != null);
    try testing.expect(defn.getChannel(2) != null);
    try testing.expect(defn.getChannel(99) == null);

    try testing.expect(defn.getTest(0) != null);
    try testing.expect(defn.getTest(1) == null);

    try testing.expect(defn.getDecoder(5) != null);
    try testing.expect(defn.getDecoder(0) == null);
}

test "xml_merge: expand nonexistent returns error" {
    var defn = XmlDefinition.init(testing.allocator);
    defer defn.deinit();

    try defn.addString("<sie><ch id=\"1\"/></sie>");

    try testing.expectError(error.InvalidData, defn.expand("ch", 999));
}

test "xml_merge: tag replacement during merge" {
    // Two blocks merge — tag with same id gets replaced
    var defn = XmlDefinition.init(testing.allocator);
    defer defn.deinit();

    try defn.addString("<sie><ch id=\"1\"><tag id=\"x\">old</tag></ch></sie>");
    try defn.addString("<sie><ch id=\"1\"><tag id=\"x\">new</tag></ch></sie>");

    const ch = defn.getChannel(1).?;
    var cur = ch.child;
    while (cur) |c| {
        if (c.node_type == .Element and std.mem.eql(u8, c.getName(), "tag")) {
            if (c.getAttribute("id")) |id| {
                if (std.mem.eql(u8, id, "x")) {
                    if (c.child) |text| {
                        try testing.expectEqualStrings("new", text.text.?);
                    }
                    return;
                }
            }
        }
        cur = c.next;
    }
    return error.TestUnexpectedResult; // tag not found
}
