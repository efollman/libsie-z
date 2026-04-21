# SPDX-License-Identifier: MIT
#
# BinaryBuilder.jl recipe for libsie_jll.
#
# Usage (interactive build for one platform, no deploy):
#   julia --project=. build_tarballs.jl --verbose --debug x86_64-linux-gnu
#
# Usage (build all supported platforms and deploy to a JLL repo on GitHub):
#   julia --project=. build_tarballs.jl --verbose --deploy="JuliaBinaryWrappers/libsie_jll.jl"
#
# Before first run:
#   julia --project=. -e 'using Pkg; Pkg.instantiate()'
#
# This recipe expects a tagged release on the upstream Git repository (set
# `LIBSIE_REPO` and `LIBSIE_VERSION` below, or override via env vars).
# For local iteration against a working tree, point `sources` at a
# `DirectorySource` instead — see commented block below.

using BinaryBuilder, Pkg

name    = "libsie"
version = VersionNumber(get(ENV, "LIBSIE_VERSION", "0.2.0"))

# ── Sources ────────────────────────────────────────────────────────────────
# Production: pull a tagged tarball from GitHub. Replace `tree_hash` after
# tagging the release upstream (BB will tell you the expected hash on first
# run if it's wrong).
repo      = get(ENV, "LIBSIE_REPO",      "https://github.com/efollman/libsie-z.git")
tree_hash = get(ENV, "LIBSIE_TREE_HASH", "0000000000000000000000000000000000000000")

sources = [
    GitSource(repo, tree_hash),
]

# For local development, comment the GitSource block above and uncomment:
# sources = [
#     DirectorySource(joinpath(@__DIR__, ".."); target = "libsie-z"),
# ]

# ── Build script ───────────────────────────────────────────────────────────
# Runs inside the BinaryBuilder sandbox. `${target}` is the BB GNU triple,
# which `build.zig` translates to a Zig target via `-Dtriple=`.
script = raw"""
cd $WORKSPACE/srcdir/libsie-z*

# Zig's own cache lives under $HOME inside the sandbox.
export ZIG_GLOBAL_CACHE_DIR=$WORKSPACE/.zig-cache

zig build jll \
    -Dtriple=${target} \
    -Doptimize=ReleaseSafe \
    --prefix ${prefix}

install_license LICENSE
"""

# ── Platforms ──────────────────────────────────────────────────────────────
# All BinaryBuilder-supported platforms whose GNU triple our build.zig knows
# how to translate. Trim this list if you want to ship fewer artifacts.
platforms = [
    # Linux glibc
    Platform("i686",    "linux"; libc="glibc"),
    Platform("x86_64",  "linux"; libc="glibc"),
    Platform("aarch64", "linux"; libc="glibc"),
    Platform("armv6l",  "linux"; libc="glibc", call_abi="eabihf"),
    Platform("armv7l",  "linux"; libc="glibc", call_abi="eabihf"),
    Platform("powerpc64le", "linux"; libc="glibc"),
    Platform("riscv64", "linux"; libc="glibc"),

    # Linux musl
    Platform("i686",    "linux"; libc="musl"),
    Platform("x86_64",  "linux"; libc="musl"),
    Platform("aarch64", "linux"; libc="musl"),
    Platform("armv6l",  "linux"; libc="musl", call_abi="eabihf"),
    Platform("armv7l",  "linux"; libc="musl", call_abi="eabihf"),

    # macOS
    Platform("x86_64",  "macos"),
    Platform("aarch64", "macos"),

    # FreeBSD
    Platform("x86_64",  "freebsd"),
    Platform("aarch64", "freebsd"),

    # Windows
    Platform("i686",    "windows"),
    Platform("x86_64",  "windows"),
]

# ── Products ───────────────────────────────────────────────────────────────
# `LibraryProduct(:sie, :libsie)` tells BB to look for libsie.{so,dylib,dll}
# under ${prefix}/lib (Unix) or ${prefix}/bin (Windows). The Julia side
# accesses it as `libsie_jll.libsie`.
products = [
    LibraryProduct("libsie", :libsie),
]

# ── Dependencies ───────────────────────────────────────────────────────────
# libsie has no third-party runtime dependencies — only libc.
dependencies = HostBuildDependency[]

# ── Build ──────────────────────────────────────────────────────────────────
# `julia_compat` is the JLL-side Julia version constraint, not a build
# requirement. Bump as needed.
build_tarballs(
    ARGS, name, version, sources, script, platforms, products, dependencies;
    julia_compat   = "1.9",
    preferred_gcc_version = v"10",  # only used for the host-tool stage
)
