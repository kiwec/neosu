// Copyright (c) 2025, WH, All rights reserved.
#pragma once

// #include "boost/sort/flat_stable_sort/flat_stable_sort.hpp"
#include "boost/sort/pdqsort/pdqsort.hpp"
#include "boost/sort/spinsort/spinsort.hpp"
#include "boost/sort/spreadsort/spreadsort.hpp"

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
using boost::sort::pdqsort;
using boost::sort::spinsort;
using boost::sort::spreadsort::spreadsort;

// ranges wrappers since Boost.Sort is a bit of an old library which doesn't support them natively
template <typename Range, typename Compare = boost::sort::compare_iter<Range>>
void spinsort(Range& range, Compare comp = {}) {
    spinsort(std::ranges::begin(range), std::ranges::end(range), std::move(comp));
}

template <typename Range, typename Compare = boost::sort::compare_iter<Range>>
void pdqsort(Range& range, Compare comp = {}) {
    pdqsort(std::ranges::begin(range), std::ranges::end(range), std::move(comp));
}

}  // namespace srt