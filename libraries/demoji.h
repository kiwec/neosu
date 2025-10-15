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

#ifndef DEMOJI_H
#define DEMOJI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Convert mojibake string by treating UTF-8 as CP850, then as SHIFT-JIS.
	 *
	 * @param input Input bytes
	 * @param input_len Input length in bytes
	 * @param output Output buffer (should be at least input_len * 4 bytes)
	 * @param output_len Size of output buffer
	 * @return Number of bytes written to output, or negative on error:
	 *         -1: Library initialization failed (future attempts will also fail)
	 *         -2: Conversion failed
	 *         -3: Output buffer too small
	 */
	ptrdiff_t demoji_fwd(const char *input, size_t input_len, char *output, size_t output_len);

	/**
	 * Reverse operation: convert UTF-8 to SHIFT-JIS, then treat as CP850.
	 *
	 * @param input Input bytes
	 * @param input_len Input length in bytes
	 * @param output Output buffer (should be at least input_len * 4 bytes)
	 * @param output_len Size of output buffer
	 * @return Number of bytes written to output, or negative on error:
	 *         -1: Library initialization failed (future attempts will also fail)
	 *         -2: Conversion failed
	 *         -3: Output buffer too small
	 */
	ptrdiff_t demoji_bwd(const char *input, size_t input_len, char *output, size_t output_len);

#ifdef __cplusplus
}
#endif

#endif
