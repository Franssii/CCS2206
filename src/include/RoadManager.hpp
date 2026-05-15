#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "RoadDef.hpp"
#include "WorldGen.hpp"

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

enum class RoadPlacingState { NONE, AWAITING_START, DRAGGING, SINGLE };

class BuildingManager;

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
        _closeBtn.setPosition({_winPos.x + RM_WIN_W - 30.f, _winPos.y + 3.f});
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

    bool isOccupied(int gx, int gy, int w, int h, const BuildingManager* bm, const GeneratedWorld* world) const;

    void buildGhostPositions();
    int  placeSingleRoad(int gx, int gy, float stepX, float stepY, long long currentCash);

    void drawWindow(sf::RenderWindow& w);
    void drawList(sf::RenderWindow& w);
    void drawVariants(sf::RenderWindow& w);
    void drawInfo(sf::RenderWindow& w);
    void drawPlaceButton(sf::RenderWindow& w);
};
