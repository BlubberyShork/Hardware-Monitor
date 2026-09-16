#include "DisplayGrid.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace {
constexpr size_t kColumnPadding = 3;
constexpr size_t kMinColumnWidth = 14;
constexpr const char* kCursorHomeAndClear = "\033[H\033[0J";
} // namespace

void DisplayGrid::update(const std::string& client_name, const std::string& device_key, ClientRow row) {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_[client_name][device_key] = std::move(row);
    dirty_ = true;
}

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

std::string DisplayGrid::buildFrame() const {
    if (rows_.empty()) {
        return "Waiting for telemetry clients...\n";
    }

    struct ColumnData {
        std::string name;
        std::vector<std::pair<std::string, std::string>> fields;
    };

    std::vector<ColumnData> columns;
    for (const auto& [client_name, devices] : rows_) {
        ColumnData col;
        col.name = client_name;
        for (const auto& [device_key, row] : devices) {
            for (const auto& field : row.fields) {
                col.fields.push_back(field);
            }
        }
        columns.push_back(std::move(col));
    }

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

    for (size_t c = 0; c < columns.size(); ++c) {
        out << columns[c].name;
        out << std::string(widths[c] - columns[c].name.size(), ' ');
    }
    out << "\n";

    for (size_t c = 0; c < columns.size(); ++c) {
        out << std::string(std::min(widths[c] - kColumnPadding, widths[c]), '-');
        out << std::string(kColumnPadding, ' ');
    }
    out << "\n";

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
