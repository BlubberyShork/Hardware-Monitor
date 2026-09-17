#include "DisplayGrid.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace {
constexpr size_t kColumnPadding = 3;
constexpr size_t kMinColumnWidth = 14;
constexpr const char* kCursorHomeAndClear = "\033[H\033[0J";
} // namespace

// Upserts a single device's telemetry into the grid, keyed by client + device type.
void DisplayGrid::update(const std::string& client_name, const std::string& device_key, ClientRow row) {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_[client_name][device_key] = std::move(row);
    dirty_ = true;
}

// Re-renders the terminal display only when underlying data has changed.
// Uses ANSI escape codes to clear and redraw from the top of the terminal.
bool DisplayGrid::renderIfDirty() {
    std::string frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!dirty_) {
            return false;
        }
        frame = buildFrame();
        dirty_ = false;
    }

    std::fputs(kCursorHomeAndClear, stdout);
    std::fputs(frame.c_str(), stdout);
    std::fflush(stdout);
    return true;
}

// Builds the full terminal frame as a string.
// Layout: one column per client, each column lists its devices vertically
// with a blank line separating different device types (e.g. GPU from CPU).
std::string DisplayGrid::buildFrame() const {
    if (rows_.empty()) {
        return "Waiting for telemetry clients...\n";
    }

    struct ColumnData {
        std::string name;
        std::vector<std::pair<std::string, std::string>> fields;
    };

    // Flatten each client's devices into a single column of fields,
    // inserting a blank separator row between each device group.
    std::vector<ColumnData> columns;
    for (const auto& [client_name, devices] : rows_) {
        ColumnData col;
        col.name = client_name;
        bool first_device = true;
        for (const auto& [device_key, row] : devices) {
            if (!first_device) {
                col.fields.emplace_back("", "");
            }
            first_device = false;
            for (const auto& field : row.fields) {
                col.fields.push_back(field);
            }
        }
        columns.push_back(std::move(col));
    }

    // Compute column widths: widest label+value pair, with minimum enforced.
    std::vector<size_t> widths(columns.size());
    for (size_t c = 0; c < columns.size(); ++c) {
        size_t width = columns[c].name.size();
        for (const auto& [label, value] : columns[c].fields) {
            width = std::max(width, label.size() + 2 + value.size());
        }
        widths[c] = std::max(width + kColumnPadding, kMinColumnWidth);
    }

    size_t max_rows = 0;
    for (const auto& col : columns) {
        max_rows = std::max(max_rows, col.fields.size());
    }

    std::ostringstream out;

    // Client name header row
    for (size_t c = 0; c < columns.size(); ++c) {
        out << columns[c].name;
        out << std::string(widths[c] - columns[c].name.size(), ' ');
    }
    out << "\n";

    // Dash separator under headers
    for (size_t c = 0; c < columns.size(); ++c) {
        out << std::string(std::min(widths[c] - kColumnPadding, widths[c]), '-');
        out << std::string(kColumnPadding, ' ');
    }
    out << "\n";

    // Data rows: each cell is either "label: value" or a blank separator
    for (size_t r = 0; r < max_rows; ++r) {
        for (size_t c = 0; c < columns.size(); ++c) {
            const auto& fields = columns[c].fields;
            std::string cell;
            if (r < fields.size()) {
                if (fields[r].second.empty()) {
                    cell = fields[r].first;
                } else {
                    cell = fields[r].first + ": " + fields[r].second;
                }
            }
            out << cell;
            out << std::string(widths[c] > cell.size() ? widths[c] - cell.size() : 1, ' ');
        }
        out << "\n";
    }

    return out.str();
}
