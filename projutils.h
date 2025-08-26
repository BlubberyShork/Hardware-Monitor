#pragma once

/****************************************************************************
*                                                                           *
*			  Utility functions and Macros used in SystemInfo               *             
*                                                                           *
****************************************************************************/

#include "windows.h"
#include <comdef.h>

#define BINARY_UNIT_MULTIPLIER (ULONG)1024

_bstr_t simplifyBytesAsString(ULONG sz);
_bstr_t explainAvailability(USHORT av_status);
