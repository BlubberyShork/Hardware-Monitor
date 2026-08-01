#pragma once

#include "AccessControlCustom.h"
#include <open62541pp/server.hpp>
#include <open62541/types_generated.h>

#define OPC_APPLICATION_URI

/**
 * PKI-Certificate validation-based OPC UA server
 */
class SystemInfoServer {
public:
    explicit SystemInfoServer();
    ~SystemInfoServer();

    void run();
    void stop();

private:
    // Container holding server configuration attributes for ServerConfig initialization
    typedef struct _ServerConfigAttributes {
        UA_ByteString   certificate;            // Server cert
        UA_ByteString   private_key;            // Server prv key
        
        UA_ByteString*  trust_list;             // List of whitelisted/verified client certificates
        size_t          trust_list_size;
        
        UA_ByteString*  issuer_list;            // List of whitelisted/verified certificate authorities
        size_t          issuer_list_size;
        
        UA_ByteString*  revocation_list;        // Blacklisted certificates
        size_t          revocation_list_size;

    } ServerConfigAttributes;

    opcua::Server          server_;
    ServerConfigAttributes cfg_attrs_;

    //// Helper Functions ////
    ServerConfigAttributes getServerConfigAttributes(); 
    UA_ByteString          readBytesFromFile(const std::filesystem::path& path);
    UA_ApplicationDescription configureApplicationDescription();
    
    // Debug print functions //
    void dumpByteString(const char* label, const UA_ByteString& bs);
    void dumpConfigAttrs(const SystemInfoServer::ServerConfigAttributes& attrs);
};
