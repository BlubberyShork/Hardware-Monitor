#include "CustomClient.h"
#include "shared_security_config.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

CustomClient::CustomClient(std::string_view client_name, std::filesystem::path project_root,
                            std::shared_ptr<FileLogger> logger)
    : logger_(std::move(logger)), client_name_(client_name), project_root_(std::move(project_root)) {
    cfg_attrs_ = getClientConfigAttributes();
    dumpConfigAttrs(cfg_attrs_);

    UA_ClientConfig* h_cfg = client_.config().handle();

    if (logger_) {
        client_.config().setLogger(logger_->asLogFunction());
    }

    opcua::throwIfBad(UA_CertificateVerification_Trustlist(
        &h_cfg->certificateVerification,
        cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
        cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
        NULL, 0)
    );
    h_cfg->certificateVerification.logging = h_cfg->logging;

    h_cfg->securityPolicies->clear(h_cfg->securityPolicies);
    h_cfg->securityPoliciesSize = 0;

    opcua::throwIfBad(UA_ClientConfig_addSecurityPolicyBasic256Sha256(
        h_cfg, &cfg_attrs_.certificate, &cfg_attrs_.private_key)
    );

    UA_String_clear(&h_cfg->securityPolicyUri);

    UA_UserTokenPolicy cfg_tkn_pol = UA_UserTokenPolicy {
        .policyId = UA_STRING_ALLOC(std::string(X509_TOKEN_POLICY_ID).c_str()),
        .tokenType = UA_USERTOKENTYPE_CERTIFICATE,
        .issuedTokenType = {},
        .issuerEndpointUrl = {},
        .securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str())
    };
    h_cfg->userTokenPolicy = cfg_tkn_pol;

    UA_X509IdentityToken* identity_tkn = UA_X509IdentityToken_new();
    UA_StatusCode retval = UA_ByteString_copy(&cfg_attrs_.certificate, &identity_tkn->certificateData);
    UA_ExtensionObject_clear(&h_cfg->userIdentityToken);
    h_cfg->userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
    h_cfg->userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_X509IDENTITYTOKEN];
    h_cfg->userIdentityToken.content.decoded.data = identity_tkn;

    h_cfg->authSecurityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    h_cfg->authSecurityPoliciesSize = 1;

    UA_SecurityPolicy* a_sp = static_cast<UA_SecurityPolicy*>(UA_calloc(
        h_cfg->authSecurityPoliciesSize,
        sizeof(UA_SecurityPolicy)
    ));
    h_cfg->authSecurityPolicies = a_sp;
    opcua::throwIfBad(UA_SecurityPolicy_Basic256Sha256(
        h_cfg->authSecurityPolicies,
        cfg_attrs_.certificate,
        cfg_attrs_.private_key,
        h_cfg->logging
    ));
    h_cfg->securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());

    UA_EndpointDescription& ep = h_cfg->endpoint;
    char* server_ip = nullptr;
    size_t server_ip_length = 0;
    if (_dupenv_s(&server_ip, &server_ip_length, "SERVER_IP") != 0 || server_ip == nullptr) {
        throw std::runtime_error("SERVER_IP is not set");
    }
    std::string endpoint_url = "opc.tcp://" + std::string(server_ip) + ":4840";
    std::free(server_ip);
    ep.endpointUrl = UA_STRING_ALLOC(endpoint_url.c_str());
    ep.securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    ep.serverCertificate = cfg_attrs_.trust_list[0];
    ep.transportProfileUri = UA_STRING_ALLOC(std::string(TRANSPORT_PROFILE_URI).c_str());
    ep.securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;

    if (logger_) {
        logger_->write("previous userIdentityTokensSize: " +
                        std::to_string(ep.userIdentityTokensSize));
    }
    ep.userIdentityTokensSize = 1;
    ep.userIdentityTokens = static_cast<UA_UserTokenPolicy*>(UA_Array_new(
        ep.userIdentityTokensSize,
        &UA_TYPES[UA_TYPES_USERTOKENPOLICY]
    ));
    if (!ep.userIdentityTokens)
        throw std::bad_alloc();
    ep.userIdentityTokens[0].tokenType = UA_USERTOKENTYPE_CERTIFICATE;
    ep.userIdentityTokens[0].policyId = UA_STRING_ALLOC(std::string(X509_TOKEN_POLICY_ID).c_str());
    ep.userIdentityTokens[0].securityPolicyUri = UA_STRING_ALLOC(std::string(SECURITY_POLICY_URI).c_str());
    ep.userIdentityTokens[0].issuerEndpointUrl = {};
    ep.userIdentityTokens[0].issuedTokenType = {};

    UA_ApplicationDescription_clear(&h_cfg->clientDescription);
    UA_ApplicationDescription desc = configureApplicationDescription(client_name_);
    h_cfg->clientDescription = desc;

    dumpClient(client_.handle());
}

CustomClient::~CustomClient() {
    if (cfg_attrs_.certificate.data != nullptr) {
        UA_ByteString_clear(&cfg_attrs_.certificate);
    }
    if (cfg_attrs_.private_key.data != nullptr) {
        UA_ByteString_clear(&cfg_attrs_.private_key);
    }

    if (cfg_attrs_.trust_list != nullptr) {
        for (size_t i = 0; i < cfg_attrs_.trust_list_size; ++i) {
            if (cfg_attrs_.trust_list[i].data != nullptr) {
                UA_ByteString_clear(&cfg_attrs_.trust_list[i]);
            }
        }
        free(cfg_attrs_.trust_list);
        cfg_attrs_.trust_list = nullptr;
        cfg_attrs_.trust_list_size = 0;
    }

    if (cfg_attrs_.issuer_list != nullptr) {
        for (size_t i = 0; i < cfg_attrs_.issuer_list_size; ++i) {
            if (cfg_attrs_.issuer_list[i].data != nullptr) {
                UA_ByteString_clear(&cfg_attrs_.issuer_list[i]);
            }
        }
        free(cfg_attrs_.issuer_list);
        cfg_attrs_.issuer_list = nullptr;
        cfg_attrs_.issuer_list_size = 0;
    }
}

void CustomClient::connect(std::string_view endpoint_url) {
    server_endpoint_url_ = std::string(endpoint_url);
    client_.connect(endpoint_url);
}

void CustomClient::disconnect() {
    client_.disconnect();
}

CustomClient::ClientConfigAttributes CustomClient::getClientConfigAttributes() {
    namespace fs = std::filesystem;
    ClientConfigAttributes attrs;

    const fs::path pki_root    = project_root_ / "pki";
    const fs::path ca_dir      = pki_root / "ca";
    const fs::path devices_dir = pki_root / "devices";
    const std::string client_name = client_name_;
    const std::string trusted_server_name = std::string("server");

    try {
        attrs.certificate = readBytesFromFile(devices_dir / client_name / (client_name + ".crt"));
        attrs.private_key = readBytesFromFile(devices_dir / client_name / (client_name + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
        throw;
    }

    std::vector<opcua::ByteString> trust_list_storage{};
    fs::path serv_cert_path = devices_dir / trusted_server_name / (trusted_server_name + ".crt");
    try {
        trust_list_storage.push_back(readBytesFromFile(serv_cert_path));
    } catch (const std::runtime_error& e) {
        std::cerr << "Skipping trust list entry 'server.crt': " << e.what() << "\n";
    }

    attrs.trust_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * trust_list_storage.size());
    for (size_t i = 0; i < trust_list_storage.size(); ++i) {
        UA_ByteString_copy(trust_list_storage[i].handle(), &attrs.trust_list[i]);
    }
    attrs.trust_list_size = trust_list_storage.size();

    std::vector<opcua::ByteString> issuer_list_storage{};
    fs::path ca_cert_path = ca_dir / ("ca.crt");
    try {
        issuer_list_storage.push_back(readBytesFromFile(ca_cert_path));
    } catch (const std::runtime_error& e) {
        std::cerr << "Skipping trust list entry '" << "ca.crt" << "': " << e.what() << "\n";
    }

    attrs.issuer_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * issuer_list_storage.size());
    for (size_t i = 0; i < issuer_list_storage.size(); ++i) {
        UA_ByteString_copy(issuer_list_storage[i].handle(), &attrs.issuer_list[i]);
    }
    attrs.issuer_list_size = issuer_list_storage.size();

    return attrs;
}

UA_ByteString CustomClient::readBytesFromFile(const std::filesystem::path& path) {
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
CustomClient::UA_ClientConfig_addSecurityPolicyBasic256Sha256(
    UA_ClientConfig* config,
    const UA_ByteString* certificate,
    const UA_ByteString* privateKey
) {
    UA_SecurityPolicy* tmp = (UA_SecurityPolicy*)
        UA_realloc(config->securityPolicies,
                   sizeof(UA_SecurityPolicy) * (1 + config->securityPoliciesSize));
    if (!tmp)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    config->securityPolicies = tmp;

    UA_ByteString localCertificate = UA_BYTESTRING_NULL;
    UA_ByteString localPrivateKey  = UA_BYTESTRING_NULL;
    if (certificate)
        localCertificate = *certificate;
    if (privateKey)
        localPrivateKey = *privateKey;
    UA_StatusCode retval =
        UA_SecurityPolicy_Basic256Sha256(&config->securityPolicies[config->securityPoliciesSize],
                                         localCertificate, localPrivateKey, config->logging);
    if (retval != UA_STATUSCODE_GOOD) {
        if (config->securityPoliciesSize == 0) {
            UA_free(config->securityPolicies);
            config->securityPolicies = NULL;
        }
        return retval;
    }

    config->securityPoliciesSize++;
    return UA_STATUSCODE_GOOD;
}

UA_ApplicationDescription CustomClient::configureApplicationDescription(std::string_view cli_name) {
    UA_ApplicationDescription desc = {0};

    std::string name(cli_name);
    desc.applicationName.locale = UA_STRING_NULL;
    desc.applicationName.text = UA_STRING_ALLOC(name.c_str());

    std::string application_uri = "urn:myorg:telemetry:" + name;
    desc.applicationUri = UA_STRING_ALLOC(application_uri.c_str());

    desc.applicationType = UA_APPLICATIONTYPE_CLIENT;

    return desc;
}

void CustomClient::dumpByteString(std::ostream& out, const char* label, const UA_ByteString& bs) {
    out << "  " << label << ": length=" << bs.length
        << " data=" << static_cast<const void*>(bs.data);
    if (bs.data && bs.length > 0) {
        size_t preview_len = std::min<size_t>(bs.length, 40);
        out << " preview=[";
        for (size_t i = 0; i < preview_len; ++i) {
            unsigned char c = bs.data[i];
            if (std::isprint(c)) out << c;
            else out << "\\x" << std::hex << (int)c << std::dec;
        }
        out << (bs.length > preview_len ? "..." : "") << "]";
    }
    out << "\n";
}

void CustomClient::dumpConfigAttrs(const ClientConfigAttributes& attrs) {
    if (!logger_) {
        return;
    }
    std::ostringstream oss;
    oss << "=== ClientConfigAttributes dump ===\n";
    dumpByteString(oss, "certificate", attrs.certificate);
    dumpByteString(oss, "private_key", attrs.private_key);

    oss << "  trust_list_size=" << attrs.trust_list_size
        << " trust_list_ptr=" << static_cast<void*>(attrs.trust_list) << "\n";
    for (size_t i = 0; i < attrs.trust_list_size; ++i) {
        dumpByteString(oss, ("trust_list[" + std::to_string(i) + "]").c_str(), attrs.trust_list[i]);
    }
    logger_->write(oss.str());
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

static void printString(std::ostream& out, const UA_String& s) {
    if (!s.data || s.length == 0) {
        out << "<empty>";
        return;
    }

    out.write(reinterpret_cast<const char*>(s.data), s.length);
}

void CustomClient::dumpClient(const UA_Client* client) {
    if (!logger_) {
        return;
    }

    std::ostringstream oss;

    if (!client) {
        oss << "Client is null\n";
        logger_->write(oss.str());
        return;
    }

    const UA_ClientConfig* cfg = UA_Client_getConfig(
        const_cast<UA_Client*>(client));

    if (!cfg) {
        oss << "Config is null\n";
        logger_->write(oss.str());
        return;
    }

    oss << "=============================\n";
    oss << "UA_ClientConfig\n";
    oss << "=============================\n";

    oss << "Timeout: " << cfg->timeout << " ms\n";
    oss << "SecureChannel lifetime: "
        << cfg->secureChannelLifeTime << '\n';

    oss << "Requested Session Timeout: "
        << cfg->requestedSessionTimeout << '\n';

    oss << "Connectivity Check Interval: "
        << cfg->connectivityCheckInterval << '\n';

    oss << "\n=== Security ===\n";

    oss << "Security Mode: "
        << static_cast<int>(cfg->securityMode) << '\n';

    oss << "Security Policy URI: ";
    printString(oss, cfg->securityPolicyUri);
    oss << '\n';

    for (size_t i = 0; i < cfg->securityPoliciesSize; i++)
        dumpByteString(oss, "certificate", cfg->securityPolicies[i].localCertificate);

    oss << "\n=== Event Loop ===\n";
    oss << "EventLoop: " << cfg->eventLoop << '\n';

    oss << "\n=== Logging ===\n";
    oss << "Logger: " << cfg->logging << '\n';

    oss << "=============================\n";

    logger_->write(oss.str());
}
