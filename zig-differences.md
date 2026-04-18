# Zig Implementation Differences

This document describes how the Zig port of libsie differs from the original C
library. The original C library is available at
[github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference)
or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).

---

## Ported Features

All core functionality from the C library has been ported to idiomatic Zig.
The following lists the C features and how they map to the Zig implementation.

### File Recovery (`sie_file_recover`)

- **C**: `sie_file_recover()` entry point in `recover.c`
- **Zig**: `recover.recover(allocator, path, mod) !RecoverResult` in `src/recover.zig`
- Full 3-pass recovery algorithm (magic scan, block glue, reassembly) is implemented.
  Results can be serialized to JSON via `RecoverResult.toJson()`.

### Output Deep Copy (`sie_output_deep_copy`)

- **C**: `sie_output_deep_copy()` — allocates a new output with copied data
- **Zig**: `Output.deepCopy(allocator) !Output` in `src/output.zig`

### File Backward Search

- **C**: `sie_file_search_backwards_for_magic()`, `sie_file_read_block_backward()`, `sie_file_find_block_backward()`
- **Zig**: `File.searchBackwardsForMagic()`, `File.readBlockBackward()`, `File.findBlockBackward()` in `src/file.zig`

### Unindexed Blocks

- **C**: `sie_file_get_unindexed_blocks()` — returns blocks not yet cataloged in the index
- **Zig**: `File.getUnindexedBlocks() !ArrayList(UnindexedBlock)` in `src/file.zig`

### Stream Block Access

- **C**: `sie_stream_read_group_block()`, `sie_stream_get_group_block_size()`
- **Zig**: `Stream.readGroupBlock()`, `Stream.getGroupBlockSize()` in `src/stream.zig`

### Transform Construction from XML

- **C**: `sie_transform_set_from_xform_node()`, `sie_transform_set_map_from_channel()`
- **Zig**: `Transform.setFromXformNode()`, `Transform.setMapFromOutputData()` in `src/transform.zig`

### Debug Formatting

- **C**: `sie_*_dump()` functions using `fprintf(debug_stream, ...)`
- **Zig**: `format()` method on Block, Channel, Dimension, Tag, and Output for use
  with `std.fmt.Formatter` (e.g. `std.debug.print("{}", .{block})`)

### XML Pack (Legacy)

- **C**: `sie_xml_pack()` — allocates all child nodes in a single contiguous memory block
  for reduced fragmentation and faster cleanup
- **Zig**: `Node.pack(allocator) !*Node` in `src/xml.zig` — delegates to `Node.clone()`
  since Zig's allocator handles memory layout. Semantically identical: produces an
  independent deep copy of the tree.

---

## Intentional Differences

These C features are intentionally **not** ported because Zig provides better
alternatives or because they are C-specific infrastructure.

### Replaced by Zig Idioms

| C Feature | Zig Equivalent |
|-----------|----------------|
| APR emulation layer (`sie_apr.c`) | `std.fs` for file I/O, `std.mem.Allocator` for memory |
| Windows `__stdcall` wrappers (`stdcall.c`) | Zig handles calling conventions automatically |
| Growable array (`sie_vec.c`) | `std.ArrayList` |
| `SIE_API_METHOD` / `SIE_VOID_API_METHOD` macros | Zig error unions |
| `SIE_TRY`/`SIE_CATCH`/`SIE_FINALLY` exception macros | `try`/`catch`/`errdefer` |
| `sie_class_parent()`, `sie_class_lookup_method()` — runtime type hierarchy | Comptime dispatch |
| `sie_strtod()` — portable locale-independent float parser | `std.fmt.parseFloat` (already locale-independent) |
| `sie_binary_search()` (deprecated) | `lowerBound()`/`upperBound()` on spigot |
| `sie_system_free()` (deprecated) | Allocator-based deallocation (`allocator.free()`) |

### Not Needed in Zig

| C Feature | Reason |
|-----------|--------|
| `TagSpigot` (tag-level data streaming) | Zig tags expose binary data directly as slices via `Tag.getBinary()` |
| Weak references in object system | Zig uses allocator-based ownership; no cycle prevention needed |
| `_sie_debug()` / `_sie_vdebug()` (context-level debug logging) | Use `std.log` scoped logging instead |
| NULL safety tests (`test_null_api`, `test_null_args`, etc.) | Zig's type system prevents null pointer issues at compile time |

---

## Architecture Differences

The Zig port maintains the same module structure as the C library but uses
idiomatic Zig patterns throughout:

- **Memory**: Zig allocators (typically `GeneralPurposeAllocator`) instead of APR pools
- **File I/O**: `std.fs` instead of `apr_file_*`
- **Error handling**: Error unions (`!T`) instead of `apr_status_t` return codes
- **Strings**: Slice-based (`[]const u8`) instead of null-terminated `char *`
- **Collections**: `std.ArrayList` / `std.AutoHashMap` instead of custom C containers
- **Polymorphism**: Vtable structs instead of C function pointers with void context
- **Debug printing**: `std.fmt.Formatter` `format()` methods instead of `fprintf`-based dump functions
