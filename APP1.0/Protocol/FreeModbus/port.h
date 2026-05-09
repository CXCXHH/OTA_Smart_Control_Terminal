#ifndef _PORT_H
#define _PORT_H

#include <assert.h>
#include <inttypes.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"

#define	INLINE
#define PR_BEGIN_EXTERN_C           extern "C" {
#define	PR_END_EXTERN_C             }

#define ENTER_CRITICAL_SECTION( )   portENTER_CRITICAL()
#define EXIT_CRITICAL_SECTION( )    portEXIT_CRITICAL()

typedef uint8_t BOOL;

typedef unsigned char UCHAR;
typedef char CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#endif
