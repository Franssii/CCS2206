#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "BuildingManager.hpp"
#include "RoadManager.hpp"
#include "IndustrialManager.hpp"
#include "PowerManager.hpp"
#include "WorldGen.hpp"

struct ExplosionAnim {
    sf::Sprite sprite;
    int currentFrame = 0;
    int frameTimer = 0;
    bool finished = false;
};

class DemolitionManager {
public:
    DemolitionManager(const sf::Texture& demolishTex, const sf::Texture& hlTex, const sf::Texture& explosionTex);

    bool isActive() const { return _active; }
    void setActive(bool active) { _active = active; }

    void update(sf::Vector2f worldMousePos, float stepX, float stepY);
    bool handleWorldClick(sf::Vector2f worldPos, float stepX, float stepY, BuildingManager* bm,
                          RoadManager* rm, IndustrialManager* im, PowerManager* pm,
                          GeneratedWorld* world = nullptr);

    void drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY);
    void drawAnimations(sf::RenderWindow& window);

private:
    bool _active = false;

    sf::Sprite _demolishSprite;
    sf::Sprite _hlSprite;
    const sf::Texture& _explosionTex;

    std::vector<ExplosionAnim> _explosions;
};