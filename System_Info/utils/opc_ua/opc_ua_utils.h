#include "../OPC_UA/utils.h"
#include <queue>
#include <condition_variable>

// TODO -> This is fragile. Come up with a different approach
namespace _client_queue {
    std::condition_variable _g_queue_cv_;
    std::mutex _g_queue_mtx_;
    std::unique_lock<std::mutex> _g_lck_;
    std::queue<opc_ua_utils::TelemetryStore> _g_opcua_client_queue_();
}
