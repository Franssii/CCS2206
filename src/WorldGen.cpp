#include "include/WorldGen.hpp"
#include <cmath>
#include <algorithm>

static float smoothstep(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

static float hash2(int x, int y, unsigned int seed) {
    unsigned int h = static_cast<unsigned int>(x * 1619 + y * 31337 + seed * 6971);
    h = (h ^ (h >> 16)) * 0x45d9f3b;
    h = (h ^ (h >> 16)) * 0x45d9f3b;
    h ^= (h >> 16);
    return static_cast<float>(h & 0xFFFF) / 65535.f;
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static float valueNoise(float x, float y, unsigned int seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;

    float sx = smoothstep(fx);
    float sy = smoothstep(fy);

    float n00 = hash2(ix, iy, seed);
    float n10 = hash2(ix + 1, iy, seed);
    float n01 = hash2(ix, iy + 1, seed);
    float n11 = hash2(ix + 1, iy + 1, seed);

    float nx0 = lerp(n00, n10, sx);
    float nx1 = lerp(n01, n11, sx);
    return lerp(nx0, nx1, sy);
}

static float fBm(float x, float y, int octaves, unsigned int seed) {
    float v = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        v += amplitude * valueNoise(x * frequency, y * frequency, seed + i);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return v;
}

bool WorldGenerator::inEllipse(int x, int y, int cx, int cy, float rx, float ry, float noise) const {
    float dx = (x - cx) / rx;
    float dy = (y - cy) / ry;
    float dist = std::sqrt(dx * dx + dy * dy);
    float n = noise * 0.25f;
    return dist < (1.f - n + n * 0.5f);
}

GeneratedWorld WorldGenerator::generate(const WorldSettings& settings) {
    _rng.seed(settings.seed);

    GeneratedWorld w;
    w.width    = settings.gridW();
    w.height   = settings.gridH();
    w.settings = settings;
    w.tiles.assign(w.height, std::vector<TileType>(w.width, TileType::EMPTY));

    generateIslandShape(w);

    int rivers = 0;
    switch (settings.riverCount) {
        case RiverCount::ONE:   rivers = 1; break;
        case RiverCount::TWO:   rivers = 2; break;
        case RiverCount::THREE: rivers = 3; break;
    }
    generateRivers(w, rivers);

    generateOre(w, settings.oreFreq, TileType::COAL);
    generateOre(w, settings.oreFreq, TileType::IRON);
    generateTrees(w);
    placeBorderCrossings(w, settings.borderCrossings());

    return w;
}

void WorldGenerator::generateIslandShape(GeneratedWorld& w) {
    int cx = w.width  / 2;
    int cy = w.height / 2;
    float rx = w.width  * 0.40f;
    float ry = w.height * 0.40f;

    float noiseMod = 0.5f;
    if (w.settings.shapeRandomness == ShapeRandomness::LOW) noiseMod = 0.2f;
    else if (w.settings.shapeRandomness == ShapeRandomness::HIGH) noiseMod = 1.0f;

    for (int y = 0; y < w.height; ++y) {
        for (int x = 0; x < w.width; ++x) {
            float angle = std::atan2(static_cast<float>(y - cy), static_cast<float>(x - cx));
            float noiseVal = fBm(std::cos(angle) * 4.0f + 10.0f, std::sin(angle) * 4.0f + 10.0f, 4, w.settings.seed);
            float radMod = 0.7f + noiseVal * noiseMod;

            float dx = (x - cx) / (rx * radMod);
            float dy = (y - cy) / (ry * radMod);

            if (dx * dx + dy * dy <= 1.0f) {
                w.tiles[y][x] = TileType::BARE;
            } else {
                w.tiles[y][x] = TileType::EMPTY;
            }
        }
    }

    int smoothPasses = 2;
    for (int pass = 0; pass < smoothPasses; ++pass) {
        std::vector<std::vector<TileType>> copy = w.tiles;
        for (int y = 0; y < w.height; ++y) {
            for (int x = 0; x < w.width; ++x) {
                int landNeighbors = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < w.width && ny >= 0 && ny < w.height) {
                            if (w.tiles[ny][nx] != TileType::EMPTY && w.tiles[ny][nx] != TileType::WATER) {
                                landNeighbors++;
                            }
                        }
                    }
                }
                
                if (landNeighbors > 4) {
                    copy[y][x] = TileType::BARE;
                } else if (landNeighbors < 4) {
                    copy[y][x] = TileType::EMPTY;
                }
            }
        }
        w.tiles = copy;
    }
}

void WorldGenerator::generateRivers(GeneratedWorld& w, int count) {
    std::uniform_int_distribution<int> sideDist(0, 3);
    std::uniform_int_distribution<int> widthDist(2, 4);
    std::uniform_int_distribution<int> turnsDist(3, 8);
    std::uniform_real_distribution<float> ampDist(10.f, 25.f);

    for (int r = 0; r < count; ++r) {
        int s1 = sideDist(_rng);
        int s2 = (s1 + 2) % 4;

        float ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;
        std::uniform_int_distribution<int> edgeX(1, w.width - 2);
        std::uniform_int_distribution<int> edgeY(1, w.height - 2);

        if (s1 == 0) { ax = static_cast<float>(edgeX(_rng)); ay = 0.f; }
        else if (s1 == 1) { ax = static_cast<float>(w.width - 1); ay = static_cast<float>(edgeY(_rng)); }
        else if (s1 == 2) { ax = static_cast<float>(edgeX(_rng)); ay = static_cast<float>(w.height - 1); }
        else { ax = 0.f; ay = static_cast<float>(edgeY(_rng)); }

        if (s2 == 0) { bx = static_cast<float>(edgeX(_rng)); by = 0.f; }
        else if (s2 == 1) { bx = static_cast<float>(w.width - 1); by = static_cast<float>(edgeY(_rng)); }
        else if (s2 == 2) { bx = static_cast<float>(edgeX(_rng)); by = static_cast<float>(w.height - 1); }
        else { bx = 0.f; by = static_cast<float>(edgeY(_rng)); }

        int rw = widthDist(_rng);
        int turns = turnsDist(_rng);
        float amp = ampDist(_rng);
        float radius = static_cast<float>(rw) / 2.0f;

        int steps = std::max(w.width, w.height) * 4;
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float base_x = ax + t * (bx - ax);
            float base_y = ay + t * (by - ay);

            float angle = std::atan2(by - ay, bx - ax);
            float perpX = -std::sin(angle);
            float perpY = std::cos(angle);

            float offset = std::sin(t * 3.14159f * static_cast<float>(turns)) * amp;
            float noise = (valueNoise(t * 15.0f, r * 15.0f, w.settings.seed) - 0.5f) * 10.0f;
            offset += noise;

            int cx = static_cast<int>(base_x + perpX * offset);
            int cy = static_cast<int>(base_y + perpY * offset);

            for (int dy = -rw; dy <= rw; ++dy) {
                for (int dx = -rw; dx <= rw; ++dx) {
                    if (static_cast<float>(dx * dx + dy * dy) <= radius * radius) {
                        int nx = cx + dx;
                        int ny = cy + dy;
                        if (nx >= 0 && ny >= 0 && nx < w.width && ny < w.height && w.tiles[ny][nx] != TileType::EMPTY) {
                            w.tiles[ny][nx] = TileType::WATER;
                        }
                    }
                }
            }
        }
    }

    for (int y = 0; y < w.height; ++y) {
        for (int x = 0; x < w.width; ++x) {
            if (w.tiles[y][x] == TileType::WATER) continue;
            if (w.tiles[y][x] == TileType::EMPTY)  continue;
            bool adjacentToWater = false;
            int dx[] = {-1,1,0,0};
            int dy[] = {0,0,-1,1};
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && ny >= 0 && nx < w.width && ny < w.height
                    && w.tiles[ny][nx] == TileType::WATER) {
                    adjacentToWater = true;
                    break;
                }
            }
            if (adjacentToWater) w.tiles[y][x] = TileType::BARE_WATER;
        }
    }
}

void WorldGenerator::generateOre(GeneratedWorld& w, OreFreq freq, TileType oreType) {
    int clusters = 0;
    switch (freq) {
        case OreFreq::RARE:     clusters = w.width / 20; break;
        case OreFreq::COMMON:   clusters = w.width / 12; break;
        case OreFreq::FREQUENT: clusters = w.width /  7; break;
    }

    std::uniform_int_distribution<int> xDist(2, w.width  - 3);
    std::uniform_int_distribution<int> yDist(2, w.height - 3);
    std::uniform_int_distribution<int> clusterSize(2, 6);
    std::uniform_int_distribution<int> spreadDist(-2, 2);

    for (int c = 0; c < clusters; ++c) {
        int ox = xDist(_rng);
        int oy = yDist(_rng);
        if (w.tiles[oy][ox] != TileType::BARE) { --c; continue; }

        int size = clusterSize(_rng);
        w.tiles[oy][ox] = oreType;

        int px = ox, py = oy;
        for (int s = 1; s < size; ++s) {
            int nx = px + spreadDist(_rng);
            int ny = py + spreadDist(_rng);
            if (nx >= 0 && ny >= 0 && nx < w.width && ny < w.height
                && w.tiles[ny][nx] == TileType::BARE) {
                w.tiles[ny][nx] = oreType;
                px = nx; py = ny;
            }
        }
    }
}

void WorldGenerator::generateTrees(GeneratedWorld& w) {
    int numForests = 0;
    int maxClusterRadius = 0;

    switch (w.settings.mapSize) {
        case MapSize::SMALL:      
            numForests = 25;  
            maxClusterRadius = 8;  
            break;
        case MapSize::MEDIUM:     
            numForests = 60;  
            maxClusterRadius = 15; 
            break;
        case MapSize::LARGE:      
            numForests = 120; 
            maxClusterRadius = 22; 
            break;
        case MapSize::VERY_LARGE: 
            numForests = 250; 
            maxClusterRadius = 32; 
            break;
    }

    std::uniform_int_distribution<int> distX(0, w.width - 1);
    std::uniform_int_distribution<int> distY(0, w.height - 1);
    std::uniform_real_distribution<float> distRadius(maxClusterRadius * 0.4f, static_cast<float>(maxClusterRadius));
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    for (int i = 0; i < numForests; ++i) {
        int cx = distX(_rng);
        int cy = distY(_rng);
        float currentRadius = distRadius(_rng);

        int iRadius = static_cast<int>(std::ceil(currentRadius));
        for (int y = cy - iRadius; y <= cy + iRadius; ++y) {
            for (int x = cx - iRadius; x <= cx + iRadius; ++x) {
                if (x >= 0 && x < w.width && y >= 0 && y < w.height) {
                    if (w.tiles[y][x] == TileType::BARE) {
                        float dx = static_cast<float>(x - cx);
                        float dy = static_cast<float>(y - cy);
                        float distance = std::sqrt(dx * dx + dy * dy);

                        if (distance <= currentRadius) {
                            float baseChance = 1.0f - (distance / currentRadius);
                            float finalChance = baseChance + 0.15f; 

                            if (chanceDist(_rng) < finalChance) {
                                w.tiles[y][x] = TileType::TREE;
                            }
                        }
                    }
                }
            }
        }
    }
}


void WorldGenerator::placeBorderCrossings(GeneratedWorld& w, int count) {
    std::vector<std::pair<int,int>> borderCells;

    for (int y = 0; y < w.height; ++y) {
        for (int x = 0; x < w.width; ++x) {
            TileType t = w.tiles[y][x];
            if (t == TileType::EMPTY || t == TileType::WATER || t == TileType::BARE_WATER) continue;

            if (w.get(x - 1, y) == TileType::EMPTY ||
                w.get(x + 1, y) == TileType::EMPTY ||
                w.get(x, y - 1) == TileType::EMPTY ||
                w.get(x, y + 1) == TileType::EMPTY) {
                borderCells.push_back({x, y});
            }
        }
    }

    if (borderCells.empty()) return;

    int total = static_cast<int>(borderCells.size());
    int step  = total / std::max(1, count);

    for (int i = 0; i < count && i * step < total; ++i) {
        auto [bx, by] = borderCells[i * step];
        w.borderCrossings.push_back({bx, by});
    }
}
