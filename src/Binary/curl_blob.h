// Copyright (c) 2025, WH, All rights reserved.
#pragma once

// unneeded for MSVC because it uses schannel instead of OpenSSL
#ifndef _MSC_VER

#include "incbin.h"

// data in curl_blob.cpp
INCBIN_H(curl_ca_embed)
#endif
