#ifndef __ALIGNED_ALLOC_H__
#define __ALIGNED_ALLOC_H__

#include <c11/_cdefs.h>

/*
 *  7.22.3.1 The aligned_alloc function
 *
 *  aligned_free is a temporary, non-standard workaround
 */

#if defined(_MSC_VER) || defined(__MINGW32__)
#   include <malloc.h>
#   define aligned_alloc(alignment, size) \
        _aligned_malloc((size), (alignment))
#   define aligned_free(addr) _aligned_free((addr))
#else
void* aligned_alloc(size_t, size_t);
void free(void*);
#   define aligned_free(addr) free((addr))
#endif /* defined(_MSC_VER) */

#endif /* __ALIGNED_ALLOC_H__ */
