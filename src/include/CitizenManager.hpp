#pragma once

#include "BuildingManager.hpp"
#include "IndustrialManager.hpp"
#include "RoadManager.hpp"
#include "IndustryWorkers.hpp"
#include "VehicleManager.hpp"
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct BusStopQueue {
    int waitingForBus = 0;
    int assignedFactoryIdx = -1;
};

struct Walker {
    float worldX = 0.f;
    float worldY = 0.f;
    int   houseIdx = -1;
    int   factoryIdx = -1;
    int   destGridX = 0;
    int   destGridY = 0;
    bool  toBoardingStop = true;
    bool  male = true;
    std::vector<std::pair<int,int>> path;
    size_t pathSeg = 0;
    float  segT = 0.f;
};

class CitizenManager {
public:
    void setIndustryWorkers(const IndustryWorkers* iw) { _industryWorkers = iw; }
    bool loadWalkerTextures(const std::string& dir);

    void distributeToHouses(BuildingManager& bm, int totalPopulation);
    int  totalResidents(const BuildingManager& bm) const;
    int  homelessCount(int totalPopulation, const BuildingManager& bm) const;

    bool assignWorkplace(BuildingManager& bm, int housePlacedIdx,
                         int industrialPlacedIdx,
                         const RoadManager& rm, const IndustrialManager& im,
                         const VehicleManager& vm,
                         int maxRoadSteps = 20);

    bool assignBoardingStop(BuildingManager& bm, int housePlacedIdx,
                            int stopX, int stopY,
                            const RoadManager& rm, const IndustrialManager& im,
                            const VehicleManager& vm,
                            int maxRoadSteps = 20);

    void clearWorkplace(BuildingManager& bm, int housePlacedIdx);

    void update(float dt, float stepX, float stepY,
                  BuildingManager& bm, const RoadManager& rm,
                  const IndustrialManager& im);

    void onBusAtStop(PlacedBus& bus, int stopX, int stopY, int prevTileIdx,
                     int passengerCapacity, const RoadManager& rm,
                     const IndustrialManager& im, float stepX, float stepY);

    bool getStopCounts(int gx, int gy, int& waitingAtStop, int& walkingToStop,
                       int& pendingResidents,
                       const BuildingManager* bm = nullptr) const;
    int  getStopAssignedFactory(int gx, int gy) const;

    bool assignFactoryAtStop(int stopX, int stopY, int factoryIdx,
                             BuildingManager& bm, const RoadManager& rm,
                             const IndustrialManager& im,
                             const IndustryWorkers& iw,
                             int maxRoadSteps = 20);

    void drawWorld(sf::RenderWindow& window, float stepX, float stepY) const;

    void save(const std::string& path, const BuildingManager& bm) const;
    void load(const std::string& path, BuildingManager& bm);
    void saveStops(const std::string& path) const;
    void loadStops(const std::string& path);
    void clearStops();
    void clearWalkers() { _walkers.clear(); }
    void dispatchAllWalkers(BuildingManager& bm, const RoadManager& rm,
                            const IndustrialManager& im, float stepX, float stepY);

private:
    const IndustryWorkers* _industryWorkers = nullptr;
    std::map<std::pair<int,int>, BusStopQueue> _stopQueues;
    std::vector<Walker> _walkers;
    sf::Texture _maleTex;
    sf::Texture _femaleTex;
    bool _hasWalkerTex = false;

    BusStopQueue& queueAt(int gx, int gy);
    bool findNearestBusStop(const RoadManager& rm, int gx, int gy, int w, int h,
                            int& outX, int& outY, int maxSteps = 80) const;
    bool isWalkableToFactory(const BuildingManager& bm, const RoadManager& rm,
                             const IndustrialManager& im, int houseIdx,
                             int maxRoadSteps) const;
    bool hasBusRouteFromStop(const VehicleManager& vm, int stopAX, int stopAY,
                             int& outDestX, int& outDestY) const;
    int  findFactoryNearStop(const RoadManager& rm, const IndustrialManager& im,
                             int stopX, int stopY, int maxSteps = 20) const;
    bool buildWalkerPath(Walker& w, int fromX, int fromY, int toX, int toY,
                         const RoadManager& rm) const;
    void spawnWalkersForHouse(BuildingManager& bm, int houseIdx,
                              const RoadManager& rm, const IndustrialManager& im,
                              float stepX, float stepY);
    void moveWalkers(float dt, float stepX, float stepY, const RoadManager& rm,
                     BuildingManager& bm, const IndustrialManager& im);
    static sf::Vector2f pathNodeToWorld(int gx, int gy, float stepX, float stepY);
};
