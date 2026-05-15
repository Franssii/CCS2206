#pragma once

#include <vector>
#include <string>
#include <SFML/Graphics.hpp>
#include <cstdlib>

struct GameTime {
    int min = 1;
    int hour = 12;
    int day = 26;
    int month = 4;
    int year = 1967;
    int xt2 = 0;
    int xt3 = 0;

    const int framesPerMinute = 60;

    bool isPaused = false;
    int timeSpeed = 1;
    int timeMultiplier = 1;

    int temperature = 10;

    void updateTemperature() {
        int targetT = 0;
        switch (month) {
            case 1:  targetT = -22; break; // Styczeń
            case 2:  targetT = -15; break; // Luty
            case 3:  targetT = -2;  break; // Marzec
            case 4:  targetT = 8;   break; // Kwiecień
            case 5:  targetT = 16;  break; // Maj
            case 6:  targetT = 22;  break; // Czerwiec
            case 7:  targetT = 26;  break; // Lipiec
            case 8:  targetT = 22;  break; // Sierpień
            case 9:  targetT = 12;  break; // Wrzesień
            case 10: targetT = 4;   break; // Październik
            case 11: targetT = -5;  break; // Listopad
            case 12: targetT = -15; break; // Grudzień
        }
        targetT += (std::rand() % 9) - 4; // Losowe odchylenie +/- 4 stopnie

        int diff = targetT - temperature;
        if (diff > 0) {
            temperature += (std::rand() % 3) + 1; // +1 do +3 stopni na dzień
            if (temperature > targetT) temperature = targetT;
        } else if (diff < 0) {
            temperature -= (std::rand() % 3) + 1; // -1 do -3 stopni na dzień
            if (temperature < targetT) temperature = targetT;
        }
    }

    void cycleSpeed() {
        if (timeSpeed == 1) { timeSpeed = 2; timeMultiplier = 2; }
        else if (timeSpeed == 2) { timeSpeed = 3; timeMultiplier = 8; }
        else { timeSpeed = 1; timeMultiplier = 1; }
    }

    void update() {
        xt2 = 0;
        xt3 = 0;
        if (isPaused) return;

        min += timeMultiplier;
        if (min >= framesPerMinute) {
            min -= framesPerMinute;
            hour++;
            xt2 = 1;
        }
        if (hour >= 24) {
            hour = 0; day++; xt3 = 1;
        }
        if (day > 30)  { day = 1; month++; }
        if (month > 12){ month = 1; year++; }

        if (xt3 == 1) updateTemperature();
    }

    void setTime(int m, int h, int d, int mo, int y) {
        min = m; hour = h; day = d; month = mo; year = y;
        isPaused = false;
        timeSpeed = 1;
        timeMultiplier = 1;

        switch (month) {
            case 1:  temperature = -22; break;
            case 2:  temperature = -15; break;
            case 3:  temperature = -2;  break;
            case 4:  temperature = 8;   break;
            case 5:  temperature = 16;  break;
            case 6:  temperature = 22;  break;
            case 7:  temperature = 26;  break;
            case 8:  temperature = 22;  break;
            case 9:  temperature = 12;  break;
            case 10: temperature = 4;   break;
            case 11: temperature = -5;  break;
            case 12: temperature = -15; break;
            default: temperature = 10;  break;
        }
    }

    int getMin()   const { return min; }
    int getHour()  const { return hour; }
    int getDay()   const { return day; }
    int getMonth() const { return month; }
    int getYear()  const { return year; }
    int getXt2()   const { return xt2; }
    int getXt3()   const { return xt3; }

    std::string getDateString() const {
        return std::to_string(hour) + ":00 " +
               std::to_string(day) + "/" +
               std::to_string(month) + "/" +
               std::to_string(year);
    }
};

class GameManager {
public:
    GameManager();

    void save(const std::vector<sf::Sprite>& block_1s);
    void load(std::vector<sf::Sprite>& block_1s, const sf::Texture& block_1_texture);
    void restart(std::vector<sf::Sprite>& block_1s);
    static bool fileExists(const std::string& filename);

    bool game_start = true;
    bool delete_save_warning = false;
    bool quitting_warn = false;
    bool failed_toload = false;
    bool ingame_settings = false;

    int population = 0;
    long long cash = 1000000;
    int loyalty = 75;
    int workers = 0;
    int unemployment = 0;
    int sick = 0;
    int power = 0;

    int steel = 0;
    int concrete = 0;

    int option_marked_position = 0;
    int option_marked_position_2 = 0;
    int xt1 = 0;

    GameTime clock;
};
