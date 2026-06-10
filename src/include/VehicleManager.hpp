#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

class RoadManager;
class CitizenManager;
class IndustrialManager;

struct VehicleDef {
    std::string id;
    std::string displayName;
    std::string description;
    int         cost = 0;
    int         passengerCapacity = 150;
};

struct PlacedBus {
    int depotGridX = 0;
    int depotGridY = 0;
    std::string vehicleId = "Bus_1";
    int stopAGridX = -1;
    int stopAGridY = -1;
    int stopBGridX = -1;
    int stopBGridY = -1;
    std::vector<std::pair<int,int>> loopPath;
    int   abPathNodes = 0;
    float pathProgress = 0.f;
    int   prevPathTile = -1;
    int   passengersToWork = 0;
    int   passengersToHome = 0;
    bool  routeActive = false;
};

constexpr float VM_DETAIL_W     = 420.f;
constexpr float VM_DETAIL_MIN_H = 210.f;
constexpr float VM_TITLEBAR_H   = 32.f;
constexpr unsigned int VM_FONT  = 14;

class VehicleManager {
public:
    VehicleManager(const sf::Font& font, const sf::Texture& closeTexture);

    bool loadDefs(const std::string& dataFilePath);
    void loadBusSprites(const std::string& texturesDir);

    bool isDepotOpen() const { return _depotOpen; }
    bool isPickingRoute() const { return _routeBusIdx >= 0; }

    bool tryOpenDepotAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                             const RoadManager& rm, sf::RenderWindow& window,
                             const sf::View& gameView);
    void closeDepot(RoadManager& rm);

    bool handleDepotEvent(const sf::Event& event, sf::RenderWindow& window,
                          long long& cash, RoadManager& rm);
    bool handleRouteStopClick(int gridX, int gridY, RoadManager& rm);
    void drawDepotPanel(sf::RenderWindow& window);
    void drawWorld(sf::RenderWindow& window, float stepX, float stepY);

    void update(float dt, float stepX, float stepY, int timeMultiplier, bool paused,
                RoadManager& rm, CitizenManager* cm, const IndustrialManager* im);

    void save(const std::string& path) const;
    void load(const std::string& path, RoadManager& rm);
    void clear();

    const std::vector<PlacedBus>& getBuses() const { return _buses; }

private:
    std::vector<VehicleDef> _defs;
    std::vector<PlacedBus>  _buses;
    std::map<std::string, sf::Texture> _busTextures;
    bool _hasBusTexture = false;

    bool  _depotOpen = false;
    int   _depotGridX = 0;
    int   _depotGridY = 0;
    sf::Vector2f _depotWorldPos;
    sf::Vector2f _panelPos;
    float        _panelH = VM_DETAIL_MIN_H;
    sf::FloatRect _closeRect;
    sf::Vector2f _mousePos;

    int  _routeBusIdx = -1;
    int  _routePickStep = 0;
    int  _pendingStopAX = -1;
    int  _pendingStopAY = -1;

    const sf::Font& _font;
    sf::Sprite      _closeBtn;

    struct HitBox { sf::FloatRect rect; std::string id; };
    std::vector<HitBox> _hits;

    void addHit(float x, float y, float w, float h, const std::string& id);
    const HitBox* hitAt(sf::Vector2f p) const;
    float computePanelHeight() const;
    void rebuildBusLoop(PlacedBus& bus, RoadManager& rm);
    void drawBusSprite(sf::RenderWindow& w, sf::Vector2f pos,
                       int pdx, int pdy, int dx, int dy, float frac) const;
    static const char* pickBusTextureId(int pdx, int pdy, int dx, int dy, float frac);
    static sf::Vector2f gridToWorld(int gx, int gy, float stepX, float stepY);
};
