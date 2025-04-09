const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const mode = b.standardOptimizeOption(.{});

    const c_flags = [_][]const u8{
        "-std=c11",
        "-pedantic",
        "-Wall",
        "-Wno-missing-field-initializers",
    };

    // Search for all C/C++ files in `src` and add them
    var sources = std.ArrayList([]const u8).init(b.allocator);
    try findFiles("./src", &[_][]const u8{".c"}, &sources, b);

    ////////////////////// Build //////////////////////
    const buildModule = b.createModule(.{
        .target = target,
        .optimize = mode,
        .link_libc = true,
    });
    buildModule.addCSourceFiles(.{
        .files = sources.items,
        .flags = &c_flags,
        .root = b.path("src"),
    });
    buildModule.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = &c_flags,
    });

    const pit = b.addExecutable(.{
        .name = "pit",
        .root_module = buildModule,
    });

    b.installArtifact(pit);

    const run_cmd = b.addRunArtifact(pit);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run pit");
    run_step.dependOn(&run_cmd.step);

    ////////////////////// Tests //////////////////////

    const test_flags = [_][]const u8{
        "-std=c11",
        "-pedantic",
        "-Wall",
        "-Wno-missing-field-initializers",
        "-lcmocka",
        "-Wl,-wrap,realloc",
    };

    const testModule = b.createModule(.{
        .target = target,
        .optimize = mode,
        .link_libc = true,
        .root_source_file = b.path("test/tests.zig"),
    });
    testModule.addCSourceFiles(.{
        .files = sources.items,
        .flags = &test_flags,
        .root = b.path("src"),
    });
    testModule.linkSystemLibrary("cmocka", .{});
    testModule.addCSourceFile(.{
        .file = b.path("test/wrappers/realloc_wrapper.c"),
        .flags = &test_flags,
    });

    const unit_tests = b.addTest(.{
        .root_module = testModule,
        .target = target,
        .optimize = mode,
        .link_libc = true,
    });
    unit_tests.addIncludePath(b.path("./src"));

    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_unit_tests.step);
}

pub fn findFiles(sub_path: []const u8, extensions: []const []const u8, arrayList: *std.ArrayList([]const u8), b: *std.Build) !void {
    var dir = try std.fs.cwd().openDir(sub_path, .{ .iterate = true });

    var walker = try dir.walk(b.allocator);
    defer walker.deinit();

    while (try walker.next()) |entry| {
        const ext = std.fs.path.extension(entry.basename);
        const valid_extension = for (extensions) |e| {
            if (std.mem.eql(u8, ext, e))
                break true;
        } else false;
        if (valid_extension and !std.mem.eql(u8, entry.basename, "main.c")) {
            //            std.debug.print("found {s} file: {s}\n", .{ ext, entry.path });
            // we have to clone the path as walker.next() or walker.deinit() will override/kill it
            try arrayList.append(b.dupe(entry.path));
        }
    }
}
