cmake_minimum_required(VERSION 3.20)
project(neosu VERSION 40.04 LANGUAGES C CXX)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

find_program(CCACHE ccache)
if(NOT CCACHE STREQUAL "CCACHE-NOTFOUND")
    set(CMAKE_C_COMPILER_LAUNCHER ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

include(deps.cmake)

file(GLOB_RECURSE SOURCES ../src/*.cpp ../src/*.c ../libraries/*.cpp ../libraries/*.c)
add_executable(neosu ${SOURCES})

target_compile_definitions(neosu PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    FMT_USE_EXCEPTIONS=0
    _HAS_EXCEPTIONS=0
    ST_NO_EXCEPTION_HANDLING=1
    SOLOUD_NO_ASSERTS=1
    _UNICODE
    UNICODE=1
    SHADERS_INCDIR="${CMAKE_SOURCE_DIR}/../assets/shaders/"
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)

if(WIN32)
    # setcb defined in deps.cmake
    setcb(MCENGINE_PLATFORM_WINDOWS ON)
endif()

setcb(MCENGINE_FEATURE_OPENGL ON)
setcb(MCENGINE_FEATURE_BASS ON)
setcb(MCENGINE_FEATURE_SOLOUD ON)
configure_file(../cmake-win/config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

# Extract unique directories from the source files since they need to be include paths
file(GLOB_RECURSE HEADERS ../src/*.h ../libraries/*.h)
set(SRC_DIRS)
foreach(SOURCE_FILE IN LISTS SOURCES HEADERS)
    get_filename_component(DIR ${SOURCE_FILE} DIRECTORY)
    list(APPEND SRC_DIRS ${DIR})
endforeach()
list(REMOVE_DUPLICATES SRC_DIRS)

message("SRC_DIRS: ${SRC_DIRS}")

target_include_directories(neosu PRIVATE
    ${SRC_DIRS}
    ${bass_SOURCE_DIR}
    ${bassflac_SOURCE_DIR}/c
    ${bassfx_SOURCE_DIR}
    ${bassloud_SOURCE_DIR}/c
    ${bassmix_SOURCE_DIR}
    ${discord_game_sdk_SOURCE_DIR}/c
    ${CMAKE_BINARY_DIR}
    ${lzma_SOURCE_DIR}/src/liblzma/api
    ${bassasio_SOURCE_DIR}
    ${basswasapi_header_SOURCE_DIR}/c
    ${CMAKE_BINARY_DIR}/jpeg-prefix/src/jpeg/src
    /usr/include/freetype2
    /usr/include/soundtouch
)

target_link_directories(neosu PRIVATE ${CMAKE_BINARY_DIR}/jpeg-prefix/src/jpeg-build)
target_link_libraries(neosu PRIVATE
    OpenGL::GL
    OpenGL::GLU
    SDL3::SDL3
    libjpeg-turbo::turbojpeg
    PNG::PNG
    Freetype::Freetype
    fmt::fmt
    LibLZMA::LibLZMA
    CURL::libcurl
    OpenSSL::SSL
    glm::glm
    SoundTouch
    soloud
    mpg123
    BZip2::BZip2
    LibArchive::LibArchive
)

include(../cmake-win/src/binclude.cmake)
enable_language(ASM_NASM)
if(NOT EXISTS "${CMAKE_BINARY_DIR}/cacert.pem")
    file(DOWNLOAD https://curl.se/ca/cacert.pem "${CMAKE_BINARY_DIR}/cacert.pem")
endif()
target_binary_resources(neosu PRIVATE cacert_pem "${CMAKE_BINARY_DIR}/cacert.pem")

# need to copy bass libs
set(LIB_DIR ${CMAKE_BINARY_DIR}/lib)
make_directory(${LIB_DIR})
if(LINUX)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(BASS_ARCH x86_64)
        file(COPY ${discord_game_sdk_SOURCE_DIR}/lib/x86_64/discord_game_sdk.dll DESTINATION ${CMAKE_BINARY_DIR})
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|pentium4")
        set(BASS_ARCH x86)
    endif()
    file(COPY
        ${bass-linux_SOURCE_DIR}/${BASS_ARCH}/libbass.so
        ${bassfx-linux_SOURCE_DIR}/${BASS_ARCH}/libbass_fx.so
        ${bassmix-linux_SOURCE_DIR}/${BASS_ARCH}/libbassmix.so
        ${bassloud-linux_SOURCE_DIR}/libs/${BASS_ARCH}/libbassloud.so
        ${bassflac-linux_SOURCE_DIR}/libs/${BASS_ARCH}/libbassflac.so
        DESTINATION ${LIB_DIR})
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(BASS_ARCH x64)
        set(DGS_ARCH x86_64)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "X86")
        set(DGS_ARCH x86)
    endif()
    file(COPY
        ${bass_SOURCE_DIR}/${BASS_ARCH}/bass.dll
        ${basswasapi_SOURCE_DIR}/${BASS_ARCH}/basswasapi.dll
        ${bassmix_SOURCE_DIR}/${BASS_ARCH}/bassmix.dll
        ${bassloud_SOURCE_DIR}/${BASS_ARCH}/bassloud.dll
        ${bassfx_SOURCE_DIR}/${BASS_ARCH}/bass_fx.dll
        ${bassflac_SOURCE_DIR}/${BASS_ARCH}/bassflac.dll
        ${bassasio_SOURCE_DIR}/${BASS_ARCH}/bassasio.dll
        DESTINATION ${LIB_DIR})
    file(COPY ${discord_game_sdk_SOURCE_DIR}/lib/${DGS_ARCH}/discord_game_sdk.dll DESTINATION ${CMAKE_BINARY_DIR})
endif()

file(COPY ../assets/fonts ../assets/materials DESTINATION ${CMAKE_BINARY_DIR})
