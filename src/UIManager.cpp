#include "include/UIManager.hpp"

void UIManager::setButtonBounds(const std::string& menuName, const std::vector<sf::FloatRect>& bounds) {
    menuButtons[menuName] = bounds;
}

int UIManager::getHoveredButton(const std::string& menuName, const sf::Vector2f& mousePos) const {
    auto it = menuButtons.find(menuName);
    if (it != menuButtons.end()) {
        const auto& bounds = it->second;
        for (size_t i = 0; i < bounds.size(); ++i) {
            if (bounds[i].contains(mousePos)) {
                return static_cast<int>(i) + 1; 
            }
        }
    }
    return 0;
}