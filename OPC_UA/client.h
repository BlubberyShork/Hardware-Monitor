#pragma once

#include <open62541/client_config_default.h>
#include <open62541pp/client.hpp>
#include <filesystem>

class SystemInfoClient {
public:
    SystemInfoClient();
    ~SystemInfoClient() = default;

    // TODO - connect(), etc
private:
    // Container holding server configuration attributes for ServerConfig initialization
    
    typedef struct _ClientConfigAttributes {
        opcua::ByteString                     certificate;      // Client cert
        opcua::ByteString                     private_key;      // Client prv key
        opcua::Span<const opcua::ByteString>  trust_list;       // List of whitelisted/verified server certificates
        opcua::Span<const opcua::ByteString>  revocation_list;  // Blacklisted server certificates
    } ClientConfigAttributes;

    opcua::Client client_;
    ClientConfigAttributes client_cfg_attrs_;

    // Helper funcs //
    ClientConfigAttributes getClientConfigAttributes(); 
    opcua::ByteString readBytesFromFile(const std::filesystem::path& path); 
};
