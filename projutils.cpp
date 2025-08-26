#include "projutils.h"

_bstr_t simplifyBytesAsString(ULONG sz) {
    ULONG sz_updating = sz;

    int divisions = 0;
    while (sz_updating > BINARY_UNIT_MULTIPLIER) {
        sz_updating = sz_updating / BINARY_UNIT_MULTIPLIER;
        divisions++;
    }
    _bstr_t value_as_str = _bstr_t(sz_updating);

    _bstr_t unit = _bstr_t("");
    switch (divisions) {
    case 0:
        unit = _bstr_t("B");
        break;
    case 1:
        unit = _bstr_t("KB");
        break;
    case 2:
        unit = _bstr_t("MB");
        break;
    case 3:
        unit = _bstr_t("GB");
        break;
    case 4:
        unit = _bstr_t("TB");
        break;
    case 5:
        unit = _bstr_t("PB");
        break;
    default:
        fprintf(stderr, "Invalid RAM size\n");
        break;
    }

    return _bstr_t(value_as_str + unit);
}

_bstr_t explainAvailability(USHORT av_status) {
    _bstr_t ret = _bstr_t("");

    switch (av_status) {
    case 1:
        ret = _bstr_t("Other");
        break;
    case 2:
        ret = _bstr_t("Unknown");
        break;
    case 3:
        ret = _bstr_t("Running / Full Power");
        break;
    case 4:
        ret = _bstr_t("Warning");
        break;
    case 5:
        ret = _bstr_t("In Test");
        break;
    case 6:
        ret = _bstr_t("Not Applicable");
        break;
    case 7:
        ret = _bstr_t("Power Off");
        break;
    case 8:
        ret = _bstr_t("Offline");
        break;
    case 9:
        ret = _bstr_t("Off Duty");
        break;
    case 10:
        ret = _bstr_t("Degraded");
        break;
    case 11:
        ret = _bstr_t("Not Installed");
        break;
    case 12:
        ret = _bstr_t("Install Error");
        break;
    case 13:
        ret = _bstr_t("Power Save State, Status Unknown");
        break;
    case 14:
        ret = _bstr_t("Power Save State - Low Power Mode");
        break;
    case 15:
        ret = _bstr_t("Power Save State - On Standby");
        break;
    case 16:
        ret = _bstr_t("Power Cycle");
        break;
    case 17:
        ret = _bstr_t("Power Save State - Warning State");
        break;
    case 18:
        ret = _bstr_t("Paused");
        break;
    case 19:
        ret = _bstr_t("Not Ready");
        break;
    case 20:
        ret = _bstr_t("Not Configured");
        break;
    case 21:
        ret = _bstr_t("Quiesced - The Device Is Quiet");
        break;
    default:
        fprintf(stderr, "Invalid Availability\n");
        break;
    }

    return ret;
}