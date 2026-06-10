#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "PowerDef.hpp"
#include "WorldGen.hpp"
#include "DetailPanelUtil.hpp"

class RoadManager;
class BuildingManager;
class IndustrialManager;

constexpr float PM_WIN_W        = 605.f;
constexpr float PM_WIN_H        = 470.f;
constexpr float PM_TITLEBAR_H   = 32.f;
constexpr float PM_LIST_W       = 245.f;
constexpr float PM_SCROLL_W     = 14.f;
constexpr float PM_PREVIEW_W    = PM_WIN_W - PM_LIST_W - PM_SCROLL_W - 12.f;
constexpr float PM_INFO_H       = 160.f;
constexpr float PM_BTN_H        = 36.f;
constexpr float PM_LIST_H       = PM_WIN_H - PM_TITLEBAR_H - PM_INFO_H - PM_BTN_H - 10.f;
constexpr float PM_VARIANT_SIZE = 120.f;
constexpr float PM_VARIANT_PAD  = 8.f;
constexpr unsigned int PM_FONT_SIZE = 14;

class PowerManager {
public:
    PowerManager(const sf::Font& font, const sf::Texture& closeTexture, const sf::Texture& hlTexture);

    void loadPower(const std::string& dataFilePath,
                   const std::string& texturesDir,
                   std::function<void(const std::string&)> onError = nullptr);

    bool isOpen() const { return _open; }
    void open(sf::Vector2f pos) {
        _open = true;
        _winPos = pos;
        _closeBtn.setPosition({_winPos.x + PM_WIN_W - 30.f, _winPos.y + 3.f});
    }
    void close() { _open = false; _placing = false; }

    bool isPlacing() const { return _placing; }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);
    void simulatePlaceButtonClick();
    void cycleVariation();
    bool update(sf::Vector2f worldMousePos, float stepX, float stepY,
                int& outGridX, int& outGridY, long long currentCash,
                const RoadManager* rm, const BuildingManager* bm,
                const IndustrialManager* im, const GeneratedWorld* world = nullptr);
    int confirmPlace(sf::Vector2f worldPos, float stepX, float stepY,
                     long long currentCash, const RoadManager* rm,
                     const BuildingManager* bm, const IndustrialManager* im,
                     const GeneratedWorld* world = nullptr);

    void drawUI(sf::RenderWindow& window);
    void drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY);

    void save(const std::string& filePath) const;
    void load(const std::string& filePath,
              std::function<void(const std::string&)> onError = nullptr);

    const std::vector<PlacedPower>& getPlaced() const { return _placed; }
    int getTotalProduction() const;
    bool hasCollision(int gx, int gy, int w, int h) const;
    bool removeAt(int gx, int gy);
    void clear();

    static sf::Vector2f gridToWorld(int gx, int gy, float stepX, float stepY);
    static sf::Vector2i worldToGrid(sf::Vector2f world, float stepX, float stepY);

private:
    std::vector<PowerDef>        _defs;
    std::map<std::string, size_t> _defIndex;
    std::vector<PlacedPower>      _placed;

    bool  _open = false;
    bool  _placing = false;
    int   _selDef = -1;
    int   _selVar = -1;
    sf::Vector2f _winPos;
    int   _scrollOffset = 0;

    sf::Sprite   _ghostSprite;
    int          _ghostGridX = 0;
    int          _ghostGridY = 0;
    bool         _ghostValid = false;

    const sf::Font&    _font;
    sf::Sprite         _closeBtn;
    const sf::Texture& _hlTexture;
    sf::Sprite         _hlSprite;
    std::string _texturesDir;

    struct FloatingText { sf::Text text; int frames; };
    std::vector<FloatingText> _floatingTexts;

    bool isOccupied(int gx, int gy, int w, int h,
                    const RoadManager* rm, const BuildingManager* bm,
                    const IndustrialManager* im, const GeneratedWorld* world) const;

    void drawWindow(sf::RenderWindow& w);
    void drawList(sf::RenderWindow& w);
    void drawVariants(sf::RenderWindow& w);
    void drawInfo(sf::RenderWindow& w);
    void drawPlaceButton(sf::RenderWindow& w);
};
