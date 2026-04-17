# LibSIE Zig Port - Development History

Ported from C (with autotools + APR) to pure Zig in 6 phases.

## Phase 1: Foundation (15 unit tests)
Type system, platform configuration, byte swapping, error handling (22 variants),
atomic reference counting, string utilities, string interning, hash tables.

**Modules:** types, config, byteswap, error, ref, utils, stringtable, uthash

## Phase 2: Object System & I/O (41 unit tests)
Context management (cleanup stack, error contexts, progress, recursion limit).
SIE binary block format (magic 0x51EDA7A0, 12-byte header, 8-byte trailer, CRC-32).
File I/O with group index building. Stream intake with incremental block parsing.
Abstract Intake vtable (File/Stream backends). Test/Channel/Dimension hierarchy
with tags and XML. Object system with tagged unions and ref counting.

**Modules:** context, object, block, file, stream, intake, channel, test, dimension,
tag, group, vec, parser, spigot (partial)

## Phase 3: XML & Support Modules (29 unit tests)
XML DOM tree with 4 node types, attributes, tree manipulation, walking.
19-state incremental XML parser with entity resolution. XML serialization.
XML merge engine (definition builder, merge, base expansion, ID maps).
Output buffers (float64/raw). Relation key-value store. Iterators (slice,
hashmap, XML child with filtering).

**Modules:** xml, xml_merge, output, relation, iterator

## Phase 4: Data Pipeline (20 unit tests)
51-opcode register-based bytecode decoder VM with comparison flags, conditional
branches, disassembly, CRC-32 signatures. Dimension remapping combiner.
None/Linear/Map data transforms. Multi-dimensional histogram with bin bounds,
flat/unflat indexing, binary bound search.

**Modules:** decoder, combiner, transform, histogram

## Phase 5: Higher-Level Features (32 unit tests)
SIE block writer with XML/index buffering, CRC-32, 64KB auto-flush.
Plot data reduction via min/max pair tracking.
XML-to-bytecode compiler (full expression parser, register resolution, label fixup).
Subset extraction sifter with ID remapping and XML rewriting.
Spigot vtable dispatch with binary search and scan limits.
Recovery via magic byte scanning and block glue reconstruction.

**Modules:** writer, plot_crusher, compiler, sifter, spigot (complete), recover

## Phase 6: Integration Testing & Demo (41 integration tests)
Ported 5 integration test files from C test suite (t_decoder.c, t_file.c, t_api.c,
t_functional.c, t_spigot.c). Built sie_dump demo application for inspecting SIE
files. Set up build.zig with `test` and `example` build steps.

**Test files:** decoder_test, file_test, api_test, functional_test, spigot_test
**Demo:** examples/sie_dump.zig

## Lines of Code

| Phase | LOC | Scope |
|-------|-----|-------|
| 1 | ~600 | Foundation types and utilities |
| 2 | ~2,200 | Object system, binary I/O |
| 3-4 | ~2,000 | XML, data pipeline |
| 5 | ~1,800 | Higher-level features |
| 6 | ~1,200 | Integration tests + demo |
| **Total** | **~7,800** | **40+ files** |

## Key Design Decisions

- C opaque pointers → Zig structs with proper types
- `apr_status_t` → Zig error unions
- APR memory pools → Zig allocators
- Null-terminated C strings → Zig slices
- `apr_file_*` → `std.fs`
- Custom containers → `std.ArrayList` / `std.AutoHashMap`
- Reference counting preserved with `std.atomic`

## Original C Code

The original C source (headers, autotools, contrib, docs, tarballs) is preserved
in `libsie-c/` for reference.
