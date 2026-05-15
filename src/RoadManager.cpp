#include "include/RoadManager.hpp"
#include "include/BuildingManager.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cstdint>

namespace fs = std::filesystem;

static std::vector<std::string> rmSplitLine(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delim))
        parts.push_back(token);
    return parts;
}

static std::string rmTrim(const std::string& s) {
    std::string res = s;
    if (res.size() >= 3 && res.substr(0, 3) == "\xEF\xBB\xBF")
        res.erase(0, 3);
    size_t a = res.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = res.find_last_not_of(" \t\r\n");
    return res.substr(a, b - a + 1);
}

static std::string rmToLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

RoadManager::RoadManager(const sf::Font& font,
                         const sf::Texture& closeTexture,
                         const sf::Texture& hlTexture)
    : _font(font),
      _closeBtn(closeTexture),
      _hlTexture(hlTexture),
      _hlSprite(hlTexture),
      _ghostSprite(hlTexture)
{
}

void RoadManager::loadRoads(const std::string& dataFilePath,
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
        line = rmTrim(line);
        if (line.empty() || line[0] == '#') continue;

        auto parts = rmSplitLine(line, '|');
        if (parts.size() < 5) continue;

        RoadDef def;
        def.id          = rmTrim(parts[0]);
        def.displayName = rmTrim(parts[1]);
        def.description = rmTrim(parts[2]);
        def.cost        = std::stoi(rmTrim(parts[3]));
        def.materials   = rmTrim(parts[4]);
        def.isDraggable = (parts.size() >= 6 && rmTrim(parts[5]) == "1");

        std::string idLower = rmToLower(def.id);
        std::vector<std::string> variantSuffixes = { "", "b", "c", "d", "e" };
        for (const auto& suf : variantSuffixes) {
            std::string path = texturesDir + idLower + suf + ".png";
            if (fs::exists(path)) {
                sf::Texture tex;
                if (tex.loadFromFile(path))
                    def.textures.push_back(std::move(tex));
            } else {
                break;
            }
        }

        if (def.textures.empty()) {
            if (onError) onError("ERROR, missing texture for: " + def.id);
            continue;
        }

        def.gridW = 1;
        def.gridH = 1;

        _defIndex[def.id] = _defs.size();
        _defs.push_back(std::move(def));
    }
}

sf::Vector2f RoadManager::gridToWorld(int gx, int gy, float stepX, float stepY) {
    return {
        static_cast<float>(gx - gy) * stepX,
        static_cast<float>(gx + gy) * stepY
    };
}

sf::Vector2i RoadManager::worldToGrid(sf::Vector2f world, float stepX, float stepY) {
    if (stepX <= 0 || stepY <= 0) return {0, 0};
    float gxf = (world.x / stepX + world.y / stepY) / 2.f;
    float gyf = (world.y / stepY - world.x / stepX) / 2.f;
    return { static_cast<int>(std::floor(gxf)),
             static_cast<int>(std::floor(gyf)) };
}

void RoadManager::buildGhostPositions() {
    _ghostPositions.clear();
    if (_selDef < 0 || _selVar < 0) return;

    if (_selVar == 0) {
        int minX = std::min(_dragStartGX, _dragCurrentGX);
        int maxX = std::max(_dragStartGX, _dragCurrentGX);
        for (int gx = minX; gx <= maxX; ++gx)
            _ghostPositions.push_back({gx, _dragStartGY});
    } else {
        int minY = std::min(_dragStartGY, _dragCurrentGY);
        int maxY = std::max(_dragStartGY, _dragCurrentGY);
        for (int gy = minY; gy <= maxY; ++gy)
            _ghostPositions.push_back({_dragStartGX, gy});
    }
}

bool RoadManager::hasCollision(int startX, int startY, int w, int h) const {
    for (const auto& pr : _placed) {
        auto it = _defIndex.find(pr.defId);
        int pw = 1, ph = 1;
        if (it != _defIndex.end()) {
            pw = _defs[it->second].gridW;
            ph = _defs[it->second].gridH;
        }
        if (startX < pr.gridX + pw && startX + w > pr.gridX &&
            startY < pr.gridY + ph && startY + h > pr.gridY) {
            return true;
        }
    }
    return false;
}

bool RoadManager::isOccupied(int startX, int startY, int w, int h, const BuildingManager* bm, const GeneratedWorld* world) const {
    if (hasCollision(startX, startY, w, h)) return true;
    if (bm && bm->hasCollision(startX, startY, w, h)) return true;
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

void RoadManager::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
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
            float listY  = _winPos.y + RM_TITLEBAR_H + 2.f;
            float rowH   = 22.f;
            int   visRows = static_cast<int>(RM_LIST_H / rowH);

            for (int i = 0; i < visRows; ++i) {
                int defIdx = _scrollOffset + i;
                if (defIdx >= static_cast<int>(_defs.size())) break;
                sf::FloatRect row({ listX, listY + i * rowH }, { RM_LIST_W - 2.f, rowH });
                if (row.contains(mp)) {
                    _selDef = defIdx;
                    _selVar = (_defs[defIdx].textures.empty()) ? -1 : 0;
                    _placingState = RoadPlacingState::NONE;
                    break;
                }
            }

            float scrollBarX = _winPos.x + RM_LIST_W + 1.f;
            float scrollBarY = _winPos.y + RM_TITLEBAR_H + 2.f;
            sf::FloatRect scrollArea({scrollBarX, scrollBarY}, {RM_SCROLL_W, RM_LIST_H});
            if (scrollArea.contains(mp)) {
                float relY = mp.y - scrollBarY;
                float ratio = relY / RM_LIST_H;
                int maxScroll = std::max(0, (int)_defs.size() - (int)(RM_LIST_H / 22.f));
                _scrollOffset = static_cast<int>(ratio * maxScroll);
                _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
            }

            if (_selDef >= 0) {
                const auto& def = _defs[_selDef];
                float vx = _winPos.x + RM_LIST_W + RM_SCROLL_W + 18.f;
                float vy = _winPos.y + RM_TITLEBAR_H + 10.f;
                int cols = std::max(1, static_cast<int>(RM_PREVIEW_W / (RM_VARIANT_SIZE + RM_VARIANT_PAD)));

                for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
                    int col = vi % cols;
                    int row = vi / cols;
                    sf::FloatRect vRect(
                        {vx + col * (RM_VARIANT_SIZE + RM_VARIANT_PAD),
                         vy + row * (RM_VARIANT_SIZE + RM_VARIANT_PAD)},
                        {RM_VARIANT_SIZE, RM_VARIANT_SIZE});
                    if (vRect.contains(mp)) {
                        _selVar = vi;
                        _placingState = RoadPlacingState::NONE;
                        break;
                    }
                }
            }

            float btnX = _winPos.x + 3.f;
            float btnY = _winPos.y + RM_TITLEBAR_H + RM_LIST_H + RM_INFO_H + 6.f;
            sf::FloatRect btnRect({btnX, btnY}, {RM_LIST_W - 6.f, RM_BTN_H});
            if (btnRect.contains(mp) && _selDef >= 0 && _selVar >= 0) {
                const auto& def = _defs[_selDef];
                if (def.isDraggable) {
                    _placingState = RoadPlacingState::AWAITING_START;
                } else {
                    _placingState = RoadPlacingState::SINGLE;
                    _ghostSprite = sf::Sprite(def.textures[_selVar]);
                    _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
                }
            }
        }
        else if (mb->button == sf::Mouse::Button::Right) {
            _placingState = RoadPlacingState::NONE;
            _ghostPositions.clear();
        }
    }

    if (const auto* sw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float listX = _winPos.x + 3.f;
        float listY = _winPos.y + RM_TITLEBAR_H + 2.f;
        sf::FloatRect listArea({listX, listY}, {RM_LIST_W + RM_SCROLL_W, RM_LIST_H});
        if (listArea.contains(mp)) {
            int maxScroll = std::max(0, (int)_defs.size() - (int)(RM_LIST_H / 22.f));
            _scrollOffset -= static_cast<int>(sw->delta);
            _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
        }
    }
}

void RoadManager::simulatePlaceButtonClick() {
    if (_selDef >= 0 && _selVar >= 0) {
        const auto& def = _defs[_selDef];
        if (def.isDraggable) {
            _placingState = RoadPlacingState::AWAITING_START;
        } else {
            _placingState = RoadPlacingState::SINGLE;
            _ghostSprite = sf::Sprite(def.textures[_selVar]);
            _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
        }
    }
}

void RoadManager::cycleVariation() {
    if (_selDef >= 0 && !_defs[_selDef].textures.empty()) {
        _selVar++;
        if (_selVar >= static_cast<int>(_defs[_selDef].textures.size())) {
            _selVar = 0;
        }
        const auto& def = _defs[_selDef];
        if (!def.isDraggable) {
            _ghostSprite = sf::Sprite(def.textures[_selVar]);
            _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
        }
    }
}

int RoadManager::handleWorldClick(sf::Vector2f worldPos, float stepX, float stepY, long long currentCash, const BuildingManager* bm, const GeneratedWorld* world) {
    if (_selDef < 0 || _selVar < 0) return 0;

    sf::Vector2i g = worldToGrid(worldPos, stepX, stepY);

    if (_placingState == RoadPlacingState::AWAITING_START) {
        _dragStartGX  = g.x;
        _dragStartGY  = g.y;
        _dragCurrentGX = g.x;
        _dragCurrentGY = g.y;
        _placingState = RoadPlacingState::DRAGGING;
        buildGhostPositions();
        return 0;
    }

    if (_placingState == RoadPlacingState::DRAGGING) {
        const RoadDef& def = _defs[_selDef];
        long long validCount = 0;
        for (const auto& [gx, gy] : _ghostPositions) {
            if (!isOccupied(gx, gy, def.gridW, def.gridH, bm, world)) validCount++;
        }
        long long totalCost = static_cast<long long>(def.cost) * validCount;

        if (currentCash < totalCost || validCount == 0) return 0; 

        int confirmed = 0;
        for (const auto& [gx, gy] : _ghostPositions) {
            if (isOccupied(gx, gy, def.gridW, def.gridH, bm, world)) continue; 
            sf::Vector2f wPos = gridToWorld(gx, gy, stepX, stepY);
            wPos.y += stepY * 2.f;

            PlacedRoad pr(def.id, _selVar, wPos, gx, gy, def.textures[_selVar]);
            float tw = static_cast<float>(def.textures[_selVar].getSize().x);
            float th = static_cast<float>(def.textures[_selVar].getSize().y);
            pr.sprite.setOrigin({tw / 2.f, th});
            pr.sprite.setPosition(pr.worldPos);
            _placed.push_back(std::move(pr));
            ++confirmed;
        }

        if (confirmed > 0) {
            FloatingText ft{sf::Text(_font, "-" + std::to_string(totalCost) + "$", 18), 120};
            ft.text.setFillColor(sf::Color::Red);
            ft.text.setOutlineColor(sf::Color::Black);
            ft.text.setOutlineThickness(1.f);
            sf::FloatRect tr = ft.text.getLocalBounds();
            ft.text.setOrigin({tr.position.x + tr.size.x / 2.f, tr.position.y + tr.size.y / 2.f});
            ft.text.setPosition({worldPos.x, worldPos.y - 60.f});
            _floatingTexts.push_back(std::move(ft));
        }

        _ghostPositions.clear();
        _placingState = RoadPlacingState::AWAITING_START;
        return static_cast<int>(totalCost);
    }

    if (_placingState == RoadPlacingState::SINGLE) {
        const RoadDef& def = _defs[_selDef];
        if (currentCash < def.cost || isOccupied(g.x, g.y, def.gridW, def.gridH, bm, world)) return 0;

        sf::Vector2f wPos = gridToWorld(g.x, g.y, stepX, stepY);
        wPos.y += stepY * 2.f;

        PlacedRoad pr(def.id, _selVar, wPos, g.x, g.y, def.textures[_selVar]);
        float tw = static_cast<float>(def.textures[_selVar].getSize().x);
        float th = static_cast<float>(def.textures[_selVar].getSize().y);
        pr.sprite.setOrigin({tw / 2.f, th});
        pr.sprite.setPosition(pr.worldPos);
        _placed.push_back(std::move(pr));

        FloatingText ft{sf::Text(_font, "-" + std::to_string(def.cost) + "$", 18), 120};
        ft.text.setFillColor(sf::Color::Red);
        ft.text.setOutlineColor(sf::Color::Black);
        ft.text.setOutlineThickness(1.f);
        sf::FloatRect tr = ft.text.getLocalBounds();
        ft.text.setOrigin({tr.position.x + tr.size.x / 2.f, tr.position.y + tr.size.y / 2.f});
        ft.text.setPosition({worldPos.x, worldPos.y - 60.f});
        _floatingTexts.push_back(std::move(ft));

        return def.cost;
    }

    return 0;
}

void RoadManager::update(sf::Vector2f worldMousePos, float stepX, float stepY, long long currentCash, const BuildingManager* bm, const GeneratedWorld* world) {
    for (auto it = _floatingTexts.begin(); it != _floatingTexts.end(); ) {
        it->frames--;
        it->text.move({0.f, -1.5f});
        if (it->frames < 60) {
            sf::Color c  = it->text.getFillColor();
            sf::Color oc = it->text.getOutlineColor();
            c.a  = static_cast<std::uint8_t>((it->frames / 60.f) * 255);
            oc.a = c.a;
            it->text.setFillColor(c);
            it->text.setOutlineColor(oc);
        }
        if (it->frames <= 0) it = _floatingTexts.erase(it);
        else ++it;
    }

    if (_selDef < 0 || _selVar < 0) return;
    if (_placingState == RoadPlacingState::NONE) return;

    sf::Vector2i g = worldToGrid(worldMousePos, stepX, stepY);

    if (_placingState == RoadPlacingState::DRAGGING) {
        _dragCurrentGX = g.x;
        _dragCurrentGY = g.y;
        buildGhostPositions();

        const RoadDef& def = _defs[_selDef];
        long long validCount = 0;
        for (const auto& [gx, gy] : _ghostPositions) {
            if (!isOccupied(gx, gy, def.gridW, def.gridH, bm, world)) validCount++;
        }
        long long totalCost = static_cast<long long>(def.cost) * validCount;
        bool canAfford = (currentCash >= totalCost && validCount > 0);
        _hlSprite.setColor(canAfford ? sf::Color(0, 255, 0, 220) : sf::Color(255, 0, 0, 220));
    }

    if (_placingState == RoadPlacingState::SINGLE || _placingState == RoadPlacingState::AWAITING_START) {
        const RoadDef& def = _defs[_selDef];
        sf::Vector2f wp = gridToWorld(g.x, g.y, stepX, stepY);
        wp.y += stepY * 2.f;

        const sf::Texture& tex = def.textures[_selVar];
        float tw = static_cast<float>(tex.getSize().x);
        float th = static_cast<float>(tex.getSize().y);
        _ghostSprite = sf::Sprite(tex);
        _ghostSprite.setOrigin({tw / 2.f, th});
        _ghostSprite.setPosition(wp);
        _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
        _ghostValid = true;

        bool canAfford = (currentCash >= def.cost);
        if (!canAfford || isOccupied(g.x, g.y, def.gridW, def.gridH, bm, world)) {
            _ghostSprite.setColor(sf::Color(255, 80, 80, 160));
            _hlSprite.setColor(sf::Color(255, 0, 0, 255));
        } else {
            _ghostSprite.setColor(sf::Color(255, 255, 255, 160));
            _hlSprite.setColor(sf::Color(0, 255, 0, 255));
        }
    }
}

void RoadManager::save(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f.is_open()) return;
    f << _placed.size() << "\n";
    for (const auto& pr : _placed) {
        f << pr.defId << " " << pr.variantIdx << " "
          << pr.gridX << " " << pr.gridY << " "
          << pr.worldPos.x << " " << pr.worldPos.y << "\n";
    }
}

void RoadManager::load(const std::string& filePath,
                        std::function<void(const std::string&)> onError)
{
    _placed.clear();
    if (filePath.empty()) return;

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
            if (onError) onError("ERROR, unknown road in save: " + defId);
            continue;
        }

        const RoadDef& def = _defs[it->second];
        if (variantIdx < 0 || variantIdx >= static_cast<int>(def.textures.size())) continue;

        PlacedRoad pr(defId, variantIdx, {wx, wy}, gx, gy, def.textures[variantIdx]);
        float tw = static_cast<float>(def.textures[variantIdx].getSize().x);
        float th = static_cast<float>(def.textures[variantIdx].getSize().y);
        pr.sprite.setOrigin({tw / 2.f, th});
        pr.sprite.setPosition(pr.worldPos);
        _placed.push_back(std::move(pr));
    }
}

bool RoadManager::removeAt(int gx, int gy) {
    for (auto it = _placed.begin(); it != _placed.end(); ++it) {
        if (it->gridX == gx && it->gridY == gy) {
            _placed.erase(it);
            return true;
        }
    }
    return false;
}

void RoadManager::drawUI(sf::RenderWindow& w) {
    if (!_open) return;
    drawWindow(w);
    drawList(w);
    drawVariants(w);
    drawInfo(w);
    drawPlaceButton(w);
    w.draw(_closeBtn);
}

void RoadManager::drawWindow(sf::RenderWindow& w) {
    sf::RectangleShape bg({RM_WIN_W, RM_WIN_H});
    bg.setPosition({_winPos.x, _winPos.y});
    bg.setFillColor(sf::Color::Black);
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({RM_WIN_W, RM_TITLEBAR_H});
    tb.setPosition({_winPos.x, _winPos.y});
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "ROADS   " + _texturesDir, RM_FONT_SIZE);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_winPos.x + 6.f, _winPos.y + 6.f});
    w.draw(title);

    sf::RectangleShape sep({2.f, RM_WIN_H - RM_TITLEBAR_H});
    sep.setPosition({_winPos.x + RM_LIST_W + RM_SCROLL_W + 4.f, _winPos.y + RM_TITLEBAR_H});
    sep.setFillColor(sf::Color(80, 80, 80));
    w.draw(sep);

    sf::RectangleShape sepH({RM_LIST_W + RM_SCROLL_W, 2.f});
    sepH.setPosition({_winPos.x + 1.f, _winPos.y + RM_TITLEBAR_H + RM_LIST_H});
    sepH.setFillColor(sf::Color(80, 80, 80));
    w.draw(sepH);
}

void RoadManager::drawList(sf::RenderWindow& w) {
    float listX  = _winPos.x + 4.f;
    float listY  = _winPos.y + RM_TITLEBAR_H + 4.f;
    float rowH   = 22.f;
    int   visRows = static_cast<int>(RM_LIST_H / rowH);

    for (int i = 0; i < visRows; ++i) {
        int defIdx = _scrollOffset + i;
        if (defIdx >= static_cast<int>(_defs.size())) break;

        const auto& def = _defs[defIdx];
        float ry = listY + i * rowH;

        if (defIdx == _selDef) {
            sf::RectangleShape sel({RM_LIST_W - 6.f, rowH - 2.f});
            sel.setPosition({listX - 2.f, ry + 3.f});
            sel.setFillColor(sf::Color::Transparent);
            sel.setOutlineColor(sf::Color(220, 120, 0));
            sel.setOutlineThickness(1.f);
            w.draw(sel);
        }

        sf::Text txt(_font, std::to_string(_scrollOffset + i + 1) + ". " + def.displayName, RM_FONT_SIZE);
        txt.setFillColor(_selDef == defIdx ? sf::Color(255, 160, 30) : sf::Color::White);
        txt.setPosition({listX, ry + 3.f});
        w.draw(txt);
    }

    float scrollBarX = _winPos.x + RM_LIST_W + 2.f;
    float scrollBarY = _winPos.y + RM_TITLEBAR_H + 2.f;
    sf::RectangleShape scrollBg({RM_SCROLL_W - 2.f, RM_LIST_H - 1.f});
    scrollBg.setPosition({scrollBarX, scrollBarY});
    scrollBg.setFillColor(sf::Color(30, 30, 30));
    scrollBg.setOutlineColor(sf::Color(80, 80, 80));
    scrollBg.setOutlineThickness(1.f);
    w.draw(scrollBg);

    int total = static_cast<int>(_defs.size());
    if (total > 0) {
        float thumbH = RM_LIST_H - 1.f;
        if (total > visRows)
            thumbH = std::max(20.f, (RM_LIST_H - 1.f) * (static_cast<float>(visRows) / total));
        float maxScroll = std::max(1, total - visRows);
        float thumbY = scrollBarY + (_scrollOffset / static_cast<float>(maxScroll)) * ((RM_LIST_H - 1.f) - thumbH);
        sf::RectangleShape thumb({RM_SCROLL_W - 4.f, thumbH});
        thumb.setPosition({scrollBarX + 1.f, thumbY});
        thumb.setFillColor(sf::Color(130, 60, 0));
        w.draw(thumb);
    }
}

void RoadManager::drawVariants(sf::RenderWindow& w) {
    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    float vx = _winPos.x + RM_LIST_W + RM_SCROLL_W + 18.f;
    float vy = _winPos.y + RM_TITLEBAR_H + 10.f;
    int cols = std::max(1, static_cast<int>(RM_PREVIEW_W / (RM_VARIANT_SIZE + RM_VARIANT_PAD)));

    for (int vi = 0; vi < static_cast<int>(def.textures.size()); ++vi) {
        int col = vi % cols;
        int row = vi / cols;
        float cx = vx + col * (RM_VARIANT_SIZE + RM_VARIANT_PAD);
        float cy = vy + row * (RM_VARIANT_SIZE + RM_VARIANT_PAD);

        sf::RectangleShape tile({RM_VARIANT_SIZE, RM_VARIANT_SIZE});
        tile.setPosition({cx, cy});
        tile.setFillColor(sf::Color(15, 15, 15));
        tile.setOutlineThickness(vi == _selVar ? 1.f : 0.f);
        tile.setOutlineColor(sf::Color(220, 120, 0));
        w.draw(tile);

        sf::Sprite spr(def.textures[vi]);
        float sx = spr.getGlobalBounds().size.x;
        float sy = spr.getGlobalBounds().size.y;
        float scale = std::min((RM_VARIANT_SIZE - 8.f) / sx, (RM_VARIANT_SIZE - 8.f) / sy);
        spr.setScale({scale, scale});
        spr.setPosition({cx + (RM_VARIANT_SIZE - sx * scale) / 2.f, cy + (RM_VARIANT_SIZE - sy * scale) / 2.f});
        w.draw(spr);
    }
}

void RoadManager::drawInfo(sf::RenderWindow& w) {
    if (_selDef < 0) return;
    const auto& def = _defs[_selDef];

    float infoX = _winPos.x + 4.f;
    float infoY = _winPos.y + RM_TITLEBAR_H + RM_LIST_H + 6.f;

    sf::Text nameT(_font, "Road info " + def.displayName + ":", RM_FONT_SIZE);
    nameT.setFillColor(sf::Color::White);
    nameT.setStyle(sf::Text::Style::Bold);
    nameT.setPosition({infoX, infoY});
    w.draw(nameT);

    sf::Text descT(_font, "", RM_FONT_SIZE - 1);
    descT.setFillColor(sf::Color(200, 200, 200));
    descT.setPosition({infoX, infoY + 18.f});

    std::string wrapped;
    std::string currentLine;
    std::stringstream ss(def.description);
    std::string word;
    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        descT.setString(testLine);
        if (descT.getLocalBounds().size.x > RM_LIST_W - 8.f) {
            wrapped += currentLine + "\n";
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    wrapped += currentLine;
    descT.setString(wrapped);
    w.draw(descT);

    sf::Text costT(_font, "Costs " + std::to_string(def.cost) + "$ per tile", RM_FONT_SIZE);
    costT.setFillColor(sf::Color::White);
    costT.setPosition({infoX, infoY + RM_INFO_H - 42.f});
    w.draw(costT);

    sf::Text matT(_font, "Materials: " + def.materials, RM_FONT_SIZE);
    matT.setFillColor(sf::Color::White);
    matT.setPosition({infoX, infoY + RM_INFO_H - 24.f});
    w.draw(matT);
}

void RoadManager::drawPlaceButton(sf::RenderWindow& w) {
    float btnX = _winPos.x + 4.f;
    float btnY = _winPos.y + RM_TITLEBAR_H + RM_LIST_H + RM_INFO_H + 6.f;

    bool active = (_selDef >= 0 && _selVar >= 0);
    sf::RectangleShape btn({RM_LIST_W - 8.f, RM_BTN_H});
    btn.setPosition({btnX, btnY});
    btn.setFillColor(active ? sf::Color(200, 100, 0) : sf::Color(60, 60, 60));
    w.draw(btn);

    std::string btnLabel = "PLACE ROAD";
    if (_placingState == RoadPlacingState::AWAITING_START)
        btnLabel = "CLICK START POINT";
    else if (_placingState == RoadPlacingState::DRAGGING)
        btnLabel = "CLICK END POINT";

    sf::Text btnTxt(_font, btnLabel, RM_FONT_SIZE + 1);
    btnTxt.setFillColor(sf::Color::White);
    btnTxt.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = btnTxt.getLocalBounds();
    btnTxt.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
    btnTxt.setPosition({btnX + (RM_LIST_W - 8.f) / 2.f, btnY + RM_BTN_H / 2.f});
    w.draw(btnTxt);
}

void RoadManager::drawWorldOverlay(sf::RenderWindow& w, float stepX, float stepY, const BuildingManager* bm, const GeneratedWorld* world) {
    for (const auto& ft : _floatingTexts)
        w.draw(ft.text);

    if (_placingState == RoadPlacingState::NONE) return;
    if (_selDef < 0 || _selVar < 0) return;

    const RoadDef& def = _defs[_selDef];
    float hlTexW = static_cast<float>(_hlTexture.getSize().x);
    float hlTexH = static_cast<float>(_hlTexture.getSize().y);

    if (_placingState == RoadPlacingState::DRAGGING && !_ghostPositions.empty()) {
        sf::Color originalColor = _hlSprite.getColor();
        for (const auto& [gx, gy] : _ghostPositions) {
            bool occupied = isOccupied(gx, gy, def.gridW, def.gridH, bm, world);
            sf::Color tileColor = occupied ? sf::Color(255, 0, 0, 220) : originalColor; 
            sf::Color ghostColor = occupied ? sf::Color(255, 80, 80, 160) : sf::Color(255, 255, 255, 160);

            sf::Vector2f tilePos = gridToWorld(gx, gy, stepX, stepY);

            _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
            _hlSprite.setPosition(tilePos);
            _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});
            _hlSprite.setColor(tileColor);
            w.draw(_hlSprite);

            sf::Vector2f wp = tilePos;
            wp.y += stepY * 2.f;
            sf::Sprite gs(def.textures[_selVar]);
            float tw = static_cast<float>(def.textures[_selVar].getSize().x);
            float th = static_cast<float>(def.textures[_selVar].getSize().y);
            gs.setOrigin({tw / 2.f, th});
            gs.setPosition(wp);
            gs.setColor(ghostColor);
            w.draw(gs);
        }
        _hlSprite.setColor(originalColor);
        return;
    }

    if ((_placingState == RoadPlacingState::SINGLE || _placingState == RoadPlacingState::AWAITING_START) && _ghostValid) {
        sf::Vector2f tilePos = _ghostSprite.getPosition();
        tilePos.y -= stepY * 2.f;

        _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
        _hlSprite.setPosition(tilePos);
        _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});
        w.draw(_hlSprite);
        w.draw(_ghostSprite);
    }
}
