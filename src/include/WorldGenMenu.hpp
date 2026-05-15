#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "WorldGen.hpp"

class WorldGenMenu {
public:
    WorldGenMenu(const sf::Font& font, const sf::Texture& bgTexture, sf::Vector2u windowSize);

    bool isOpen()  const { return _open; }
    void open()          { _open = true; }
    void close()         { _open = false; }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);
    void draw(sf::RenderWindow& window);

    bool startPressed()  const { return _startPressed; }
    void resetStart()          { _startPressed = false; }

    const WorldSettings& getSettings() const { return _settings; }

private:
    const sf::Font&    _font;
    sf::Sprite         _bg;
    bool               _open         = false;
    bool               _startPressed = false;
    WorldSettings      _settings;

    std::string        _seedInput;
    bool               _seedFocused  = false;

    struct OptionRow {
        std::string  label;
        std::vector<std::string> choices;
        int          selected = 0;
        float        y        = 0.f;
    };

    std::vector<OptionRow> _rows;

    void buildRows();
    void applyRows();
    void drawRow(sf::RenderWindow& w, const OptionRow& row, float winX, float winW);

    float _winX = 0.f;
    float _winY = 0.f;
    static constexpr float WIN_W    = 1100.f;
    static constexpr float TITLE_H  = 50.f;
    static constexpr float ROW_H    = 64.f;
    static constexpr float BTN_H    = 54.f;
    static constexpr float PAD      = 24.f;
};
