#include "include/SettingsManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr unsigned int FS_TITLE        = 26;
constexpr unsigned int FS_SECTION      = 18;
constexpr unsigned int FS_ROW          = 16;
constexpr unsigned int FS_BTN          = 16;
constexpr unsigned int FS_HINT         = 12;
constexpr unsigned int FS_VERSION      = 12;

constexpr float ROW_H              = 28.f;
constexpr float SECTION_H          = 26.f;
constexpr float ROW_INNER_PAD      = 8.f;

constexpr float VALUE_BOX_W        = 140.f;
constexpr float VALUE_BOX_H        = 22.f;
constexpr float ARROW_BOX_W        = 22.f;

constexpr float BTN_W              = 110.f;
constexpr float BTN_H              = 28.f;
constexpr float BTN_GAP            = 14.f;

const sf::Color COL_BG          (10, 10, 10, 240);
const sf::Color COL_BORDER      (200, 200, 200);
const sf::Color COL_TITLE_BAR   (200, 100, 0);
const sf::Color COL_SECTION     (255, 160, 30);
const sf::Color COL_LABEL       (230, 230, 230);
const sf::Color COL_VALUE_BG    (25, 25, 25);
const sf::Color COL_VALUE_BORDER(120, 120, 120);
const sf::Color COL_VALUE_TEXT  (255, 255, 255);
const sf::Color COL_BTN_BG      (200, 100, 0);
const sf::Color COL_BTN_BG_HOVER(240, 140, 30);
const sf::Color COL_BTN_BG_RED  (160,  40, 40);
const sf::Color COL_BTN_BG_RED_HOVER (210, 70, 70);
const sf::Color COL_BTN_BG_GREY ( 80,  80, 80);
const sf::Color COL_BTN_BG_GREY_HOVER(130, 130, 130);
const sf::Color COL_VALUE_BG_HOVER(55, 55, 55);
const sf::Color COL_HINT        (170, 170, 170);
const sf::Color COL_CAPTURE_BG  (180,  40, 40);

sf::Color hoverIf(bool hover, const sf::Color& normal, const sf::Color& hov) {
    return hover ? hov : normal;
}

} // anonymous namespace

SettingsManager::SettingsManager(const sf::Font& font) : _font(font) {
    resetDefaults();
}

void SettingsManager::resetDefaults() {
    _musicVolume = 50;
    _soundVolume = 100;
    _fullscreen  = true;
    _keys.clear();
    _keys["camera_up"]    = sf::Keyboard::Key::W;
    _keys["camera_down"]  = sf::Keyboard::Key::S;
    _keys["camera_left"]  = sf::Keyboard::Key::A;
    _keys["camera_right"] = sf::Keyboard::Key::D;
    _keys["place"]        = sf::Keyboard::Key::Enter;
    _keys["cycle"]        = sf::Keyboard::Key::R;
    _keys["save"]         = sf::Keyboard::Key::F1;
    _keys["quit"]         = sf::Keyboard::Key::Escape;
    _keys["cancel"]       = sf::Keyboard::Key::Backspace;
}

std::string SettingsManager::trim(const std::string& s) {
    std::string res = s;
    if (res.size() >= 3 && res.substr(0, 3) == "\xEF\xBB\xBF") res.erase(0, 3);
    size_t a = res.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = res.find_last_not_of(" \t\r\n");
    return res.substr(a, b - a + 1);
}

void SettingsManager::loadLabels(const std::string& filePath) {
    _labels.clear();
    std::ifstream f(filePath);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t bar = line.find('|');
        if (bar == std::string::npos) continue;
        std::string k = trim(line.substr(0, bar));
        std::string v = trim(line.substr(bar + 1));
        if (!k.empty()) _labels[k] = v;
    }
}

std::string SettingsManager::getLabel(const std::string& key) const {
    auto it = _labels.find(key);
    if (it != _labels.end()) return it->second;
    return "[" + key + "]";
}

sf::Keyboard::Key SettingsManager::getKey(const std::string& action) const {
    auto it = _keys.find(action);
    if (it != _keys.end()) return it->second;
    return sf::Keyboard::Key::Unknown;
}

void SettingsManager::loadSettings(const std::string& filePath, const std::string& legacyFile) {
    bool loadedMain = false;
    std::ifstream f(filePath);
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim(line.substr(0, eq));
            std::string v = trim(line.substr(eq + 1));
            if      (k == "version")       _version     = v;
            else if (k == "music_volume")  _musicVolume = std::clamp(std::atoi(v.c_str()), SM_VOLUME_MIN, SM_VOLUME_MAX);
            else if (k == "sound_volume")  _soundVolume = std::clamp(std::atoi(v.c_str()), SM_VOLUME_MIN, SM_VOLUME_MAX);
            else if (k == "fullscreen")    _fullscreen  = (v == "1" || v == "true" || v == "on");
            else if (k.rfind("key_", 0) == 0) {
                std::string action = k.substr(4);
                sf::Keyboard::Key sk = stringToKey(v);
                if (sk != sf::Keyboard::Key::Unknown) _keys[action] = sk;
            }
        }
        loadedMain = true;
    }

    if (!loadedMain && !legacyFile.empty()) {
        std::ifstream lf(legacyFile);
        if (lf.is_open()) {
            std::string verLine, mv, sv;
            if (std::getline(lf, verLine)) _version     = trim(verLine);
            if (std::getline(lf, mv))      _musicVolume = std::clamp(std::atoi(trim(mv).c_str()), SM_VOLUME_MIN, SM_VOLUME_MAX);
            if (std::getline(lf, sv))      _soundVolume = std::clamp(std::atoi(trim(sv).c_str()), SM_VOLUME_MIN, SM_VOLUME_MAX);
        }
    } else if (!loadedMain) {
        std::ifstream lf("settings.ini");
        if (lf.is_open()) {
            std::string verLine;
            if (std::getline(lf, verLine)) _version = trim(verLine);
        }
    }
}

void SettingsManager::saveSettings(const std::string& filePath) const {
    fs::path p(filePath);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream f(filePath, std::ios::trunc);
    if (!f.is_open()) return;
    f << "# Game settings - auto-generated. You can edit values manually,\n"
      << "# but do not rename the keys.\n";
    f << "version="      << _version     << "\n";
    f << "music_volume=" << _musicVolume << "\n";
    f << "sound_volume=" << _soundVolume << "\n";
    f << "fullscreen="   << (_fullscreen ? 1 : 0) << "\n";
    for (const auto& [action, key] : _keys) {
        f << "key_" << action << "=" << keyToString(key) << "\n";
    }
}

void SettingsManager::addHit(float x, float y, float w, float h, const std::string& id) {
    _hitBoxes.push_back({sf::FloatRect({x, y}, {w, h}), id});
}

const SettingsManager::HitBox* SettingsManager::hitAt(sf::Vector2f p) const {
    for (const auto& h : _hitBoxes) if (h.rect.contains(p)) return &h;
    return nullptr;
}

SettingsResult SettingsManager::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (_captureAction.empty()) {
        if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (mb->button == sf::Mouse::Button::Left) {
                sf::Vector2f mp = window.mapPixelToCoords(
                    sf::Mouse::getPosition(window), window.getDefaultView());
                const HitBox* h = hitAt(mp);
                if (!h) return SettingsResult::NONE;

                _clickConsumed = true;
                const std::string& id = h->id;
                if (id == "music_dec") { _musicVolume = std::max(SM_VOLUME_MIN, _musicVolume - SM_VOLUME_STEP); return SettingsResult::APPLY_AUDIO; }
                if (id == "music_inc") { _musicVolume = std::min(SM_VOLUME_MAX, _musicVolume + SM_VOLUME_STEP); return SettingsResult::APPLY_AUDIO; }
                if (id == "sound_dec") { _soundVolume = std::max(SM_VOLUME_MIN, _soundVolume - SM_VOLUME_STEP); return SettingsResult::APPLY_AUDIO; }
                if (id == "sound_inc") { _soundVolume = std::min(SM_VOLUME_MAX, _soundVolume + SM_VOLUME_STEP); return SettingsResult::APPLY_AUDIO; }
                if (id == "fullscreen_toggle") {
                    _fullscreen = !_fullscreen;
                    return _fullscreen ? SettingsResult::APPLY_FULLSCREEN
                                       : SettingsResult::APPLY_WINDOWED;
                }
                if (id.rfind("key_", 0) == 0) {
                    _captureAction = id.substr(4);
                    return SettingsResult::NONE;
                }
                if (id == "btn_save")  return SettingsResult::NONE;
                if (id == "btn_reset") { resetDefaults(); return SettingsResult::APPLY_AUDIO; }
                if (id == "btn_close") return SettingsResult::CLOSE_REQUESTED;
            }
        }
        return SettingsResult::NONE;
    }

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            _captureAction.clear();
            return SettingsResult::NONE;
        }
        _keys[_captureAction] = kp->code;
        _captureAction.clear();
    }
    return SettingsResult::NONE;
}

void SettingsManager::draw(sf::RenderWindow& w) {
    _hitBoxes.clear();
    _mousePos = w.mapPixelToCoords(sf::Mouse::getPosition(w), w.getDefaultView());
    drawPanel(w);

    float curY = SM_PANEL_Y + 50.f;

    drawSectionHeader(w, "audio_section", curY);
    drawVolumeRow(w, "music_volume", _musicVolume, "music_dec", "music_inc", curY);
    drawVolumeRow(w, "sound_volume", _soundVolume, "sound_dec", "sound_inc", curY);
    curY += 6.f;

    drawSectionHeader(w, "display_section", curY);
    drawFullscreenRow(w, curY);
    curY += 6.f;

    drawSectionHeader(w, "keys_section", curY);
    const std::vector<std::string> keyOrder = {
        "camera_up", "camera_down", "camera_left", "camera_right",
        "place", "cycle", "save", "quit", "cancel"
    };
    for (const auto& a : keyOrder) drawKeyRow(w, a, curY);
    curY += 8.f;

    drawBottomButtons(w, curY);
}

void SettingsManager::drawPanel(sf::RenderWindow& w) {
    sf::RectangleShape bg({SM_PANEL_W, SM_PANEL_H});
    bg.setPosition({SM_PANEL_X, SM_PANEL_Y});
    bg.setFillColor(COL_BG);
    bg.setOutlineColor(COL_BORDER);
    bg.setOutlineThickness(2.f);
    w.draw(bg);

    sf::RectangleShape tb({SM_PANEL_W, 36.f});
    tb.setPosition({SM_PANEL_X, SM_PANEL_Y});
    tb.setFillColor(COL_TITLE_BAR);
    w.draw(tb);

    sf::Text title(_font, getLabel("title"), FS_TITLE);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb1 = title.getLocalBounds();
    title.setOrigin({tb1.position.x + tb1.size.x / 2.f, tb1.position.y + tb1.size.y / 2.f});
    title.setPosition({SM_PANEL_X + SM_PANEL_W / 2.f, SM_PANEL_Y + 18.f});
    w.draw(title);

    sf::Text ver(_font, getLabel("version_label") + ": " + _version, FS_VERSION);
    ver.setFillColor(COL_HINT);
    sf::FloatRect vb = ver.getLocalBounds();
    ver.setOrigin({vb.position.x + vb.size.x, vb.position.y});
    ver.setPosition({SM_PANEL_X + SM_PANEL_W - 8.f, SM_PANEL_Y + SM_PANEL_H - 18.f});
    w.draw(ver);

    sf::Text hint(_font, getLabel("hint_back"), FS_HINT);
    hint.setFillColor(COL_HINT);
    hint.setPosition({SM_PANEL_X + 8.f, SM_PANEL_Y + SM_PANEL_H - 18.f});
    w.draw(hint);
}

void SettingsManager::drawSectionHeader(sf::RenderWindow& w, const std::string& labelKey, float& curY) {
    sf::Text t(_font, "--- " + getLabel(labelKey) + " ---", FS_SECTION);
    t.setFillColor(COL_SECTION);
    t.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y});
    t.setPosition({SM_PANEL_X + SM_PANEL_W / 2.f, curY});
    w.draw(t);
    curY += SECTION_H;
}

void SettingsManager::drawVolumeRow(sf::RenderWindow& w, const std::string& labelKey,
                                     int value, const std::string& idDec, const std::string& idInc,
                                     float& curY)
{
    sf::Text label(_font, getLabel(labelKey), FS_ROW);
    label.setFillColor(COL_LABEL);
    label.setPosition({SM_PANEL_X + ROW_INNER_PAD + 4.f, curY + (ROW_H - FS_ROW) / 2.f - 2.f});
    w.draw(label);

    float valueX = SM_PANEL_X + SM_PANEL_W - ROW_INNER_PAD - VALUE_BOX_W - 4.f;
    float valueY = curY + (ROW_H - VALUE_BOX_H) / 2.f;

    sf::FloatRect decRect({valueX, valueY}, {ARROW_BOX_W, VALUE_BOX_H});
    sf::FloatRect incRect({valueX + VALUE_BOX_W - ARROW_BOX_W, valueY}, {ARROW_BOX_W, VALUE_BOX_H});
    bool hoverDec = decRect.contains(_mousePos);
    bool hoverInc = incRect.contains(_mousePos);

    sf::RectangleShape decBtn({ARROW_BOX_W, VALUE_BOX_H});
    decBtn.setPosition({valueX, valueY});
    decBtn.setFillColor(hoverIf(hoverDec, COL_BTN_BG, COL_BTN_BG_HOVER));
    w.draw(decBtn);
    sf::Text decT(_font, "<", FS_ROW);
    decT.setFillColor(sf::Color::White);
    decT.setStyle(sf::Text::Style::Bold);
    sf::FloatRect db = decT.getLocalBounds();
    decT.setOrigin({db.position.x + db.size.x / 2.f, db.position.y + db.size.y / 2.f});
    decT.setPosition({valueX + ARROW_BOX_W / 2.f, valueY + VALUE_BOX_H / 2.f - 1.f});
    w.draw(decT);

    sf::RectangleShape midBox({VALUE_BOX_W - ARROW_BOX_W * 2.f, VALUE_BOX_H});
    midBox.setPosition({valueX + ARROW_BOX_W, valueY});
    midBox.setFillColor(COL_VALUE_BG);
    midBox.setOutlineColor(COL_VALUE_BORDER);
    midBox.setOutlineThickness(1.f);
    w.draw(midBox);

    float fillRatio = static_cast<float>(value) / static_cast<float>(SM_VOLUME_MAX);
    sf::RectangleShape fill({(VALUE_BOX_W - ARROW_BOX_W * 2.f - 2.f) * fillRatio, VALUE_BOX_H - 2.f});
    fill.setPosition({valueX + ARROW_BOX_W + 1.f, valueY + 1.f});
    fill.setFillColor(sf::Color(200, 100, 0, 140));
    w.draw(fill);

    sf::Text valT(_font, std::to_string(value), FS_ROW);
    valT.setFillColor(COL_VALUE_TEXT);
    valT.setStyle(sf::Text::Style::Bold);
    sf::FloatRect vb = valT.getLocalBounds();
    valT.setOrigin({vb.position.x + vb.size.x / 2.f, vb.position.y + vb.size.y / 2.f});
    valT.setPosition({valueX + VALUE_BOX_W / 2.f, valueY + VALUE_BOX_H / 2.f - 1.f});
    w.draw(valT);

    sf::RectangleShape incBtn({ARROW_BOX_W, VALUE_BOX_H});
    incBtn.setPosition({valueX + VALUE_BOX_W - ARROW_BOX_W, valueY});
    incBtn.setFillColor(hoverIf(hoverInc, COL_BTN_BG, COL_BTN_BG_HOVER));
    w.draw(incBtn);
    sf::Text incT(_font, ">", FS_ROW);
    incT.setFillColor(sf::Color::White);
    incT.setStyle(sf::Text::Style::Bold);
    sf::FloatRect ib = incT.getLocalBounds();
    incT.setOrigin({ib.position.x + ib.size.x / 2.f, ib.position.y + ib.size.y / 2.f});
    incT.setPosition({valueX + VALUE_BOX_W - ARROW_BOX_W / 2.f, valueY + VALUE_BOX_H / 2.f - 1.f});
    w.draw(incT);

    addHit(valueX,                              valueY, ARROW_BOX_W, VALUE_BOX_H, idDec);
    addHit(valueX + VALUE_BOX_W - ARROW_BOX_W,  valueY, ARROW_BOX_W, VALUE_BOX_H, idInc);
    curY += ROW_H;
}

void SettingsManager::drawFullscreenRow(sf::RenderWindow& w, float& curY) {
    sf::Text label(_font, getLabel("fullscreen"), FS_ROW);
    label.setFillColor(COL_LABEL);
    label.setPosition({SM_PANEL_X + ROW_INNER_PAD + 4.f, curY + (ROW_H - FS_ROW) / 2.f - 2.f});
    w.draw(label);

    float valueX = SM_PANEL_X + SM_PANEL_W - ROW_INNER_PAD - VALUE_BOX_W - 4.f;
    float valueY = curY + (ROW_H - VALUE_BOX_H) / 2.f;

    sf::FloatRect rect({valueX, valueY}, {VALUE_BOX_W, VALUE_BOX_H});
    bool hover = rect.contains(_mousePos);

    sf::RectangleShape box({VALUE_BOX_W, VALUE_BOX_H});
    box.setPosition({valueX, valueY});
    box.setFillColor(hoverIf(hover, COL_BTN_BG, COL_BTN_BG_HOVER));
    w.draw(box);

    std::string txt = getLabel(_fullscreen ? "fullscreen_on" : "fullscreen_off");
    sf::Text t(_font, txt, FS_ROW);
    t.setFillColor(sf::Color::White);
    t.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
    t.setPosition({valueX + VALUE_BOX_W / 2.f, valueY + VALUE_BOX_H / 2.f - 1.f});
    w.draw(t);

    addHit(valueX, valueY, VALUE_BOX_W, VALUE_BOX_H, "fullscreen_toggle");
    curY += ROW_H;
}

void SettingsManager::drawKeyRow(sf::RenderWindow& w, const std::string& action, float& curY) {
    sf::Text label(_font, getLabel("key_" + action), FS_ROW);
    label.setFillColor(COL_LABEL);
    label.setPosition({SM_PANEL_X + ROW_INNER_PAD + 4.f, curY + (ROW_H - FS_ROW) / 2.f - 2.f});
    w.draw(label);

    float valueX = SM_PANEL_X + SM_PANEL_W - ROW_INNER_PAD - VALUE_BOX_W - 4.f;
    float valueY = curY + (ROW_H - VALUE_BOX_H) / 2.f;

    bool capturing = (_captureAction == action);
    sf::FloatRect rect({valueX, valueY}, {VALUE_BOX_W, VALUE_BOX_H});
    bool hover = !capturing && rect.contains(_mousePos);

    sf::RectangleShape box({VALUE_BOX_W, VALUE_BOX_H});
    box.setPosition({valueX, valueY});
    box.setFillColor(capturing ? COL_CAPTURE_BG : hoverIf(hover, COL_VALUE_BG, COL_VALUE_BG_HOVER));
    box.setOutlineColor(capturing ? sf::Color(255, 80, 80) : (hover ? sf::Color(220, 120, 0) : COL_VALUE_BORDER));
    box.setOutlineThickness(1.f);
    w.draw(box);

    std::string txt = capturing ? getLabel("hint_press_key") : keyToString(_keys[action]);
    unsigned int fs = capturing ? FS_HINT : FS_ROW;
    sf::Text t(_font, txt, fs);
    t.setFillColor(sf::Color::White);
    t.setStyle(sf::Text::Style::Bold);
    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
    t.setPosition({valueX + VALUE_BOX_W / 2.f, valueY + VALUE_BOX_H / 2.f - 1.f});
    w.draw(t);

    addHit(valueX, valueY, VALUE_BOX_W, VALUE_BOX_H, "key_" + action);
    curY += ROW_H;
}

void SettingsManager::drawBottomButtons(sf::RenderWindow& w, float& curY) {
    float totalW = BTN_W * 3.f + BTN_GAP * 2.f;
    float startX = SM_PANEL_X + (SM_PANEL_W - totalW) / 2.f;
    float y      = curY;

    auto drawBtn = [&](float x, const std::string& labelKey,
                       const sf::Color& col, const sf::Color& colHover,
                       const std::string& id) {
        sf::FloatRect rect({x, y}, {BTN_W, BTN_H});
        bool hover = rect.contains(_mousePos);
        sf::RectangleShape b({BTN_W, BTN_H});
        b.setPosition({x, y});
        b.setFillColor(hoverIf(hover, col, colHover));
        b.setOutlineColor(sf::Color::White);
        b.setOutlineThickness(hover ? 2.f : 1.f);
        w.draw(b);
        sf::Text t(_font, getLabel(labelKey), FS_BTN);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Style::Bold);
        sf::FloatRect tb = t.getLocalBounds();
        t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
        t.setPosition({x + BTN_W / 2.f, y + BTN_H / 2.f - 1.f});
        w.draw(t);
        addHit(x, y, BTN_W, BTN_H, id);
    };

    drawBtn(startX,                       "btn_save",  COL_BTN_BG,      COL_BTN_BG_HOVER,      "btn_save");
    drawBtn(startX + BTN_W + BTN_GAP,     "btn_reset", COL_BTN_BG_RED,  COL_BTN_BG_RED_HOVER,  "btn_reset");
    drawBtn(startX + (BTN_W + BTN_GAP)*2, "btn_close", COL_BTN_BG_GREY, COL_BTN_BG_GREY_HOVER, "btn_close");
}

std::string SettingsManager::keyToString(sf::Keyboard::Key k) {
    using K = sf::Keyboard::Key;
    switch (k) {
        case K::A: return "A"; case K::B: return "B"; case K::C: return "C"; case K::D: return "D";
        case K::E: return "E"; case K::F: return "F"; case K::G: return "G"; case K::H: return "H";
        case K::I: return "I"; case K::J: return "J"; case K::K: return "K"; case K::L: return "L";
        case K::M: return "M"; case K::N: return "N"; case K::O: return "O"; case K::P: return "P";
        case K::Q: return "Q"; case K::R: return "R"; case K::S: return "S"; case K::T: return "T";
        case K::U: return "U"; case K::V: return "V"; case K::W: return "W"; case K::X: return "X";
        case K::Y: return "Y"; case K::Z: return "Z";
        case K::Num0: return "0"; case K::Num1: return "1"; case K::Num2: return "2";
        case K::Num3: return "3"; case K::Num4: return "4"; case K::Num5: return "5";
        case K::Num6: return "6"; case K::Num7: return "7"; case K::Num8: return "8";
        case K::Num9: return "9";
        case K::Escape:    return "Escape";
        case K::LControl:  return "LCtrl";    case K::LShift:    return "LShift";
        case K::LAlt:      return "LAlt";     case K::LSystem:   return "LSystem";
        case K::RControl:  return "RCtrl";    case K::RShift:    return "RShift";
        case K::RAlt:      return "RAlt";     case K::RSystem:   return "RSystem";
        case K::Menu:      return "Menu";     case K::LBracket:  return "[";
        case K::RBracket:  return "]";        case K::Semicolon: return ";";
        case K::Comma:     return ",";        case K::Period:    return ".";
        case K::Apostrophe:return "'";        case K::Slash:     return "/";
        case K::Backslash: return "\\";       case K::Grave:     return "`";
        case K::Equal:     return "=";        case K::Hyphen:    return "-";
        case K::Space:     return "Space";    case K::Enter:     return "Enter";
        case K::Backspace: return "Backspace";case K::Tab:       return "Tab";
        case K::PageUp:    return "PageUp";   case K::PageDown:  return "PageDown";
        case K::End:       return "End";      case K::Home:      return "Home";
        case K::Insert:    return "Insert";   case K::Delete:    return "Delete";
        case K::Add:       return "Num+";     case K::Subtract:  return "Num-";
        case K::Multiply:  return "Num*";     case K::Divide:    return "Num/";
        case K::Left:      return "Left";     case K::Right:     return "Right";
        case K::Up:        return "Up";       case K::Down:      return "Down";
        case K::Numpad0:   return "Numpad0";  case K::Numpad1:   return "Numpad1";
        case K::Numpad2:   return "Numpad2";  case K::Numpad3:   return "Numpad3";
        case K::Numpad4:   return "Numpad4";  case K::Numpad5:   return "Numpad5";
        case K::Numpad6:   return "Numpad6";  case K::Numpad7:   return "Numpad7";
        case K::Numpad8:   return "Numpad8";  case K::Numpad9:   return "Numpad9";
        case K::F1:  return "F1";  case K::F2:  return "F2";  case K::F3:  return "F3";
        case K::F4:  return "F4";  case K::F5:  return "F5";  case K::F6:  return "F6";
        case K::F7:  return "F7";  case K::F8:  return "F8";  case K::F9:  return "F9";
        case K::F10: return "F10"; case K::F11: return "F11"; case K::F12: return "F12";
        case K::F13: return "F13"; case K::F14: return "F14"; case K::F15: return "F15";
        case K::Pause: return "Pause";
        default:       return "Unknown";
    }
}

sf::Keyboard::Key SettingsManager::stringToKey(const std::string& s) {
    using K = sf::Keyboard::Key;
    static const std::map<std::string, K> map = {
        {"A",K::A},{"B",K::B},{"C",K::C},{"D",K::D},{"E",K::E},{"F",K::F},{"G",K::G},
        {"H",K::H},{"I",K::I},{"J",K::J},{"K",K::K},{"L",K::L},{"M",K::M},{"N",K::N},
        {"O",K::O},{"P",K::P},{"Q",K::Q},{"R",K::R},{"S",K::S},{"T",K::T},{"U",K::U},
        {"V",K::V},{"W",K::W},{"X",K::X},{"Y",K::Y},{"Z",K::Z},
        {"0",K::Num0},{"1",K::Num1},{"2",K::Num2},{"3",K::Num3},{"4",K::Num4},
        {"5",K::Num5},{"6",K::Num6},{"7",K::Num7},{"8",K::Num8},{"9",K::Num9},
        {"Escape",K::Escape},{"LCtrl",K::LControl},{"LShift",K::LShift},{"LAlt",K::LAlt},
        {"LSystem",K::LSystem},{"RCtrl",K::RControl},{"RShift",K::RShift},{"RAlt",K::RAlt},
        {"RSystem",K::RSystem},{"Menu",K::Menu},
        {"[",K::LBracket},{"]",K::RBracket},{";",K::Semicolon},{",",K::Comma},
        {".",K::Period},{"'",K::Apostrophe},{"/",K::Slash},{"\\",K::Backslash},
        {"`",K::Grave},{"=",K::Equal},{"-",K::Hyphen},
        {"Space",K::Space},{"Enter",K::Enter},{"Backspace",K::Backspace},{"Tab",K::Tab},
        {"PageUp",K::PageUp},{"PageDown",K::PageDown},{"End",K::End},{"Home",K::Home},
        {"Insert",K::Insert},{"Delete",K::Delete},
        {"Num+",K::Add},{"Num-",K::Subtract},{"Num*",K::Multiply},{"Num/",K::Divide},
        {"Left",K::Left},{"Right",K::Right},{"Up",K::Up},{"Down",K::Down},
        {"Numpad0",K::Numpad0},{"Numpad1",K::Numpad1},{"Numpad2",K::Numpad2},
        {"Numpad3",K::Numpad3},{"Numpad4",K::Numpad4},{"Numpad5",K::Numpad5},
        {"Numpad6",K::Numpad6},{"Numpad7",K::Numpad7},{"Numpad8",K::Numpad8},
        {"Numpad9",K::Numpad9},
        {"F1",K::F1},{"F2",K::F2},{"F3",K::F3},{"F4",K::F4},{"F5",K::F5},
        {"F6",K::F6},{"F7",K::F7},{"F8",K::F8},{"F9",K::F9},{"F10",K::F10},
        {"F11",K::F11},{"F12",K::F12},{"F13",K::F13},{"F14",K::F14},{"F15",K::F15},
        {"Pause",K::Pause}
    };
    auto it = map.find(s);
    return it != map.end() ? it->second : K::Unknown;
}
