cmake_minimum_required(VERSION 3.20)
project(neosu VERSION 41.04 LANGUAGES C CXX)

# MSVC specific configuration
if(NOT MSVC)
    message(FATAL_ERROR "This CMake configuration is specifically for MSVC")
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
set(MCENGINE_FEATURE_OPENGL ON)
set(MCENGINE_FEATURE_BASS ON)
set(MCENGINE_FEATURE_SOLOUD ON)

# Generate config.h
configure_file(
    "${CMAKE_SOURCE_DIR}/../config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
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

# Find packages that provide config files
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)

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

# Create main executable
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

# Apply compiler options
target_compile_options(neosu PRIVATE
    "/utf-8"
    "/Zc:preprocessor"
    "/GR"
    "/MP"  # multi-processor compilation
    "/D_UNICODE"
    "/DUNICODE=1"
    $<$<CONFIG:Debug>:/Zi;/Od;/RTC1>
    $<$<CONFIG:Release>:/O2;/Oi;/Oy;/Gw;/GL;/GS-;/Gy>
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
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

# Link options
target_link_options(neosu PRIVATE
    $<$<CONFIG:Debug>:/DEBUG:FULL;/SUBSYSTEM:CONSOLE;/ENTRY:wmainCRTStartup;/PROFILE>
    $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS;/ENTRY:wWinMainCRTStartup;/LTCG;/OPT:REF;/OPT:ICF>
    ${LIBARCHIVE_STATIC_LDFLAGS}
)

target_link_directories(neosu PRIVATE
    ${DEPS_INSTALL_DIR}/lib
    ${BZIP2_STATIC_LIBRARY_DIRS}
    ${LIBARCHIVE_STATIC_LIBRARY_DIRS}
)

# binary resources
target_binary_resources(neosu PRIVATE
    cursortrail_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/cursortrail_v.glsl"
    cursortrail_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/cursortrail_f.glsl"
    slider_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/slider_v.glsl"
    slider_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/slider_f.glsl"
    flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/flashlight_v.glsl"
    flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/flashlight_f.glsl"
    actual_flashlight_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/actual_flashlight_v.glsl"
    actual_flashlight_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/actual_flashlight_f.glsl"
    smoothclip_vsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/smoothclip_v.glsl"
    smoothclip_fsh "${CMAKE_SOURCE_DIR}/../../assets/shaders/smoothclip_f.glsl"
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
    spdlog::spdlog
    LibLZMA::LibLZMA
    CURL::libcurl
    SoundTouch::SoundTouch
    SoLoud::soloud
    MPG123::libmpg123
    ${DEPS_INSTALL_DIR}/lib/bz2_static.lib # cool awesome
    ${LIBARCHIVE_STATIC_LIBRARIES}
    comctl32
)

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
