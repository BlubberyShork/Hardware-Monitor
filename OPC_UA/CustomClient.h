#pragma once

#include <open62541/client_config_default.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>
#include <open62541pp/client.hpp>
#include <open62541/types_generated.h>

#include <filesystem>
#include <string>
#include <string_view>

class CustomClient {
public:
    CustomClient(const CustomClient&) = delete;
    CustomClient& operator=(const CustomClient&) = delete;
    CustomClient(CustomClient&&) = delete;
    CustomClient& operator=(CustomClient&&) = delete;

    void connect(std::string_view endpoint_url);
    void disconnect();

    opcua::Client& native() { return client_; }
    const std::string& clientName() const { return client_name_; }

protected:
    CustomClient(std::string_view client_name, std::filesystem::path project_root);
    ~CustomClient();

    struct ClientConfigAttributes {
        UA_ByteString   certificate;
        UA_ByteString   private_key;
        UA_ByteString*  trust_list;
        size_t          trust_list_size;
        UA_ByteString*  issuer_list;
        size_t          issuer_list_size;
    };

    ClientConfigAttributes getClientConfigAttributes();

    UA_ByteString readBytesFromFile(const std::filesystem::path& path);

    UA_StatusCode UA_ClientConfig_addSecurityPolicyBasic256Sha256(
        UA_ClientConfig* config,
        const UA_ByteString* certificate,
        const UA_ByteString* privateKey);

    UA_ApplicationDescription configureApplicationDescription(std::string_view client_name);

    void dumpByteString(const char* label, const UA_ByteString& bs);
    void dumpConfigAttrs(const ClientConfigAttributes& attrs);
    void dumpClient(const UA_Client* client);

    opcua::Client           client_;
    ClientConfigAttributes  cfg_attrs_;
    std::string             client_name_;
    std::filesystem::path   project_root_;
    std::string             server_endpoint_url_;
};
