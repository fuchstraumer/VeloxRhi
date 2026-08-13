# Standalone test executables - no GTest, no Catch2. Each prints its failures and returns nonzero, so
# it runs by hand or under ctest. Emscripten builds these to .js/.wasm run via node; native builds are
# ordinary .exe. They exist because broken math is otherwise only visible as wrong pixels, or as vertex
# soup smeared across the screen by bad transforms.
function(add_velox_unit_test NAME)
    add_executable(${NAME} ${ARGN} TestHarness.cpp TestHarness.hpp)

    set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
    set_target_properties(${NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

    # headers only: velox::math is header-only, and linking the library would drag in Dawn and GLFW.
    # include/math is on the path because TestHarness.cpp and MathTests.cpp both say "Math.hpp"
    # unqualified, the way the library's own sources do.
    target_include_directories(${NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/include/math")

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

# The cooker's size-expression parser is pure -- a string and a symbol table in, an integer out -- so
# it is tested by compiling those two sources directly rather than by linking the host tool, which
# would drag in Slang. That also keeps the test running under the Emscripten preset.
set(VELOX_SHADER_COOKER_DIR "${CMAKE_SOURCE_DIR}/tools/shader_cooker")
add_velox_unit_test(SizeExpressionTests
    SizeExpressionTests.cpp
    "${VELOX_SHADER_COOKER_DIR}/src/SizeExpression.cpp"
    "${VELOX_SHADER_COOKER_DIR}/src/CookerErrors.cpp")
target_include_directories(SizeExpressionTests PRIVATE "${VELOX_SHADER_COOKER_DIR}/include")
target_link_libraries(SizeExpressionTests PRIVATE magic_enum::magic_enum)
