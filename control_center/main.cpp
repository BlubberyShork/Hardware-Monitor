#include "ControlCenterClient.h"
#include "DisplayGrid.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>

namespace {
volatile std::sig_atomic_t g_running = 1;
void handleSigint(int) { g_running = 0; }
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSigint);

    const std::string endpoint = (argc > 1) ? argv[1] : "opc.tcp://localhost:4840";
    const std::filesystem::path project_root =
        (argc > 2) ? std::filesystem::path(argv[2]) : std::filesystem::current_path();

    auto grid = std::make_shared<DisplayGrid>();

    ControlCenterClient control_center(
        "control_center",
        project_root,
        [grid](const std::string& client_name, ClientRow row) {
            grid->update(client_name, std::move(row));
        });

    try {
        control_center.connect(endpoint);
        control_center.start();
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect: " << e.what() << "\n";
        return 1;
    }

    using namespace std::chrono_literals;
    constexpr auto kIoTimeout = 100ms;
    constexpr auto kPollInterval = 1750ms;
    constexpr auto kRenderInterval = 1750ms;

    auto last_render = std::chrono::steady_clock::now();

    while (g_running) {
        control_center.tick(kIoTimeout, kPollInterval);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_render >= kRenderInterval) {
            grid->renderIfDirty();
            last_render = now;
        }
    }

    std::cout << "\nShutting down...\n";
    control_center.disconnect();
    return 0;
}
