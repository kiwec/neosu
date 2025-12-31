// Copyright (c) 2025, WH, All rights reserved.
#pragma once

// decrease header bloat (boost is so slow to compile...)
#if defined(WANT_PDQSORT) || defined(WANT_SPINSORT) || defined(WANT_SPREADSORT)
// #include "boost/sort/flat_stable_sort/flat_stable_sort.hpp"
#ifdef WANT_PDQSORT
#include "boost/sort/pdqsort/pdqsort.hpp"
#endif
#ifdef WANT_SPINSORT
#include "boost/sort/spinsort/spinsort.hpp"
#endif
#ifdef WANT_SPREADSORT
#include "boost/sort/spreadsort/spreadsort.hpp"
#endif

namespace srt {
// https://github.com/boostorg/sort?tab=readme-ov-file#single-thread-algorithms
//
// Algorithm 			Stable 	Additional memory 	Best, average, and worst case 	Comparison method
// spreadsort 			no 		key_length 			N, N sqrt(LogN), 				Hybrid radix sort
// 													min(N logN, N key_length)
// pdqsort 				no 		Log N 				N, N LogN, N LogN 				Comparison operator
// spinsort 			yes 	N / 2 				N, N LogN, N LogN 				Comparison operator
// flat_stable_sort 	yes 	size of the data 	N, N LogN, N LogN 				Comparison operator
// 								/ 256 + 8K

// using boost::sort::flat_stable_sort;
#if defined(WANT_PDQSORT) || defined(WANT_SPINSORT)

template <class iter_t>
using compare_iter = std::less<typename std::iterator_traits<iter_t>::value_type>;

#endif

#ifdef WANT_PDQSORT
using boost::sort::pdqsort;

template <typename Range, typename Compare = compare_iter<Range>>
void pdqsort(Range& range, Compare comp = {}) {
    pdqsort(std::ranges::begin(range), std::ranges::end(range), std::move(comp));
}

#endif
#ifdef WANT_SPINSORT
using boost::sort::spinsort;

// ranges wrappers since Boost.Sort is a bit of an old library which doesn't support them natively
template <typename Range, typename Compare = compare_iter<Range>>
void spinsort(Range& range, Compare comp = {}) {
    spinsort(std::ranges::begin(range), std::ranges::end(range), std::move(comp));
}

#endif
#ifdef WANT_SPREADSORT
using boost::sort::spreadsort::spreadsort;
#endif

}  // namespace srt

#endif
