#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include "WorldGen.hpp"

class MapRenderer {
public:
    MapRenderer(const sf::Texture& bareTexture,
                int mapWidthChunks, int mapHeightChunks,
                float bareSpacing);

    void setWorld(const GeneratedWorld* world) { _world = world; }

    void setTexture(TileType type, const sf::Texture* tex) { _texMap[type] = tex; }
    void setSnowTextures(const sf::Texture* s1, const sf::Texture* s2, const sf::Texture* s3) {
        _snow1 = s1;
        _snow2 = s2;
        _snow3 = s3;
    }

    void draw(sf::RenderWindow& window, const sf::View& view, int temperature = 10);

    float getStepX() const { return _stepX; }
    float getStepY() const { return _stepY; }

private:
    sf::Sprite   _bareSprite;
    const sf::Texture* _snow1 = nullptr;
    const sf::Texture* _snow2 = nullptr;
    const sf::Texture* _snow3 = nullptr;
    int          _mapWidthChunks;
    int          _mapHeightChunks;
    float        _bareSpacing;
    float        _stepX = 0.f;
    float        _stepY = 0.f;

    const GeneratedWorld*                _world = nullptr;
    std::map<TileType, const sf::Texture*> _texMap;

    sf::Vector2f screenToGrid(float sx, float sy, float stepX, float stepY) const;
};
