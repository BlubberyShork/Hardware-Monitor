#pragma once

/****************************************************************************
*                                                                           *
*			  Utility functions and Macros used in SystemInfo               *             
*                                                                           *
****************************************************************************/

#include "windows.h"
#include <comdef.h>
#include <string>
#include <unordered_map>
#include <sstream>         // For std::wstringstream
#include <iomanip>         // For std::fixed and std::setprecision
#include <wbemidl.h>
#include <iostream>

#define BINARY_UNIT_MULTIPLIER (ULONGLONG)1024

bstr_t simplifyBytesAsString(ULONGLONG sz);
bstr_t explainAvailability(USHORT av_status);
ULONGLONG VTConvertNumeric(VARIANT v);
inline std::string wideToUtf8(const wchar_t* wstr);

/*
 *	Hashing functor for bstr_t hashing
 */
struct bstrHash {
	std::size_t operator()(const bstr_t& b) const noexcept {
		std::string str((char*)b);
		return std::hash<std::string>()(str);
	}
};

/*
 *	Equality functor for bstr_t unordered_map
 */
struct bstrEqual {
	bool operator()(const bstr_t& a, const bstr_t& b) const noexcept {
		std::string str_a((char*)a);
		std::string str_b((char*)b);
		return str_a == str_b;
	}
};

struct ULONGHash {
	std::size_t operator()(ULONG val) const noexcept {
		return static_cast<std::size_t>(val);
	}
};

struct ULONGEqual {
	std::size_t operator()(ULONG val1, ULONG val2) const noexcept {
		return val1 == val2;
	}
};

//struct IWbemClassObjHash {
//	std::size_t operator()(IWbemClassObject* o) const noexcept {
//		return std::hash<void*>()((void*)o);
//	}
//};
//
//struct IWbemClassObjEqual {
//	bool operator()(IWbemClassObject* a, IWbemClassObject* b) const noexcept {
//		return a == b;
//	}
//};
