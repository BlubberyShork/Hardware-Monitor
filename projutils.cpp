#include "projutils.h"

bstr_t simplifyBytesAsString(ULONGLONG sz) {
    float sz_updating = (float) sz;
    bstr_t unit = bstr_t("");

    int divisions = 0;
    while (sz_updating > BINARY_UNIT_MULTIPLIER) {
        sz_updating = sz_updating / (float) BINARY_UNIT_MULTIPLIER;
        divisions++;
    }
    bstr_t value_as_str = bstr_t(sz_updating);

    static const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };

    if (divisions < 0 || divisions > 7) {
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

bstr_t explainAvailability(USHORT av_status) {
    bstr_t ret = bstr_t("");

    switch (av_status) {
    case 1:
        ret = bstr_t("Other");
        break;
    case 2:
        ret = bstr_t("Unknown");
        break;
    case 3:
        ret = bstr_t("Running / Full Power");
        break;
    case 4:
        ret = bstr_t("Warning");
        break;
    case 5:
        ret = bstr_t("In Test");
        break;
    case 6:
        ret = bstr_t("Not Applicable");
        break;
    case 7:
        ret = bstr_t("Power Off");
        break;
    case 8:
        ret = bstr_t("Offline");
        break;
    case 9:
        ret = bstr_t("Off Duty");
        break;
    case 10:
        ret = bstr_t("Degraded");
        break;
    case 11:
        ret = bstr_t("Not Installed");
        break;
    case 12:
        ret = bstr_t("Install Error");
        break;
    case 13:
        ret = bstr_t("Power Save State, Status Unknown");
        break;
    case 14:
        ret = bstr_t("Power Save State - Low Power Mode");
        break;
    case 15:
        ret = bstr_t("Power Save State - On Standby");
        break;
    case 16:
        ret = bstr_t("Power Cycle");
        break;
    case 17:
        ret = bstr_t("Power Save State - Warning State");
        break;
    case 18:
        ret = bstr_t("Paused");
        break;
    case 19:
        ret = bstr_t("Not Ready");
        break;
    case 20:
        ret = bstr_t("Not Configured");
        break;
    case 21:
        ret = bstr_t("Quiesced - The Device Is Quiet");
        break;
    default:
        fprintf(stderr, "Invalid Availability\n");
        break;
    }

    return ret;
}