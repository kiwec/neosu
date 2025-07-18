cmake_minimum_required(VERSION 3.20)
project(neosu-superbuild)

# MSVC specific configuration
if(NOT MSVC)
    message(FATAL_ERROR "This CMake configuration is specifically for MSVC")
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Use static runtime libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Parallel build configuration
include(ProcessorCount)
ProcessorCount(CPU_COUNT)
if(CPU_COUNT EQUAL 0)
    set(CPU_COUNT 4)
endif()

if(WINEBUILD)
    set(CMAKE_COMPAT_GENERATOR "Ninja")
else()
    set(CMAKE_COMPAT_GENERATOR "${CMAKE_GENERATOR}")
endif()

# Shared dependency cache and install dirs
set(DEPS_CACHE "${CMAKE_SOURCE_DIR}/depcache")
set(DEPS_PREFIX "${CMAKE_BINARY_DIR}/deps")
set(DEPS_INSTALL_DIR "${DEPS_PREFIX}/install")

# Create directories
file(MAKE_DIRECTORY "${DEPS_CACHE}")
file(MAKE_DIRECTORY "${DEPS_PREFIX}")
file(MAKE_DIRECTORY "${DEPS_INSTALL_DIR}")

# Load dependency information
include(${CMAKE_SOURCE_DIR}/dependencies.cmake)

# for libjpeg-turbo
find_program(NASM nasm HINTS "C:/Program Files/NASM" OPTIONAL)

include(ExternalProject)
find_package(PkgConfig REQUIRED)

# Set up pkg-config environment for finding our built dependencies
set(ENV{PKG_CONFIG_PATH} "${DEPS_INSTALL_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${DEPS_INSTALL_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_MSVC_SYNTAX} 1)
set(PKG_CONFIG_ARGN "--msvc-syntax")

# Compiler flags
# Strip default cmake exception handling flags
string(REGEX REPLACE "/EH[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(COMMON_C_FLAGS "/utf-8" "/D_HAS_EXCEPTIONS=0")
set(COMMON_CXX_FLAGS "/utf-8" "/Zc:preprocessor" "/GR" "/D_HAS_EXCEPTIONS=0" "/DSOLOUD_NO_ASSERTS")
set(DEBUG_FLAGS "/Zi" "/Od" "/RTC1")
set(RELEASE_FLAGS "/O2" "/Oi" "/Oy" "/Gw" "/GL" "/GS-" "/Gy")

# Link flags
set(DEBUG_LINK_FLAGS "/DEBUG:FULL")
set(RELEASE_LINK_FLAGS "")

# Convert flag lists to strings for external projects
string(JOIN " " COMMON_C_FLAGS_STR ${COMMON_C_FLAGS})
string(JOIN " " COMMON_CXX_FLAGS_STR ${COMMON_CXX_FLAGS})
string(JOIN " " DEBUG_FLAGS_STR ${DEBUG_FLAGS})
string(JOIN " " RELEASE_FLAGS_STR ${RELEASE_FLAGS})
string(JOIN " " DEBUG_LINK_FLAGS_STR ${DEBUG_LINK_FLAGS})
string(JOIN " " RELEASE_LINK_FLAGS_STR ${RELEASE_LINK_FLAGS})

# Common external project arguments
set(EXTERNAL_PROJECT_CMAKE_ARGS
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_SYSTEM_PROCESSOR=AMD64
    -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
    -DCMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}
    -DCMAKE_FIND_ROOT_PATH=${DEPS_INSTALL_DIR}
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
    -DCMAKE_C_FLAGS=${COMMON_C_FLAGS_STR}
    -DCMAKE_CXX_FLAGS=${COMMON_CXX_FLAGS_STR}
    -DCMAKE_C_FLAGS_DEBUG=${DEBUG_FLAGS_STR}
    -DCMAKE_CXX_FLAGS_DEBUG=${DEBUG_FLAGS_STR}
    -DCMAKE_C_FLAGS_RELEASE=${RELEASE_FLAGS_STR}
    -DCMAKE_CXX_FLAGS_RELEASE=${RELEASE_FLAGS_STR}
    -DCMAKE_EXE_LINKER_FLAGS_DEBUG=${DEBUG_LINK_FLAGS_STR}
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE=${RELEASE_LINK_FLAGS_STR}
    -DCMAKE_SHARED_LINKER_FLAGS_DEBUG=${DEBUG_LINK_FLAGS_STR}
    -DCMAKE_SHARED_LINKER_FLAGS_RELEASE=${RELEASE_LINK_FLAGS_STR}
    -DCMAKE_STATIC_LINKER_FLAGS_RELEASE=${RELEASE_LINK_FLAGS_STR}
    -DBZIP2_LIBRARIES=${DEPS_INSTALL_DIR}/lib/bz2_static.lib # bzip2's .pc file is literally just broken
)

# Build dependencies - order matters due to interdependencies
ExternalProject_Add(zlib_external
    URL ${ZLIB_URL}
    URL_HASH ${ZLIB_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DZLIB_COMPAT=ON
        -DZLIB_ENABLE_TESTS=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(bzip2_external
    URL ${BZIP2_URL}
    URL_HASH ${BZIP2_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DENABLE_STATIC_RUNTIME=ON
        -DENABLE_STATIC_LIB=ON
        -DENABLE_SHARED_LIB=OFF
        -DENABLE_LIB_ONLY=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(libjpeg_external
    URL ${LIBJPEG_URL}
    URL_HASH ${LIBJPEG_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DENABLE_STATIC=ON
        -DENABLE_SHARED=OFF
        -DWITH_JPEG8=ON
        -DWITH_SIMD=ON
        -DREQUIRE_SIMD=OFF
        -DCMAKE_ASM_NASM_FLAGS="-DPIC"
        -DCMAKE_ASM_NASM_OBJECT_FORMAT=win64
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(libpng_external
    URL ${LIBPNG_URL}
    URL_HASH ${LIBPNG_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS zlib_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DPNG_SHARED=OFF
        -DPNG_STATIC=ON
        -DPNG_TESTS=OFF
        -DPNG_TOOLS=OFF
        -DMINGW=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(freetype_external
    URL ${FREETYPE2_URL}
    URL_HASH ${FREETYPE2_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS zlib_external libpng_external bzip2_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DFT_DISABLE_HARFBUZZ=ON
        -DFT_DISABLE_BROTLI=ON
        -DFT_REQUIRE_ZLIB=ON
        -DFT_REQUIRE_PNG=ON
        -DFT_REQUIRE_BZIP2=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(fmt_external
    URL ${FMT_URL}
    URL_HASH ${FMT_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DFMT_CUDA_TEST=OFF
        -DFMT_DOC=OFF
        -DFMT_FUZZ=OFF
        -DFMT_INSTALL=ON
        -DFMT_MODULE=OFF
        -DFMT_OS=ON
        -DFMT_PEDANTIC=OFF
        -DFMT_SYSTEM_HEADERS=OFF
        -DFMT_TEST=OFF
        -DFMT_UNICODE=ON
        -DFMT_WERROR=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(glm_external
    URL ${GLM_URL}
    URL_HASH ${GLM_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DGLM_BUILD_TESTS=OFF
        -DGLM_BUILD_LIBRARY=OFF
        -DGLM_ENABLE_CXX_20=ON
        -DGLM_ENABLE_LANG_EXTENSIONS=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(sdl3_external
    URL ${SDL3_URL}
    URL_HASH ${SDL3_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS freetype_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DSDL_SHARED=OFF
        -DSDL_STATIC=ON
        -DSDL_HIDAPI_LIBUSB=OFF
        -DSDL_RPATH=OFF
        -DSDL_IBUS=OFF
        -DSDL_TEST_LIBRARY=OFF
        -DSDL_TESTS=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(lzma_external
    URL ${LZMA_URL}
    URL_HASH ${LZMA_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DBUILD_TESTING=OFF
        -DXZ_TOOL_LZMADEC=OFF
        -DXZ_TOOL_LZMAINFO=OFF
        -DXZ_TOOL_SCRIPTS=OFF
        -DXZ_TOOL_SYMLINKS=OFF
        -DXZ_TOOL_SYMLINKS_LZMA=OFF
        -DXZ_TOOL_XZ=OFF
        -DXZ_TOOL_XZDEC=OFF
        -DXZ_SANDBOX=no
        -DXZ_THREADS=yes
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(libarchive_external
    URL ${LIBARCHIVE_URL}
    URL_HASH ${LIBARCHIVE_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS lzma_external bzip2_external zlib_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DENABLE_BZ2LIB=ON
        -DENABLE_ZLIB=ON
        -DENABLE_LZMA=ON
        -DENABLE_CAT_SHARED=OFF
        -DENABLE_CAT=OFF
        -DENABLE_CPIO_SHARED=OFF
        -DENABLE_CPIO=OFF
        -DENABLE_EXPAT=OFF
        -DENABLE_ICONV=OFF
        -DENABLE_LIBB2=OFF
        -DENABLE_LZ4=OFF
        -DENABLE_LZO=OFF
        -DENABLE_MBEDTLS=OFF
        -DENABLE_NETTLE=OFF
        -DENABLE_OPENSSL=OFF
        -DENABLE_SAFESEH=OFF
        -DENABLE_TAR_SHARED=OFF
        -DENABLE_TAR=OFF
        -DENABLE_TEST=OFF
        -DENABLE_UNZIP_SHARED=OFF
        -DENABLE_UNZIP=OFF
        -DENABLE_WIN32_XMLLITE=OFF
        -DENABLE_XML2=OFF
        -DENABLE_ZSTD=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(curl_external
    URL ${CURL_URL}
    URL_HASH ${CURL_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS zlib_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DBUILD_STATIC_LIBS=ON
        -DBUILD_CURL_EXE=OFF
        -DBUILD_STATIC_CURL=OFF
        -DCURL_USE_LIBPSL=OFF
        -DCURL_USE_SCHANNEL=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(mpg123_external
    URL ${MPG123_URL}
    URL_HASH ${MPG123_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    SOURCE_SUBDIR ports/cmake
    # weird... unix makefiles just blows up in an incomprehensible way
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DBUILD_LIBOUT123=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(soundtouch_external
    URL ${SOUNDTOUCH_URL}
    URL_HASH ${SOUNDTOUCH_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DST_NO_EXCEPTION_HANDLING=1
        -DSOUNDSTRETCH=OFF
        -DSOUNDTOUCH_DLL=OFF
        -DOPENMP=OFF
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

ExternalProject_Add(soloud_external
    URL ${SOLOUD_URL}
    URL_HASH ${SOLOUD_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS sdl3_external mpg123_external
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DSOLOUD_WITH_SDL3=ON
        -DSOLOUD_WITH_MPG123=ON
        -DSOLOUD_WITH_FFMPEG=OFF # way too lazy
        -DPKG_CONFIG_ARGN="--msvc-syntax"
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
)

# Discord SDK (pre-built binary)
ExternalProject_Add(discordsdk_external
    URL ${DISCORDSDK_URL}
    URL_HASH ${DISCORDSDK_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/lib/x86
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_INSTALL_DIR}/lib/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/c/discord_game_sdk.h ${DEPS_INSTALL_DIR}/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/lib/x86/discord_game_sdk.dll ${DEPS_INSTALL_DIR}/lib/x86/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/lib/x86/discord_game_sdk.dll.lib ${DEPS_INSTALL_DIR}/lib/x86/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/lib/x86_64/discord_game_sdk.dll ${DEPS_INSTALL_DIR}/lib/x86_64/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/lib/x86_64/discord_game_sdk.dll.lib ${DEPS_INSTALL_DIR}/lib/x86_64/
)

# BASS audio libraries (pre-built binaries)
ExternalProject_Add(bass_external
    URL ${BASS_URL}
    URL_HASH ${BASS_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bass.h ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bass.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bass.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bass/lib/windows/x86_64/
)

ExternalProject_Add(bassfx_external
    URL ${BASSFX_URL}
    URL_HASH ${BASSFX_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bass_fx.h ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bass_fx.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bass_fx.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassfx/lib/windows/x86_64/
)

ExternalProject_Add(bassmix_external
    URL ${BASSMIX_URL}
    URL_HASH ${BASSMIX_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassmix.h ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassmix.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bassmix.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassmix/lib/windows/x86_64/
)

ExternalProject_Add(bassloud_external
    URL ${BASSLOUD_URL}
    URL_HASH ${BASSLOUD_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/c/bassloud.h ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassloud.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bassloud.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassloud/lib/windows/x86_64/
)

ExternalProject_Add(bassasio_external
    URL ${BASSASIO_URL}
    URL_HASH ${BASSASIO_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassasio.h ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/include/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassasio.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bassasio.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassasio/lib/windows/x86_64/
)

ExternalProject_Add(basswasapi_header_external
    URL ${BASSWASAPI_HEADER_URL}
    URL_HASH ${BASSWASAPI_HEADER_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/include
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/c/basswasapi.h ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/include/
)

ExternalProject_Add(basswasapi_external
    URL ${BASSWASAPI_URL}
    URL_HASH ${BASSWASAPI_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS basswasapi_header_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/basswasapi.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/basswasapi.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/basswasapi/lib/windows/x86_64/
)

ExternalProject_Add(bassflac_external
    URL ${BASSFLAC_URL}
    URL_HASH ${BASSFLAC_HASH}
    DOWNLOAD_DIR ${DEPS_CACHE}
    DEPENDS bass_external
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassflac/lib/windows/i686
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPS_CACHE}/bass-${BASS_VERSION}/bassflac/lib/windows/x86_64
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/bassflac.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassflac/lib/windows/i686/
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/bassflac.dll ${DEPS_CACHE}/bass-${BASS_VERSION}/bassflac/lib/windows/x86_64/
)

# Now build the main project after all dependencies are ready
ExternalProject_Add(neosu_main
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/src
    BINARY_DIR ${CMAKE_BINARY_DIR}/neosu
    DEPENDS
        zlib_external
        bzip2_external
        libjpeg_external
        libpng_external
        freetype_external
        fmt_external
        glm_external
        sdl3_external
        lzma_external
        libarchive_external
        curl_external
        mpg123_external
        soundtouch_external
        soloud_external
        discordsdk_external
        bass_external
        bassfx_external
        bassmix_external
        bassloud_external
        bassasio_external
        basswasapi_external
        bassflac_external
    CMAKE_ARGS
        -DCMAKE_SYSTEM_NAME=Windows
        -DCMAKE_SYSTEM_PROCESSOR=AMD64
        -DCMAKE_C_COMPILER=cl
        -DCMAKE_CXX_COMPILER=cl
        -DCMAKE_C_OUTPUT_EXTENSION=obj
        -DCMAKE_CXX_OUTPUT_EXTENSION=obj
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
        -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
        -DDEPS_INSTALL_DIR=${DEPS_INSTALL_DIR}
        -DDEPS_CACHE=${DEPS_CACHE}
        -DBASS_VERSION=${BASS_VERSION}
        -DCMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}
        -DCMAKE_FIND_ROOT_PATH=${DEPS_INSTALL_DIR}
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
        -DCMAKE_C_FLAGS=${COMMON_C_FLAGS_STR}
        -DCMAKE_CXX_FLAGS=${COMMON_CXX_FLAGS_STR}
        -DCMAKE_C_FLAGS_DEBUG=${DEBUG_FLAGS_STR}
        -DCMAKE_CXX_FLAGS_DEBUG=${DEBUG_FLAGS_STR}
        -DCMAKE_C_FLAGS_RELEASE=${RELEASE_FLAGS_STR}
        -DCMAKE_CXX_FLAGS_RELEASE=${RELEASE_FLAGS_STR}
        -DCMAKE_EXE_LINKER_FLAGS_DEBUG=${DEBUG_LINK_FLAGS_STR}
        -DCMAKE_EXE_LINKER_FLAGS_RELEASE=${RELEASE_LINK_FLAGS_STR}
        -DCMAKE_COLOR_DIAGNOSTICS=ON
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    BUILD_COMMAND cmake --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE} --parallel ${CPU_COUNT}
    # INSTALL_COMMAND cmake --install <BINARY_DIR> --config ${CMAKE_BUILD_TYPE}
)

# Build summary
message(STATUS "neosu Superbuild Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Parallel Jobs: ${CPU_COUNT}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Dependencies Cache: ${DEPS_CACHE}")
message(STATUS "  Dependencies Install: ${DEPS_INSTALL_DIR}")
