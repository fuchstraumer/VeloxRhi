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

# include our various scripts: most of these in cmake/ are just to reduce targets we don't
# need across the fairly massive surface of third_party deps we pull in. As this project
# in particular is a scoped endpoint, this is fairly safe to do
include(cmake/ConfigureDawn.cmake)
include(cmake/ConfigureBuildFlags.cmake)
include(cmake/ConfigureMiscThirdParty.cmake)

if (NOT EMSCRIPTEN)
    include(cmake/ConfigureSlang.cmake)
endif()

# need this to generate various support files for the ocean fft
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if (NOT EMSCRIPTEN)
    find_package(Vulkan REQUIRED)
    set(DAWN_VULKAN_HEADERS_DIR ${Vulkan_INCLUDE_DIR})
    set(VulkanHeaders_INCLUDE_DIR ${Vulkan_INCLUDE_DIR} CACHE PATH "" FORCE)
endif()


if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT EMSCRIPTEN)
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "Clang")
        message(FATAL_ERROR "Clang-CL in MSVC frontend variant is not supported. Please use a different compiler.")
    else()
        add_msvc_exclusive_build_flags_all_targets()
        add_shared_msvc_style_build_flags_all_targets()
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT EMSCRIPTEN)
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # Running as clang, but with MSVC frontend (clang-cl as shim layer)
        add_shared_msvc_style_build_flags_all_targets()
        disable_msvc_frontend_clang_compiler_warnings_all_targets()
    else()
        add_clang_style_build_flags_all_targets()
    endif()
elseif(EMSCRIPTEN)
    add_emscripten_build_flags_all_targets()
else()
    message(STATUS
        "Could not find supported compiler. Please use Clang, MSVC, or Emscripten and make sure they are on your PATH.")
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if (NOT EMSCRIPTEN)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
    set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")
    add_subdirectory(third_party/glfw)
    set_target_properties(glfw PROPERTIES FOLDER "third_party")

    add_subdirectory(third_party/slang EXCLUDE_FROM_ALL)
    organize_slang_targets()

endif()

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

set(VeloxRhi_GraphSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/GraphBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/ResourceBinding.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/ResourceDeclaration.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/ResourceFlagConversions.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/ResourceFlags.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/ResourceHandle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph/SubpassBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graph/ResourceFlagConversions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graph/SubpassDesc.hpp")

# The vocabulary the shader cooker writes and the rendergraph reads. The cooker compiles these same
# files, so the offline tool and the runtime cannot disagree about what a binding is.
set(VeloxRhi_ShaderSources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/shader/ShaderLibraryTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shader/ShaderLibraryTypes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/shader/ShaderManifest.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shader/ShaderManifest.cpp")

set(VeloxRhi_UtilitySources
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utility/EnumClassUtils.hpp"
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
    ${VeloxRhi_ShaderSources}
    ${VeloxRhi_UtilitySources}
    ${VeloxRhi_SharedMathSources}
    ${VeloxRhi_BackendMathSources}
    ${IMGUI_SOURCES}
    ${OCEAN_FFT_GENERATED_OUTPUTS})

target_include_directories(VeloxRhi PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(VeloxRhi PUBLIC
        $<$<CONFIG:Debug,RelWithDebInfo>:VELOX_ENABLE_DIAGNOSTICS=1>)
# add mikktspace source, should run through C compiler
set(VeloxRHI_mikktspace_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c")

target_sources(VeloxRhi PRIVATE ${VeloxRHI_mikktspace_SOURCES})

source_group("rhi\\async" FILES ${VeloxRhi_AsyncSources})
source_group("rhi\\common" FILES ${VeloxRhi_CommonSources})
source_group("rhi\\core" FILES ${VeloxRhi_CoreSources})
source_group("rhi\\shader" FILES ${VeloxRhi_ShaderSources})
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
