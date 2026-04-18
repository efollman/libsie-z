# LibSIE - Zig Port

A complete rewrite of [libsie](https://github.com/efollman/libsie-reference) (SIE file reader library) in pure Zig, with zero external dependencies. The original C library is also available from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).

**Author:** Evan Follman | **322 tests passing** | Zig 0.15.x | Zero dependencies

## AI-Assisted Development

This port was largely written with the assistance of AI (Claude Opus 4.6). While
the resulting code passes a comprehensive test suite (322 tests covering all
modules), users should be aware of potential issues inherent to AI-assisted code
generation:

- **Subtle logic errors** — AI-generated code may contain edge-case bugs that
  are not covered by the existing test suite, particularly in rarely-exercised
  code paths.
- **Semantic drift** — The Zig implementation may deviate from the original C
  library's behavior in ways that are not immediately obvious, especially
  around undefined behavior, numeric overflow, or platform-specific details.
- **Incomplete understanding** — The AI may have misinterpreted the intent of
  the original C code in cases where the logic was complex or poorly
  documented, leading to functionally different behavior.
- **Documentation accuracy** — Ported documentation and code comments may
  contain inaccuracies introduced during the translation process.

All AI-generated code was reviewed and tested, but additional scrutiny is
recommended for production use. Bug reports and contributions are welcome.

## Building

```bash
# Run all tests
zig build test --summary all

# Build sie_dump demo
zig build example
./zig-out/bin/sie_dump <file.sie>
```

### Using as a library

Add libsie-zig as a dependency in your `build.zig`:

```zig
const libsie_dep = b.dependency("libsie", .{
    .target = target,
    .optimize = optimize,
});
exe.root_module.addImport("libsie", libsie_dep.module("libsie"));
```

Then import in your Zig code:

```zig
const libsie = @import("libsie");
const SieFile = libsie.SieFile;
const Channel = libsie.Channel;
```

### Building as a shared library

```bash
# Build libsie.so (Linux) / libsie.dylib (macOS) / sie.dll (Windows)
zig build lib

# With release optimizations
zig build lib -Doptimize=ReleaseFast
```

Output is placed in `zig-out/lib/`.

## Project Structure

```
src/                    Zig library source (42 modules, 483 public functions)
test/                   Integration tests (21 files, 172 tests)
  data/                 Test SIE files and decoder fixtures
examples/
  sie_dump.zig          SIE file dumper demo (verbose tutorial)
docs/
  SIE_FORMAT.md         The SIE file format specification
  CORE_SCHEMA.md        The core metadata schema
  SOMAT_SCHEMA.md       The somat data schema
  API_REFERENCE.md      Zig API reference (ported from sie.h)
  apilist.md            Complete public API function listing
  zig-details.md        Implementation details, history, test coverage, audit
build.zig               Build configuration
```

## Modules

| Module | Description |
|--------|-------------|
| `root` | Library entry point and exports |
| `types` | Type definitions (int/float aliases) |
| `config` | Platform configuration (endianness, OS) |
| `byteswap` | Byte ordering (ntoh/hton 16/32/64) |
| `error` | Error types (22 variants), exception handling |
| `ref` | Atomic reference counting |
| `utils` | String utilities (strToDouble, trim, indexOf) |
| `stringtable` | String interning/deduplication |
| `uthash` | Generic hash table wrapper |
| `context` | Library context (cleanup stack, error contexts, progress) |
| `object` | Base object with tagged union dispatch |
| `block` | SIE binary blocks (CRC-32, parse, serialize) |
| `file` | File I/O with group indexing, backward search, Intake vtable |
| `stream` | Stream intake with incremental block parsing, block random access |
| `intake` | Abstract data source interface (vtable) |
| `channel` | Data series with dimensions and tags |
| `test` | Test container with channels |
| `dimension` | Axis metadata (decoder, transforms) |
| `tag` | Key-value metadata (string/binary) |
| `group` | Block group tracking |
| `vec` | ArrayList alias |
| `parser` | Parsing utilities (tag names, quoted strings, numbers) |
| `xml` | XML DOM tree, incremental parser, serialization |
| `xml_merge` | XML definition builder, merge engine, base expansion |
| `output` | Output buffers (float64/raw, resize/grow/trim/clear/deepCopy) |
| `relation` | Key-value string store (split/clone/merge) |
| `iterator` | Slice, HashMap, XML child iterators with filtering |
| `decoder` | Bytecode VM (51 opcodes, registers, disassemble, CRC-32) |
| `combiner` | Dimension remapping (input→output layout) |
| `transform` | None/Linear/Map transforms, XML-driven construction |
| `histogram` | Multi-dimensional bin bounds, flat/unflat indexing |
| `writer` | SIE block writer (XML/index buffering, CRC-32, auto-flush) |
| `plot_crusher` | Data reduction via min/max pair tracking |
| `compiler` | XML-to-bytecode compiler (expressions, registers, labels) |
| `sifter` | Subset extraction with ID remapping, XML rewriting |
| `spigot` | Data pipeline (vtable dispatch, binary search, scan limits) |
| `recover` | File recovery: magic scan, block glue, 3-pass algorithm, JSON |
| `file_stream` | Incremental SIE stream-to-file writer with group tracking |

## Architecture

The port maintains the original libsie module structure using Zig idioms:

- **Memory**: Zig allocators instead of APR pools
- **File I/O**: `std.fs` instead of `apr_file_*`
- **Error handling**: Error unions instead of `apr_status_t`
- **Strings**: Slice-based instead of null-terminated
- **Collections**: `std.ArrayList`/`AutoHashMap` instead of custom types
- **Polymorphism**: Vtable structs instead of C function pointers
- **Debug printing**: `std.fmt.Formatter` `format()` methods on key types

## Documentation

| Document | Description |
|----------|-------------|
| [SIE_FORMAT.md](docs/SIE_FORMAT.md) | The SIE file format — block structure, XML metadata, decoder language, data rendering algorithm |
| [CORE_SCHEMA.md](docs/CORE_SCHEMA.md) | The `core` metadata schema — standard tags (`core:schema`, `core:units`, `core:sample_rate`, etc.) |
| [SOMAT_SCHEMA.md](docs/SOMAT_SCHEMA.md) | The `somat` data schema — sequential, burst, histogram, rainflow, message data layouts |
| [API_REFERENCE.md](docs/API_REFERENCE.md) | Zig API reference — C-to-Zig migration guide with side-by-side examples |
| [zig-details.md](docs/zig-details.md) | Implementation details, history, C→Zig differences, test coverage, audit |
| [apilist.md](docs/apilist.md) | Complete public API function listing |
| [examples/sie_dump.zig](examples/sie_dump.zig) | Verbose tutorial demo (Zig port of libsie-demo.c) |

Format/schema docs are ported from the original C library LaTeX sources ([available on GitHub](https://github.com/efollman/libsie-reference) or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/)). Original content is preserved verbatim where applicable; Zig implementation differences are called out in clearly marked blocks.

## Tests

322 total: 150 unit tests (inline in `src/`) + 172 integration tests (`test/`).

| Test File | Count | Coverage |
|-----------|-------|----------|
| Unit tests (src/*.zig) | 150 | All modules |
| spigot_data_test.zig | 19 | Data pipeline with real files |
| xml_test.zig | 18 | XML parsing, serialization, entities |
| decoder_test.zig | 14 | Bytecode VM execution |
| file_stream_test.zig | 14 | FileStream writing and roundtrip |
| file_test.zig | 13 | SIE file I/O, block reading, index blocks |
| context_test.zig | 11 | Context lifecycle, cleanup, progress |
| file_highlevel_test.zig | 10 | High-level SieFile API |
| api_test.zig | 9 | Block, Dimension, Channel, Context |
| relation_test.zig | 9 | Key-value store operations |
| functional_dump_test.zig | 8 | End-to-end file dump |
| histogram_test.zig | 6 | Multi-dim bin indexing |
| functional_test.zig | 5 | End-to-end file parsing |
| id_map_test.zig | 5 | ID mapping |
| object_test.zig | 5 | Object system |
| output_test.zig | 5 | Output data management |
| stringtable_test.zig | 5 | String interning |
| xml_merge_test.zig | 5 | XML definition merging |
| regression_test.zig | 4 | Edge cases and regressions |
| spigot_test.zig | 4 | Position, seek, scan limits |
| sifter_test.zig | 3 | Subset extraction |

## Public API

See [apilist.md](docs/apilist.md) for the complete public API reference.

## Requirements

- Zig 0.15.x (tested on 0.15.2)

## License

LGPL 2.1 (same as [original libsie](https://github.com/efollman/libsie-reference), also available from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/))

Copyright (C) 2025-2026 Evan Follman

Original C library Copyright (C) 2005-2015 HBM Inc., SoMat Products

## History

See [zig-details.md](docs/zig-details.md) for development history, C→Zig differences, test coverage report, and port audit.
