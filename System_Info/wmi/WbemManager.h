#pragma once

#include <comdef.h>
#include <wbemidl.h>
#include <winrt/base.h>
#include <iostream>
#include "projutils.h"

#pragma comment(lib, "wbemuuid.lib")

class WbemManager
{
public:
	WbemManager();
	~WbemManager() = default;

	// Non-copyable
	WbemManager(const WbemManager&) = delete;
	WbemManager& operator=(const WbemManager&) = delete;

	// Accessors (non-owning)
	IWbemLocator*  getW32Locator()   const { return w32_loc.get();   }
	IWbemLocator*  getMsftLocator()	 const { return w32_loc.get();   }
	IWbemServices* getW32Services()  const { return w32_svcs.get();  }
	IWbemServices* getMsftServices() const { return msft_svcs.get(); }

private:
	winrt::com_ptr<IWbemLocator>	w32_loc;
	winrt::com_ptr<IWbemLocator>	msft_loc;
	winrt::com_ptr<IWbemServices>	w32_svcs;
	winrt::com_ptr<IWbemServices>	msft_svcs;

	void connect(
		winrt::com_ptr<IWbemLocator>& locator,
		winrt::com_ptr<IWbemServices>& svcs,
		const wchar_t* ns
	);
	void connectAll();
	void setProxySecurity(IWbemServices* services);

};

