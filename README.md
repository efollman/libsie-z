# LibSIE - Zig Port

[![CI](https://github.com/efollman/libsie-zig/actions/workflows/ci.yml/badge.svg)](https://github.com/efollman/libsie-zig/actions/workflows/ci.yml)

A complete rewrite of [libsie](https://github.com/efollman/libsie-reference) (SIE file reader library) in pure Zig, with zero external dependencies. The original C library is also available from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).

**Author:** Evan Follman | **322 tests passing** | Zig 0.15.x | Zero dependencies

## Building

```bash
# Run all tests
zig build test --summary all

# Build examples
zig build example
./zig-out/bin/sie_dump <file.sie>
./zig-out/bin/sie_export <input.sie> <output.txt>
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

Output is placed in `zig-out/lib/` (or `zig-out/bin/` for the DLL on
Windows). The shared library exports the full C ABI declared in
[`include/sie.h`](include/sie.h) — see [C ABI / FFI](#c-abi--ffi) below.

### Changes for jll packaging

#### Cross-compiling with GNU triples

The build script accepts `-Dtriple=<gnu-triple>` as a friendlier alternative
to Zig's `-Dtarget=` for cross-compilation. The following 18 triples are
recognized (the same set BinaryBuilder.jl uses):

```
i686-linux-gnu          x86_64-linux-gnu        aarch64-linux-gnu
armv6l-linux-gnueabihf  armv7l-linux-gnueabihf  powerpc64le-linux-gnu
riscv64-linux-gnu       i686-linux-musl         x86_64-linux-musl
aarch64-linux-musl      armv6l-linux-musleabihf armv7l-linux-musleabihf
x86_64-apple-darwin     aarch64-apple-darwin
x86_64-unknown-freebsd  aarch64-unknown-freebsd
i686-w64-mingw32        x86_64-w64-mingw32
```

```bash
zig build lib -Dtriple=x86_64-linux-gnu
zig build lib -Dtriple=x86_64-w64-mingw32
```

#### C ABI / FFI

`src/c_api.zig` declares ~60 `export fn`s wrapping the public Zig API for
use from C, Julia (`ccall`), Python (`ctypes`), Rust (`bindgen`), and any
other FFI consumer. The C header lives at [`include/sie.h`](include/sie.h)
and is installed by the JLL packaging step (see below).

Coverage:

- `SieFile` — open/close, channel/test/tag enumeration and lookup.
- `Test` / `Channel` / `Dimension` / `Tag` accessors. Strings are returned
  as `(ptr, len)` pairs because tag values are binary-safe and may contain
  embedded NULs.
- `Spigot` — attach/free/get, plus `tell`/`seek`/`reset`/`is_done`,
  `disable_transforms`, `transform_output`, `set_scan_limit`,
  `lower_bound`, `upper_bound`.
- `Output` — dimension type query, `get_float64`, `get_raw`.
- `Stream` — incremental block ingest with group queries.
- `Histogram` — build from a channel, query bins and bounds.
- `sie_version`, `sie_status_message` for library info / error decoding.

All fallible functions return `int` (0 = `SIE_OK`, non-zero = status code
from `src/error.zig`'s `errorToStatus`). All handles are opaque pointers;
ownership conventions are documented per function in `sie.h`.

#### BinaryBuilder.jl / JLL packaging

`zig build jll` produces the exact directory layout BinaryBuilder.jl
expects under `${prefix}`:

```
${prefix}/lib/libsie.so       (or bin/sie.dll on Windows)
${prefix}/include/sie.h
${prefix}/share/licenses/libsie-z/LICENSE
```

It defaults to `ReleaseSafe` (overridable with `-Doptimize=`). Combined
with `-Dtriple=`, it slots directly into a BB recipe. A ready-to-run
recipe and full instructions live in [`jll-build/`](jll-build/) — see
[`jll-build/jll-info.md`](jll-build/jll-info.md).

```bash
zig build jll -Dtriple=x86_64-linux-gnu --prefix zig-out/prefix
```

## Project Structure

```
src/                    Zig library source (42 modules, 483 public functions)
  c_api.zig             C ABI exports for FFI (Julia, Python, Rust, C, ...)
include/
  sie.h                 Hand-written C header matching c_api.zig
test/                   Integration tests (21 files, 172 tests)
  data/                 Test SIE files and decoder fixtures
examples/
  sie_dump.zig          SIE file dumper demo (verbose tutorial)
  sie_export.zig        SIE-to-ASCII exporter (metadata + channel data)
jll-build/              BinaryBuilder.jl recipe for libsie_jll
  build_tarballs.jl     Multi-platform JLL build recipe
  Project.toml          Pinned BinaryBuilder env
  jll-info.md           Library changes for JLL + build/deploy instructions
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
| `c_api` | C ABI exports (~60 `export fn`s) wrapping the public Zig surface |

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
| [examples/sie_export.zig](examples/sie_export.zig) | SIE-to-ASCII file exporter |

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
