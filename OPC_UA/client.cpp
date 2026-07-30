#include "client.h"

#include <iostream>
#include <fstream>

SystemInfoClient::SystemInfoClient(
    std::string_view client_name
) 
    : client_name_(client_name)
{
    client_cfg_attrs_ = getClientConfigAttributes();
    opcua::ClientConfig client_cfg = opcua::ClientConfig(
        client_cfg_attrs_.certificate,
        client_cfg_attrs_.private_key,
        client_cfg_attrs_.trust_list,
        client_cfg_attrs_.revocation_list
    ); 

    // TODO - temporary, for now. Later use SignAndEncrypt
    client_cfg.setSecurityMode(opcua::MessageSecurityMode::None);
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
    const fs::path pkiRoot   = proj_root / "pki";
    const fs::path devicesDir = pkiRoot / "devices";
    const std::string thisDeviceName = std::string(client_name_);
    const std::string trustedServerName = std::string("server");

    try {
        attrs.certificate = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".crt"));
        attrs.private_key = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
        throw;
    }

    // TODO -> Hardcoded for now since im only working with one server. Alter the server file to be \servers
    //      and search for trusted servers if need be
    std::vector<opcua::ByteString> trust_list_storage{};
    std::error_code ec;
    // TODO -> Change this from an iterator to a raw file grab. We only have one server for now, and we will expand to support multiple trusted servers later if needed
    for (const auto& serverEntry : fs::directory_iterator(fs::path(pkiRoot) / "server", ec)) {
        if (ec) break;
        if (!serverEntry.is_directory()) continue;
        
        const std::string serverName = serverEntry.path().filename().string();
        
        if (serverName != trustedServerName) continue;
        const fs::path certPath = serverEntry.path() / (serverName + ".crt");
        
        std::error_code fileEc;
        if (!fs::is_regular_file(certPath, fileEc) || fileEc) continue;
        try {
            trust_list_storage.push_back(readBytesFromFile(certPath));
        } catch (const std::runtime_error& e) {
            std::cerr << "Skipping trust list entry '" << trustedServerName << "': " << e.what() << "\n";
            continue;  // don't abort startup over one bad device cert
        }
    }

    attrs.trust_list = opcua::Span<opcua::ByteString>(trust_list_storage);
    return attrs;
}

opcua::ByteString SystemInfoClient::readBytesFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open PKI file: " + path.string());
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("PKI file is empty or unreadable: " + path.string());
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> res_buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(res_buffer.data()), size)) {
        throw std::runtime_error("Failed to read PKI file: " + path.string());
    }

    return opcua::ByteString(res_buffer);   
}


