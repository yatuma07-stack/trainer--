#pragma once
// Pure game logic: no raylib, no file I/O. Everything here is covered by tests/sim_test.cpp.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace sim {

struct V3 {
    float x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) {
    float l = length(a);
    return l > 1e-6f ? a * (1.0f / l) : V3{};
}

class Rng {
public:
    explicit Rng(uint32_t seed = 1) : g_(seed) {}
    float range(float a, float b) { return a + (b - a) * std::uniform_real_distribution<float>(0.f, 1.f)(g_); }
    bool chance(float p) { return range(0.f, 1.f) < p; }

private:
    std::mt19937 g_;
};

// ---------------------------------------------------------------- map

struct Pillar {
    float x, z, r, h;
};

struct Map {
    float half = 30;  // arena is a square from -half..+half on X and Z
    std::vector<Pillar> pillars;
};

// Keeps a circle of `radius` inside the arena and outside every pillar.
inline void resolve(const Map& m, V3& p, float radius) {
    const float lim = std::max(0.1f, m.half - radius);
    p.x = std::clamp(p.x, -lim, lim);
    p.z = std::clamp(p.z, -lim, lim);
    for (const Pillar& c : m.pillars) {
        float dx = p.x - c.x, dz = p.z - c.z;
        float d = std::sqrt(dx * dx + dz * dz);
        const float minD = c.r + radius;
        if (d < minD) {
            if (d < 1e-5f) {
                dx = 1;
                dz = 0;
                d = 1;
            }
            p.x = c.x + dx / d * minD;
            p.z = c.z + dz / d * minD;
        }
    }
    p.x = std::clamp(p.x, -lim, lim);
    p.z = std::clamp(p.z, -lim, lim);
}

inline bool isFree(const Map& m, float x, float z, float margin) {
    if (std::fabs(x) > m.half - margin || std::fabs(z) > m.half - margin) return false;
    for (const Pillar& c : m.pillars) {
        float dx = x - c.x, dz = z - c.z, r = c.r + margin;
        if (dx * dx + dz * dz < r * r) return false;
    }
    return true;
}

inline V3 randomSpot(const Map& m, Rng& rng, float margin) {
    const float lim = std::max(0.1f, m.half - margin);
    for (int i = 0; i < 64; i++) {
        float x = rng.range(-lim, lim), z = rng.range(-lim, lim);
        if (isFree(m, x, z, margin)) return {x, 0, z};
    }
    return {0, 0, 0};
}

// ---------------------------------------------------------------- bots

struct BotCfg {
    float height = 1.8f, radius = 0.35f, head_radius = 0.13f;
    float speed_min = 2.0f, speed_max = 5.0f;
    float pause_chance = 0.15f, pause_min = 0.1f, pause_max = 0.6f;
    float strafe_chance = 0.25f;
    float retarget_min = 0.5f, retarget_max = 2.5f;  // seconds before the bot picks a new destination
    float hp = 100, respawn = 0.4f;
};

constexpr float kHitFlash = 0.12f;

struct Bot {
    V3 pos, dest;
    float yaw = 0, speed = 3, hp = 100;
    float pause = 0, retarget = 0, strafeT = 0;
    int strafeDir = 0;
    float hitFlash = 0, respawnT = 0;
    bool alive = false;
};

inline void spawn(Bot& b, const Map& m, Rng& rng, const BotCfg& c) {
    const float margin = c.radius + 0.5f;
    b.pos = randomSpot(m, rng, margin);
    b.dest = randomSpot(m, rng, margin);
    b.hp = c.hp;
    b.alive = true;
    b.speed = rng.range(c.speed_min, c.speed_max);
    b.pause = 0;
    b.retarget = rng.range(c.retarget_min, c.retarget_max);
    b.strafeT = 0;
    b.strafeDir = 0;
    b.hitFlash = 0;
}

// Random-waypoint wandering: every bot keeps picking new destinations anywhere in the arena,
// with random speed, pauses and strafing, so there is no fixed route to learn.
inline void update(Bot& b, float dt, const Map& m, Rng& rng, const BotCfg& c) {
    if (!b.alive) {
        b.respawnT -= dt;
        if (b.respawnT <= 0) spawn(b, m, rng, c);
        return;
    }
    b.hitFlash = std::max(0.f, b.hitFlash - dt);
    if (b.pause > 0) {
        b.pause -= dt;
        return;
    }
    b.retarget -= dt;
    V3 to = b.dest - b.pos;
    to.y = 0;
    if (length(to) < 0.6f || b.retarget <= 0) {
        b.dest = randomSpot(m, rng, c.radius + 0.5f);
        b.retarget = rng.range(c.retarget_min, c.retarget_max);
        b.speed = rng.range(c.speed_min, c.speed_max);
        if (rng.chance(c.pause_chance)) b.pause = rng.range(c.pause_min, c.pause_max);
        to = b.dest - b.pos;
        to.y = 0;
    }
    V3 dir = normalize(to);
    b.strafeT -= dt;
    if (b.strafeT <= 0) {
        b.strafeDir = rng.chance(c.strafe_chance) ? (rng.chance(0.5f) ? 1 : -1) : 0;
        b.strafeT = rng.range(0.15f, 0.6f);
    }
    V3 side{-dir.z, 0, dir.x};
    V3 move = normalize(dir + side * (0.9f * static_cast<float>(b.strafeDir)));
    V3 before = b.pos;
    b.pos = b.pos + move * (b.speed * dt);
    resolve(m, b.pos, c.radius);
    if (length(b.pos - before) < b.speed * dt * 0.3f) b.retarget = 0;  // blocked: pick a new destination next tick
    if (length(move) > 0) b.yaw = std::atan2(move.x, move.z) * 57.29578f;
}

// Returns true if this hit killed the bot.
inline bool applyHit(Bot& b, float dmg, const BotCfg& c) {
    if (!b.alive) return false;
    b.hp -= dmg;
    b.hitFlash = kHitFlash;
    if (b.hp <= 0) {
        b.alive = false;
        b.respawnT = c.respawn;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- hitscan

enum class Part { None, Body, Head };

struct Hit {
    int bot = -1;
    Part part = Part::None;
    float t = 0;  // distance to whatever the ray hit first (also set on a miss)
};

inline bool raySphere(V3 o, V3 d, V3 c, float r, float& t) {
    V3 oc = o - c;
    float b = dot(oc, d), cc = dot(oc, oc) - r * r, disc = b * b - cc;
    if (disc < 0) return false;
    float s = std::sqrt(disc), t0 = -b - s;
    if (t0 < 0) t0 = -b + s;
    if (t0 < 0) return false;
    t = t0;
    return true;
}

inline bool rayPillar(V3 o, V3 d, const Pillar& p, float& t) {
    float ox = o.x - p.x, oz = o.z - p.z;
    float a = d.x * d.x + d.z * d.z;
    if (a < 1e-8f) return false;
    float b = ox * d.x + oz * d.z, cc = ox * ox + oz * oz - p.r * p.r, disc = b * b - a * cc;
    if (disc < 0) return false;
    float s = std::sqrt(disc), t0 = (-b - s) / a;
    if (t0 < 0) t0 = (-b + s) / a;
    if (t0 < 0) return false;
    float y = o.y + d.y * t0;
    if (y < 0 || y > p.h) return false;
    t = t0;
    return true;
}

// `d` must be normalized. Pillars and the floor block shots. Bots are a head sphere on top of a
// chain of body spheres, which is cheap and accurate enough for a capsule-shaped target.
inline Hit raycast(const Map& m, const std::vector<Bot>& bots, const BotCfg& c, V3 o, V3 d, float maxDist = 300.f) {
    float block = maxDist;
    for (const Pillar& p : m.pillars) {
        float t;
        if (rayPillar(o, d, p, t)) block = std::min(block, t);
    }
    if (d.y < -1e-6f) block = std::min(block, -o.y / d.y);

    Hit best;
    best.t = block;
    for (size_t i = 0; i < bots.size(); i++) {
        const Bot& b = bots[i];
        if (!b.alive) continue;
        float t;
        V3 head{b.pos.x, b.pos.y + c.height - c.head_radius, b.pos.z};
        if (raySphere(o, d, head, c.head_radius, t) && t < best.t) best = {static_cast<int>(i), Part::Head, t};
        const float y0 = c.radius;
        const float y1 = std::max(y0, c.height - 2 * c.head_radius - c.radius);
        for (int k = 0; k < 5; k++) {
            V3 cc{b.pos.x, b.pos.y + y0 + (y1 - y0) * (static_cast<float>(k) / 4.f), b.pos.z};
            if (raySphere(o, d, cc, c.radius, t) && t < best.t) best = {static_cast<int>(i), Part::Body, t};
        }
    }
    return best;
}

struct Stats {
    int shots = 0, hits = 0, headshots = 0, kills = 0;
    float accuracy() const { return shots ? 100.f * static_cast<float>(hits) / static_cast<float>(shots) : 0.f; }
};

}  // namespace sim
