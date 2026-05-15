#include "include/WorldGenMenu.hpp"
#include <cstdlib>
#include <algorithm>
#include <random>

WorldGenMenu::WorldGenMenu(const sf::Font& font, const sf::Texture& bgTexture, sf::Vector2u windowSize)
    : _font(font), _bg(bgTexture)
{
    float winH = TITLE_H + ROW_H * 7.5f + BTN_H + PAD * 3;
    _winX = (windowSize.x - WIN_W) / 2.f;
    _winY = (windowSize.y - winH) / 2.f;

    sf::Vector2u ts = bgTexture.getSize();
    _bg.setPosition({0.f, 0.f});
    if (ts.x > 0 && ts.y > 0) {
        _bg.setScale({static_cast<float>(windowSize.x) / ts.x, static_cast<float>(windowSize.y) / ts.y});
    }

    buildRows();

    std::random_device rd;
    _settings.seed = rd();
    _seedInput = std::to_string(_settings.seed);
}

void WorldGenMenu::buildRows() {
    _rows.clear();

    OptionRow r0;
    r0.label    = "Map Size";
    r0.choices  = {"Small", "Medium", "Large", "Very Large"};
    r0.selected = 1;
    _rows.push_back(r0);

    OptionRow r1;
    r1.label    = "Rivers";
    r1.choices  = {"1", "2", "3"};
    r1.selected = 0;
    _rows.push_back(r1);

    OptionRow r2;
    r2.label    = "Ore Frequency";
    r2.choices  = {"Rare", "Common", "Frequent"};
    r2.selected = 1;
    _rows.push_back(r2);

    OptionRow r3;
    r3.label    = "Difficulty";
    r3.choices  = {"Easy", "Normal", "Hard"};
    r3.selected = 1;
    _rows.push_back(r3);

    OptionRow r4;
    r4.label    = "Shape Randomness";
    r4.choices  = {"Low", "Normal", "High"};
    r4.selected = 1;
    _rows.push_back(r4);
}

void WorldGenMenu::applyRows() {
    switch (_rows[0].selected) {
        case 0: _settings.mapSize = MapSize::SMALL;      break;
        case 1: _settings.mapSize = MapSize::MEDIUM;     break;
        case 2: _settings.mapSize = MapSize::LARGE;      break;
        case 3: _settings.mapSize = MapSize::VERY_LARGE; break;
    }
    switch (_rows[1].selected) {
        case 0: _settings.riverCount = RiverCount::ONE;   break;
        case 1: _settings.riverCount = RiverCount::TWO;   break;
        case 2: _settings.riverCount = RiverCount::THREE; break;
    }
    switch (_rows[2].selected) {
        case 0: _settings.oreFreq = OreFreq::RARE;     break;
        case 1: _settings.oreFreq = OreFreq::COMMON;   break;
        case 2: _settings.oreFreq = OreFreq::FREQUENT; break;
    }
    switch (_rows[3].selected) {
        case 0: _settings.difficulty = Difficulty::EASY;   break;
        case 1: _settings.difficulty = Difficulty::NORMAL; break;
        case 2: _settings.difficulty = Difficulty::HARD;   break;
    }
    switch (_rows[4].selected) {
        case 0: _settings.shapeRandomness = ShapeRandomness::LOW;    break;
        case 1: _settings.shapeRandomness = ShapeRandomness::NORMAL; break;
        case 2: _settings.shapeRandomness = ShapeRandomness::HIGH;   break;
    }
    if (!_seedInput.empty()) {
        try { _settings.seed = static_cast<unsigned int>(std::stoul(_seedInput)); }
        catch (...) {}
    }
}

void WorldGenMenu::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (!_open) return;

    sf::Vector2f mp = window.mapPixelToCoords(sf::Mouse::getPosition(window),
                                               window.getDefaultView());

    float winH  = TITLE_H + ROW_H * static_cast<float>(_rows.size()) + ROW_H + BTN_H + PAD * 3;
    float curY  = _winY + TITLE_H + PAD;

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            _seedFocused = false;

            for (auto& row : _rows) {
                float btnW = (WIN_W - PAD * 2) / static_cast<float>(row.choices.size());
                for (int i = 0; i < static_cast<int>(row.choices.size()); ++i) {
                    sf::FloatRect r(
                        {_winX + PAD + i * btnW, curY + ROW_H * 0.45f},
                        {btnW - 4.f, ROW_H * 0.45f});
                    if (r.contains(mp)) row.selected = i;
                }
                curY += ROW_H;
            }

            sf::FloatRect seedBox({_winX + PAD, curY + ROW_H * 0.45f},
                                  {WIN_W - PAD * 2, ROW_H * 0.45f});
            if (seedBox.contains(mp)) _seedFocused = true;
            curY += ROW_H;

            float btnY = curY + PAD;
            sf::FloatRect startBtn({_winX + WIN_W * 0.3f, btnY},
                                   {WIN_W * 0.4f, BTN_H});
            if (startBtn.contains(mp)) {
                applyRows();
                _startPressed = true;
            }
        }
    }

    if (_seedFocused) {
        if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
            if (te->unicode == 8 && !_seedInput.empty()) {
                _seedInput.pop_back();
            } else if (te->unicode >= '0' && te->unicode <= '9' && _seedInput.size() < 10) {
                _seedInput += static_cast<char>(te->unicode);
            }
        }
    }
}

void WorldGenMenu::drawRow(sf::RenderWindow& w, const OptionRow& row, float winX, float winW) {
    float labelSize = 22.f;
    sf::Text label(_font, row.label + ":", static_cast<unsigned int>(labelSize));
    label.setFillColor(sf::Color::White);
    label.setStyle(sf::Text::Style::Bold);
    label.setPosition({winX + PAD, row.y});
    w.draw(label);

    float btnW = (winW - PAD * 2) / static_cast<float>(row.choices.size());
    for (int i = 0; i < static_cast<int>(row.choices.size()); ++i) {
        float bx = winX + PAD + i * btnW;
        float by = row.y + ROW_H * 0.45f;

        sf::RectangleShape btn({btnW - 4.f, ROW_H * 0.45f});
        btn.setPosition({bx, by});
        bool sel = (row.selected == i);
        btn.setFillColor(sel ? sf::Color(180, 40, 40) : sf::Color(40, 40, 40));
        btn.setOutlineColor(sel ? sf::Color(255, 80, 80) : sf::Color(100, 100, 100));
        btn.setOutlineThickness(1.f);
        w.draw(btn);

        // TUTAJ ZMIEN ROZMIAR CZCIONKI NA GUZIKACH
        unsigned int fontSize = std::max(7u, static_cast<unsigned int>(btnW / 11.f));
        sf::Text t(_font, row.choices[i], fontSize);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Style::Bold);
        sf::FloatRect tb = t.getLocalBounds();
        t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
        t.setPosition({bx + (btnW - 4.f) / 2.f, by + ROW_H * 0.45f / 2.f});
        w.draw(t);
    }
}

void WorldGenMenu::draw(sf::RenderWindow& window) {
    if (!_open) return;

    window.draw(_bg);

    float winH = TITLE_H + ROW_H * (static_cast<float>(_rows.size()) + 1) + BTN_H + PAD * 3;

    sf::RectangleShape bg({WIN_W, winH});
    bg.setPosition({_winX, _winY});
    bg.setFillColor(sf::Color(10, 10, 10, 230));
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    window.draw(bg);

    sf::RectangleShape titleBar({WIN_W, TITLE_H});
    titleBar.setPosition({_winX, _winY});
    titleBar.setFillColor(sf::Color(200, 100, 0));
    window.draw(titleBar);

    sf::Text title(_font, "WORLD GENERATION SETTINGS", 28);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
    title.setPosition({_winX + WIN_W / 2.f, _winY + TITLE_H / 2.f});
    window.draw(title);

    float curY = _winY + TITLE_H + PAD;
    for (auto& row : _rows) {
        row.y = curY;
        drawRow(window, row, _winX, WIN_W);
        curY += ROW_H;
    }

    sf::Text seedLabel(_font, "Map Seed:", 22);
    seedLabel.setFillColor(sf::Color::White);
    seedLabel.setStyle(sf::Text::Style::Bold);
    seedLabel.setPosition({_winX + PAD, curY});
    window.draw(seedLabel);

    sf::RectangleShape seedBox({WIN_W - PAD * 2, ROW_H * 0.45f});
    seedBox.setPosition({_winX + PAD, curY + ROW_H * 0.45f});
    seedBox.setFillColor(sf::Color(30, 30, 30));
    seedBox.setOutlineColor(_seedFocused ? sf::Color(255, 80, 80) : sf::Color(100, 100, 100));
    seedBox.setOutlineThickness(1.f);
    window.draw(seedBox);

    sf::Text seedText(_font, _seedInput + (_seedFocused ? "|" : ""), 22);
    seedText.setFillColor(sf::Color::White);
    seedText.setPosition({_winX + PAD + 6.f, curY + ROW_H * 0.45f + 3.f});
    window.draw(seedText);

    sf::Text diffNote(_font, "Easy: 2 000 000$  |  Normal: 1 000 000$  |  Hard: 400 000$  starting cash", 16);
    diffNote.setFillColor(sf::Color(180, 180, 180));
    diffNote.setPosition({_winX + PAD, curY + ROW_H * 0.95f});
    window.draw(diffNote);

    curY += ROW_H;

    float btnY = curY + PAD;
    sf::RectangleShape startBtn({WIN_W * 0.4f, BTN_H});
    startBtn.setPosition({_winX + WIN_W * 0.3f, btnY});
    startBtn.setFillColor(sf::Color(200, 100, 0));
    startBtn.setOutlineColor(sf::Color::White);
    startBtn.setOutlineThickness(2.f);
    window.draw(startBtn);

    sf::Text startText(_font, "GENERATE WORLD", 24);
    startText.setFillColor(sf::Color::White);
    startText.setStyle(sf::Text::Style::Bold);
    sf::FloatRect stb = startText.getLocalBounds();
    startText.setOrigin({stb.position.x + stb.size.x / 2.f, stb.position.y + stb.size.y / 2.f});
    startText.setPosition({_winX + WIN_W * 0.5f, btnY + BTN_H / 2.f});
    window.draw(startText);
}
