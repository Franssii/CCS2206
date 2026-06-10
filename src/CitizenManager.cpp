#include "include/CitizenManager.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

BusStopQueue& CitizenManager::queueAt(int gx, int gy) {
    return _stopQueues[{gx, gy}];
}

bool CitizenManager::loadWalkerTextures(const std::string& dir) {
    _hasWalkerTex = false;
    std::string base = dir;
    if (!base.empty() && base.back() != '/') base += '/';
    if (base.find("citizens") == std::string::npos)
        base += "citizens/";

    if (_maleTex.loadFromFile(base + "male.png") &&
        _femaleTex.loadFromFile(base + "female.png"))
        _hasWalkerTex = true;
    return _hasWalkerTex;
}

sf::Vector2f CitizenManager::pathNodeToWorld(int gx, int gy, float stepX, float stepY) {
    return {
        static_cast<float>(gx - gy) * stepX,
        static_cast<float>(gx + gy) * stepY + stepY
    };
}

bool CitizenManager::findNearestBusStop(const RoadManager& rm, int gx, int gy, int w, int h,
                                         int& outX, int& outY, int maxSteps) const
{
    return rm.findNearestBusStop(gx, gy, w, h, outX, outY, maxSteps);
}

bool CitizenManager::isWalkableToFactory(const BuildingManager& bm, const RoadManager& rm,
                                          const IndustrialManager& im, int houseIdx,
                                          int maxRoadSteps) const
{
    const auto& houses = bm.getPlaced();
    if (houseIdx < 0 || houseIdx >= static_cast<int>(houses.size())) return false;
    const auto& house = houses[houseIdx];
    if (house.assignedWorkIdx < 0) return false;

    int hw = 1, hh = 1;
    bm.getGridSize(house.defId, hw, hh);
    auto reachable = rm.findReachableIndustrialIndices(
        house.gridX, house.gridY, hw, hh, im, maxRoadSteps);
    return std::find(reachable.begin(), reachable.end(), house.assignedWorkIdx) != reachable.end();
}

bool CitizenManager::hasBusRouteFromStop(const VehicleManager& vm, int stopAX, int stopAY,
                                          int& outDestX, int& outDestY) const
{
    for (const auto& bus : vm.getBuses()) {
        if (!bus.routeActive) continue;
        if (bus.stopAGridX == stopAX && bus.stopAGridY == stopAY) {
            outDestX = bus.stopBGridX;
            outDestY = bus.stopBGridY;
            return true;
        }
    }
    return false;
}

int CitizenManager::findFactoryNearStop(const RoadManager& rm, const IndustrialManager& im,
                                         int stopX, int stopY, int maxSteps) const
{
    if (!rm.isRoadAt(stopX, stopY)) return -1;

    const auto& inds = im.getPlaced();
    int bestIdx = -1;
    int bestDist = maxSteps + 1;

    std::map<std::pair<int,int>, int> dist;
    std::vector<std::pair<int,int>> queue;
    dist[{stopX, stopY}] = 0;
    queue.push_back({stopX, stopY});

    const int ndx[] = {1, -1, 0, 0};
    const int ndy[] = {0, 0, 1, -1};

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        auto [cx, cy] = queue[qi];
        int d = dist[{cx, cy}];

        for (size_t i = 0; i < inds.size(); ++i) {
            int iw = 1, ih = 1;
            im.getGridSize(inds[i].defId, iw, ih);
            for (int dy = 0; dy < ih; ++dy) {
                for (int dx = 0; dx < iw; ++dx) {
                    int bx = inds[i].gridX + dx;
                    int by = inds[i].gridY + dy;
                    for (int n = 0; n < 4; ++n) {
                        if (cx == bx + ndx[n] && cy == by + ndy[n] && d < bestDist) {
                            bestDist = d;
                            bestIdx = static_cast<int>(i);
                        }
                    }
                }
            }
        }

        if (d >= maxSteps) continue;
        for (int n = 0; n < 4; ++n) {
            int nx = cx + ndx[n];
            int ny = cy + ndy[n];
            if (!rm.isRoadAt(nx, ny)) continue;
            if (dist.count({nx, ny})) continue;
            dist[{nx, ny}] = d + 1;
            queue.push_back({nx, ny});
        }
    }
    return bestIdx;
}

bool CitizenManager::buildWalkerPath(Walker& w, int fromX, int fromY, int toX, int toY,
                                      const RoadManager& rm) const
{
    w.path = rm.findRoadPath(fromX, fromY, toX, toY);
    if (w.path.size() < 2) return false;
    w.pathSeg = 0;
    w.segT = 0.f;
    return true;
}

void CitizenManager::spawnWalkersForHouse(BuildingManager& bm, int houseIdx,
                                          const RoadManager& rm, const IndustrialManager& im,
                                          float stepX, float stepY)
{
    const auto& houses = bm.getPlaced();
    if (houseIdx < 0 || houseIdx >= static_cast<int>(houses.size())) return;
    const auto& house = houses[houseIdx];
    if (house.residents <= 0) return;
    if (!house.useBusCommute && house.assignedWorkIdx < 0) return;
    if (house.useBusCommute && house.boardingStopX < 0) return;

    int hw = 1, hh = 1;
    bm.getGridSize(house.defId, hw, hh);

    int destX = -1, destY = -1;
    bool toStop = false;

    if (house.useBusCommute && house.boardingStopX >= 0) {
        destX = house.boardingStopX;
        destY = house.boardingStopY;
        toStop = true;
    } else if (!house.useBusCommute) {
        const auto& inds = im.getPlaced();
        if (house.assignedWorkIdx >= static_cast<int>(inds.size())) return;
        const auto& fac = inds[house.assignedWorkIdx];
        int iw = 1, ih = 1;
        im.getGridSize(fac.defId, iw, ih);
        if (!rm.findNearestBusStop(fac.gridX, fac.gridY, iw, ih, destX, destY, 20))
            return;
        toStop = false;
    } else {
        return;
    }

    int startX = -1, startY = -1;
    if (!rm.findNearestBusStop(house.gridX, house.gridY, hw, hh, startX, startY, 40))
        return;

    int existing = 0;
    for (const auto& w : _walkers)
        if (w.houseIdx == houseIdx) ++existing;

    int toSpawn = house.residents - existing;
    if (toSpawn <= 0) return;

    for (int i = 0; i < toSpawn; ++i) {
        Walker w;
        w.houseIdx = houseIdx;
        w.factoryIdx = house.assignedWorkIdx;
        w.destGridX = destX;
        w.destGridY = destY;
        w.toBoardingStop = toStop;
        w.male = (i % 2 == 0);
        if (!buildWalkerPath(w, startX, startY, destX, destY, rm)) continue;
        auto p = pathNodeToWorld(w.path[0].first, w.path[0].second, stepX, stepY);
        w.worldX = p.x;
        w.worldY = p.y;
        _walkers.push_back(w);
    }
}

void CitizenManager::moveWalkers(float dt, float stepX, float stepY, const RoadManager& rm,
                                  BuildingManager& bm, const IndustrialManager& im)
{
    const float speed = 2.5f;
    auto& houses = bm.getPlacedMutable();

    for (auto it = _walkers.begin(); it != _walkers.end(); ) {
        Walker& w = *it;
        if (w.path.size() < 2) {
            it = _walkers.erase(it);
            continue;
        }

        if (w.pathSeg >= w.path.size() - 1) {
            if (w.toBoardingStop) {
                queueAt(w.destGridX, w.destGridY).waitingForBus++;
            }
            it = _walkers.erase(it);
            continue;
        }

        auto [x0, y0] = w.path[w.pathSeg];
        auto [x1, y1] = w.path[w.pathSeg + 1];
        sf::Vector2f p0 = pathNodeToWorld(x0, y0, stepX, stepY);
        sf::Vector2f p1 = pathNodeToWorld(x1, y1, stepX, stepY);

        w.segT += speed * dt;
        while (w.segT >= 1.f && w.pathSeg < w.path.size() - 1) {
            w.segT -= 1.f;
            ++w.pathSeg;
            if (w.pathSeg >= w.path.size() - 1) break;
            std::tie(x0, y0) = w.path[w.pathSeg];
            std::tie(x1, y1) = w.path[w.pathSeg + 1];
            p0 = pathNodeToWorld(x0, y0, stepX, stepY);
            p1 = pathNodeToWorld(x1, y1, stepX, stepY);
        }

        if (w.pathSeg >= w.path.size() - 1) {
            if (w.toBoardingStop)
                queueAt(w.destGridX, w.destGridY).waitingForBus++;
            it = _walkers.erase(it);
            continue;
        }

        w.worldX = p0.x + (p1.x - p0.x) * w.segT;
        w.worldY = p0.y + (p1.y - p0.y) * w.segT;
        ++it;
    }
}

void CitizenManager::dispatchAllWalkers(BuildingManager& bm, const RoadManager& rm,
                                         const IndustrialManager& im,
                                         float stepX, float stepY)
{
    const auto& houses = bm.getPlaced();
    for (size_t i = 0; i < houses.size(); ++i) {
        const auto& h = houses[i];
        if (h.residents <= 0) continue;
        if (h.useBusCommute && h.boardingStopX >= 0)
            spawnWalkersForHouse(bm, static_cast<int>(i), rm, im, stepX, stepY);
        else if (h.assignedWorkIdx >= 0)
            spawnWalkersForHouse(bm, static_cast<int>(i), rm, im, stepX, stepY);
    }
}

void CitizenManager::update(float dt, float stepX, float stepY,
                            BuildingManager& bm, const RoadManager& rm,
                            const IndustrialManager& im)
{
    moveWalkers(dt, stepX, stepY, rm, bm, im);
}

void CitizenManager::onBusAtStop(PlacedBus& bus, int stopX, int stopY, int prevTileIdx,
                                  int passengerCapacity, const RoadManager& rm,
                                  const IndustrialManager& im, float stepX, float stepY)
{
    const int loopSize = static_cast<int>(bus.loopPath.size());
    if (loopSize < 2 || bus.abPathNodes < 2) return;

    const int curTile = bus.prevPathTile;
    if (curTile < 0 || curTile >= loopSize) return;

    int freeSeats = passengerCapacity - bus.passengersToWork;
    if (freeSeats < 0) freeSeats = 0;

    const int bTile = bus.abPathNodes - 1;

    if (curTile == 0 && bus.stopAGridX == stopX && bus.stopAGridY == stopY &&
        prevTileIdx != loopSize - 1) {
        auto& q = queueAt(bus.stopAGridX, bus.stopAGridY);
        int take = std::min(q.waitingForBus, freeSeats);
        q.waitingForBus -= take;
        bus.passengersToWork += take;
        return;
    }

    if (curTile == bTile && bus.stopBGridX == stopX && bus.stopBGridY == stopY &&
        prevTileIdx == bTile - 1 && bus.passengersToWork > 0) {
        int drop = bus.passengersToWork;
        bus.passengersToWork = 0;

        int destX = bus.stopBGridX;
        int destY = bus.stopBGridY;
        int facIdx = queueAt(bus.stopAGridX, bus.stopAGridY).assignedFactoryIdx;
        if (facIdx < 0)
            facIdx = findFactoryNearStop(rm, im, destX, destY, 20);

        int goalX = destX, goalY = destY;
        if (facIdx >= 0) {
            const auto& fac = im.getPlaced()[facIdx];
            int iw = 1, ih = 1;
            im.getGridSize(fac.defId, iw, ih);
            rm.findNearestBusStop(fac.gridX, fac.gridY, iw, ih, goalX, goalY, 20);
        }

        for (int p = 0; p < drop; ++p) {
            Walker w;
            w.houseIdx = -1;
            w.factoryIdx = facIdx;
            w.destGridX = goalX;
            w.destGridY = goalY;
            w.toBoardingStop = false;
            w.male = (p % 2 == 0);

            w.path = rm.findRoadPath(destX, destY, goalX, goalY);
            if (w.path.size() < 2)
                w.path = {{destX, destY}, {goalX, goalY}};
            w.pathSeg = 0;
            w.segT = static_cast<float>(p) * 0.08f;
            auto pos = pathNodeToWorld(destX, destY, stepX, stepY);
            w.worldX = pos.x;
            w.worldY = pos.y;
            _walkers.push_back(w);
        }
    }

    (void)stopY;
}

bool CitizenManager::getStopCounts(int gx, int gy, int& waitingAtStop, int& walkingToStop,
                                   int& pendingResidents, const BuildingManager* bm) const
{
    waitingAtStop = 0;
    walkingToStop = 0;
    pendingResidents = 0;

    auto it = _stopQueues.find({gx, gy});
    if (it != _stopQueues.end())
        waitingAtStop = it->second.waitingForBus;

    for (const auto& w : _walkers) {
        if (w.toBoardingStop && w.destGridX == gx && w.destGridY == gy)
            ++walkingToStop;
    }

    if (bm) {
        for (const auto& h : bm->getPlaced()) {
            if (h.useBusCommute && h.boardingStopX == gx && h.boardingStopY == gy &&
                h.assignedWorkIdx < 0)
                pendingResidents += h.residents;
        }
    }

    return waitingAtStop > 0 || walkingToStop > 0 || pendingResidents > 0;
}

int CitizenManager::getStopAssignedFactory(int gx, int gy) const {
    auto it = _stopQueues.find({gx, gy});
    return (it != _stopQueues.end()) ? it->second.assignedFactoryIdx : -1;
}

void CitizenManager::drawWorld(sf::RenderWindow& window, float stepX, float stepY) const {
    constexpr float kScale = 4.f;

    auto drawPerson = [&](float wx, float wy, bool male) {
        if (_hasWalkerTex) {
            const sf::Texture& tex = male ? _maleTex : _femaleTex;
            sf::Sprite spr(tex);
            spr.setScale({kScale, kScale});
            spr.setOrigin({static_cast<float>(tex.getSize().x) / 2.f,
                           static_cast<float>(tex.getSize().y)});
            spr.setPosition({wx, wy});
            window.draw(spr);
        } else {
            sf::RectangleShape dot({kScale * 2.f, kScale * 2.f});
            dot.setOrigin({kScale, kScale * 2.f});
            dot.setPosition({wx, wy});
            dot.setFillColor(male ? sf::Color(80, 140, 255) : sf::Color(255, 120, 180));
            window.draw(dot);
        }
    };

    for (const auto& w : _walkers)
        drawPerson(w.worldX, w.worldY, w.male);

    for (const auto& [key, q] : _stopQueues) {
        if (q.waitingForBus <= 0) continue;
        auto pos = pathNodeToWorld(key.first, key.second, stepX, stepY);
        int shown = std::min(q.waitingForBus, 12);
        for (int i = 0; i < shown; ++i) {
            float ox = static_cast<float>((i % 4) - 1) * 6.f;
            float oy = static_cast<float>((i / 4)) * -5.f;
            drawPerson(pos.x + ox, pos.y + oy, (i % 2) == 0);
        }
    }
}

void CitizenManager::clearStops() {
    _stopQueues.clear();
    _walkers.clear();
}

bool CitizenManager::assignWorkplace(BuildingManager& bm, int housePlacedIdx,
                                     int industrialPlacedIdx,
                                     const RoadManager& rm, const IndustrialManager& im,
                                     const VehicleManager& vm,
                                     int maxRoadSteps)
{
    auto& houses = bm.getPlacedMutable();
    if (housePlacedIdx < 0 || housePlacedIdx >= static_cast<int>(houses.size()))
        return false;

    const auto& placedInd = im.getPlaced();
    if (industrialPlacedIdx < 0 || industrialPlacedIdx >= static_cast<int>(placedInd.size()))
        return false;

    const PlacedBuilding& house = houses[housePlacedIdx];
    const PlacedIndustrial& target = placedInd[industrialPlacedIdx];

    if (_industryWorkers && _industryWorkers->maxWorkersFor(target.defId) == 0)
        return false;

    int hw = 1, hh = 1;
    bm.getGridSize(house.defId, hw, hh);
    auto reachable = rm.findReachableIndustrialIndices(
        house.gridX, house.gridY, hw, hh, im, maxRoadSteps);
    bool walkable = std::find(reachable.begin(), reachable.end(),
                              industrialPlacedIdx) != reachable.end();

    if (walkable) {
        if (_industryWorkers) {
            int maxW = _industryWorkers->maxWorkersFor(target.defId);
            if (maxW > 0) {
                int assigned = 0;
                for (const auto& h : houses) {
                    if (h.assignedWorkIdx == industrialPlacedIdx)
                        assigned += h.residents;
                }
                if (assigned + house.residents > maxW) return false;
            }
        }
        houses[housePlacedIdx].assignedWorkIdx = industrialPlacedIdx;
        houses[housePlacedIdx].useBusCommute = false;
        houses[housePlacedIdx].boardingStopX = -1;
        houses[housePlacedIdx].boardingStopY = -1;
        _walkers.erase(std::remove_if(_walkers.begin(), _walkers.end(),
            [&](const Walker& w) { return w.houseIdx == housePlacedIdx; }),
            _walkers.end());
        spawnWalkersForHouse(bm, housePlacedIdx, rm, im, 32.f, 16.f);
        return true;
    }

    int homeStopX = -1, homeStopY = -1;
    int workStopX = -1, workStopY = -1;
    int iw = 1, ih = 1;
    im.getGridSize(target.defId, iw, ih);
    if (!findNearestBusStop(rm, house.gridX, house.gridY, hw, hh, homeStopX, homeStopY))
        return false;
    if (!findNearestBusStop(rm, target.gridX, target.gridY, iw, ih, workStopX, workStopY))
        return false;

    int destX = 0, destY = 0;
    if (!hasBusRouteFromStop(vm, homeStopX, homeStopY, destX, destY))
        return false;
    if (destX != workStopX || destY != workStopY) {
        if (!hasBusRouteFromStop(vm, workStopX, workStopY, destX, destY))
            return false;
    }

    if (_industryWorkers) {
        int maxW = _industryWorkers->maxWorkersFor(target.defId);
        if (maxW > 0) {
            int assigned = 0;
            for (const auto& h : houses) {
                if (h.assignedWorkIdx == industrialPlacedIdx)
                    assigned += h.residents;
            }
            if (assigned + house.residents > maxW) return false;
        }
    }

    houses[housePlacedIdx].assignedWorkIdx = industrialPlacedIdx;
    houses[housePlacedIdx].useBusCommute = true;
    houses[housePlacedIdx].boardingStopX = homeStopX;
    houses[housePlacedIdx].boardingStopY = homeStopY;
    _walkers.erase(std::remove_if(_walkers.begin(), _walkers.end(),
        [&](const Walker& w) { return w.houseIdx == housePlacedIdx; }),
        _walkers.end());
    spawnWalkersForHouse(bm, housePlacedIdx, rm, im, 32.f, 16.f);
    return true;
}

bool CitizenManager::assignBoardingStop(BuildingManager& bm, int housePlacedIdx,
                                         int stopX, int stopY,
                                         const RoadManager& rm, const IndustrialManager& im,
                                         const VehicleManager& vm,
                                         int maxRoadSteps)
{
    auto& houses = bm.getPlacedMutable();
    if (housePlacedIdx < 0 || housePlacedIdx >= static_cast<int>(houses.size()))
        return false;

    const PlacedBuilding& house = houses[housePlacedIdx];
    int hw = 1, hh = 1;
    bm.getGridSize(house.defId, hw, hh);

    auto stops = rm.findReachableBusStops(house.gridX, house.gridY, hw, hh, maxRoadSteps);
    bool found = false;
    for (const auto& [sx, sy] : stops)
        if (sx == stopX && sy == stopY) { found = true; break; }
    if (!found) return false;

    int destX = 0, destY = 0;
    if (!hasBusRouteFromStop(vm, stopX, stopY, destX, destY))
        return false;

    auto& q = queueAt(stopX, stopY);
    houses[housePlacedIdx].assignedWorkIdx = q.assignedFactoryIdx;
    houses[housePlacedIdx].useBusCommute = true;
    houses[housePlacedIdx].boardingStopX = stopX;
    houses[housePlacedIdx].boardingStopY = stopY;
    _walkers.erase(std::remove_if(_walkers.begin(), _walkers.end(),
        [&](const Walker& w) { return w.houseIdx == housePlacedIdx; }),
        _walkers.end());
    spawnWalkersForHouse(bm, housePlacedIdx, rm, im, 32.f, 16.f);
    return true;
}

bool CitizenManager::assignFactoryAtStop(int stopX, int stopY, int factoryIdx,
                                          BuildingManager& bm, const RoadManager& rm,
                                          const IndustrialManager& im,
                                          const IndustryWorkers& iw,
                                          int maxRoadSteps)
{
    const auto& inds = im.getPlaced();
    if (factoryIdx < 0 || factoryIdx >= static_cast<int>(inds.size()))
        return false;
    if (iw.maxWorkersFor(inds[factoryIdx].defId) == 0)
        return false;

    auto reachable = rm.findReachableIndustrialIndices(stopX, stopY, 1, 1, im, maxRoadSteps);
    if (std::find(reachable.begin(), reachable.end(), factoryIdx) == reachable.end())
        return false;

    queueAt(stopX, stopY).assignedFactoryIdx = factoryIdx;

    auto& houses = bm.getPlacedMutable();
    for (auto& h : houses) {
        if (h.useBusCommute && h.boardingStopX == stopX && h.boardingStopY == stopY)
            h.assignedWorkIdx = factoryIdx;
    }
    return true;
}

void CitizenManager::clearWorkplace(BuildingManager& bm, int housePlacedIdx) {
    auto& houses = bm.getPlacedMutable();
    if (housePlacedIdx >= 0 && housePlacedIdx < static_cast<int>(houses.size())) {
        houses[housePlacedIdx].assignedWorkIdx = -1;
        houses[housePlacedIdx].useBusCommute = false;
        houses[housePlacedIdx].boardingStopX = -1;
        houses[housePlacedIdx].boardingStopY = -1;
        _walkers.erase(std::remove_if(_walkers.begin(), _walkers.end(),
            [&](const Walker& w) { return w.houseIdx == housePlacedIdx; }),
            _walkers.end());
    }
}

void CitizenManager::distributeToHouses(BuildingManager& bm, int totalPopulation) {
    auto& placed = bm.getPlacedMutable();
    for (auto& pb : placed)
        pb.residents = 0;

    int remaining = std::max(0, totalPopulation);
    for (auto& pb : placed) {
        if (remaining <= 0) break;
        int cap = bm.getCapacityFor(pb);
        int add = std::min(cap, remaining);
        pb.residents = add;
        remaining -= add;
    }
}

int CitizenManager::totalResidents(const BuildingManager& bm) const {
    int sum = 0;
    for (const auto& pb : bm.getPlaced())
        sum += pb.residents;
    return sum;
}

int CitizenManager::homelessCount(int totalPopulation, const BuildingManager& bm) const {
    return std::max(0, totalPopulation - totalResidents(bm));
}

void CitizenManager::save(const std::string& path, const BuildingManager& bm) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return;

    const auto& placed = bm.getPlaced();
    f << placed.size() << "\n";
    for (const auto& pb : placed) {
        f << pb.gridX << " " << pb.gridY << " "
          << pb.residents << " " << pb.assignedWorkIdx << " "
          << (pb.useBusCommute ? 1 : 0) << " "
          << pb.boardingStopX << " " << pb.boardingStopY << "\n";
    }
}

void CitizenManager::load(const std::string& path, BuildingManager& bm) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    size_t count = 0;
    if (!(f >> count)) return;

    auto& placed = bm.getPlacedMutable();
    for (size_t i = 0; i < count && i < placed.size(); ++i) {
        int gx = 0, gy = 0, residents = 0, workIdx = -1, bus = 0;
        int bsx = -1, bsy = -1;
        if (!(f >> gx >> gy >> residents >> workIdx >> bus)) break;
        if (!(f >> bsx >> bsy)) { bsx = -1; bsy = -1; }

        for (auto& pb : placed) {
            if (pb.gridX == gx && pb.gridY == gy) {
                pb.residents = residents;
                pb.assignedWorkIdx = workIdx;
                pb.useBusCommute = (bus != 0);
                pb.boardingStopX = bsx;
                pb.boardingStopY = bsy;
                break;
            }
        }
    }
}

void CitizenManager::saveStops(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return;
    f << _stopQueues.size() << "\n";
    for (const auto& [key, q] : _stopQueues) {
        f << key.first << " " << key.second << " " << q.waitingForBus << " "
          << q.assignedFactoryIdx << "\n";
    }
}

void CitizenManager::loadStops(const std::string& path) {
    _stopQueues.clear();
    std::ifstream f(path);
    if (!f.is_open()) return;
    size_t count = 0;
    if (!(f >> count)) return;
    for (size_t i = 0; i < count; ++i) {
        int gx = 0, gy = 0, w = 0, fac = -1;
        if (!(f >> gx >> gy >> w)) break;
        if (!(f >> fac)) fac = -1;
        BusStopQueue q;
        q.waitingForBus = w;
        q.assignedFactoryIdx = fac;
        _stopQueues[{gx, gy}] = q;
    }
}
