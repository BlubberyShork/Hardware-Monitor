#include "server.h"

#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/util.h>

constexpr uint16_t SERVER_PORT = 4840;

SystemInfoServer::SystemInfoServer() {
    cfg_attrs_ = getServerConfigAttributes();
    //dumpConfigAttrs(cfg_attrs_);

    UA_ByteString* revocation_list = NULL;
    size_t revocation_size = 0;
    cfg_attrs_.revocation_list = revocation_list;
    cfg_attrs_.revocation_list_size = revocation_size;

    opcua::ServerConfig server_config{};
    UA_ServerConfig* h_cfg = server_config.handle();

    try {
        // Setting server session PKI
        opcua::throwIfBad(UA_CertificateVerification_Trustlist(
            &h_cfg->sessionPKI,
            cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
            cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
            cfg_attrs_.revocation_list, cfg_attrs_.revocation_list_size
        ));
        std::cout << "Server trust list set\n";

        opcua::throwIfBad(UA_CertificateVerification_Trustlist(
            &h_cfg->secureChannelPKI,
            cfg_attrs_.trust_list, cfg_attrs_.trust_list_size,
            cfg_attrs_.issuer_list, cfg_attrs_.issuer_list_size,
            cfg_attrs_.revocation_list, cfg_attrs_.revocation_list_size
        ));
        std::cout << "Server trust list set\n";

        opcua::throwIfBad(UA_ServerConfig_addSecurityPolicyBasic256Sha256(
            h_cfg, &cfg_attrs_.certificate, &cfg_attrs_.private_key
        ));

        server_config.setAccessControl(std::make_unique<AccessControlCustom>());
        server_config.setApplicationName("OPC UA Test Server");
        server_config.setApplicationUri("urn:myorg:telemetry:server"); // TODO, hardcoded for now
        opcua::throwIfBad(UA_ServerConfig_addAllEndpoints(h_cfg));
        std::cout << "Server access control and application properties set\n";

        server_ = opcua::Server(std::move(server_config));
    } catch (const opcua::BadStatus& e) {
        std::cerr << "Failed to construct ServerConfig. Status: " << e.what()
                  << " (0x" << std::hex << e.code() << std::dec << ")\n"
                  << "Likely cause: certificate/key encoding mismatch, expired cert, "
                     "or trust/issuer list malformed. Check DER vs PEM on all loaded files.\n";
        throw;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        throw; 
    }
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
    opcua::throwIfBad(
            UA_ByteString_allocBuffer(&result, static_cast<size_t>(size)) 
    );

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
