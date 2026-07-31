#include "client.h"

#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/plugin/pki_default.h>
#include <iostream>
#include <fstream>

#define ENDPOINT_URL_BASE "opc.tcp://DESKTOP-NC10ANV:4840"

SystemInfoClient::SystemInfoClient(
    std::string_view client_name
) 
    : client_name_(client_name)
{
    cfg_attrs_ = getClientConfigAttributes();
    opcua::ClientConfig client_cfg = opcua::ClientConfig();
    UA_ClientConfig* h_cfg = client_cfg.handle();

    // TODO - Change all if checks to opcua::throwIfBad(UA_StatusCode)
    opcua::ByteString throw_away{};
    opcua::throwIfBad(UA_CertificateVerification_Trustlist(
        &h_cfg->certificateVerification,
        cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
        throw_away.handle(), 0,
        cfg_attrs_.revocation_list, cfg_attrs_.revocation_list_size
    ));
    
    opcua::throwIfBad(UA_ClientConfig_addSecurityPolicyBasic256Sha256(
        h_cfg, &cfg_attrs_.certificate, &cfg_attrs_.private_key
    ));

    client_cfg.setSecurityMode(opcua::MessageSecurityMode::SignAndEncrypt);
    client_ = opcua::Client(std::move(client_cfg));
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

    const fs::path proj_root = fs::current_path().parent_path().parent_path();
    const fs::path pki_root   = proj_root / "pki";
    const fs::path devices_dir = pki_root / "devices";
    const std::string client_name = std::string(client_name_);
    const std::string trusted_server_name = std::string("server");

    try {
        attrs.certificate = readBytesFromFile(devices_dir / client_name / (client_name + ".crt"));
        attrs.private_key = readBytesFromFile(devices_dir / client_name / (client_name + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
        throw;
    }

    std::vector<opcua::ByteString> trust_list_storage{};
    fs::path cert_path = devices_dir / client_name / (client_name + ".crt"); 
    try {
        trust_list_storage.push_back(readBytesFromFile(cert_path));
    } catch (const std::runtime_error& e) {
        std::cerr << "Skipping trust list entry '" << client_name << "': " << e.what() << "\n";
    }

    attrs.trust_list = (UA_ByteString*)malloc(sizeof(UA_ByteString) * trust_list_storage.size());
    for(size_t i = 0; i < trust_list_storage.size(); ++i) {
        UA_ByteString_copy(trust_list_storage[i].handle(), &attrs.trust_list[i]);
    }
    attrs.trust_list_size = trust_list_storage.size(); 

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
    opcua::throwIfBad(
            UA_ByteString_allocBuffer(&result, static_cast<size_t>(size))
    );

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


