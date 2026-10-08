set(LZ4_BUILD_CLI OFF CACHE BOOL "Build lz4 program")
set(LLVM_ENABLE_WARNINGS OFF CACHE BOOL "Enable compiler warnings")
# tested with LLVM_INCLUDE_TOOLS disable: breaks DXC build and install, removed. UTILS is safe though
set(LLVM_INCLUDE_UTILS OFF CACHE BOOL "Generate build targets for the LLVM utils.")
# CPack toggles. not sure where these are used, but I'm trying to cut down targets that we don't need
set(CPACK_BINARY_NSIS OFF CACHE BOOL "Enable to build NSIS packages")
set(CPACK_SOURCE_7Z OFF CACHE BOOL "Enable to build 7-Zip source packages")
set(CPACK_SOURCE_ZIP OFF CACHE BOOL "Enable to build ZIP source packages")