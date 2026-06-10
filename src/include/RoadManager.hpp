#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "RoadDef.hpp"
#include "BuildingDef.hpp"
#include "WorldGen.hpp"
#include "DetailPanelUtil.hpp"

class IndustrialManager;

constexpr float RM_WIN_W        = 605.f;
constexpr float RM_WIN_H        = 470.f;

constexpr float RM_TITLEBAR_H   = 32.f;
constexpr float RM_LIST_W       = 245.f;
constexpr float RM_SCROLL_W     = 14.f;

constexpr float RM_PREVIEW_W    = RM_WIN_W - RM_LIST_W - RM_SCROLL_W - 12.f;

constexpr float RM_INFO_H       = 160.f;
constexpr float RM_BTN_H        = 36.f;
constexpr float RM_LIST_H       = RM_WIN_H - RM_TITLEBAR_H - RM_INFO_H - RM_BTN_H - 10.f;

constexpr float RM_VARIANT_SIZE = 120.f;
constexpr float RM_VARIANT_PAD  = 8.f;
constexpr unsigned int RM_FONT_SIZE = 14;

constexpr float RM_STOP_DETAIL_W = 400.f;
constexpr float RM_STOP_DETAIL_MIN_H = 220.f;

enum class RoadPlacingState { NONE, AWAITING_START, DRAGGING, SINGLE };

class BuildingManager;
class CitizenManager;
class IndustrialManager;
class IndustryWorkers;

class RoadManager {
public:
    RoadManager(const sf::Font& font, const sf::Texture& closeTexture, const sf::Texture& hlTexture);

    void loadRoads(const std::string& dataFilePath,
                   const std::string& texturesDir,
                   std::function<void(const std::string&)> onError = nullptr);

    bool isOpen()    const { return _open; }
    void open(sf::Vector2f pos) { 
        _open = true; 
        _winPos = pos; 
        _closeBtn.setPosition({_winPos.x + RM_WIN_W - DP_CLOSE_SZ - 8.f,
                               _winPos.y + (RM_TITLEBAR_H - DP_CLOSE_SZ) * 0.5f});
    }
    void close()           { _open = false; _placingState = RoadPlacingState::NONE; }

    bool isPlacing() const { return _placingState != RoadPlacingState::NONE; }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);

    void simulatePlaceButtonClick();
    void cycleVariation();

    int  handleWorldClick(sf::Vector2f worldPos, float stepX, float stepY, long long currentCash, const BuildingManager* bm, const GeneratedWorld* world = nullptr);

    void update(sf::Vector2f worldMousePos, float stepX, float stepY, long long currentCash, const BuildingManager* bm, const GeneratedWorld* world = nullptr);

    void drawUI(sf::RenderWindow& window);
    void drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY, const BuildingManager* bm, const GeneratedWorld* world = nullptr);

    void save(const std::string& filePath) const;
    void load(const std::string& filePath,
              std::function<void(const std::string&)> onError = nullptr);

    const std::vector<PlacedRoad>& getPlaced() const { return _placed; }

    bool removeAt(int gx, int gy);
    void clear() { _placed.clear(); }
    bool hasCollision(int gx, int gy, int w, int h) const;

    static sf::Vector2f gridToWorld(int gx, int gy, float stepX, float stepY);
    static sf::Vector2i worldToGrid(sf::Vector2f world, float stepX, float stepY);

    bool isRoadAt(int gx, int gy) const;
    bool getRoadDefIdAt(int gx, int gy, std::string& outDefId) const;
    bool isBusStopAt(int gx, int gy) const;
    bool isVehicleStationAt(int gx, int gy) const;
    std::vector<std::pair<int,int>> findRoadPath(int fromX, int fromY, int toX, int toY,
                                                 int maxSteps = 500) const;
    bool hasRoadAdjacentToRect(int gx, int gy, int w, int h) const;
    std::vector<int> findReachableIndustrialIndices(int houseGx, int houseGy, int houseW, int houseH,
                                                    const IndustrialManager& im,
                                                    int maxSteps = 20) const;
    std::vector<std::pair<int,int>> findReachableBusStops(int houseGx, int houseGy, int houseW, int houseH,
                                                          int maxSteps = 20) const;
    void setHighlightBusStops(bool on) { _highlightBusStops = on; }
    void drawBusStopHighlights(sf::RenderWindow& w, float stepX, float stepY);
    bool findBusStopAt(int gx, int gy, int& outX, int& outY) const;
    bool findNearestBusStop(int gx, int gy, int w, int h,
                            int& outX, int& outY, int maxSteps = 80) const;

    bool isStopDetailOpen() const { return _stopDetailOpen; }
    bool tryOpenStopDetailAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                                  sf::RenderWindow& window, const sf::View& gameView);
    void closeStopDetail();
    bool handleStopDetailEvent(const sf::Event& event, sf::RenderWindow& window,
                               CitizenManager* cm, const IndustrialManager* im,
                               const IndustryWorkers* iw, BuildingManager* bm);
    void drawStopDetailPanel(sf::RenderWindow& window, const CitizenManager* cm,
                             const IndustrialManager* im, const IndustryWorkers* iw,
                             const BuildingManager* bm);

    void setWorkplaceHighlights(const std::vector<int>& indices) { _highlightIndIndices = indices; }
    void clearWorkplaceHighlights() { _highlightIndIndices.clear(); _highlightAssignBusStops.clear(); }
    void setAssignBusStopHighlights(const std::vector<std::pair<int,int>>& stops) {
        _highlightAssignBusStops = stops;
    }
    void drawWorkplaceHighlights(sf::RenderWindow& w, float stepX, float stepY,
                                 const IndustrialManager& im);

private:
    std::vector<RoadDef>          _defs;
    std::map<std::string, size_t> _defIndex;
    std::vector<PlacedRoad>       _placed;

    bool               _open         = false;
    RoadPlacingState   _placingState  = RoadPlacingState::NONE;
    int                _selDef        = -1;
    int                _selVar        = -1;

    sf::Vector2f _winPos;
    int   _scrollOffset       = 0;
    int   _dragStartGX        = 0;
    int   _dragStartGY        = 0;
    int   _dragCurrentGX      = 0;
    int   _dragCurrentGY      = 0;

    std::vector<std::pair<int,int>> _ghostPositions;

    sf::Sprite   _ghostSprite;
    bool         _ghostValid  = false;

    const sf::Font&    _font;
    sf::Sprite         _closeBtn;
    const sf::Texture& _hlTexture;
    sf::Sprite         _hlSprite;

    std::string _texturesDir;

    struct FloatingText {
        sf::Text text;
        int frames;
    };
    std::vector<FloatingText> _floatingTexts;
    std::vector<int> _highlightIndIndices;
    std::vector<std::pair<int,int>> _highlightAssignBusStops;
    bool _highlightBusStops = false;

    bool  _stopDetailOpen = false;
    int   _stopDetailGX = 0;
    int   _stopDetailGY = 0;
    sf::Vector2f _stopDetailPos;
    sf::FloatRect _stopDetailCloseRect;
    sf::Vector2f _stopDetailMousePos;
    float _stopDetailH = RM_STOP_DETAIL_MIN_H;
    std::vector<std::pair<sf::FloatRect, std::string>> _stopDetailHits;

    float computeStopDetailHeight(const IndustrialManager* im, const IndustryWorkers* iw) const;
    bool isOccupied(int gx, int gy, int w, int h, const BuildingManager* bm, const GeneratedWorld* world) const;

    void buildGhostPositions();
    int  placeSingleRoad(int gx, int gy, float stepX, float stepY, long long currentCash);

    void drawWindow(sf::RenderWindow& w);
    void drawList(sf::RenderWindow& w);
    void drawVariants(sf::RenderWindow& w);
    void drawInfo(sf::RenderWindow& w);
    void drawPlaceButton(sf::RenderWindow& w);
};
