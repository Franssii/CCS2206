#include "include/PowerManager.hpp"
#include "include/DetailPanelUtil.hpp"
#include "include/RoadManager.hpp"
#include "include/BuildingManager.hpp"
#include "include/IndustrialManager.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cstdint>

namespace fs = std::filesystem;

static std::vector<std::string> splitLine(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delim))
        parts.push_back(token);
    return parts;
}

static std::string trim(const std::string& s) {
    std::string res = s;
    if (res.size() >= 3 && res.substr(0, 3) == "\xEF\xBB\xBF") {
        res.erase(0, 3);
    }
    size_t a = res.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = res.find_last_not_of(" \t\r\n");
    return res.substr(a, b - a + 1);
}

static std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

PowerManager::PowerManager(const sf::Font& font,
                                 const sf::Texture& closeTexture,
                                 const sf::Texture& hlTexture)
    : _font(font),
      _closeBtn(closeTexture),
      _hlTexture(hlTexture),
      _hlSprite(hlTexture),
      _ghostSprite(hlTexture)
{
    scaleCloseSprite(_closeBtn, closeTexture);
}

void PowerManager::loadPower(const std::string& dataFilePath,
                                  const std::string& texturesDir,
                                  std::function<void(const std::string&)> onError)
{
    _texturesDir = texturesDir;
    _defs.clear();
    _defIndex.clear();

    std::ifstream file(dataFilePath);
    if (!file.is_open()) {
        if (onError) onError("ERROR, cannot open file: " + dataFilePath);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto parts = splitLine(line, '|');
        if (parts.size() < 5) continue;

        PowerDef def;
        def.id          = trim(parts[0]);
        def.displayName = trim(parts[1]);
        def.description = trim(parts[2]);
        def.cost        = std::stoi(trim(parts[3]));
        def.materials   = trim(parts[4]);

        std::vector<std::string> texFiles;
        if (def.id == "Power_pole") {
            texFiles = {"pl_1a.png", "pl_1b.png"};
        } else if (def.id == "Power_plant") {
            texFiles = {"power_stationa.png"};
            if (fs::exists(texturesDir + "power_stationb.png"))
                texFiles.push_back("power_stationb.png");
        }
        for (const auto& file : texFiles) {
            std::string path = texturesDir + file;
            if (!fs::exists(path)) {
                if (def.textures.empty())
                    std::cout << "[PowerManager] ERROR, missing texture: " << path << "\n";
                continue;
            }
            sf::Texture tex;
            if (tex.loadFromFile(path))
                def.textures.push_back(std::move(tex));
        }

        if (def.textures.empty()) continue;

        def.gridW = 1;
        def.gridH = 1;
        if (parts.size() >= 7) {
            def.gridW = std::stoi(trim(parts[5]));
            def.gridH = std::stoi(trim(parts[6]));
        }
        if (parts.size() >= 8)
            def.powerOutput = std::stoi(trim(parts[7]));

        _defIndex[def.id] = _defs.size();
        _defs.push_back(std::move(def));
    }
    std::cout << "[PowerManager] loaded " << _defs.size() << " power types.\n";
}

sf::Vector2f PowerManager::gridToWorld(int gx, int gy, float stepX, float stepY) {
    return {
        static_cast<float>(gx - gy) * stepX,
        static_cast<float>(gx + gy) * stepY
    };
}

sf::Vector2i PowerManager::worldToGrid(sf::Vector2f world, float stepX, float stepY) {
    if (stepX <= 0 || stepY <= 0) return {0, 0};
    float gxf = (world.x / stepX + world.y / stepY) / 2.f;
    float gyf = (world.y / stepY - world.x / stepX) / 2.f;
    return { static_cast<int>(std::floor(gxf)),
             static_cast<int>(std::floor(gyf)) };
}

void PowerManager::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (!_open) return;

    sf::Vector2f mp = window.mapPixelToCoords(sf::Mouse::getPosition(window),
                                               window.getDefaultView());

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {

            if (_closeBtn.getGlobalBounds().contains(mp)) {
                close();
                return;
            }

            float listX  = _winPos.x + 3.f;
            float listY  = _winPos.y + PM_TITLEBAR_H + 2.f;
            float rowH   = 22.f;
            int   visRows = static_cast<int>(PM_LIST_H / rowH);

            for (int i = 0; i < visRows; ++i) {
                int defIdx = _scrollOffset + i;
                if (defIdx >= static_cast<int>(_defs.size())) break;
                sf::FloatRect row({ listX, listY + i * rowH },
                                  { PM_LIST_W - 2.f, rowH });
                if (row.contains(mp)) {
                    _selDef = defIdx;
                    _selVar = (_defs[defIdx].textures.empty()) ? -1 : 0;
                    _placing = false;
                    break;
                }
            }

            float scrollBarX = _winPos.x + PM_LIST_W + 1.f;
            float scrollBarY = _winPos.y + PM_TITLEBAR_H + 2.f;
            sf::FloatRect scrollArea({scrollBarX, scrollBarY},
                                     {PM_SCROLL_W, PM_LIST_H});
            if (scrollArea.contains(mp)) {
                float relY = mp.y - scrollBarY;
                float ratio = relY / PM_LIST_H;
                int maxScroll = std::max(0, (int)_defs.size() - (int)(PM_LIST_H / 22.f));
                _scrollOffset = static_cast<int>(ratio * maxScroll);
                _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
            }

            if (_selDef >= 0) {
                const auto& def = _defs[_selDef];
                float vx = _winPos.x + PM_LIST_W + PM_SCROLL_W + 18.f;
                float vy = _winPos.y + PM_TITLEBAR_H + 10.f;
                int cols = std::max(1, static_cast<int>(
                    (PM_PREVIEW_W) / (PM_VARIANT_SIZE + PM_VARIANT_PAD)));

                for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
                    int col = vi % cols;
                    int row = vi / cols;
                    sf::FloatRect vRect(
                        {vx + col * (PM_VARIANT_SIZE + PM_VARIANT_PAD),
                         vy + row * (PM_VARIANT_SIZE + PM_VARIANT_PAD)},
                        {PM_VARIANT_SIZE, PM_VARIANT_SIZE});
                    if (vRect.contains(mp)) {
                        _selVar = vi;
                        break;
                    }
                }
            }

            float btnX = _winPos.x + 3.f;
            float btnY = _winPos.y + PM_TITLEBAR_H + PM_LIST_H + PM_INFO_H + 6.f;
            sf::FloatRect btnRect({btnX, btnY}, {PM_LIST_W - 6.f, PM_BTN_H});
            if (btnRect.contains(mp) && _selDef >= 0 && _selVar >= 0) {
                _placing = true;
                const sf::Texture& tex = _defs[_selDef].textures[_selVar];
                _ghostSprite = sf::Sprite(tex);
                _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
            }
        }
        else if (mb->button == sf::Mouse::Button::Right) {
            if (_placing) {
                _placing = false;
            }
        }
    }

    if (const auto* sw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float listX = _winPos.x + 3.f;
        float listY = _winPos.y + PM_TITLEBAR_H + 2.f;
        sf::FloatRect listArea({listX, listY}, {PM_LIST_W + PM_SCROLL_W, PM_LIST_H});
        if (listArea.contains(mp)) {
            int maxScroll = std::max(0, (int)_defs.size() - (int)(PM_LIST_H / 22.f));
            _scrollOffset -= static_cast<int>(sw->delta);
            _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
        }
    }
}

void PowerManager::simulatePlaceButtonClick() {
    if (_selDef >= 0 && _selVar >= 0) {
        _placing = true;
        const sf::Texture& tex = _defs[_selDef].textures[_selVar];
        _ghostSprite = sf::Sprite(tex);
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
    }
}

void PowerManager::cycleVariation() {
    if (_selDef >= 0 && !_defs[_selDef].textures.empty()) {
        _selVar++;
        if (_selVar >= static_cast<int>(_defs[_selDef].textures.size())) {
            _selVar = 0;
        }
        const sf::Texture& tex = _defs[_selDef].textures[_selVar];
        _ghostSprite = sf::Sprite(tex);
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
    }
}

bool PowerManager::hasCollision(int startX, int startY, int w, int h) const {
    for (const auto& pp : _placed) {
        auto it = _defIndex.find(pp.defId);
        int pw = 1, ph = 1;
        if (it != _defIndex.end()) {
            pw = _defs[it->second].gridW;
            ph = _defs[it->second].gridH;
        }
        if (startX < pp.gridX + pw && startX + w > pp.gridX &&
            startY < pp.gridY + ph && startY + h > pp.gridY) {
            return true;
        }
    }
    return false;
}

bool PowerManager::isOccupied(int startX, int startY, int w, int h,
                              const RoadManager* rm, const BuildingManager* bm,
                              const IndustrialManager* im, const GeneratedWorld* world) const {
    if (hasCollision(startX, startY, w, h)) return true;
    if (rm && rm->hasCollision(startX, startY, w, h)) return true;
    if (bm && bm->hasCollision(startX, startY, w, h)) return true;
    if (im && im->hasCollision(startX, startY, w, h)) return true;
    if (world) {
        for (int y = startY; y < startY + h; ++y) {
            for (int x = startX; x < startX + w; ++x) {
                TileType t = world->get(x, y);
                if (t == TileType::TREE || t == TileType::WATER || t == TileType::BARE_WATER || t == TileType::EMPTY) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool PowerManager::removeAt(int gx, int gy) {
    for (auto it = _placed.begin(); it != _placed.end(); ++it) {
        int pw = 1, ph = 1;
        auto defIt = _defIndex.find(it->defId);
        if (defIt != _defIndex.end()) {
            pw = _defs[defIt->second].gridW;
            ph = _defs[defIt->second].gridH;
        }
        if (gx >= it->gridX && gx < it->gridX + pw &&
            gy >= it->gridY && gy < it->gridY + ph) {
            _placed.erase(it);
            return true;
        }
    }
    return false;
}

bool PowerManager::update(sf::Vector2f worldMousePos, float stepX, float stepY,
                              int& outGridX, int& outGridY, long long currentCash,
                              const RoadManager* rm, const BuildingManager* bm,
                              const IndustrialManager* im, const GeneratedWorld* world)
{
    for (auto it = _floatingTexts.begin(); it != _floatingTexts.end(); ) {
        it->frames--;
        it->text.move({0.f, -1.5f});
        if (it->frames < 60) {
            sf::Color c = it->text.getFillColor();
            sf::Color oc = it->text.getOutlineColor();
            c.a = static_cast<std::uint8_t>((it->frames / 60.f) * 255);
            oc.a = c.a;
            it->text.setFillColor(c);
            it->text.setOutlineColor(oc);
        }
        if (it->frames <= 0) it = _floatingTexts.erase(it);
        else ++it;
    }

    if (!_placing || _selDef < 0 || _selVar < 0) return false;

    sf::Vector2i g = worldToGrid(worldMousePos, stepX, stepY);
    _ghostGridX = g.x;
    _ghostGridY = g.y;
    outGridX = g.x;
    outGridY = g.y;

    const PowerDef& def = _defs[_selDef];

    sf::Vector2f wp = gridToWorld(g.x, g.y, stepX, stepY);
    wp.x += (def.gridW - def.gridH) * stepX / 2.f;
    wp.y += (def.gridW + def.gridH) * stepY;

    const sf::Texture& tex = def.textures[_selVar];
    float tw = static_cast<float>(tex.getSize().x);
    float th = static_cast<float>(tex.getSize().y);
    _ghostSprite.setOrigin({tw / 2.f, th});
    _ghostSprite.setPosition(wp);
    _ghostValid = true;

    bool canPlace = (currentCash >= def.cost) &&
        !isOccupied(g.x, g.y, def.gridW, def.gridH, rm, bm, im, world);
    if (canPlace) {
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
        _hlSprite.setColor(sf::Color(0, 255, 0, 255));
    } else {
        _ghostSprite.setColor(sf::Color(255, 100, 100, 160));
        _hlSprite.setColor(sf::Color(255, 0, 0, 255));
    }

    return false;
}

int PowerManager::confirmPlace(sf::Vector2f worldPos, float stepX, float stepY, long long currentCash,
                                const RoadManager* rm, const BuildingManager* bm,
                                const IndustrialManager* im, const GeneratedWorld* world) {
    if (!_placing || _selDef < 0 || _selVar < 0) return 0;

    const PowerDef& def = _defs[_selDef];
    if (currentCash < def.cost ||
        isOccupied(_ghostGridX, _ghostGridY, def.gridW, def.gridH, rm, bm, im, world)) {
        return 0;
    }

    int cost = def.cost;

    sf::Vector2f wPos = gridToWorld(_ghostGridX, _ghostGridY, stepX, stepY);
    wPos.x += (def.gridW - def.gridH) * stepX / 2.f;
    wPos.y += (def.gridW + def.gridH) * stepY;
    PlacedPower pp(def.id, _selVar, wPos, _ghostGridX, _ghostGridY, def.textures[_selVar]);

    float tw = static_cast<float>(def.textures[_selVar].getSize().x);
    float th = static_cast<float>(def.textures[_selVar].getSize().y);
    pp.sprite.setOrigin({tw / 2.f, th});
    pp.sprite.setPosition(pp.worldPos);

    _placed.push_back(std::move(pp));

    FloatingText ft{sf::Text(_font, "-" + std::to_string(cost) + "$", 18), 120};
    ft.text.setFillColor(sf::Color::Red);
    ft.text.setOutlineColor(sf::Color::Black);
    ft.text.setOutlineThickness(1.f);
    sf::FloatRect tr = ft.text.getLocalBounds();
    ft.text.setOrigin({tr.position.x + tr.size.x / 2.f, tr.position.y + tr.size.y / 2.f});
    ft.text.setPosition({worldPos.x, worldPos.y - 100.f});
    _floatingTexts.push_back(std::move(ft));

    return cost;
}

void PowerManager::save(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f.is_open()) return;

    f << _placed.size() << "\n";
    for (const auto& pp : _placed) {
        f << pp.defId << " " << pp.variantIdx << " "
          << pp.gridX << " " << pp.gridY << " "
          << pp.worldPos.x << " " << pp.worldPos.y << "\n";
    }
}

void PowerManager::load(const std::string& filePath,
                            std::function<void(const std::string&)> onError)
{
    _placed.clear();
    std::ifstream f(filePath);
    if (!f.is_open()) return;

    size_t count = 0;
    f >> count;

    for (size_t i = 0; i < count; ++i) {
        std::string defId;
        int variantIdx, gx, gy;
        float wx, wy;
        f >> defId >> variantIdx >> gx >> gy >> wx >> wy;

        auto it = _defIndex.find(defId);
        if (it == _defIndex.end()) {
            if (onError) onError("ERROR, UNKNOWN BUILDING: " + defId);
            continue;
        }

        const PowerDef& def = _defs[it->second];
        if (variantIdx < 0 || variantIdx >= static_cast<int>(def.textures.size())) continue;

        PlacedPower pp(defId, variantIdx, {wx, wy}, gx, gy, def.textures[variantIdx]);
        float tw = static_cast<float>(def.textures[variantIdx].getSize().x);
        float th = static_cast<float>(def.textures[variantIdx].getSize().y);
        pp.sprite.setOrigin({tw / 2.f, th});
        pp.sprite.setPosition(pp.worldPos);

        _placed.push_back(std::move(pp));
    }
}

void PowerManager::drawUI(sf::RenderWindow& w) {
    if (!_open) return;
    drawWindow(w);
    drawList(w);
    drawVariants(w);
    drawInfo(w);
    drawPlaceButton(w);
    w.draw(_closeBtn);
}

void PowerManager::drawWindow(sf::RenderWindow& w) {
    sf::RectangleShape bg({PM_WIN_W, PM_WIN_H});
    bg.setPosition({_winPos.x, _winPos.y});
    bg.setFillColor(sf::Color::Black);
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({PM_WIN_W, PM_TITLEBAR_H});
    tb.setPosition({_winPos.x, _winPos.y});
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "POWER GRID   " + _texturesDir, PM_FONT_SIZE);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_winPos.x + 6.f, _winPos.y + 6.f});
    w.draw(title);

    sf::RectangleShape sep({2.f, PM_WIN_H - PM_TITLEBAR_H});
    sep.setPosition({_winPos.x + PM_LIST_W + PM_SCROLL_W + 4.f,
                     _winPos.y + PM_TITLEBAR_H});
    sep.setFillColor(sf::Color(80, 80, 80));
    w.draw(sep);

    float sepInfoY = _winPos.y + PM_TITLEBAR_H + PM_LIST_H;
    sf::RectangleShape sepH({PM_LIST_W + PM_SCROLL_W, 2.f});
    sepH.setPosition({_winPos.x + 1.f, sepInfoY});
    sepH.setFillColor(sf::Color(80, 80, 80));
    w.draw(sepH);
}

void PowerManager::drawList(sf::RenderWindow& w) {
    float listX  = _winPos.x + 4.f;
    float listY  = _winPos.y + PM_TITLEBAR_H + 4.f;
    float rowH   = 22.f;
    int   visRows = static_cast<int>(PM_LIST_H / rowH);

    for (int i = 0; i < visRows; ++i) {
        int defIdx = _scrollOffset + i;
        if (defIdx >= static_cast<int>(_defs.size())) break;

        const auto& def = _defs[defIdx];
        float ry = listY + i * rowH;

        if (defIdx == _selDef) {
            sf::RectangleShape sel({PM_LIST_W - 6.f, rowH - 2.f});
            sel.setPosition({listX - 2.f, ry + 3.f});
            sel.setFillColor(sf::Color::Transparent);
            sel.setOutlineColor(sf::Color(220, 120, 0));
            sel.setOutlineThickness(1.f);
            w.draw(sel);
        }

        std::string label = std::to_string(_scrollOffset + i + 1) + ". " + def.displayName;
        sf::Text txt(_font, label, PM_FONT_SIZE);
        txt.setFillColor(_selDef == defIdx ? sf::Color(255, 160, 30) : sf::Color::White);
        txt.setPosition({listX, ry + 3.f});
        w.draw(txt);
    }

    float scrollBarX = _winPos.x + PM_LIST_W + 2.f;
    float scrollBarY = _winPos.y + PM_TITLEBAR_H + 2.f;
    sf::RectangleShape scrollBg({PM_SCROLL_W - 2.f, PM_LIST_H - 1.f});
    scrollBg.setPosition({scrollBarX, scrollBarY});
    scrollBg.setFillColor(sf::Color(30, 30, 30));
    scrollBg.setOutlineColor(sf::Color(80, 80, 80));
    scrollBg.setOutlineThickness(1.f);
    w.draw(scrollBg);

    int total = static_cast<int>(_defs.size());
    if (total > 0) {
        float thumbH = PM_LIST_H - 1.f;
        if (total > visRows) {
            thumbH = std::max(20.f, (PM_LIST_H - 1.f) * (static_cast<float>(visRows) / total));
        }
        
        float maxScroll = std::max(1, total - visRows);
        float thumbY = scrollBarY + (_scrollOffset / static_cast<float>(maxScroll))
                       * ((PM_LIST_H - 1.f) - thumbH);
        sf::RectangleShape thumb({PM_SCROLL_W - 4.f, thumbH});
        thumb.setPosition({scrollBarX + 1.f, thumbY});
        thumb.setFillColor(sf::Color(130, 60, 0));
        w.draw(thumb);
    }
}

void PowerManager::drawVariants(sf::RenderWindow& w) {
    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    float vx = _winPos.x + PM_LIST_W + PM_SCROLL_W + 18.f;
    float vy = _winPos.y + PM_TITLEBAR_H + 10.f;
    int cols = std::max(1, static_cast<int>(
        (PM_PREVIEW_W) / (PM_VARIANT_SIZE + PM_VARIANT_PAD)));

    for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
        int col = vi % cols;
        int row = vi / cols;
        float cx = vx + col * (PM_VARIANT_SIZE + PM_VARIANT_PAD);
        float cy = vy + row * (PM_VARIANT_SIZE + PM_VARIANT_PAD);

        sf::RectangleShape tile({PM_VARIANT_SIZE, PM_VARIANT_SIZE});
        tile.setPosition({cx, cy});
        tile.setFillColor(sf::Color(15, 15, 15));
        tile.setOutlineThickness(vi == _selVar ? 1.f : 0.f);
        tile.setOutlineColor(sf::Color(220, 120, 0));
        w.draw(tile);

        sf::Sprite spr(def.textures[vi]);
        float sx = spr.getGlobalBounds().size.x;
        float sy = spr.getGlobalBounds().size.y;
        float scale = std::min((PM_VARIANT_SIZE - 8.f) / sx, (PM_VARIANT_SIZE - 8.f) / sy);
        spr.setScale({scale, scale});
        spr.setPosition({cx + (PM_VARIANT_SIZE - sx * scale) / 2.f, cy + (PM_VARIANT_SIZE - sy * scale) / 2.f});
        w.draw(spr);
    }
}

void PowerManager::drawInfo(sf::RenderWindow& w) {
    float infoX = _winPos.x + 4.f;
    float infoY = _winPos.y + PM_TITLEBAR_H + PM_LIST_H + 6.f;

    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    sf::Text nameT(_font, "Building info " + def.displayName + ":", PM_FONT_SIZE);
    nameT.setFillColor(sf::Color::White);
    nameT.setStyle(sf::Text::Style::Bold);
    nameT.setPosition({infoX, infoY});
    w.draw(nameT);

    sf::Text descT(_font, "", PM_FONT_SIZE - 1);
    descT.setFillColor(sf::Color(200, 200, 200));
    descT.setPosition({infoX, infoY + 18.f});

    std::string wrapped;
    std::string currentLine;
    std::stringstream ss(def.description);
    std::string word;
    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        descT.setString(testLine);
        if (descT.getLocalBounds().size.x > PM_LIST_W - 8.f) {
            wrapped += currentLine + "\n";
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    wrapped += currentLine;
    descT.setString(wrapped);
    w.draw(descT);

    sf::Text costT(_font, "Costs " + std::to_string(def.cost) + "$", PM_FONT_SIZE);
    costT.setFillColor(sf::Color::White);
    costT.setPosition({infoX, infoY + PM_INFO_H - 42.f});
    w.draw(costT);

    sf::Text matT(_font, "Materials: " + def.materials, PM_FONT_SIZE);
    matT.setFillColor(sf::Color::White);
    matT.setPosition({infoX, infoY + PM_INFO_H - 24.f});
    w.draw(matT);
}

void PowerManager::drawPlaceButton(sf::RenderWindow& w) {
    float btnX = _winPos.x + 4.f;
    float btnY = _winPos.y + PM_TITLEBAR_H + PM_LIST_H + PM_INFO_H + 6.f;

    bool active = (_selDef >= 0 && _selVar >= 0);
    sf::RectangleShape btn({PM_LIST_W - 8.f, PM_BTN_H});
    btn.setPosition({btnX, btnY});
    btn.setFillColor(active ? sf::Color(200, 100, 0) : sf::Color(60, 60, 60));
    w.draw(btn);

    sf::Text btnTxt(_font, "PLACE POWER", PM_FONT_SIZE + 1);
    btnTxt.setFillColor(sf::Color::White);
    btnTxt.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = btnTxt.getLocalBounds();
    btnTxt.setOrigin({tb.position.x + tb.size.x / 2.f,
                      tb.position.y + tb.size.y / 2.f});
    btnTxt.setPosition({btnX + (PM_LIST_W - 8.f) / 2.f, btnY + PM_BTN_H / 2.f});
    w.draw(btnTxt);
}

void PowerManager::drawWorldOverlay(sf::RenderWindow& w, float stepX, float stepY) {
    for (const auto& ft : _floatingTexts) {
        w.draw(ft.text);
    }

    if (!_placing || !_ghostValid) return;

    float hlTexW = static_cast<float>(_hlTexture.getSize().x);
    float hlTexH = static_cast<float>(_hlTexture.getSize().y);
    
    _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
    _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});

    const PowerDef& def = _defs[_selDef];
    for (int dy = 0; dy < def.gridH; ++dy) {
        for (int dx = 0; dx < def.gridW; ++dx) {
            sf::Vector2f tPos = gridToWorld(_ghostGridX + dx, _ghostGridY + dy, stepX, stepY);
            _hlSprite.setPosition(tPos);
            w.draw(_hlSprite);
        }
    }

    w.draw(_ghostSprite);
}

void PowerManager::clear() {
    _placed.clear();
}

int PowerManager::getTotalProduction() const {
    int total = 0;
    for (const auto& pp : _placed) {
        auto it = _defIndex.find(pp.defId);
        if (it != _defIndex.end())
            total += _defs[it->second].powerOutput;
    }
    return total;
}
