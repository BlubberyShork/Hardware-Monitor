#include "test_client.h"
#include <open62541pp/node.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
opcua::ByteString readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("File is empty: " + path.string());
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return opcua::ByteString(buffer);
}
}  // namespace

TestClient::TestClient() {
    opcua::ByteString certificate = loadCertificate();
    opcua::ByteString privateKey  = loadPrivateKey();
    std::vector<opcua::ByteString> trustList = loadTrustList();

    opcua::ClientConfig config(
        certificate,
        privateKey,
        opcua::Span<const opcua::ByteString>(trustList),
        opcua::Span<const opcua::ByteString>{}  // issuer list, add CA here if needed
    );

    client_ = opcua::Client(std::move(config));
}

opcua::ByteString TestClient::loadCertificate() const {
    return readFile("pki/devices/test-client/test-client.crt");
}

opcua::ByteString TestClient::loadPrivateKey() const {
    return readFile("pki/devices/test-client/test-client.key");
}

std::vector<opcua::ByteString> TestClient::loadTrustList() const {
    return { readFile("pki/ca/ca.crt") };
}

void TestClient::run(const std::string& endpointUrl, const std::string& message) {
    std::cout << "Connecting to " << endpointUrl << "...\n";
    client_.connect(endpointUrl);
    std::cout << "Connected.\n";

    // Adjust this NodeId to match wherever your server exposes a writable string node.
    opcua::Node node{client_, opcua::NodeId(1, "TestMessage")};

    std::cout << "Writing: \"" << message << "\"\n";
    node.writeValue(opcua::Variant{message});

    const opcua::String readBack = node.readValue().to<opcua::String>();
    // NOTE: opcua::String -> std::string conversion method not yet confirmed
    // against your exact vendored version. If this line fails to compile,
    // check open62541pp/types.hpp for String's actual accessor -- likely one
    // of: .get(), .data()+.length(), or an explicit std::string(readBack) cast.
    std::cout << "Read back: \"" << std::string(readBack) << "\"\n";

    client_.disconnect();
    std::cout << "Disconnected.\n";
}
