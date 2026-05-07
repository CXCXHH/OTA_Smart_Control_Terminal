#ifndef VSC_INTELLISENSE_FIX_H
#define VSC_INTELLISENSE_FIX_H

/*
 * This header is forced-included only by VSCode C/C++ IntelliSense.
 * It gives the parser the C99 fixed-width types and hides ARMCC-only
 * keywords that are valid for Keil but not for the IntelliSense parser.
 */
#ifdef __INTELLISENSE__

#include <stdint.h>

#ifndef __CC_ARM
#define __CC_ARM 1
#endif

#ifndef __arm__
#define __arm__ 1
#endif

#ifndef __asm
#define __asm
#endif

#ifndef __ASM
#define __ASM(...)
#endif

#ifndef __INLINE
#define __INLINE inline
#endif

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

#ifndef __weak
#define __weak
#endif

#ifndef __packed
#define __packed
#endif

#ifndef __align
#define __align(x)
#endif

#ifndef __irq
#define __irq
#endif

#endif /* __INTELLISENSE__ */

#endif /* VSC_INTELLISENSE_FIX_H */
