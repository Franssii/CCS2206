#include "include/Game.hpp"
#include <fstream>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
//main
namespace fs = std::filesystem;

static void fatalError(const std::string& msg) {
    MessageBoxA(NULL, msg.c_str(), "FATAL ERROR", MB_ICONERROR | MB_OK);
    exit(EXIT_FAILURE);
}
static void warnError(const std::string& msg) {
    MessageBoxA(NULL, msg.c_str(), "WARNING", MB_ICONWARNING | MB_OK);
}
static void updatePowerBalance(GameManager& gm, const BuildingManager* bm,
                               const IndustrialManager* im, const PowerManager* pm,
                               StatsManager* stats = nullptr) {
    int powerProd = pm ? pm->getTotalProduction() : 0;
    int powerCons = 0;
    if (bm) {
        for (const auto& h : bm->getPlaced())
            powerCons += bm->getPowerDrawFor(h);
    }
    if (im) {
        for (const auto& ind : im->getPlaced()) {
            if (ind.defId != "Magazine_1" && ind.defId != "Magazine_open")
                powerCons += 5;
        }
    }
    gm.power = powerProd - powerCons;
    gm.powerShortage = powerCons > 0 && powerProd < powerCons;
    if (stats)
        stats->setHourAccum(StatCategory::POWER,
            static_cast<float>(powerProd), static_cast<float>(powerCons));
}

static std::string mousePosStr(sf::Vector2f p) {
    std::ostringstream ss;
    ss << "X:" << std::fixed << std::setprecision(0) << p.x << " Y:" << p.y;
    return ss.str();
}

static const sf::Texture& preloadAssets(AssetManager& assets, std::string& version) {
    assets.loadFont("main_font", "mingliu.ttf");
    std::ifstream sf("saves/settings.ini");
    if (sf) {
        std::string line;
        while (std::getline(sf, line)) {
            if (line.rfind("version=", 0) == 0) {
                version = line.substr(8);
                break;
            }
        }
    }
    if (version.empty()) {
        std::ifstream legacy("settings.ini");
        if (legacy) legacy >> version;
    }

    assets.loadTexture("bare",                      "gfx/textures/terrain/bare.png");
    assets.loadTexture("bare_water",                "gfx/textures/terrain/bare_water.png");
    assets.loadTexture("water_1",                   "gfx/textures/terrain/water_1.png");
    assets.loadTexture("coal",                      "gfx/textures/terrain/coal.png");
    assets.loadTexture("iron",                      "gfx/textures/terrain/iron.png");
    assets.loadTexture("tree",                      "gfx/textures/terrain/tree.png");
    assets.loadTexture("snow_1",                    "gfx/textures/terrain/snow_1.png");
    assets.loadTexture("snow_2",                    "gfx/textures/terrain/snow_2.png");
    assets.loadTexture("snow_3",                    "gfx/textures/terrain/snow_3.png");
    assets.loadTexture("ui",                        "gfx/textures/ui.png");
    assets.loadTexture("lobby",                     "gfx/textures/ui/lobby.png");
    assets.loadTexture("option_marked",             "gfx/textures/option_marked.png");
    assets.loadTexture("menu_option",               "gfx/textures/manu_option.png");
    assets.loadTexture("nsf",                       "gfx/textures/nsf.png");
    assets.loadTexture("quitting_warning",          "gfx/textures/quitting_warning.png");
    assets.loadTexture("starting_new_game_warning", "gfx/textures/starting_new_game_warning.png");
    assets.loadTexture("yesno_marker",              "gfx/textures/yesno_marker.png");
    assets.loadTexture("true_marker",               "gfx/textures/true.png");
    assets.loadTexture("false_marker",              "gfx/textures/false.png");
    assets.loadTexture("close_x",                   "gfx/textures/X.png");
    assets.loadTexture("hl",                        "gfx/textures/hl.png");
    assets.loadTexture("demolish",                  "gfx/textures/demolish.png");
    assets.loadTexture("explosion",                 "gfx/textures/animations/explosion/explosion.png");
    assets.loadTexture("world_gen_menu",            "gfx/textures/world_generation_menu.png");
    assets.loadTexture("block_1",                   "gfx/textures/buildings/houses/block_1.png");
    assets.loadTexture("border_crossing",           "gfx/textures/roads/border_crossing.png");
    assets.loadTexture("border_crossingb",          "gfx/textures/roads/border_crossingb.png");
    for (int i = 1; i <= 6; ++i)
        assets.loadTexture("ico" + std::to_string(i), "gfx/textures/ico/" + std::to_string(i) + ".png");

    assets.loadTexture("paused",                    "gfx/textures/paused.png");
    assets.loadTexture("resumed",                   "gfx/textures/resumed.png");
    assets.loadTexture("speed_1",                   "gfx/textures/speed_1.png");
    assets.loadTexture("speed_2",                   "gfx/textures/speed_2.png");
    assets.loadTexture("speed_3",                   "gfx/textures/speed_3.png");
    assets.loadTexture("thermometer",               "gfx/textures/thermometer.png");

    assets.loadMusic("lobby_ost",       "audio/lobby.ogg");
    assets.loadMusic("demolition_ost",  "audio/demolition.ogg");
    assets.loadMusic("fire_ignite_ost", "audio/Fire_ignite.ogg");
    assets.loadMusic("notifi_ost",      "audio/notifi.ogg");
    assets.loadMusic("click_ost",       "audio/click.ogg");
    
    return assets.getTexture("bare");
}

Game::Game() :
    window(sf::VideoMode::getDesktopMode(), "CCS 2026", sf::State::Fullscreen),
    gameManager(),
    bare(preloadAssets(assets, game_version)),
    ui(assets.getTexture("ui")),
    lobby(assets.getTexture("lobby")),
    option_marked(assets.getTexture("option_marked")),
    manu_option(assets.getTexture("menu_option")),
    nsf(assets.getTexture("nsf")),
    quitting_warrning(assets.getTexture("quitting_warning")),
    starting_new_game_warning(assets.getTexture("starting_new_game_warning")),
    yesno_marker(assets.getTexture("yesno_marker")),
    true_marker(assets.getTexture("true_marker")),
    false_marker(assets.getTexture("false_marker")),
    pauseBtnSprite(assets.getTexture("paused")),
    speedBtnSprite(assets.getTexture("speed_1")),
    thermometerSprite(assets.getTexture("thermometer")),
    newBlock_1(assets.getTexture("block_1")),
    date(assets.getFont("main_font"), "", 16),
    music_level_text(assets.getFont("main_font"), "", 30),
    sound_level_text(assets.getFont("main_font"), "", 30),
    game_version_text(assets.getFont("main_font"), "", 20),
    mousePosText(assets.getFont("main_font"), "", 14),
    temperatureText(assets.getFont("main_font"), "", 16)
{
    initWindow();
    initAssets();
    initUI();
    initBuildings();
    initRoads();
    initIndustrial();
    initPower();
    initCitizens();
}

void Game::initWindow() {
    window.setFramerateLimit(60);
    gameView = window.getDefaultView();
}

void Game::initAssets() {
    option_marked.setPosition({790.f, 443.f});
    true_marker.setPosition({1428.f, 7.f});
    false_marker.setPosition({1428.f, 7.f});

    date.setFillColor(sf::Color::White);
    date.setStyle(sf::Text::Style::Bold);
    date.setPosition({UI_DATE_X, UI_DATE_Y});

    music_level_text.setFillColor(sf::Color::White);
    music_level_text.setStyle(sf::Text::Style::Bold);
    music_level_text.setPosition({915.f, 615.f});

    sound_level_text.setFillColor(sf::Color::White);
    sound_level_text.setStyle(sf::Text::Style::Bold);
    sound_level_text.setPosition({1030.f, 648.f});

    game_version_text.setFillColor(sf::Color::White);
    game_version_text.setStyle(sf::Text::Style::Bold);
    game_version_text.setPosition({967.f, 265.f});

    mousePosText.setFillColor(sf::Color(200, 200, 200));
    mousePosText.setStyle(sf::Text::Style::Bold);

    assets.getMusic("lobby_ost").setLooping(true);
    assets.getMusic("lobby_ost").play();
    assets.getMusic("lobby_ost").setVolume(50.f);
    assets.getMusic("demolition_ost").setVolume(100.f);
    assets.getMusic("fire_ignite_ost").setVolume(50.f);
    assets.getMusic("notifi_ost").setVolume(100.f);

    sf::FloatRect bareBounds = bare.getGlobalBounds();
    bareStepX = (bareBounds.size.x + BARE_SPACING) / 2.f;
    bareStepY = (bareBounds.size.y + BARE_SPACING) / 2.f;

    mapRenderer = std::make_unique<MapRenderer>(
        assets.getTexture("bare"), 20, 20, BARE_SPACING);

    mapRenderer->setTexture(TileType::BARE,         &assets.getTexture("bare"));
    mapRenderer->setTexture(TileType::BARE_WATER,   &assets.getTexture("bare_water"));
    mapRenderer->setTexture(TileType::WATER,        &assets.getTexture("water_1"));
    mapRenderer->setTexture(TileType::COAL,         &assets.getTexture("coal"));
    mapRenderer->setTexture(TileType::IRON,         &assets.getTexture("iron"));
    mapRenderer->setTexture(TileType::TREE,         &assets.getTexture("tree"));

    mapRenderer->setSnowTextures(
        &assets.getTexture("snow_1"),
        &assets.getTexture("snow_2"),
        &assets.getTexture("snow_3")
    );

    
    pauseBtnSprite.setPosition({1430.f, UI_DATE_Y});
    float pauseWidth = pauseBtnSprite.getGlobalBounds().size.x;
    speedBtnSprite.setPosition({1430.f + pauseWidth + 10.f, UI_DATE_Y});

    float speedWidth = speedBtnSprite.getGlobalBounds().size.x;
    thermometerSprite.setPosition({speedBtnSprite.getPosition().x + speedWidth + 25.f, UI_DATE_Y - 7.f});
    float thermoWidth = thermometerSprite.getGlobalBounds().size.x;
    temperatureText.setFillColor(sf::Color::White);
    temperatureText.setStyle(sf::Text::Style::Bold);
    temperatureText.setPosition({thermometerSprite.getPosition().x + thermoWidth + 5.f, UI_DATE_Y});

    worldGenMenu = std::make_unique<WorldGenMenu>(
        assets.getFont("main_font"),
        assets.getTexture("world_gen_menu"),
        window.getSize()
    );

    settingsManager = std::make_unique<SettingsManager>(assets.getFont("main_font"));
    settingsManager->loadLabels("gfx/textures/settings_labels.txt");
    fs::create_directories("saves");
    settingsManager->loadSettings("saves/settings.ini", "settings.ini");
    if (!settingsManager->getVersion().empty()) game_version = settingsManager->getVersion();
    settingsManager->saveSettings("saves/settings.ini");
    applyAudioSettings();
    applyDisplayMode();

    statsManager = std::make_unique<StatsManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"));
    statsManager->loadDescriptions("language");

    loadVariables("variables.ini");
}

void Game::playClick() {
    auto& click = assets.getMusic("click_ost");
    click.stop();
    click.play();
}

void Game::applyAudioSettings() {
    if (!settingsManager) return;
    float mv = static_cast<float>(settingsManager->getMusicVolume());
    float sv = static_cast<float>(settingsManager->getSoundVolume());
    assets.getMusic("lobby_ost")      .setVolume(mv);
    assets.getMusic("demolition_ost") .setVolume(sv);
    assets.getMusic("fire_ignite_ost").setVolume(sv);
    assets.getMusic("notifi_ost")     .setVolume(sv);
    assets.getMusic("click_ost")      .setVolume(sv);
}

void Game::applyDisplayMode() {
    if (!settingsManager) return;
    bool wantFs = settingsManager->isFullscreen();
    bool isFs   = (window.getSize() == sf::VideoMode::getDesktopMode().size);
    if (wantFs == isFs) return;

    sf::Vector2f viewCenter = gameView.getCenter();
    sf::Vector2f viewSize   = gameView.getSize();

    if (wantFs) {
        window.create(sf::VideoMode::getDesktopMode(), "CCS 2026", sf::State::Fullscreen);
    } else {
        auto dm = sf::VideoMode::getDesktopMode();
        sf::Vector2u winSize(
            static_cast<unsigned int>(dm.size.x * 0.8f),
            static_cast<unsigned int>(dm.size.y * 0.8f));
        window.create(sf::VideoMode(winSize), "CCS 2026", sf::Style::Default);
    }
    window.setFramerateLimit(60);
    gameView.setCenter(viewCenter);
    gameView.setSize(viewSize);
}

void Game::initUI() {
    optionPositions = {
        {790.f, 443.f}, {790.f, 525.f}, {790.f, 607.f}, {790.f, 689.f}
    };
    std::vector<sf::FloatRect> lobbyBounds;
    for (const auto& pos : optionPositions)
        lobbyBounds.push_back(sf::FloatRect({pos.x, pos.y}, {400.f, 40.f}));
    uiManager.setButtonBounds("Lobby", lobbyBounds);
    uiManager.setButtonBounds("InGame", {sf::FloatRect({1590.f, 5.f}, {50.f, 50.f})});

    sf::Color colors[] = {
        sf::Color::Red, sf::Color(255,165,0), sf::Color::Yellow,
        sf::Color::Green, sf::Color(144,238,144), sf::Color::Blue, sf::Color(255,192,203)
    };
    for (int i = 0; i < 7; ++i) {
        StatBox sb;
        sb.box.setSize({UI_STATBOX_WIDTH, UI_STATBOX_HEIGHT});
        sb.box.setFillColor(sf::Color::Black);
        sb.box.setOutlineThickness(2.f);
        sb.box.setOutlineColor(colors[i]);
        sb.defaultColor = colors[i];
        sb.box.setPosition({UI_STATBOX_X_START + (i * UI_STATBOX_SPACING), UI_STATBOX_Y});
        sb.text = std::make_unique<sf::Text>(assets.getFont("main_font"), "", 13);
        sb.text->setFillColor(sf::Color::White);
        sb.text->setStyle(sf::Text::Style::Bold);
        statBoxes.push_back(std::move(sb));
    }

    for (int i = 1; i <= 6; ++i) {
        sf::Sprite ico(assets.getTexture("ico" + std::to_string(i)));
        ico.setPosition({UI_ICON_START_X, UI_ICON_START_Y + (i - 1) * UI_ICON_SPACING});
        uiIcons.push_back(ico);
    }
}

void Game::initBuildings() {
    fs::create_directories("saves");
    buildingManager = std::make_unique<BuildingManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"), assets.getTexture("hl"));
    buildingManager->loadHouses(
        "gfx/textures/buildings/houses/houses_data.txt",
        "gfx/textures/buildings/houses/",
        [](const std::string& m) { warnError(m); });
}

void Game::initRoads() {
    roadManager = std::make_unique<RoadManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"), assets.getTexture("hl"));
    roadManager->loadRoads(
        "gfx/textures/roads/roads_data.txt",
        "gfx/textures/roads/",
        [](const std::string& m) { warnError(m); });
    demolitionManager = std::make_unique<DemolitionManager>(
        assets.getTexture("demolish"), assets.getTexture("hl"), assets.getTexture("explosion"));
}

void Game::initIndustrial() {
    industrialManager = std::make_unique<IndustrialManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"), assets.getTexture("hl"));
    industrialManager->loadBuildings(
        "gfx/textures/buildings/industrial/industrial_data.txt",
        "gfx/textures/buildings/industrial/",
        [](const std::string& m) { warnError(m); }, true);

    industrialManager->loadBuildings(
        "gfx/textures/buildings/industrial/factory_connection/factory_connection_data.txt",
        "gfx/textures/buildings/industrial/factory_connection/",
        [](const std::string& m) { warnError(m); }, false);
}

void Game::initPower() {
    powerManager = std::make_unique<PowerManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"), assets.getTexture("hl"));
    powerManager->loadPower(
        "gfx/textures/buildings/power/power_data.txt",
        "gfx/textures/buildings/power/",
        [](const std::string& m) { warnError(m); });
}

void Game::initCitizens() {
    industryWorkers.load("industry_workers.txt");
    citizenManager = std::make_unique<CitizenManager>();
    citizenManager->setIndustryWorkers(&industryWorkers);
    citizenManager->loadWalkerTextures("gfx/textures/");
    vehicleManager = std::make_unique<VehicleManager>(
        assets.getFont("main_font"), assets.getTexture("close_x"));
    vehicleManager->loadDefs("gfx/textures/vehicles/vehicles_data.txt");
    vehicleManager->loadBusSprites("gfx/textures/vehicles/");
}

void Game::startNewGame(const WorldSettings& settings) {
    currentWorld     = worldGenerator.generate(settings);
    _worldGenerated  = true;

    mapRenderer->setWorld(&currentWorld);

    int gw = currentWorld.width;
    int gh = currentWorld.height;
    float cx = static_cast<float>(gw / 2 - gh / 2) * bareStepX;
    float cy = static_cast<float>(gw / 2 + gh / 2) * bareStepY;
    gameView.setCenter({cx, cy});
    currentZoom = 1.0f;
    gameView.setSize(window.getDefaultView().getSize());

    buildingManager->clear();
    roadManager->clear();
    if (industrialManager) industrialManager->clear();
    if (powerManager) powerManager->clear();
    if (vehicleManager) vehicleManager->clear();
    if (citizenManager) citizenManager->clearStops();
    block_1s.clear();
    borderCrossingSprites.clear();

    for (auto& [bx, by] : currentWorld.borderCrossings) {
        bool upLeft    = currentWorld.get(bx - 1, by) == TileType::EMPTY;
        bool downRight = currentWorld.get(bx + 1, by) == TileType::EMPTY;
        int var = (upLeft || downRight) ? 0 : 1;

        sf::Sprite spr(var == 0 ? assets.getTexture("border_crossing")
                                : assets.getTexture("border_crossingb"));
        float tw = spr.getGlobalBounds().size.x;
        float th = spr.getGlobalBounds().size.y;
        spr.setOrigin({tw / 2.f, th});
        float isoX = static_cast<float>(bx - by) * bareStepX;
        float isoY = static_cast<float>(bx + by) * bareStepY + bareStepY * 2.f;
        spr.setPosition({isoX, isoY});
        borderCrossingSprites.push_back(spr);
    }

    gameManager.cash       = settings.startCash();
    gameManager.population = 0;
    gameManager.loyalty    = 75;
    gameManager.workers       = 0;
    gameManager.accommodation = 0;
    gameManager.sick          = 0;
    gameManager.power         = 0;
    gameManager.loanRemaining = 0;
    gameManager.loanMonthlyPayment = 0;
    gameManager.loanMonthsLeft = 0;
    gameManager.loanInterestRate = 0.f;
    gameManager.clock.setTime(1, 10, 26, 4, 1967);
    gameManager.game_start = false;

    if (statsManager) statsManager->clearHistory();
}

void Game::saveGame() {
    std::ofstream f("saves/game.save");
    if (!f.is_open()) return;
    f << gameManager.cash       << "\n"
      << gameManager.population << "\n"
      << gameManager.loyalty    << "\n"
      << gameManager.workers    << "\n"
      << gameManager.accommodation << "\n"
      << gameManager.sick       << "\n"
      << gameManager.power      << "\n"
      << gameManager.clock.getMin()   << "\n"
      << gameManager.clock.getHour()  << "\n"
      << gameManager.clock.getDay()   << "\n"
      << gameManager.clock.getMonth() << "\n"
      << gameManager.clock.getYear()  << "\n"
      << currentWorld.settings.seed   << "\n"
      << static_cast<int>(currentWorld.settings.mapSize)    << "\n"
      << static_cast<int>(currentWorld.settings.riverCount) << "\n"
      << static_cast<int>(currentWorld.settings.oreFreq)    << "\n"
      << static_cast<int>(currentWorld.settings.difficulty) << "\n"
      << static_cast<int>(currentWorld.settings.shapeRandomness) << "\n"
      << gameManager.loanRemaining     << "\n"
      << gameManager.loanMonthlyPayment << "\n"
      << gameManager.loanMonthsLeft    << "\n"
      << gameManager.loanInterestRate  << "\n";
    buildingManager->save("saves/buildings.save");
    roadManager->save("saves/roads.save");
    if (industrialManager) industrialManager->save("saves/industrial.save");
    if (powerManager) powerManager->save("saves/power.save");
    if (statsManager) statsManager->saveGraphs("saves/graphs.save");
    if (citizenManager && buildingManager) {
        citizenManager->save("saves/citizens.save", *buildingManager);
        citizenManager->saveStops("saves/bus_stops.save");
    }
    if (vehicleManager) vehicleManager->save("saves/vehicles.save");
}

void Game::loadGame() {
    std::ifstream f("saves/game.save");
    if (!f.is_open()) { gameManager.failed_toload = true; return; }

    int m = 1, h = 12, d = 1, mo = 1, y = 1967;
    unsigned int seed = 12345;
    int ms = 1, rc = 0, of = 1, diff = 1, sr = 1;
    
    if (!(f >> gameManager.cash >> gameManager.population >> gameManager.loyalty
          >> gameManager.workers >> gameManager.accommodation >> gameManager.sick
          >> gameManager.power >> m >> h >> d >> mo >> y)) {
        gameManager.failed_toload = true; return;
    }
    
    if (!(f >> seed >> ms >> rc >> of >> diff)) {
        seed = 12345;
    } else {
        if (!(f >> sr)) {
            sr = 1;
        }
    }

    long long lRem = 0, lMon = 0;
    int lLeft = 0;
    float lRate = 0.f;
    if (f >> lRem >> lMon >> lLeft >> lRate) {
        gameManager.loanRemaining     = lRem;
        gameManager.loanMonthlyPayment = lMon;
        gameManager.loanMonthsLeft    = lLeft;
        gameManager.loanInterestRate  = lRate;
    } else {
        gameManager.loanRemaining     = 0;
        gameManager.loanMonthlyPayment = 0;
        gameManager.loanMonthsLeft    = 0;
        gameManager.loanInterestRate  = 0.f;
    }
    if (statsManager)
        statsManager->loadGraphs("saves/graphs.save");

    gameManager.clock.setTime(m, h, d, mo, y);

    WorldSettings ws;
    ws.seed       = static_cast<unsigned int>(seed);
    ws.mapSize    = static_cast<MapSize>(ms);
    ws.riverCount = static_cast<RiverCount>(rc);
    ws.oreFreq    = static_cast<OreFreq>(of);
    ws.difficulty = static_cast<Difficulty>(diff);
    ws.shapeRandomness = static_cast<ShapeRandomness>(sr);

    currentWorld    = worldGenerator.generate(ws);
    _worldGenerated = true;
    mapRenderer->setWorld(&currentWorld);

    int gw = currentWorld.width;
    int gh = currentWorld.height;
    float cx = static_cast<float>(gw / 2 - gh / 2) * bareStepX;
    float cy = static_cast<float>(gw / 2 + gh / 2) * bareStepY;
    gameView.setCenter({cx, cy});
    currentZoom = 1.0f;
    gameView.setSize(window.getDefaultView().getSize());

    borderCrossingSprites.clear();
    for (auto& [bx, by] : currentWorld.borderCrossings) {
        bool upLeft    = currentWorld.get(bx - 1, by) == TileType::EMPTY;
        bool downRight = currentWorld.get(bx + 1, by) == TileType::EMPTY;
        int var = (upLeft || downRight) ? 1 : 0;

        sf::Sprite spr(var == 0 ? assets.getTexture("border_crossing")
                                : assets.getTexture("border_crossingb"));
        float tw = spr.getGlobalBounds().size.x;
        float th = spr.getGlobalBounds().size.y;
        spr.setOrigin({tw / 2.f, th});
        float isoX = static_cast<float>(bx - by) * bareStepX;
        float isoY = static_cast<float>(bx + by) * bareStepY + bareStepY * 2.f;
        spr.setPosition({isoX, isoY});
        borderCrossingSprites.push_back(spr);
    }

    if (fs::exists("saves/buildings.save"))
        buildingManager->load("saves/buildings.save", [](const std::string& m){ warnError(m); });
    if (fs::exists("saves/roads.save"))
        roadManager->load("saves/roads.save", [](const std::string& m){ warnError(m); });
    if (fs::exists("saves/industrial.save") && industrialManager)
        industrialManager->load("saves/industrial.save", [](const std::string& m){ warnError(m); });
    if (fs::exists("saves/power.save") && powerManager)
        powerManager->load("saves/power.save", [](const std::string& m){ warnError(m); });
    if (citizenManager && buildingManager && fs::exists("saves/citizens.save"))
        citizenManager->load("saves/citizens.save", *buildingManager);
    if (citizenManager && fs::exists("saves/bus_stops.save"))
        citizenManager->loadStops("saves/bus_stops.save");
    if (citizenManager && buildingManager && roadManager && industrialManager)
        citizenManager->dispatchAllWalkers(*buildingManager, *roadManager,
            *industrialManager, bareStepX, bareStepY);
    if (vehicleManager && roadManager && fs::exists("saves/vehicles.save"))
        vehicleManager->load("saves/vehicles.save", *roadManager);

    gameManager.game_start = false;
}

std::string Game::formatNumber(long long num) {
    char buf[64];
    if      (num >= 1000000000LL) snprintf(buf, sizeof(buf), "%.1fmld", num/1e9);
    else if (num >= 1000000LL)    snprintf(buf, sizeof(buf), "%.1fmln", num/1e6);
    else if (num >= 1000LL)       snprintf(buf, sizeof(buf), "%.1ftys", num/1e3);
    else                          snprintf(buf, sizeof(buf), "%lld", num);
    std::string res(buf);
    size_t p = res.find(".0m"); if (p!=std::string::npos) res.replace(p,2,"");
    p = res.find(".0t");        if (p!=std::string::npos) res.replace(p,2,"");
    return res;
}

void Game::run() {
    while (window.isOpen()) {
        processEvents();
        update();
        render();
    }
}

void Game::processEvents() {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) { window.close(); return; }

        if (const auto* mbAny = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mbAny->button == sf::Mouse::Button::Left) playClick();
        }

        if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
            sf::Keyboard::Key kSave  = settingsManager ? settingsManager->getKey("save")  : sf::Keyboard::Key::F1;
            sf::Keyboard::Key kPlace = settingsManager ? settingsManager->getKey("place") : sf::Keyboard::Key::Enter;
            sf::Keyboard::Key kCycle = settingsManager ? settingsManager->getKey("cycle") : sf::Keyboard::Key::R;

            if (kp->code == kSave && !gameManager.game_start) {
                saveGame();
                if (gameManager.quitting_warn) {
                    gameManager.restart(block_1s);
                    gameManager.game_start = true;
                    gameManager.quitting_warn = false;
                    _worldGenerated = false;
                    mapRenderer->setWorld(nullptr);
                    buildingManager->close();
                    roadManager->close();
                    buildingManager->clear();
                    roadManager->clear();
                    if (industrialManager) { industrialManager->close(); industrialManager->clear(); }
                    if (powerManager) { powerManager->close(); powerManager->clear(); }
                    if (statsManager) { statsManager->close(); statsManager->clearHistory(); }
                }
            }

            if (kp->code == kPlace && !gameManager.game_start && !gameManager.quitting_warn && !gameManager.ingame_settings) {
                if (buildingManager && buildingManager->isOpen() && !buildingManager->isPlacing())
                    buildingManager->simulatePlaceButtonClick();
                else if (roadManager && roadManager->isOpen() && !roadManager->isPlacing())
                    roadManager->simulatePlaceButtonClick();
                else if (industrialManager && industrialManager->isOpen() && !industrialManager->isPlacing())
                    industrialManager->simulatePlaceButtonClick();
            }

            if (kp->code == kCycle && !gameManager.game_start && !gameManager.quitting_warn && !gameManager.ingame_settings) {
                if (buildingManager && buildingManager->isOpen() && buildingManager->isPlacing())
                    buildingManager->cycleVariation();
                else if (roadManager && roadManager->isOpen() && roadManager->isPlacing())
                    roadManager->cycleVariation();
                else if (industrialManager && industrialManager->isOpen() && industrialManager->isPlacing())
                    industrialManager->cycleVariation();
            }
        }

        if (_showWorldMenu && worldGenMenu) {
            worldGenMenu->handleEvent(*event, window);
            continue;
        }

        if (gameManager.ingame_settings && settingsManager) {
            SettingsResult res = settingsManager->handleEvent(*event, window);
            if (res == SettingsResult::APPLY_AUDIO) {
                applyAudioSettings();
                settingsManager->saveSettings("saves/settings.ini");
            } else if (res == SettingsResult::APPLY_FULLSCREEN ||
                       res == SettingsResult::APPLY_WINDOWED) {
                applyDisplayMode();
                settingsManager->saveSettings("saves/settings.ini");
            } else if (res == SettingsResult::CLOSE_REQUESTED) {
                settingsManager->saveSettings("saves/settings.ini");
                gameManager.ingame_settings = false;
                _lmbWasDown = true; // blokuj lobby/in-game button na ten sam klik
            }
            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Escape &&
                    !settingsManager->isCapturingKey()) {
                    settingsManager->saveSettings("saves/settings.ini");
                    gameManager.ingame_settings = false;
                }
            }
            continue;
        }

        if (statsManager && statsManager->isOpen() && !gameManager.game_start) {
            if (statsManager->handleEvent(*event, window))
                continue;
        }

        if (buildingManager && buildingManager->isDetailOpen() && !gameManager.game_start) {
            if (buildingManager->handleDetailEvent(*event, window, roadManager.get(),
                    industrialManager.get(), citizenManager.get(), vehicleManager.get()))
                continue;
        }
        if (industrialManager && industrialManager->isDetailOpen() && !gameManager.game_start) {
            if (industrialManager->handleDetailEvent(*event, window))
                continue;
        }
        if (vehicleManager && vehicleManager->isDepotOpen() && !gameManager.game_start) {
            if (vehicleManager->handleDepotEvent(*event, window, gameManager.cash, *roadManager))
                continue;
        }
        if (roadManager && roadManager->isStopDetailOpen() && !gameManager.game_start) {
            if (roadManager->handleStopDetailEvent(*event, window, citizenManager.get(),
                    industrialManager.get(), &industryWorkers, buildingManager.get()))
                continue;
        }
        if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (buildingManager && buildingManager->isOpen())
                buildingManager->handleEvent(*event, window);
            if (roadManager && roadManager->isOpen())
                roadManager->handleEvent(*event, window);
            if (industrialManager && industrialManager->isOpen())
                industrialManager->handleEvent(*event, window);
            if (powerManager && powerManager->isOpen())
                powerManager->handleEvent(*event, window);
            if (scroll->wheel == sf::Mouse::Wheel::Vertical && !gameManager.game_start) {
                float zf = (scroll->delta > 0) ? 0.9f : 1.1f;
                float nz = currentZoom * zf;
                if (nz >= minZoom && nz <= maxZoom) { currentZoom = nz; gameView.zoom(zf); }
            }
            continue;
        }

        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), gameView);
            sf::Vector2f uiPos    = window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getDefaultView());

            if (mb->button == sf::Mouse::Button::Left) {
                if (!gameManager.game_start && !gameManager.ingame_settings && !gameManager.quitting_warn) {
                    if (pauseBtnSprite.getGlobalBounds().contains(uiPos)) {
                        gameManager.clock.isPaused = !gameManager.clock.isPaused;
                        continue;
                    }
                    if (speedBtnSprite.getGlobalBounds().contains(uiPos)) {
                        gameManager.clock.cycleSpeed();
                        continue;
                    }
                }

                if (buildingManager && buildingManager->isOpen()) {
                    if (buildingManager->isPlacing()) {
                        int cost = buildingManager->confirmPlace(worldPos, bareStepX, bareStepY,
                                                                  gameManager.cash, roadManager.get(), &currentWorld);
                        if (cost > 0) {
                            gameManager.cash -= cost;
                            if (statsManager) statsManager->recordExpense(StatCategory::CASH, static_cast<float>(cost));
                        }
                        continue;
                    }
                    buildingManager->handleEvent(*event, window);
                    continue;
                }
                if (roadManager && roadManager->isOpen()) {
                    if (roadManager->isPlacing()) {
                        int cost = roadManager->handleWorldClick(worldPos, bareStepX, bareStepY,
                                                                  gameManager.cash, buildingManager.get(), &currentWorld);
                        if (cost > 0) {
                            gameManager.cash -= cost;
                            if (statsManager) statsManager->recordExpense(StatCategory::CASH, static_cast<float>(cost));
                        }
                        continue;
                    }
                    roadManager->handleEvent(*event, window);
                    continue;
                }
                if (industrialManager && industrialManager->isOpen()) {
                    if (industrialManager->isPlacing()) {
                        int cost = industrialManager->confirmPlace(worldPos, bareStepX, bareStepY, gameManager.cash, roadManager.get(), buildingManager.get(), &currentWorld);
                        if (cost > 0) {
                            gameManager.cash -= cost;
                            if (statsManager) statsManager->recordExpense(StatCategory::CASH, static_cast<float>(cost));
                        }
                        continue;
                    }
                    industrialManager->handleEvent(*event, window);
                    continue;
                }
                if (powerManager && powerManager->isOpen()) {
                    if (powerManager->isPlacing()) {
                        int cost = powerManager->confirmPlace(worldPos, bareStepX, bareStepY,
                            gameManager.cash, roadManager.get(), buildingManager.get(),
                            industrialManager.get(), &currentWorld);
                        if (cost > 0) {
                            gameManager.cash -= cost;
                            if (statsManager) statsManager->recordExpense(StatCategory::CASH, static_cast<float>(cost));
                        }
                        continue;
                    }
                    powerManager->handleEvent(*event, window);
                    continue;
                }
                if (demolitionManager && demolitionManager->isActive()) {
                    if (demolitionManager->handleWorldClick(worldPos, bareStepX, bareStepY,
                                                             buildingManager.get(), roadManager.get(),
                                                             industrialManager.get(), powerManager.get(),
                                                             &currentWorld)) {
                        assets.getMusic("demolition_ost").stop();
                        assets.getMusic("demolition_ost").play();
                    }
                } else if (!gameManager.ingame_settings && !gameManager.quitting_warn &&
                           !(statsManager && statsManager->isOpen()) &&
                           !(buildingManager && buildingManager->isDetailOpen()) &&
                           !(industrialManager && industrialManager->isDetailOpen()) &&
                           !(vehicleManager && vehicleManager->isDepotOpen()) &&
                           !(roadManager && roadManager->isStopDetailOpen())) {
                    if (vehicleManager && vehicleManager->isPickingRoute() && roadManager) {
                        sf::Vector2i g = BuildingManager::worldToGrid(
                            worldPos, bareStepX, bareStepY);
                        if (vehicleManager->handleRouteStopClick(g.x, g.y, *roadManager))
                            continue;
                    }
                    bool opened = false;
                    if (vehicleManager && roadManager)
                        opened = vehicleManager->tryOpenDepotAtWorld(
                            worldPos, bareStepX, bareStepY, *roadManager, window, gameView);
                    if (opened) {
                        if (buildingManager) buildingManager->closeDetail();
                        if (industrialManager) industrialManager->closeDetail();
                        if (roadManager) roadManager->closeStopDetail();
                    }
                    if (!opened && roadManager) {
                        opened = roadManager->tryOpenStopDetailAtWorld(
                            worldPos, bareStepX, bareStepY, window, gameView);
                        if (opened) {
                            if (buildingManager) buildingManager->closeDetail();
                            if (industrialManager) industrialManager->closeDetail();
                            if (vehicleManager) vehicleManager->closeDepot(*roadManager);
                        }
                    }
                    if (!opened && buildingManager)
                        opened = buildingManager->tryOpenDetailAtWorld(
                            worldPos, bareStepX, bareStepY, window, gameView);
                    if (!opened && industrialManager)
                        industrialManager->tryOpenDetailAtWorld(
                            worldPos, bareStepX, bareStepY, window, gameView);
                    if (opened && buildingManager && buildingManager->isDetailOpen()) {
                        if (industrialManager) industrialManager->closeDetail();
                        if (roadManager) roadManager->closeStopDetail();
                        if (vehicleManager) vehicleManager->closeDepot(*roadManager);
                    }
                    if (opened && industrialManager && industrialManager->isDetailOpen()) {
                        if (buildingManager) buildingManager->closeDetail();
                        if (roadManager) roadManager->closeStopDetail();
                        if (vehicleManager) vehicleManager->closeDepot(*roadManager);
                    }
                } else if (vehicleManager && vehicleManager->isPickingRoute() && roadManager) {
                    sf::Vector2i g = BuildingManager::worldToGrid(
                        worldPos, bareStepX, bareStepY);
                    vehicleManager->handleRouteStopClick(g.x, g.y, *roadManager);
                }
            }
            if (mb->button == sf::Mouse::Button::Right) {
                if (buildingManager) buildingManager->handleEvent(*event, window);
                if (roadManager)     roadManager->handleEvent(*event, window);
                if (industrialManager) industrialManager->handleEvent(*event, window);
                if (powerManager) powerManager->handleEvent(*event, window);
                if (demolitionManager) demolitionManager->setActive(false);
            }
        }
    }
}

void Game::update() {
    static sf::Clock frameClock;
    sf::Time frameDt = frameClock.restart();
    if (statsManager) statsManager->update(frameDt);
    sf::Vector2f mousePosUI = window.mapPixelToCoords(
        sf::Mouse::getPosition(window), window.getDefaultView());
    mousePosText.setString(mousePosStr(mousePosUI));
    sf::FloatRect mb = mousePosText.getLocalBounds();
    sf::Vector2f vs  = window.getDefaultView().getSize();
    mousePosText.setPosition({vs.x - mb.size.x - mb.position.x - 10.f,
                               vs.y - mb.size.y - mb.position.y - 10.f});

    if (_showWorldMenu && worldGenMenu) {
        if (worldGenMenu->startPressed()) {
            worldGenMenu->resetStart();
            _showWorldMenu = false;
            worldGenMenu->close();
            startNewGame(worldGenMenu->getSettings());
        }
        return;
    }

    if (gameManager.ingame_settings) {
        _lmbWasDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        return;
    }

    sf::Keyboard::Key kUp     = settingsManager ? settingsManager->getKey("camera_up")    : sf::Keyboard::Key::W;
    sf::Keyboard::Key kDown   = settingsManager ? settingsManager->getKey("camera_down")  : sf::Keyboard::Key::S;
    sf::Keyboard::Key kLeft   = settingsManager ? settingsManager->getKey("camera_left")  : sf::Keyboard::Key::A;
    sf::Keyboard::Key kRight  = settingsManager ? settingsManager->getKey("camera_right") : sf::Keyboard::Key::D;
    sf::Keyboard::Key kPlace  = settingsManager ? settingsManager->getKey("place")        : sf::Keyboard::Key::Enter;
    sf::Keyboard::Key kQuit   = settingsManager ? settingsManager->getKey("quit")         : sf::Keyboard::Key::Escape;
    sf::Keyboard::Key kCancel = settingsManager ? settingsManager->getKey("cancel")       : sf::Keyboard::Key::Backspace;

    if (gameManager.game_start) {
        if (gameManager.delete_save_warning) {
            if (sf::Keyboard::isKeyPressed(kPlace)) {
                std::remove("saves/game.save");
                std::remove("saves/buildings.save");
                std::remove("saves/roads.save");
                buildingManager->clear();
                roadManager->clear();
                block_1s.clear();
                gameManager.delete_save_warning = false;
                _showWorldMenu = true;
                worldGenMenu->open();
            }
            if (sf::Keyboard::isKeyPressed(kQuit))
                gameManager.delete_save_warning = false;
            return;
        }

        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        gameManager.option_marked_position = 0;

        if (!gameManager.failed_toload) {
            int hoveredBtn = uiManager.getHoveredButton("Lobby", mousePos);
            if (hoveredBtn > 0) {
                gameManager.option_marked_position = hoveredBtn;
                option_marked.setPosition(optionPositions[hoveredBtn - 1]);
            }
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !_lmbWasDown) {
                switch (gameManager.option_marked_position) {
                    case 1:
                        if (GameManager::fileExists("saves/game.save"))
                            gameManager.delete_save_warning = true;
                        else { _showWorldMenu = true; worldGenMenu->open(); }
                        gameManager.option_marked_position = 0;
                        break;
                    case 2:
                        loadGame();
                        gameManager.option_marked_position = 0;
                        break;
                    case 3:
                        gameManager.ingame_settings = true;
                        gameManager.option_marked_position = 0;
                        break;
                    case 4:
                        window.close();
                        break;
                }
            }
        }
        if (sf::Keyboard::isKeyPressed(kQuit)) {
            if (gameManager.failed_toload)   gameManager.failed_toload = false;
            if (gameManager.ingame_settings) gameManager.ingame_settings = false;
        }

    } else {
        float cameraSpeed = 15.0f * currentZoom;
        if (sf::Keyboard::isKeyPressed(kUp))    gameView.move({0.f,-cameraSpeed});
        if (sf::Keyboard::isKeyPressed(kDown))  gameView.move({0.f, cameraSpeed});
        if (sf::Keyboard::isKeyPressed(kLeft))  gameView.move({-cameraSpeed,0.f});
        if (sf::Keyboard::isKeyPressed(kRight)) gameView.move({ cameraSpeed,0.f});

        gameManager.clock.update();
        date.setString(gameManager.clock.getDateString());
        temperatureText.setString(std::to_string(gameManager.clock.temperature) + "*C");

        if (gameManager.clock.getXt2() == 1) {
            if (statsManager) {
                int workforce = static_cast<int>(gameManager.population * 0.6f);
                int unemp = std::max(0, workforce - gameManager.workers);
                int homeless = std::max(0, gameManager.population - gameManager.accommodation);
                statsManager->setHourAccum(StatCategory::WORKERS,
                    static_cast<float>(gameManager.workers),
                    static_cast<float>(unemp));
                statsManager->setHourAccum(StatCategory::ACCOMM,
                    static_cast<float>(gameManager.accommodation),
                    static_cast<float>(homeless));
                statsManager->onGameHour();
            }
        }

        if (gameManager.clock.getXt3() == 1)
            tickEconomy();

        if (statsManager) {
            long long la = 0; float lr = 0.f;
            if (statsManager->consumeLoanRequest(la, lr)) {
                gameManager.cash += la;
                long long total = static_cast<long long>(la * (1.0 + lr));
                gameManager.loanRemaining     = total;
                gameManager.loanMonthlyPayment = (total + 11) / 12;
                gameManager.loanMonthsLeft    = 12;
                gameManager.loanInterestRate  = lr;
                statsManager->recordIncome(StatCategory::CASH, static_cast<float>(la));
            }

            int immigrCount = 0;
            long long immigrCost = 0;
            if (statsManager->consumeImmigrationRequest(immigrCount, immigrCost)) {
                if (immigrCount > 0 && gameManager.cash >= immigrCost) {
                    gameManager.cash -= immigrCost;
                    gameManager.population += immigrCount;

                    if (citizenManager && buildingManager)
                        citizenManager->distributeToHouses(*buildingManager, gameManager.population);

                    int housingCap = buildingManager ? buildingManager->getTotalHousingCapacity() : 0;
                    gameManager.accommodation = std::min(gameManager.population, housingCap);

                    int workers = 0;
                    if (buildingManager) {
                        for (const auto& h : buildingManager->getPlaced())
                            if (h.assignedWorkIdx >= 0) workers += h.residents;
                    }
                    gameManager.workers = workers;

                    statsManager->recordExpense(StatCategory::CASH, static_cast<float>(immigrCost));
                    statsManager->recordIncome (StatCategory::POPS, static_cast<float>(immigrCount));
                }
            }
        }

        sf::Vector2f worldMouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), gameView);

        gameManager.option_marked_position_2 = 0;

        if (!gameManager.ingame_settings && !gameManager.quitting_warn) {
            int hoveredBtn = uiManager.getHoveredButton("InGame", mousePosUI);
            if (hoveredBtn > 0) gameManager.option_marked_position_2 = hoveredBtn;
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !_lmbWasDown
                && gameManager.option_marked_position_2 == 1) {
                gameManager.ingame_settings = true;
                gameManager.option_marked_position_2 = 0;
            }

            for (size_t i = 0; i < uiIcons.size(); ++i) {
                if (uiIcons[i].getGlobalBounds().contains(mousePosUI)) {
                    uiIcons[i].setColor(sf::Color::Red);
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !_lmbWasDown) {
                        if (i == 0) {
                            if (buildingManager->isOpen()) buildingManager->close();
                            else {
                                buildingManager->open({uiIcons[i].getPosition().x + 70.f,
                                                       uiIcons[i].getPosition().y});
                                roadManager->close();
                                if (industrialManager) industrialManager->close();
                                if (powerManager) powerManager->close();
                                demolitionManager->setActive(false);
                            }
                        }
                        if (i == 1) {
                            if (industrialManager && industrialManager->isOpen()) {
                                industrialManager->close();
                            }
                            else if (industrialManager) {
                                industrialManager->open({uiIcons[i].getPosition().x + 70.f,
                                                         uiIcons[i].getPosition().y});
                                buildingManager->close();
                                roadManager->close();
                                if (powerManager) powerManager->close();
                                demolitionManager->setActive(false);
                            }
                        }
                        if (i == 2) {
                            if (roadManager->isOpen()) roadManager->close();
                            else {
                                roadManager->open({uiIcons[i].getPosition().x + 70.f,
                                                   uiIcons[i].getPosition().y});
                                buildingManager->close();
                                if (industrialManager) industrialManager->close();
                                if (powerManager) powerManager->close();
                                demolitionManager->setActive(false);
                            }
                        }
                        if (i == 3) {
                            if (powerManager && powerManager->isOpen()) powerManager->close();
                            else if (powerManager) {
                                powerManager->open({uiIcons[i].getPosition().x + 70.f,
                                                    uiIcons[i].getPosition().y});
                                buildingManager->close();
                                roadManager->close();
                                if (industrialManager) industrialManager->close();
                                demolitionManager->setActive(false);
                            }
                        }
                        if (i == 5) {
                            if (demolitionManager->isActive()) demolitionManager->setActive(false);
                        else { demolitionManager->setActive(true); buildingManager->close(); roadManager->close(); if (industrialManager) industrialManager->close(); if (powerManager) powerManager->close(); }
                        }
                    }
                } else {
                    uiIcons[i].setColor(sf::Color::White);
                }
            }
        }

        if (buildingManager) {
            int gx = 0, gy = 0;
            buildingManager->update(worldMouse, bareStepX, bareStepY, gx, gy, gameManager.cash, roadManager.get(), &currentWorld);
        }
        if (roadManager)
            roadManager->update(worldMouse, bareStepX, bareStepY, gameManager.cash, buildingManager.get(), &currentWorld);
        if (industrialManager) {
            int gx = 0, gy = 0;
            industrialManager->update(worldMouse, bareStepX, bareStepY, gx, gy, gameManager.cash, roadManager.get(), buildingManager.get(), &currentWorld);
        }
        if (powerManager) {
            int gx = 0, gy = 0;
            powerManager->update(worldMouse, bareStepX, bareStepY, gx, gy, gameManager.cash,
                roadManager.get(), buildingManager.get(), industrialManager.get(), &currentWorld);
        }
        if (demolitionManager)
            demolitionManager->update(worldMouse, bareStepX, bareStepY);

        if (!gameManager.ingame_settings)
            gameManager.xt1 += sf::Keyboard::isKeyPressed(kQuit) ? 1 : -gameManager.xt1;
        if (gameManager.xt1 >= 180) { gameManager.quitting_warn = true; gameManager.xt1 = 0; }

        if (citizenManager && buildingManager && roadManager && industrialManager)
            citizenManager->update(frameDt.asSeconds(), bareStepX, bareStepY,
                                   *buildingManager, *roadManager, *industrialManager);

        updatePowerBalance(gameManager, buildingManager.get(), industrialManager.get(),
                           powerManager.get(), statsManager.get());

        if (vehicleManager && roadManager)
            vehicleManager->update(frameDt.asSeconds(), bareStepX, bareStepY,
                                   gameManager.clock.timeMultiplier,
                                   gameManager.clock.isPaused,
                                   *roadManager, citizenManager.get(),
                                   industrialManager.get());

        if (sf::Keyboard::isKeyPressed(kPlace) && gameManager.quitting_warn) {
            gameManager.restart(block_1s);
            gameManager.game_start = true;
            gameManager.quitting_warn = false;
            _worldGenerated = false;
            mapRenderer->setWorld(nullptr);
            buildingManager->close();
            roadManager->close();
            buildingManager->clear();
            roadManager->clear();
            if (industrialManager) { industrialManager->close(); industrialManager->clear(); }
            if (powerManager) { powerManager->close(); powerManager->clear(); }
            if (statsManager) { statsManager->close(); statsManager->clearHistory(); }
            if (vehicleManager) vehicleManager->clear();
            if (citizenManager) citizenManager->clearStops();
            if (roadManager) roadManager->setHighlightBusStops(false);
        }
        if (sf::Keyboard::isKeyPressed(kCancel) && gameManager.quitting_warn)
            gameManager.quitting_warn = false;

        if (sf::Keyboard::isKeyPressed(kQuit) && gameManager.ingame_settings)
            gameManager.ingame_settings = false;

        if (statsManager) {
            LiveStatValues lv;
            lv.cash          = gameManager.cash;
            lv.population    = gameManager.population;
            lv.loyalty       = gameManager.loyalty;
            lv.workers       = gameManager.workers;
            lv.workforce     = static_cast<int>(gameManager.population * 0.6f);
            lv.accommodation = gameManager.accommodation;
            lv.sick          = gameManager.sick;
            lv.power         = gameManager.power;
            lv.hourProgress  = static_cast<float>(gameManager.clock.min)
                             / static_cast<float>(gameManager.clock.framesPerMinute);
            statsManager->setLiveValues(lv);
        }

        long long values[]     = { gameManager.cash, gameManager.population, gameManager.loyalty,
                                   gameManager.workers, gameManager.accommodation, gameManager.sick, gameManager.power };
        std::string labels[]   = { "Cash: ","Pops: ","Loyalty: ","Workers: ","Accomm: ","Sick: ","Power: " };
        std::string suffixes[] = { "$","","%","","","","" };
        const char* keys[]     = { "cash","pops","loyalty","workers","accomm","sick","power" };

        bool buildingsOpen = (buildingManager   && buildingManager->isOpen()) ||
                             (powerManager      && powerManager->isOpen()) ||
                             (roadManager       && roadManager->isOpen())     ||
                             (industrialManager && industrialManager->isOpen());

        for (size_t i = 0; i < statBoxes.size(); ++i) {
            statBoxes[i].text->setString(labels[i] + formatNumber(values[i]) + suffixes[i]);
            sf::FloatRect tr = statBoxes[i].text->getLocalBounds();
            statBoxes[i].text->setOrigin({tr.position.x + tr.size.x/2.f, tr.position.y + tr.size.y/2.f});
            sf::Vector2f bp = statBoxes[i].box.getPosition();
            sf::Vector2f bs = statBoxes[i].box.getSize();
            statBoxes[i].text->setPosition({bp.x + bs.x/2.f, bp.y + bs.y/2.f});
            bool hovered = statBoxes[i].box.getGlobalBounds().contains(mousePosUI);
            statBoxes[i].box.setFillColor(hovered ? sf::Color(50,50,50) : sf::Color::Black);
            statBoxes[i].box.setOutlineColor(colorForStat(keys[i], values[i]));

            if (hovered && !gameManager.ingame_settings && !gameManager.quitting_warn &&
                sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !_lmbWasDown &&
                statsManager) {
                if (statsManager->isOpen()) {
                    statsManager->switchCategory(static_cast<StatCategory>(i));
                    _lmbWasDown = true;
                } else if (!buildingsOpen) {
                    statsManager->open(static_cast<StatCategory>(i), window.getSize());
                    _lmbWasDown = true;
                }
            }
        }

        music_level_text.setString("100");
        sound_level_text.setString("100");
        game_version_text.setString(game_version);
    }

    _lmbWasDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
}

void Game::render() {
    window.clear(sf::Color::Black);

    if (_showWorldMenu) {
        window.draw(lobby);
        if (worldGenMenu) worldGenMenu->draw(window);
        window.setView(window.getDefaultView());
        window.draw(mousePosText);
        window.display();
        return;
    }

    if (gameManager.game_start) {
        window.draw(lobby);
        window.draw(option_marked);
        if (gameManager.delete_save_warning) window.draw(starting_new_game_warning);
        if (gameManager.failed_toload)       window.draw(nsf);
        if (gameManager.ingame_settings) {
            window.draw(manu_option);
            if (settingsManager) settingsManager->draw(window);
        }
    } else {
        window.setView(gameView);
        mapRenderer->draw(window, gameView, gameManager.clock.temperature);

        for (const auto& spr : borderCrossingSprites)
            window.draw(spr);

        if (roadManager)
            for (const auto& pr : roadManager->getPlaced())
                window.draw(pr.sprite);

        if (buildingManager)
            for (const auto& pb : buildingManager->getPlaced())
                window.draw(pb.sprite);
        if (industrialManager)
            for (const auto& pi : industrialManager->getPlaced())
                window.draw(pi.sprite);
        if (powerManager)
            for (const auto& pp : powerManager->getPlaced())
                window.draw(pp.sprite);

        for (const auto& b : block_1s) window.draw(b);

        if (roadManager)
            roadManager->drawWorldOverlay(window, bareStepX, bareStepY, buildingManager.get(), &currentWorld);
        if (buildingManager)
            buildingManager->drawWorldOverlay(window, bareStepX, bareStepY);
        if (industrialManager)
            industrialManager->drawWorldOverlay(window, bareStepX, bareStepY, roadManager.get(), buildingManager.get(), &currentWorld);
        if (powerManager)
            powerManager->drawWorldOverlay(window, bareStepX, bareStepY);
        if (roadManager && industrialManager)
            roadManager->drawWorkplaceHighlights(window, bareStepX, bareStepY, *industrialManager);
        if (roadManager)
            roadManager->drawBusStopHighlights(window, bareStepX, bareStepY);
        if (demolitionManager) {
            demolitionManager->drawWorldOverlay(window, bareStepX, bareStepY);
            demolitionManager->drawAnimations(window);
        }
        if (vehicleManager)
            vehicleManager->drawWorld(window, bareStepX, bareStepY);
        if (citizenManager)
            citizenManager->drawWorld(window, bareStepX, bareStepY);

        window.setView(window.getDefaultView());
        window.draw(ui);
        window.draw(date);

        for (const auto& sb : statBoxes) { window.draw(sb.box); window.draw(*sb.text); }
        for (const auto& ico : uiIcons)  window.draw(ico);


        pauseBtnSprite.setTexture(assets.getTexture(gameManager.clock.isPaused ? "paused" : "resumed"), true);
        
        std::string speedTex = "speed_1";
        if (gameManager.clock.timeSpeed == 2) speedTex = "speed_2";
        else if (gameManager.clock.timeSpeed == 3) speedTex = "speed_3";
        speedBtnSprite.setTexture(assets.getTexture(speedTex), true);

        window.draw(pauseBtnSprite);
        window.draw(speedBtnSprite);
        window.draw(thermometerSprite);
        window.draw(temperatureText);

        if (buildingManager && buildingManager->isOpen()) buildingManager->drawUI(window);
        if (roadManager && roadManager->isOpen())         roadManager->drawUI(window);
        if (industrialManager && industrialManager->isOpen()) industrialManager->drawUI(window);
        if (powerManager && powerManager->isOpen()) powerManager->drawUI(window);

        if (statsManager && statsManager->isOpen())
            statsManager->draw(window);

        if (buildingManager && buildingManager->isDetailOpen())
            buildingManager->drawDetailPanel(window, industrialManager.get(), gameManager.powerShortage);
        if (industrialManager && industrialManager->isDetailOpen())
            industrialManager->drawDetailPanel(window, buildingManager.get(), &industryWorkers,
                                               gameManager.powerShortage);
        if (vehicleManager && vehicleManager->isDepotOpen())
            vehicleManager->drawDepotPanel(window);
        if (roadManager && roadManager->isStopDetailOpen())
            roadManager->drawStopDetailPanel(window, citizenManager.get(),
                industrialManager.get(), &industryWorkers, buildingManager.get());

        if (gameManager.ingame_settings) {
            window.draw(manu_option);
            if (settingsManager) settingsManager->draw(window);
        }
        if (gameManager.quitting_warn) window.draw(quitting_warrning);
    }

    window.setView(window.getDefaultView());
    window.draw(mousePosText);
    window.display();
}

static std::string trimVarLine(const std::string& s) {
    std::string r = s;
    if (r.size() >= 3 && r.substr(0, 3) == "\xEF\xBB\xBF") r.erase(0, 3);
    size_t a = r.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = r.find_last_not_of(" \t\r\n");
    return r.substr(a, b - a + 1);
}

void Game::loadVariables(const std::string& path) {
    statThresholds.clear();
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        line = trimVarLine(line);
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;

        size_t dot = line.find('.');
        size_t eq  = line.find('=');
        if (dot == std::string::npos || eq == std::string::npos || dot >= eq) continue;

        std::string cat   = trimVarLine(line.substr(0, dot));
        std::string field = trimVarLine(line.substr(dot + 1, eq - dot - 1));
        std::string val   = trimVarLine(line.substr(eq + 1));

        auto& th = statThresholds[cat];
        if      (field == "direction") th.direction = val;
        else if (field == "green")     th.green     = std::atoll(val.c_str());
        else if (field == "orange")    th.orange    = std::atoll(val.c_str());
    }
}

sf::Color Game::colorForStat(const std::string& key, long long value) const {
    auto it = statThresholds.find(key);
    if (it == statThresholds.end()) return sf::Color::White;
    const auto& th = it->second;

    const sf::Color GREEN  ( 60, 220,  90);
    const sf::Color ORANGE (255, 165,   0);
    const sf::Color RED    (220,  70,  70);

    if (th.direction == "low") {
        if (value <= th.green)  return GREEN;
        if (value <= th.orange) return ORANGE;
        return RED;
    }
    if (value >= th.green)  return GREEN;
    if (value >= th.orange) return ORANGE;
    return RED;
}

void Game::tickEconomy() {
    auto& gm = gameManager;

    int numHouses    = buildingManager   ? static_cast<int>(buildingManager->getPlaced().size())   : 0;
    int numRoads     = roadManager       ? static_cast<int>(roadManager->getPlaced().size())       : 0;
    int numFactories = industrialManager ? static_cast<int>(industrialManager->getPlaced().size()) : 0;
    int housingCap   = buildingManager   ? buildingManager->getTotalHousingCapacity() : 0;

    int newWorkers = 0;
    if (buildingManager) {
        for (const auto& h : buildingManager->getPlaced())
            if (h.assignedWorkIdx >= 0) newWorkers += h.residents;
    }
    int workforce = static_cast<int>(gm.population * 0.6f);
    int unemp     = std::max(0, workforce - newWorkers);

    long long cashIncome  = static_cast<long long>(gm.population) * 2 +
                            static_cast<long long>(numFactories) * 50;
    long long cashExpense = static_cast<long long>(numHouses) * 3 +
                            static_cast<long long>(numFactories) * 8 +
                            static_cast<long long>(numRoads) / 2;
    if (unemp > 0) cashExpense += static_cast<long long>(unemp) * 2;

    bool firstOfMonth = (gm.clock.getDay() == 1);
    if (firstOfMonth && gm.loanRemaining > 0 && gm.loanMonthsLeft > 0) {
        long long pay = std::min<long long>(gm.loanMonthlyPayment, gm.loanRemaining);
        cashExpense += pay;
        gm.loanRemaining -= pay;
        gm.loanMonthsLeft--;
        if (gm.loanRemaining <= 0 || gm.loanMonthsLeft <= 0) {
            gm.loanRemaining       = 0;
            gm.loanMonthsLeft      = 0;
            gm.loanMonthlyPayment  = 0;
            gm.loanInterestRate    = 0.f;
        }
    }

    gm.cash += cashIncome - cashExpense;
    if (statsManager) {
        statsManager->recordIncome (StatCategory::CASH, static_cast<float>(cashIncome));
        statsManager->recordExpense(StatCategory::CASH, static_cast<float>(cashExpense));
    }

    int births     = (gm.population > 0) ? std::max(1, gm.population / 100)
                                          : (housingCap > 0 ? 2 : 0);
    int immigrants = (gm.loyalty > 70 && gm.population < housingCap)
                     ? std::min(3, std::max(0, housingCap - gm.population)) : 0;
    int emigrants  = (gm.loyalty < 30 && gm.population > 0) ? std::max(1, gm.population / 200) : 0;
    int deaths     = std::max(0, gm.sick / 10);

    int incoming = births + immigrants;
    int outgoing = deaths + emigrants;
    gm.population = std::max(0, gm.population + incoming - outgoing);
    gm.accommodation = std::min(gm.population, housingCap);

    if (citizenManager && buildingManager)
        citizenManager->distributeToHouses(*buildingManager, gm.population);

    if (statsManager) {
        statsManager->recordIncome (StatCategory::POPS, static_cast<float>(incoming));
        statsManager->recordExpense(StatCategory::POPS, static_cast<float>(outgoing));
    }

    gm.workers = newWorkers;

    int newSick   = std::max(0, static_cast<int>(gm.population * 0.005f) + numFactories / 4);
    int recovered = std::max(0, gm.sick / 8);
    gm.sick = std::max(0, gm.sick + newSick - recovered);
    if (statsManager) {
        statsManager->recordIncome (StatCategory::SICK, static_cast<float>(recovered));
        statsManager->recordExpense(StatCategory::SICK, static_cast<float>(newSick));
    }

    updatePowerBalance(gm, buildingManager.get(), industrialManager.get(), powerManager.get());
    gm.loyalty = 75;
}
