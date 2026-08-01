#pragma once

#include <open62541/client_config_default.h>
#include <open62541pp/client.hpp>
#include <open62541/types_generated.h>

#include <filesystem>

class SystemInfoClient {
public:
    explicit SystemInfoClient(std::string_view client_name);
    ~SystemInfoClient();

    void connect(std::string_view endpoint_url);
    void disconnect();

private:
    // Container holding server configuration attributes for ServerConfig initialization
    
    typedef struct _ClientConfigAttributes {
        UA_ByteString   certificate;      // Client cert
        UA_ByteString   private_key;      // Client prv key
        
        UA_ByteString*  trust_list;       // List of whitelisted/verified server certificates
        size_t          trust_list_size;

        UA_ByteString*  issuer_list;      // List of whitelisted/verified server certificates
        size_t          issuer_list_size;

        UA_ByteString*  revocation_list;  // Blacklisted server certificates
        size_t          revocation_list_size;

    } ClientConfigAttributes;

    opcua::Client client_;
    ClientConfigAttributes cfg_attrs_;
    std::string_view client_name_;
    std::string_view server_endpoint_url_;

    // Helper funcs //
    ClientConfigAttributes getClientConfigAttributes(); 
    UA_ByteString readBytesFromFile(const std::filesystem::path& path); 
    UA_StatusCode UA_ClientConfig_addSecurityPolicyBasic256Sha256(
        UA_ClientConfig *config,
        const UA_ByteString *certificate,
        const UA_ByteString *privateKey
    );
    UA_ApplicationDescription configureApplicationDescription(std::string_view client_name);
    void dumpByteString(const char* label, const UA_ByteString& bs); 
    void dumpConfigAttrs(const ClientConfigAttributes& attrs); 
};
