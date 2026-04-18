# libsie-zig Implementation Details

Zig port of [C libsie 1.1.6](https://github.com/efollman/libsie-reference). All 322 tests pass, 43/43 build steps succeed.

---

## Development History

Ported from C (with autotools + APR) to pure Zig in 6 phases, ~7,800 lines across 40+ files.

| Phase | Scope | Tests | Modules |
|-------|-------|-------|---------|
| 1 | Foundation types and utilities | 15 | types, config, byteswap, error, ref, utils, stringtable, uthash |
| 2 | Object system, binary I/O, context | 41 | context, object, block, file, stream, intake, channel, test, dimension, tag, group, vec, parser, spigot (partial) |
| 3 | XML DOM, parser, merge engine, output | 29 | xml, xml_merge, output, relation, iterator |
| 4 | Bytecode decoder VM, transforms, histogram | 20 | decoder, combiner, transform, histogram |
| 5 | Writer, compiler, sifter, recovery | 32 | writer, plot_crusher, compiler, sifter, spigot (complete), recover |
| 6 | Integration tests, sie_dump demo | 41 | decoder_test, file_test, api_test, functional_test, spigot_test |

---

## File Mapping: C → Zig

### Ported (35 of 39 C source files)

| C Source | Zig Source | Notes |
|----------|------------|-------|
| `block.c` | `block.zig` | |
| `channel.c` | `channel.zig` + `channel_spigot.zig` | |
| `combiner.c` | `combiner.zig` | |
| `compiler.c` | `compiler.zig` | |
| `context.c` | `context.zig` | |
| `decoder.c` | `decoder.zig` | |
| `dimension.c` | `dimension.zig` | |
| `file.c` | `file.zig` + `sie_file.zig` | Backward scan + index block parsing |
| `group.c` | `group.zig` + `group_spigot.zig` | |
| `histogram.c` | `histogram.zig` | |
| `id_map.c` | `uthash.zig` | Replaced with generic HashMap |
| `intake.c` | `intake.zig` | |
| `iterator.c` | `iterator.zig` | |
| `object.c` | `object.zig` | Tagged unions replace vtables |
| `output.c` | `output.zig` | |
| `parser.c` | `parser.zig` | |
| `plot_crusher.c` | `plot_crusher.zig` | |
| `recover.c` | `recover.zig` | |
| `ref.c` | `ref.zig` | |
| `relation.c` | `relation.zig` | |
| `sifter.c` | `sifter.zig` | |
| `spigot.c` | `spigot.zig` | |
| `stream.c` | `stream.zig` | |
| `stringtable.c` | `stringtable.zig` | |
| `tag.c` | `tag.zig` | |
| `test.c` | `test.zig` | |
| `transform.c` | `transform.zig` | |
| `utils.c` | `utils.zig` | |
| `vec.c` | `vec.zig` | Replaced with std.ArrayList |
| `writer.c` | `writer.zig` | |
| `xml.c` | `xml.zig` | |
| `xml_merge.c` | `xml_merge.zig` | |

### Not Ported (replaced by Zig idioms)

| C Source | Replacement |
|----------|-------------|
| `exception.c` | Zig error unions + `error.zig` |
| `debug.c` | `std.log` scoped logging |
| `stdcall.c` | Zig handles calling conventions automatically |
| `strtod.c` | `std.fmt.parseFloat` |
| `sie_apr.c` | `std.fs` / `std.mem.Allocator` |
| `sie_vec.c` | `std.ArrayList` |

### Zig-Only Files

| File | Purpose |
|------|---------|
| `config.zig` | Platform constants (endianness, OS detection) |
| `error.zig` | Zig-native error sets + Exception struct |
| `root.zig` | Module root, re-exports all public modules |
| `byteswap.zig` | Endianness conversion (replaces C macros) |
| `file_stream.zig` | Incremental SIE stream-to-file writer |

---

## Architecture Differences from C

| C Pattern | Zig Equivalent |
|-----------|----------------|
| APR memory pools | `std.mem.Allocator` (typically `GeneralPurposeAllocator`) |
| `apr_file_*` | `std.fs` |
| `apr_status_t` return codes | Error unions (`!T`) |
| Null-terminated `char *` | Slices (`[]const u8`) |
| Custom containers (`sie_vec`, `id_map`) | `std.ArrayList` / `std.AutoHashMap` |
| `SIE_TRY`/`SIE_CATCH`/`SIE_FINALLY` (`setjmp`/`longjmp`) | `try`/`catch`/`errdefer` |
| `SIE_API_METHOD` macros | Zig error unions natively |
| C function pointers + void context | Vtable structs |
| `fprintf`-based dump functions | `std.fmt.Formatter` `format()` methods |
| `sie_class_parent()` / runtime type hierarchy | Comptime dispatch |
| `sie_strtod()` | `std.fmt.parseFloat` |
| `TagSpigot` (one-shot text wrapper) | Direct slice access via `Tag.getBinary()` / `Tag.getString()` |
| Weak references | Allocator-based ownership (no cycle prevention needed) |
| NULL safety tests | Not applicable — Zig prevents null pointer issues at compile time |

### Error Handling

The C library uses `setjmp`/`longjmp` with `SIE_TRY`/`SIE_CATCH`/`SIE_FINALLY` macros and a handler stack. The Zig port replaces this with error unions, `errdefer`/`defer` for cleanup, and an error context stack in `context.zig` that preserves the C library's nested "while:" verbose error reports.

---

## Audit Issues — All Resolved

### Critical (fixed)

1. **Index block parsing** — `File.expandIndexBlock()` parses `SIE_INDEX_GROUP` (group 1) payloads to extract 12-byte entries (8-byte offset + 4-byte group).

2. **Backward index building** — `File.buildIndexBackward()` scans from EOF, expands index blocks, and jumps past indexed ranges. Falls back to forward scan on failure. Provides O(index_blocks + unindexed_blocks) performance.

3. **CAN raw file parsing** — Added `maybeExpandChDimPath()` to `xml_merge.zig` to wrap shorthand `<ch test="X">` into `<test id="X"><ch .../></test>`. Fixed `buildChannel()` to scan both base-expanded and channel-specific nodes for dimensions.

### Major (fixed)

4. **FileStream writing** — Implemented `FileStream` in `file_stream.zig`: incremental block reassembly from raw stream data, file writing, group index maintenance, Intake vtable, Writer roundtrip. 13 tests.

5. **`groupForEach`** — Added `groupForEach(callback, extra)` to `File`, `FileStream`, and `Stream`, matching C's `sie_file_group_foreach()`.

### Minor (fixed)

6. **XML utility functions** — Added `setAttributes`, `attributeEqual`, `nameEqual`, `find`, `print` to `Node` in `xml.zig`. 4 tests.

### No Action Needed

7. **TagSpigot** — C's `TagSpigot` is a trivial one-shot wrapper. Zig's `[]const u8` slices via `getString()`/`getBinary()` are equivalent. Large tags are group-backed and use `GroupSpigot` (already ported).

8. **Lazy XML/dimension expansion** — Eager approach only loads small XML metadata (~KB). Multi-GB binary data is read on-demand through spigots. No performance concern.

9. **`dump()` debug methods** — `std.fmt.Formatter` integration is idiomatic and sufficient.

---

## Test Coverage

322 tests across 21 test files cover all 17 C test files. Summary by C test file:

| C Test File | C Tests | Zig Tests | Status |
|-------------|---------|-----------|--------|
| t_decoder.c | 14 | 14 | Full coverage |
| t_exception.c | 9 | 11 | Covered (2 C-macro-specific N/A) |
| t_file.c | 12 | 19 | Full coverage |
| t_functional.c | 8 | 8 | Full coverage |
| t_histogram.c | 8 | 6 | Covered (2 null-safety excluded) |
| t_id_map.c | 3 | 5 | Full + bonus |
| t_object.c | 4 | 5 | Covered |
| t_output.c | 5 | 5 | Covered (1 null-safety excluded) |
| t_progress.c | 2 | 3 | Full coverage |
| t_regression.c | 1 | 4 | Full + bonus |
| t_relation.c | 2 | 9 | Full + bonus |
| t_sifter.c | 2 | 3 | Full + bonus |
| t_spigot.c | 7 | 18 | Full + bonus |
| t_stringtable.c | 3 | 5 | Full + bonus |
| t_xml.c | 7 | 14 | Full + bonus |
| t_xml_merge.c | 1 | 5 | Full + bonus |
| t_api.c | 1 | 9 | Different approach (C tests NULL guards) |

Excluded C tests are null-safety checks (`test_null_*`, `test_*_null_*`) not applicable to Zig's type system, and C macro infrastructure tests (`SIE_API_METHOD` / `SIE_VOID_API_METHOD`).

---

## Key Design Decisions

- C opaque pointers → Zig structs with proper types
- Reference counting preserved with `std.atomic`
- 51-opcode register-based bytecode VM with comparison flags and conditional branches
- 19-state incremental XML parser with entity resolution
- SIE binary blocks: 12-byte header (magic `0x51EDA7A0`), variable payload, 8-byte trailer with CRC-32

## Original C Code

Available at [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference) or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).
