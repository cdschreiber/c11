#ifndef __STDALIGN_H__
#define __STDALIGN_H__

#include <c11/_cdefs.h>

/*
 *  7.15.2 alignas, alignof
 *  7.15.3 __alignas_is_defined, __alignof_is_defined
 */

#if !defined(__alignas_is_defined)
#   define alignas _Alignas
#   define __alignas_is_defined 1
#endif /* !defined(__alignas_is_defined) */

#if !defined(__alignof_is_defined)
#   define alignof _Alignof
#   define __alignof_is_defined 1
#endif /* !defined(__alignof_is_defined) */

#endif /* __STDALIGN_H__ */
