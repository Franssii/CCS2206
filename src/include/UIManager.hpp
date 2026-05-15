#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <string>

class UIManager {
public:
    UIManager() = default;

    void setButtonBounds(const std::string& menuName, const std::vector<sf::FloatRect>& bounds);
    int getHoveredButton(const std::string& menuName, const sf::Vector2f& mousePos) const;

private:
    std::map<std::string, std::vector<sf::FloatRect>> menuButtons;
};