#include "projutils.h"

bstr_t simplifyBytesAsString(ULONGLONG sz) {
    double sz_updating = static_cast<double>(sz);    
    bstr_t unit = bstr_t("");

    int divisions = 0;
    while (sz_updating > BINARY_UNIT_MULTIPLIER) {
        sz_updating = sz_updating / static_cast<double>(BINARY_UNIT_MULTIPLIER);
        divisions++;
    }

    static const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB" };
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(2) << sz_updating;
    bstr_t value_as_str(ss.str().c_str());

    if (divisions < 0 || divisions >= 7) {
        fprintf(stderr, "Invalid RAM size\n");
    }
    else {
        unit = bstr_t(units[divisions]);
    }

    bstr_t result = value_as_str;
    result += " ";
    result += unit;
    return result;
}

ULONGLONG VTConvertNumeric(VARIANT v) {
    switch (v.vt) {
    case VT_I1:    return static_cast<ULONGLONG>(v.cVal);
    case VT_UI1:   return static_cast<ULONGLONG>(v.bVal);
    case VT_I2:    return static_cast<ULONGLONG>(v.iVal);
    case VT_UI2:   return static_cast<ULONGLONG>(v.uiVal);
    case VT_I4:    return static_cast<ULONGLONG>(v.intVal);
    case VT_UI4:   return static_cast<ULONGLONG>(v.uintVal);
    case VT_I8:    return static_cast<ULONGLONG>(v.llVal);
    case VT_UI8:   return static_cast<ULONGLONG>(v.ullVal);
    case VT_BOOL:  return (v.boolVal == VARIANT_TRUE) ? 1ULL : 0ULL;
    case VT_BSTR:  // Used in some storage device sizings, etc
        return (v.bstrVal != nullptr) ? _wtoi64(v.bstrVal) : 0ULL;
    case VT_NULL:
    case VT_EMPTY:
        return 0ULL;
    default:
        // Fallback, attempt coercion
        VARIANT vConv;
        VariantInit(&vConv);
        if (SUCCEEDED(VariantChangeType(&vConv, const_cast<VARIANT*>(&v), 0, VT_I8))) {
            ULONGLONG result = static_cast<ULONGLONG>(vConv.llVal);
            VariantClear(&vConv);
            return result;
        }
        return 0ULL;
    }
}

bstr_t explainAvailability(USHORT av_status) {
    bstr_t ret = bstr_t("");

    switch (av_status) {
    case 1:  ret = bstr_t("Other"); break;
    case 2:  ret = bstr_t("Unknown"); break;
    case 3:  ret = bstr_t("Running / Full Power"); break;
    case 4:  ret = bstr_t("Warning"); break;
    case 5:  ret = bstr_t("In Test"); break;
    case 6:  ret = bstr_t("Not Applicable"); break;
    case 7:  ret = bstr_t("Power Off"); break;
    case 8:  ret = bstr_t("Offline"); break;
    case 9:  ret = bstr_t("Off Duty"); break;
    case 10: ret = bstr_t("Degraded"); break;
    case 11: ret = bstr_t("Not Installed"); break;
    case 12: ret = bstr_t("Install Error"); break;
    case 13: ret = bstr_t("Power Save State, Status Unknown"); break;
    case 14: ret = bstr_t("Power Save State - Low Power Mode"); break;
    case 15: ret = bstr_t("Power Save State - On Standby"); break;
    case 16: ret = bstr_t("Power Cycle"); break;
    case 17: ret = bstr_t("Power Save State - Warning State"); break;
    case 18: ret = bstr_t("Paused"); break;
    case 19: ret = bstr_t("Not Ready"); break;
    case 20: ret = bstr_t("Not Configured"); break;
    case 21: ret = bstr_t("Quiet - The Device Is Quiet"); break;
    default: fprintf(stderr, "Invalid Availability\n"); break;
    }

    return ret;
}

// TODO - Un-AI this and make it half decent
inline std::string wideToUtf8(const wchar_t* wstr)
{
    if (!wstr)
        return {};

    int size_needed = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size_needed <= 0)
        return {};

    std::string result(size_needed - 1, 0); // exclude null terminator

    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        result.data(),
        size_needed,
        nullptr,
        nullptr
    );

    return result;
}