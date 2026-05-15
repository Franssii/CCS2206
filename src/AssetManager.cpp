#include "include/AssetManager.hpp"
#include <windows.h>
#include <stdexcept>

static void fatalError(const std::string& message) {
    MessageBoxA(NULL, message.c_str(), "FATAL ERROR", MB_ICONERROR | MB_OK);
    exit(EXIT_FAILURE);
}

void AssetManager::loadTexture(const std::string& name, const std::string& fileName) {
    sf::Texture tex;
    if (tex.loadFromFile(fileName)) {
        this->_textures.insert_or_assign(name, std::move(tex));
    } else {
        fatalError("Failed to load texture: " + fileName);
    }
}

sf::Texture& AssetManager::getTexture(const std::string& name) {
    try {
        return this->_textures.at(name);
    } catch (const std::out_of_range& e) {
        fatalError("Texture not found: " + name);
        throw;
    }
}

void AssetManager::loadFont(const std::string& name, const std::string& fileName) {
    sf::Font font;
    if (font.openFromFile(fileName)) {
        this->_fonts.insert_or_assign(name, std::move(font));
    } else {
        fatalError("Failed to load font: " + fileName);
    }
}

sf::Font& AssetManager::getFont(const std::string& name) {
    try {
        return this->_fonts.at(name);
    } catch (const std::out_of_range& e) {
        fatalError("Font not found: " + name);
        throw;
    }
}

void AssetManager::loadMusic(const std::string& name, const std::string& fileName) {
    auto music = std::make_unique<sf::Music>();
    if (music->openFromFile(fileName)) {
        this->_music[name] = std::move(music);
    } else {
        fatalError("Failed to load music: " + fileName);
    }
}

sf::Music& AssetManager::getMusic(const std::string& name) {
    try {
        return *this->_music.at(name);
    } catch (const std::out_of_range& e) {
        fatalError("Music not found: " + name);
        throw;
    }
}