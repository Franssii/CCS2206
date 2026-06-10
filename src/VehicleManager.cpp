#include "include/VehicleManager.hpp"
#include "include/RoadManager.hpp"
#include "include/CitizenManager.hpp"
#include "include/IndustrialManager.hpp"
#include "include/DetailPanelUtil.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s) {
    std::string res = s;
    if (res.size() >= 3 && res.substr(0, 3) == "\xEF\xBB\xBF") res.erase(0, 3);
    size_t a = res.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = res.find_last_not_of(" \t\r\n");
    return res.substr(a, b - a + 1);
}

} // namespace

VehicleManager::VehicleManager(const sf::Font& font, const sf::Texture& closeTexture)
    : _font(font), _closeBtn(closeTexture)
{
    scaleCloseSprite(_closeBtn, closeTexture);
}

bool VehicleManager::loadDefs(const std::string& dataFilePath) {
    _defs.clear();
    std::ifstream file(dataFilePath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> parts;
        while (std::getline(ss, token, '|'))
            parts.push_back(trim(token));
        if (parts.size() < 5) continue;

        VehicleDef def;
        def.id                = parts[0];
        def.displayName       = parts[1];
        def.description       = parts[2];
        def.cost              = std::stoi(parts[3]);
        def.passengerCapacity = std::stoi(parts[4]);
        _defs.push_back(def);
    }
    return !_defs.empty();
}

void VehicleManager::loadBusSprites(const std::string& texturesDir) {
    _busTextures.clear();
    _hasBusTexture = false;
    std::string dir = texturesDir;
    if (!dir.empty() && dir.back() != '/') dir += '/';
    if (dir.find("buses") == std::string::npos)
        dir += "buses/";

    static const char* ids[] = {
        "bus_1", "bus_1b", "bus_1c", "bus_1d", "bus_1e", "bus_1f", "bus_1g", "bus_1h"
    };
    for (const char* id : ids) {
        std::string path = dir + id + ".png";
        if (!fs::exists(path)) continue;
        sf::Texture tex;
        if (tex.loadFromFile(path)) {
            _busTextures[id] = std::move(tex);
            _hasBusTexture = true;
        }
    }
}

sf::Vector2f VehicleManager::gridToWorld(int gx, int gy, float stepX, float stepY) {
    return {
        static_cast<float>(gx - gy) * stepX,
        static_cast<float>(gx + gy) * stepY + stepY
    };
}

float VehicleManager::computePanelHeight() const {
    float h = VM_TITLEBAR_H + 12.f + 36.f + 10.f;
    int busesHere = 0;
    for (const auto& b : _buses)
        if (b.depotGridX == _depotGridX && b.depotGridY == _depotGridY) ++busesHere;

    h += static_cast<float>(std::max(1, busesHere)) * 36.f + 10.f;
    if (_routeBusIdx >= 0) h += 40.f;
    return std::max(VM_DETAIL_MIN_H, h);
}

bool VehicleManager::tryOpenDepotAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                                         const RoadManager& rm, sf::RenderWindow& window,
                                         const sf::View& gameView)
{
    sf::Vector2i g = RoadManager::worldToGrid(worldPos, stepX, stepY);
    if (!rm.isVehicleStationAt(g.x, g.y)) return false;

    _depotOpen = true;
    _depotGridX = g.x;
    _depotGridY = g.y;
    _depotWorldPos = gridToWorld(g.x, g.y, stepX, stepY);
    _routeBusIdx = -1;
    _routePickStep = 0;
    _panelH = computePanelHeight();
    _panelPos = panelPosBesideBuilding(_depotWorldPos, window, gameView, VM_DETAIL_W, _panelH);
    _closeRect = detailCloseRect(_panelPos, VM_DETAIL_W, VM_TITLEBAR_H);
    return true;
}

void VehicleManager::closeDepot(RoadManager& rm) {
    rm.setHighlightBusStops(false);
    _depotOpen = false;
    _routeBusIdx = -1;
    _routePickStep = 0;
}

void VehicleManager::addHit(float x, float y, float w, float h, const std::string& id) {
    _hits.push_back({sf::FloatRect({x, y}, {w, h}), id});
}

const VehicleManager::HitBox* VehicleManager::hitAt(sf::Vector2f p) const {
    for (const auto& h : _hits)
        if (h.rect.contains(p)) return &h;
    return nullptr;
}

void VehicleManager::rebuildBusLoop(PlacedBus& bus, RoadManager& rm) {
    bus.loopPath.clear();
    bus.routeActive = false;
    if (bus.stopAGridX < 0 || bus.stopBGridX < 0) return;

    auto pathAB = rm.findRoadPath(bus.stopAGridX, bus.stopAGridY,
                                  bus.stopBGridX, bus.stopBGridY);
    auto pathBA = rm.findRoadPath(bus.stopBGridX, bus.stopBGridY,
                                  bus.stopAGridX, bus.stopAGridY);
    if (pathAB.size() < 2 || pathBA.size() < 2) return;

    bus.loopPath = pathAB;
    bus.abPathNodes = static_cast<int>(pathAB.size());
    for (size_t i = 1; i < pathBA.size(); ++i)
        bus.loopPath.push_back(pathBA[i]);
    bus.routeActive = true;
    bus.pathProgress = 0.f;
    bus.prevPathTile = -1;
    bus.passengersToWork = 0;
    bus.passengersToHome = 0;
}

bool VehicleManager::handleDepotEvent(const sf::Event& event, sf::RenderWindow& window,
                                      long long& cash, RoadManager& rm)
{
    if (!_depotOpen) return false;
    _mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getDefaultView());

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (_routeBusIdx >= 0) {
                _routeBusIdx = -1;
                _routePickStep = 0;
                rm.setHighlightBusStops(false);
            } else {
                closeDepot(rm);
            }
            return true;
        }
        return false;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;

        if (_closeRect.contains(_mousePos)) {
            closeDepot(rm);
            return true;
        }

        const HitBox* h = hitAt(_mousePos);
        if (!h) {
            if (_routeBusIdx >= 0) return false;
            return true;
        }

        if (h->id == "buy_bus") {
            const VehicleDef* def = _defs.empty() ? nullptr : &_defs[0];
            if (def && cash >= def->cost) {
                cash -= def->cost;
                PlacedBus bus;
                bus.depotGridX = _depotGridX;
                bus.depotGridY = _depotGridY;
                bus.vehicleId = def->id;
                _buses.push_back(bus);
            }
            return true;
        }

        if (h->id.rfind("route_", 0) == 0) {
            _routeBusIdx = std::atoi(h->id.c_str() + 6);
            _routePickStep = 0;
            _pendingStopAX = _pendingStopAY = -1;
            rm.setHighlightBusStops(true);
            return true;
        }
    }

    return _depotOpen;
}

bool VehicleManager::handleRouteStopClick(int gridX, int gridY, RoadManager& rm) {
    if (_routeBusIdx < 0 || _routeBusIdx >= static_cast<int>(_buses.size())) return false;

    int stopX = gridX, stopY = gridY;
    if (!rm.findBusStopAt(gridX, gridY, stopX, stopY)) return false;

    PlacedBus& bus = _buses[_routeBusIdx];
    if (bus.depotGridX != _depotGridX || bus.depotGridY != _depotGridY)
        return false;

    if (_routePickStep == 0) {
        _pendingStopAX = stopX;
        _pendingStopAY = stopY;
        _routePickStep = 1;
        return true;
    }

    if (stopX == _pendingStopAX && stopY == _pendingStopAY) return true;

    bus.stopAGridX = _pendingStopAX;
    bus.stopAGridY = _pendingStopAY;
    bus.stopBGridX = stopX;
    bus.stopBGridY = stopY;
    rebuildBusLoop(bus, rm);

    _routeBusIdx = -1;
    _routePickStep = 0;
    rm.setHighlightBusStops(false);
    return true;
}
void VehicleManager::drawDepotPanel(sf::RenderWindow& w) {
    if (!_depotOpen) return;

    _hits.clear();
    _panelH = computePanelHeight();

    sf::RectangleShape bg({VM_DETAIL_W, _panelH});
    bg.setPosition(_panelPos);
    bg.setFillColor(sf::Color(10, 10, 10, 245));
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({VM_DETAIL_W, VM_TITLEBAR_H});
    tb.setPosition(_panelPos);
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "VEHICLE DEPOT", VM_FONT + 2);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_panelPos.x + 10.f, _panelPos.y + 6.f});
    w.draw(title);
    drawDetailCloseButton(w, _closeBtn, _panelPos, VM_DETAIL_W, VM_TITLEBAR_H, _closeRect);

    float x = _panelPos.x + 14.f;
    float y = _panelPos.y + VM_TITLEBAR_H + 10.f;
    float btnW = VM_DETAIL_W - 28.f;

    const VehicleDef* def = _defs.empty() ? nullptr : &_defs[0];
    char buyLbl[96];
    if (def)
        std::snprintf(buyLbl, sizeof(buyLbl), "Buy bus ($%d, %d seats)",
                      def->cost, def->passengerCapacity);
    else
        std::snprintf(buyLbl, sizeof(buyLbl), "Buy bus");

    auto drawBtn = [&](const std::string& label, const std::string& id) {
        sf::FloatRect rect({x, y}, {btnW, 30.f});
        bool hover = rect.contains(_mousePos);
        sf::RectangleShape b({btnW, 30.f});
        b.setPosition({x, y});
        b.setFillColor(hover ? sf::Color(240, 140, 30) : sf::Color(200, 100, 0));
        w.draw(b);
        sf::Text t(_font, label, VM_FONT);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Style::Bold);
        sf::FloatRect tb = t.getLocalBounds();
        t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
        t.setPosition({x + btnW / 2.f, y + 15.f});
        w.draw(t);
        addHit(x, y, btnW, 30.f, id);
        y += 36.f;
    };

    drawBtn(buyLbl, "buy_bus");

    int idx = 0;
    for (size_t i = 0; i < _buses.size(); ++i) {
        const auto& bus = _buses[i];
        if (bus.depotGridX != _depotGridX || bus.depotGridY != _depotGridY) continue;

        char lbl[128];
        if (bus.routeActive)
            std::snprintf(lbl, sizeof(lbl), "Bus %d: route set  [Edit route]", idx + 1);
        else
            std::snprintf(lbl, sizeof(lbl), "Bus %d: no route  [Set route]", idx + 1);

        drawBtn(lbl, "route_" + std::to_string(static_cast<int>(i)));
        ++idx;
    }

    if (_routeBusIdx >= 0) {
        sf::Text hint(_font,
            _routePickStep == 0
                ? "Click FIRST bus stop on the map..."
                : "Click SECOND bus stop on the map...",
            VM_FONT - 1);
        hint.setFillColor(sf::Color(255, 160, 30));
        hint.setPosition({x, y + 4.f});
        w.draw(hint);
    }
}

const char* VehicleManager::pickBusTextureId(int pdx, int pdy, int dx, int dy, float frac) {
    const bool turning = (pdx != dx || pdy != dy) && (pdx != 0 || pdy != 0) && (dx != 0 || dy != 0);
    if (turning && frac < 0.42f) {
        if ((pdx == 1 && dy == 1) || (pdy == 1 && dx == 1))   return "bus_1e";
        if ((pdx == 1 && dy == -1) || (pdy == -1 && dx == 1)) return "bus_1f";
        if ((pdx == -1 && dy == 1) || (pdy == 1 && dx == -1)) return "bus_1g";
        if ((pdx == -1 && dy == -1) || (pdy == -1 && dx == -1)) return "bus_1h";
        return "bus_1h";
    }
    if (dx > 0)       return "bus_1";
    if (dx < 0)       return "bus_1c";
    if (dy > 0)       return "bus_1b";
    if (dy < 0)       return "bus_1d";
    return "bus_1";
}

void VehicleManager::drawBusSprite(sf::RenderWindow& w, sf::Vector2f pos,
                                   int pdx, int pdy, int dx, int dy, float frac) const {
    if (_hasBusTexture) {
        const char* texId = pickBusTextureId(pdx, pdy, dx, dy, frac);
        auto it = _busTextures.find(texId);
        if (it == _busTextures.end()) it = _busTextures.begin();
        sf::Sprite spr(it->second);
        auto ts = it->second.getSize();
        spr.setOrigin({static_cast<float>(ts.x) / 2.f, static_cast<float>(ts.y)});
        spr.setPosition(pos);
        w.draw(spr);
    } else {
        sf::CircleShape c(8.f, 6);
        c.setFillColor(sf::Color(255, 200, 40));
        c.setOutlineColor(sf::Color::Black);
        c.setOutlineThickness(1.f);
        c.setOrigin({8.f, 8.f});
        c.setPosition(pos);
        w.draw(c);
    }
}

void VehicleManager::drawWorld(sf::RenderWindow& w, float stepX, float stepY) {
    for (const auto& bus : _buses) {
        if (!bus.routeActive || bus.loopPath.size() < 2) continue;

        float len = static_cast<float>(bus.loopPath.size() - 1);
        float t = bus.pathProgress;
        while (t >= len) t -= len;
        while (t < 0.f) t += len;

        size_t seg = static_cast<size_t>(t);
        if (seg >= bus.loopPath.size() - 1) seg = bus.loopPath.size() - 2;
        float frac = t - static_cast<float>(seg);

        auto [x0, y0] = bus.loopPath[seg];
        auto [x1, y1] = bus.loopPath[seg + 1];

        int pdx = 0, pdy = 0;
        if (seg > 0) {
            auto [px0, py0] = bus.loopPath[seg - 1];
            pdx = x0 - px0;
            pdy = y0 - py0;
        } else if (bus.loopPath.size() > 2) {
            auto [px0, py0] = bus.loopPath[bus.loopPath.size() - 2];
            pdx = x0 - px0;
            pdy = y0 - py0;
        }
        sf::Vector2f p0 = gridToWorld(x0, y0, stepX, stepY);
        sf::Vector2f p1 = gridToWorld(x1, y1, stepX, stepY);
        sf::Vector2f pos = p0 + (p1 - p0) * frac;

        drawBusSprite(w, pos, pdx, pdy, x1 - x0, y1 - y0, frac);
    }
}

void VehicleManager::update(float dt, float stepX, float stepY, int timeMultiplier, bool paused,
                            RoadManager& rm, CitizenManager* cm, const IndustrialManager* im)
{
    (void)stepX;
    (void)stepY;
    if (paused) return;

    const float speed = 4.f * static_cast<float>(std::max(1, timeMultiplier));
    const VehicleDef* def = _defs.empty() ? nullptr : &_defs[0];
    int capacity = def ? def->passengerCapacity : 150;

    for (auto& bus : _buses) {
        if (!bus.routeActive || bus.loopPath.size() < 2) continue;

        float len = static_cast<float>(bus.loopPath.size() - 1);
        int prevTile = static_cast<int>(bus.pathProgress) % static_cast<int>(bus.loopPath.size());

        bus.pathProgress += speed * dt;
        while (bus.pathProgress >= len) bus.pathProgress -= len;

        int curTile = static_cast<int>(bus.pathProgress) % static_cast<int>(bus.loopPath.size());
        if (cm && curTile != prevTile) {
            bus.prevPathTile = curTile;
            auto [sx, sy] = bus.loopPath[curTile];
            if (rm.isBusStopAt(sx, sy) ||
                (curTile > 0 && rm.isBusStopAt(bus.loopPath[curTile - 1].first,
                                                bus.loopPath[curTile - 1].second)))
                if (im)
                    cm->onBusAtStop(bus, sx, sy, prevTile, capacity, rm, *im, stepX, stepY);
        }
    }
}

void VehicleManager::save(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return;
    f << _buses.size() << "\n";
    for (const auto& b : _buses) {
        f << b.depotGridX << " " << b.depotGridY << " " << b.vehicleId << " "
          << b.stopAGridX << " " << b.stopAGridY << " "
          << b.stopBGridX << " " << b.stopBGridY << " "
          << b.pathProgress << " " << (b.routeActive ? 1 : 0) << " "
          << b.passengersToWork << " " << b.passengersToHome << "\n";
    }
}

void VehicleManager::load(const std::string& path, RoadManager& rm) {
    _buses.clear();
    std::ifstream f(path);
    if (!f.is_open()) return;

    size_t count = 0;
    if (!(f >> count)) return;
    for (size_t i = 0; i < count; ++i) {
        PlacedBus b;
        int active = 0;
        if (!(f >> b.depotGridX >> b.depotGridY >> b.vehicleId
                  >> b.stopAGridX >> b.stopAGridY
                  >> b.stopBGridX >> b.stopBGridY
                  >> b.pathProgress >> active))
            break;
        b.routeActive = (active != 0);
        if (!(f >> b.passengersToWork >> b.passengersToHome)) {
            b.passengersToWork = 0;
            b.passengersToHome = 0;
        }
        if (b.stopAGridX >= 0 && b.stopBGridX >= 0)
            rebuildBusLoop(b, rm);
        _buses.push_back(b);
    }
}

void VehicleManager::clear() {
    _buses.clear();
    _depotOpen = false;
    _routeBusIdx = -1;
    _routePickStep = 0;
}
