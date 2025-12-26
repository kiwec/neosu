//----------------------------------------------------------------------------
/// @file noexcept_hlp.hpp
///
/// @author Copyright (c) 2025 William Horvath \n
///         Distributed under the Boost Software License, Version 1.0.\n
///         ( See accompanyingfile LICENSE_1_0.txt or copy at
///           http://www.boost.org/LICENSE_1_0.txt  )
/// @version 0.1
///
/// @remarks
//-----------------------------------------------------------------------------
#ifndef __BOOST_SORT_COMMON_NOEXCEPT_HLP_HPP
#define __BOOST_SORT_COMMON_NOEXCEPT_HLP_HPP

#define BOOST_QUOTE2(s) #s
#define BOOST_STRINGIZE2(s) BOOST_QUOTE2(s)

#define BOOST_TERMINATE(message)                                                                \
    do {                                                                                        \
        fprintf(stderr, __FILE__ ":" BOOST_STRINGIZE2(__LINE__) ": assertion failed: " message); \
        abort();                                                                                \
    } while(false);

#endif
