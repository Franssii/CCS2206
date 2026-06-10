#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>

constexpr float ST_WIN_W           = 964.f;
constexpr float ST_WIN_H           = 675.f;
constexpr float ST_TITLEBAR_H      = 36.f;
constexpr float ST_DESC_H          = 140.f;
constexpr float ST_CHART_H         = 360.f;
constexpr float ST_CHART_CTRL_H    = 34.f;
constexpr float ST_LOAN_H          = 145.f;
constexpr float ST_PAD             = 18.f;
constexpr unsigned int ST_FONT     = 15;
constexpr unsigned int ST_TITLE_FS   = 24;

constexpr size_t ST_MAX_HOURLY     = 8760;

enum class StatCategory {
    CASH = 0,
    POPS,
    LOYALTY,
    WORKERS,
    ACCOMM,
    SICK,
    POWER,
    COUNT
};

struct LiveStatValues {
    long long cash         = 0;
    int       population   = 0;
    int       loyalty      = 0;
    int       workers      = 0;
    int       workforce    = 0;
    int       accommodation = 0;
    int       sick         = 0;
    int       power        = 0;
    float     hourProgress = 0.f;
};

enum class ChartScale {
    DAILY,
    MONTHLY,
    YEARLY
};

struct StatSample {
    float income  = 0.f;
    float expense = 0.f;
};

struct LoanOption {
    long long amount;
    float     annualInterest;
};

struct ImmigrationOption {
    int       citizens;
    long long cost;
};

class StatsManager {
public:
    StatsManager(const sf::Font& font, const sf::Texture& closeTexture);

    void loadDescriptions(const std::string& langDir);

    void recordIncome (StatCategory cat, float amount);
    void recordExpense(StatCategory cat, float amount);
    void setHourAccum(StatCategory cat, float income, float expense);
    void setLiveValues(const LiveStatValues& values);
    void clearHistory();

    void onGameHour();

    void saveGraphs(const std::string& path) const;
    void loadGraphs(const std::string& path);

    bool isOpen() const { return _open; }
    bool containsPoint(sf::Vector2f p) const;

    void open(StatCategory cat, sf::Vector2u screenSize);
    void switchCategory(StatCategory cat);
    void close() { _open = false; _confirmingLoanIdx = -1; _confirmingImmigrationIdx = -1; }

    bool handleEvent(const sf::Event& event, sf::RenderWindow& window);
    void update(sf::Time dt);
    void draw(sf::RenderWindow& window);

    bool consumeLoanRequest(long long& outAmount, float& outAnnualInterest);
    bool consumeImmigrationRequest(int& outCitizens, long long& outCost);

    StatCategory currentCategory() const { return _currentCat; }

    static const char* categoryToTitle(StatCategory c);
    static const char* categoryToKey(StatCategory c);
    static const char* incomeLegend(StatCategory c);
    static const char* expenseLegend(StatCategory c);

    static const std::vector<LoanOption>& getLoanOptions();
    static const std::vector<ImmigrationOption>& getImmigrationOptions();

private:
    std::array<std::vector<StatSample>, static_cast<size_t>(StatCategory::COUNT)> _hourly{};
    std::array<StatSample, static_cast<size_t>(StatCategory::COUNT)> _hourAccum{};

    std::map<StatCategory, std::string> _descriptions;

    bool _open = false;
    StatCategory _currentCat = StatCategory::CASH;
    ChartScale   _chartScale = ChartScale::DAILY;
    sf::Vector2f _winPos;

    int      _confirmingLoanIdx = -1;
    bool     _loanRequested = false;
    long long _pendingLoanAmount = 0;
    float    _pendingLoanInterest = 0.f;

    int      _confirmingImmigrationIdx = -1;
    bool     _immigrationRequested = false;
    int      _pendingImmigrationCitizens = 0;
    long long _pendingImmigrationCost = 0;

    const sf::Font&    _font;
    sf::Sprite         _closeBtn;
    float              _closeBtnSize = 24.f;

    sf::Vector2f _mousePos;
    LiveStatValues _liveValues;

    float _switchAnim = 0.f;

    struct HitBox {
        sf::FloatRect rect;
        std::string   id;
    };
    std::vector<HitBox> _hitBoxes;

    void addHit(float x, float y, float w, float h, const std::string& id);
    const HitBox* hitAt(sf::Vector2f p) const;

    void commitHourlySamples();
    std::vector<StatSample> buildChartSeries(StatCategory cat) const;

    void drawWindow(sf::RenderWindow& w);
    void drawDescription(sf::RenderWindow& w);
    void drawCharts(sf::RenderWindow& w);
    void drawChartAxes(sf::RenderWindow& w,
                       float plotX0, float plotY0, float plotW, float plotH,
                       float axisMax);
    void drawStepLine(sf::RenderWindow& w,
                      float plotX0, float plotY0, float plotW, float plotH,
                      const std::vector<float>& values, float axisMax,
                      sf::Color color);
    void drawScaleButtons(sf::RenderWindow& window, float x, float y, float width);
    size_t chartSlotCount() const;
    void drawLoan(sf::RenderWindow& w);
    void drawImmigration(sf::RenderWindow& w);

    float chartPlotHeight() const;
    float actionSectionY() const;
    bool hasActionSection() const;

    std::string formatCurrentValue(StatCategory cat) const;
    static std::string formatWithSpaces(long long num);
};
