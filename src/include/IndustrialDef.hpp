#pragma once
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

struct IndustrialDef {
    std::string id;
    std::string displayName;
    std::string description;
    int         cost = 0;
    std::string materials;
    bool        isDraggable = false;

    std::vector<sf::Texture> textures;

    int gridW = 1; // width in grid tiles
    int gridH = 1; // height in grid tiles
};

struct PlacedIndustrial {
    std::string  defId;
    int          variantIdx;
    sf::Vector2f worldPos;
    int          gridX;
    int          gridY;
    sf::Sprite   sprite;

    PlacedIndustrial(const std::string& id, int vIdx, sf::Vector2f wPos,
                     int gX, int gY, const sf::Texture& tex)
        : defId(id), variantIdx(vIdx), worldPos(wPos),
          gridX(gX), gridY(gY), sprite(tex) {}
};
