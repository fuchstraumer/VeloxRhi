# Standalone test executables - no GTest, no Catch2. Each one is a plain program that prints its
# failures and returns nonzero, so it can be run by hand or through ctest. Under the Emscripten
# preset these build to .js/.wasm and are run with node; natively they are ordinary .exe files.
#
# The point of these is that broken math is otherwise only discoverable by staring at wrong pixels.
# or vertex soup scattered across the screen into an urecognizable smear of bad transforms.
function(add_velox_unit_test NAME)
    add_executable(${NAME} ${ARGN} TestHarness.cpp TestHarness.hpp)

    set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
    set_target_properties(${NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

    # Only the headers are needed: velox::math is header-only, so linking the whole library (and
    # therefore Dawn and GLFW) would make these far slower to build than they need to be
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
