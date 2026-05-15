#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <map>
#include <memory>

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager() = default;

    void loadTexture(const std::string& name, const std::string& fileName);
    sf::Texture& getTexture(const std::string& name);

    void loadFont(const std::string& name, const std::string& fileName);
    sf::Font& getFont(const std::string& name);

    void loadMusic(const std::string& name, const std::string& fileName);
    sf::Music& getMusic(const std::string& name);

private:
    std::map<std::string, sf::Texture> _textures;
    std::map<std::string, sf::Font> _fonts;
    std::map<std::string, std::unique_ptr<sf::Music>> _music;
};