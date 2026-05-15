#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <random>
#include <functional>

enum class TileType {
    EMPTY,
    BARE,
    BARE_WATER,
    WATER,
    COAL,
    IRON,
    TREE
};

enum class MapSize    { SMALL, MEDIUM, LARGE, VERY_LARGE };
enum class RiverCount { ONE, TWO, THREE };
enum class OreFreq    { RARE, COMMON, FREQUENT };
enum class Difficulty { EASY, NORMAL, HARD };
enum class ShapeRandomness { LOW, NORMAL, HIGH };

struct WorldSettings {
    MapSize         mapSize         = MapSize::MEDIUM;
    RiverCount      riverCount      = RiverCount::ONE;
    OreFreq         oreFreq         = OreFreq::COMMON;
    Difficulty      difficulty      = Difficulty::NORMAL;
    ShapeRandomness shapeRandomness = ShapeRandomness::NORMAL;
    unsigned int    seed            = 12345;

    int gridW() const {
        // TUTAJ ZMIENIASZ ROZMIAR MAPY (w kratkach)
        switch (mapSize) {
            case MapSize::SMALL:      return 150;
            case MapSize::MEDIUM:     return 300;
            case MapSize::LARGE:      return 400;
            case MapSize::VERY_LARGE: return 600;
        }
        return 100;
    }
    int gridH() const { return gridW(); }

    long long startCash() const {
        switch (difficulty) {
            case Difficulty::EASY:   return 2000000;
            case Difficulty::NORMAL: return 1000000;
            case Difficulty::HARD:   return 400000;
        }
        return 1000000;
    }

    int borderCrossings() const {
        switch (mapSize) {
            case MapSize::SMALL:      return 4;
            case MapSize::MEDIUM:     return 6;
            case MapSize::LARGE:      return 8;
            case MapSize::VERY_LARGE: return 10;
        }
        return 6;
    }
};

struct GeneratedWorld {
    int width  = 0;
    int height = 0;
    std::vector<std::vector<TileType>> tiles;
    std::vector<std::pair<int,int>> borderCrossings;
    WorldSettings settings;

    TileType get(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return TileType::EMPTY;
        return tiles[y][x];
    }

    bool isWalkable(int x, int y) const {
        TileType t = get(x, y);
        return t != TileType::EMPTY && t != TileType::WATER;
    }
};

class WorldGenerator {
public:
    GeneratedWorld generate(const WorldSettings& settings);

private:
    std::mt19937 _rng;

    void generateIslandShape(GeneratedWorld& w);
    void generateRivers(GeneratedWorld& w, int count);
    void generateOre(GeneratedWorld& w, OreFreq freq, TileType oreType);
    void generateTrees(GeneratedWorld& w);
    void placeBorderCrossings(GeneratedWorld& w, int count);

    bool inEllipse(int x, int y, int cx, int cy, float rx, float ry, float noise) const;
};

