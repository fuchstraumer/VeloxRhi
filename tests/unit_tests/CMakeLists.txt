# Standalone test executables - no GTest, no Catch2. Each prints its failures and returns nonzero, so
# it runs by hand or under ctest. Emscripten builds these to .js/.wasm run via node; native builds are
# ordinary .exe. They exist because broken math is otherwise only visible as wrong pixels, or as vertex
# soup smeared across the screen by bad transforms.
function(add_velox_unit_test NAME)
    add_executable(${NAME} ${ARGN} TestHarness.cpp TestHarness.hpp)

    set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
    set_target_properties(${NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

    # headers only: velox::math is header-only, and linking the library would drag in Dawn and GLFW
    target_include_directories(${NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

    if (EMSCRIPTEN)
        target_compile_options(${NAME} PRIVATE -msimd128)
        if (VELOX_RHI_MATH_RELAXED_SIMD)
            target_compile_options(${NAME} PRIVATE -mrelaxed-simd)
            target_compile_definitions(${NAME} PRIVATE VX_MATH_RELAXED_SIMD)
        endif()
        target_link_options(${NAME} PRIVATE "-sEXIT_RUNTIME=1" "-sNO_FILESYSTEM=1")

        # node runs the emitted .js, which loads the sibling .wasm
        find_program(VELOX_NODE_EXECUTABLE NAMES node node.exe)
        if (VELOX_NODE_EXECUTABLE)
            add_test(NAME ${NAME}
                     COMMAND ${VELOX_NODE_EXECUTABLE} "$<TARGET_FILE_DIR:${NAME}>/${NAME}.js")
        endif()
    else()
        add_test(NAME ${NAME} COMMAND ${NAME})
    endif()
endfunction()

add_velox_unit_test(MathTests MathTests.cpp)
