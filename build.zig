const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Run test suite
    const test_step = b.step("test", "Run tests");

    // Example/demo build step
    const example_step = b.step("example", "Build sie_dump example");

    // Shared library build step
    const lib_step = b.step("lib", "Build shared library");

    // Create the libsie module
    const libsie_mod = b.createModule(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
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

    // Build shared library
    const shared_lib = b.addLibrary(.{
        .name = "sie",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/root.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    const install_lib = b.addInstallArtifact(shared_lib, .{});
    lib_step.dependOn(&install_lib.step);

    // Unit tests from src/
    const main_tests = b.addTest(.{
        .root_module = libsie_mod,
    });

    const run_main_tests = b.addRunArtifact(main_tests);
    test_step.dependOn(&run_main_tests.step);

    // Integration tests from test/
    const integration_test_files = [_][]const u8{
        "test/decoder_test.zig",
        "test/file_test.zig",
        "test/api_test.zig",
        "test/functional_test.zig",
        "test/spigot_test.zig",
        "test/file_highlevel_test.zig",
        "test/functional_dump_test.zig",
        "test/spigot_data_test.zig",
        "test/regression_test.zig",
        "test/histogram_test.zig",
        "test/output_test.zig",
        "test/xml_test.zig",
        "test/object_test.zig",
        "test/stringtable_test.zig",
        "test/relation_test.zig",
        "test/id_map_test.zig",
        "test/context_test.zig",
        "test/xml_merge_test.zig",
        "test/sifter_test.zig",
        "test/file_stream_test.zig",
    };

    for (integration_test_files) |test_file| {
        const test_mod = b.createModule(.{
            .root_source_file = b.path(test_file),
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
