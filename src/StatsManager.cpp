#include "include/StatsManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

const sf::Color COL_BG_PANEL    (10, 10, 10, 245);
const sf::Color COL_BORDER      (255, 255, 255);
const sf::Color COL_TITLE_BAR   (200, 100, 0);
const sf::Color COL_LABEL       (230, 230, 230);
const sf::Color COL_HINT        (170, 170, 170);
const sf::Color COL_SECTION     (255, 160,  30);
const sf::Color COL_CHART_BG    ( 15,  15,  15);
const sf::Color COL_CHART_GRID  ( 60,  60,  60);
const sf::Color COL_CHART_AXIS  (130, 130, 130);
const sf::Color COL_LINE_GREEN  ( 55, 210,  85);
const sf::Color COL_LINE_RED    (215,  60,  60);

constexpr float ST_CLOSE_BTN_SIZE = 24.f;
constexpr float ST_SWITCH_ANIM_S  = 0.22f;
constexpr float ST_AXIS_LEFT      = 64.f;
constexpr float ST_AXIS_BOTTOM    = 26.f;
constexpr float ST_CHART_HEADER   = 58.f;
constexpr int   ST_Y_TICKS        = 4;
const sf::Color COL_BTN         (200, 100,   0);
const sf::Color COL_BTN_HOVER   (240, 140,  30);
const sf::Color COL_BTN_ACTIVE  (255, 160,  30);
const sf::Color COL_BTN_GREEN   ( 40, 140,  60);
const sf::Color COL_BTN_GREEN_H ( 60, 180,  90);

sf::Color hoverIf(bool h, sf::Color a, sf::Color b) { return h ? b : a; }

float niceAxisMax(float v) {
    if (v <= 0.f) return 1.f;
    float mag = std::pow(10.f, std::floor(std::log10(v)));
    float norm = v / mag;
    if (norm <= 1.f) return mag;
    if (norm <= 2.f) return 2.f * mag;
    if (norm <= 5.f) return 5.f * mag;
    return 10.f * mag;
}

std::string formatAxisValue(float v) {
    if (v >= 1000000.f) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1fM", v / 1e6f);
        std::string s(buf);
        auto p = s.find(".0"); if (p != std::string::npos) s.erase(p, 2);
        return s;
    }
    if (v >= 1000.f) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1fK", v / 1e3f);
        std::string s(buf);
        auto p = s.find(".0"); if (p != std::string::npos) s.erase(p, 2);
        return s;
    }
    if (v >= 10.f) return std::to_string(static_cast<int>(v + 0.5f));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

std::string formatMoney(long long v) {
    char buf[64];
    if      (v >= 1000000000LL) std::snprintf(buf, sizeof(buf), "%.1fB",  v / 1e9);
    else if (v >= 1000000LL)    std::snprintf(buf, sizeof(buf), "%.1fM",  v / 1e6);
    else if (v >= 1000LL)       std::snprintf(buf, sizeof(buf), "%.1fK",  v / 1e3);
    else                        std::snprintf(buf, sizeof(buf), "%lld",   v);
    std::string s(buf);
    auto p = s.find(".0"); if (p != std::string::npos) s.erase(p, 2);
    return s;
}

const char* scaleLabel(ChartScale s) {
    switch (s) {
        case ChartScale::DAILY:   return "Daily";
        case ChartScale::MONTHLY: return "Monthly";
        case ChartScale::YEARLY:  return "Yearly";
    }
    return "Daily";
}

} // anonymous namespace

StatsManager::StatsManager(const sf::Font& font, const sf::Texture& closeTexture)
    : _font(font), _closeBtn(closeTexture)
{
    auto ts = closeTexture.getSize();
    float maxDim = static_cast<float>(std::max(ts.x, ts.y));
    if (maxDim > 0.f) {
        float s = ST_CLOSE_BTN_SIZE / maxDim;
        _closeBtn.setScale({s, s});
        _closeBtnSize = ST_CLOSE_BTN_SIZE;
    }
}

const char* StatsManager::categoryToTitle(StatCategory c) {
    switch (c) {
        case StatCategory::CASH:    return "CASH";
        case StatCategory::POPS:    return "POPULATION";
        case StatCategory::LOYALTY: return "LOYALTY";
        case StatCategory::WORKERS: return "WORKERS";
        case StatCategory::ACCOMM:  return "ACCOMMODATION";
        case StatCategory::SICK:    return "HEALTH";
        case StatCategory::POWER:   return "POWER";
        default:                    return "STATS";
    }
}

const char* StatsManager::categoryToKey(StatCategory c) {
    switch (c) {
        case StatCategory::CASH:    return "cash";
        case StatCategory::POPS:    return "pops";
        case StatCategory::LOYALTY: return "loyalty";
        case StatCategory::WORKERS: return "workers";
        case StatCategory::ACCOMM:  return "accomm";
        case StatCategory::SICK:    return "sick";
        case StatCategory::POWER:   return "power";
        default:                    return "unknown";
    }
}

const char* StatsManager::incomeLegend(StatCategory c) {
    switch (c) {
        case StatCategory::CASH:    return "Income";
        case StatCategory::POPS:    return "New citizens";
        case StatCategory::LOYALTY: return "Support gained";
        case StatCategory::WORKERS: return "Employed";
        case StatCategory::ACCOMM:  return "Housed";
        case StatCategory::SICK:    return "Recovered";
        case StatCategory::POWER:   return "Production";
        default:                    return "Positive";
    }
}

const char* StatsManager::expenseLegend(StatCategory c) {
    switch (c) {
        case StatCategory::CASH:    return "Expenses";
        case StatCategory::POPS:    return "Deaths & emigration";
        case StatCategory::LOYALTY: return "Support lost";
        case StatCategory::WORKERS: return "Unemployed";
        case StatCategory::ACCOMM:  return "Homeless";
        case StatCategory::SICK:    return "New cases";
        case StatCategory::POWER:   return "Consumption";
        default:                    return "Negative";
    }
}

const std::vector<LoanOption>& StatsManager::getLoanOptions() {
    static const std::vector<LoanOption> opts = {
        { 10000LL,  0.12f },
        { 50000LL,  0.08f },
        {100000LL,  0.05f },
        {500000LL,  0.03f }
    };
    return opts;
}

const std::vector<ImmigrationOption>& StatsManager::getImmigrationOptions() {
    static const std::vector<ImmigrationOption> opts = {
        {  1,   1LL },
        { 10,  10LL },
        {100, 100LL }
    };
    return opts;
}

void StatsManager::loadDescriptions(const std::string& langDir) {
    auto load = [&](StatCategory cat) {
        std::string path = langDir + "/" + categoryToKey(cat) + ".txt";
        std::ifstream f(path);
        if (!f.is_open()) {
            _descriptions[cat] = std::string("(missing description: ") + path + ")";
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string s = ss.str();
        if (s.size() >= 3 && s.substr(0, 3) == "\xEF\xBB\xBF") s.erase(0, 3);
        _descriptions[cat] = s;
    };
    for (int i = 0; i < (int)StatCategory::COUNT; ++i)
        load(static_cast<StatCategory>(i));
}

void StatsManager::recordIncome(StatCategory cat, float amount) {
    if (amount <= 0.f) return;
    _hourAccum[(size_t)cat].income += amount;
}

void StatsManager::recordExpense(StatCategory cat, float amount) {
    if (amount <= 0.f) return;
    _hourAccum[(size_t)cat].expense += amount;
}

void StatsManager::setHourAccum(StatCategory cat, float income, float expense) {
    _hourAccum[(size_t)cat].income  = income;
    _hourAccum[(size_t)cat].expense = expense;
}

void StatsManager::setLiveValues(const LiveStatValues& values) {
    _liveValues = values;
}

std::string StatsManager::formatWithSpaces(long long num) {
    std::string s = std::to_string(num < 0 ? -num : num);
    std::string out;
    int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i) {
        if (i > 0 && (n - i) % 3 == 0) out += ' ';
        out += s[i];
    }
    if (num < 0) out = "-" + out;
    return out;
}

std::string StatsManager::formatCurrentValue(StatCategory cat) const {
    switch (cat) {
        case StatCategory::CASH:
            return formatWithSpaces(_liveValues.cash) + "$";
        case StatCategory::POPS:
            return formatWithSpaces(_liveValues.population);
        case StatCategory::LOYALTY: {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", _liveValues.loyalty);
            return buf;
        }
        case StatCategory::WORKERS: {
            int pct = (_liveValues.workforce > 0)
                ? (_liveValues.workers * 100 / _liveValues.workforce) : 0;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", pct);
            return buf;
        }
        case StatCategory::ACCOMM:
            return formatWithSpaces(_liveValues.accommodation);
        case StatCategory::SICK:
            return formatWithSpaces(_liveValues.sick);
        case StatCategory::POWER:
            return formatWithSpaces(_liveValues.power) + " W";
        default:
            return "0";
    }
}

void StatsManager::clearHistory() {
    for (auto& h : _hourly) h.clear();
    for (auto& a : _hourAccum) { a.income = 0.f; a.expense = 0.f; }
}

void StatsManager::commitHourlySamples() {
    for (size_t c = 0; c < _hourly.size(); ++c) {
        _hourly[c].push_back(_hourAccum[c]);
        if (_hourly[c].size() > ST_MAX_HOURLY)
            _hourly[c].erase(_hourly[c].begin(),
                             _hourly[c].begin() + (_hourly[c].size() - ST_MAX_HOURLY));
        _hourAccum[c].income  = 0.f;
        _hourAccum[c].expense = 0.f;
    }
}

void StatsManager::onGameHour() {
    commitHourlySamples();
}

void StatsManager::saveGraphs(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return;

    f << "1\n";
    f << static_cast<int>(StatCategory::COUNT) << "\n";

    for (size_t c = 0; c < _hourly.size(); ++c) {
        f << _hourly[c].size() << "\n";
        for (const auto& s : _hourly[c])
            f << s.income << " " << s.expense << "\n";
    }

    f << "accum\n";
    for (size_t c = 0; c < _hourAccum.size(); ++c)
        f << _hourAccum[c].income << " " << _hourAccum[c].expense << "\n";
}

void StatsManager::loadGraphs(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    clearHistory();

    int version = 0;
    int numCats = 0;
    if (!(f >> version >> numCats)) return;
    if (numCats != (int)StatCategory::COUNT) return;

    for (size_t c = 0; c < _hourly.size(); ++c) {
        size_t count = 0;
        if (!(f >> count)) return;
        _hourly[c].reserve(count);
        for (size_t i = 0; i < count; ++i) {
            StatSample s;
            if (!(f >> s.income >> s.expense)) return;
            _hourly[c].push_back(s);
        }
    }

    std::string tag;
    if (f >> tag && tag == "accum") {
        for (size_t c = 0; c < _hourAccum.size(); ++c)
            f >> _hourAccum[c].income >> _hourAccum[c].expense;
    }
}

size_t StatsManager::chartSlotCount() const {
    switch (_chartScale) {
        case ChartScale::DAILY:   return 24;
        case ChartScale::MONTHLY: return 30;
        case ChartScale::YEARLY:  return 12;
    }
    return 24;
}

std::vector<StatSample> StatsManager::buildChartSeries(StatCategory cat) const {
    const auto& src = _hourly[(size_t)cat];
    std::vector<StatSample> out;

    auto aggregate = [&](size_t bucketSize, size_t numBuckets) {
        if (src.empty()) return;
        size_t total = src.size();
        size_t useCount = std::min(total, bucketSize * numBuckets);
        size_t start = total - useCount;

        for (size_t b = 0; b < numBuckets; ++b) {
            size_t bStart = start + b * bucketSize;
            size_t bEnd   = std::min(start + (b + 1) * bucketSize, total);
            if (bStart >= bEnd) break;

            StatSample bucket{};
            for (size_t i = bStart; i < bEnd; ++i) {
                bucket.income  += src[i].income;
                bucket.expense += src[i].expense;
            }
            out.push_back(bucket);
        }
    };

    switch (_chartScale) {
        case ChartScale::DAILY: {
            size_t n = std::min<size_t>(24, src.size());
            for (size_t i = src.size() - n; i < src.size(); ++i)
                out.push_back(src[i]);
            break;
        }
        case ChartScale::MONTHLY:
            aggregate(24, 30);
            break;
        case ChartScale::YEARLY:
            aggregate(730, 12);
            break;
    }

    out.push_back(_hourAccum[(size_t)cat]);

    size_t slots = chartSlotCount();
    if (out.size() > slots)
        out.erase(out.begin(), out.begin() + (out.size() - slots));
    else if (out.size() < slots)
        out.insert(out.begin(), slots - out.size(), StatSample{});

    return out;
}

bool StatsManager::containsPoint(sf::Vector2f p) const {
    if (!_open) return false;
    return sf::FloatRect({_winPos.x, _winPos.y}, {ST_WIN_W, ST_WIN_H}).contains(p);
}

void StatsManager::open(StatCategory cat, sf::Vector2u screenSize) {
    _open = true;
    _currentCat = cat;
    _confirmingLoanIdx = -1;
    _confirmingImmigrationIdx = -1;
    _switchAnim = 1.f;
    _winPos = {
        (static_cast<float>(screenSize.x) - ST_WIN_W) / 2.f,
        (static_cast<float>(screenSize.y) - ST_WIN_H) / 2.f
    };
    _closeBtn.setPosition({_winPos.x + ST_WIN_W - _closeBtnSize - 8.f, _winPos.y + 6.f});
}

void StatsManager::switchCategory(StatCategory cat) {
    if (cat != _currentCat) {
        _switchAnim = 0.f;
        _currentCat = cat;
    }
    _confirmingLoanIdx = -1;
    _confirmingImmigrationIdx = -1;
}

void StatsManager::update(sf::Time dt) {
    if (!_open) return;
    if (_switchAnim < 1.f) {
        _switchAnim = std::min(1.f, _switchAnim + dt.asSeconds() / ST_SWITCH_ANIM_S);
    }
}

bool StatsManager::consumeLoanRequest(long long& outAmount, float& outAnnualInterest) {
    if (!_loanRequested) return false;
    outAmount         = _pendingLoanAmount;
    outAnnualInterest = _pendingLoanInterest;
    _loanRequested = false;
    _pendingLoanAmount = 0;
    _pendingLoanInterest = 0.f;
    return true;
}

bool StatsManager::consumeImmigrationRequest(int& outCitizens, long long& outCost) {
    if (!_immigrationRequested) return false;
    outCitizens = _pendingImmigrationCitizens;
    outCost     = _pendingImmigrationCost;
    _immigrationRequested = false;
    _pendingImmigrationCitizens = 0;
    _pendingImmigrationCost = 0;
    return true;
}

bool StatsManager::hasActionSection() const {
    return _currentCat == StatCategory::CASH || _currentCat == StatCategory::POPS;
}

void StatsManager::addHit(float x, float y, float w, float h, const std::string& id) {
    _hitBoxes.push_back({sf::FloatRect({x, y}, {w, h}), id});
}

const StatsManager::HitBox* StatsManager::hitAt(sf::Vector2f p) const {
    for (const auto& h : _hitBoxes) if (h.rect.contains(p)) return &h;
    return nullptr;
}

float StatsManager::chartPlotHeight() const {
    float used = ST_TITLEBAR_H + ST_DESC_H + ST_PAD + ST_CHART_CTRL_H + ST_PAD;
    if (hasActionSection()) used += ST_LOAN_H + ST_PAD;
    return ST_WIN_H - used - ST_PAD;
}

float StatsManager::actionSectionY() const {
    return _winPos.y + ST_TITLEBAR_H + ST_DESC_H + ST_PAD
         + chartPlotHeight() + ST_CHART_CTRL_H + ST_PAD;
}

bool StatsManager::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (!_open) return false;

    sf::Vector2f mp = window.mapPixelToCoords(
        sf::Mouse::getPosition(window), window.getDefaultView());

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            close();
            return true;
        }
        return false;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;

        if (_closeBtn.getGlobalBounds().contains(mp)) {
            close();
            return true;
        }

        if (!containsPoint(mp)) return false;

        const HitBox* h = hitAt(mp);
        if (!h) return true;

        const std::string& id = h->id;

        if (id == "scale_daily")   { _chartScale = ChartScale::DAILY;   return true; }
        if (id == "scale_monthly") { _chartScale = ChartScale::MONTHLY; return true; }
        if (id == "scale_yearly")  { _chartScale = ChartScale::YEARLY;  return true; }

        if (id.rfind("loan_", 0) == 0) {
            int idx = std::atoi(id.c_str() + 5);
            const auto& opts = getLoanOptions();
            if (idx >= 0 && idx < (int)opts.size()) {
                if (_confirmingLoanIdx == idx) {
                    _pendingLoanAmount   = opts[idx].amount;
                    _pendingLoanInterest = opts[idx].annualInterest;
                    _loanRequested       = true;
                    _confirmingLoanIdx   = -1;
                } else {
                    _confirmingLoanIdx = idx;
                }
            }
            return true;
        }

        if (id.rfind("immigr_", 0) == 0) {
            int idx = std::atoi(id.c_str() + 7);
            const auto& opts = getImmigrationOptions();
            if (idx >= 0 && idx < (int)opts.size()) {
                if (_confirmingImmigrationIdx == idx) {
                    _pendingImmigrationCitizens = opts[idx].citizens;
                    _pendingImmigrationCost       = opts[idx].cost;
                    _immigrationRequested         = true;
                    _confirmingImmigrationIdx     = -1;
                } else {
                    _confirmingImmigrationIdx = idx;
                }
            }
            return true;
        }
        return true;
    }

    return false;
}

void StatsManager::draw(sf::RenderWindow& w) {
    if (!_open) return;
    _hitBoxes.clear();
    _mousePos = w.mapPixelToCoords(sf::Mouse::getPosition(w), w.getDefaultView());

    drawWindow(w);
    drawDescription(w);
    drawCharts(w);
    if (_currentCat == StatCategory::CASH) drawLoan(w);
    if (_currentCat == StatCategory::POPS) drawImmigration(w);
    w.draw(_closeBtn);
}

void StatsManager::drawWindow(sf::RenderWindow& w) {
    sf::RectangleShape bg({ST_WIN_W, ST_WIN_H});
    bg.setPosition({_winPos.x, _winPos.y});
    bg.setFillColor(COL_BG_PANEL);
    bg.setOutlineColor(COL_BORDER);
    bg.setOutlineThickness(3.f);
    w.draw(bg);

    sf::RectangleShape tb({ST_WIN_W, ST_TITLEBAR_H});
    tb.setPosition({_winPos.x, _winPos.y});
    tb.setFillColor(COL_TITLE_BAR);
    w.draw(tb);

    std::string title = std::string("STATS - ") + categoryToTitle(_currentCat);
    sf::Text t(_font, title, ST_TITLE_FS);
    t.setFillColor(sf::Color::White);
    t.setStyle(sf::Text::Style::Bold);
    t.setPosition({_winPos.x + 12.f, _winPos.y + 5.f});
    w.draw(t);
}

void StatsManager::drawDescription(sf::RenderWindow& w) {
    float x = _winPos.x + ST_PAD;
    float y = _winPos.y + ST_TITLEBAR_H + 10.f;
    float boxW = ST_WIN_W - ST_PAD * 2.f;
    float boxH = ST_DESC_H;

    sf::RectangleShape bg({boxW, boxH});
    bg.setPosition({x, y});
    bg.setFillColor(sf::Color(20, 20, 20));
    bg.setOutlineColor(sf::Color(80, 80, 80));
    bg.setOutlineThickness(1.f);
    w.draw(bg);

    auto it = _descriptions.find(_currentCat);
    std::string desc = (it != _descriptions.end()) ? it->second : "(no description)";

    float padX = 10.f;
    float padY = 10.f;

    sf::Text test(_font, "", ST_FONT);
    std::vector<std::string> outLines;
    std::stringstream ss(desc);
    std::string srcLine;
    while (std::getline(ss, srcLine)) {
        if (!srcLine.empty() && srcLine.back() == '\r') srcLine.pop_back();
        if (srcLine.empty()) { outLines.push_back(""); continue; }

        std::stringstream ws(srcLine);
        std::string word, current;
        while (ws >> word) {
            std::string trial = current.empty() ? word : current + " " + word;
            test.setString(trial);
            if (test.getLocalBounds().size.x > boxW - padX * 2.f) {
                if (!current.empty()) outLines.push_back(current);
                current = word;
            } else {
                current = trial;
            }
        }
        if (!current.empty()) outLines.push_back(current);
    }

    float lineH = static_cast<float>(ST_FONT) + 7.f;
    int maxLines = static_cast<int>((boxH - padY * 2.f) / lineH);
    int linesToDraw = std::min<int>(static_cast<int>(outLines.size()), maxLines);

    for (int i = 0; i < linesToDraw; ++i) {
        sf::Text line(_font, outLines[i], ST_FONT);
        line.setFillColor(i == 0 ? COL_SECTION : COL_LABEL);
        if (i == 0) line.setStyle(sf::Text::Style::Bold);
        line.setPosition({x + padX, y + padY + i * lineH});
        w.draw(line);
    }
}

void StatsManager::drawScaleButtons(sf::RenderWindow& window, float x, float y, float width) {
    const char* ids[]   = { "scale_daily", "scale_monthly", "scale_yearly" };
    ChartScale scales[] = { ChartScale::DAILY, ChartScale::MONTHLY, ChartScale::YEARLY };
    float gap = 12.f;
    float btnW = (width - gap * 2.f) / 3.f;
    float btnH = ST_CHART_CTRL_H - 4.f;

    for (int i = 0; i < 3; ++i) {
        float bx = x + i * (btnW + gap);
        sf::FloatRect rect({bx, y}, {btnW, btnH});
        bool active = (_chartScale == scales[i]);
        bool hover  = !active && rect.contains(_mousePos);

        sf::RectangleShape b({btnW, btnH});
        b.setPosition({bx, y});
        b.setFillColor(active ? COL_BTN_ACTIVE : hoverIf(hover, COL_BTN, COL_BTN_HOVER));
        b.setOutlineColor(active ? sf::Color::White : sf::Color(40, 40, 40));
        b.setOutlineThickness(active ? 2.f : 1.f);
        window.draw(b);

        sf::Text t(_font, scaleLabel(scales[i]), ST_FONT);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Style::Bold);
        sf::FloatRect tb = t.getLocalBounds();
        t.setOrigin({tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f});
        t.setPosition({bx + btnW / 2.f, y + btnH / 2.f - 1.f});
        window.draw(t);

        addHit(bx, y, btnW, btnH, ids[i]);
    }
}

void StatsManager::drawChartAxes(sf::RenderWindow& w,
                                 float plotX0, float plotY0, float plotW, float plotH,
                                 float axisMax)
{
    float baseY = plotY0 + plotH;

    sf::RectangleShape yAxis({1.f, plotH});
    yAxis.setPosition({plotX0 - 1.f, plotY0});
    yAxis.setFillColor(COL_CHART_AXIS);
    w.draw(yAxis);

    sf::RectangleShape xAxis({plotW + 1.f, 1.f});
    xAxis.setPosition({plotX0 - 1.f, baseY});
    xAxis.setFillColor(COL_CHART_AXIS);
    w.draw(xAxis);

    for (int t = 0; t <= ST_Y_TICKS; ++t) {
        float frac = static_cast<float>(t) / ST_Y_TICKS;
        float gy   = baseY - plotH * frac;

        sf::RectangleShape grid({plotW, 1.f});
        grid.setPosition({plotX0, gy});
        grid.setFillColor(t == 0 ? COL_CHART_AXIS : COL_CHART_GRID);
        w.draw(grid);

        if (t > 0) {
            float val = axisMax * frac;
            sf::Text lbl(_font, formatAxisValue(val), ST_FONT - 2);
            lbl.setFillColor(COL_HINT);
            sf::FloatRect lb = lbl.getLocalBounds();
            lbl.setOrigin({lb.position.x + lb.size.x, lb.position.y + lb.size.y * 0.5f});
            lbl.setPosition({plotX0 - 6.f, gy});
            w.draw(lbl);
        }
    }

    sf::Text zeroLbl(_font, "0", ST_FONT - 2);
    zeroLbl.setFillColor(COL_HINT);
    sf::FloatRect zb = zeroLbl.getLocalBounds();
    zeroLbl.setOrigin({zb.position.x + zb.size.x, zb.position.y});
    zeroLbl.setPosition({plotX0 - 6.f, baseY - 2.f});
    w.draw(zeroLbl);

    const char* xLabels[] = { "-24h", "-18h", "-12h", "-6h", "now" };
    const char* xLabelsM[] = { "-30d", "-24d", "-18d", "-12d", "-6d", "now" };
    const char* xLabelsY[] = { "-12m", "-10m", "-8m", "-6m", "-4m", "-2m", "now" };
    const char** labels = xLabels;
    int labelCount = 5;
    if (_chartScale == ChartScale::MONTHLY) { labels = xLabelsM; labelCount = 6; }
    if (_chartScale == ChartScale::YEARLY)  { labels = xLabelsY; labelCount = 7; }

    for (int i = 0; i < labelCount; ++i) {
        float frac = static_cast<float>(i) / static_cast<float>(labelCount - 1);
        float lx   = plotX0 + plotW * frac;
        sf::Text xl(_font, labels[i], ST_FONT - 2);
        xl.setFillColor(COL_HINT);
        sf::FloatRect xb = xl.getLocalBounds();
        xl.setOrigin({xb.position.x + xb.size.x * 0.5f, xb.position.y});
        xl.setPosition({lx, baseY + 5.f});
        w.draw(xl);
    }
}

void StatsManager::drawStepLine(sf::RenderWindow& w,
                                float plotX0, float plotY0, float plotW, float plotH,
                                const std::vector<float>& values, float axisMax,
                                sf::Color color)
{
    if (values.empty() || plotW <= 0.f || plotH <= 0.f || axisMax <= 0.f) return;

    size_t nPts = values.size();
    if (nPts < 1) return;

    float baseY = plotY0 + plotH;
    float scale = plotH / axisMax;
    float pitch = (nPts > 1) ? plotW / static_cast<float>(nPts - 1) : 0.f;

    sf::VertexArray line(sf::PrimitiveType::LineStrip);
    float y0 = baseY - values[0] * scale;
    line.append(sf::Vertex{{plotX0, y0}, color});

    for (size_t i = 0; i + 1 < nPts; ++i) {
        float xNext = plotX0 + static_cast<float>(i + 1) * pitch;
        float yCur  = baseY - values[i] * scale;
        float yNext = baseY - values[i + 1] * scale;
        line.append(sf::Vertex{{xNext, yCur}, color});
        line.append(sf::Vertex{{xNext, yNext}, color});
    }

    if (line.getVertexCount() >= 2)
        w.draw(line);
}

void StatsManager::drawCharts(sf::RenderWindow& w) {
    float x  = _winPos.x + ST_PAD;
    float y  = _winPos.y + ST_TITLEBAR_H + ST_DESC_H + ST_PAD;
    float cw = ST_WIN_W - ST_PAD * 2.f;
    float ch = chartPlotHeight();

    sf::RectangleShape bg({cw, ch + ST_CHART_CTRL_H + 8.f});
    bg.setPosition({x, y});
    bg.setFillColor(COL_CHART_BG);
    bg.setOutlineColor(sf::Color(80, 80, 80));
    bg.setOutlineThickness(1.f);
    w.draw(bg);

    const char* scaleNames[] = { "last 24 hours", "last 30 days", "last 12 months" };
    int scaleIdx = (_chartScale == ChartScale::DAILY) ? 0
                 : (_chartScale == ChartScale::MONTHLY) ? 1 : 2;
    char titleBuf[96];
    std::snprintf(titleBuf, sizeof(titleBuf), "Activity (%s)", scaleNames[scaleIdx]);

    sf::Text title(_font, titleBuf, ST_FONT);
    title.setFillColor(COL_HINT);
    title.setPosition({x + 10.f, y + 8.f});
    w.draw(title);

    const char* incLeg = incomeLegend(_currentCat);
    const char* expLeg = expenseLegend(_currentCat);

    float legendY = y + 30.f;
    float legendX = x + 10.f;
    sf::RectangleShape gSwatch({14.f, 14.f});
    gSwatch.setPosition({legendX, legendY + 2.f});
    gSwatch.setFillColor(COL_LINE_GREEN);
    w.draw(gSwatch);
    sf::Text gT(_font, incLeg, ST_FONT);
    gT.setFillColor(COL_LABEL);
    gT.setPosition({legendX + 18.f, legendY});
    w.draw(gT);

    float expX = legendX + 18.f + gT.getLocalBounds().size.x + 24.f;
    sf::RectangleShape rSwatch({14.f, 14.f});
    rSwatch.setPosition({expX, legendY + 2.f});
    rSwatch.setFillColor(COL_LINE_RED);
    w.draw(rSwatch);
    sf::Text rT(_font, expLeg, ST_FONT);
    rT.setFillColor(COL_LABEL);
    rT.setPosition({expX + 18.f, legendY});
    w.draw(rT);

    float plotX0 = x + ST_AXIS_LEFT;
    float plotY0 = y + ST_CHART_HEADER;
    float plotX1 = x + cw - 12.f;
    float plotY1 = y + ch - ST_AXIS_BOTTOM - 10.f;
    float plotW  = plotX1 - plotX0;
    float plotH  = plotY1 - plotY0;

    std::string curVal = formatCurrentValue(_currentCat);
    sf::Text curT(_font, curVal, ST_FONT + 3);
    curT.setFillColor(sf::Color::White);
    curT.setStyle(sf::Text::Style::Bold);
    sf::FloatRect curB = curT.getLocalBounds();
    curT.setOrigin({curB.position.x + curB.size.x, curB.position.y + curB.size.y * 0.5f});
    curT.setPosition({plotX1 - 4.f, y + 20.f});
    w.draw(curT);

    auto series = buildChartSeries(_currentCat);
    std::vector<float> incomes, expenses;
    incomes.reserve(series.size());
    expenses.reserve(series.size());
    float dataMax = 0.f;
    for (const auto& s : series) {
        incomes.push_back(s.income);
        expenses.push_back(s.expense);
        dataMax = std::max(dataMax, std::max(s.income, s.expense));
    }
    float axisMax = niceAxisMax(dataMax);

    drawChartAxes(w, plotX0, plotY0, plotW, plotH, axisMax);
    drawStepLine(w, plotX0, plotY0, plotW, plotH, incomes, axisMax, COL_LINE_GREEN);
    drawStepLine(w, plotX0, plotY0, plotW, plotH, expenses, axisMax, COL_LINE_RED);

    float ctrlY = plotY1 + ST_AXIS_BOTTOM + 6.f;
    drawScaleButtons(w, x + ST_AXIS_LEFT, ctrlY, plotW);
}

void StatsManager::drawLoan(sf::RenderWindow& w) {
    float x  = _winPos.x + ST_PAD;
    float y  = actionSectionY();
    float pw = ST_WIN_W - ST_PAD * 2.f;
    float ph = ST_LOAN_H;

    sf::RectangleShape bg({pw, ph});
    bg.setPosition({x, y});
    bg.setFillColor(sf::Color(20, 20, 20));
    bg.setOutlineColor(sf::Color(80, 80, 80));
    bg.setOutlineThickness(1.f);
    w.draw(bg);

    sf::Text title(_font, "--- LOANS ---  (bigger loan = lower interest rate)", ST_FONT + 1);
    title.setFillColor(COL_SECTION);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({x + 12.f, y + 8.f});
    w.draw(title);

    const auto& opts = getLoanOptions();
    int n = (int)opts.size();
    float gap  = 14.f;
    float btnW = (pw - 24.f - (n - 1) * gap) / n;
    float btnH = 68.f;
    float btnY = y + 34.f;

    for (int i = 0; i < n; ++i) {
        float bx = x + 12.f + i * (btnW + gap);
        sf::FloatRect rect({bx, btnY}, {btnW, btnH});
        bool hover = rect.contains(_mousePos);
        bool selected = (_confirmingLoanIdx == i);

        sf::Color base = selected ? COL_BTN_GREEN : COL_BTN;
        sf::Color hov  = selected ? COL_BTN_GREEN_H : COL_BTN_HOVER;

        sf::RectangleShape b({btnW, btnH});
        b.setPosition({bx, btnY});
        b.setFillColor(hoverIf(hover, base, hov));
        b.setOutlineColor(selected ? sf::Color::White : sf::Color(30, 30, 30));
        b.setOutlineThickness(selected ? 2.f : 1.f);
        w.draw(b);

        char l1[64], l2[64], l3[64];
        std::snprintf(l1, sizeof(l1), "$%s", formatMoney(opts[i].amount).c_str());
        std::snprintf(l2, sizeof(l2), "%.0f%% / year", opts[i].annualInterest * 100.f);
        long long total = (long long)(opts[i].amount * (1.0 + opts[i].annualInterest));
        long long monthly = (total + 11) / 12;
        std::snprintf(l3, sizeof(l3), "Monthly: $%s", formatMoney(monthly).c_str());

        auto drawCentered = [&](const char* txt, float cy, unsigned int fs, sf::Color col, bool bold) {
            sf::Text t(_font, txt, fs);
            t.setFillColor(col);
            if (bold) t.setStyle(sf::Text::Style::Bold);
            sf::FloatRect bnd = t.getLocalBounds();
            t.setOrigin({bnd.position.x + bnd.size.x / 2.f, bnd.position.y + bnd.size.y / 2.f});
            t.setPosition({bx + btnW / 2.f, cy});
            w.draw(t);
        };

        drawCentered(l1, btnY + 16.f, ST_FONT + 5, sf::Color::White, true);
        drawCentered(l2, btnY + 38.f, ST_FONT, sf::Color::White, true);
        drawCentered(l3, btnY + 56.f, ST_FONT - 1, sf::Color(220, 220, 220), false);

        addHit(bx, btnY, btnW, btnH, std::string("loan_") + std::to_string(i));
    }

    sf::Text hint(_font,
        _confirmingLoanIdx >= 0
            ? "Click the same option again to CONFIRM the loan."
            : "Select a loan. Repaid automatically every month.",
        ST_FONT - 1);
    hint.setFillColor(COL_HINT);
    hint.setPosition({x + 12.f, btnY + btnH + 6.f});
    w.draw(hint);
}

void StatsManager::drawImmigration(sf::RenderWindow& w) {
    float x  = _winPos.x + ST_PAD;
    float y  = actionSectionY();
    float pw = ST_WIN_W - ST_PAD * 2.f;
    float ph = ST_LOAN_H;

    sf::RectangleShape bg({pw, ph});
    bg.setPosition({x, y});
    bg.setFillColor(sf::Color(20, 20, 20));
    bg.setOutlineColor(sf::Color(80, 80, 80));
    bg.setOutlineThickness(1.f);
    w.draw(bg);

    sf::Text title(_font, "--- IMMIGRATION ---  ($1 per citizen, linear cost)", ST_FONT + 1);
    title.setFillColor(COL_SECTION);
    title.setStyle(sf::Text::Style::Bold);
    title.setPosition({x + 12.f, y + 8.f});
    w.draw(title);

    const auto& opts = getImmigrationOptions();
    int n = (int)opts.size();
    float gap  = 18.f;
    float btnW = (pw - 24.f - (n - 1) * gap) / n;
    float btnH = 68.f;
    float btnY = y + 34.f;

    for (int i = 0; i < n; ++i) {
        float bx = x + 12.f + i * (btnW + gap);
        sf::FloatRect rect({bx, btnY}, {btnW, btnH});
        bool hover = rect.contains(_mousePos);
        bool selected = (_confirmingImmigrationIdx == i);
        bool canAfford = _liveValues.cash >= opts[i].cost;

        sf::Color base = selected ? COL_BTN_GREEN : COL_BTN;
        sf::Color hov  = selected ? COL_BTN_GREEN_H : COL_BTN_HOVER;
        if (!canAfford && !selected) base = sf::Color(45, 45, 45);

        sf::RectangleShape b({btnW, btnH});
        b.setPosition({bx, btnY});
        b.setFillColor(hoverIf(hover && canAfford, base, hov));
        b.setOutlineColor(selected ? sf::Color::White : sf::Color(30, 30, 30));
        b.setOutlineThickness(selected ? 2.f : 1.f);
        w.draw(b);

        char l1[64], l2[64], l3[64];
        std::snprintf(l1, sizeof(l1), "+%d citizens", opts[i].citizens);
        std::snprintf(l2, sizeof(l2), "Cost: $%s", formatMoney(opts[i].cost).c_str());
        std::snprintf(l3, sizeof(l3), "From abroad");

        auto drawCentered = [&](const char* txt, float cy, unsigned int fs, sf::Color col, bool bold) {
            sf::Text t(_font, txt, fs);
            t.setFillColor(col);
            if (bold) t.setStyle(sf::Text::Style::Bold);
            sf::FloatRect bnd = t.getLocalBounds();
            t.setOrigin({bnd.position.x + bnd.size.x / 2.f, bnd.position.y + bnd.size.y / 2.f});
            t.setPosition({bx + btnW / 2.f, cy});
            w.draw(t);
        };

        sf::Color txtCol = canAfford ? sf::Color::White : sf::Color(120, 120, 120);
        drawCentered(l1, btnY + 16.f, ST_FONT + 5, txtCol, true);
        drawCentered(l2, btnY + 38.f, ST_FONT, txtCol, true);
        drawCentered(l3, btnY + 56.f, ST_FONT - 1, sf::Color(220, 220, 220), false);

        if (canAfford)
            addHit(bx, btnY, btnW, btnH, std::string("immigr_") + std::to_string(i));
    }

    sf::Text hint(_font,
        _confirmingImmigrationIdx >= 0
            ? "Click the same option again to CONFIRM immigration."
            : "Import citizens. Unhoused / unemployed affect Accomm and Workers.",
        ST_FONT - 1);
    hint.setFillColor(COL_HINT);
    hint.setPosition({x + 12.f, btnY + btnH + 6.f});
    w.draw(hint);
}
