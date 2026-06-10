#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "IndustrialDef.hpp"
#include "WorldGen.hpp"
#include "DetailPanelUtil.hpp"

class IndustryWorkers;

constexpr float IM_WIN_W        = 605.f;
constexpr float IM_WIN_H        = 470.f;
constexpr float IM_DETAIL_W     = 420.f;
constexpr float IM_DETAIL_MIN_H = 200.f;

constexpr float IM_TITLEBAR_H   = 32.f;
constexpr float IM_LIST_W       = 245.f;
constexpr float IM_SCROLL_W     = 14.f;

constexpr float IM_PREVIEW_W    = IM_WIN_W - IM_LIST_W - IM_SCROLL_W - 12.f;

constexpr float IM_INFO_H       = 160.f;
constexpr float IM_BTN_H        = 36.f;
constexpr float IM_LIST_H       = IM_WIN_H - IM_TITLEBAR_H - IM_INFO_H - IM_BTN_H - 10.f;

constexpr float IM_VARIANT_SIZE = 120.f;
constexpr float IM_VARIANT_PAD  = 8.f;
constexpr unsigned int IM_FONT_SIZE = 14;

enum class IndPlacingState { NONE, AWAITING_START, DRAGGING, SINGLE };

class RoadManager;
class BuildingManager;

class IndustrialManager {
public:
    IndustrialManager(const sf::Font& font, const sf::Texture& closeTexture, const sf::Texture& hlTexture);

    void loadBuildings(const std::string& dataFilePath,
                       const std::string& texturesDir,
                       std::function<void(const std::string&)> onError = nullptr,
                       bool clearFirst = true);

    bool isOpen() const { return _open; }
    void open(sf::Vector2f pos) {
        _open = true;
        _winPos = pos;
        _closeBtn.setPosition({_winPos.x + IM_WIN_W - 30.f, _winPos.y + 3.f});
    }
    void close() { _open = false; _placingState = IndPlacingState::NONE; }

    bool isPlacing() const { return _placingState != IndPlacingState::NONE; }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);

    void simulatePlaceButtonClick();
    void cycleVariation();

    bool update(sf::Vector2f worldMousePos, float stepX, float stepY,
                int& outGridX, int& outGridY, long long currentCash,
                const RoadManager* rm, const BuildingManager* bm,
                const GeneratedWorld* world = nullptr);

    int confirmPlace(sf::Vector2f worldPos, float stepX, float stepY,
                     long long currentCash,
                     const RoadManager* rm, const BuildingManager* bm,
                     const GeneratedWorld* world = nullptr);

    void drawUI(sf::RenderWindow& window);
    void drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY, const RoadManager* rm, const BuildingManager* bm, const GeneratedWorld* world = nullptr);

    void save(const std::string& filePath) const;
    void load(const std::string& filePath,
              std::function<void(const std::string&)> onError = nullptr);

    const std::vector<PlacedIndustrial>& getPlaced() const { return _placed; }

    bool findAt(int gx, int gy, int& outIdx) const;
    void getGridSize(const std::string& defId, int& outW, int& outH) const;
    std::string getDisplayName(const std::string& defId) const;

    bool isDetailOpen() const { return _detailOpen; }
    bool tryOpenDetailAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                              sf::RenderWindow& window, const sf::View& gameView);
    void closeDetail();
    bool handleDetailEvent(const sf::Event& event, sf::RenderWindow& window);
    void drawDetailPanel(sf::RenderWindow& w, const BuildingManager* bm,
                         const IndustryWorkers* iw, bool powerShortage = false);

    bool removeAt(int gx, int gy);
    void clear();
    bool hasCollision(int gx, int gy, int w, int h) const;

    static sf::Vector2f gridToWorld(int gx, int gy, float stepX, float stepY);
    static sf::Vector2i worldToGrid(sf::Vector2f world, float stepX, float stepY);

private:
    std::vector<IndustrialDef>        _defs;
    std::map<std::string, size_t>     _defIndex;
    std::vector<PlacedIndustrial>     _placed;

    bool  _open    = false;
    bool  _detailOpen = false;
    int   _detailIdx = -1;
    sf::Vector2f _detailPos;
    float        _detailH = IM_DETAIL_MIN_H;
    sf::FloatRect _detailCloseRect;
    IndPlacingState _placingState = IndPlacingState::NONE;
    int   _selDef  = -1;
    int   _selVar  = -1;

    sf::Vector2f _winPos;
    int   _scrollOffset = 0;
    int   _dragStartGX   = 0;
    int   _dragStartGY   = 0;
    int   _dragCurrentGX = 0;
    int   _dragCurrentGY = 0;

    std::vector<std::pair<int,int>> _ghostPositions;
    void buildGhostPositions();

    sf::Sprite   _ghostSprite;
    int          _ghostGridX = 0;
    int          _ghostGridY = 0;
    bool         _ghostValid = false;

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

    bool isOccupied(int gx, int gy, int w, int h,
                    const RoadManager* rm, const BuildingManager* bm,
                    const GeneratedWorld* world) const;

    void drawWindow(sf::RenderWindow& w);
    void drawList(sf::RenderWindow& w);
    void drawVariants(sf::RenderWindow& w);
    void drawInfo(sf::RenderWindow& w);
    void drawPlaceButton(sf::RenderWindow& w);
};
