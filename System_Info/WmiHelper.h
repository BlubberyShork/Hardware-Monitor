#pragma once
#include <wbemidl.h>
#include <comdef.h>
#include <iostream>
#include <vector>
#include <functional>

class WmiHelper {
public:
    template<typename ObjectType, typename Container>
    static void ExecuteWmiQuery(
        IWbemServices* svcs,
        const std::wstring& query,
        std::function<void(IWbemClassObject*, ObjectType&)> processObject,
        Container& outList
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
            addElement(obj, outlist);

            obj->Release();
        }
        enumerator->Release();
    }
};

namespace {
    template<typename T>
    struct is_vec : std::false_type {};

    template<typename T, typename Allocator>
    struct is_vec <std::vector<T, Allocator>> : std::true_type {};

    template<typename Object, typename Container>
    typename std::enable_if<is_vec<Container>::value, void>::type
        addElement(Object& o, Container& c) {
        c.push_back(o);
    }

    template<typename T>
    struct is_u_map : std::false_type {};

    template <typename K, typename V, typename H, typename P, typename A>
    struct is_u_map<std::unordered_map<K, V, H, P, A>> : std::true_type {};

    template<typename Object, typename Container>
    typename std::enable_if<is_u_map<Container>::value, void>::type
        addElement(Object& o, Container& c) {
        c.insert({ o.id, o });
    }
}
