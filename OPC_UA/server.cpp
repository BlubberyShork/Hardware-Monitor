#include "server.h"
#include "opcua_logging.hpp"

#include "open62541pp/wrapper.hpp"
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/util.h>

constexpr std::string_view TOKEN_POLICY_ID = "open62541-certificate-policy";

SystemInfoServer::SystemInfoServer() {
    cfg_attrs_ = getServerConfigAttributes();
    dumpConfigAttrs(cfg_attrs_);

    cfg_attrs_.revocation_list = NULL;
    cfg_attrs_.revocation_list_size = 0;

    UA_ServerConfig* h_cfg = server_.config().handle();
    
    static UA_Logger serv_logger = UA_Log_Stdout_withLevel(UA_LOGLEVEL_TRACE);
    serv_logger.clear = nullptr;
    h_cfg->logging = &serv_logger;
    
    h_cfg->eventLoop = UA_EventLoop_new_POSIX(&serv_logger);
    h_cfg->externalEventLoop = false;

    // Add the TCP connection manager
    std::string s1("tcp connection manager");
    UA_ConnectionManager *tcpCM =
        UA_ConnectionManager_new_POSIX_TCP(UA_STRING_ALLOC(s1.c_str()));
    h_cfg->eventLoop->registerEventSource(h_cfg->eventLoop, (UA_EventSource *)tcpCM);

    /* Add the UDP connection manager */
    std::string s2("udp connection manager");
    UA_ConnectionManager *udpCM =
        UA_ConnectionManager_new_POSIX_UDP(UA_STRING_ALLOC(s2.c_str()));
    h_cfg->eventLoop->registerEventSource(h_cfg->eventLoop, (UA_EventSource *)udpCM);

    // Setting server session PKI
    opcua::throwIfBad(UA_CertificateVerification_Trustlist(
        &h_cfg->sessionPKI,
        cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
        cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
        cfg_attrs_.revocation_list, cfg_attrs_.revocation_list_size));
    h_cfg->sessionPKI.logging = &serv_logger;

    // Setting the secure channel PKI
    opcua::throwIfBad(UA_CertificateVerification_Trustlist(
        &h_cfg->secureChannelPKI,
        cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
        cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
        cfg_attrs_.revocation_list, cfg_attrs_.revocation_list_size));
    h_cfg->sessionPKI.logging = &serv_logger;

    // Removing the #None default policy
    h_cfg->securityPolicies->clear(h_cfg->securityPolicies);
    h_cfg->securityPoliciesSize = 0;
    h_cfg->endpointsSize = 0; 
    h_cfg->endpoints = nullptr; 
    
    // Setting encryption to policy#Basic256Sha256
    opcua::throwIfBad(UA_ServerConfig_addSecurityPolicyBasic256Sha256(
        h_cfg, &cfg_attrs_.certificate, &cfg_attrs_.private_key));

    UA_ApplicationDescription_clear(&h_cfg->applicationDescription);
    UA_ApplicationDescription desc = configureApplicationDescription();
    h_cfg->applicationDescription = desc;

    server_.config().setAccessControl(std::make_unique<AccessControlCustom>());
 
    opcua::throwIfBad(UA_ServerConfig_addEndpoint(
        h_cfg, 
        UA_STRING_ALLOC("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256"), 
        UA_MESSAGESECURITYMODE_SIGNANDENCRYPT));
    
    // Endpoint configuration
    UA_EndpointDescription& ep = h_cfg->endpoints[0];
    std::string endpoint_url_prefix("opc.tcp://");
    std::string port(":4840");
    std::string endpoint_url = endpoint_url_prefix + std::getenv("SERVER_IP") + port;
    h_cfg->endpointsSize = 1;
    h_cfg->endpoints[0].endpointUrl = UA_STRING_ALLOC(endpoint_url.c_str());
    
    // Endpoint->userIdentityToken configuration
    h_cfg->endpoints[0].userIdentityTokensSize = 1;
    h_cfg->endpoints[0].userIdentityTokens = (UA_UserTokenPolicy*)UA_Array_new(
            h_cfg->endpoints[0].userIdentityTokensSize, 
            &UA_TYPES[UA_TYPES_USERTOKENPOLICY]
    );
    if(!h_cfg->endpoints[0].userIdentityTokens)
        std::cerr << "Server endpoint user identity tokens cannot be allocated\n";

    h_cfg->endpoints[0].userIdentityTokens[0].tokenType = UA_USERTOKENTYPE_CERTIFICATE;
    h_cfg->endpoints[0].userIdentityTokens[0].policyId = UA_STRING_ALLOC(std::string(TOKEN_POLICY_ID).c_str());
    h_cfg->endpoints[0].transportProfileUri = UA_String_fromChars("http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary");
}

SystemInfoServer::~SystemInfoServer() {
    if(cfg_attrs_.issuer_list) {
        free(cfg_attrs_.issuer_list);
    }

    if(cfg_attrs_.trust_list) {
        free(cfg_attrs_.trust_list);
    }

    if(cfg_attrs_.revocation_list) {
        free(cfg_attrs_.revocation_list);
    }
}

void SystemInfoServer::run() {
    server_.run();
}

void SystemInfoServer::stop() {
    server_.stop();
}

//// Helper Functions ////
SystemInfoServer::ServerConfigAttributes SystemInfoServer::getServerConfigAttributes() {
    namespace fs = std::filesystem;
    ServerConfigAttributes attrs;

    const fs::path proj_root = fs::current_path().parent_path().parent_path();
    const fs::path pkiRoot     = proj_root / "pki";
    const fs::path caDir       = pkiRoot / "ca";
    const fs::path devicesDir  = pkiRoot / "devices";
    const std::string thisDeviceName = "server";

    try {
        attrs.certificate = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".crt"));
        attrs.private_key = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
        throw;
    }

    std::vector<opcua::ByteString> issuer_list_storage{};
    try {
        issuer_list_storage.push_back(readBytesFromFile(caDir / "ca.crt"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading CA cert: " << e.what() << "\n";
        throw;
    }

    std::vector<opcua::ByteString> trust_list_storage{};
    std::error_code ec;
    for (const auto& deviceEntry : fs::directory_iterator(devicesDir, ec)) {
        if (ec) break;
        if (!deviceEntry.is_directory()) continue;
        const std::string deviceName = deviceEntry.path().filename().string();
        if (deviceName == thisDeviceName) continue;

        const fs::path certPath = deviceEntry.path() / (deviceName + ".crt");
        std::error_code fileEc;
        if (!fs::is_regular_file(certPath, fileEc) || fileEc) continue;

        try {
            trust_list_storage.push_back(readBytesFromFile(certPath));
        } catch (const std::runtime_error& e) {
            std::cerr << "Skipping trust list entry '" << deviceName << "': " << e.what() << "\n";
            continue;  // don't abort startup over one bad device cert -- log and skip
        }
    }

    attrs.trust_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * trust_list_storage.size());
    for(size_t i = 0; i < trust_list_storage.size(); ++i) {
        UA_ByteString_copy(trust_list_storage[i].handle(), &attrs.trust_list[i]);
    }
    attrs.trust_list_size = trust_list_storage.size(); 

    attrs.issuer_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * issuer_list_storage.size());
    for(size_t i = 0; i < issuer_list_storage.size(); ++i) {
        UA_ByteString_copy(issuer_list_storage[i].handle(), &attrs.issuer_list[i]);
    }
    attrs.issuer_list_size = issuer_list_storage.size(); 

    return attrs;
}

UA_ApplicationDescription SystemInfoServer::configureApplicationDescription(){
    UA_ApplicationDescription desc = {0};
   
    std::string name("server"); 
    desc.applicationName.locale = UA_STRING_NULL;
    desc.applicationName.text = UA_STRING_ALLOC(name.c_str());

    std::string application_uri = "urn:myorg:telemetry:" + name;
    desc.applicationUri = UA_STRING_ALLOC(application_uri.c_str());

    desc.applicationType = UA_APPLICATIONTYPE_SERVER;

    return desc;
}

UA_ByteString SystemInfoServer::readBytesFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open PKI file: " + path.string());
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("PKI file is empty or unreadable: " + path.string());
    }
    file.seekg(0, std::ios::beg);

    UA_ByteString result;
    opcua::throwIfBad(UA_ByteString_allocBuffer(&result, static_cast<size_t>(size)));

    if (!file.read(reinterpret_cast<char*>(result.data), size)) {
        throw std::runtime_error("Failed to read PKI file: " + path.string());
    }

    return result;   
}

void SystemInfoServer::dumpByteString(const char* label, const UA_ByteString& bs) {
    std::cout << "  " << label << ": length=" << bs.length
               << " data=" << static_cast<const void*>(bs.data);
    if (bs.data && bs.length > 0) {
        size_t preview_len = std::min<size_t>(bs.length, 40);
        std::cout << " preview=[";
        for (size_t i = 0; i < preview_len; ++i) {
            unsigned char c = bs.data[i];
            if (std::isprint(c)) std::cout << c;
            else std::cout << "\\x" << std::hex << (int)c << std::dec;
        }
        std::cout << (bs.length > preview_len ? "..." : "") << "]";
    }
    std::cout << "\n";
}

void SystemInfoServer::dumpConfigAttrs(const ServerConfigAttributes& attrs) {
    std::cout << "=== ServerConfigAttributes dump ===\n";
    dumpByteString("certificate", attrs.certificate);
    dumpByteString("private_key", attrs.private_key);

    std::cout << "  trust_list_size=" << attrs.trust_list_size
               << " trust_list_ptr=" << static_cast<void*>(attrs.trust_list) << "\n";
    for (size_t i = 0; i < attrs.trust_list_size; ++i) {
        dumpByteString(("trust_list[" + std::to_string(i) + "]").c_str(), attrs.trust_list[i]);
    }

    std::cout << "  issuer_list_size=" << attrs.issuer_list_size
               << " issuer_list_ptr=" << static_cast<void*>(attrs.issuer_list) << "\n";
    for (size_t i = 0; i < attrs.issuer_list_size; ++i) {
        dumpByteString(("issuer_list[" + std::to_string(i) + "]").c_str(), attrs.issuer_list[i]);
    }

    std::cout << "  revocation_list_size=" << attrs.revocation_list_size
               << " revocation_list_ptr=" << static_cast<void*>(attrs.revocation_list) << "\n";
    std::cout << "=== end dump ===\n";
}
