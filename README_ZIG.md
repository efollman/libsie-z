# LibSIE - Zig Port

A complete rewrite of libsie (SIE file reader library) in pure Zig, with zero external dependencies.

**178 tests passing** | Zig 0.15.x | Zero dependencies

## Building

```bash
# Run all tests
zig build test --summary all

# Build sie_dump demo
zig build example
./zig-out/bin/sie_dump <file.sie>
```

## Project Structure

```
src/                    Zig library source (36 modules)
test/                   Integration tests (5 files, 41 tests)
  data/                 Test SIE files and decoder fixtures
examples/
  sie_dump.zig          SIE file dumper demo
libsie-c/               Original C source (reference only)
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
| `file` | File I/O with group indexing and Intake vtable |
| `stream` | Stream intake with incremental block parsing |
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
| `output` | Output buffers (float64/raw, resize/grow/trim/clear) |
| `relation` | Key-value string store (split/clone/merge) |
| `iterator` | Slice, HashMap, XML child iterators with filtering |
| `decoder` | Bytecode VM (51 opcodes, registers, disassemble, CRC-32) |
| `combiner` | Dimension remapping (input→output layout) |
| `transform` | None/Linear/Map transforms (in-place on Output) |
| `histogram` | Multi-dimensional bin bounds, flat/unflat indexing |
| `writer` | SIE block writer (XML/index buffering, CRC-32, auto-flush) |
| `plot_crusher` | Data reduction via min/max pair tracking |
| `compiler` | XML-to-bytecode compiler (expressions, registers, labels) |
| `sifter` | Subset extraction with ID remapping, XML rewriting |
| `spigot` | Data pipeline (vtable dispatch, binary search, scan limits) |
| `recover` | Magic byte scanning, block glue, JSON results |

## Architecture

The port maintains the original libsie module structure using Zig idioms:

- **Memory**: Zig allocators instead of APR pools
- **File I/O**: `std.fs` instead of `apr_file_*`
- **Error handling**: Error unions instead of `apr_status_t`
- **Strings**: Slice-based instead of null-terminated
- **Collections**: `std.ArrayList`/`AutoHashMap` instead of custom types
- **Polymorphism**: Vtable structs instead of C function pointers

## Tests

178 total: 137 unit tests (inline in `src/`) + 41 integration tests (`test/`).

| Test File | Count | Coverage |
|-----------|-------|----------|
| Unit tests (src/*.zig) | 137 | All modules |
| decoder_test.zig | 14 | Bytecode VM execution |
| file_test.zig | 9 | SIE file I/O, block reading |
| api_test.zig | 9 | Block, Dimension, Channel, Context |
| functional_test.zig | 5 | End-to-end file parsing |
| spigot_test.zig | 4 | Position, seek, scan limits |

## Requirements

- Zig 0.15.x (tested on 0.15.2, Windows x86_64)

## License

LGPL 2.1 (same as original libsie)

## History

See [HISTORY.md](HISTORY.md) for development phases and porting notes.
