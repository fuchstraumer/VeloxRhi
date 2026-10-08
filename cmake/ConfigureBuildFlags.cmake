# There are a number of warnings that clang-cl will emit that are not relevant to *our* code
# but come from third-party libraries and using clang-cl on windows. We only add this when
# on Win32 and using clang-cl with MSVC frontend/env
function(disable_msvc_frontend_clang_compiler_warnings_all_targets)
    add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
    # -Wno-unused-command-line-argument is needed as clang-cl will emit this warning
    # when using /MP and for the appended /Zc:preprocessor
    # So many more needed because DXC is a bit of a hot mess and clang-cl is quite strict
    # (good! but a pain for us....)
    add_compile_options("-Wno-variadic-macro-arguments-omitted"
                        "-Wno-microsoft-enum-value"
                        "-Wno-deprecated-declarations"
                        "-Wno-assume"
                        "-Wno-unused-private-field"
                        "-Wno-switch-default"
                        "-Wno-switch"
                        "-Wno-switch-enum"
                        "-Wno-nrvo"
                        "-Wno-unused-command-line-argument"
                        "-Wno-nullability-extension"
                        )
endfunction()

function(add_emscripten_build_flags_all_targets)
    add_compile_options(
        "$<$<CONFIG:Debug>:-g-separate-dwarf>"
        "$<$<CONFIG:RelWithDebInfo>:-g-separate-dwarf>"
        "$<$<CONFIG:Release>:-closure=1;>"
        "$<$<CONFIG:MinSizeRel>:-closure=2;>")
    add_link_options(
        "$<$<CONFIG:Release>:-closure=1>"
        "$<$<CONFIG:MinSizeRel>:-closure=2;>")
endfunction()
