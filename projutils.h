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
#include <wbemidl.h>

#define BINARY_UNIT_MULTIPLIER (ULONGLONG)1024

_bstr_t simplifyBytesAsString(ULONGLONG sz);
_bstr_t explainAvailability(USHORT av_status);

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
