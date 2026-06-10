#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>

constexpr float DP_CLOSE_SZ  = 24.f;
constexpr float DP_OFFSET_X  = 100.f;
constexpr float DP_OFFSET_Y  = 100.f;

inline sf::Vector2f worldPosToUi(sf::Vector2f world, sf::RenderWindow& w, const sf::View& gameView) {
    sf::Vector2i px = w.mapCoordsToPixel(world, gameView);
    return w.mapPixelToCoords(px, w.getDefaultView());
}

inline sf::Vector2f panelPosBesideBuilding(sf::Vector2f buildingWorld, sf::RenderWindow& w,
                                           const sf::View& gameView, float panelW, float panelH) {
    sf::Vector2f pos = worldPosToUi(buildingWorld, w, gameView) + sf::Vector2f(DP_OFFSET_X, DP_OFFSET_Y);
    sf::Vector2u sz = w.getSize();
    pos.x = std::clamp(pos.x, 8.f, static_cast<float>(sz.x) - panelW - 8.f);
    pos.y = std::clamp(pos.y, 8.f, static_cast<float>(sz.y) - panelH - 8.f);
    return pos;
}

inline sf::FloatRect detailCloseRect(sf::Vector2f panelPos, float panelW, float titlebarH) {
    float x = panelPos.x + panelW - DP_CLOSE_SZ - 8.f;
    float y = panelPos.y + (titlebarH - DP_CLOSE_SZ) * 0.5f;
    return sf::FloatRect({x, y}, {DP_CLOSE_SZ, DP_CLOSE_SZ});
}

inline void scaleCloseSprite(sf::Sprite& spr, const sf::Texture& tex, float targetSize = DP_CLOSE_SZ) {
    auto ts = tex.getSize();
    float maxDim = static_cast<float>(ts.x > ts.y ? ts.x : ts.y);
    if (maxDim > 0.f)
        spr.setScale({targetSize / maxDim, targetSize / maxDim});
}

inline void drawDetailCloseButton(sf::RenderWindow& w, const sf::Sprite& closeBtn,
                                  sf::Vector2f panelPos, float panelW, float titlebarH,
                                  sf::FloatRect& outHitRect) {
    outHitRect = detailCloseRect(panelPos, panelW, titlebarH);
    sf::Sprite spr = closeBtn;
    spr.setPosition(outHitRect.position);
    w.draw(spr);
}
