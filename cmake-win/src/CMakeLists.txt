cmake_minimum_required(VERSION 3.20)
project(neosu VERSION 41.10 LANGUAGES C CXX)

# MSVC specific configuration
if(NOT MSVC)
    message(FATAL_ERROR "This CMake configuration is specifically for MSVC")
endif()

find_program(CCACHE ccache)
if(NOT CCACHE STREQUAL "CCACHE-NOTFOUND")
    set(CMAKE_C_COMPILER_LAUNCHER ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Standards
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)

# Use static runtime libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependencies install directory (passed from superbuild)
if(NOT DEPS_INSTALL_DIR)
    set(DEPS_INSTALL_DIR "${CMAKE_BINARY_DIR}/../deps/install")
endif()

# soundtouch's cmake interface is broken and cmake complains about a non-existent directory
make_directory(${DEPS_INSTALL_DIR}/COMPONENT)
make_directory(${DEPS_INSTALL_DIR}/SoundTouch)

# Dependencies cache directory (passed from superbuild)
if(NOT DEPS_CACHE)
    set(DEPS_CACHE "${CMAKE_BINARY_DIR}/../depcache")
endif()

# BASS version (passed from superbuild)
if(NOT BASS_VERSION)
    set(BASS_VERSION "20250813")
endif()

# Set variables for config.h generation
set(PROJECT_TARNAME "neosu")
set(PROJECT_URL "https://github.com/kiwec/neosu")
string(TIMESTAMP BUILD_TIMESTAMP %y%m%d%H%M UTC)
message(STATUS "Build timestamp: ${BUILD_TIMESTAMP}")

# Platform and feature detection
set(MCENGINE_PLATFORM_WINDOWS ON)
set(MCENGINE_FEATURE_DISCORD ON)
set(MCENGINE_FEATURE_OPENGL ON)
set(MCENGINE_FEATURE_BASS ON)
set(MCENGINE_FEATURE_SOLOUD ON)
set(MCENGINE_FEATURE_DIRECTX11 ON)
option(WITH_LIVEPP "Enable LivePP support" OFF)

if(WITH_LIVEPP)
    message(STATUS "LivePP support enabled")
    add_compile_definitions(WITH_LIVEPP)
    include_directories("${CMAKE_SOURCE_DIR}/../../LivePP/API/x64")
endif()

if(NOT DISABLE_NSYNC)
set(USE_NSYNC ON)
endif()

# Generate config.h
configure_file(
    "${CMAKE_SOURCE_DIR}/../config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)

# Generate build_timestamp.h
configure_file(
    "${CMAKE_SOURCE_DIR}/../build_timestamp.h.in"
    "${CMAKE_BINARY_DIR}/build_timestamp.h"
    @ONLY
)

# Set CMAKE_PREFIX_PATH to help find_package locate our dependencies
list(PREPEND CMAKE_PREFIX_PATH "${DEPS_INSTALL_DIR}")

find_package(PkgConfig REQUIRED)

set(ENV{PKG_CONFIG_PATH} "${DEPS_INSTALL_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${DEPS_INSTALL_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_MSVC_SYNTAX} 1)
set(PKG_CONFIG_ARGN "--msvc-syntax")

# Find system dependencies
find_package(OpenGL REQUIRED)

# Find our built dependencies using CMake's native finders
find_package(ZLIB REQUIRED)
find_package(libjpeg-turbo REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(CURL REQUIRED)
find_package(MPG123 REQUIRED)
find_package(SoundTouch REQUIRED)
find_package(SoLoud REQUIRED)
find_package(simdutf REQUIRED)

# Find packages that provide config files
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)

# Optionally use nsync
if(NOT DISABLE_NSYNC)
    find_package(nsync_cpp)
    if(nsync_cpp_FOUND)
        message(STATUS "Building with nsync support")
    else()
        message(FATAL_ERROR "nsync desired but not found")
    endif()
else()
    message(STATUS "Building without nsync support (explicitly disabled)")
endif()

# Handle these manually since their CMake support is broken
pkg_check_modules(BZIP2 REQUIRED bzip2)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

# for binary resources
enable_language(ASM_NASM)

include(${CMAKE_SOURCE_DIR}/binclude.cmake)

# Collect source files
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/*.c"
    "${CMAKE_SOURCE_DIR}/../../libraries/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../libraries/*.c"
)

# Add main executable target
add_executable(neosu ${SOURCES})

# Get all source files and extract their directories
file(GLOB_RECURSE ALL_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/*.h"
)

# Extract unique directories from the source files
set(SRC_DIRS)
foreach(SOURCE_FILE ${ALL_SOURCE_FILES})
    get_filename_component(DIR ${SOURCE_FILE} DIRECTORY)
    list(APPEND SRC_DIRS ${DIR})
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES SRC_DIRS)

# BASS library paths (using cached downloads)
set(BASS_CACHE_DIR "${DEPS_CACHE}/bass-${BASS_VERSION}")

# Include directories
target_include_directories(neosu PRIVATE
    ${CMAKE_BINARY_DIR}  # for generated config.h
    ${CMAKE_SOURCE_DIR}/../../src
    ${SRC_DIRS}
    ${CMAKE_SOURCE_DIR}/../../libraries/
    ${CMAKE_SOURCE_DIR}/../../libraries/glad
    ${CMAKE_SOURCE_DIR}/../../libraries/delegatesv2
    ${BASS_CACHE_DIR}/bass/include
    ${BASS_CACHE_DIR}/bassfx/include
    ${BASS_CACHE_DIR}/bassmix/include
    ${BASS_CACHE_DIR}/bassloud/include
    ${BASS_CACHE_DIR}/bassasio/include
    ${BASS_CACHE_DIR}/basswasapi/include
    ${DEPS_INSTALL_DIR}/include
    ${DEPS_INSTALL_DIR}/include/soundtouch # thanks...
    ${BZIP2_STATIC_INCLUDE_DIRS}
    ${LIBARCHIVE_STATIC_INCLUDE_DIRS}
)

# Strip default cmake exception handling flags
string(REGEX REPLACE "/EH[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Apply compiler options
target_compile_options(neosu PRIVATE
    "/utf-8"
    "/Zc:preprocessor"
    "/GR"
    "/MP"  # multi-processor compilation
    "/D_UNICODE"
    "/DUNICODE=1"
    "/EHs-c-"
    "/D_HAS_EXCEPTIONS=0" # idk why it needs to be added here too...?
    $<$<CONFIG:Debug>:/Zi;/Od;/RTC1;/Gm->
    $<$<CONFIG:Release>:/Zi;/O2;/Oi;/Oy;/Gw;/GS-;/Gy>
    ${BZIP2_STATIC_CFLAGS}
    ${LIBARCHIVE_STATIC_CFLAGS}
)

# Definitions
target_compile_definitions(neosu PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    FMT_USE_EXCEPTIONS=0
    _HAS_EXCEPTIONS=0
    ST_NO_EXCEPTION_HANDLING=1
    SOLOUD_NO_ASSERTS=1
    _UNICODE
    UNICODE=1
    SPDLOG_NO_EXCEPTIONS
    SPDLOG_FMT_EXTERNAL
    SPDLOG_HEADER_ONLY
    SPDLOG_WCHAR_TO_UTF8_SUPPORT
    SPDLOG_UTF8_TO_WCHAR_CONSOLE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

# Link options
target_link_options(neosu PRIVATE
    $<$<CONFIG:Debug>:/DEBUG:FULL;/SUBSYSTEM:CONSOLE;/ENTRY:wmainCRTStartup;/INCREMENTAL;/FUNCTIONPADMIN;/OPT:NOREF;/OPT:NOICF>
    $<$<CONFIG:Release>:/DEBUG:FULL;/SUBSYSTEM:WINDOWS;/ENTRY:wWinMainCRTStartup;/OPT:REF;/OPT:ICF>
    /STACK:8388608 # 8MB default stack size
    ${LIBARCHIVE_STATIC_LDFLAGS}
)

target_link_directories(neosu PRIVATE
    ${DEPS_INSTALL_DIR}/lib
    ${BZIP2_STATIC_LIBRARY_DIRS}
    ${LIBARCHIVE_STATIC_LIBRARY_DIRS}
)

# binary resources
target_binary_resources(neosu PRIVATE
    GL_cursortrail_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_cursortrail_v.glsl"
    GL_cursortrail_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_cursortrail_f.glsl"
    GL_slider_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_slider_v.glsl"
    GL_slider_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_slider_f.glsl"
    GL_flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_flashlight_v.glsl"
    GL_flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_flashlight_f.glsl"
    GL_actual_flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_actual_flashlight_v.glsl"
    GL_actual_flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_actual_flashlight_f.glsl"
    GL_smoothclip_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_smoothclip_v.glsl"
    GL_smoothclip_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/GL_smoothclip_f.glsl"
    DX11_default_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_default_v.hlsl"
    DX11_default_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_default_f.hlsl"
    DX11_cursortrail_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_cursortrail_v.hlsl"
    DX11_cursortrail_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_cursortrail_f.hlsl"
    DX11_slider_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_slider_v.hlsl"
    DX11_slider_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_slider_f.hlsl"
    DX11_flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_flashlight_v.hlsl"
    DX11_flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_flashlight_f.hlsl"
    DX11_actual_flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_actual_flashlight_v.hlsl"
    DX11_actual_flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_actual_flashlight_f.hlsl"
    DX11_smoothclip_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_smoothclip_v.hlsl"
    DX11_smoothclip_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/DX11_smoothclip_f.hlsl"
    convar_template "${CMAKE_SOURCE_DIR}/../../assets/misc/convar_template.html"
)

# Link libraries
target_link_libraries(neosu PRIVATE
    OpenGL::GL
    OpenGL::GLU
    SDL3::SDL3-static
    ZLIB::ZLIB
    libjpeg-turbo::turbojpeg-static
    PNG::PNG
    Freetype::Freetype
    fmt::fmt
    spdlog::spdlog_header_only
    LibLZMA::LibLZMA
    CURL::libcurl
    SoundTouch::SoundTouch
    SoLoud::soloud
    MPG123::libmpg123
    simdutf::simdutf
    ${DEPS_INSTALL_DIR}/lib/bz2_static.lib # cool awesome
    ${LIBARCHIVE_STATIC_LIBRARIES}
    comctl32
)

if(NOT DISABLE_NSYNC AND nsync_cpp_FOUND)
    target_link_libraries(neosu PRIVATE nsync_cpp)
    target_compile_definitions(neosu PRIVATE NSYNC_ATOMIC_CPP11 NSYNC_USE_CPP11_TIMEPOINT)
endif()

# Installation
install(TARGETS neosu RUNTIME DESTINATION bin)

install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION bin OPTIONAL)

# Install assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../../assets/materials DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../../assets/fonts DESTINATION bin)

# Install BASS DLLs
install(FILES
    "${BASS_CACHE_DIR}/bass/lib/windows/x86_64/bass.dll"
    "${BASS_CACHE_DIR}/bassfx/lib/windows/x86_64/bass_fx.dll"
    "${BASS_CACHE_DIR}/bassmix/lib/windows/x86_64/bassmix.dll"
    "${BASS_CACHE_DIR}/bassloud/lib/windows/x86_64/bassloud.dll"
    "${BASS_CACHE_DIR}/bassasio/lib/windows/x86_64/bassasio.dll"
    "${BASS_CACHE_DIR}/basswasapi/lib/windows/x86_64/basswasapi.dll"
    "${BASS_CACHE_DIR}/bassflac/lib/windows/x86_64/bassflac.dll"
    DESTINATION bin
)

# Install Discord SDK DLL
install(FILES
    "${DEPS_INSTALL_DIR}/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x86_64,x86>/discord_game_sdk.dll"
    DESTINATION bin
)

install(DIRECTORY DESTINATION bin/screenshots)

message(STATUS "neosu Main Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Dependencies: ${DEPS_INSTALL_DIR}")
message(STATUS "  BASS Cache: ${BASS_CACHE_DIR}")
