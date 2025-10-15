/*
 * mojibake conversion utilities
 *
 * Copyright (c) 2025 William Horvath
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "demoji.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOWINRES
#define NOWINRES
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOIME
#define NOIME
#endif
#ifndef NOCRYPT
#define NOCRYPT
#endif
#ifndef NOMETAFILE
#define NOMETAFILE
#endif
#ifndef MMNOSOUND
#define MMNOSOUND
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <libloaderapi.h>

typedef HMODULE lib_handle_t;
#define lib_load_(x) LoadLibraryA(x)
#define lib_sym(x, y) (void *)GetProcAddress(x, y)
#define lib_close(x) FreeLibrary(x)
#define LIB_PREFIX ""
#define LIB_SUFFIX ".dll"

#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>

typedef void *lib_handle_t;
#define lib_load_(x) dlopen(x, RTLD_NOW)
#define lib_sym(x, y) dlsym(x, y)
#define lib_close(x) dlclose(x)
#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".so"
#endif

#define LIB_NAME(x) LIB_PREFIX #x LIB_SUFFIX

/* thread-safe initialization */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__) && !(defined(__has_include) && !__has_include(<threads.h>))
#include <threads.h>
#define ONCE_FLAG once_flag
#define ONCE_INIT ONCE_FLAG_INIT
#define CALL_ONCE(flag, func) call_once(flag, func)
#elif defined(_WIN32)
#define ONCE_FLAG INIT_ONCE
#define ONCE_INIT INIT_ONCE_STATIC_INIT
static void call_once_wrapper(ONCE_FLAG *flag, void (*func)(void))
{
	InitOnceExecuteOnce(flag, (PINIT_ONCE_FN)func, NULL, NULL);
}
#define CALL_ONCE(flag, func) call_once_wrapper(flag, func)
#elif defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#define ONCE_FLAG pthread_once_t
#define ONCE_INIT PTHREAD_ONCE_INIT
#define CALL_ONCE(flag, func) pthread_once(flag, func)
#endif

typedef void *iconv_t;

static int (*iconv_close_func)(iconv_t);
static iconv_t (*iconv_open_func)(const char *, const char *);
static size_t (*iconv_func)(iconv_t, char **, size_t *, char **, size_t *);

static lib_handle_t iconv_lib = NULL;
static int use_iconv = 0;
static int init_failed = 0;

static ONCE_FLAG init_once = ONCE_INIT;

static inline lib_handle_t lib_load(const char *libname)
{
#ifdef _WIN32
	if (!libname)
		return GetModuleHandle(NULL);
#endif
	return lib_load_(libname);
#undef lib_load_
}

static void cleanup(void)
{
	if (iconv_lib)
	{
#ifdef _WIN32
		if (iconv_lib != GetModuleHandle(NULL))
#endif
			lib_close(iconv_lib);
		iconv_lib = NULL;
	}
}

static void init_library(void)
{
	static const char *libs[] = {NULL, /* try currently loaded module's symbols first */
	                             LIB_NAME(iconv)};
	size_t i;

	for (i = 0; i < sizeof(libs) / sizeof(libs[0]); i++)
	{
		iconv_lib = lib_load(libs[i]);
		if (!iconv_lib)
			continue;

		iconv_close_func = (int (*)(iconv_t))lib_sym(iconv_lib, "iconv_close");
		iconv_open_func = (iconv_t (*)(const char *, const char *))lib_sym(iconv_lib, "iconv_open");
		iconv_func = (size_t (*)(iconv_t, char **, size_t *, char **, size_t *))lib_sym(iconv_lib, "iconv");

		if (iconv_close_func && iconv_open_func && iconv_func)
		{
			use_iconv = 1;
			atexit(cleanup);
			return;
		}

		cleanup();
	}

#ifdef _WIN32
	/* Windows API fallback doesn't need iconv */
	return;
#else
	init_failed = 1;
#endif
}

static int init(void)
{
	CALL_ONCE(&init_once, init_library);
	return !init_failed;
}

static ptrdiff_t convert_via_iconv(const char *to, const char *from, const char *input, size_t input_len, char *output, size_t output_len)
{
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t inbytesleft, outbytesleft;
	size_t result;

	cd = iconv_open_func(to, from);
	if (cd == (iconv_t)-1)
		return -2;

	inbuf = (char *)input;
	inbytesleft = input_len;
	outbuf = output;
	outbytesleft = output_len;

	result = iconv_func(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	iconv_close_func(cd);

	if (result == (size_t)-1)
		return -2;

	return (ptrdiff_t)(output_len - outbytesleft);
}

#ifdef _WIN32
static ptrdiff_t convert_via_winapi(UINT to_cp, UINT from_cp, const char *input, size_t input_len, char *output, size_t output_len)
{
	wchar_t *wide;
	int wide_len, result_len;

	wide_len = MultiByteToWideChar(from_cp, 0, input, (int)input_len, NULL, 0);
	if (wide_len <= 0)
		return -2;

	wide = (wchar_t *)malloc(wide_len * sizeof(wchar_t));
	if (!wide)
		return -2;

	if (MultiByteToWideChar(from_cp, 0, input, (int)input_len, wide, wide_len) <= 0)
	{
		free(wide);
		return -2;
	}

	result_len = WideCharToMultiByte(to_cp, 0, wide, wide_len, NULL, 0, NULL, NULL);
	if (result_len <= 0 || (size_t)result_len > output_len)
	{
		free(wide);
		return result_len <= 0 ? -2 : -3;
	}

	if (WideCharToMultiByte(to_cp, 0, wide, wide_len, output, (int)output_len, NULL, NULL) <= 0)
	{
		free(wide);
		return -2;
	}

	free(wide);
	return result_len;
}
#endif

typedef enum _EncType
{
	ENC_CP850,
	ENC_SHIFTJIS,
	ENC_UTF8,
	ENC_MAX
} EncType;

static ptrdiff_t convert_encoding(EncType to, EncType from, const char *input, size_t input_len, char *output, size_t output_len)
{
#ifdef _WIN32
	static const UINT enc_cps[ENC_MAX] = {850, 932, 65001 /* CP_UTF8 */};
#endif
	static const char *enc_strs[ENC_MAX] = {"CP850", "SHIFT-JIS", "UTF-8"};

	if (use_iconv)
		return convert_via_iconv(enc_strs[to], enc_strs[from], input, input_len, output, output_len);
#ifdef _WIN32
	return convert_via_winapi(enc_cps[to], enc_cps[from], input, input_len, output, output_len);
#else
	return -2;
#endif
}

ptrdiff_t demoji_fwd(const char *input, size_t input_len, char *output, size_t output_len)
{
	char *intermediate;
	size_t intermediate_capacity;
	ptrdiff_t intermediate_len, final_len;

	if (!init())
		return -1;

	/* UTF-8 to CP850 shouldn't expand, but give some headroom */
	intermediate_capacity = input_len * 2;
	intermediate = (char *)malloc(intermediate_capacity);
	if (!intermediate)
		return -2;

	intermediate_len = convert_encoding(ENC_CP850, ENC_UTF8, input, input_len, intermediate, intermediate_capacity);
	if (intermediate_len < 0)
	{
		free(intermediate);
		return intermediate_len;
	}

	/* SHIFT-JIS to UTF-8 can expand up to 3x */
	final_len = convert_encoding(ENC_UTF8, ENC_SHIFTJIS, intermediate, (size_t)intermediate_len, output, output_len);
	free(intermediate);
	return final_len;
}

ptrdiff_t demoji_bwd(const char *input, size_t input_len, char *output, size_t output_len)
{
	char *intermediate;
	size_t intermediate_capacity;
	ptrdiff_t intermediate_len, final_len;

	if (!init())
		return -1;

	/* UTF-8 to SHIFT-JIS typically doesn't expand much */
	intermediate_capacity = input_len * 2;
	intermediate = (char *)malloc(intermediate_capacity);
	if (!intermediate)
		return -2;

	intermediate_len = convert_encoding(ENC_SHIFTJIS, ENC_UTF8, input, input_len, intermediate, intermediate_capacity);
	if (intermediate_len < 0)
	{
		free(intermediate);
		return intermediate_len;
	}

	/* CP850 to UTF-8 can expand */
	final_len = convert_encoding(ENC_UTF8, ENC_CP850, intermediate, (size_t)intermediate_len, output, output_len);
	free(intermediate);
	return final_len;
}
