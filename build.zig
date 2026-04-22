const std = @import("std");

pub fn build(b: *std.Build) void {
    // Allow callers (e.g. C-style cross-build drivers) to pass GNU-style
    // target triples via `-Dtriple=<triple>`. When supplied, this takes
    // precedence over the standard `-Dtarget=` option.
    const gnu_triple = b.option(
        []const u8,
        "triple",
        "GNU-style target triple (e.g. i686-linux-gnu, x86_64-w64-mingw32). Overrides -Dtarget.",
    );

    const target = if (gnu_triple) |t|
        b.resolveTargetQuery(translateGnuTriple(t))
    else
        b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Run test suite
    const test_step = b.step("test", "Run tests");

    // Example/demo build step
    const example_step = b.step("example", "Build examples");

    // Shared library build step
    const lib_step = b.step("lib", "Build shared library");

    // BinaryBuilder/JLL packaging step. Produces the exact layout BB expects
    // under ${prefix}: shared library in libdir (= bin/ on Windows, lib/
    // elsewhere) and the license under share/licenses/libsie-z/.
    // Defaults optimize to ReleaseSafe so JLL artifacts are not Debug builds.
    const jll_step = b.step("jll", "Build shared library + license layout for BinaryBuilder/JLL packaging");
    const jll_optimize = if (b.user_input_options.contains("optimize"))
        optimize
    else
        std.builtin.OptimizeMode.ReleaseSafe;

    // Create the libsie module
    const libsie_mod = b.createModule(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
        // The C ABI surface in src/c_api.zig uses std.heap.c_allocator,
        // which requires libc on every consumer of this module.
        .link_libc = true,
    });

    // Build sie_dump example
    const sie_dump = b.addExecutable(.{
        .name = "sie_dump",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/sie_dump.zig"),
            .target = target,
            .imports = &.{
                .{ .name = "libsie", .module = libsie_mod },
            },
        }),
    });
    const install_example = b.addInstallArtifact(sie_dump, .{});
    example_step.dependOn(&install_example.step);

    // Build sie_export example
    const sie_export = b.addExecutable(.{
        .name = "sie_export",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/sie_export.zig"),
            .target = target,
            .imports = &.{
                .{ .name = "libsie", .module = libsie_mod },
            },
        }),
    });
    const install_export = b.addInstallArtifact(sie_export, .{});
    example_step.dependOn(&install_export.step);

    // Build shared library
    const shared_lib = b.addLibrary(.{
        .name = "sie",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/root.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    const install_lib = b.addInstallArtifact(shared_lib, .{});
    lib_step.dependOn(&install_lib.step);

    // --- JLL packaging ---------------------------------------------------
    // Build a dedicated copy of the shared library at the JLL optimize level
    // so `zig build jll` doesn't depend on `-Doptimize=` being passed.
    const jll_lib = b.addLibrary(.{
        .name = "sie",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/root.zig"),
            .target = target,
            .optimize = jll_optimize,
            // The C ABI uses `std.heap.c_allocator`, which requires libc.
            .link_libc = true,
        }),
    });
    const install_jll_lib = b.addInstallArtifact(jll_lib, .{});
    jll_step.dependOn(&install_jll_lib.step);

    // Install the license where BinaryBuilder's audit looks for it.
    const install_license = b.addInstallFile(
        b.path("LICENSE"),
        "share/licenses/libsie-z/LICENSE",
    );
    jll_step.dependOn(&install_license.step);

    // Install the C header into ${prefix}/include for downstream FFI consumers.
    const install_header = b.addInstallFile(
        b.path("include/sie.h"),
        "include/sie.h",
    );
    jll_step.dependOn(&install_header.step);

    // Unit tests from src/
    const main_tests = b.addTest(.{
        .root_module = libsie_mod,
    });

    const run_main_tests = b.addRunArtifact(main_tests);
    test_step.dependOn(&run_main_tests.step);

    // Integration tests from test/ — discovered automatically so adding a
    // new test_*.zig file under test/ does not require touching build.zig.
    var test_dir = std.fs.cwd().openDir("test", .{ .iterate = true }) catch |err| {
        std.debug.panic("failed to open test/ directory: {s}", .{@errorName(err)});
    };
    defer test_dir.close();

    var it = test_dir.iterate();
    while (it.next() catch |err| std.debug.panic(
        "failed to iterate test/: {s}",
        .{@errorName(err)},
    )) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.endsWith(u8, entry.name, "_test.zig")) continue;

        const test_path = b.fmt("test/{s}", .{entry.name});
        const test_mod = b.createModule(.{
            .root_source_file = b.path(test_path),
            .target = target,
            .imports = &.{
                .{ .name = "libsie", .module = libsie_mod },
            },
        });
        const t = b.addTest(.{
            .root_module = test_mod,
        });
        const run_t = b.addRunArtifact(t);
        test_step.dependOn(&run_t.step);
    }
}

/// Translate a GNU-style target triple (as used by autotools / CMake / many
/// C cross-build drivers) into a `std.Target.Query` that Zig understands.
///
/// Supported inputs (extend as needed):
///   i686-linux-gnu              x86_64-linux-gnu        aarch64-linux-gnu
///   armv6l-linux-gnueabihf      armv7l-linux-gnueabihf
///   powerpc64le-linux-gnu       riscv64-linux-gnu
///   i686-linux-musl             x86_64-linux-musl       aarch64-linux-musl
///   armv6l-linux-musleabihf     armv7l-linux-musleabihf
///   x86_64-apple-darwin         aarch64-apple-darwin
///   x86_64-unknown-freebsd      aarch64-unknown-freebsd
///   i686-w64-mingw32            x86_64-w64-mingw32
fn translateGnuTriple(triple: []const u8) std.Target.Query {
    const Mapping = struct {
        gnu: []const u8,
        zig: []const u8,
        cpu: ?[]const u8 = null,
    };

    const table = [_]Mapping{
        // Linux / glibc
        .{ .gnu = "i686-linux-gnu", .zig = "x86-linux-gnu", .cpu = "i686" },
        .{ .gnu = "x86_64-linux-gnu", .zig = "x86_64-linux-gnu" },
        .{ .gnu = "aarch64-linux-gnu", .zig = "aarch64-linux-gnu" },
        .{ .gnu = "armv6l-linux-gnueabihf", .zig = "arm-linux-gnueabihf", .cpu = "arm1176jzf_s" },
        .{ .gnu = "armv7l-linux-gnueabihf", .zig = "arm-linux-gnueabihf", .cpu = "cortex_a7" },
        .{ .gnu = "powerpc64le-linux-gnu", .zig = "powerpc64le-linux-gnu" },
        .{ .gnu = "riscv64-linux-gnu", .zig = "riscv64-linux-gnu" },

        // Linux / musl
        .{ .gnu = "i686-linux-musl", .zig = "x86-linux-musl", .cpu = "i686" },
        .{ .gnu = "x86_64-linux-musl", .zig = "x86_64-linux-musl" },
        .{ .gnu = "aarch64-linux-musl", .zig = "aarch64-linux-musl" },
        .{ .gnu = "armv6l-linux-musleabihf", .zig = "arm-linux-musleabihf", .cpu = "arm1176jzf_s" },
        .{ .gnu = "armv7l-linux-musleabihf", .zig = "arm-linux-musleabihf", .cpu = "cortex_a7" },

        // macOS
        .{ .gnu = "x86_64-apple-darwin", .zig = "x86_64-macos-none" },
        .{ .gnu = "aarch64-apple-darwin", .zig = "aarch64-macos-none" },

        // FreeBSD
        .{ .gnu = "x86_64-unknown-freebsd", .zig = "x86_64-freebsd-none" },
        .{ .gnu = "aarch64-unknown-freebsd", .zig = "aarch64-freebsd-none" },

        // Windows / MinGW-w64
        .{ .gnu = "i686-w64-mingw32", .zig = "x86-windows-gnu", .cpu = "i686" },
        .{ .gnu = "x86_64-w64-mingw32", .zig = "x86_64-windows-gnu" },
    };

    for (table) |m| {
        // Accept either an exact match or the table key followed by an OS
        // version suffix (e.g. BinaryBuilder passes `aarch64-apple-darwin20`
        // and `x86_64-unknown-freebsd14.1`).
        const matches = std.mem.eql(u8, m.gnu, triple) or
            (std.mem.startsWith(u8, triple, m.gnu) and
                triple.len > m.gnu.len and
                (std.ascii.isDigit(triple[m.gnu.len]) or triple[m.gnu.len] == '.'));
        if (matches) {
            const query = std.Target.Query.parse(.{
                .arch_os_abi = m.zig,
                .cpu_features = m.cpu orelse "baseline",
            }) catch |err| {
                std.debug.panic("internal: failed to parse zig triple '{s}' for gnu triple '{s}': {s}", .{
                    m.zig, triple, @errorName(err),
                });
            };
            return query;
        }
    }

    // Fallback: try to feed the triple straight to Zig in case the caller
    // already passed a Zig-style one (e.g. `x86-linux-gnu`).
    return std.Target.Query.parse(.{ .arch_os_abi = triple }) catch {
        std.debug.panic(
            "unsupported -Dtriple value: '{s}'. Add a mapping in build.zig:translateGnuTriple.",
            .{triple},
        );
    };
}
