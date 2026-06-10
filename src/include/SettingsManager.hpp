#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>
#include <map>
#include <vector>
#include <functional>

enum class SettingsResult {
    NONE,
    APPLY_FULLSCREEN,
    APPLY_WINDOWED,
    APPLY_AUDIO,
    CLOSE_REQUESTED
};

constexpr float SM_PANEL_X = 705.f;
constexpr float SM_PANEL_Y = 225.f;
constexpr float SM_PANEL_W = 465.f;
constexpr float SM_PANEL_H = 600.f;

constexpr int   SM_VOLUME_STEP = 5;
constexpr int   SM_VOLUME_MIN  = 0;
constexpr int   SM_VOLUME_MAX  = 100;

class SettingsManager {
public:
    explicit SettingsManager(const sf::Font& font);

    void loadLabels  (const std::string& filePath);
    void loadSettings(const std::string& filePath, const std::string& legacyFile = "");
    void saveSettings(const std::string& filePath) const;
    void resetDefaults();

    int  getMusicVolume() const { return _musicVolume; }
    int  getSoundVolume() const { return _soundVolume; }
    bool isFullscreen()   const { return _fullscreen; }
    const std::string& getVersion() const { return _version; }

    sf::Keyboard::Key getKey(const std::string& action) const;
    std::string       getLabel(const std::string& key) const;

    bool isCapturingKey() const { return !_captureAction.empty(); }
    bool consumeClick() { bool c = _clickConsumed; _clickConsumed = false; return c; }

    SettingsResult handleEvent(const sf::Event& event, sf::RenderWindow& window);
    void draw(sf::RenderWindow& window);

    static std::string       keyToString(sf::Keyboard::Key k);
    static sf::Keyboard::Key  stringToKey(const std::string& s);

private:
    const sf::Font& _font;

    std::map<std::string, std::string>          _labels;
    std::map<std::string, sf::Keyboard::Key>    _keys;

    int  _musicVolume = 50;
    int  _soundVolume = 100;
    bool _fullscreen  = true;
    std::string _version = "1.0.0.1";

    std::string _captureAction;
    bool        _clickConsumed = false;
    sf::Vector2f _mousePos;

    struct HitBox {
        sf::FloatRect rect;
        std::string   id;
    };
    mutable std::vector<HitBox> _hitBoxes;

    void drawPanel(sf::RenderWindow& w);
    void drawSectionHeader(sf::RenderWindow& w, const std::string& labelKey, float& curY);
    void drawVolumeRow(sf::RenderWindow& w, const std::string& labelKey,
                       int value, const std::string& idDec, const std::string& idInc,
                       float& curY);
    void drawFullscreenRow(sf::RenderWindow& w, float& curY);
    void drawKeyRow(sf::RenderWindow& w, const std::string& action, float& curY);
    void drawBottomButtons(sf::RenderWindow& w, float& curY);

    void addHit(float x, float y, float w, float h, const std::string& id);
    const HitBox* hitAt(sf::Vector2f p) const;

    static std::string trim(const std::string& s);
};
