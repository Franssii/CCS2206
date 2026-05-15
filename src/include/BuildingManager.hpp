#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "BuildingDef.hpp"
#include "WorldGen.hpp"

constexpr float BM_WIN_W = 605.f;
constexpr float BM_WIN_H = 470.f;

constexpr float BM_TITLEBAR_H = 32.f;
constexpr float BM_LIST_W = 245.f;
constexpr float BM_SCROLL_W = 14.f;

constexpr float BM_PREVIEW_W = BM_WIN_W - BM_LIST_W - BM_SCROLL_W - 12.f;

constexpr float BM_INFO_H = 160.f;
constexpr float BM_BTN_H = 36.f;
constexpr float BM_LIST_H = BM_WIN_H - BM_TITLEBAR_H - BM_INFO_H - BM_BTN_H - 10.f;

constexpr float BM_VARIANT_SIZE = 120.f;
constexpr float BM_VARIANT_PAD = 8.f;
constexpr unsigned int BM_FONT_SIZE = 14;

class RoadManager;

class BuildingManager {
public:
    BuildingManager(const sf::Font& font, const sf::Texture& closeTexture, const sf::Texture& hlTexture);

    void loadHouses(const std::string& dataFilePath,
                    const std::string& texturesDir,
                    std::function<void(const std::string&)> onError = nullptr);

    bool isOpen() const { return _open; }
    void open(sf::Vector2f pos) { 
        _open = true; 
        _winPos = pos; 
        _closeBtn.setPosition({_winPos.x + BM_WIN_W - 30.f, _winPos.y + 3.f});
    }
    void close()        { _open = false; _placing = false; }

    bool isPlacing() const { return _placing; }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);

    bool update(sf::Vector2f worldMousePos, float stepX, float stepY,
                int& outGridX, int& outGridY, long long currentCash, const RoadManager* rm, const GeneratedWorld* world = nullptr);

    void simulatePlaceButtonClick();
    void cycleVariation();

    int confirmPlace(sf::Vector2f worldPos, float stepX, float stepY, long long currentCash, const RoadManager* rm, const GeneratedWorld* world = nullptr);

    void drawUI(sf::RenderWindow& window);

    void drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY);

    void save(const std::string& filePath) const;
    void load(const std::string& filePath,
              std::function<void(const std::string&)> onError = nullptr);

    const std::vector<PlacedBuilding>& getPlaced() const { return _placed; }

    bool removeAt(int gx, int gy);
    void clear() { _placed.clear(); }

    bool hasCollision(int gx, int gy, int w, int h) const;

    static sf::Vector2f gridToWorld(int gx, int gy, float stepX, float stepY);
    static sf::Vector2i worldToGrid(sf::Vector2f world, float stepX, float stepY);

private:
    std::vector<BuildingDef>             _defs;
    std::map<std::string, size_t>        _defIndex;
    std::vector<PlacedBuilding>          _placed;

    bool  _open    = false;
    bool  _placing = false;
    int   _selDef  = -1;
    int   _selVar  = -1;

    sf::Vector2f _winPos;
    int   _scrollOffset = 0;
    bool  _draggingScroll = false;
    float _scrollDragStartY = 0.f;
    int   _scrollDragStartOffset = 0;

    sf::Sprite   _ghostSprite;
    int          _ghostGridX = 0;
    int          _ghostGridY = 0;
    bool         _ghostValid = false;

    const sf::Font&    _font;
    sf::Sprite         _closeBtn;
    const sf::Texture& _hlTexture;
    sf::Sprite         _hlSprite;

    void rebuildUI();

    void drawWindow(sf::RenderWindow& w);
    void drawList(sf::RenderWindow& w);
    void drawVariants(sf::RenderWindow& w);
    void drawInfo(sf::RenderWindow& w);
    void drawPlaceButton(sf::RenderWindow& w);

    std::string _texturesDir;

    struct FloatingText {
        sf::Text text;
        int frames;
    };
    std::vector<FloatingText> _floatingTexts;

    bool isOccupied(int gx, int gy, int w, int h, const RoadManager* rm, const GeneratedWorld* world) const;
    bool pointInRect(sf::Vector2f p, sf::FloatRect r) const;
};
