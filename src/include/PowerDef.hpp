#pragma once

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

struct PowerDef {
    std::string id;
    std::string displayName;
    std::string description;
    int         cost = 0;
    std::string materials;
    int         gridW = 1;
    int         gridH = 1;
    int         powerOutput = 0;
    std::vector<sf::Texture> textures;
};

struct PlacedPower {
    std::string  defId;
    int          variantIdx;
    sf::Vector2f worldPos;
    int          gridX;
    int          gridY;
    sf::Sprite   sprite;

    PlacedPower(const std::string& id, int vIdx, sf::Vector2f wPos, int gX, int gY, const sf::Texture& tex)
        : defId(id), variantIdx(vIdx), worldPos(wPos), gridX(gX), gridY(gY), sprite(tex) {}
};
