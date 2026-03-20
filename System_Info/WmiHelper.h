#pragma once
#include <wbemidl.h>
#include <comdef.h>
#include <iostream>
#include <vector>
#include <functional>

class WmiHelper {
public:
    template<typename ObjectType>
    static void ExecuteWmiQuery(
        IWbemServices* svcs,
        const std::wstring& query,
        std::function<void(IWbemClassObject*, ObjectType&)> processObject
    )
    {
        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = svcs->ExecQuery(
            bstr_t(L"WQL"),
            bstr_t(query.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &enumerator
        );

        if (FAILED(hr)) {
            std::wcerr << L"WMI ExecQuery failed 0x" << std::hex << hr
                << L" for query: " << query << std::endl;
            return;
        }

        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (enumerator) {
            hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &ret);
            if (ret == 0) break;

            ObjectType instance;
            processObject(obj, instance);

            obj->Release();
        }
        enumerator->Release();
    }
};

