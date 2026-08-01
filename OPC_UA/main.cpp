#include "server.h"
#include "client.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace {
std::atomic<bool> g_running{true};
SystemInfoServer* g_serverPtr = nullptr; // for the signal handler

void handleSignal(int) {
    g_running = false;
    if (g_serverPtr) g_serverPtr->stop();
}
} // namespace

void runServer(SystemInfoServer& server) {
    g_serverPtr = &server;
    try {
        std::cout << "Server starting...\n";
        server.run(); // blocks here until server.stop() is called
        std::cout << "Server stopped.\n";
    } catch (const opcua::BadStatus& e) {
        std::cerr << "Fatal: open62541 status error: " << e.what()
                  << " (0x" << std::hex << e.code() << std::dec << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Fatal: unknown exception\n";
    }
}

void runClient() {
    const char* envEndpoint = std::getenv("SERVER_ENDPOINT");
    if (!envEndpoint || std::string_view(envEndpoint).empty()) {
        std::cerr << "SERVER_ENDPOINT env var not set\n";
        return;
    }
    const std::string endpoint(envEndpoint);

    try {
        SystemInfoClient client("test_client");
        client.connect(endpoint);
        // opcua::Node node{client, opcua::VariableId::Server_ServerStatus_CurrentTime};
        // const auto dt = node.readValue().to<opcua::DateTime>();
        client.disconnect();
    } catch (const opcua::BadStatus& e) {
        std::cerr << "Fatal: open62541 status error: " << e.what()
                  << " (0x" << std::hex << e.code() << std::dec << ")\n";
    }
}

int main() {
    SystemInfoServer server{};
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::thread ts(runServer, std::ref(server));
    std::thread tc(runClient);

    tc.join();
    ts.join(); // unblocks once SIGINT/SIGTERM fires server.stop()
    return 0;
}
