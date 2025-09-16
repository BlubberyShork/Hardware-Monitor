#pragma once

#include <iostream>
#include "windows.h"
#include <wbemidl.h>
#include <comdef.h>
#include "projutils.h"

struct Motherboard {
	bstr_t description;
	BOOL hosting_board;
	BOOL powered_on;
	bstr_t product;
	bstr_t status;

	// Printing
	void outputMotherboardInfo();
};