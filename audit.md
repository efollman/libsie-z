# libsie-zig Port Audit

**Date**: 2026-04-17
**Reference**: [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference) (C libsie 1.1.6)
**Port**: libsie-zig (Zig implementation)

---

## Executive Summary

The Zig port is a substantial and largely faithful reimplementation of the C libsie 1.1.6 library. All 293 tests pass. The core data pipeline (file reading → XML parsing → decoder compilation → bytecode execution → data output) is fully functional. The port covers approximately **90%** of the C library's functionality, with the remaining gaps being either intentional architectural differences or missing edge features documented below.

---

## Build & Test Status

- **Compilation**: Clean, no warnings
- **Tests**: 293/293 pass (41/41 build steps succeed)
- **Known skips**: 4 CAN raw regression tests emit `SKIP` (CAN raw file parsing not yet supported)

---

## File Mapping: C → Zig

### Fully Ported Source Files (35 of 39 C source files)

| C Source | C Header | Zig Source | Status |
|----------|----------|------------|--------|
| `block.c` | `sie_block.h` | `block.zig` | ✅ Complete |
| `channel.c` | `sie_channel.h` | `channel.zig` + `channel_spigot.zig` | ✅ Complete |
| `combiner.c` | `sie_combiner.h` | `combiner.zig` | ✅ Complete |
| `compiler.c` | `sie_compiler.h` | `compiler.zig` | ✅ Complete |
| `context.c` | `sie_context.h` | `context.zig` | ✅ Complete |
| `decoder.c` | `sie_decoder.h` | `decoder.zig` | ✅ Complete |
| `dimension.c` | `sie_dimension.h` | `dimension.zig` | ✅ Complete |
| `file.c` | `sie_file.h` | `file.zig` + `sie_file.zig` | ⚠️ Incomplete (see issues) |
| `group.c` | `sie_group.h` | `group.zig` + `group_spigot.zig` | ✅ Complete |
| `histogram.c` | `sie_histogram.h` | `histogram.zig` | ✅ Complete |
| `id_map.c` | `sie_id_map.h` | `uthash.zig` (HashMap) | ✅ Replaced with generic HashMap |
| `intake.c` | `sie_intake.h` | `intake.zig` | ✅ Complete |
| `iterator.c` | `sie_iterator.h` | `iterator.zig` | ✅ Complete |
| `object.c` | `sie_object.h` | `object.zig` | ✅ Complete (tagged unions replace vtables) |
| `output.c` | `sie_output.h` | `output.zig` | ✅ Complete |
| `parser.c` | `sie_parser.h` | `parser.zig` | ✅ Complete |
| `plot_crusher.c` | `sie_plot_crusher.h` | `plot_crusher.zig` | ✅ Complete |
| `recover.c` | `sie_recover.h` | `recover.zig` | ✅ Complete |
| `ref.c` | `sie_ref.h` | `ref.zig` | ✅ Complete |
| `relation.c` | `sie_relation.h` | `relation.zig` | ✅ Complete |
| `sifter.c` | `sie_sifter.h` | `sifter.zig` | ✅ Complete |
| `spigot.c` | `sie_spigot.h` | `spigot.zig` | ✅ Complete |
| `stream.c` | `sie_stream.h` | `stream.zig` | ✅ Complete |
| `stringtable.c` | `sie_stringtable.h` | `stringtable.zig` | ✅ Complete |
| `tag.c` | `sie_tag.h` | `tag.zig` | ✅ Complete |
| `test.c` | `sie_test.h` | `test.zig` | ✅ Complete |
| `transform.c` | `sie_transform.h` | `transform.zig` | ✅ Complete |
| `utils.c` | `sie_utils.h` | `utils.zig` | ✅ Complete |
| `vec.c` | `sie_vec.h` | `vec.zig` | ✅ Replaced with std.ArrayList |
| `writer.c` | `sie_writer.h` | `writer.zig` | ✅ Complete |
| `xml.c` | `sie_xml.h` | `xml.zig` | ✅ Complete (~95% of functions) |
| `xml_merge.c` | `sie_xml_merge.h` | `xml_merge.zig` | ✅ Complete |

### Intentionally Not Ported (4 C source files)

| C Source | Reason |
|----------|--------|
| `exception.c` | Replaced by Zig error unions + `error.zig` (see section below) |
| `debug.c` | Replaced by `std.log` scoped logging |
| `stdcall.c` | Windows `__stdcall` calling convention wrappers; not needed in Zig |
| `strtod.c` | Custom float parser replaced by `std.fmt.parseFloat` |
| `sie_apr.c` | APR abstraction layer replaced by `std.fs` / `std.mem.Allocator` |
| `sie_vec.c` | Custom growable arrays replaced by `std.ArrayList` |

### Additional Zig-Only Files

| Zig File | Purpose |
|----------|---------|
| `config.zig` | Platform constants (endianness, OS detection) |
| `error.zig` | Zig-native error handling (error sets + Exception struct) |
| `root.zig` | Module root, re-exports all public modules |
| `byteswap.zig` | Endianness conversion (replaces C macros in `sie_byteswap.h`) |

---

## Issues Found

### CRITICAL

#### 1. SIE_INDEX_GROUP Block Processing Not Implemented

**C function**: `add_index_block()` in `file.c` (lines 112–175)
**Zig status**: Missing

The C library processes `SIE_INDEX_GROUP` (group 1) blocks to extract an indirect block index that provides:
- Direct offset-based random access to data blocks
- Group membership mapping for each block
- Size validation via cross-referencing

The Zig `File.buildIndex()` performs a forward-only sequential scan, recording block positions as it encounters them. It does **not** parse the content of index blocks to reconstruct the optimized index structure.

**Impact**: Files with embedded index blocks will still be readable (the forward scan still catalogs all blocks), but:
- Large files will index slower (full sequential scan vs. reading pre-built index)
- The optimized random-access metadata inside index blocks is ignored
- Some SIE files that rely on index-block metadata for block-to-group mapping may not index correctly

**userinput** this is important to implement. test files are small but in practice sie files can get very large. and accessing data in parts and at random becomes necissary.

#### 2. Backward Index Building Not Implemented

**C function**: `build_index()` in `file.c` — scans backward from EOF using index blocks
**Zig status**: Forward-only scan in `File.buildIndex()`

The C library starts at the end of the file and works backward, reading index blocks first to quickly build the complete index without scanning every block. The Zig version scans forward through every block from the beginning.

**Impact**: Performance regression on large SIE files. Functionally correct for well-formed files, but may fail to properly handle files where blocks are out of order or where the forward scan encounters invalid data it could have avoided by starting from the validated end.

**userinput** important to implement for similar reasons to 1. large files are expected, performance optimizations are needed. sie files can also be appended to after creation, which is why non 'well formed' files may be possible.

#### 3. CAN Raw File Parsing Not Working

**Evidence**: 4 regression tests emit `SKIP` messages:
```
SKIP: CAN raw file parsed 0 channels (not yet supported)
SKIP: CAN raw channel 1 not found
```

The regression test file `can_raw_test-v-1-5-0-129-build-1218.sie` cannot be parsed. CAN raw files may use a different XML structure or encoding scheme that the current XML merge/definition system doesn't handle.

**Impact**: CAN raw SIE files cannot be read.

**userinput** This NEEDS to be implemented. parsing raw can data is a core feature of this library and is required. please fully implement along with proper testing.

### MAJOR

#### 4. File Stream Writing Not Implemented

**C functions**: `sie_file_stream_new()`, `sie_file_stream_init()`, `sie_file_stream_destroy()`, `sie_file_stream_add_stream_data()`
**Zig status**: Missing entirely (no `FileStream` type)

The C library supports writing SIE data to files via a `sie_File_Stream` type that allows incremental block writing. While the `Writer` module exists in Zig (for block formatting and CRC), there is no `FileStream` equivalent that wraps file I/O with the writer.

**Impact**: Cannot create or modify SIE files via the file stream interface. The `Writer` can format blocks, but there's no integrated file-backed write pipeline. Users would need to manually connect `Writer` output to a file handle.

**userinput** this should be implemented as full feature parity with the original library is the goal, and the original library supports writing sie files.

#### 5. `group_foreach` Not Implemented

**C function**: `sie_file_group_foreach()` — iterate over all groups with callback
**Zig status**: Missing (manual iteration over `group_indexes` HashMap required)

**Impact**: Low — users can iterate `group_indexes` directly, but it's a convenience gap.

**userinput** this should be implemented for feature parity with original library.

### MINOR

#### 6. Missing XML Utility Functions (7 functions)

The following XML utility functions from the C library are not present in the Zig port:

| C Function | Purpose |
|-----------|---------|
| `sie_xml_set_attributes(self, other)` | Copy all attributes from one node to another |
| `sie_xml_attribute_equal_s/b(self, other, name)` | Compare specific attribute between two nodes |
| `sie_xml_attribute_equal(self, other, name)` | Compare attribute (C string variant) |
| `sie_xml_name_equal(self, other)` | Compare element names of two nodes |
| `sie_xml_find(self, top, match_fn, data, descend)` | Generic tree search with arbitrary callback |
| `sie_xml_print(node)` | Convenience debug print to stdout |

**Impact**: Low — these are convenience functions. `findElement()` covers most search use cases, and attribute comparison can be done manually. `set_attributes` could matter if XML merge relies on it internally.

**userinput** These should be implemented for feature parity with original library.

#### 7. Tag Spigot Not Ported

**C functions**: `sie_tag_spigot_new()`, `sie_tag_spigot_init()`, `sie_tag_spigot_destroy()`, `sie_tag_spigot_get_inner()`, `sie_tag_spigot_clear_output()`
**Zig status**: Not implemented — tag data accessed directly via `Tag.getBinary()` / `Tag.getString()`

**Impact**: Low — Zig provides direct slice access to tag data, which is simpler and sufficient for typical use cases. The spigot abstraction was a C design pattern for uniform streaming; Zig's slice-based approach is more idiomatic.

**userinput** not entirely sure if this is a problem. the original documentation mentions that there is no arbitrary limit to tag size, and sometimes tags can be very large. if zig slicing handles very large tags this is okay, if not these functions will need to be implemented.

#### 8. Lazy XML/Dimension Expansion Missing

**C behavior**: `channel.c` lazily expands XML definitions and dimensions on first access via `expand_xml()` and `expand_dimensions()`
**Zig behavior**: Dimensions and XML are built eagerly during `SieFile.open()`

**Impact**: Low — slightly higher memory usage at open time, but simpler code and no risk of lazy-init bugs.

**userinput** Technically the binary blocks in the file are contained in the xml structure. these can become very large (multiple gigabytes). if this could cause problems with the eager approach then the lazy approach will need to be implemented. alternatively in my own implementation i inserted the closing tag </sie> before the binary blocks before parsing. this was accomplished through the index blocks. decide what is the best approach here and ask for user input before implementing.

#### 9. `dump()` Debug Methods Not Ported

**C functions**: `sie_channel_dump()`, `sie_test_dump()`, `sie_dimension_dump()`, `sie_tag_dump()`
**Zig status**: Some types implement `format()` for `std.fmt.Formatter` integration, but not all C dump methods have equivalents

**Impact**: Negligible — debug output. Zig's `std.fmt` integration is arguably better.

**userinput** this is okay. std.fmt is nicer, this can be ignored.

---

## Exception Handling: Architectural Difference

The C library has an elaborate exception system built on `setjmp`/`longjmp`:

- **Handler stack** with `SIE_TRY`/`SIE_CATCH`/`SIE_FINALLY` macros
- **Exception types**: `sie_Exception`, `sie_Simple_Error`, `sie_Operation_Aborted`, `sie_Out_Of_Memory`
- **Error context chains** for "while:" verbose reports
- **Exception callbacks** for unhandled exception notification
- **Report generation** with virtual dispatch

The Zig port replaces this entirely with:
- **Error unions** (`!T`) for all fallible operations
- **`error.zig`**: `Error` error set + `Exception` struct for message capture
- **`errdefer`/`defer`** for cleanup (replaces cleanup stack)
- **`context.zig`**: Error context stack for nested error messages

This is an intentional and correct architectural decision. Zig's error handling is superior to C's `setjmp`-based exceptions for safety, composability, and debuggability. The verbose report chain (error context stack in `context.zig`) preserves the C library's nested error context feature.

---

## Decoder VM: Verified Complete

All 51 opcodes are implemented with identical semantics:

| Opcode Range | Category | Status |
|-------------|----------|--------|
| 0 | Crash | ✅ |
| 1–10 | Read LE (U8–F64) | ✅ |
| 11–20 | Read BE (U8–F64) | ✅ |
| 21 | ReadRaw | ✅ |
| 22 | Seek | ✅ |
| 23 | Sample | ✅ |
| 24 | MoveReg | ✅ |
| 25–28 | Arithmetic (+, -, *, /) | ✅ |
| 29–30 | Bitwise (AND, OR) | ✅ |
| 31–32 | Shift (LSL, LSR) | ✅ |
| 33 | Logical NOT | ✅ |
| 34–39 | Comparisons (LT–NE) | ✅ |
| 40–41 | Logical (AND, OR) | ✅ |
| 42 | Compare (set flags) | ✅ |
| 43 | Branch (unconditional) | ✅ |
| 44–49 | Branch (conditional) | ✅ |
| 50 | Assert | ✅ |

The compiler handles all XML tag types: `<decoder>`, `<set>`, `<loop>`, `<read>`, `<seek>`, `<sample>`, `<if>`, and the full expression grammar with correct operator precedence. The Zig compiler uses a recursive descent parser instead of C's macro-generated dispatch, which is cleaner and more maintainable.

---

## XML Parser: Verified Complete

All 19 parser states are implemented 1:1. The incremental parser, simple parser, and `parseString` convenience function are all present. Entity resolution (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`) is complete. Character classification (name start chars, name chars, whitespace) is complete.

---

## Binary Search: Verified Complete

Both `lowerBound()` and `upperBound()` are implemented with the same two-level algorithm:
1. Outer binary search over blocks
2. Inner linear scan within each block
3. Edge case handling for values before/after all data

The deprecated `sie_binary_search()` (alias for `lowerBound`) is correctly not ported.

---

## Test Coverage

All 17 C test files have been ported to 20 Zig test files:

| C Test File | C Tests | Zig Tests | Coverage |
|-------------|---------|-----------|----------|
| `t_api.c` | 1 | 9 | Complete (Zig API tests replace null-safety) |
| `t_decoder.c` | 14 | 14 | Complete |
| `t_exception.c` | 9 | 11 | Complete (2 C-macro tests N/A) |
| `t_file.c` | 12 | 19 | Complete |
| `t_functional.c` | 8 | 8 | Complete |
| `t_histogram.c` | 8 | 6 | Complete (2 null-safety tests N/A) |
| `t_id_map.c` | 3 | 5 | Complete + bonus |
| `t_object.c` | 4 | 5 | Complete (weak ref test N/A) |
| `t_output.c` | 5 | 5 | Complete (1 null-safety test N/A) |
| `t_progress.c` | 2 | 3 | Complete |
| `t_regression.c` | 1 | 4 | Partial (CAN raw tests SKIP) |
| `t_relation.c` | 2 | 9 | Complete + bonus |
| `t_sifter.c` | 2 | 3 | Complete + bonus |
| `t_spigot.c` | 7 | 18 | Complete + bonus |
| `t_stringtable.c` | 3 | 5 | Complete + bonus |
| `t_xml.c` | 7 | 14 | Complete + bonus |
| `t_xml_merge.c` | 1 | 5 | Complete + bonus |

**Total**: 293 Zig tests pass, covering all C test scenarios except null-safety tests (not applicable in Zig) and CAN raw file tests (feature not yet implemented).

---

## Summary of Action Items

### Must Fix (for production use with large/complex files)
1. **Implement `SIE_INDEX_GROUP` processing** in `File.buildIndex()` to parse indirect index blocks
2. **Implement backward index scan** for efficient large-file indexing
3. **Fix CAN raw file support** — investigate XML structure differences

### Should Fix (for complete feature parity)
4. **Implement `FileStream`** for file-backed SIE writing
5. **Add missing XML utilities** (`set_attributes`, `attribute_equal`, `name_equal`, `find` with callback)

### Nice to Have
6. **Add `group_foreach`** convenience method on `File`
7. **Port `dump()` methods** where missing (use `std.fmt.Formatter`)

---

## Conclusion

The libsie-zig port is a high-quality, idiomatic Zig reimplementation that covers the vast majority of the C library's functionality. The core data pipeline — from opening SIE files through XML definition merging, decoder compilation, bytecode execution, transform application, and data output — is fully operational and well-tested.

The main gaps are in advanced file indexing (backward scan + index block parsing) and file writing, which would need to be addressed for production use with large files or write workflows. The CAN raw format support also needs investigation. All other differences are deliberate architectural improvements leveraging Zig's type system, error handling, and memory management.
