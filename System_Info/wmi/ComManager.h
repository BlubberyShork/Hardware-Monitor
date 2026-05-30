#pragma once
#include <comdef.h>
#include <iostream>

class ComManager
{
public:
	/* COM Initialization boilerplate */
	ComManager();

	/* Calls CoUninitialize */
	~ComManager();

	bool isInitialized() const { return initialized; }
private:

	bool initialized = false;

};

