#include "client.h"
#include "shared_security_config.h"

#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/types_generated.h>
#include "opcua_logging.hpp"
#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>

#include <iostream>
#include <fstream>

void dumpClient(const UA_Client* client);

SystemInfoClient::SystemInfoClient(std::string_view client_name) : client_name_(client_name) {
    cfg_attrs_ = getClientConfigAttributes();
    dumpConfigAttrs(cfg_attrs_);

    UA_ClientConfig* h_cfg = client_.config().handle();
    
    static UA_Logger cli_logger = UA_Log_Stdout_withLevel(UA_LOGLEVEL_TRACE);
    cli_logger.clear = nullptr;
    h_cfg->logging = &cli_logger;
    
    opcua::throwIfBad(UA_CertificateVerification_Trustlist(
        &h_cfg->certificateVerification,
        cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
        cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
        NULL, 0)
    );
    h_cfg->certificateVerification.logging = &cli_logger;
    
    // Remove default-configured auth/cfg security policy #None
    h_cfg->securityPolicies->clear(h_cfg->securityPolicies);
    h_cfg->securityPoliciesSize = 0;

    // Set encryption policy -> Basic256Sha256, per OPC UA foundation
    opcua::throwIfBad(UA_ClientConfig_addSecurityPolicyBasic256Sha256(
        h_cfg, &cfg_attrs_.certificate, &cfg_attrs_.private_key)
    );

    UA_String_clear(&h_cfg->securityPolicyUri);

    // Config token policy
    UA_UserTokenPolicy cfg_tkn_pol = UA_UserTokenPolicy {
        .policyId = UA_STRING_ALLOC(std::string(X509_TOKEN_POLICY_ID).c_str()), 
        .tokenType = UA_USERTOKENTYPE_CERTIFICATE,
        .issuedTokenType = {},
        .issuerEndpointUrl = {},
        .securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str())
    };
    h_cfg->userTokenPolicy = cfg_tkn_pol;

    /* Create config's UserIdentityToken -> Checked against endpoint identity tokens 
     *    at runtime for validation during session activation */
    UA_X509IdentityToken* identity_tkn = UA_X509IdentityToken_new();
    /* Don't set identityToken->policyId. This is taken from the appropriate
     * endpoint at runtime. */
    UA_StatusCode retval = UA_ByteString_copy(&cfg_attrs_.certificate, &identity_tkn->certificateData);
    UA_ExtensionObject_clear(&h_cfg->userIdentityToken); // Zero-initialize
    h_cfg->userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
    h_cfg->userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_X509IDENTITYTOKEN];
    h_cfg->userIdentityToken.content.decoded.data = identity_tkn;

    // Configure auth security policies
    h_cfg->authSecurityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    h_cfg->authSecurityPoliciesSize = 1;
    
    // This field is not allocated by the default open62541 construction function called in the C++ wrapper's constructor, 
    //   so we do it for the first time here
    UA_SecurityPolicy *a_sp = static_cast<UA_SecurityPolicy *>(UA_calloc(
        h_cfg->authSecurityPoliciesSize, 
        sizeof(UA_SecurityPolicy)
    ));
    h_cfg->authSecurityPolicies = a_sp;
    opcua::throwIfBad(UA_SecurityPolicy_Basic256Sha256(
        h_cfg->authSecurityPolicies, // pointer base [0], overwriting the #None default
        cfg_attrs_.certificate,
        cfg_attrs_.private_key,
        h_cfg->logging
    ));
    
    // Configure Endpoints
    UA_EndpointDescription& ep = h_cfg->endpoint;
    client_.config().setSecurityMode(opcua::MessageSecurityMode::SignAndEncrypt);
    h_cfg->securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());

    std::string endpoint_url = "opc.tcp://" + std::string(std::getenv("SERVER_IP")) + ":4840";
    ep.endpointUrl = UA_STRING_ALLOC(endpoint_url.c_str());
    ep.securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    ep.serverCertificate = cfg_attrs_.trust_list[0];
    ep.securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT ; 

    // Endpoint token configuration
    ep.userIdentityTokensSize = 1;
    ep.userIdentityTokens = static_cast<UA_UserTokenPolicy*>(UA_Array_new(
        ep.userIdentityTokensSize, 
        &UA_TYPES[UA_TYPES_USERTOKENPOLICY]
    ));
    ep.userIdentityTokens[0].tokenType = UA_USERTOKENTYPE_CERTIFICATE;
    ep.userIdentityTokens[0].policyId = UA_STRING_ALLOC(std::string(X509_TOKEN_POLICY_ID).c_str());
    ep.userIdentityTokens[0].securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    ep.userIdentityTokens[0].issuerEndpointUrl = {};
    ep.userIdentityTokens[0].issuedTokenType = {};
    ep.transportProfileUri = UA_STRING_ALLOC(std::string(TRANSPORT_PROFILE_URI).c_str());

    UA_ApplicationDescription_clear(&h_cfg->clientDescription);
    UA_ApplicationDescription desc = configureApplicationDescription(client_name_);
    h_cfg->clientDescription = desc;

    dumpClient(client_.handle()); 
}

SystemInfoClient::~SystemInfoClient() {
    if(cfg_attrs_.trust_list)
        free(cfg_attrs_.trust_list);
}

void SystemInfoClient::connect(std::string_view endpoint_url) {
    client_.connect(endpoint_url);
}

void SystemInfoClient::disconnect() {
    client_.disconnect();
}

//// Helper Functions ////
SystemInfoClient::ClientConfigAttributes SystemInfoClient::getClientConfigAttributes() {
    namespace fs = std::filesystem;
    ClientConfigAttributes attrs;

    const fs::path proj_root   = fs::current_path().parent_path().parent_path();
    const fs::path pki_root    = proj_root / "pki";
    const fs::path ca_dir      = pki_root / "ca";
    const fs::path devices_dir = pki_root / "devices";
    const std::string client_name = std::string(client_name_);
    const std::string trusted_server_name = std::string("server");

    // Populating clients certificate and private key
    try {
        attrs.certificate = readBytesFromFile(devices_dir / client_name / (client_name + ".crt"));
        attrs.private_key = readBytesFromFile(devices_dir / client_name / (client_name + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
        throw;
    }

    // Populating clients trust list
    std::vector<opcua::ByteString> trust_list_storage{};
    fs::path serv_cert_path = devices_dir / trusted_server_name / (trusted_server_name + ".crt");
    try {
        trust_list_storage.push_back(readBytesFromFile(serv_cert_path));
    } catch (const std::runtime_error& e) {
        std::cerr << "Skipping trust list entry 'server.crt': " << e.what() << "\n";
    }

    attrs.trust_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * trust_list_storage.size());
    for(size_t i = 0; i < trust_list_storage.size(); ++i) {
        UA_ByteString_copy(trust_list_storage[i].handle(), &attrs.trust_list[i]);
    }
    attrs.trust_list_size = trust_list_storage.size(); 

    // Populating clients issuer list
    std::vector<opcua::ByteString> issuer_list_storage{};
    fs::path ca_cert_path = ca_dir / ("ca.crt"); 
    try {
        issuer_list_storage.push_back(readBytesFromFile(ca_cert_path));
    } catch (const std::runtime_error& e) {
        std::cerr << "Skipping trust list entry '" << "ca.crt" << "': " << e.what() << "\n";
    }

    attrs.issuer_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * issuer_list_storage.size());
    for(size_t i = 0; i < issuer_list_storage.size(); ++i) {
        UA_ByteString_copy(issuer_list_storage[i].handle(), &attrs.issuer_list[i]);
    }
    attrs.issuer_list_size = issuer_list_storage.size(); 

    return attrs;
}

UA_ByteString SystemInfoClient::readBytesFromFile(const std::filesystem::path& path) {
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

UA_StatusCode
SystemInfoClient::UA_ClientConfig_addSecurityPolicyBasic256Sha256(
    UA_ClientConfig *config,
    const UA_ByteString *certificate,
    const UA_ByteString *privateKey
) {
    /* Allocate the SecurityPolicies */
    UA_SecurityPolicy *tmp = (UA_SecurityPolicy *)
        UA_realloc(config->securityPolicies,
                   sizeof(UA_SecurityPolicy) * (1 + config->securityPoliciesSize));
    if(!tmp)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    config->securityPolicies = tmp;

    /* Populate the SecurityPolicies */
    UA_ByteString localCertificate = UA_BYTESTRING_NULL;
    UA_ByteString localPrivateKey  = UA_BYTESTRING_NULL;
    if(certificate)
        localCertificate = *certificate;
    if(privateKey)
       localPrivateKey = *privateKey;
    UA_StatusCode retval =
        UA_SecurityPolicy_Basic256Sha256(&config->securityPolicies[config->securityPoliciesSize],
                                         localCertificate, localPrivateKey, config->logging);
    if(retval != UA_STATUSCODE_GOOD) {
        if(config->securityPoliciesSize == 0) {
            UA_free(config->securityPolicies);
            config->securityPolicies = NULL;
        }
        return retval;
    }

    config->securityPoliciesSize++;
    return UA_STATUSCODE_GOOD;
}

UA_ApplicationDescription SystemInfoClient::configureApplicationDescription(std::string_view cli_name) {
    UA_ApplicationDescription desc = {0};
    
    std::string name(cli_name); 
    desc.applicationName.locale = UA_STRING_NULL;
    desc.applicationName.text = UA_STRING_ALLOC(name.c_str());

    std::string application_uri = "urn:myorg:telemetry:" + name;
    desc.applicationUri = UA_STRING_ALLOC(application_uri.c_str());

    desc.applicationType = UA_APPLICATIONTYPE_CLIENT; 

    return desc;
}

void SystemInfoClient::dumpByteString(const char* label, const UA_ByteString& bs) {
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

void SystemInfoClient::dumpConfigAttrs(const ClientConfigAttributes& attrs) {
    std::cout << "=== ClientConfigAttributes dump ===\n";
    dumpByteString("certificate", attrs.certificate);
    dumpByteString("private_key", attrs.private_key);

    std::cout << "  trust_list_size=" << attrs.trust_list_size
               << " trust_list_ptr=" << static_cast<void*>(attrs.trust_list) << "\n";
    for (size_t i = 0; i < attrs.trust_list_size; ++i) {
        dumpByteString(("trust_list[" + std::to_string(i) + "]").c_str(), attrs.trust_list[i]);
    }
}

static void printByteString(const UA_ByteString& bs) {
    if (bs.length == 0 || bs.data == nullptr) {
        std::cout << "<empty>";
        return;
    }

    std::ios old(nullptr);
    old.copyfmt(std::cout);

    for (size_t i = 0; i < bs.length; ++i) {
        std::cout << std::hex
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<unsigned>(bs.data[i]);
    }

    std::cout.copyfmt(old);
}

static void printString(const UA_String& s) {
    if (!s.data || s.length == 0) {
        std::cout << "<empty>";
        return;
    }

    std::cout.write(reinterpret_cast<const char*>(s.data), s.length);
}

void SystemInfoClient::dumpClient(const UA_Client* client) {
    if (!client) {
        std::cout << "Client is null\n";
        return;
    }

    const UA_ClientConfig* cfg = UA_Client_getConfig(
        const_cast<UA_Client*>(client));

    if (!cfg) {
        std::cout << "Config is null\n";
        return;
    }

    std::cout << "=============================\n";
    std::cout << "UA_ClientConfig\n";
    std::cout << "=============================\n";

    std::cout << "Timeout: " << cfg->timeout << " ms\n";
    std::cout << "SecureChannel lifetime: "
              << cfg->secureChannelLifeTime << '\n';

    std::cout << "Requested Session Timeout: "
              << cfg->requestedSessionTimeout << '\n';

    std::cout << "Connectivity Check Interval: "
              << cfg->connectivityCheckInterval << '\n';

    std::cout << "\n=== Security ===\n";

    std::cout << "Security Mode: "
              << static_cast<int>(cfg->securityMode) << '\n';

    std::cout << "Security Policy URI: ";
    printString(cfg->securityPolicyUri);
    std::cout << '\n';

    for(size_t i = 0; i < cfg->securityPoliciesSize; i++) 
        dumpByteString("certificate", cfg->securityPolicies[i].localCertificate);

    std::cout << "\n=== Event Loop ===\n";
    std::cout << "EventLoop: " << cfg->eventLoop << '\n';

    std::cout << "\n=== Logging ===\n";
    std::cout << "Logger: " << cfg->logging << '\n';

    std::cout << "=============================\n";
}
