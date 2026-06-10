#include "include/BuildingManager.hpp"
#include "include/DetailPanelUtil.hpp"
#include "include/RoadManager.hpp"
#include "include/IndustrialManager.hpp"
#include "include/CitizenManager.hpp"
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

BuildingManager::BuildingManager(const sf::Font& font,
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

void BuildingManager::loadHouses(const std::string& dataFilePath,
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

        BuildingDef def;
        def.id          = trim(parts[0]);
        def.displayName = trim(parts[1]);
        def.description = trim(parts[2]);
        def.cost        = std::stoi(trim(parts[3]));
        def.materials   = trim(parts[4]);

        std::string idLower = toLower(def.id);
        std::vector<std::string> variantSuffixes = { "", "b", "c", "d", "e" };
        for (const auto& suf : variantSuffixes) {
            std::string path = texturesDir + idLower + suf + ".png";
            if (fs::exists(path)) {
                sf::Texture tex;
                if (tex.loadFromFile(path)) {
                    def.textures.push_back(std::move(tex));
                }
            } else {
                if (suf.empty()) {

                    std::cout << "[BuildingManager] ERROR, missing main texture: " << path << "\n";
                }
                break; 
            }
        }

        if (def.textures.empty()) {
            continue;
        }

        def.gridW = 1;
        def.gridH = 1;
        if (parts.size() >= 7) {
            def.gridW = std::stoi(trim(parts[5]));
            def.gridH = std::stoi(trim(parts[6]));
        } else if (!def.textures.empty()) {
            float tw = static_cast<float>(def.textures[0].getSize().x);
            if (tw > 100.f) {
                def.gridW = 2;
                def.gridH = 2;
            }
        }
        if (parts.size() >= 8)
            def.capacity = std::stoi(trim(parts[7]));
        if (parts.size() >= 9)
            def.powerDraw = std::stoi(trim(parts[8]));

        _defIndex[def.id] = _defs.size();
        _defs.push_back(std::move(def));
    }
    std::cout << "[BuildingManager] load completed " << _defs.size() << " buildings types.\n";
}

sf::Vector2f BuildingManager::gridToWorld(int gx, int gy, float stepX, float stepY) {
    return {
        static_cast<float>(gx - gy) * stepX,
        static_cast<float>(gx + gy) * stepY
    };
}

sf::Vector2i BuildingManager::worldToGrid(sf::Vector2f world, float stepX, float stepY) {
    if (stepX <= 0 || stepY <= 0) return {0, 0};
    float gxf = (world.x / stepX + world.y / stepY) / 2.f;
    float gyf = (world.y / stepY - world.x / stepX) / 2.f;
    return { static_cast<int>(std::floor(gxf)),
             static_cast<int>(std::floor(gyf)) };
}

void BuildingManager::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
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
            float listY  = _winPos.y + BM_TITLEBAR_H + 2.f;
            float rowH   = 22.f;
            int   visRows = static_cast<int>(BM_LIST_H / rowH);

            for (int i = 0; i < visRows; ++i) {
                int defIdx = _scrollOffset + i;
                if (defIdx >= static_cast<int>(_defs.size())) break;
                sf::FloatRect row({ listX, listY + i * rowH },
                                  { BM_LIST_W - 2.f, rowH });
                if (row.contains(mp)) {
                    _selDef = defIdx;
                    _selVar = (_defs[defIdx].textures.empty()) ? -1 : 0;
                    _placing = false;
                    break;
                }
            }

            float scrollBarX = _winPos.x + BM_LIST_W + 1.f;
            float scrollBarY = _winPos.y + BM_TITLEBAR_H + 2.f;
            sf::FloatRect scrollArea({scrollBarX, scrollBarY},
                                     {BM_SCROLL_W, BM_LIST_H});
            if (scrollArea.contains(mp)) {
                float relY = mp.y - scrollBarY;
                float ratio = relY / BM_LIST_H;
                int maxScroll = std::max(0, (int)_defs.size() - (int)(BM_LIST_H / 22.f));
                _scrollOffset = static_cast<int>(ratio * maxScroll);
                _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
            }

            if (_selDef >= 0) {
                const auto& def = _defs[_selDef];
                float vx = _winPos.x + BM_LIST_W + BM_SCROLL_W + 18.f;
                float vy = _winPos.y + BM_TITLEBAR_H + 10.f;
                int cols = std::max(1, static_cast<int>(
                    (BM_PREVIEW_W) / (BM_VARIANT_SIZE + BM_VARIANT_PAD)));

                for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
                    int col = vi % cols;
                    int row = vi / cols;
                    sf::FloatRect vRect(
                        {vx + col * (BM_VARIANT_SIZE + BM_VARIANT_PAD),
                         vy + row * (BM_VARIANT_SIZE + BM_VARIANT_PAD)},
                        {BM_VARIANT_SIZE, BM_VARIANT_SIZE});
                    if (vRect.contains(mp)) {
                        _selVar = vi;
                        break;
                    }
                }
            }

            float btnX = _winPos.x + 3.f;
            float btnY = _winPos.y + BM_TITLEBAR_H + BM_LIST_H + BM_INFO_H + 6.f;
            sf::FloatRect btnRect({btnX, btnY}, {BM_LIST_W - 6.f, BM_BTN_H});
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
        float listY = _winPos.y + BM_TITLEBAR_H + 2.f;
        sf::FloatRect listArea({listX, listY}, {BM_LIST_W + BM_SCROLL_W, BM_LIST_H});
        if (listArea.contains(mp)) {
            int maxScroll = std::max(0, (int)_defs.size() - (int)(BM_LIST_H / 22.f));
            _scrollOffset -= static_cast<int>(sw->delta);
            _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
        }
    }
}

void BuildingManager::simulatePlaceButtonClick() {
    if (_selDef >= 0 && _selVar >= 0) {
        _placing = true;
        const sf::Texture& tex = _defs[_selDef].textures[_selVar];
        _ghostSprite = sf::Sprite(tex);
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
    }
}

void BuildingManager::cycleVariation() {
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

bool BuildingManager::hasCollision(int startX, int startY, int w, int h) const {
    for (const auto& pb : _placed) {
        auto it = _defIndex.find(pb.defId);
        int pw = 1, ph = 1;
        if (it != _defIndex.end()) {
            pw = _defs[it->second].gridW;
            ph = _defs[it->second].gridH;
        }
        if (startX < pb.gridX + pw && startX + w > pb.gridX &&
            startY < pb.gridY + ph && startY + h > pb.gridY) {
            return true;
        }
    }
    return false;
}

bool BuildingManager::isOccupied(int startX, int startY, int w, int h, const RoadManager* rm, const GeneratedWorld* world) const {
    if (hasCollision(startX, startY, w, h)) return true;
    if (rm && rm->hasCollision(startX, startY, w, h)) return true;
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

bool BuildingManager::removeAt(int gx, int gy) {
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

bool BuildingManager::update(sf::Vector2f worldMousePos, float stepX, float stepY,
                              int& outGridX, int& outGridY, long long currentCash, const RoadManager* rm, const GeneratedWorld* world)
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

    const BuildingDef& def = _defs[_selDef];

    sf::Vector2f wp = gridToWorld(g.x, g.y, stepX, stepY);
    wp.x += (def.gridW - def.gridH) * stepX / 2.f;
    wp.y += (def.gridW + def.gridH) * stepY;

    const sf::Texture& tex = def.textures[_selVar];
    float tw = static_cast<float>(tex.getSize().x);
    float th = static_cast<float>(tex.getSize().y);
    _ghostSprite.setOrigin({tw / 2.f, th});
    _ghostSprite.setPosition(wp);
    _ghostValid = true;

    bool canPlace = (currentCash >= def.cost) && !isOccupied(g.x, g.y, def.gridW, def.gridH, rm, world);
    if (canPlace) {
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
        _hlSprite.setColor(sf::Color(0, 255, 0, 255));
    } else {
        _ghostSprite.setColor(sf::Color(255, 100, 100, 160));
        _hlSprite.setColor(sf::Color(255, 0, 0, 255));
    }

    return false;
}

int BuildingManager::confirmPlace(sf::Vector2f worldPos, float stepX, float stepY, long long currentCash, const RoadManager* rm, const GeneratedWorld* world) {
    if (!_placing || _selDef < 0 || _selVar < 0) return 0;

    const BuildingDef& def = _defs[_selDef];
    if (currentCash < def.cost || isOccupied(_ghostGridX, _ghostGridY, def.gridW, def.gridH, rm, world)) {
        return 0;
    }

    int cost = def.cost;

    sf::Vector2f wPos = gridToWorld(_ghostGridX, _ghostGridY, stepX, stepY);
    wPos.x += (def.gridW - def.gridH) * stepX / 2.f;
    wPos.y += (def.gridW + def.gridH) * stepY;
    PlacedBuilding pb(def.id, _selVar, wPos, _ghostGridX, _ghostGridY, def.textures[_selVar]);

    float tw = static_cast<float>(def.textures[_selVar].getSize().x);
    float th = static_cast<float>(def.textures[_selVar].getSize().y);
    pb.sprite.setOrigin({tw / 2.f, th});
    pb.sprite.setPosition(pb.worldPos);

    _placed.push_back(std::move(pb));

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

void BuildingManager::save(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f.is_open()) return;

    f << _placed.size() << "\n";
    for (const auto& pb : _placed) {
        f << pb.defId << " " << pb.variantIdx << " "
          << pb.gridX << " " << pb.gridY << " "
          << pb.worldPos.x << " " << pb.worldPos.y << "\n";
    }
}

void BuildingManager::load(const std::string& filePath,
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

        const BuildingDef& def = _defs[it->second];
        if (variantIdx < 0 || variantIdx >= static_cast<int>(def.textures.size())) continue;

        PlacedBuilding pb(defId, variantIdx, {wx, wy}, gx, gy, def.textures[variantIdx]);
        float tw = static_cast<float>(def.textures[variantIdx].getSize().x);
        float th = static_cast<float>(def.textures[variantIdx].getSize().y);
        pb.sprite.setOrigin({tw / 2.f, th});
        pb.sprite.setPosition(pb.worldPos);

        _placed.push_back(std::move(pb));
    }
}

bool BuildingManager::pointInRect(sf::Vector2f p, sf::FloatRect r) const {
    return r.contains(p);
}

void BuildingManager::drawUI(sf::RenderWindow& w) {
    if (!_open) return;
    drawWindow(w);
    drawList(w);
    drawVariants(w);
    drawInfo(w);
    drawPlaceButton(w);
    w.draw(_closeBtn);
}

void BuildingManager::drawWindow(sf::RenderWindow& w) {
    sf::RectangleShape bg({BM_WIN_W, BM_WIN_H});
    bg.setPosition({_winPos.x, _winPos.y});
    bg.setFillColor(sf::Color::Black);
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({BM_WIN_W, BM_TITLEBAR_H});
    tb.setPosition({_winPos.x, _winPos.y});
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "HOUSES SELECTION   " + _texturesDir, BM_FONT_SIZE);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_winPos.x + 6.f, _winPos.y + 6.f});
    w.draw(title);

    sf::RectangleShape sep({2.f, BM_WIN_H - BM_TITLEBAR_H});
    sep.setPosition({_winPos.x + BM_LIST_W + BM_SCROLL_W + 4.f,
                     _winPos.y + BM_TITLEBAR_H});
    sep.setFillColor(sf::Color(80, 80, 80));
    w.draw(sep);

    float sepInfoY = _winPos.y + BM_TITLEBAR_H + BM_LIST_H;
    sf::RectangleShape sepH({BM_LIST_W + BM_SCROLL_W, 2.f});
    sepH.setPosition({_winPos.x + 1.f, sepInfoY});
    sepH.setFillColor(sf::Color(80, 80, 80));
    w.draw(sepH);
}

void BuildingManager::drawList(sf::RenderWindow& w) {
    float listX  = _winPos.x + 4.f;
    float listY  = _winPos.y + BM_TITLEBAR_H + 4.f;
    float rowH   = 22.f;
    int   visRows = static_cast<int>(BM_LIST_H / rowH);

    for (int i = 0; i < visRows; ++i) {
        int defIdx = _scrollOffset + i;
        if (defIdx >= static_cast<int>(_defs.size())) break;

        const auto& def = _defs[defIdx];
        float ry = listY + i * rowH;

        if (defIdx == _selDef) {
            sf::RectangleShape sel({BM_LIST_W - 6.f, rowH - 2.f});
            sel.setPosition({listX - 2.f, ry + 3.f});
            sel.setFillColor(sf::Color::Transparent);
            sel.setOutlineColor(sf::Color(220, 120, 0));
            sel.setOutlineThickness(1.f);
            w.draw(sel);
        }

        std::string label = std::to_string(_scrollOffset + i + 1) + ". " + def.displayName;
        sf::Text txt(_font, label, BM_FONT_SIZE);
        txt.setFillColor(_selDef == defIdx ? sf::Color(255, 160, 30) : sf::Color::White);
        txt.setPosition({listX, ry + 3.f});
        w.draw(txt);
    }

    float scrollBarX = _winPos.x + BM_LIST_W + 2.f;
    float scrollBarY = _winPos.y + BM_TITLEBAR_H + 2.f;
    sf::RectangleShape scrollBg({BM_SCROLL_W - 2.f, BM_LIST_H - 1.f});
    scrollBg.setPosition({scrollBarX, scrollBarY});
    scrollBg.setFillColor(sf::Color(30, 30, 30));
    scrollBg.setOutlineColor(sf::Color(80, 80, 80));
    scrollBg.setOutlineThickness(1.f);
    w.draw(scrollBg);

    int total = static_cast<int>(_defs.size());
    if (total > 0) {
        float thumbH = BM_LIST_H - 1.f;
        if (total > visRows) {
            thumbH = std::max(20.f, (BM_LIST_H - 1.f) * (static_cast<float>(visRows) / total));
        }
        
        float maxScroll = std::max(1, total - visRows);
        float thumbY = scrollBarY + (_scrollOffset / static_cast<float>(maxScroll))
                       * ((BM_LIST_H - 1.f) - thumbH);
        sf::RectangleShape thumb({BM_SCROLL_W - 4.f, thumbH});
        thumb.setPosition({scrollBarX + 1.f, thumbY});
        thumb.setFillColor(sf::Color(130, 60, 0));
        w.draw(thumb);
    }
}

void BuildingManager::drawVariants(sf::RenderWindow& w) {
    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    float vx = _winPos.x + BM_LIST_W + BM_SCROLL_W + 18.f;
    float vy = _winPos.y + BM_TITLEBAR_H + 10.f;
    int cols = std::max(1, static_cast<int>(
        (BM_PREVIEW_W) / (BM_VARIANT_SIZE + BM_VARIANT_PAD)));

    for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
        int col = vi % cols;
        int row = vi / cols;
        float cx = vx + col * (BM_VARIANT_SIZE + BM_VARIANT_PAD);
        float cy = vy + row * (BM_VARIANT_SIZE + BM_VARIANT_PAD);

        sf::RectangleShape tile({BM_VARIANT_SIZE, BM_VARIANT_SIZE});
        tile.setPosition({cx, cy});
        tile.setFillColor(sf::Color(15, 15, 15));
        tile.setOutlineThickness(vi == _selVar ? 1.f : 0.f);
        tile.setOutlineColor(sf::Color(220, 120, 0));
        w.draw(tile);

        sf::Sprite spr(def.textures[vi]);
        float sx = spr.getGlobalBounds().size.x;
        float sy = spr.getGlobalBounds().size.y;
        float scale = std::min((BM_VARIANT_SIZE - 8.f) / sx, (BM_VARIANT_SIZE - 8.f) / sy);
        spr.setScale({scale, scale});
        spr.setPosition({cx + (BM_VARIANT_SIZE - sx * scale) / 2.f, cy + (BM_VARIANT_SIZE - sy * scale) / 2.f});
        w.draw(spr);
    }
}

void BuildingManager::drawInfo(sf::RenderWindow& w) {
    float infoX = _winPos.x + 4.f;
    float infoY = _winPos.y + BM_TITLEBAR_H + BM_LIST_H + 6.f;

    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    sf::Text nameT(_font, "Building info " + def.displayName + ":", BM_FONT_SIZE);
    nameT.setFillColor(sf::Color::White);
    nameT.setStyle(sf::Text::Style::Bold);
    nameT.setPosition({infoX, infoY});
    w.draw(nameT);

    sf::Text descT(_font, "", BM_FONT_SIZE - 1);
    descT.setFillColor(sf::Color(200, 200, 200));
    descT.setPosition({infoX, infoY + 18.f});

    std::string wrapped;
    std::string currentLine;
    std::stringstream ss(def.description);
    std::string word;
    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        descT.setString(testLine);
        if (descT.getLocalBounds().size.x > BM_LIST_W - 8.f) {
            wrapped += currentLine + "\n";
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    wrapped += currentLine;
    descT.setString(wrapped);
    w.draw(descT);

    sf::Text costT(_font, "Costs " + std::to_string(def.cost) + "$", BM_FONT_SIZE);
    costT.setFillColor(sf::Color::White);
    costT.setPosition({infoX, infoY + BM_INFO_H - 42.f});
    w.draw(costT);

    sf::Text matT(_font, "Materials: " + def.materials, BM_FONT_SIZE);
    matT.setFillColor(sf::Color::White);
    matT.setPosition({infoX, infoY + BM_INFO_H - 24.f});
    w.draw(matT);
}

void BuildingManager::drawPlaceButton(sf::RenderWindow& w) {
    float btnX = _winPos.x + 4.f;
    float btnY = _winPos.y + BM_TITLEBAR_H + BM_LIST_H + BM_INFO_H + 6.f;

    bool active = (_selDef >= 0 && _selVar >= 0);
    sf::RectangleShape btn({BM_LIST_W - 8.f, BM_BTN_H});
    btn.setPosition({btnX, btnY});
    btn.setFillColor(active ? sf::Color(200, 100, 0) : sf::Color(60, 60, 60));
    w.draw(btn);

    sf::Text btnTxt(_font, "PLACE BUILDING", BM_FONT_SIZE + 1);
    btnTxt.setFillColor(sf::Color::White);
    btnTxt.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = btnTxt.getLocalBounds();
    btnTxt.setOrigin({tb.position.x + tb.size.x / 2.f,
                      tb.position.y + tb.size.y / 2.f});
    btnTxt.setPosition({btnX + (BM_LIST_W - 8.f) / 2.f, btnY + BM_BTN_H / 2.f});
    w.draw(btnTxt);
}

void BuildingManager::drawWorldOverlay(sf::RenderWindow& w, float stepX, float stepY) {
    for (const auto& ft : _floatingTexts) {
        w.draw(ft.text);
    }

    if (!_placing || !_ghostValid) return;

    float hlTexW = static_cast<float>(_hlTexture.getSize().x);
    float hlTexH = static_cast<float>(_hlTexture.getSize().y);
    
    _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
    _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});

    const BuildingDef& def = _defs[_selDef];
    for (int dy = 0; dy < def.gridH; ++dy) {
        for (int dx = 0; dx < def.gridW; ++dx) {
            sf::Vector2f tPos = gridToWorld(_ghostGridX + dx, _ghostGridY + dy, stepX, stepY);
            _hlSprite.setPosition(tPos);
            w.draw(_hlSprite);
        }
    }

    w.draw(_ghostSprite);
}

void BuildingManager::clear() {
    _placed.clear();
    closeDetail();
}

bool BuildingManager::findAt(int gx, int gy, int& outIdx) const {
    for (size_t i = 0; i < _placed.size(); ++i) {
        const auto& pb = _placed[i];
        int pw = 1, ph = 1;
        getGridSize(pb.defId, pw, ph);
        if (gx >= pb.gridX && gx < pb.gridX + pw &&
            gy >= pb.gridY && gy < pb.gridY + ph) {
            outIdx = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool BuildingManager::tryOpenDetailAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                                           sf::RenderWindow& window, const sf::View& gameView)
{
    sf::Vector2i g = worldToGrid(worldPos, stepX, stepY);
    int idx = -1;
    if (!findAt(g.x, g.y, idx)) return false;
    openDetail(idx, window, gameView);
    return true;
}

float BuildingManager::computeDetailHeight() const {
    float lineH = static_cast<float>(BM_FONT_SIZE) + 8.f;
    float h = BM_TITLEBAR_H + 12.f + 4.f * lineH + 6.f + 3.f * 36.f + 10.f;
    if (_pickingWorkplace) {
        h += 4.f + lineH;
        if (_reachableWorkIndices.empty() && _reachableBusStops.empty())
            h += lineH;
        else {
            h += static_cast<float>(_reachableWorkIndices.size()) * 36.f;
            h += static_cast<float>(_reachableBusStops.size()) * 36.f;
        }
    }
    return std::max(BM_DETAIL_MIN_H, h);
}

void BuildingManager::openDetail(int placedIdx, sf::RenderWindow& window, const sf::View& gameView) {
    if (placedIdx < 0 || placedIdx >= static_cast<int>(_placed.size())) return;
    _detailOpen = true;
    _detailIdx = placedIdx;
    _detailScroll = 0;
    _pickingWorkplace = false;
    _reachableWorkIndices.clear();
    _reachableBusStops.clear();
    _detailH = computeDetailHeight();
    _detailPos = panelPosBesideBuilding(_placed[placedIdx].worldPos, window, gameView,
                                        BM_DETAIL_W, _detailH);
    _detailCloseRect = detailCloseRect(_detailPos, BM_DETAIL_W, BM_TITLEBAR_H);
}

void BuildingManager::closeDetail() {
    _detailOpen = false;
    _detailIdx = -1;
    _pickingWorkplace = false;
    _reachableWorkIndices.clear();
    _reachableBusStops.clear();
}

int BuildingManager::getTotalHousingCapacity() const {
    int total = 0;
    for (const auto& pb : _placed)
        total += getCapacityFor(pb);
    return total;
}

int BuildingManager::getCapacityFor(const PlacedBuilding& pb) const {
    auto it = _defIndex.find(pb.defId);
    if (it == _defIndex.end()) return 0;
    return _defs[it->second].capacity;
}

int BuildingManager::getPowerDrawFor(const PlacedBuilding& pb) const {
    auto it = _defIndex.find(pb.defId);
    if (it == _defIndex.end()) return 0;
    return _defs[it->second].powerDraw;
}

void BuildingManager::getGridSize(const std::string& defId, int& outW, int& outH) const {
    outW = 1; outH = 1;
    auto it = _defIndex.find(defId);
    if (it != _defIndex.end()) {
        outW = _defs[it->second].gridW;
        outH = _defs[it->second].gridH;
    }
}

void BuildingManager::addDetailHit(float x, float y, float w, float h, const std::string& id) {
    _detailHits.push_back({sf::FloatRect({x, y}, {w, h}), id});
}

const BuildingManager::DetailHitBox* BuildingManager::detailHitAt(sf::Vector2f p) const {
    for (const auto& h : _detailHits)
        if (h.rect.contains(p)) return &h;
    return nullptr;
}

bool BuildingManager::handleDetailEvent(const sf::Event& event, sf::RenderWindow& window,
                                        RoadManager* rm, IndustrialManager* im, CitizenManager* cm,
                                        VehicleManager* vm)
{
    if (!_detailOpen || _detailIdx < 0) return false;

    sf::Vector2f mp = window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getDefaultView());
    _detailMousePos = mp;

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (_pickingWorkplace && rm) {
                _pickingWorkplace = false;
                _reachableWorkIndices.clear();
                _reachableBusStops.clear();
                rm->clearWorkplaceHighlights();
            } else {
                closeDetail();
            }
            return true;
        }
        return false;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;

        if (_detailCloseRect.contains(mp)) {
            closeDetail();
            if (rm) rm->clearWorkplaceHighlights();
            return true;
        }

        const DetailHitBox* h = detailHitAt(mp);
        if (!h) return true;

        if (h->id == "assign_work" && rm && im && cm) {
            _pickingWorkplace = true;
            const auto& house = _placed[_detailIdx];
            int hw = 1, hh = 1;
            getGridSize(house.defId, hw, hh);
            _reachableWorkIndices =
                rm->findReachableIndustrialIndices(house.gridX, house.gridY, hw, hh, *im, 20);
            _reachableBusStops =
                rm->findReachableBusStops(house.gridX, house.gridY, hw, hh, 20);
            rm->setWorkplaceHighlights(_reachableWorkIndices);
            rm->setAssignBusStopHighlights(_reachableBusStops);
            _detailH = computeDetailHeight();
            return true;
        }
        if (h->id == "clear_work" && cm) {
            cm->clearWorkplace(*this, _detailIdx);
            if (rm) rm->clearWorkplaceHighlights();
            _pickingWorkplace = false;
            _reachableWorkIndices.clear();
            _reachableBusStops.clear();
            return true;
        }
        if (h->id == "toggle_bus") {
            _placed[_detailIdx].useBusCommute = !_placed[_detailIdx].useBusCommute;
            return true;
        }
        if (h->id.rfind("work_", 0) == 0 && cm && im && rm && vm) {
            int widx = std::atoi(h->id.c_str() + 5);
            if (cm->assignWorkplace(*this, _detailIdx, widx, *rm, *im, *vm, 20)) {
                _pickingWorkplace = false;
                _reachableWorkIndices.clear();
                _reachableBusStops.clear();
                rm->clearWorkplaceHighlights();
            }
            return true;
        }
        if (h->id.rfind("stop_", 0) == 0 && cm && im && rm && vm) {
            int sx = 0, sy = 0;
            if (std::sscanf(h->id.c_str(), "stop_%d_%d", &sx, &sy) == 2 &&
                cm->assignBoardingStop(*this, _detailIdx, sx, sy, *rm, *im, *vm, 20)) {
                _pickingWorkplace = false;
                _reachableWorkIndices.clear();
                _reachableBusStops.clear();
                rm->clearWorkplaceHighlights();
            }
            return true;
        }
        return true;
    }

    return false;
}

void BuildingManager::drawDetailPanel(sf::RenderWindow& w, const IndustrialManager* im,
                                      bool powerShortage) {
    if (!_detailOpen || _detailIdx < 0) return;

    _detailHits.clear();
    _detailMousePos = w.mapPixelToCoords(sf::Mouse::getPosition(w), w.getDefaultView());
    _detailH = computeDetailHeight();

    const PlacedBuilding& pb = _placed[_detailIdx];
    auto defIt = _defIndex.find(pb.defId);
    std::string name = (defIt != _defIndex.end()) ? _defs[defIt->second].displayName : pb.defId;
    int cap = getCapacityFor(pb);
    int power = getPowerDrawFor(pb);

    sf::RectangleShape bg({BM_DETAIL_W, _detailH});
    bg.setPosition(_detailPos);
    bg.setFillColor(sf::Color(10, 10, 10, 245));
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({BM_DETAIL_W, BM_TITLEBAR_H});
    tb.setPosition(_detailPos);
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "HOUSE - " + name, BM_FONT_SIZE + 2);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_detailPos.x + 10.f, _detailPos.y + 6.f});
    w.draw(title);
    drawDetailCloseButton(w, _closeBtn, _detailPos, BM_DETAIL_W, BM_TITLEBAR_H, _detailCloseRect);

    float x = _detailPos.x + 14.f;
    float y = _detailPos.y + BM_TITLEBAR_H + 12.f;
    float lineH = static_cast<float>(BM_FONT_SIZE) + 8.f;

    auto drawLine = [&](const std::string& txt, sf::Color col = sf::Color(220, 220, 220)) {
        sf::Text t(_font, txt, BM_FONT_SIZE);
        t.setFillColor(col);
        t.setPosition({x, y});
        w.draw(t);
        y += lineH;
    };

    drawLine("Residents: " + std::to_string(pb.residents) + " / " + std::to_string(cap), sf::Color::White);
    drawLine("Power draw: " + std::to_string(power) + " kW");
    if (powerShortage)
        drawLine("NO POWER — building without electricity!", sf::Color(255, 60, 60));

    std::string workLine = "Workplace: none";
    if (pb.assignedWorkIdx >= 0 && im) {
        const auto& inds = im->getPlaced();
        if (pb.assignedWorkIdx < static_cast<int>(inds.size())) {
            auto iit = im->getDisplayName(inds[pb.assignedWorkIdx].defId);
            workLine = "Workplace: " + iit + " (commuters)";
        }
    }
    drawLine(workLine);
    if (pb.useBusCommute && pb.boardingStopX >= 0) {
        if (pb.assignedWorkIdx >= 0 && im &&
            pb.assignedWorkIdx < static_cast<int>(im->getPlaced().size()))
            drawLine("Commute: Bus stop (" + std::to_string(pb.boardingStopX) + "," +
                     std::to_string(pb.boardingStopY) + ") -> " +
                     im->getDisplayName(im->getPlaced()[pb.assignedWorkIdx].defId),
                     sf::Color(100, 220, 255));
        else
            drawLine("Commute: Bus stop (" + std::to_string(pb.boardingStopX) + "," +
                     std::to_string(pb.boardingStopY) + ") — set factory at stop",
                     sf::Color(100, 220, 255));
    } else if (pb.assignedWorkIdx >= 0)
        drawLine("Commute: Walk (within 20 road tiles)", sf::Color(120, 255, 120));
    else
        drawLine("Commute: none");

    y += 6.f;
    auto drawBtn = [&](const std::string& label, const std::string& id, float bw) {
        sf::FloatRect rect({x, y}, {bw, 30.f});
        bool hover = rect.contains(_detailMousePos);
        sf::RectangleShape b({bw, 30.f});
        b.setPosition({x, y});
        b.setFillColor(hover ? sf::Color(240, 140, 30) : sf::Color(200, 100, 0));
        w.draw(b);
        sf::Text t(_font, label, BM_FONT_SIZE);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Style::Bold);
        sf::FloatRect tb = t.getLocalBounds();
        t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
        t.setPosition({x + bw / 2.f, y + 15.f});
        w.draw(t);
        addDetailHit(x, y, bw, 30.f, id);
        y += 36.f;
    };

    drawBtn("Assign destination (road <= 20)", "assign_work", BM_DETAIL_W - 28.f);
    drawBtn("Clear assignment", "clear_work", BM_DETAIL_W - 28.f);

    if (_pickingWorkplace && im) {
        y += 4.f;
        drawLine("--- Factories (green) / Bus stops (cyan) ---", sf::Color(255, 160, 30));
        const auto& inds = im->getPlaced();
        if (_reachableWorkIndices.empty() && _reachableBusStops.empty())
            drawLine("Nothing reachable within 20 road tiles.", sf::Color(170, 170, 170));
        for (int wi : _reachableWorkIndices) {
            if (wi < 0 || wi >= static_cast<int>(inds.size())) continue;
            std::string lbl = "[Factory] " + im->getDisplayName(inds[wi].defId);
            drawBtn(lbl, "work_" + std::to_string(wi), BM_DETAIL_W - 28.f);
        }
        for (const auto& [sx, sy] : _reachableBusStops) {
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "[Bus stop] %d, %d", sx, sy);
            char id[48];
            std::snprintf(id, sizeof(id), "stop_%d_%d", sx, sy);
            drawBtn(lbl, id, BM_DETAIL_W - 28.f);
        }
    }
}
