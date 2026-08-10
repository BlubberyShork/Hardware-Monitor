#pragma once

#include <string_view>

constexpr std::string_view SECURITY_POLICY_URI =
    "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";

// Open62541 auto-configures a postfix of the #SecurityPolicy through its constructor functions
// Due to the manual configuration I used, I ran into issues with this
// Add this or client session activation will fail in ua_server_binary.c in selectEndpointAndTokenPolicy()
//
// As far as I could find, this behavior is not documented (annoying)
constexpr std::string_view X509_TOKEN_POLICY_ID =
    "open62541-certificate-policy#Basic256Sha256";

constexpr std::string_view TRANSPORT_PROFILE_URI =
    "http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary";
