add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
add_repositories("groupmountain-repo https://github.com/GroupMountain/xmake-repo.git")
add_repositories("miracleforest-repo https://github.com/MiracleForest/xmake-repo.git")

local versions = {
    ["1.21.60.10"]  = "1.1.2",
    ["1.21.93.1"]   = "1.4.4",
    ["1.21.132.3"]  = "1.9.8"
}

option("target_version")
    set_default("1.21.132.3")
    set_showmenu(true)
    set_values(table.keys(versions))
option_end()

add_requires("levilamina " .. (versions[get_config("target_version") or "1.21.132.3"]), { configs = { target_type = "server" } })
add_requires("levibuildscript 0.5.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("iScale")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
    add_defines(
        "NOMINMAX",
        "UNICODE",
        "_HAS_CXX23=1",
        "_SILENCE_CXX20_IS_ALWAYS_EQUAL_DEPRECATION_WARNING",
        "MC_VERSION_" .. string.gsub(get_config("target_version") or "1.21.132.3", "%.", "_")
    )
    add_packages(
        "levilamina"
    )
    set_exceptions("none")
    set_kind("shared")
    set_languages("cxx20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    if is_mode("release") then
        set_strip("all")
        set_optimize("aggressive")
    end

    before_link(function(target)
        import("lib.detect.find_file")
        import("core.project.config")

        -- 修复莫名其妙的环境变量缺失导致的链接失败
        os.addenvs(target:pkgenvs())

        local libdir = path.join(config.builddir(), ".prelink", "lib")
        if os.exists(libdir) then os.rm(libdir) end
        os.mkdir(libdir)

        local data = assert(find_file("bedrock_runtime_data", {"$(env PATH)"}), "Cannot find bedrock_runtime_data")
        local link = assert(find_file("prelink.exe", {"$(env PATH)"}), "Cannot find prelink.exe")

        os.runv(link, {
            string.format("%s-%s-%s", get_config("target_type"), target:plat(), target:arch()),
            path.join(config.builddir(), ".prelink"),
            data,
            table.unpack(target:objectfiles())
        })

        target:add("linkdirs", libdir)
        target:add("links", "bedrock_runtime_api")
    end)

    after_build(function (target)
        import("utils.archive")

        local output_dir = path.join(os.projectdir(), "bin_" .. (get_config("target_version") or "1.21.132.3"), "iScale")

        os.rm(output_dir)

        os.vcp(target:targetfile(), format("%s/", output_dir))
        os.vcp(target:symbolfile(), format("%s/", output_dir))

        import("scripts.generate-manifest", { rootdir = os.projectdir() }).generate_manifest(
            format("%s/manifest.json", output_dir),
            {
                name = "iScale",
                entry = "iScale.dll",
                version = import("scripts.get-version-info", { rootdir = os.projectdir() }).get_version_info().version_str,
                author = "MiracleForest"
            }
        )

        archive.archive(
            path.join(os.projectdir(), "bin", "iScale." .. (get_config("target_version") or "1.21.132.3") .. ".zip"),
            output_dir
        )
    end)