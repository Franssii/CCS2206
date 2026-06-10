#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include "GameManager.hpp"
#include "AssetManager.hpp"
#include "MapRenderer.hpp"
#include "UIManager.hpp"
#include "BuildingManager.hpp"
#include "RoadManager.hpp"
#include "DemolitionManager.hpp"
#include "WorldGen.hpp"
#include "WorldGenMenu.hpp"
#include "IndustrialManager.hpp"
#include "PowerManager.hpp"
#include "SettingsManager.hpp"
#include "StatsManager.hpp"
#include "IndustryWorkers.hpp"
#include "CitizenManager.hpp"
#include "VehicleManager.hpp"

constexpr float UI_STATBOX_Y        = 12.f;
constexpr float UI_STATBOX_X_START  = 328.f;
constexpr float UI_STATBOX_WIDTH    = 120.f;
constexpr float UI_STATBOX_HEIGHT   = 28.f;
constexpr float UI_STATBOX_SPACING  = 130.f;

constexpr float UI_DATE_X           = 1285.f;
constexpr float UI_DATE_Y           = 15.f;

constexpr float UI_ICON_START_X     = 16.f;
constexpr float UI_ICON_START_Y     = 170.f;
constexpr float UI_ICON_SPACING     = 60.f;

constexpr float UI_MOUSE_POS_X      = 5.f;
constexpr float UI_MOUSE_POS_Y      = 5.f;

constexpr float BARE_SPACING        = 0.f;

struct StatBox {
    sf::RectangleShape box;
    std::unique_ptr<sf::Text> text;
    sf::Color defaultColor;
};

struct StatThreshold {
    std::string direction = "high"; // "high" or "low"
    long long   green     = 0;
    long long   orange    = 0;
};

class Game {
public:
    Game();
    void run();

private:
    void processEvents();
    void update();
    void render();

    void initWindow();
    void initAssets();
    void initUI();
    void initBuildings();
    void initRoads();
    void initIndustrial();
    void initPower();
    void initCitizens();

    void startNewGame(const WorldSettings& settings);
    void saveGame();
    void loadGame();

    std::string formatNumber(long long num);

    sf::RenderWindow window;
    sf::View gameView;

    AssetManager assets;
    std::unique_ptr<MapRenderer>       mapRenderer;
    std::unique_ptr<BuildingManager>   buildingManager;
    std::unique_ptr<RoadManager>       roadManager;
    std::unique_ptr<DemolitionManager> demolitionManager;
    std::unique_ptr<IndustrialManager> industrialManager;
    std::unique_ptr<PowerManager>      powerManager;
    std::unique_ptr<WorldGenMenu>      worldGenMenu;
    std::unique_ptr<SettingsManager>   settingsManager;
    std::unique_ptr<StatsManager>      statsManager;
    IndustryWorkers                    industryWorkers;
    std::unique_ptr<CitizenManager>    citizenManager;
    std::unique_ptr<VehicleManager>    vehicleManager;
    GameManager gameManager;
    UIManager   uiManager;

    std::map<std::string, StatThreshold> statThresholds;
    void loadVariables(const std::string& path);
    sf::Color colorForStat(const std::string& key, long long value) const;
    void tickEconomy();

    void applyAudioSettings();
    void applyDisplayMode();
    void playClick();

    WorldGenerator  worldGenerator;
    GeneratedWorld  currentWorld;
    bool            _worldGenerated = false;

    float currentZoom = 1.0f;
    const float minZoom = 0.2f;
    const float maxZoom = 12.0f;
    std::string game_version;

    float bareStepX = 32.f;
    float bareStepY = 16.f;

    sf::Sprite bare;
    sf::Sprite ui;
    sf::Sprite lobby;
    sf::Sprite option_marked;
    sf::Sprite manu_option;
    sf::Sprite nsf;
    sf::Sprite quitting_warrning;
    sf::Sprite starting_new_game_warning;
    sf::Sprite yesno_marker;
    sf::Sprite true_marker;
    sf::Sprite false_marker;
    sf::Sprite pauseBtnSprite;
    sf::Sprite speedBtnSprite;
    sf::Sprite thermometerSprite;

    std::vector<sf::Sprite> block_1s;
    sf::Sprite newBlock_1;

    std::vector<sf::Sprite> uiIcons;
    std::vector<sf::Sprite> borderCrossingSprites;

    std::vector<StatBox> statBoxes;

    sf::Text date;
    sf::Text music_level_text;
    sf::Text sound_level_text;
    sf::Text game_version_text;
    sf::Text mousePosText;
    sf::Text temperatureText;

    std::vector<sf::Vector2f> optionPositions;

    bool _lmbWasDown    = false;
    bool _showWorldMenu = false;
};
