#pragma once

#include <string_view>

constexpr std::string_view SECURITY_POLICY_URI =
    "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";

constexpr std::string_view X509_TOKEN_POLICY_ID =
    "open62541-certificate-policy";

constexpr std::string_view TRANSPORT_PROFILE_URI =
    "http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary";
