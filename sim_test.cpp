#include <cstdio>
#include <iterator>
#include <string>
#include <cstdlib>
#include <fstream>
#include <set>
#include <utility>

#include "../src/ini.h"
#include "../src/sim.h"
#include "../src/strafe.h"

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
    map.boxes = {{8, -6, 2.5f, 1.0f, 3}, {-9, 6, 1.0f, 3.0f, 3}};
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
            for (const Box& bx : map.boxes) {
                float cx = std::clamp(bb.pos.x, bx.x - bx.hw, bx.x + bx.hw), cz = std::clamp(bb.pos.z, bx.z - bx.hd, bx.z + bx.hd);
                float dx = bb.pos.x - cx, dz = bb.pos.z - cz;
                if (std::sqrt(dx * dx + dz * dz) < cfg.radius - 0.01f) outOfPillars = false;
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

    // ---- boxes: shots are blocked, circles are pushed out, spots inside are not free
    {
        Map bm;
        bm.half = 30;
        bm.boxes = {{0, -6, 2.0f, 1.0f, 2.0f}};
        std::vector<Bot> target(1);
        target[0].alive = true;
        target[0].pos = {0, 0, -12};
        V3 e{0, 1.5f, 0};
        Hit hb = raycast(bm, target, cfg, e, {0, 0, -1});
        CHECK(hb.bot == -1);                          // the block is in the way
        CHECK(std::fabs(hb.t - 5.0f) < 0.01f);        // front face at z = -5
        hb = raycast(bm, target, cfg, {0, 3.5f, 0}, normalize({0, -0.15f, -1}));
        CHECK(hb.bot == 0);                           // shooting over the top works
        CHECK(!isFree(bm, 0, -6, 0.5f));
        CHECK(isFree(bm, 6, 6, 0.5f));
        V3 q{0.5f, 0, -6.2f};                         // centre inside the box
        resolve(bm, q, 0.35f);
        CHECK(!(std::fabs(q.x) < 2.0f + 0.34f && std::fabs(q.z + 6.f) < 1.0f + 0.34f));
        V3 q2{2.1f, 0, -6.f};                         // overlapping a side face
        resolve(bm, q2, 0.35f);
        CHECK(std::fabs(q2.x - 2.35f) < 0.01f);
    }

    // ---- ini: updateFile keeps comments and only touches the keys it is given
    {
        {
            std::ofstream f("ini_upd_tmp.ini");
            f << "[mouse]\nsensitivity = 1.0    ; degrees per count\ndpi = 800\n[audio]\nmaster = 0.6\n";
        }
        int n = Ini::updateFile("ini_upd_tmp.ini", {{"mouse.sensitivity", "2.35"}, {"audio.master", "0.3"}, {"nope.key", "1"}});
        CHECK(n == 2);
        Ini u;
        CHECK(u.load("ini_upd_tmp.ini"));
        CHECK(std::fabs(u.num("mouse.sensitivity", 0) - 2.35f) < 1e-4f);
        CHECK(u.integer("mouse.dpi", 0) == 800);
        CHECK(std::fabs(u.num("audio.master", 0) - 0.3f) < 1e-4f);
        std::ifstream chk("ini_upd_tmp.ini");
        std::string all((std::istreambuf_iterator<char>(chk)), std::istreambuf_iterator<char>());
        CHECK(all.find("degrees per count") != std::string::npos);
        CHECK(Ini::updateFile("does_not_exist.ini", {}) == -1);
        std::remove("ini_upd_tmp.ini");
    }

    // ---- strafe timing
    {
        using strafe::Tracker;
        Tracker t;
        CHECK(!t.hasGap());
        t.press(Tracker::A, 0.5);
        t.release(Tracker::A, 1.000);
        t.press(Tracker::D, 1.050);  // 50 ms of dead time
        CHECK(t.hasGap() && std::fabs(t.lastGapMs() - 50.f) < 0.01f);

        t.release(Tracker::D, 1.5);
        t.press(Tracker::A, 1.7);  // pressed 0.2 s after release, still counts
        CHECK(std::fabs(t.lastGapMs() - 200.f) < 0.01f);

        t.press(Tracker::D, 2.000);  // D goes down while A is still held
        t.release(Tracker::A, 2.030);
        CHECK(std::fabs(t.lastGapMs() + 30.f) < 0.01f);  // 30 ms overlap

        const size_t before = t.history().size();
        t.release(Tracker::D, 2.2);
        t.press(Tracker::A, 5.0);  // outside the 600 ms window: not counted
        CHECK(t.history().size() == before);

        t.press(Tracker::D, 6.0);  // overlap, but the new key is dropped first: nothing recorded
        t.release(Tracker::D, 6.01);
        t.release(Tracker::A, 6.02);
        CHECK(t.history().size() == before);

        Tracker h;
        for (int i = 0; i < 15; i++) {
            h.press(Tracker::A, i * 10.0);
            h.release(Tracker::A, i * 10.0 + 1);
            h.press(Tracker::D, i * 10.0 + 1.1);
        }
        CHECK(h.history().size() == 10);
        CHECK(std::fabs(h.avgAbsMs() - 100.f) < 0.5f);
        h.reset();
        CHECK(!h.hasGap());

        CHECK(strafe::moveToward(0.f, 5.f, 2.f) == 2.f);
        CHECK(strafe::moveToward(4.f, 5.f, 2.f) == 5.f);
        CHECK(strafe::moveToward(1.f, -5.f, 2.f) == -1.f);
    }

    if (g_fail == 0) std::printf("all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
