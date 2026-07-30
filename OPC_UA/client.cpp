#include "client.h"

#include <iostream>
#include <fstream>

SystemInfoClient::SystemInfoClient() {
    client_cfg_attrs_ = getClientConfigAttributes();
    opcua::ClientConfig client_cfg = opcua::ClientConfig(
        &client_cfg_attrs_.certificate,
        &client_cfg_attrs_.private_key,
        client_cfg_attrs_.trust_list,
        client_cfg_attrs_.revocation_list
    ); 

    client_ = opcua::ClientConfig(std::move(client_cfg));
}

//// Helper Functions ////
SystemInfoClient::ClientConfigAttributes SystemInfoClient::getClientConfigAttributes() {
    namespace fs = std::filesystem;
    ClientConfigAttributes attrs;

    // TODO -> needs to grab our certificate
    //  Check documentation: but im pretty sure we need to populate the trust_list from server directory
    const fs::path proj_root = fs::current_path().parent_path().parent_path();
    const fs::path pkiRoot   = proj_root / "pki";
    const fs::path serverDir = pkiRoot / "devices";
    const std::string thisDeviceName = "server";

    try {
        attrs.certificate = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".crt"));
        attrs.private_key = readBytesFromFile(devicesDir / thisDeviceName / (thisDeviceName + ".key"));
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed loading this server's own identity files: " << e.what() << "\n";
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

    UA_ByteString result;
    UA_StatusCode alloc_status = UA_ByteString_allocBuffer(&result, static_cast<size_t>(size));
    if (alloc_status != UA_STATUSCODE_GOOD) {
        throw std::runtime_error("Failed to allocate buffer for PKI file: " + path.string());
    }

    if (!file.read(reinterpret_cast<char*>(result.data), size)) {
        throw std::runtime_error("Failed to read PKI file: " + path.string());
    }

    return result;   
}


