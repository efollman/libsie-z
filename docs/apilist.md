## Public API

### `block` — SIE Binary Blocks

| Function | Description |
|----------|-------------|
| `crc32(buf) u32` | Compute CRC-32 checksum |
| `Block.init(allocator) Block` | Create empty block |
| `Block.expand(size) !void` | Grow payload buffer |
| `Block.parseFromData(allocator, data) !Block` | Parse block from bytes |
| `Block.writeTo(writer) !void` | Serialize to writer |
| `Block.getGroup() u32` | Get group ID |
| `Block.getPayloadSize() u32` | Get payload size |
| `Block.getTotalSize() u32` | Get total size (with overhead) |
| `Block.getPayload() []const u8` | Get payload bytes |
| `Block.getPayloadMut() []u8` | Get mutable payload |
| `Block.validateChecksum() bool` | Verify CRC-32 |
| `Block.isXml() bool` | Check if XML block (group 0) |
| `Block.isIndex() bool` | Check if index block (group 1) |
| `Block.deinit()` | Free payload memory |
| `Block.format(...)` | Debug formatter |

### `byteswap` — Byte Ordering

| Function | Description |
|----------|-------------|
| `ntoh16(u16) u16` | Network to host 16-bit |
| `hton16(u16) u16` | Host to network 16-bit |
| `ntoh32(u32) u32` | Network to host 32-bit |
| `hton32(u32) u32` | Host to network 32-bit |
| `ntoh64(u64) u64` | Network to host 64-bit |
| `hton64(u64) u64` | Host to network 64-bit |
| `swapF64Bytes(f64) f64` | Swap float64 byte order |
| `ntohF64(f64) f64` | Network to host float64 |
| `htonF64(f64) f64` | Host to network float64 |

### `channel` — Data Series

| Function | Description |
|----------|-------------|
| `Channel.init(allocator, id, name) Channel` | Create channel |
| `Channel.deinit()` | Cleanup |
| `Channel.getId() u32` | Get channel ID |
| `Channel.getName() []const u8` | Get channel name |
| `Channel.getTestId() u32` | Get parent test ID |
| `Channel.addDimension(dim) !void` | Add a dimension |
| `Channel.getDimensions() []const Dimension` | Get all dimensions |
| `Channel.getDimension(index) ?*const Dimension` | Get dimension by index |
| `Channel.getNumDimensions() usize` | Count dimensions |
| `Channel.addTag(tag) !void` | Add metadata tag |
| `Channel.getTags() []const Tag` | Get all tags |
| `Channel.findTag(key) ?*const Tag` | Find tag by key |
| `Channel.setRawXml(xml, owned)` | Set raw XML definition |
| `Channel.setExpandedXml(xml, owned)` | Set expanded XML |
| `Channel.format(...)` | Debug formatter |

### `channel_spigot` — Channel Data Pipeline

| Function | Description |
|----------|-------------|
| `ChannelSpigot.init(allocator, ...) ChannelSpigot` | Create from channel + file |
| `ChannelSpigot.deinit()` | Cleanup |
| `ChannelSpigot.get() !?*Output` | Get next output chunk |
| `ChannelSpigot.numBlocks() usize` | Total block count |
| `ChannelSpigot.seek(target) u64` | Seek to scan position |
| `ChannelSpigot.tell() u64` | Current scan position |
| `ChannelSpigot.isDone() bool` | Check if exhausted |
| `ChannelSpigot.reset()` | Reset to beginning |
| `ChannelSpigot.disableTransforms(bool)` | Toggle transforms |
| `ChannelSpigot.transformOutput(output)` | Apply transforms |
| `ChannelSpigot.clearOutput()` | Clear output buffer |
| `ChannelSpigot.setScanLimit(limit)` | Set max scans |
| `ChannelSpigot.lowerBound(dim, value) !?BoundResult` | Binary search lower |
| `ChannelSpigot.upperBound(dim, value) !?BoundResult` | Binary search upper |

### `combiner` — Dimension Remapping

| Function | Description |
|----------|-------------|
| `Combiner.init(allocator, num_dims) !Combiner` | Create combiner |
| `Combiner.deinit()` | Cleanup |
| `Combiner.addMapping(in_dim, out_dim)` | Map input→output dim |
| `Combiner.combine(input) !*Output` | Remap dimensions |

### `compiler` — XML-to-Bytecode Compiler

| Function | Description |
|----------|-------------|
| `classifyValue(name) ?ValueType` | Classify identifier type |
| `parseNumber(name) ?f64` | Parse numeric literal |
| `Compiler.init(allocator) Compiler` | Create compiler |
| `Compiler.deinit()` | Cleanup |
| `Compiler.compile(source) !CompileResult` | Compile full XML program |
| `Compiler.compileNode(node) !void` | Compile single node |
| `Compiler.compileChildren(node) !void` | Compile child nodes |
| `Compiler.compileExprNode(ret, node) ![]const u8` | Compile expression |
| `Compiler.emit0..3(op, ...) !void` | Emit instructions |
| `Compiler.emitBranch(op, label) !void` | Emit branch |
| `Compiler.fixupLabels()` | Resolve label addresses |

### `context` — Library Context

| Function | Description |
|----------|-------------|
| `Context.init(config) !Context` | Create context |
| `Context.deinit()` | Cleanup |
| `Context.setException(message) !void` | Set error |
| `Context.hasException() bool` | Check for error |
| `Context.getException() ?Exception` | Get error details |
| `Context.clearException()` | Clear error |
| `Context.internString(str) ![]const u8` | Intern string |
| `Context.cleanupPush(func, target) !void` | Push cleanup action |
| `Context.cleanupPop(fire)` | Pop cleanup |
| `Context.cleanupPopAll()` | Pop all cleanups |
| `Context.errorContextPush(msg) !void` | Push error context |
| `Context.errorContextPop()` | Pop error context |
| `Context.setProgressCallbacks(cb)` | Set progress hooks |
| `Context.progressMessage(msg)` | Report progress |
| `Context.recursionEnter() !void` | Recursion guard enter |
| `Context.recursionLeave()` | Recursion guard leave |

### `decoder` — Bytecode VM

| Function | Description |
|----------|-------------|
| `Decoder.init(allocator, ...) Decoder` | Create decoder |
| `Decoder.deinit()` | Cleanup |
| `Decoder.isEqual(other) bool` | Compare decoders |
| `Decoder.signature() u32` | Get signature hash |
| `Decoder.disassemble(allocator) ![]u8` | Disassemble to text |
| `DecoderMachine.init(allocator, decoder) !DecoderMachine` | Create VM |
| `DecoderMachine.deinit()` | Cleanup |
| `DecoderMachine.prep(data)` | Load data for execution |
| `DecoderMachine.run() !void` | Execute bytecode |
| `DecoderMachine.getOutput() ?*const Output` | Get decoded output |

### `dimension` — Axis Metadata

| Function | Description |
|----------|-------------|
| `Dimension.init(allocator, ...) Dimension` | Create dimension |
| `Dimension.deinit()` | Cleanup |
| `Dimension.getIndex() u32` | Get dimension index |
| `Dimension.getName() []const u8` | Get name |
| `Dimension.getGroup() u32` | Get group |
| `Dimension.getDecoderId() u32` | Get decoder ID |
| `Dimension.setDecoder(id, version)` | Set decoder info |
| `Dimension.addTag(tag) !void` | Add metadata tag |
| `Dimension.getTags() []const Tag` | Get all tags |
| `Dimension.findTag(key) ?*const Tag` | Find tag by key |
| `Dimension.format(...)` | Debug formatter |

### `file` — File I/O

| Function | Description |
|----------|-------------|
| `File.init(allocator, path) File` | Create file handle |
| `File.open() !void` | Open for reading |
| `File.close()` | Close handle |
| `File.deinit()` | Close and cleanup |
| `File.read(buffer) !usize` | Read bytes |
| `File.seek(offset) !void` | Seek absolute |
| `File.seekBy(delta) !void` | Seek relative |
| `File.tell() i64` | Current position |
| `File.size() i64` | File size |
| `File.isEof() bool` | At end of file |
| `File.readAll() ![]u8` | Read entire file |
| `File.readBlockAt(offset) !Block` | Read block at offset |
| `File.readBlock() !Block` | Read next block |
| `File.isSie() !bool` | Check SIE magic |
| `File.buildIndex() !void` | Build group index |
| `File.getGroupIndex(id) ?*FileGroupIndex` | Get group index |
| `File.getNumGroups() u32` | Count groups |
| `File.getHighestGroup() u32` | Highest group ID |
| `File.asIntake() Intake` | Create Intake interface |
| `File.searchBackwardsForMagic(max) !?i64` | Search backward for magic |
| `File.readBlockBackward() !Block` | Read block from end |
| `File.findBlockBackward(max) !Block` | Find block searching backward |
| `File.getUnindexedBlocks() !ArrayList(UnindexedBlock)` | Get uncatalogued blocks |
| `File.groupForEach(callback, extra)` | Iterate all groups with callback |

### `file_stream` — Incremental Stream-to-File Writer

| Function | Description |
|----------|-------------|
| `FileStream.init(allocator, path) FileStream` | Create file stream |
| `FileStream.open() !void` | Open file for writing |
| `FileStream.close()` | Close file handle |
| `FileStream.deinit()` | Close and cleanup |
| `FileStream.addStreamData(data) !usize` | Feed raw bytes, write complete blocks |
| `FileStream.isGroupClosed(group) bool` | Check if group is closed |
| `FileStream.getGroupIndex(id) ?*FileGroupIndex` | Get group index |
| `FileStream.getNumGroups() u32` | Count groups |
| `FileStream.getHighestGroup() u32` | Highest group ID |
| `FileStream.groupForEach(callback, extra)` | Iterate all groups with callback |
| `FileStream.readBlockAt(offset) !Block` | Read block at file offset |
| `FileStream.asIntake() Intake` | Create Intake interface |

### `group` — Block Group Tracking

| Function | Description |
|----------|-------------|
| `Group.init(allocator, id) Group` | Create group |
| `Group.deinit()` | Cleanup |
| `Group.getId() u32` | Get group ID |
| `Group.isXmlGroup() bool` | Is XML group (0) |
| `Group.isIndexGroup() bool` | Is index group (1) |
| `Group.getNumBlocks() usize` | Block count |
| `Group.getTotalPayloadBytes() u64` | Total payload size |
| `Group.isClosed() bool` | Group complete |
| `Group.close()` | Mark closed |
| `Group.recordBlock(size)` | Record a block |

### `group_spigot` — Group-Level Data Access

| Function | Description |
|----------|-------------|
| `GroupSpigot.init(allocator, file, group_id) GroupSpigot` | Create from file+group |
| `GroupSpigot.deinit()` | Cleanup |
| `GroupSpigot.numBlocks() usize` | Block count |
| `GroupSpigot.get() !?[]const u8` | Get next payload |
| `GroupSpigot.getAt(index) !?[]const u8` | Get payload by index |
| `GroupSpigot.seek(target) u64` | Seek to position |
| `GroupSpigot.tell() u64` | Current position |
| `GroupSpigot.isDone() bool` | Check if exhausted |
| `GroupSpigot.reset()` | Reset to beginning |

### `histogram` — Multi-Dimensional Binning

| Function | Description |
|----------|-------------|
| `Histogram.init(allocator, num_dims) !Histogram` | Create histogram |
| `Histogram.fromChannel(allocator, sf, ch) !Histogram` | Create from channel |
| `Histogram.deinit()` | Cleanup |
| `Histogram.addBound(dim, lower, upper) !void` | Add bin boundary |
| `Histogram.finalize() !void` | Finalize bin layout |
| `Histogram.flattenIndices(indices) usize` | Multi-dim → flat index |
| `Histogram.unflattenIndex(index, indices)` | Flat → multi-dim index |
| `Histogram.findBound(dim, lower, upper) usize` | Find bin by bounds |
| `Histogram.getNumDims() usize` | Dimension count |
| `Histogram.getNumBins(dim) usize` | Bin count for dim |
| `Histogram.getBin(indices) f64` | Get bin value |
| `Histogram.setBin(indices, value)` | Set bin value |
| `Histogram.setBinByBounds(pairs, value) !void` | Set bin by bound pairs |
| `Histogram.getNextNonzeroBin(start, indices) f64` | Iterate nonzero bins |
| `Histogram.getBinBounds(dim, lower, upper)` | Get bin boundaries |

### `intake` — Abstract Data Source

| Function | Description |
|----------|-------------|
| `Intake.init(allocator, vtable, ctx) Intake` | Create from vtable |
| `Intake.deinit()` | Cleanup |
| `Intake.getGroupHandle(group) ?GroupHandle` | Get group handle |
| `Intake.getGroupNumBlocks(handle) usize` | Block count |
| `Intake.getGroupNumBytes(handle) u64` | Byte count |
| `Intake.getGroupBlockSize(handle, entry) u32` | Block size |
| `Intake.readGroupBlock(handle, entry, blk) !void` | Read block |
| `Intake.isGroupClosed(handle) bool` | Group complete |
| `Intake.addStreamData(data) usize` | Feed stream data |
| `Intake.addTest(id, name) !void` | Register test |
| `Intake.addTag(key, value, group) !void` | Add tag |
| `Intake.findTest(id) ?TestEntry` | Find test |
| `Intake.findChannel(id) ?ChannelEntry` | Find channel |
| `nullIntake(allocator) Intake` | Create no-op intake |

### `output` — Output Data Buffers

| Function | Description |
|----------|-------------|
| `Output.init(allocator, num_dims) !Output` | Create output |
| `Output.deinit()` | Cleanup |
| `Output.setType(dim, type)` | Set dimension type |
| `Output.resize(dim, max) !void` | Resize dimension buffer |
| `Output.grow(dim) !void` | Double dimension capacity |
| `Output.growTo(dim, target) !void` | Grow to target |
| `Output.trim(start, size)` | Keep row subrange |
| `Output.clear()` | Clear rows (keep allocs) |
| `Output.clearAndShrink()` | Clear and free buffers |
| `Output.setFloat64Dimension(dim, data) !void` | Set float64 column |
| `Output.setRawDimension(dim, data) !void` | Set raw column |
| `Output.setRaw(dim, scan, data) !void` | Set single raw cell |
| `Output.getFloat64(dim, row) ?f64` | Get float64 value |
| `Output.getRaw(dim, row) ?RawData` | Get raw value |
| `Output.getDimensionType(dim) ?OutputType` | Get dimension type |
| `Output.deepCopy(allocator) !Output` | Deep clone all data |
| `Output.compare(other) bool` | Compare equality |
| `Output.format(...)` | Debug formatter |

### `plot_crusher` — Data Reduction

| Function | Description |
|----------|-------------|
| `PlotCrusher.init(allocator, ideal_scans) PlotCrusher` | Create crusher |
| `PlotCrusher.deinit()` | Cleanup |
| `PlotCrusher.work(input) !bool` | Process input chunk |
| `PlotCrusher.finalize()` | Finalize output |
| `PlotCrusher.getOutput() ?*const Output` | Get reduced output |

### `recover` — File Recovery

| Function | Description |
|----------|-------------|
| `recover(allocator, path, mod) !RecoverResult` | Full 3-pass file recovery |
| `tryGlue(left, right, size, offset, mod) ?usize` | Try gluing two buffers |
| `scanForMagic(allocator, data) !ArrayList(u64)` | Find magic byte positions |
| `RecoverResult.deinit(allocator)` | Free results |
| `RecoverResult.toJson(allocator) ![]u8` | Serialize to JSON |

### `relation` — Key-Value Store

| Function | Description |
|----------|-------------|
| `Relation.init(allocator) Relation` | Create empty relation |
| `Relation.deinit()` | Cleanup |
| `Relation.count() usize` | Entry count |
| `Relation.value(name) ?[]const u8` | Get value by name |
| `Relation.setValue(name, val) !void` | Set/add entry |
| `Relation.deleteAtIndex(idx)` | Delete entry |
| `Relation.clear()` | Clear all entries |
| `Relation.clone() !Relation` | Deep copy |
| `Relation.merge(other) !void` | Merge entries |
| `Relation.intValue(name) ?i64` | Get as integer |
| `Relation.floatValue(name) ?f64` | Get as float |
| `Relation.splitString(...) !Relation` | Parse delimited pairs |
| `Relation.decodeQueryString(allocator, query) !Relation` | Parse URL query string |

### `sie_file` — High-Level SIE File API

| Function | Description |
|----------|-------------|
| `SieFile.open(allocator, path) !SieFile` | Open and parse SIE file |
| `SieFile.deinit()` | Cleanup |
| `SieFile.getAllChannels() []*Channel` | Get all channels |
| `SieFile.getFileTags() []const Tag` | Get file-level tags |
| `SieFile.findChannel(id) ?*Channel` | Find channel by ID |
| `SieFile.getFile() *File` | Get underlying file |
| `SieFile.getXmlDef() *XmlDefinition` | Get XML definition |
| `SieFile.attachSpigot(ch) !ChannelSpigot` | Create data spigot |
| `SieFile.getDecoder(id) ?*const Decoder` | Get decoder by ID |

### `sifter` — Subset Extraction

| Function | Description |
|----------|-------------|
| `Sifter.init(allocator, writer) Sifter` | Create sifter |
| `Sifter.deinit()` | Cleanup |
| `Sifter.findId(map_type, intake_id, from_id) ?u32` | Lookup mapped ID |
| `Sifter.mapId(map_type, intake_id, from_id) !u32` | Map ID (auto-assign) |
| `Sifter.setId(map_type, intake_id, from, to) !void` | Force ID mapping |
| `Sifter.remapXml(intake_id, node) !void` | Rewrite XML IDs |
| `Sifter.totalEntries() usize` | Count mapped entries |
| `Sifter.addChannel(allocator, intake, ...) !void` | Add channel to output |
| `Sifter.finish(file) !void` | Finalize sifted output |

### `spigot` — Base Data Pipeline

| Function | Description |
|----------|-------------|
| `Spigot.init(allocator, size) Spigot` | Create base spigot |
| `Spigot.deinit()` | Cleanup |
| `Spigot.setScanLimit(limit)` | Set max scans |
| `Spigot.prep()` | Prepare for reading |
| `Spigot.get() ?*Output` | Get next output chunk |
| `Spigot.seek(target) u64` | Seek to position |
| `Spigot.tell() u64` | Current position |
| `Spigot.isDone() bool` | Check if exhausted |
| `Spigot.clearOutput()` | Clear output buffer |
| `Spigot.lowerBound(dim, value) ?SearchResult` | Binary search lower |
| `Spigot.upperBound(dim, value) ?SearchResult` | Binary search upper |

### `stream` — Stream Intake

| Function | Description |
|----------|-------------|
| `Stream.init(allocator) Stream` | Create stream |
| `Stream.deinit()` | Cleanup |
| `Stream.addStreamData(data) !usize` | Feed data and parse blocks |
| `Stream.getGroupHandle(group_id) ?*StreamGroupIndex` | Get group handle |
| `Stream.getGroupNumBlocks(group_id) usize` | Block count |
| `Stream.getGroupNumBytes(group_id) u64` | Byte count |
| `Stream.isGroupClosed(group_id) bool` | Group complete |
| `Stream.getData() []const u8` | Raw data buffer |
| `Stream.getNumGroups() u32` | Group count |
| `Stream.getGroupBlockSize(group_id, entry) u32` | Block payload size |
| `Stream.readGroupBlock(group_id, entry) !Block` | Read block by index |
| `Stream.asIntake() Intake` | Create Intake interface |
| `Stream.groupForEach(callback, extra)` | Iterate all groups with callback |
| `Stream.groupForEach(callback, extra)` | Iterate all groups with callback |

### `stringtable` — String Interning

| Function | Description |
|----------|-------------|
| `StringTable.init(allocator) StringTable` | Create table |
| `StringTable.deinit()` | Cleanup |
| `StringTable.intern(str) ![]const u8` | Intern string |
| `StringTable.get(str) ?[]const u8` | Lookup interned |
| `StringTable.contains(str) bool` | Check if interned |

### `tag` — Metadata Tags

| Function | Description |
|----------|-------------|
| `Tag.initString(allocator, key, value) !Tag` | Create string tag |
| `Tag.initBinary(allocator, key, value) !Tag` | Create binary tag |
| `Tag.initWithGroup(allocator, key, value, group) !Tag` | Create grouped tag |
| `Tag.isString() bool` | Is string value |
| `Tag.isBinary() bool` | Is binary value |
| `Tag.getId() []const u8` | Get key |
| `Tag.getString() ?[]const u8` | Get string value |
| `Tag.getBinary() ?[]const u8` | Get binary value |
| `Tag.getValueSize() usize` | Value byte size |
| `Tag.getGroup() u32` | Get group ID |
| `Tag.isFromGroup() bool` | Has group association |
| `Tag.deinit()` | Cleanup |
| `Tag.format(...)` | Debug formatter |

### `transform` — Data Transforms

| Function | Description |
|----------|-------------|
| `Transform.init(allocator, num_dims) !Transform` | Create transform set |
| `Transform.deinit()` | Cleanup |
| `Transform.setNone(dim)` | Set pass-through |
| `Transform.setLinear(dim, scale, offset)` | Set linear transform |
| `Transform.setMap(dim, values) !void` | Set lookup table |
| `Transform.setMapFromOutputData(dim, data) !void` | Set map from channel data |
| `Transform.setFromXformNode(dim, node) !void` | Configure from XML node |
| `Transform.apply(output)` | Apply transforms in-place |

### `writer` — SIE Block Writer

See `writer.zig` for the full Writer API (block buffering, XML/index groups, CRC-32, auto-flush).

### `xml` — XML DOM and Parsing

| Function | Description |
|----------|-------------|
| `Node.newElement(allocator, name) !*Node` | Create element node |
| `Node.newText(allocator, text) !*Node` | Create text node |
| `Node.newComment(allocator, text) !*Node` | Create comment node |
| `Node.newPI(allocator, text) !*Node` | Create processing instruction |
| `Node.clone(allocator) !*Node` | Deep copy |
| `Node.pack(allocator) !*Node` | Deep copy (legacy compat) |
| `Node.deinit()` | Free node and children |
| `Node.setAttribute(name, value) !void` | Set attribute |
| `Node.getAttribute(name) ?[]const u8` | Get attribute value |
| `Node.clearAttribute(name)` | Remove attribute |
| `Node.numAttrs() usize` | Attribute count |
| `Node.setAttributes(other) !void` | Copy all attributes from another node |
| `Node.attributeEqual(other, attr) bool` | Compare attribute across two nodes |
| `Node.getName() []const u8` | Get element name |
| `Node.nameEqual(other) bool` | Compare element names |
| `Node.appendChild(child)` | Append child node |
| `Node.prependChild(child)` | Prepend child node |
| `Node.insertAfter(new, target)` | Insert after target |
| `Node.insertBefore(new, target)` | Insert before target |
| `Node.unlink()` | Detach from parent |
| `Node.walkNext(top, descend) ?*Node` | Next node in tree walk |
| `Node.walkPrev(top, descend) ?*Node` | Previous node in tree walk |
| `Node.findElement(name, attr, value) ?*Node` | Find element by name+attr |
| `Node.getChildElement(name) ?*Node` | Find first child element |
| `Node.find(top, match_fn, data, descend) ?*Node` | Generic callback tree search |
| `Node.countChildren() usize` | Count child nodes |
| `Node.print()` | Output XML to stderr |
| `Node.toXml(allocator) ![]u8` | Serialize to XML string |
| `Node.outputXml(allocator, buf, indent) !void` | Serialize with indentation |
| `XmlParser.init(allocator) XmlParser` | Create parser |
| `XmlParser.deinit()` | Cleanup |
| `XmlParser.parse(data) !void` | Parse XML data |
| `XmlParser.getDocument() ?*Node` | Get parsed document |
| `parseString(allocator, input) !*Node` | One-shot parse from string |

### `xml_merge` — XML Definition Builder

| Function | Description |
|----------|-------------|
| `XmlDefinition.init(allocator) XmlDefinition` | Create definition |
| `XmlDefinition.deinit()` | Cleanup |
| `XmlDefinition.addString(data) !void` | Feed XML string data |
| `XmlDefinition.getSieNode() ?*Node` | Get root SIE node |
| `XmlDefinition.getChannel(id) ?*Node` | Get channel node by ID |
| `XmlDefinition.getTest(id) ?*Node` | Get test node by ID |
| `XmlDefinition.getDecoder(id) ?*Node` | Get decoder node by ID |
| `XmlDefinition.expand(type, id) !*Node` | Expand with base inheritance |
