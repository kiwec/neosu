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

set(SDL3_VERSION "f9395a766f8eb1b9428539b3c41ff2f86197b2f8")
set(SDL3_URL "https://github.com/libsdl-org/SDL/archive/${SDL3_VERSION}.tar.gz")
set(SDL3_HASH "SHA512=f5a98f78dc2b17ae0615ae6868d16bf6659b63d33778755ac9bf6868050d98cc47b7b4e38d406a14a72929d0d2fbe9ac32c28d6de8cfc67a470759027abcd3aa")
set_download_name("sdl3" "${SDL3_VERSION}" "${SDL3_URL}")

set(FREETYPE_VERSION "2.13.3")
string(REPLACE "." "-" _freetype_ver_temp "${FREETYPE_VERSION}")
set(FREETYPE_URL "https://github.com/freetype/freetype/archive/refs/tags/VER-${_freetype_ver_temp}.tar.gz")
set(FREETYPE_HASH "SHA512=fccfaa15eb79a105981bf634df34ac9ddf1c53550ec0b334903a1b21f9f8bf5eb2b3f9476e554afa112a0fca58ec85ab212d674dfd853670efec876bacbe8a53")
set_download_name("freetype" "${FREETYPE_VERSION}" "${FREETYPE_URL}")
unset(_freetype_ver_temp)

set(LIBJPEG_VERSION "3.1.3")
set(LIBJPEG_URL "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_VERSION}/libjpeg-turbo-${LIBJPEG_VERSION}.tar.gz")
set(LIBJPEG_HASH "SHA512=d3410a072044b4962c1aa08eb144b4e4b959f4f65203dfac4013b14e2fd987b9a6ee9b59f5570980fa691ddf5e9f9d3aa328a63afb487a46c2e76de722f3d693")
set_download_name("libjpeg" "${LIBJPEG_VERSION}" "${LIBJPEG_URL}")

set(LIBPNG_VERSION "1.6.54")
set(LIBPNG_URL "https://github.com/pnggroup/libpng/archive/refs/tags/v${LIBPNG_VERSION}.tar.gz")
set(LIBPNG_HASH "SHA512=f09e41c5e05714760b8dd05bd11bfe8b6b5840c277e5377a3854f5260879b69709c708f9581c07bdc2ffafda45796066ec258b67b0e96af64614b12346bedb1b")
set_download_name("libpng" "${LIBPNG_VERSION}" "${LIBPNG_URL}")

set(ZLIB_VERSION "2.3.2")
set(ZLIB_URL "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/${ZLIB_VERSION}.tar.gz")
set(ZLIB_HASH "SHA512=8781ee4bfda7cb8c8c5150c2e6a067d699580616b61af2ea4cf03cbe14c6715b31a29a20b7c3dd97254a9e487c72c5228c9cfa817ff71aa765fe7043ab136f04")
set_download_name("zlib" "${ZLIB_VERSION}" "${ZLIB_URL}")

set(BZIP2_VERSION "1ea1ac188ad4b9cb662e3f8314673c63df95a589")
set(BZIP2_URL "https://github.com/libarchive/bzip2/archive/${BZIP2_VERSION}.tar.gz")
set(BZIP2_HASH "SHA512=a1aae1e884f85a225e2a1ddf610f11dda672bc242d4e8d0cda3534efb438b3a0306ec1d130eec378d46abb48f6875687d6b20dcc18a6037a4455f531c22d50f6")
set_download_name("bzip2" "${BZIP2_VERSION}" "${BZIP2_URL}")

set(FMT_VERSION "0e078f6ed0624be8babc43bd145371d9f3a08aab")
set(FMT_URL "https://github.com/fmtlib/fmt/archive/${FMT_VERSION}.tar.gz")
set(FMT_HASH "SHA512=df87cbd340ddb6ea272fd8f7eff7f8bb7d9d0e75cb5d1fbda4cdb7f05222478c47a1e8bc72fcb58bb6810d370a71c967232f72b3db3644eba3f56f770a44abbc")
set_download_name("fmt" "${FMT_VERSION}" "${FMT_URL}")

set(SPDLOG_VERSION "1.17.0")
set(SPDLOG_URL "https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz")
set(SPDLOG_HASH "SHA512=8df117055d19ff21c9c9951881c7bdf27cc0866ea3a4aa0614b2c3939cedceab94ac9abaa63dc4312b51562b27d708cb2f014c68c603fd1c1051d3ed5c1c3087")
set_download_name("spdlog" "${SPDLOG_VERSION}" "${SPDLOG_URL}")

set(GLM_VERSION "1.0.2")
set(GLM_URL "https://github.com/g-truc/glm/archive/refs/tags/${GLM_VERSION}.tar.gz")
set(GLM_HASH "SHA512=e66e4f192f6579128198c47ed20442dda13c741f371b447722b7449200f05785e1b69386a465febf97f33b437f6eb69b3fb282e1e9eabf6261eb7b57998cd68c")
set_download_name("glm" "${GLM_VERSION}" "${GLM_URL}")

set(LZMA_VERSION "5.8.2")
set(LZMA_URL "https://github.com/tukaani-project/xz/releases/download/v${LZMA_VERSION}/xz-${LZMA_VERSION}.tar.gz")
set(LZMA_HASH "SHA512=0b808fc8407e7c50da3a7b2db05be732c2fcd41850b92c7f5647181443483848ff359e176c816ce2038c115273f51575877c14f1356417cc9d53845841acb063")
set_download_name("lzma" "${LZMA_VERSION}" "${LZMA_URL}")

set(LIBARCHIVE_VERSION "3.8.5")
set(LIBARCHIVE_URL "https://github.com/libarchive/libarchive/releases/download/v${LIBARCHIVE_VERSION}/libarchive-${LIBARCHIVE_VERSION}.tar.gz")
set(LIBARCHIVE_HASH "SHA512=81b11d433636fd19967c74e84498529d14722ed56164637380f449a1e096c20684eb9d7f3fec99ce7a7912bc5a20c06b892afd4d4167cb35476ee2c421943e4d")
set_download_name("libarchive" "${LIBARCHIVE_VERSION}" "${LIBARCHIVE_URL}")

set(MPG123_VERSION "a06133928e6518bd65314c9cea12ccb5588703e9")
set(MPG123_URL "https://github.com/madebr/mpg123/archive/${MPG123_VERSION}.tar.gz")
set(MPG123_HASH "SHA512=a1c2767c628432c4adcdbc04e2e6441ff6af3033dc5fae927b845f90eb1fb9f407e0a7e196702a51aa768d5348432abc6604746ff7a5dffb1f1d72241b7da30a")
set_download_name("mpg123" "${MPG123_VERSION}" "${MPG123_URL}")

set(SOUNDTOUCH_VERSION "2.4.0")
set(SOUNDTOUCH_URL "https://codeberg.org/soundtouch/soundtouch/archive/${SOUNDTOUCH_VERSION}.tar.gz")
set(SOUNDTOUCH_HASH "SHA512=8bd199c6363104ba6c9af1abbd3c4da3567ccda5fe3a68298917817fc9312ecb0914609afba1abd864307b0a596becf450bc7073eeec17b1de5a7c5086fbc45e")
set_download_name("soundtouch" "${SOUNDTOUCH_VERSION}" "${SOUNDTOUCH_URL}")

set(SOLOUD_VERSION "062f66a7b80d8b416e7211c4a3469ac22e4ee3e0")
set(SOLOUD_URL "https://github.com/whrvt/neoloud/archive/${SOLOUD_VERSION}.tar.gz")
set(SOLOUD_HASH "SHA512=bae0d939deb825043eae9bdf5c8b399b2d3fedd2b0076f1114b6c7d62bf506425a4a6b333d4f6ca76cc825086cf703097f638d296976cba92b1238dafecc9753")
set_download_name("soloud" "${SOLOUD_VERSION}" "${SOLOUD_URL}")

set(NSYNC_VERSION "1.30.0")
set(NSYNC_URL "https://github.com/google/nsync/archive/refs/tags/${NSYNC_VERSION}.tar.gz")
set(NSYNC_HASH "SHA512=fdcd61eb686ca6d6804d82837fcd33ddee54d6b2aeb7bc20cdff8c5bd2a75f87b724f72c7e835459a1a82ee8bed3d6da5e4c111b3bca22545c6e037f129839f2")
set_download_name("nsync" "${NSYNC_VERSION}" "${NSYNC_URL}")

set(SIMDUTF_VERSION "8.0.0")
set(SIMDUTF_URL "https://github.com/simdutf/simdutf/archive/refs/tags/v${SIMDUTF_VERSION}.tar.gz")
set(SIMDUTF_HASH "SHA512=ee30ac7b7b96dfef2ced3938b1cc8e10cd5ec5b3d35ac9679f30fbb7811bf3f930a31f5b4cf0b4002f27eba84d2e27e7c7bd910d96aa04485588188ad910361d")
set_download_name("simdutf" "${SIMDUTF_VERSION}" "${SIMDUTF_URL}")

set(CURL_VERSION "8.18.0")
string(REPLACE "." "_" _curl_ver_temp "${CURL_VERSION}")
set(CURL_URL "https://github.com/curl/curl/releases/download/curl-${_curl_ver_temp}/curl-${CURL_VERSION}.tar.gz")
set(CURL_HASH "SHA512=84f193f28369ccb7fba0d8933cfc24f5fbb282b046e7e8c2c1a0da35db8ec13d17e6407c240ce3a12cf4dccac62e5919bd98f3add77065408c6259cfe1071575")
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
