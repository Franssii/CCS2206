#pragma once

#include <string>
#include <map>

struct IndustryWorkerDef {
    std::string id;
    int         maxWorkers   = 0;
    std::string product;
    int         outputPerWorker = 0;
    int         passengerCapacity = 0;
};

class IndustryWorkers {
public:
    bool load(const std::string& path);

    const IndustryWorkerDef* find(const std::string& id) const;
    int maxWorkersFor(const std::string& id) const;
    int busStopCapacity() const { return _busStopCapacity; }

private:
    std::map<std::string, IndustryWorkerDef> _defs;
    int _busStopCapacity = 200;
};
