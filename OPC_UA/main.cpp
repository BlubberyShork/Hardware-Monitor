#include "server.h"
#include "client.h"

#include <cstdlib>

int main(int argc, char* argv[]) {
    SystemInfoClient client("test_client");
    std::cout << "Client built correctly, the C++ style worked\n";
    return 0;

    try {
        SystemInfoServer server;
        server.run();
        std::cout << "Server is running...\n";
        server.stop();
    } catch (const opcua::BadStatus& e) {
        std::cerr << "Fatal: open62541 status error: " << e.what()
                  << " (0x" << std::hex << e.code() << std::dec << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cout << "Hit a general exception\n";
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal: unknown exception\n";
        return 1;
    }

    // TODO - Move this to clients build, or run on separate threads 
    const std::string ENV_SERVER_ENDPOINT(std::getenv("SERVER_ENDPOINT"));
    if(ENV_SERVER_ENDPOINT == "") {
        std::cerr << "SERVER_ENDPOINT env var not set\n";
        throw;
    }

    try {
        SystemInfoClient client("Test Client");
        client.connect(std::string_view(ENV_SERVER_ENDPOINT));
       
        //opcua::Node node{client, opcua::VariableId::Server_ServerStatus_CurrentTime};
        //const auto dt = node.readValue().to<opcua::DateTime>();
        
        client.disconnect();
        //std::cout << "Server date (UTC): " << dt.format("%Y-%m-%d %H:%M:%S") << std::endl;
    } catch (const opcua::BadStatus& e) {
        std::cerr << "Fatal: open62541 status error: " << e.what()
                  << " (0x" << std::hex << e.code() << std::dec << ")\n";
        return 1;
    }

    return 0;
}
