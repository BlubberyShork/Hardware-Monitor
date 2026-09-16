#include "ControlCenterClient.h"
#include "DisplayGrid.h"
#include "../OPC_UA/FileLogger.h"
#include "../shared/PerformanceLogger.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
volatile std::sig_atomic_t g_running = 1;
void handleSigint(int) { g_running = 0; }

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    std::fputs("\033[?1049h", stdout);  // alternate screen buffer
    std::fputs("\033[?25l", stdout);    // hide cursor
    std::fflush(stdout);
}

void restoreConsole() {
    std::fputs("\033[?25h", stdout);    // show cursor
    std::fputs("\033[?1049l", stdout);  // restore main screen buffer
    std::fputs("\033[0m", stdout);      // reset attributes
    std::fflush(stdout);
}
} // namespace

int main() {
    std::signal(SIGINT, handleSigint);
    setupConsole();

    char* server_ip = nullptr;
    size_t server_ip_length = 0;
    if (_dupenv_s(&server_ip, &server_ip_length, "SERVER_IP") != 0 || server_ip == nullptr) {
        std::cerr << "SERVER_IP is not set\n";
        return 1;
    }
    const std::string endpoint = "opc.tcp://" + std::string(server_ip) + ":4840";
    std::free(server_ip);

    const std::filesystem::path project_root =
        std::filesystem::current_path().parent_path().parent_path();

    auto grid = std::make_shared<DisplayGrid>();
    auto logger = std::make_shared<FileLogger>(defaultLogPath(project_root, "control_center"));

    auto perf_logger = std::make_shared<PerformanceLogger>(
        defaultPerfLogPath(project_root, "control_center"));

    ControlCenterClient control_center(
        "control_center",
        project_root,
        logger,
        [grid](const std::string& client_name, const std::string& device_key, ClientRow row) {
            grid->update(client_name, device_key, std::move(row));
        });
    control_center.setPerfLogger(perf_logger);

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

    restoreConsole();
    std::cout << "Shutting down...\n";
    try {
        control_center.disconnect();
    } catch (const std::exception& e) {
        std::cerr << "Disconnect error: " << e.what() << "\n";
    }
    return 0;
}
