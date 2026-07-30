#pragma once

#include <open62541/client_config_default.h>
#include <open62541pp/client.hpp>
#include <filesystem>

class SystemInfoClient {
public:
    explicit SystemInfoClient(std::string_view client_name);
    ~SystemInfoClient() = default;

    void connect(std::string_view endpoint_url);
    void disconnect();

private:
    // Container holding server configuration attributes for ServerConfig initialization
    
    struct ClientConfigAttributes {
        opcua::ByteString               certificate;      // Client cert
        opcua::ByteString               private_key;      // Client prv key
        opcua::Span<opcua::ByteString>  trust_list;       // List of whitelisted/verified server certificates
        opcua::Span<opcua::ByteString>  revocation_list;  // Blacklisted server certificates
    };

    opcua::Client client_;
    ClientConfigAttributes client_cfg_attrs_;
    std::string_view client_name_;
    std::string_view server_endpoint_url_;

    // Helper funcs //
    ClientConfigAttributes getClientConfigAttributes(); 
    opcua::ByteString readBytesFromFile(const std::filesystem::path& path); 
};
