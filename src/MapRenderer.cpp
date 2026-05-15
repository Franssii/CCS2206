#include "include/MapRenderer.hpp"
#include <algorithm>
#include <cmath>

MapRenderer::MapRenderer(const sf::Texture& bareTexture,
                         int mapWidthChunks, int mapHeightChunks,
                         float bareSpacing)
    : _bareSprite(bareTexture),
      _mapWidthChunks(mapWidthChunks),
      _mapHeightChunks(mapHeightChunks),
      _bareSpacing(bareSpacing)
{
}

static float tileHash(int x, int y, unsigned int seed) {
    unsigned int h = static_cast<unsigned int>(x * 374761393) ^ static_cast<unsigned int>(y * 668265263) ^ seed;
    h = (h ^ (h >> 13)) * 1274126177;
    return static_cast<float>(h & 0xFFFF) / 65535.f;
}

sf::Vector2f MapRenderer::screenToGrid(float sx, float sy, float stepX, float stepY) const {
    float mapX = (sx / stepX + sy / stepY) / 2.f;
    float mapY = (sy / stepY - sx / stepX) / 2.f;
    return {mapX, mapY};
}

void MapRenderer::draw(sf::RenderWindow& window, const sf::View& view, int temperature) {
    sf::Vector2f viewSize   = view.getSize();
    sf::Vector2f viewCenter = view.getCenter();
    float viewLeft   = viewCenter.x - viewSize.x / 2.f;
    float viewTop    = viewCenter.y - viewSize.y / 2.f;
    float viewRight  = viewLeft + viewSize.x;
    float viewBottom = viewTop  + viewSize.y;

    float bareWidth  = _bareSprite.getGlobalBounds().size.x;
    float bareHeight = _bareSprite.getGlobalBounds().size.y;

    _stepX = (bareWidth  + _bareSpacing) / 2.f;
    _stepY = (bareHeight + _bareSpacing) / 2.f;

    if (_stepX <= 0 || _stepY <= 0) return;

    sf::Vector2f TL = screenToGrid(viewLeft,  viewTop,    _stepX, _stepY);
    sf::Vector2f TR = screenToGrid(viewRight, viewTop,    _stepX, _stepY);
    sf::Vector2f BL = screenToGrid(viewLeft,  viewBottom, _stepX, _stepY);
    sf::Vector2f BR = screenToGrid(viewRight, viewBottom, _stepX, _stepY);

    int minGridX = static_cast<int>(std::floor(std::min({TL.x, TR.x, BL.x, BR.x}))) - 1;
    int maxGridX = static_cast<int>(std::ceil (std::max({TL.x, TR.x, BL.x, BR.x}))) + 1;
    int minGridY = static_cast<int>(std::floor(std::min({TL.y, TR.y, BL.y, BR.y}))) - 1;
    int maxGridY = static_cast<int>(std::ceil (std::max({TL.y, TR.y, BL.y, BR.y}))) + 1;

    unsigned int seed = _world ? _world->settings.seed : 12345;

    auto drawSnowOverlay = [&](float px, float py, int gridX, int gridY) {
        if (temperature >= 0) return;
        
        float h = tileHash(gridX, gridY, seed);
        float tempAbs = static_cast<float>(std::abs(temperature));
        int snowType = 0; 
        
        if (tempAbs <= 5.f) {
            float p1 = tempAbs / 5.f; 
            if (h < p1) snowType = 1;
        } else if (tempAbs <= 12.f) {
            float p2 = (tempAbs - 5.f) / 7.f;
            if (h < p2) snowType = 2;
            else snowType = 1;
        } else if (tempAbs <= 20.f) {
            float p3 = (tempAbs - 12.f) / 8.f;
            if (h < p3) snowType = 3;
            else snowType = 2;
        } else {
            snowType = 3;
        }

        if (snowType == 1 && _snow1) {
            sf::Sprite spr(*_snow1);
            spr.setPosition({px, py});
            window.draw(spr);
        } else if (snowType == 2 && _snow2) {
            sf::Sprite spr(*_snow2);
            spr.setPosition({px, py});
            window.draw(spr);
        } else if (snowType == 3 && _snow3) {
            sf::Sprite spr(*_snow3);
            spr.setPosition({px, py});
            window.draw(spr);
        }
    };

    if (_world) {
        minGridX = std::max(0, minGridX);
        maxGridX = std::min(_world->width,  maxGridX + 1);
        minGridY = std::max(0, minGridY);
        maxGridY = std::min(_world->height, maxGridY + 1);

        for (int x = minGridX; x < maxGridX; ++x) {
            for (int y = minGridY; y < maxGridY; ++y) {
                TileType t = _world->get(x, y);
                if (t == TileType::EMPTY) continue;

                float isoX = static_cast<float>(x - y) * _stepX;
                float isoY = static_cast<float>(x + y) * _stepY;

                auto it = _texMap.find(t);
                if (it != _texMap.end() && it->second) {
                    if (t == TileType::TREE) {
                        
                        auto bare_it = _texMap.find(TileType::BARE);
                        if (bare_it != _texMap.end() && bare_it->second) {
                            sf::Sprite baseSpr(*bare_it->second);
                            baseSpr.setPosition({isoX - bareWidth / 2.f, isoY});
                            window.draw(baseSpr);
                        drawSnowOverlay(isoX - bareWidth / 2.f, isoY, x, y);
                        } else {
                            _bareSprite.setPosition({isoX - bareWidth / 2.f, isoY});
                            window.draw(_bareSprite);
                        drawSnowOverlay(isoX - bareWidth / 2.f, isoY, x, y);
                        }
                    } else {
                        sf::Sprite spr(*it->second);
                        float w = spr.getGlobalBounds().size.x;
                        float h = spr.getGlobalBounds().size.y;
                        spr.setPosition({isoX - w / 2.f, isoY});
                        window.draw(spr);

                        if (t == TileType::BARE || t == TileType::BARE_WATER || t == TileType::COAL || t == TileType::IRON) {
                            drawSnowOverlay(isoX - bareWidth / 2.f, isoY, x, y);
                        }
                    }
                } else {
                    _bareSprite.setPosition({isoX - bareWidth / 2.f, isoY});
                    window.draw(_bareSprite);
                drawSnowOverlay(isoX - bareWidth / 2.f, isoY, x, y);
                }

                bool upLeft    = _world->get(x - 1, y) == TileType::EMPTY;
                bool upRight   = _world->get(x, y - 1) == TileType::EMPTY;
                bool downLeft  = _world->get(x, y + 1) == TileType::EMPTY;
                bool downRight = _world->get(x + 1, y) == TileType::EMPTY;

                if (upLeft || upRight || downLeft || downRight) {
                    float isoX = static_cast<float>(x - y) * _stepX;
                    float isoY = static_cast<float>(x + y) * _stepY;

                    auto drawThickLine = [&](sf::Vector2f p1, sf::Vector2f p2) {
                        sf::Vector2f dir = p2 - p1;
                        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len == 0.f) return;
                        dir /= len;
                        sf::Vector2f normal(-dir.y, dir.x);
                        float thickness = 3.0f;
                        sf::ConvexShape line(4);
                        line.setPoint(0, p1 + normal * (thickness / 2.f));
                        line.setPoint(1, p2 + normal * (thickness / 2.f));
                        line.setPoint(2, p2 - normal * (thickness / 2.f));
                        line.setPoint(3, p1 - normal * (thickness / 2.f));
                        line.setFillColor(sf::Color(255, 0, 0, 200));
                        window.draw(line);
                    };

                    sf::Vector2f top(isoX, isoY);
                    sf::Vector2f right(isoX + bareWidth / 2.f, isoY + bareHeight / 2.f);
                    sf::Vector2f bottom(isoX, isoY + bareHeight);
                    sf::Vector2f left(isoX - bareWidth / 2.f, isoY + bareHeight / 2.f);

                    if (upLeft)    drawThickLine(top, left);
                    if (upRight)   drawThickLine(top, right);
                    if (downRight) drawThickLine(right, bottom);
                    if (downLeft)  drawThickLine(left, bottom);
                }

                if (t == TileType::TREE && it != _texMap.end() && it->second) {
                    sf::Sprite spr(*it->second);
                    float w = spr.getGlobalBounds().size.x;
                    float h = spr.getGlobalBounds().size.y;
                    spr.setPosition({isoX - w / 2.f, isoY + bareHeight / 2.f - h + bareHeight * 0.25f});
                    window.draw(spr);
                }
            }
        }
    } else {
        const int totalGridWidth  = _mapWidthChunks  * 6;
        const int totalGridHeight = _mapHeightChunks * 6;
        int startGridX = std::max(0, minGridX);
        int endGridX   = std::min(totalGridWidth,  maxGridX + 1);
        int startGridY = std::max(0, minGridY);
        int endGridY   = std::min(totalGridHeight, maxGridY + 1);

        for (int x = startGridX; x < endGridX; ++x) {
            for (int y = startGridY; y < endGridY; ++y) {
                float isoX = static_cast<float>(x - y) * _stepX;
                float isoY = static_cast<float>(x + y) * _stepY;
                _bareSprite.setPosition({isoX - bareWidth / 2.f, isoY});
                window.draw(_bareSprite);
                drawSnowOverlay(isoX - bareWidth / 2.f, isoY, x, y);
            }
        }
    }
}
