function(add_velox_rhi_demo NAME)
    add_executable(${NAME} ${ARGN})
    
    set_target_properties(${NAME} PROPERTIES FOLDER "Demos")
    set_target_properties(${NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

    target_include_directories(${NAME} PRIVATE
        "${CMAKE_BINARY_DIR}/generated")
    if (NOT EMSCRIPTEN)
        target_link_libraries(${NAME} PRIVATE
            VeloxRhi
            glfw
            webgpu_dawn
            webgpu_glfw
            webgpu_cpp
            dawn_native
            magic_enum::magic_enum)
    endif()

    if (EMSCRIPTEN)

        target_link_libraries(${NAME} PRIVATE
            VeloxRhi emdawnwebgpu_cpp webgpu_glfw
            magic_enum::magic_enum)
    
        # Base Emscripten WebGPU flags
        target_compile_options(${NAME} PRIVATE -msimd128 -flto)
        target_link_options(${NAME} PRIVATE
            "-sEXIT_RUNTIME=1"
            "-sNO_FILESYSTEM=1"
            "-sWASM=1"
            "-sUSE_WEBGPU=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sMALLOC=emmalloc"
            "-sUSE_GLFW=3"
            "-flto")

        if (CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
            target_compile_options(${NAME} PRIVATE -g3 -O0)
            target_link_options(${NAME} PRIVATE
                -g3 -O0 "-gseparate-dwarf=${NAME}.wasm.dwp")
        else()
            target_compile_options(${NAME} PRIVATE -O3)
            target_link_options(${NAME} PRIVATE -O3)
        endif()

    endif()

    if (NOT EMSCRIPTEN)
        add_custom_command(TARGET ${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${NAME}> $<TARGET_FILE_DIR:${NAME}>
            COMMAND_EXPAND_LISTS)
    endif()
    
endfunction()

add_subdirectory(triangle)
