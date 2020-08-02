const std = @import("std");
const Builder = std.build.Builder;

fn addFiles(b: *Builder, exe: *std.build.LibExeObjStep, dir: std.fs.Dir, rel: []const u8) void {
    var i = dir.iterate();
    while (i.next() catch @panic("Error iterating over source files!")) |file| {
        var path = std.fmt.allocPrint(b.allocator, "{}/{}", .{ rel, file.name }) catch @panic("Unable to allocatre path");
        switch (file.kind) {
            .File => {
                // These allocations should only be freed after the build, so we
                // don't bother doing it, and just let the OS handle it.
                exe.addCSourceFile(path, &[_][]const u8{});
            },
            .Directory => {
                var subdir = dir.openDir(file.name, .{ .iterate = true }) catch @panic("Error recursively finding files!");
                defer subdir.close();
                addFiles(b, exe, subdir, path);
            },
            else => @panic("... what have we done."),
        }
    }
}

pub fn build(b: *Builder) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("coyote", null);
    var dir = std.fs.cwd().openDir("src", .{ .iterate = true }) catch @panic("Error opening 'src/'");
    defer dir.close();

    addFiles(b, exe, dir, "src");
    exe.linkLibC();

    exe.setTarget(target);
    exe.setBuildMode(mode);
    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run coyote");
    run_step.dependOn(&run_cmd.step);
}
