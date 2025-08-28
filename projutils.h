#pragma once

/****************************************************************************
*                                                                           *
*			  Utility functions and Macros used in SystemInfo               *             
*                                                                           *
****************************************************************************/

#include "windows.h"
#include <comdef.h>

#define BINARY_UNIT_MULTIPLIER (ULONGLONG)1024

_bstr_t simplifyBytesAsString(ULONGLONG sz);
_bstr_t explainAvailability(USHORT av_status);
