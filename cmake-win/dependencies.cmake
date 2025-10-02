# Dependencies configuration for neosu
# This file contains version information, URLs, and hashes for all external dependencies

# helper wrapper around ExternalProject_Add to avoid repetition in the top-level CMakeLists
function(add_external_project dep_name)
    string(TOUPPER "${dep_name}" _upper_dep_name)
    ExternalProject_Add(${dep_name}_external
        URL ${${_upper_dep_name}_URL}
        URL_HASH ${${_upper_dep_name}_HASH}
        DOWNLOAD_NAME ${${_upper_dep_name}_DL_NAME}
        DOWNLOAD_DIR ${DEPS_CACHE}
        ${ARGN}
    )
endfunction()

# helper macro to set download name based on URL extension
macro(set_download_name dep_name version url)
    string(TOUPPER "${dep_name}" _upper_dep_name)
    string(REGEX MATCH "[^/]+$" _temp_filename "${url}")
    string(REGEX MATCH "\\.[^.]+(\\.gz)?$" _temp_ext "${_temp_filename}")
    set(${_upper_dep_name}_DL_NAME "${dep_name}-${version}${_temp_ext}")
endmacro()

set(SDL3_VERSION "b54c5fd79c3d078337d170944e635552107eda89")
set(SDL3_URL "https://github.com/libsdl-org/SDL/archive/${SDL3_VERSION}.tar.gz")
set(SDL3_HASH "SHA512=39a97bd933531f369d92fee9a94fe5f9ac9f63fb5005ef35a3dcd21f106383fd32f08ae376d4fab770d35f01fb2d57ab268ff14c6efc3006fb76a8cd60208cbf")
set_download_name("sdl3" "${SDL3_VERSION}" "${SDL3_URL}")

set(FREETYPE_VERSION "2.13.3")
string(REPLACE "." "-" _freetype_ver_temp "${FREETYPE_VERSION}")
set(FREETYPE_URL "https://github.com/freetype/freetype/archive/refs/tags/VER-${_freetype_ver_temp}.tar.gz")
set(FREETYPE_HASH "SHA512=fccfaa15eb79a105981bf634df34ac9ddf1c53550ec0b334903a1b21f9f8bf5eb2b3f9476e554afa112a0fca58ec85ab212d674dfd853670efec876bacbe8a53")
set_download_name("freetype" "${FREETYPE_VERSION}" "${FREETYPE_URL}")
unset(_freetype_ver_temp)

set(LIBJPEG_VERSION "3.1.2")
set(LIBJPEG_URL "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_VERSION}/libjpeg-turbo-${LIBJPEG_VERSION}.tar.gz")
set(LIBJPEG_HASH "SHA512=79271ae4ddc12e3753cc7323dc15617f1d82b2d554ef27b555712f6ab5de603323dd33747620815e3b55663a20e07b292a55172aee9f401f9fd3557145967abe")
set_download_name("libjpeg" "${LIBJPEG_VERSION}" "${LIBJPEG_URL}")

set(LIBPNG_VERSION "1.6.50")
set(LIBPNG_URL "https://github.com/pnggroup/libpng/archive/refs/tags/v${LIBPNG_VERSION}.tar.gz")
set(LIBPNG_HASH "SHA512=34c806e0dda960b480ce2f5ea13e2e55a9540f07c51948be25d312b901c431bc814f730f9322a2e3b6f88d4104a0c49bde9e616762b342d07db44e2c7fd5f2dc")
set_download_name("libpng" "${LIBPNG_VERSION}" "${LIBPNG_URL}")

set(ZLIB_VERSION "2.2.5")
set(ZLIB_URL "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/${ZLIB_VERSION}.tar.gz")
set(ZLIB_HASH "SHA512=b599ea24375d08fa098ed7c3b14548e0d9731a155a024a0904b0ae4a6d3491a69f0c0574d66b6e4af1e40f10e38b6b555d4c4b1fe3589ca83a5f97fbd92f635f")
set_download_name("zlib" "${ZLIB_VERSION}" "${ZLIB_URL}")

set(BZIP2_VERSION "1ea1ac188ad4b9cb662e3f8314673c63df95a589")
set(BZIP2_URL "https://github.com/libarchive/bzip2/archive/${BZIP2_VERSION}.tar.gz")
set(BZIP2_HASH "SHA512=a1aae1e884f85a225e2a1ddf610f11dda672bc242d4e8d0cda3534efb438b3a0306ec1d130eec378d46abb48f6875687d6b20dcc18a6037a4455f531c22d50f6")
set_download_name("bzip2" "${BZIP2_VERSION}" "${BZIP2_URL}")

set(FMT_VERSION "12.0.0")
set(FMT_URL "https://github.com/fmtlib/fmt/archive/${FMT_VERSION}.tar.gz")
set(FMT_HASH "SHA512=c4ab814c20fbad7e3f0ae169125a4988a2795631194703251481dc36b18da65c886c4faa9acd046b0a295005217b3689eb0126108a9ba5aac2ca909aae263c2f")
set_download_name("fmt" "${FMT_VERSION}" "${FMT_URL}")

set(SPDLOG_VERSION "1.15.3")
set(SPDLOG_URL "https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz")
set(SPDLOG_HASH "SHA512=21c35f4091850ea3a0cd6a24867e06e943df70d76cd5a7ec0b15a33e0e9e0cc3584ed7930e1ac6f347e7e06f0e002d0e759884eaf05310014e24ea0e0419fcc4")
set_download_name("spdlog" "${SPDLOG_VERSION}" "${SPDLOG_URL}")

set(GLM_VERSION "1.0.1")
set(GLM_URL "https://github.com/g-truc/glm/archive/refs/tags/${GLM_VERSION}.tar.gz")
set(GLM_HASH "SHA512=c6c6fa1ea7a7e97820e36ee042a78be248ae828c99c1b1111080d9bf334a5160c9993a70312351c92a867cd49907c95f9f357c8dfe2bc29946da6e83e27ba20c")
set_download_name("glm" "${GLM_VERSION}" "${GLM_URL}")

set(LZMA_VERSION "5.8.1")
set(LZMA_URL "https://github.com/tukaani-project/xz/releases/download/v${LZMA_VERSION}/xz-${LZMA_VERSION}.tar.gz")
set(LZMA_HASH "SHA512=151b2a47fdf00274c4fd71ceada8fb6c892bdac44070847ebf3259e602b97c95ee5ee88974e03d7aa821ab4f16d5c38e50dfb2baf660cf39c199878a666e19ad")
set_download_name("lzma" "${LZMA_VERSION}" "${LZMA_URL}")

set(LIBARCHIVE_VERSION "3.8.1")
set(LIBARCHIVE_URL "https://github.com/libarchive/libarchive/releases/download/v${LIBARCHIVE_VERSION}/libarchive-${LIBARCHIVE_VERSION}.tar.gz")
set(LIBARCHIVE_HASH "SHA512=0eed931378e998590ec97b191322e5c48019cce447d9dbbbcbadd3008e26fdf000a11c45cbeefa0567bd101422afc0da10248220afa280dea1a9b4f91d8ee653")
set_download_name("libarchive" "${LIBARCHIVE_VERSION}" "${LIBARCHIVE_URL}")

set(MPG123_VERSION "ec6ead5fc1102d64efbb6a04865f59454198c3fc")
set(MPG123_URL "https://github.com/madebr/mpg123/archive/${MPG123_VERSION}.tar.gz")
set(MPG123_HASH "SHA512=1ab5f881aa3653b01deb4f2a0f993d70bd8239a8e74c4caca7cc0ed72420816ce6b13f5b88a0ae5a35edf69ae4402f8583623daa6bffe271577e12bf7ea66197")
set_download_name("mpg123" "${MPG123_VERSION}" "${MPG123_URL}")

set(SOUNDTOUCH_VERSION "2.4.0")
set(SOUNDTOUCH_URL "https://codeberg.org/soundtouch/soundtouch/archive/${SOUNDTOUCH_VERSION}.tar.gz")
set(SOUNDTOUCH_HASH "SHA512=8bd199c6363104ba6c9af1abbd3c4da3567ccda5fe3a68298917817fc9312ecb0914609afba1abd864307b0a596becf450bc7073eeec17b1de5a7c5086fbc45e")
set_download_name("soundtouch" "${SOUNDTOUCH_VERSION}" "${SOUNDTOUCH_URL}")

set(SOLOUD_VERSION "22e9c9c51f51f4bf521740134673659c893d7e3c")
set(SOLOUD_URL "https://github.com/whrvt/neoloud/archive/${SOLOUD_VERSION}.tar.gz")
set(SOLOUD_HASH "SHA512=664fd767215c48bb015db822ce670b55c2fa063b1813865550ce49073483b0674156668ea65fb866763503b6307866cf2940c4941d35ee62096de696766f7c1f")
set_download_name("soloud" "${SOLOUD_VERSION}" "${SOLOUD_URL}")

set(NSYNC_VERSION "1.29.2")
set(NSYNC_URL "https://github.com/google/nsync/archive/refs/tags/${NSYNC_VERSION}.tar.gz")
set(NSYNC_HASH "SHA512=af463d768c9e4bacc5796410c6d368b8ad0cc0fcbae28ec35fbe7937e7939de1ccad97f51b4940e384b677bb8fbc9963a438f7687e002613f1669ab93e459f60")
set_download_name("nsync" "${NSYNC_VERSION}" "${NSYNC_URL}")

set(CURL_VERSION "8.16.0")
string(REPLACE "." "_" _curl_ver_temp "${CURL_VERSION}")
set(CURL_URL "https://github.com/curl/curl/releases/download/curl-${_curl_ver_temp}/curl-${CURL_VERSION}.tar.gz")
set(CURL_HASH "SHA512=7cf8378afbbbf2ace0d78342bf38fd8fe488170d9e758fd3aa1bade0a3c1f3841c2955d1434869e1ced078134436aa9a50d2fd9ac1e757dc97f9b2f465b55b50")
set_download_name("curl" "${CURL_VERSION}" "${CURL_URL}")
unset(_curl_ver_temp)

# BINARY DEPENDENCIES

set(DISCORDSDK_VERSION "2.5.6")
set(DISCORDSDK_URL "https://web.archive.org/web/20250505113314/https://dl-game-sdk.discordapp.net/${DISCORDSDK_VERSION}/discord_game_sdk.zip")
set(DISCORDSDK_HASH "SHA512=4c8f72c7bdf92bc969fb86b96ea0d835e01b9bab1a2cc27ae00bdac1b9733a1303ceadfe138c24a7609b76d61d49999a335dd596cf3f335d894702e2aa23406f")
set_download_name("discordsdk" "${DISCORDSDK_VERSION}" "${DISCORDSDK_URL}")

# BASS BINARIES
set(BASS_VERSION "20250813")

set(BASS_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bass.zip")
set(BASS_HASH "SHA512=6066f2a9097389c433b68f28d325793fcc4a3b1adf7916eb58d11cd48a8d5da16ea91de75d6f3e353bdc1cbe2cdee3eb6b1c4a035ca9ba15941ee2fb357185f3")
set_download_name("bass" "${BASS_VERSION}" "${BASS_URL}")

set(BASSFX_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bass_fx.zip")
set(BASSFX_HASH "SHA512=561572d0f6d5f108dfa11d786c664923bdb9aebc4d49a78a66f5826bcdfe102254c0308000a00cb79b6eb007938845f8625ea0ed3c4f9ff72806a48562ddd800")
set_download_name("bassfx" "${BASS_VERSION}" "${BASSFX_URL}")

set(BASSMIX_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassmix.zip")
set(BASSMIX_HASH "SHA512=e7139b71f53b30bd27f2991006781f69a5e0e415996fcd41a7122908b0245cc6e1efb82b66409b80dc4d7cb4eb0d6d445cf3eaff52fe6f7c43bbd9872ea7949b")
set_download_name("bassmix" "${BASS_VERSION}" "${BASSMIX_URL}")

set(BASSWASAPI_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/basswasapi.zip")
set(BASSWASAPI_HASH "SHA512=f5c68062936ccf60383c5dbea3dc4b9bcf52884fb745d0564bd6592e88a7336e16e5d9a63ea28177385641790d53eaa5d9edfe112e768dcfb68b82af65affddc")
set_download_name("basswasapi" "${BASS_VERSION}" "${BASSWASAPI_URL}")

set(BASSWASAPI_HEADER_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/basswasapi24-header.zip")
set(BASSWASAPI_HEADER_HASH "SHA512=de54b3961491ea832a0069af75dc1d57209b7805699d955b384bf9671a4da3615ba3ea217c596fa41616d2df4a8b2ea0f8f9d9c4e2453221541aacb0cc30dc6c")
set_download_name("basswasapi_header" "${BASS_VERSION}" "${BASSWASAPI_HEADER_URL}")

set(BASSASIO_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassasio.zip")
set(BASSASIO_HASH "SHA512=9542469b352d6a6bfd3a3292a09642639c0583963b714a780699ab0e5fa7cbf36e3c9ae8081195d6fef7daad88133cf10d0a568724edca5e8374473128da738a")
set_download_name("bassasio" "${BASS_VERSION}" "${BASSASIO_URL}")

set(BASSLOUD_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassloud.zip")
set(BASSLOUD_HASH "SHA512=8607d5d9fd07f6886ab4984cf68f6e9463b027a5766ae572e6d99f9298fafd08cc8ac4ece0c4ca4e47a532a76448d75443c9fa7ea7f1d7b84579487022cec493")
set_download_name("bassloud" "${BASS_VERSION}" "${BASSLOUD_URL}")

set(BASSFLAC_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassflac24.zip")
set(BASSFLAC_HASH "SHA512=1d912dcd342cf0ef873e743a305b5fc5f06a60c7446ff6f6e7e5f313124475526bb01c69718c401158b9803fb9935d2dd4d7f7ac1b2646f7ba4e769ef0455b29")
set_download_name("bassflac" "${BASS_VERSION}" "${BASSFLAC_URL}")
