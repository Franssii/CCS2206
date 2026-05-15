#include "include/GameManager.hpp"
#include <fstream>

GameManager::GameManager() {}

bool GameManager::fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

void GameManager::save(const std::vector<sf::Sprite>& block_1s) {
    std::ofstream saveFile1("saves/game.save");
    if (saveFile1.is_open()) {
        saveFile1 << population << std::endl;
        saveFile1 << cash << std::endl;
        saveFile1 << loyalty << std::endl;
        saveFile1 << workers << std::endl;
        saveFile1 << unemployment << std::endl;
        saveFile1 << sick << std::endl;
        saveFile1 << power << std::endl;
        saveFile1 << clock.getMin() << std::endl;
        saveFile1 << clock.getHour() << std::endl;
        saveFile1 << clock.getDay() << std::endl;
        saveFile1 << clock.getMonth() << std::endl;
        saveFile1 << clock.getYear() << std::endl;
        saveFile1.close();
    }

    std::ofstream saveFile2("saves/block_1s.save");
    if (saveFile2.is_open()) {
        saveFile2 << block_1s.size() << std::endl;
        for (const auto& b : block_1s)
            saveFile2 << b.getPosition().x << " " << b.getPosition().y << std::endl;
        saveFile2.close();
    }
}

void GameManager::restart(std::vector<sf::Sprite>& block_1s) {
    population = 0;
    cash = 1000000;
    loyalty = 75;
    workers = 0;
    unemployment = 0;
    sick = 0;
    power = 0;
    option_marked_position = 0;
    option_marked_position_2 = 0;
    xt1 = 0;

    clock.setTime(1, 10, 26, 4, 1967);
    block_1s.clear();
}

void GameManager::load(std::vector<sf::Sprite>& block_1s, const sf::Texture& block_1_texture) {
    std::ifstream loadFile1("saves/game.save", std::ios::in);
    std::ifstream loadFile2("saves/block_1s.save", std::ios::in);

    if (loadFile1.is_open() && loadFile2.is_open()) {
        block_1s.clear();

        int m, h, d, mo, y;
        loadFile1 >> population >> cash >> loyalty >> workers >> unemployment >> sick >> power
                  >> m >> h >> d >> mo >> y;
        clock.setTime(m, h, d, mo, y);

        size_t count;
        loadFile2 >> count;
        for (size_t i = 0; i < count; ++i) {
            float x, y;
            loadFile2 >> x >> y;
            sf::Sprite sprite(block_1_texture);
            sf::FloatRect bounds = sprite.getGlobalBounds();
            sprite.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
            sprite.setPosition({x, y});
            block_1s.push_back(sprite);
        }

        game_start = false;
        failed_toload = false;
    } else {
        failed_toload = true;
        game_start = true;
    }

    if (loadFile1.is_open()) loadFile1.close();
    if (loadFile2.is_open()) loadFile2.close();
}
