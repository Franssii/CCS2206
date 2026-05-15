#pragma once
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>


struct BuildingDef {
    std::string id;
    std::string displayName;
    std::string description;
    int         cost = 0;
    std::string materials;

    std::vector<sf::Texture> textures;
    sf::Texture iconTexture;

    int gridW = 1;
    int gridH = 1;
};

struct PlacedBuilding {
    std::string  defId;
    int          variantIdx;
    sf::Vector2f worldPos;
    int          gridX;
    int          gridY;
    sf::Sprite   sprite;

    PlacedBuilding(const std::string& id, int vIdx, sf::Vector2f wPos, int gX, int gY, const sf::Texture& tex)
        : defId(id), variantIdx(vIdx), worldPos(wPos), gridX(gX), gridY(gY), sprite(tex) {}
};
