#ifndef ___CDEFS_H__
#define ___CDEFS_H__

/*
 *  Copyright (c) 2015-2021 Christoph Schreiber
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (http://www.boost.org/LICENSE_1_0.txt)
 */

#if defined(_MSC_VER) && (_MSC_VER < 1800)
#   error At least Visual Studio 2013 required!
#endif /* defined(_MSC_VER) ... */

#if defined(_MSC_VER) && !(defined(_M_X64) || defined(_M_IX86))
#   error Compiling for the ARM platform is not (yet) supported!
#endif /* defined(_MSC_VER) ... */

#if defined(__INTEL_COMPILER) && (__INTEL_COMPILER < 1600)
#   error At least Parallel Studio XE 2016 required!
#endif /* defined(__INTEL_COMPILER) ... */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#   define HAVE_STDC_VERSION_201112 1
#endif /* defined(__STDC_VERSION__) ... */

/*
 *  The /std:c11 and /std:c17 compiler switches were introduced
 *  in Visual Studio 2019 version 16.8 Preview 3 (_MSC_VER 1928).
 */
#if !defined(HAVE_STDC_VERSION_201112) \
    && !(defined(_MSC_VER) && (_MSC_VER < 1928))
#   error Please add -std=c11 (or similar) to your compile flags!
#endif /* !defined(HAVE_STDC_VERSION_201112) ... */

#if defined(HAVE_STDC_VERSION_201112) && !defined(__STDC_NO_ATOMICS__) \
    && !(defined(__INTEL_COMPILER) && defined(_MSC_VER))
#   define HAVE_STDATOMIC_H 1
#endif /* defined(HAVE_STDC_VERSION_201112) ... */

#if defined(HAVE_STDC_VERSION_201112) && !defined(__STDC_NO_THREADS__) \
    && !defined(__MINGW32__) \
    && !(defined(__INTEL_COMPILER) && defined(_MSC_VER)) \
    && !(defined(__clang__) && defined(_MSC_VER))
#   define HAVE_THREADS_H 1
#endif /* defined(HAVE_STDC_VERSION_201112) ... */

/*
 *  struct timespec and timespec_get were introduced in
 *  Visual Studio 2015 (_MSC_VER 1900). On macOS they came
 *  with macOS 10.15 Catalina (101500).
 */

#if defined(__linux__)
#   define HAVE_TIME_H 1
#elif defined(__APPLE__)
#   include <Availability.h>
#   if ((__MAC_OS_X_VERSION_MIN_REQUIRED+0) >= 101500)
#       define HAVE_TIME_H 1
#   endif /* ((__MAC_OS_X_VERSION_MIN_REQUIRED+0) >= 101500) */
#elif defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(_CRT_NO_TIME_T)
#   define HAVE_TIME_H 1
#endif /* defined(__linux__) */

#if defined(_MSC_VER)
#   if (_MSC_VER < 1900) /* Visual Studio 2015 */
#       define _ALLOW_KEYWORD_MACROS 1
#       define inline __inline
#       define ____PASTE(a, b) a ## b
#       define ___PASTE(a, b) ____PASTE(a, b)
#       define ___UNIQUE_ID(prefix) \
            ___PASTE(prefix, ___PASTE(_, __COUNTER__))
#       define _Static_assert(expr, msg) \
            typedef char ___UNIQUE_ID(__static_assert)[(expr)]
#   elif (_MSC_VER < 1928)
#       define _Static_assert(expr, msg) static_assert((expr), msg)
#   endif /* (_MSC_VER < 1900) */
#   if (_MSC_VER < 1928) /* Visual Studio 2019 (16.8) */
#       define restrict __restrict
#       define _Alignas(size) __declspec(align(size))
#       define _Alignof(type) __alignof(type)
#       define _Noreturn __declspec(noreturn)
#       define _Thread_local __declspec(thread)
#   endif /* (_MSC_VER < 1928) */
/*
 *  C4201: nonstandard extension used: nameless struct/union
 *  C4324: structure was padded due to alignment specifier
 */
#   pragma warning(disable: 4201 4324)
#endif /* defined(_MSC_VER) */

#endif /* ___CDEFS_H__ */
