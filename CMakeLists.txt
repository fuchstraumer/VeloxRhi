cmake_minimum_required(VERSION 3.22)
project(VeloxRhi C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# cmake/ contains our own FindVulkanHeaders.cmake; must be set before any find_package calls
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# need this to generate various support files for the ocean fft
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if (NOT EMSCRIPTEN)
    find_package(Vulkan REQUIRED)
    set(DAWN_VULKAN_HEADERS_DIR ${Vulkan_INCLUDE_DIR})
    set(VulkanHeaders_INCLUDE_DIR ${Vulkan_INCLUDE_DIR} CACHE PATH "" FORCE)
endif()

# flags required to make things work at baseline on desktop toolchains (ime)
if (MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # MSVC specific flags
    # /MP: Tells cl.exe to multithread
    # /bigobj: Increases number of sections in .obj (slang can bump into this ime)
    # /arch:AVX2: Enables AVX2 instructions
    add_compile_options(/MP /bigobj /arch:AVX2)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang specific flags
    if (NOT EMSCRIPTEN)
        add_compile_options(-mavx2)
    endif()
endif()

# debug-specific flags and options
if (MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions($<$<CONFIG:Debug>:VELOX_ENABLE_DIAGNOSTICS>)
    add_compile_options(
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:RelWithDebInfo>:/Ob2 /Oi /Ot /Gy /Zo>
        $<$<CONFIG:Release>:/O2 /Ob3 /Oi /Ot /Gy /Oy /fp:fast /GL /Qpar>)
    add_link_options($<$<CONFIG:Debug>:/DEBUG>)
    add_link_options($<$<CONFIG:RelWithDebInfo>:/DEBUG>)
    add_link_options($<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
elseif(EMSCRIPTEN)
    add_compile_options(
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:RelWithDebInfo>:-Og -g -flto>
        $<$<CONFIG:Release>:-O3 -closure=2 -flto -fno-exceptions -fno-rtti -fno-threadsafe-statics>
        $<$<CONFIG:MinSizeRel>:-Oz -closure=2 -flto -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    add_link_options(
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:RelWithDebInfo>:-Og -g -flto>
        $<$<CONFIG:Release>:-O3 -closure=2>
        $<$<CONFIG:MinSizeRel>:-Oz -closure=2 -flto>)
endif()


if (NOT EMSCRIPTEN)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
    set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")
    add_subdirectory(third_party/glfw)
    set_target_properties(glfw PROPERTIES FOLDER "third_party")

    include(cmake/ConfigureSlang.cmake)
    add_subdirectory(third_party/slang EXCLUDE_FROM_ALL)
    organize_slang_targets()

endif()

include(cmake/ConfigureDawn.cmake)
add_subdirectory(third_party/dawn EXCLUDE_FROM_ALL)

set(MAGIC_ENUM_OPT_BUILD_EXAMPLES OFF)
set(MAGIC_ENUM_OPT_BUILD_TESTS OFF)
set(MAGIC_ENUM_OPT_INSTALL OFF)
add_subdirectory(third_party/magic_enum)

# Add the offline shader compiler, a tool for compiling shaders to baked-in WGSL source code for
# WebGPU to consume and bind at runtime. This has to be run before emscripten builds ship or are
# tested.
if(NOT EMSCRIPTEN)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tools/shader_cooker")
endif()

# rope together source files for imgui
set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_wgpu.cpp)

set(OCEAN_FFT_GENERATED_OUTPUTS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/DonelanBannerNormLUT.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/DonelanBannerNormLUT.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/SinCosLUT.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/SinCosLUT.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/OceanFFT_Generated.slang")

add_custom_command(
    OUTPUT ${OCEAN_FFT_GENERATED_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts/ocean_fft_support.py"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts/OceanFftConfig.yaml"
        --project_root "${CMAKE_CURRENT_SOURCE_DIR}"
        --lut_dir "${CMAKE_CURRENT_BINARY_DIR}/generated"
        --shader_dir "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts/ocean_fft_support.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts/OceanFftConfig.yaml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts"
    COMMENT "Generating OceanFFT support files..."
    VERBATIM)
set_source_files_properties(${OCEAN_FFT_GENERATED_OUTPUTS} PROPERTIES GENERATED TRUE)

set(VeloxRhi_AsyncSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/async/AsyncCore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/async/AsyncTasks.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/async/CoroutineAllocator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/async/Future.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/async/Scheduler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/async/AsyncTasks.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/async/Future.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/async/Scheduler.cpp")

set(VeloxRhi_CommonSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/VeloxErrors.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/VeloxLogging.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/VeloxErrors.cpp")

set(VeloxRhi_CoreSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/Application.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/InputEvent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/InputManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Application.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/InputManager.cpp")

set(VeloxRhi_UtilitySources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utility/MeshGenerator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utility/SlotMap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utility/MeshGenerator.cpp")

set(VeloxRhi_SharedMathSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/math/Math.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/math/Math.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/math/MathHashes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/math/MathPolynomials.hpp")

set(VeloxRhi_Shaders
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/OceanFFT.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/IfftPermute.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/IfftRadix2_f32.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/IfftRadix2.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/IfftRadix4.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/InitSpectrum.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/OceanFftBindings.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/OceanFftCommon.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/OceanFftDims.slang"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/compute/Spectra.slang")

# append corrent Math.inl backend based on platform
if (EMSCRIPTEN)
    set(VeloxRhi_BackendMathSources
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/MathBackendWASM.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/QuaternionBackendWASM.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/TranscendentalBackendWASM.inl")
elseif (WIN32)
    # todo/note: if we move off dx12 as a backend, we may just want to use GLM or the like
    set(VeloxRhi_BackendMathSources
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/MathBackendDX.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/QuaternionBackendDX.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/math/TranscendentalBackendDX.inl")
endif()

# todo-ship: Need to fix dxil.dll and related friends not copying to .exe output dir on Windows
add_library(VeloxRhi STATIC
    ${VeloxRhi_AsyncSources}
    ${VeloxRhi_CommonSources}
    ${VeloxRhi_CoreSources}
    ${VeloxRhi_UtilitySources}
    ${VeloxRhi_SharedMathSources}
    ${VeloxRhi_BackendMathSources}
    ${IMGUI_SOURCES}
    ${OCEAN_FFT_GENERATED_OUTPUTS})

target_include_directories(VeloxRhi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

# add mikktspace source, should run through C compiler
set(VeloxRHI_mikktspace_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c")

target_sources(VeloxRhi PRIVATE ${VeloxRHI_mikktspace_SOURCES})

source_group("rhi\\async" FILES ${VeloxRhi_AsyncSources})
source_group("rhi\\common" FILES ${VeloxRhi_CommonSources})
source_group("rhi\\core" FILES ${VeloxRhi_CoreSources})
source_group("rhi\\utility" FILES ${VeloxRhi_UtilitySources})
source_group("rhi\\math" FILES ${VeloxRhi_SharedMathSources} ${VeloxRhi_BackendMathSources})
source_group("rhi\\imgui" FILES ${IMGUI_SOURCES})
source_group("rhi\\shaders" FILES ${VeloxRhi_Shaders})
source_group("rhi\\generated" FILES ${OCEAN_FFT_GENERATED_OUTPUTS})

if (NOT EMSCRIPTEN)
    target_include_directories(VeloxRhi PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dawn/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/include)
endif()
# relaxed-simd does not degrade gracefully: a module carrying these opcodes fails validation outright
# on an engine without support rather than falling back at runtime. Chrome 114+ / Firefox 120+ /
# Safari 18+ are fine; older mobile is not. OFF widens support.
option(VELOX_RHI_MATH_RELAXED_SIMD "Use wasm relaxed-simd FMA in velox::math" ON)

if (EMSCRIPTEN)
    # PUBLIC: the math backend is header-only, instantiated in this library's TUs and every consumer's,
    # so both sides must agree on the SIMD feature set
    target_compile_options(VeloxRhi PUBLIC -msimd128)
    if (VELOX_RHI_MATH_RELAXED_SIMD)
        target_compile_options(VeloxRhi PUBLIC -mrelaxed-simd)
        target_compile_definitions(VeloxRhi PUBLIC VX_MATH_RELAXED_SIMD)
    endif()
endif()

# set required definition for imgui backend (dawn)
target_compile_definitions(VeloxRhi PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
if (CMAKE_CONFIG_TYPE MATCHES "Debug|RelWithDebInfo")
    target_compile_definitions(VeloxRhi PUBLIC VELOX_ENABLE_DIAGNOSTICS)
endif()

if (EMSCRIPTEN)
    target_link_libraries(VeloxRhi PUBLIC emdawnwebgpu_cpp webgpu_glfw magic_enum::magic_enum)
else()
    target_link_libraries(VeloxRhi PUBLIC webgpu_dawn glfw webgpu_glfw webgpu_cpp)
    if (WIN32)
        target_link_libraries(VeloxRhi PUBLIC dawn_native magic_enum::magic_enum)
    endif()
endif()

option(VELOX_RHI_BUILD_DEMOS "Build demo executables for the engine" ON)

if(VELOX_RHI_BUILD_DEMOS)
    add_subdirectory(demos)
endif()

option(VELOX_RHI_BUILD_TESTS "Build the standalone unit test executables" ON)

if(VELOX_RHI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit_tests)
endif()
