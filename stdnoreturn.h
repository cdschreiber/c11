#ifndef __STDNORETURN_H__
#define __STDNORETURN_H__

#include <c11/_cdefs.h>

#if defined(__clang__) && defined(_MSC_VER)
#   error The keyword macro noreturn conflicts with __declspec(noreturn)!
#endif /* defined(__clang__) ... */

/*
 *  7.23.1 noreturn
 */

#define noreturn _Noreturn

#endif /* __STDNORETURN_H__ */
