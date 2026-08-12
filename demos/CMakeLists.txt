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
        target_compile_options(${NAME} PRIVATE -flto)
        target_link_options(${NAME} PRIVATE
            "-sEXIT_RUNTIME=1"
            "-sNO_FILESYSTEM=1"
            "-sWASM=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sMALLOC=emmalloc"
            "-sUSE_GLFW=3")

        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            target_compile_options(${NAME} PRIVATE -g -O0)
            target_link_options(${NAME} PRIVATE -g -O0)
            target_link_options(${NAME} PRIVATE "-gseparate-dwarf")
        elseif (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
            target_compile_options(${NAME} PRIVATE -g -O2)
            target_link_options(${NAME} PRIVATE -g -O2 "-gseparate-dwarf")
        elseif (CMAKE_BUILD_TYPE MATCHES "MinSizeRel")
            target_compile_options(${NAME} PRIVATE -Oz -fno-exceptions -fno-rtti -flto)
            target_link_options(${NAME} PRIVATE -Oz -closure=2 -flto)
        else()
            target_compile_options(${NAME} PRIVATE -Oz -fno-exceptions -fno-rtti -flto)
            target_link_options(${NAME} PRIVATE -Oz -closure=2 -flto)
        endif()

        set_target_properties(${NAME} PROPERTIES SUFFIX ".html")
    endif()

    if (CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        target_compile_definitions(${NAME} PUBLIC VELOX_ENABLE_DIAGNOSTICS)
    endif()

    if (NOT EMSCRIPTEN)
        add_custom_command(TARGET ${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${NAME}> $<TARGET_FILE_DIR:${NAME}>
            COMMAND_EXPAND_LISTS)
    endif()
    
endfunction()

add_subdirectory(triangle)
add_subdirectory(mesh)
