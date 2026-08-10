#pragma once
#include <open62541pp/client.hpp>
#include <string>
#include <vector>

class TestClient {
public:
    TestClient();

    // Connects, writes a string to the server, reads it back, prints both.
    void run(const std::string& endpointUrl, const std::string& message);

private:
    opcua::Client client_;

    opcua::ByteString loadCertificate() const;
    opcua::ByteString loadPrivateKey() const;
    std::vector<opcua::ByteString> loadTrustList() const;
};
