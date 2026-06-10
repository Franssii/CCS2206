#include "include/RoadManager.hpp"
#include "include/BuildingManager.hpp"
#include "include/IndustrialManager.hpp"
#include "include/CitizenManager.hpp"
#include "include/IndustryWorkers.hpp"
#include "include/DetailPanelUtil.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
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
    scaleCloseSprite(_closeBtn, closeTexture);
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

namespace {

const int kNeighborDx[] = { 1, -1, 0, 0 };
const int kNeighborDy[] = { 0, 0, 1, -1 };

bool industrialTouchesRoadSet(int gx, int gy, int w, int h,
                              const std::map<std::pair<int,int>, int>& dist)
{
    for (int y = gy; y < gy + h; ++y) {
        for (int x = gx; x < gx + w; ++x) {
            for (int n = 0; n < 4; ++n) {
                int nx = x + kNeighborDx[n];
                int ny = y + kNeighborDy[n];
                if (dist.count({nx, ny})) return true;
            }
        }
    }
    return false;
}

} // namespace

bool RoadManager::isRoadAt(int gx, int gy) const {
    for (const auto& pr : _placed) {
        if (pr.gridX == gx && pr.gridY == gy) return true;
    }
    return false;
}

bool RoadManager::getRoadDefIdAt(int gx, int gy, std::string& outDefId) const {
    for (const auto& pr : _placed) {
        if (pr.gridX == gx && pr.gridY == gy) {
            outDefId = pr.defId;
            return true;
        }
    }
    return false;
}

static bool roadIdEquals(const std::string& id, const char* target) {
    if (id.size() != std::strlen(target)) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(id[i])) !=
            std::tolower(static_cast<unsigned char>(target[i])))
            return false;
    }
    return true;
}

bool RoadManager::isBusStopAt(int gx, int gy) const {
    std::string id;
    return getRoadDefIdAt(gx, gy, id) && roadIdEquals(id, "Bus_stop");
}

bool RoadManager::isVehicleStationAt(int gx, int gy) const {
    std::string id;
    return getRoadDefIdAt(gx, gy, id) && roadIdEquals(id, "Vehicle_station");
}

bool RoadManager::findBusStopAt(int gx, int gy, int& outX, int& outY) const {
    if (isBusStopAt(gx, gy)) {
        outX = gx;
        outY = gy;
        return true;
    }
    for (int n = 0; n < 4; ++n) {
        int nx = gx + kNeighborDx[n];
        int ny = gy + kNeighborDy[n];
        if (isBusStopAt(nx, ny)) {
            outX = nx;
            outY = ny;
            return true;
        }
    }
    return false;
}

std::vector<std::pair<int,int>> RoadManager::findRoadPath(int fromX, int fromY,
                                                          int toX, int toY,
                                                          int maxSteps) const
{
    std::vector<std::pair<int,int>> result;
    if (!isRoadAt(fromX, fromY) || !isRoadAt(toX, toY)) return result;
    if (fromX == toX && fromY == toY) return {{fromX, fromY}};

    std::map<std::pair<int,int>, int> dist;
    std::map<std::pair<int,int>, std::pair<int,int>> parent;
    std::vector<std::pair<int,int>> queue;

    dist[{fromX, fromY}] = 0;
    parent[{fromX, fromY}] = {fromX, fromY};
    queue.push_back({fromX, fromY});

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        auto [cx, cy] = queue[qi];
        int d = dist[{cx, cy}];
        if (cx == toX && cy == toY) break;
        if (d >= maxSteps) continue;

        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (!isRoadAt(nx, ny)) continue;
            if (dist.count({nx, ny})) continue;
            dist[{nx, ny}] = d + 1;
            parent[{nx, ny}] = {cx, cy};
            queue.push_back({nx, ny});
        }
    }

    if (!dist.count({toX, toY})) return result;

    for (auto cur = std::make_pair(toX, toY); ; ) {
        result.push_back(cur);
        auto par = parent[cur];
        if (par.first == cur.first && par.second == cur.second) break;
        cur = par;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

bool RoadManager::hasRoadAdjacentToRect(int gx, int gy, int w, int h) const {
    for (int y = gy; y < gy + h; ++y) {
        for (int x = gx; x < gx + w; ++x) {
            for (int n = 0; n < 4; ++n) {
                if (isRoadAt(x + kNeighborDx[n], y + kNeighborDy[n])) return true;
            }
        }
    }
    return false;
}

std::vector<int> RoadManager::findReachableIndustrialIndices(int houseGx, int houseGy,
                                                             int houseW, int houseH,
                                                             const IndustrialManager& im,
                                                             int maxSteps) const
{
    std::vector<int> result;
    if (houseW < 1) houseW = 1;
    if (houseH < 1) houseH = 1;

    if (!hasRoadAdjacentToRect(houseGx, houseGy, houseW, houseH))
        return result;

    std::map<std::pair<int,int>, int> dist;
    std::vector<std::pair<int,int>> queue;

    for (int y = houseGy; y < houseGy + houseH; ++y) {
        for (int x = houseGx; x < houseGx + houseW; ++x) {
            for (int n = 0; n < 4; ++n) {
                int rx = x + kNeighborDx[n];
                int ry = y + kNeighborDy[n];
                if (!isRoadAt(rx, ry)) continue;
                if (dist.count({rx, ry})) continue;
                dist[{rx, ry}] = 0;
                queue.push_back({rx, ry});
            }
        }
    }

    if (queue.empty()) return result;

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        auto [cx, cy] = queue[qi];
        int d = dist[{cx, cy}];
        if (d >= maxSteps) continue;

        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (!isRoadAt(nx, ny)) continue;
            if (dist.count({nx, ny})) continue;
            dist[{nx, ny}] = d + 1;
            queue.push_back({nx, ny});
        }
    }

    const auto& placed = im.getPlaced();
    for (size_t i = 0; i < placed.size(); ++i) {
        const auto& ind = placed[i];
        int iw = 1, ih = 1;
        im.getGridSize(ind.defId, iw, ih);
        if (industrialTouchesRoadSet(ind.gridX, ind.gridY, iw, ih, dist))
            result.push_back(static_cast<int>(i));
    }
    return result;
}

void RoadManager::drawWorkplaceHighlights(sf::RenderWindow& w, float stepX, float stepY,
                                          const IndustrialManager& im)
{
    if (_highlightIndIndices.empty()) return;

    float hlTexW = static_cast<float>(_hlTexture.getSize().x);
    float hlTexH = static_cast<float>(_hlTexture.getSize().y);
    _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
    _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});
    _hlSprite.setColor(sf::Color(0, 255, 0, 200));

    const auto& placed = im.getPlaced();
    for (int idx : _highlightIndIndices) {
        if (idx < 0 || idx >= static_cast<int>(placed.size())) continue;
        const auto& ind = placed[idx];
        int iw = 1, ih = 1;
        im.getGridSize(ind.defId, iw, ih);
        for (int dy = 0; dy < ih; ++dy) {
            for (int dx = 0; dx < iw; ++dx) {
                sf::Vector2f tPos = gridToWorld(ind.gridX + dx, ind.gridY + dy, stepX, stepY);
                _hlSprite.setPosition(tPos);
                w.draw(_hlSprite);
            }
        }
    }
}

std::vector<std::pair<int,int>> RoadManager::findReachableBusStops(int houseGx, int houseGy,
                                                                    int houseW, int houseH,
                                                                    int maxSteps) const
{
    std::vector<std::pair<int,int>> result;
    if (houseW < 1) houseW = 1;
    if (houseH < 1) houseH = 1;
    if (!hasRoadAdjacentToRect(houseGx, houseGy, houseW, houseH))
        return result;

    std::map<std::pair<int,int>, int> dist;
    std::vector<std::pair<int,int>> queue;

    for (int y = houseGy; y < houseGy + houseH; ++y) {
        for (int x = houseGx; x < houseGx + houseW; ++x) {
            for (int n = 0; n < 4; ++n) {
                int rx = x + kNeighborDx[n];
                int ry = y + kNeighborDy[n];
                if (!isRoadAt(rx, ry)) continue;
                if (dist.count({rx, ry})) continue;
                dist[{rx, ry}] = 0;
                queue.push_back({rx, ry});
            }
        }
    }
    if (queue.empty()) return result;

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        auto [cx, cy] = queue[qi];
        int d = dist[{cx, cy}];
        if (d >= maxSteps) continue;

        if (isBusStopAt(cx, cy))
            result.push_back({cx, cy});

        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (!isRoadAt(nx, ny)) continue;
            if (dist.count({nx, ny})) continue;
            dist[{nx, ny}] = d + 1;
            queue.push_back({nx, ny});
        }
    }
    return result;
}

void RoadManager::drawBusStopHighlights(sf::RenderWindow& w, float stepX, float stepY) {
    if (!_highlightBusStops && _highlightAssignBusStops.empty()) return;

    float hlTexW = static_cast<float>(_hlTexture.getSize().x);
    float hlTexH = static_cast<float>(_hlTexture.getSize().y);
    _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
    _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});
    _hlSprite.setColor(sf::Color(0, 255, 0, 200));

    if (_highlightBusStops) {
        for (const auto& pr : _placed) {
            if (!isBusStopAt(pr.gridX, pr.gridY)) continue;
            sf::Vector2f tPos = gridToWorld(pr.gridX, pr.gridY, stepX, stepY);
            _hlSprite.setPosition(tPos);
            w.draw(_hlSprite);
        }
    }

    if (!_highlightAssignBusStops.empty()) {
        _hlSprite.setColor(sf::Color(0, 180, 255, 200));
        for (const auto& [bx, by] : _highlightAssignBusStops) {
            sf::Vector2f tPos = gridToWorld(bx, by, stepX, stepY);
            _hlSprite.setPosition(tPos);
            w.draw(_hlSprite);
        }
        _hlSprite.setColor(sf::Color(0, 255, 0, 200));
    }
}

bool RoadManager::findNearestBusStop(int gx, int gy, int w, int h,
                                     int& outX, int& outY, int maxSteps) const
{
    outX = outY = -1;
    if (!hasRoadAdjacentToRect(gx, gy, w, h)) return false;

    std::map<std::pair<int,int>, int> dist;
    std::vector<std::pair<int,int>> queue;

    for (int y = gy; y < gy + h; ++y) {
        for (int x = gx; x < gx + w; ++x) {
            for (int n = 0; n < 4; ++n) {
                int rx = x + kNeighborDx[n];
                int ry = y + kNeighborDy[n];
                if (!isRoadAt(rx, ry)) continue;
                if (dist.count({rx, ry})) continue;
                dist[{rx, ry}] = 0;
                queue.push_back({rx, ry});
            }
        }
    }
    if (queue.empty()) return false;

    int bestDist = maxSteps + 1;
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        auto [cx, cy] = queue[qi];
        int d = dist[{cx, cy}];
        if (d >= maxSteps) continue;

        if (isBusStopAt(cx, cy) && d < bestDist) {
            bestDist = d;
            outX = cx;
            outY = cy;
        }

        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (!isRoadAt(nx, ny)) continue;
            if (dist.count({nx, ny})) continue;
            dist[{nx, ny}] = d + 1;
            queue.push_back({nx, ny});
        }
    }
    return outX >= 0;
}

bool RoadManager::tryOpenStopDetailAtWorld(sf::Vector2f worldPos, float stepX, float stepY,
                                           sf::RenderWindow& window, const sf::View& gameView)
{
    sf::Vector2i g = worldToGrid(worldPos, stepX, stepY);
    int stopX = 0, stopY = 0;
    if (!findBusStopAt(g.x, g.y, stopX, stopY)) return false;

    _stopDetailOpen = true;
    _stopDetailGX = stopX;
    _stopDetailGY = stopY;
    sf::Vector2f world = gridToWorld(stopX, stopY, stepX, stepY);
    _stopDetailH = RM_STOP_DETAIL_MIN_H;
    _stopDetailPos = panelPosBesideBuilding(world, window, gameView,
                                           RM_STOP_DETAIL_W, _stopDetailH);
    _stopDetailCloseRect = detailCloseRect(_stopDetailPos, RM_STOP_DETAIL_W, RM_TITLEBAR_H);
    return true;
}

float RoadManager::computeStopDetailHeight(const IndustrialManager* im,
                                           const IndustryWorkers* iw) const {
    float lineH = static_cast<float>(RM_FONT_SIZE) + 10.f;
    float h = RM_TITLEBAR_H + 14.f + 5.f * lineH + 10.f;
    if (im && iw) {
        auto reachable = const_cast<RoadManager*>(this)->findReachableIndustrialIndices(
            _stopDetailGX, _stopDetailGY, 1, 1, *im, 20);
        int btnCount = 0;
        for (int idx : reachable) {
            if (idx >= 0 && idx < static_cast<int>(im->getPlaced().size())) {
                const auto& ind = im->getPlaced()[idx];
                if (iw->maxWorkersFor(ind.defId) > 0) ++btnCount;
            }
        }
        h += static_cast<float>(btnCount) * 36.f;
    }
    return std::max(RM_STOP_DETAIL_MIN_H, h);
}

void RoadManager::closeStopDetail() {
    _stopDetailOpen = false;
}

bool RoadManager::handleStopDetailEvent(const sf::Event& event, sf::RenderWindow& window,
                                        CitizenManager* cm, const IndustrialManager* im,
                                        const IndustryWorkers* iw, BuildingManager* bm) {
    if (!_stopDetailOpen) return false;
    _stopDetailMousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window),
                                                   window.getDefaultView());

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            closeStopDetail();
            return true;
        }
        return false;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (_stopDetailCloseRect.contains(_stopDetailMousePos)) {
                closeStopDetail();
                return true;
            }
            for (const auto& [rect, id] : _stopDetailHits) {
                if (!rect.contains(_stopDetailMousePos)) continue;
                if (id.rfind("fac_", 0) == 0 && cm && im && iw && bm) {
                    int fidx = std::atoi(id.c_str() + 4);
                    cm->assignFactoryAtStop(_stopDetailGX, _stopDetailGY, fidx,
                                            *bm, *this, *im, *iw, 20);
                }
                return true;
            }
        }
    }
    return _stopDetailOpen;
}

void RoadManager::drawStopDetailPanel(sf::RenderWindow& w, const CitizenManager* cm,
                                      const IndustrialManager* im, const IndustryWorkers* iw,
                                      const BuildingManager* bm) {
    if (!_stopDetailOpen) return;

    _stopDetailMousePos = w.mapPixelToCoords(sf::Mouse::getPosition(w), w.getDefaultView());
    _stopDetailH = computeStopDetailHeight(im, iw);
    _stopDetailHits.clear();

    sf::RectangleShape bg({RM_STOP_DETAIL_W, _stopDetailH});
    bg.setPosition(_stopDetailPos);
    bg.setFillColor(sf::Color(10, 10, 10, 245));
    bg.setOutlineColor(sf::Color::White);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({RM_STOP_DETAIL_W, RM_TITLEBAR_H});
    tb.setPosition(_stopDetailPos);
    tb.setFillColor(sf::Color(200, 100, 0));
    w.draw(tb);

    sf::Text title(_font, "BUS STOP", RM_FONT_SIZE + 2);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({_stopDetailPos.x + 10.f, _stopDetailPos.y + 6.f});
    w.draw(title);
    drawDetailCloseButton(w, _closeBtn, _stopDetailPos, RM_STOP_DETAIL_W, RM_TITLEBAR_H,
                          _stopDetailCloseRect);

    int waitingAt = 0, walkingTo = 0, pending = 0;
    int assignedFac = -1;
    if (cm) {
        cm->getStopCounts(_stopDetailGX, _stopDetailGY, waitingAt, walkingTo, pending, bm);
        assignedFac = cm->getStopAssignedFactory(_stopDetailGX, _stopDetailGY);
    }

    float x = _stopDetailPos.x + 14.f;
    float y = _stopDetailPos.y + RM_TITLEBAR_H + 14.f;
    float lineH = static_cast<float>(RM_FONT_SIZE) + 10.f;
    float btnW = RM_STOP_DETAIL_W - 28.f;

    auto drawLine = [&](const std::string& txt, sf::Color col = sf::Color(220, 220, 220)) {
        sf::Text t(_font, txt, RM_FONT_SIZE);
        t.setFillColor(col);
        t.setPosition({x, y});
        w.draw(t);
        y += lineH;
    };

    drawLine("Grid: " + std::to_string(_stopDetailGX) + ", " + std::to_string(_stopDetailGY),
             sf::Color(180, 180, 180));
    drawLine("At stop (waiting for bus): " + std::to_string(waitingAt), sf::Color(100, 220, 255));
    drawLine("Walking to stop: " + std::to_string(walkingTo), sf::Color(120, 200, 255));
    drawLine("Homes assigned (no factory yet): " + std::to_string(pending), sf::Color(255, 200, 100));

    if (assignedFac >= 0 && im)
        drawLine("Route factory: " + im->getDisplayName(im->getPlaced()[assignedFac].defId),
                 sf::Color(120, 255, 120));
    else
        drawLine("Route factory: not set — pick below", sf::Color(255, 140, 80));

    y += 4.f;
    drawLine("--- Send commuters to factory ---", sf::Color(255, 160, 30));

    if (im && iw) {
        auto reachable = findReachableIndustrialIndices(
            _stopDetailGX, _stopDetailGY, 1, 1, *im, 20);
        bool any = false;
        for (int idx : reachable) {
            if (idx < 0 || idx >= static_cast<int>(im->getPlaced().size())) continue;
            const auto& ind = im->getPlaced()[idx];
            if (iw->maxWorkersFor(ind.defId) <= 0) continue;
            any = true;
            std::string lbl = "-> " + im->getDisplayName(ind.defId);
            sf::FloatRect rect({x, y}, {btnW, 30.f});
            bool hover = rect.contains(_stopDetailMousePos);
            sf::RectangleShape b({btnW, 30.f});
            b.setPosition({x, y});
            b.setFillColor(hover ? sf::Color(240, 140, 30) : sf::Color(200, 100, 0));
            w.draw(b);
            sf::Text t(_font, lbl, RM_FONT_SIZE);
            t.setFillColor(sf::Color::White);
            t.setStyle(sf::Text::Style::Bold);
            sf::FloatRect tbounds = t.getLocalBounds();
            t.setOrigin({tbounds.position.x + tbounds.size.x / 2.f,
                         tbounds.position.y + tbounds.size.y / 2.f});
            t.setPosition({x + btnW / 2.f, y + 15.f});
            w.draw(t);
            _stopDetailHits.push_back({rect, "fac_" + std::to_string(idx)});
            y += 36.f;
        }
        if (!any)
            drawLine("No workplaces within 20 road tiles.", sf::Color(150, 150, 150));
    }
}
