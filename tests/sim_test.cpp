#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <utility>

#include "../src/ini.h"
#include "../src/sim.h"

static int g_fail = 0;
#define CHECK(c)                                                    \
    do {                                                            \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
            ++g_fail;                                               \
        }                                                           \
    } while (0)

int main() {
    using namespace sim;
    BotCfg cfg;

    // ---- raycast: head / body / miss / pillar / floor / dead bot
    Map open;
    open.half = 40;
    std::vector<Bot> one(1);
    one[0].alive = true;
    one[0].pos = {0, 0, -10};
    V3 eye{0, 1.65f, 0};

    Hit h = raycast(open, one, cfg, eye, {0, 0, -1});
    CHECK(h.bot == 0 && h.part == Part::Head);
    CHECK(std::fabs(h.t - 9.8715f) < 0.02f);

    h = raycast(open, one, cfg, {0, 1.0f, 0}, {0, 0, -1});
    CHECK(h.bot == 0 && h.part == Part::Body);

    h = raycast(open, one, cfg, {0, 2.5f, 0}, {0, 0, -1});
    CHECK(h.bot == -1);
    CHECK(h.t >= 299.f);

    Map blocked;
    blocked.half = 40;
    blocked.pillars = {{0, -5, 1, 3}};
    h = raycast(blocked, one, cfg, eye, {0, 0, -1});
    CHECK(h.bot == -1);
    CHECK(std::fabs(h.t - 4.0f) < 0.01f);

    h = raycast(open, std::vector<Bot>{}, cfg, eye, {0, -1, 0});
    CHECK(h.bot == -1);
    CHECK(std::fabs(h.t - 1.65f) < 1e-3f);

    one[0].alive = false;
    h = raycast(open, one, cfg, eye, {0, 0, -1});
    CHECK(h.bot == -1);

    // ---- damage, kill, respawn
    Map map;
    map.half = 15;
    map.pillars = {{5, 5, 1.2f, 3}, {-6, 2, 1.5f, 3}, {0, -8, 1, 3}};
    Rng rng(7);
    Bot b;
    spawn(b, map, rng, cfg);
    CHECK(b.alive && b.hp == cfg.hp);
    CHECK(!applyHit(b, 60, cfg));
    CHECK(b.alive);
    CHECK(applyHit(b, 60, cfg));
    CHECK(!b.alive);
    CHECK(!applyHit(b, 60, cfg));  // already dead: no double kill
    for (int i = 0; i < 120 && !b.alive; i++) update(b, 1.f / 60.f, map, rng, cfg);
    CHECK(b.alive);

    // ---- wandering: stays legal and does not follow one fixed route
    Rng rng2(42);
    std::vector<Bot> bots(4);
    std::vector<V3> lastDest(bots.size());
    for (auto& bb : bots) spawn(bb, map, rng2, cfg);
    std::set<std::pair<int, int>> cells;
    int destChanges = 0;
    bool inBounds = true, outOfPillars = true;
    for (int step = 0; step < 60 * 120; step++) {
        for (size_t i = 0; i < bots.size(); i++) {
            Bot& bb = bots[i];
            update(bb, 1.f / 60.f, map, rng2, cfg);
            if (std::fabs(bb.pos.x) > map.half || std::fabs(bb.pos.z) > map.half) inBounds = false;
            for (const Pillar& p : map.pillars) {
                float dx = bb.pos.x - p.x, dz = bb.pos.z - p.z;
                if (std::sqrt(dx * dx + dz * dz) < p.r + cfg.radius - 0.01f) outOfPillars = false;
            }
            cells.insert({static_cast<int>(std::floor(bb.pos.x / 2)), static_cast<int>(std::floor(bb.pos.z / 2))});
            if (bb.dest.x != lastDest[i].x || bb.dest.z != lastDest[i].z) {
                ++destChanges;
                lastDest[i] = bb.dest;
            }
        }
    }
    CHECK(inBounds);
    CHECK(outOfPillars);
    CHECK(cells.size() >= 40);
    CHECK(destChanges > 100);
    std::printf("wander: %zu distinct 2m cells visited, %d destination changes\n", cells.size(), destChanges);

    // ---- stats
    Stats s;
    CHECK(s.accuracy() == 0.f);
    s.shots = 4;
    s.hits = 3;
    CHECK(std::fabs(s.accuracy() - 75.f) < 1e-4f);

    // ---- ini
    {
        std::ofstream f("ini_test_tmp.ini");
        f << "[window]\nwidth = 1920 ; comment\nfullscreen = true\n[aura]\nrim_color = 80, 200, 255\nname = Cool Rifle\n";
    }
    Ini ini;
    CHECK(ini.load("ini_test_tmp.ini"));
    CHECK(ini.integer("window.width", 0) == 1920);
    CHECK(ini.boolean("window.fullscreen", false));
    CHECK(ini.str("aura.name") == "Cool Rifle");
    auto col = ini.list("aura.rim_color");
    CHECK(col.size() == 3 && col[1] == 200.f);
    CHECK(ini.num("missing.key", 3.5f) == 3.5f);
    CHECK(!Ini().load("does_not_exist.ini"));
    std::remove("ini_test_tmp.ini");

    if (g_fail == 0) std::printf("all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
