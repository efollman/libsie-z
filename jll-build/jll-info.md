# libsie_jll build notes

This directory contains the [BinaryBuilder.jl](https://docs.binarybuilder.org/)
recipe used to package `libsie-z` as a Julia JLL (`libsie_jll`).

## What changed in the library to make this possible

The upstream Zig library was extended (no behavioral changes to existing Zig
APIs) with the pieces a JLL needs:

### 1. C ABI surface — [src/c_api.zig](../src/c_api.zig)

A new module of `export fn` declarations that gives the produced shared
library actual exported symbols. Roughly 60 functions covering:

- **Library info**: `sie_version`, `sie_status_message`
- **SieFile**: `sie_file_open` / `_close`, channel/test/tag enumeration and lookup,
  `sie_file_containing_test`
- **Test**: `sie_test_id`, `sie_test_name`, `sie_test_channel`, `sie_test_tag`,
  `sie_test_find_tag`, …
- **Channel**: `sie_channel_id`, `sie_channel_name`, `sie_channel_dimension`,
  `sie_channel_tag`, `sie_channel_find_tag`
- **Dimension**: `sie_dimension_index`, `sie_dimension_name`, `sie_dimension_tag`, …
- **Tag** (binary-safe): `sie_tag_key`, `sie_tag_value`, `sie_tag_value_size`,
  `sie_tag_is_string`, `sie_tag_is_binary`, `sie_tag_group`, `sie_tag_is_from_group`
- **Spigot**: `sie_spigot_attach` / `_free` / `_get`, plus `_tell`, `_seek`,
  `_reset`, `_is_done`, `_num_blocks`, `_disable_transforms`,
  `_transform_output`, `_set_scan_limit`, `_lower_bound`, `_upper_bound`
- **Output**: `sie_output_num_dims`, `_num_rows`, `_block`, `_type`,
  `_get_float64`, `_get_raw`
- **Stream** (incremental ingest): `sie_stream_new` / `_free` / `_add_data`,
  group queries
- **Histogram**: `sie_histogram_from_channel` / `_free`, `_num_dims`,
  `_total_size`, `_num_bins`, `_get_bin`, `_get_bounds`

Conventions:

- All handles are opaque pointers. Owned vs borrowed lifetimes are documented
  per function.
- Fallible functions return `int` (`0` = `SIE_OK`); status codes mirror
  `src/error.zig`'s `errorToStatus` mapping.
- **Strings are returned as `(out_ptr, out_len)` pairs**, not as
  NUL-terminated C strings. SIE tag values are binary-safe and may contain
  embedded NULs, and channel/test/dimension names parsed from XML are not
  guaranteed to be NUL-terminated in memory.

### 2. C header — [include/sie.h](../include/sie.h)

Hand-written header with `extern "C"` guard, opaque typedefs
(`sie_File`, `sie_Channel`, …), `SIE_E_*` status constants, output-type
constants (`SIE_OUTPUT_NONE`, `SIE_OUTPUT_FLOAT64`, `SIE_OUTPUT_RAW`), and
prototypes for every export.

### 3. Build system — [build.zig](../build.zig)

- New `-Dtriple=` option mapping 18 BinaryBuilder GNU triples to Zig target
  queries (e.g. `i686-linux-gnu` → arch `x86`, OS `linux`, CPU `i686`).
- New `zig build jll` step that:
  - builds a shared library (`.so` / `.dylib` / `.dll`) with libc linked,
  - installs it to `${prefix}/lib` (or `${prefix}/bin` on Windows, per Zig's
    install conventions),
  - installs `LICENSE` to `${prefix}/share/licenses/libsie-z/LICENSE` (where
    BinaryBuilder's audit looks for it),
  - installs `include/sie.h` to `${prefix}/include/sie.h`.
- `link_libc = true` on the shared library module — `std.heap.c_allocator`
  (used by the C API for all allocations) requires libc.
- `comptime { _ = c_api; std.testing.refAllDecls(c_api); }` in `src/root.zig`
  to force emission of unused `export fn`s into the final shared object.

### 4. Manifest — [build.zig.zon](../build.zig.zon)

Added `include` to `paths` so the header ships in the source tarball.

## Verifying the layout locally (no Julia required)

From the repo root:

```pwsh
zig build jll -Dtriple=x86_64-linux-gnu --prefix zig-out/prefix
```

Should produce:

```
zig-out/prefix/include/sie.h
zig-out/prefix/lib/libsie.so
zig-out/prefix/share/licenses/libsie-z/LICENSE
```

For Windows DLLs, swap the triple (note: DLL goes under `bin/`, import lib
under `lib/`):

```pwsh
zig build jll -Dtriple=x86_64-w64-mingw32 --prefix zig-out/prefix
```

## Building the JLL

### One-time setup

You need Julia ≥ 1.9 and a Linux environment (or WSL2 on Windows / macOS),
because BinaryBuilder runs the build inside a Linux sandbox. From this
directory:

```bash
julia --project=. -e 'using Pkg; Pkg.instantiate()'
```

### Local single-platform build (no deploy)

This is the fastest way to confirm the recipe is healthy. Pass any one of
the platforms listed in `build_tarballs.jl` as a positional argument:

```bash
julia --project=. build_tarballs.jl --verbose --debug x86_64-linux-gnu
```

`--debug` drops you into a sandbox shell on failure so you can inspect the
build tree.

The resulting tarball is written under `products/` next to the script.

### Iterating against a working tree

In `build_tarballs.jl`, comment out the `GitSource(...)` block and uncomment
the `DirectorySource(...)` line. BB will tar your local checkout into the
sandbox instead of cloning. Useful while still developing the C API.

### Full multi-platform build + deploy

After tagging a release upstream and computing the tree hash:

```bash
# Resolve the commit SHA1 of the tag (despite the `tree_hash` name in the
# recipe, GitSource wants the commit hash, not the git tree hash):
git clone --bare --filter=blob:none \
    https://github.com/efollman/libsie-z.git /tmp/libsie-z.git
git -C /tmp/libsie-z.git rev-list -n 1 v0.2.0

# Then update LIBSIE_TREE_HASH (or the hard-coded value) and run:
LIBSIE_VERSION=0.2.0 \
LIBSIE_TREE_HASH=<hash from above> \
julia --project=. build_tarballs.jl --verbose \
    --deploy="efollman/libsie_jll.jl"
```

This will:
1. Build the shared library for every platform listed in `platforms`.
2. Run BB's audit (checks RPATHs, license placement, exported symbols, …).
3. Push tarballs to a GitHub release on `JuliaBinaryWrappers/libsie_jll.jl`
   and open / update the corresponding wrapper package PR.

### Registering the JLL

Once the wrapper PR is merged and the JLL is registered in the General
registry, downstream Julia packages can use the library via:

```julia
using libsie_jll
ccall((:sie_version, libsie), Cstring, ())
```

## Troubleshooting

- **"dependency on libc must be explicitly specified"** — the affected Zig
  module needs `link_libc = true`. Already set on every library variant
  here; only relevant if you add new modules.
- **Empty / 6 KB shared library** — exported `fn`s in an unreferenced module
  are stripped. The `comptime { _ = c_api; … }` block in
  [src/root.zig](../src/root.zig) forces emission; do not remove it.
- **`UnknownArchitecture` for some triple** — extend the table in
  `translateGnuTriple()` in [build.zig](../build.zig).
- **BB audit complains about missing license** — `install_license LICENSE`
  in the recipe's script handles this; keep it even though our `jll` step
  also installs the file (BB's audit looks at the file the script declared).
