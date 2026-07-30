#include "server.h"
#include "test_client.h"

int main() {
    try {
        SystemInfoServer server;
        server.run();
        std::cout << "Server is running...\n";
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

    return 0;
}
