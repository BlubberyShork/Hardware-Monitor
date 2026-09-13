#include "DisplayGrid.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace {
constexpr size_t kColumnPadding = 3;
constexpr size_t kMinColumnWidth = 14;
constexpr const char* kCursorHomeAndClear = "\033[H\033[0J";
} // namespace

void DisplayGrid::update(const std::string& client_name, ClientRow row) {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_[client_name] = std::move(row);
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

    std::vector<std::string> names;
    names.reserve(rows_.size());
    for (const auto& [name, row] : rows_) {
        names.push_back(name);
    }

    std::vector<size_t> widths(names.size());
    for (size_t col = 0; col < names.size(); ++col) {
        size_t width = names[col].size();
        for (const auto& [label, value] : rows_.at(names[col]).fields) {
            width = std::max(width, label.size() + 2 + value.size());
        }
        widths[col] = std::max(width + kColumnPadding, kMinColumnWidth);
    }

    size_t max_rows = 0;
    for (const auto& name : names) {
        max_rows = std::max(max_rows, rows_.at(name).fields.size());
    }

    std::ostringstream out;

    for (size_t col = 0; col < names.size(); ++col) {
        out << names[col];
        out << std::string(widths[col] - names[col].size(), ' ');
    }
    out << "\n";

    for (size_t col = 0; col < names.size(); ++col) {
        out << std::string(std::min(widths[col] - kColumnPadding, widths[col]), '-');
        out << std::string(kColumnPadding, ' ');
    }
    out << "\n";

    for (size_t r = 0; r < max_rows; ++r) {
        for (size_t col = 0; col < names.size(); ++col) {
            const auto& fields = rows_.at(names[col]).fields;
            std::string cell;
            if (r < fields.size()) {
                cell = fields[r].first + ": " + fields[r].second;
            }
            out << cell;
            out << std::string(widths[col] > cell.size() ? widths[col] - cell.size() : 1, ' ');
        }
        out << "\n";
    }

    return out.str();
}
