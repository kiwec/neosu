# Dependencies configuration for neosu
# This file contains version information, URLs, and hashes for all external dependencies

set(SDL3_VERSION "277f91c3176c88193648760a651f8dd6407035dc")
set(SDL3_URL "https://github.com/libsdl-org/SDL/archive/${SDL3_VERSION}.tar.gz")
set(SDL3_HASH "SHA512=454e21fb61fcedc99d93c52ba544fc319e836d98f8c9bfdc904209d99201394d7c5320e1330e912945e412fb5b067de9d639bf9920d33a1219ceb95020e1b8b3")

set(FREETYPE2_VERSION "2.13.3")
string(REPLACE "." "-" _freetype_ver_temp "${FREETYPE2_VERSION}")
set(FREETYPE2_URL "https://github.com/freetype/freetype/archive/refs/tags/VER-${_freetype_ver_temp}.tar.gz")
set(FREETYPE2_HASH "SHA512=fccfaa15eb79a105981bf634df34ac9ddf1c53550ec0b334903a1b21f9f8bf5eb2b3f9476e554afa112a0fca58ec85ab212d674dfd853670efec876bacbe8a53")
unset(_freetype_ver_temp)

set(LIBJPEG_VERSION "3.1.0")
set(LIBJPEG_URL "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_VERSION}/libjpeg-turbo-${LIBJPEG_VERSION}.tar.gz")
set(LIBJPEG_HASH "SHA512=63231854529f59a95814cf2414fe384360a4ed96c572764fabc535c3770c559d26943e129c905a63d5949088fa79138b1ffdb175b817f2f4d36329a3cbb23ea7")

set(LIBPNG_VERSION "1.6.48")
set(LIBPNG_URL "https://github.com/pnggroup/libpng/archive/refs/tags/v${LIBPNG_VERSION}.tar.gz")
set(LIBPNG_HASH "SHA512=3a0006256abc6f23f5be1d67b201303ceaaa58ffa901f4659ad95f025b08f5e1c30d374cc251196a2ff1ee3ef4b37bd4d61c7779eabd86922d3bdd047264d9c1")

set(ZLIB_VERSION "2.2.4")
set(ZLIB_URL "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/${ZLIB_VERSION}.tar.gz")
set(ZLIB_HASH "SHA512=f49a89497988db55a2f2375f79443e7e2c57470dbd94b35ae38a39d988eb42f8ecc295a1bd68845fc273b59ea508d0e74b142585d85b7e869dd3c01cc6923d8d")

set(BZIP2_VERSION "1ea1ac188ad4b9cb662e3f8314673c63df95a589")
set(BZIP2_URL "https://github.com/libarchive/bzip2/archive/${BZIP2_VERSION}.tar.gz")
set(BZIP2_HASH "SHA512=a1aae1e884f85a225e2a1ddf610f11dda672bc242d4e8d0cda3534efb438b3a0306ec1d130eec378d46abb48f6875687d6b20dcc18a6037a4455f531c22d50f6")

set(FMT_VERSION "553ec11ec06fbe0beebfbb45f9dc3c9eabd83d28")
set(FMT_URL "https://github.com/fmtlib/fmt/archive/${FMT_VERSION}.tar.gz")
set(FMT_HASH "SHA512=6ef962d1509395a5f0fedc645c2d1a48f748debec5f2a2be7004e4c5d99610471b0b133a9656d25f875b4fba676c3a5076347a46c900dd78bb8bc9d3279553a1")

set(GLM_VERSION "1.0.1")
set(GLM_URL "https://github.com/g-truc/glm/archive/refs/tags/${GLM_VERSION}.tar.gz")
set(GLM_HASH "SHA512=c6c6fa1ea7a7e97820e36ee042a78be248ae828c99c1b1111080d9bf334a5160c9993a70312351c92a867cd49907c95f9f357c8dfe2bc29946da6e83e27ba20c")

set(LZMA_VERSION "5.8.1")
set(LZMA_URL "https://github.com/tukaani-project/xz/releases/download/v${LZMA_VERSION}/xz-${LZMA_VERSION}.tar.gz")
set(LZMA_HASH "SHA512=151b2a47fdf00274c4fd71ceada8fb6c892bdac44070847ebf3259e602b97c95ee5ee88974e03d7aa821ab4f16d5c38e50dfb2baf660cf39c199878a666e19ad")

set(LIBARCHIVE_VERSION "3.8.1")
set(LIBARCHIVE_URL "https://github.com/libarchive/libarchive/releases/download/v${LIBARCHIVE_VERSION}/libarchive-${LIBARCHIVE_VERSION}.tar.gz")
set(LIBARCHIVE_HASH "SHA512=0eed931378e998590ec97b191322e5c48019cce447d9dbbbcbadd3008e26fdf000a11c45cbeefa0567bd101422afc0da10248220afa280dea1a9b4f91d8ee653")

set(MPG123_VERSION "ec6ead5fc1102d64efbb6a04865f59454198c3fc")
set(MPG123_URL "https://github.com/madebr/mpg123/archive/${MPG123_VERSION}.tar.gz")
set(MPG123_HASH "SHA512=1ab5f881aa3653b01deb4f2a0f993d70bd8239a8e74c4caca7cc0ed72420816ce6b13f5b88a0ae5a35edf69ae4402f8583623daa6bffe271577e12bf7ea66197")

set(SOUNDTOUCH_VERSION "2.4.0")
set(SOUNDTOUCH_URL "https://codeberg.org/soundtouch/soundtouch/archive/${SOUNDTOUCH_VERSION}.tar.gz")
set(SOUNDTOUCH_HASH "SHA512=8bd199c6363104ba6c9af1abbd3c4da3567ccda5fe3a68298917817fc9312ecb0914609afba1abd864307b0a596becf450bc7073eeec17b1de5a7c5086fbc45e")

set(SOLOUD_VERSION "1.1.4")
set(SOLOUD_URL "https://github.com/whrvt/neoloud/archive/${SOLOUD_VERSION}.tar.gz")
set(SOLOUD_HASH "SHA512=2fd8319aedda0f7080b4e97125d8ff21637a1520ca31ed6c7e1068071c300dfce1e10740f5717d588250b9c4255ad7a9187201ea99b2e3895579bf7af7d87a76")

set(CURL_VERSION "8.15.0")
string(REPLACE "." "_" _curl_ver_temp "${CURL_VERSION}")
set(CURL_URL "https://github.com/curl/curl/releases/download/curl-${_curl_ver_temp}/curl-${CURL_VERSION}.tar.gz")
set(CURL_HASH "SHA512=382b8480e3cc89170370334c03f8b72a64e29630f8ee8d4390b01f99f63c5519fda53dc4103eb774051c8c5a673c177c6afa3c179d60de7a6a76ff1de37706ac")
unset(_curl_ver_temp)

# BINARY DEPENDENCIES

set(DISCORDSDK_VERSION "2.5.6")
set(DISCORDSDK_URL "https://web.archive.org/web/20250505113314/https://dl-game-sdk.discordapp.net/${DISCORDSDK_VERSION}/discord_game_sdk.zip")
set(DISCORDSDK_HASH "SHA512=4c8f72c7bdf92bc969fb86b96ea0d835e01b9bab1a2cc27ae00bdac1b9733a1303ceadfe138c24a7609b76d61d49999a335dd596cf3f335d894702e2aa23406f")

# BASS BINARIES
set(BASS_VERSION "20250721")

set(BASS_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bass.zip")
set(BASS_HASH "SHA512=d0fa29a34fa55e704a24162b23979d9ab8b75b89c11fe63d2bb71302786f8127df8017202c868e5e8b7efc8472c5144ab3a7b0591fe4b4dbc495224a7c5c9071")

set(BASSFX_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bass_fx.zip")
set(BASSFX_HASH "SHA512=561572d0f6d5f108dfa11d786c664923bdb9aebc4d49a78a66f5826bcdfe102254c0308000a00cb79b6eb007938845f8625ea0ed3c4f9ff72806a48562ddd800")

set(BASSMIX_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassmix.zip")
set(BASSMIX_HASH "SHA512=e7139b71f53b30bd27f2991006781f69a5e0e415996fcd41a7122908b0245cc6e1efb82b66409b80dc4d7cb4eb0d6d445cf3eaff52fe6f7c43bbd9872ea7949b")

set(BASSWASAPI_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/basswasapi.zip")
set(BASSWASAPI_HASH "SHA512=f5c68062936ccf60383c5dbea3dc4b9bcf52884fb745d0564bd6592e88a7336e16e5d9a63ea28177385641790d53eaa5d9edfe112e768dcfb68b82af65affddc")

set(BASSWASAPI_HEADER_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/basswasapi24-header.zip")
set(BASSWASAPI_HEADER_HASH "SHA512=de54b3961491ea832a0069af75dc1d57209b7805699d955b384bf9671a4da3615ba3ea217c596fa41616d2df4a8b2ea0f8f9d9c4e2453221541aacb0cc30dc6c")

set(BASSASIO_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassasio.zip")
set(BASSASIO_HASH "SHA512=9542469b352d6a6bfd3a3292a09642639c0583963b714a780699ab0e5fa7cbf36e3c9ae8081195d6fef7daad88133cf10d0a568724edca5e8374473128da738a")

set(BASSLOUD_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassloud.zip")
set(BASSLOUD_HASH "SHA512=8607d5d9fd07f6886ab4984cf68f6e9463b027a5766ae572e6d99f9298fafd08cc8ac4ece0c4ca4e47a532a76448d75443c9fa7ea7f1d7b84579487022cec493")

set(BASSFLAC_URL "https://archive.org/download/BASS-libs-${BASS_VERSION}/bassflac24.zip")
set(BASSFLAC_HASH "SHA512=1d912dcd342cf0ef873e743a305b5fc5f06a60c7446ff6f6e7e5f313124475526bb01c69718c401158b9803fb9935d2dd4d7f7ac1b2646f7ba4e769ef0455b29")
