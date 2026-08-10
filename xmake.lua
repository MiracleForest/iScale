add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_requires("levilamina 26.20.7", { configs = { target_type = get_config("target_type") } })
add_requires("levibuildscript 0.5.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("iScale")
    add_rules("@levibuildscript/linkrule")
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
    add_cxflags(
        "-Wno-microsoft-cast",
        "-Wno-invalid-offsetof",
        "-Wno-c++2b-extensions",
        "-Wno-microsoft-include",
        "-Wno-overloaded-virtual",
        "-Wno-ignored-qualifiers",
        "-Wno-missing-field-initializers",
        "-Wno-potentially-evaluated-expression",
        "-Wno-pragma-system-header-outside-header",
        { tools = { "clang_cl" } }
    )
    add_defines(
        "NOMINMAX",
        "UNICODE",
        "_HAS_CXX23=1",
        "_SILENCE_CXX20_IS_ALWAYS_EQUAL_DEPRECATION_WARNING",
        ( is_config("target_type", "server") and "LL_PLAT_S" or "LL_PLAT_C" )
    )
    add_packages(
        "levilamina"
    )
    set_toolchains("clang-cl")
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

    after_build(function (target)
        import("utils.archive")

        local output_dir = path.join(os.projectdir(), "bin", "iScale")

        os.rm(output_dir)

        os.vcp(target:targetfile(), format("%s/", output_dir))
        os.vcp(target:symbolfile(), format("%s/", output_dir))

        import("scripts.generate-manifest", { rootdir = os.projectdir() }).generate_manifest(
            format("%s/manifest.json", output_dir),
            {
                name = target:name(),
                entry = path.basename(target:targetfile()),
                version = import("scripts.get-version-info", { rootdir = os.projectdir() }).get_version_info().version_str,
                author = "MiracleForest",
                platform = get_config("target_type"),
                description = "A LeviLamina mod for scaling player size.",
                extraInfo = {
                    gitHub = "https://github.com/MiracleForest/iScale"
                }
            }
        )
    end)