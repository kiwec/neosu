#include "config.h"

// MSVC uses schannel, WASM uses #embed-based generated files
#if !defined(_MSC_VER) && !defined(MCENGINE_PLATFORM_WASM)

#include "curl_blob.h"

#ifndef CACERT_INCDIR
#define CACERT_INCDIR
#endif

// This file is just an SSL CA certificate bundle, which we use to make secure requests with curl
// (with CURLOPT_CAINFO_BLOB), without needing to rely on this data being found by curl/OpenSSL on the host
INCBIN_C(curl_ca_embed, CACERT_INCDIR "cacert.pem")

#endif
