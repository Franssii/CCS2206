#include "include/DemolitionManager.hpp"
#include <algorithm>

constexpr int FRAME_W = 47;
constexpr int FRAME_H = 47;
constexpr int GAP_X = 3;
constexpr int FRAMES_COUNT = 15;
constexpr int TICKS_PER_FRAME = 3;

DemolitionManager::DemolitionManager(const sf::Texture& demolishTex, const sf::Texture& hlTex, const sf::Texture& explosionTex)
    : _demolishSprite(demolishTex), _hlSprite(hlTex), _explosionTex(explosionTex)
{
    _demolishSprite.setOrigin({demolishTex.getSize().x / 2.f, demolishTex.getSize().y / 2.f});
}

void DemolitionManager::update(sf::Vector2f worldMousePos, float stepX, float stepY) {
    for (auto& exp : _explosions) {
        if (exp.finished) continue;
        exp.frameTimer++;
        if (exp.frameTimer >= TICKS_PER_FRAME) {
            exp.frameTimer = 0;
            exp.currentFrame++;
            if (exp.currentFrame >= FRAMES_COUNT) {
                exp.finished = true;
            } else {
                int tx = exp.currentFrame * (FRAME_W + GAP_X);
                exp.sprite.setTextureRect(sf::IntRect({tx, 0}, {FRAME_W, FRAME_H}));
            }
        }
    }
    
    _explosions.erase(std::remove_if(_explosions.begin(), _explosions.end(), [](const ExplosionAnim& a) { return a.finished; }), _explosions.end());

    if (!_active) return;

    sf::Vector2i g = BuildingManager::worldToGrid(worldMousePos, stepX, stepY);
    sf::Vector2f tilePos = BuildingManager::gridToWorld(g.x, g.y, stepX, stepY);
    
    float hlTexW = static_cast<float>(_hlSprite.getTexture().getSize().x);
    float hlTexH = static_cast<float>(_hlSprite.getTexture().getSize().y);
    _hlSprite.setOrigin({hlTexW / 2.f, 0.f});
    _hlSprite.setPosition(tilePos);
    _hlSprite.setScale({(stepX * 2.f) / hlTexW, (stepY * 2.f) / hlTexH});
    _hlSprite.setColor(sf::Color(255, 0, 0, 200));

    _demolishSprite.setPosition(worldMousePos);
}

bool DemolitionManager::handleWorldClick(sf::Vector2f worldPos, float stepX, float stepY, BuildingManager* bm, RoadManager* rm, IndustrialManager* im, GeneratedWorld* world) {
    if (!_active) return false;

    sf::Vector2i g = BuildingManager::worldToGrid(worldPos, stepX, stepY);
    sf::Vector2f effectPos = BuildingManager::gridToWorld(g.x, g.y, stepX, stepY);

    bool removed = false;
    if (bm && bm->removeAt(g.x, g.y)) removed = true;
    else if (rm && rm->removeAt(g.x, g.y)) removed = true;
    else if (im && im->removeAt(g.x, g.y)) removed = true;
    else if (world && world->get(g.x, g.y) == TileType::TREE) {
        world->tiles[g.y][g.x] = TileType::BARE;
        removed = true;
    }

    if (removed) {
        ExplosionAnim anim{sf::Sprite(_explosionTex)};
        anim.sprite.setTextureRect(sf::IntRect({0, 0}, {FRAME_W, FRAME_H}));
        anim.sprite.setOrigin({FRAME_W / 2.f, static_cast<float>(FRAME_H)});
        anim.sprite.setPosition(effectPos);
        anim.sprite.setScale({2.f, 2.f});
        _explosions.push_back(anim);
        return true;
    }
    return false;
}

void DemolitionManager::drawWorldOverlay(sf::RenderWindow& window, float stepX, float stepY) {
    if (_active) {
        window.draw(_hlSprite);
        window.draw(_demolishSprite);
    }
}

void DemolitionManager::drawAnimations(sf::RenderWindow& window) {
    for (const auto& exp : _explosions) {
        if (!exp.finished) window.draw(exp.sprite);
    }
}