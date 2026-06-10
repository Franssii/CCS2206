#include "include/IndustryWorkers.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

namespace {

std::string trim(const std::string& s) {
    std::string res = s;
    if (res.size() >= 3 && res.substr(0, 3) == "\xEF\xBB\xBF") res.erase(0, 3);
    size_t a = res.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = res.find_last_not_of(" \t\r\n");
    return res.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delim))
        parts.push_back(token);
    return parts;
}

} // namespace

bool IndustryWorkers::load(const std::string& path) {
    _defs.clear();
    _busStopCapacity = 200;

    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto parts = split(line, '|');
        if (parts.size() < 4) continue;

        IndustryWorkerDef def;
        def.id               = trim(parts[0]);
        def.maxWorkers       = std::stoi(trim(parts[1]));
        def.product          = trim(parts[2]);
        def.outputPerWorker  = std::stoi(trim(parts[3]));

        if (def.product == "passengers")
            _busStopCapacity = def.outputPerWorker;

        _defs[def.id] = def;
    }
    return true;
}

const IndustryWorkerDef* IndustryWorkers::find(const std::string& id) const {
    auto it = _defs.find(id);
    return (it != _defs.end()) ? &it->second : nullptr;
}

int IndustryWorkers::maxWorkersFor(const std::string& id) const {
    const auto* d = find(id);
    return d ? d->maxWorkers : 0;
}
