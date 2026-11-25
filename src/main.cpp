// Lightweight customizable aim trainer. raylib (OpenGL 3.3) + C++17.
// All game rules live in sim.h; this file is window, rendering, audio, input and config.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ini.h"
#include "raylib.h"
#include "rlgl.h"
#include "sim.h"

namespace {

// ====================================================================== config

struct Aura {
    bool outline = true;
    Color outline_color{80, 200, 255, 255};
    float outline_thickness = 0.03f;  // meters, extrusion along normals
    bool rim = true;
    Color rim_color{80, 200, 255, 255};
    float rim_power = 2.5f;
    float rim_intensity = 1.4f;
    float pulse_speed = 0.0f;  // Hz, 0 = steady
    bool rainbow = false;
    float rainbow_speed = 0.25f;  // hue cycles per second
    bool through_walls = false;
    Color hit_color{255, 255, 255, 255};  // flash colour right after a hit
};

struct Config {
    // window
    int width = 1280, height = 720, fps = 0, msaa = 4;
    bool fullscreen = false, vsync = false;
    float hfov = 100.f;
    // mouse: either `sensitivity` (degrees per count = sensitivity * 0.022, Source-style)
    // or cm360 + dpi, which wins when both are > 0.
    float sensitivity = 1.0f, cm360 = 0.f, dpi = 800.f;
    // weapon
    float damage = 34.f, head_mult = 2.0f, rpm = 600.f, spread = 0.f;
    bool auto_fire = true;
    // tracer
    bool tracer = true;
    Color tracer_color{255, 230, 120, 255};
    float tracer_width = 0.012f, tracer_length = 4.f, tracer_speed = 220.f;
    // hit marker
    bool hitmarker = true;
    Color hm_color{255, 255, 255, 255}, hm_head{255, 90, 60, 255}, hm_kill{255, 40, 40, 255};
    float hm_size = 9.f, hm_gap = 4.f, hm_thick = 2.f, hm_time = 0.18f;
    // crosshair
    bool crosshair = true, ch_dot = true;
    Color ch_color{60, 255, 120, 255};
    float ch_size = 7.f, ch_gap = 4.f, ch_thick = 2.f;
    // world
    Color sky_top{70, 120, 190, 255}, sky_bottom{190, 215, 235, 255};
    Color floor_col{52, 56, 62, 255}, grid_col{70, 76, 84, 255}, pillar_col{90, 96, 108, 255}, wall_col{36, 40, 46, 255};
    std::string sky_image = "sky/panorama.png";  // optional equirectangular image
    std::string map_file = "maps/arena.map";
    // bots
    int bot_count = 6;
    sim::BotCfg bot;
    Color bot_color{200, 70, 70, 255};
    std::string bot_model = "models/bot.glb";  // optional glTF, scaled to bot height
    Aura aura;
    // audio
    float master = 0.6f;
    float vol_shot = 0.35f, vol_hit = 0.8f, vol_head = 0.9f, vol_kill = 0.9f, vol_death = 0.7f;
};

Color readColor(const Ini& i, const std::string& k, Color d) {
    auto v = i.list(k);
    if (v.size() < 3) return d;
    auto c = [](float x) { return static_cast<unsigned char>(std::clamp(x, 0.f, 255.f)); };
    return Color{c(v[0]), c(v[1]), c(v[2]), v.size() > 3 ? c(v[3]) : static_cast<unsigned char>(255)};
}

void loadConfig(const std::string& path, Config& c) {
    Ini i;
    if (!i.load(path)) {
        std::printf("config: %s not found, using defaults\n", path.c_str());
        return;
    }
    c.width = i.integer("window.width", c.width);
    c.height = i.integer("window.height", c.height);
    c.fps = i.integer("window.fps", c.fps);
    c.msaa = i.integer("window.msaa", c.msaa);
    c.fullscreen = i.boolean("window.fullscreen", c.fullscreen);
    c.vsync = i.boolean("window.vsync", c.vsync);
    c.hfov = i.num("window.hfov", c.hfov);

    c.sensitivity = i.num("mouse.sensitivity", c.sensitivity);
    c.cm360 = i.num("mouse.cm360", c.cm360);
    c.dpi = i.num("mouse.dpi", c.dpi);

    c.damage = i.num("weapon.damage", c.damage);
    c.head_mult = i.num("weapon.head_multiplier", c.head_mult);
    c.rpm = i.num("weapon.rpm", c.rpm);
    c.spread = i.num("weapon.spread", c.spread);
    c.auto_fire = i.boolean("weapon.auto", c.auto_fire);

    c.tracer = i.boolean("tracer.enabled", c.tracer);
    c.tracer_color = readColor(i, "tracer.color", c.tracer_color);
    c.tracer_width = i.num("tracer.width", c.tracer_width);
    c.tracer_length = i.num("tracer.length", c.tracer_length);
    c.tracer_speed = i.num("tracer.speed", c.tracer_speed);

    c.hitmarker = i.boolean("hitmarker.enabled", c.hitmarker);
    c.hm_color = readColor(i, "hitmarker.color", c.hm_color);
    c.hm_head = readColor(i, "hitmarker.head_color", c.hm_head);
    c.hm_kill = readColor(i, "hitmarker.kill_color", c.hm_kill);
    c.hm_size = i.num("hitmarker.size", c.hm_size);
    c.hm_gap = i.num("hitmarker.gap", c.hm_gap);
    c.hm_thick = i.num("hitmarker.thickness", c.hm_thick);
    c.hm_time = i.num("hitmarker.duration", c.hm_time);

    c.crosshair = i.boolean("crosshair.enabled", c.crosshair);
    c.ch_dot = i.boolean("crosshair.dot", c.ch_dot);
    c.ch_color = readColor(i, "crosshair.color", c.ch_color);
    c.ch_size = i.num("crosshair.size", c.ch_size);
    c.ch_gap = i.num("crosshair.gap", c.ch_gap);
    c.ch_thick = i.num("crosshair.thickness", c.ch_thick);

    c.sky_top = readColor(i, "world.sky_top", c.sky_top);
    c.sky_bottom = readColor(i, "world.sky_bottom", c.sky_bottom);
    c.floor_col = readColor(i, "world.floor", c.floor_col);
    c.grid_col = readColor(i, "world.grid", c.grid_col);
    c.pillar_col = readColor(i, "world.pillar", c.pillar_col);
    c.wall_col = readColor(i, "world.wall", c.wall_col);
    c.sky_image = i.str("world.sky_image", c.sky_image);
    c.map_file = i.str("world.map", c.map_file);

    c.bot_count = std::clamp(i.integer("bots.count", c.bot_count), 0, 64);
    c.bot_color = readColor(i, "bots.color", c.bot_color);
    c.bot_model = i.str("bots.model", c.bot_model);
    sim::BotCfg& b = c.bot;
    b.height = i.num("bots.height", b.height);
    b.radius = i.num("bots.radius", b.radius);
    b.head_radius = i.num("bots.head_radius", b.head_radius);
    b.speed_min = i.num("bots.speed_min", b.speed_min);
    b.speed_max = std::max(b.speed_min, i.num("bots.speed_max", b.speed_max));
    b.pause_chance = i.num("bots.pause_chance", b.pause_chance);
    b.strafe_chance = i.num("bots.strafe_chance", b.strafe_chance);
    b.retarget_min = i.num("bots.retarget_min", b.retarget_min);
    b.retarget_max = std::max(b.retarget_min, i.num("bots.retarget_max", b.retarget_max));
    b.hp = i.num("bots.hp", b.hp);
    b.respawn = i.num("bots.respawn", b.respawn);

    Aura& a = c.aura;
    a.outline = i.boolean("aura.outline", a.outline);
    a.outline_color = readColor(i, "aura.outline_color", a.outline_color);
    a.outline_thickness = i.num("aura.outline_thickness", a.outline_thickness);
    a.rim = i.boolean("aura.rim", a.rim);
    a.rim_color = readColor(i, "aura.rim_color", a.rim_color);
    a.rim_power = i.num("aura.rim_power", a.rim_power);
    a.rim_intensity = i.num("aura.rim_intensity", a.rim_intensity);
    a.pulse_speed = i.num("aura.pulse_speed", a.pulse_speed);
    a.rainbow = i.boolean("aura.rainbow", a.rainbow);
    a.rainbow_speed = i.num("aura.rainbow_speed", a.rainbow_speed);
    a.through_walls = i.boolean("aura.through_walls", a.through_walls);
    a.hit_color = readColor(i, "aura.hit_color", a.hit_color);

    c.master = i.num("audio.master", c.master);
    c.vol_shot = i.num("audio.shot", c.vol_shot);
    c.vol_hit = i.num("audio.hit", c.vol_hit);
    c.vol_head = i.num("audio.headshot", c.vol_head);
    c.vol_kill = i.num("audio.kill", c.vol_kill);
    c.vol_death = i.num("audio.death", c.vol_death);
}

// Map file: "size <half>" and "pillar <x> <z> <radius> <height>" lines, '#' comments.
bool loadMap(const std::string& path, sim::Map& m) {
    std::ifstream f(path);
    if (!f) return false;
    sim::Map out;
    std::string line;
    while (std::getline(f, line)) {
        auto h = line.find('#');
        if (h != std::string::npos) line.erase(h);
        std::istringstream ss(line);
        std::string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == "size") {
            float half;
            if (ss >> half && half > 3.f) out.half = half;
        } else if (cmd == "pillar") {
            sim::Pillar p;
            if (ss >> p.x >> p.z >> p.r >> p.h && p.r > 0.f) out.pillars.push_back(p);
        }
    }
    m = out;
    return true;
}

sim::Map defaultMap() {
    sim::Map m;
    m.half = 30;
    m.pillars = {{-12, -10, 1.5f, 4}, {10, -14, 2.0f, 5}, {0, 8, 1.2f, 3}, {-15, 12, 2.5f, 6}, {16, 10, 1.5f, 4}, {3, -2, 1.0f, 3}};
    return m;
}

Vector3 toRl(sim::V3 v) { return Vector3{v.x, v.y, v.z}; }

// ====================================================================== audio

// Used when sounds/<name>.wav|ogg|mp3|flac does not exist, so the game works with zero assets.
Wave synth(float f0, float f1, float ms, float noiseMix, float decayPow) {
    const int rate = 44100;
    const int n = std::max(1, static_cast<int>(static_cast<float>(rate) * ms / 1000.f));
    short* d = static_cast<short*>(std::malloc(static_cast<size_t>(n) * sizeof(short)));
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        float f = f0 + (f1 - f0) * t;
        ph += 2.f * 3.14159265f * f / static_cast<float>(rate);
        float noise = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.f - 1.f;
        float s = std::sin(ph) * (1.f - noiseMix) + noise * noiseMix;
        float env = std::pow(1.f - t, decayPow);
        d[i] = static_cast<short>(s * env * 0.6f * 32767.f);
    }
    Wave w{};
    w.frameCount = static_cast<unsigned int>(n);
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = d;
    return w;
}

struct Sfx {
    std::vector<Sound> pool;  // pool[0] owns the data, the rest are aliases so rapid fire can overlap
    size_t next = 0;
    float vol = 1.f;
};

Sfx sfxLoad(const char* name, Wave fallback, float vol) {
    Sfx s;
    s.vol = vol;
    Sound base{};
    bool loaded = false;
    for (const char* ext : {".wav", ".ogg", ".mp3", ".flac"}) {
        std::string p = std::string("sounds/") + name + ext;
        if (FileExists(p.c_str())) {
            base = LoadSound(p.c_str());
            loaded = IsSoundValid(base);
            if (loaded) break;
        }
    }
    if (!loaded) base = LoadSoundFromWave(fallback);
    UnloadWave(fallback);
    s.pool.push_back(base);
    for (int i = 0; i < 5; i++) s.pool.push_back(LoadSoundAlias(base));
    return s;
}

void sfxUnload(Sfx& s) {
    for (size_t i = 1; i < s.pool.size(); i++) UnloadSoundAlias(s.pool[i]);
    if (!s.pool.empty()) UnloadSound(s.pool[0]);
    s.pool.clear();
}

void sfxPlay(Sfx& s, float master) {
    if (s.pool.empty()) return;
    Sound& snd = s.pool[s.next++ % s.pool.size()];
    SetSoundVolume(snd, std::clamp(s.vol * master, 0.f, 1.f));
    PlaySound(snd);
}

// ====================================================================== rendering assets

const char* kVert = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform float uExtrude;
out vec3 vN;
out vec3 vP;
void main() {
    vec3 n = normalize(vertexNormal);
    vec3 p = vertexPosition + n * uExtrude;
    vP = (matModel * vec4(p, 1.0)).xyz;
    vN = normalize(vec3(matNormal * vec4(n, 0.0)));
    gl_Position = mvp * vec4(p, 1.0);
}
)";

const char* kFrag = R"(#version 330
in vec3 vN;
in vec3 vP;
out vec4 finalColor;
uniform vec3 uView;
uniform vec3 uBase;
uniform vec3 uRim;
uniform float uRimPow;
uniform float uRimInt;
uniform float uFlat;
void main() {
    if (uFlat > 0.5) { finalColor = vec4(uBase, 1.0); return; }
    vec3 N = normalize(vN);
    vec3 V = normalize(uView - vP);
    float lambert = max(dot(N, normalize(vec3(0.4, 1.0, 0.3))), 0.0);
    float rim = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), uRimPow) * uRimInt;
    finalColor = vec4(uBase * (0.4 + 0.6 * lambert) + uRim * rim, 1.0);
}
)";

struct Gfx {
    Shader sh{};
    int lView = -1, lBase = -1, lRim = -1, lRimPow = -1, lRimInt = -1, lFlat = -1, lExtrude = -1;
    Model body{}, head{}, custom{}, sky{};
    bool haveCustom = false, haveSky = false;
    float customScale = 1.f, customYOffset = 0.f;
    Texture2D skyTex{};
};

Vector3 colorVec(Color c) { return Vector3{c.r / 255.f, c.g / 255.f, c.b / 255.f}; }
void setF(Gfx& g, int loc, float v) { SetShaderValue(g.sh, loc, &v, SHADER_UNIFORM_FLOAT); }
void setV3(Gfx& g, int loc, Vector3 v) { SetShaderValue(g.sh, loc, &v, SHADER_UNIFORM_VEC3); }

void loadGfx(Gfx& g, const Config& c) {
    g.sh = LoadShaderFromMemory(kVert, kFrag);
    g.lView = GetShaderLocation(g.sh, "uView");
    g.lBase = GetShaderLocation(g.sh, "uBase");
    g.lRim = GetShaderLocation(g.sh, "uRim");
    g.lRimPow = GetShaderLocation(g.sh, "uRimPow");
    g.lRimInt = GetShaderLocation(g.sh, "uRimInt");
    g.lFlat = GetShaderLocation(g.sh, "uFlat");
    g.lExtrude = GetShaderLocation(g.sh, "uExtrude");

    const sim::BotCfg& b = c.bot;
    g.body = LoadModelFromMesh(GenMeshCylinder(b.radius, std::max(0.1f, b.height - 2.f * b.head_radius), 20));
    g.head = LoadModelFromMesh(GenMeshSphere(b.head_radius, 16, 16));
    g.body.materials[0].shader = g.sh;
    g.head.materials[0].shader = g.sh;

    g.haveCustom = false;
    if (FileExists(c.bot_model.c_str())) {
        g.custom = LoadModel(c.bot_model.c_str());
        if (g.custom.meshCount > 0) {
            for (int i = 0; i < g.custom.materialCount; i++) g.custom.materials[i].shader = g.sh;
            BoundingBox bb = GetModelBoundingBox(g.custom);
            float h = bb.max.y - bb.min.y;
            if (h > 1e-4f) {
                g.customScale = b.height / h;
                g.customYOffset = -bb.min.y * g.customScale;
                g.haveCustom = true;
            }
        }
        if (!g.haveCustom) std::printf("bots.model: %s has no usable geometry, using the built-in capsule\n", c.bot_model.c_str());
    }

    g.haveSky = false;
    if (FileExists(c.sky_image.c_str())) {
        g.skyTex = LoadTexture(c.sky_image.c_str());
        if (g.skyTex.id != 0) {
            g.sky = LoadModelFromMesh(GenMeshSphere(1.f, 48, 48));
            g.sky.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g.skyTex;
            g.haveSky = true;
        }
    }
}

void unloadGfx(Gfx& g) {
    UnloadModel(g.body);
    UnloadModel(g.head);
    if (g.haveCustom) UnloadModel(g.custom);
    if (g.haveSky) {
        UnloadModel(g.sky);
        UnloadTexture(g.skyTex);
    }
    UnloadShader(g.sh);
    g = Gfx{};
}

void drawBotParts(Gfx& g, const sim::Bot& b, const sim::BotCfg& c) {
    const Vector3 up{0, 1, 0};
    if (g.haveCustom) {
        Vector3 p{b.pos.x, b.pos.y + g.customYOffset, b.pos.z};
        DrawModelEx(g.custom, p, up, b.yaw, Vector3{g.customScale, g.customScale, g.customScale}, WHITE);
    } else {
        DrawModelEx(g.body, toRl(b.pos), up, b.yaw, Vector3{1, 1, 1}, WHITE);
        Vector3 hp{b.pos.x, b.pos.y + c.height - c.head_radius, b.pos.z};
        DrawModelEx(g.head, hp, up, b.yaw, Vector3{1, 1, 1}, WHITE);
    }
}

Color mixColor(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t); };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255};
}

void drawBot(Gfx& g, const Config& cfg, const sim::Bot& b, Vector3 eye, float time) {
    const Aura& a = cfg.aura;
    float flash = std::clamp(b.hitFlash / sim::kHitFlash, 0.f, 1.f);

    Color aura = a.rainbow ? ColorFromHSV(std::fmod(time * a.rainbow_speed, 1.f) * 360.f, 0.85f, 1.f) : a.rim_color;
    Color outline = a.rainbow ? aura : a.outline_color;
    float pulse = a.pulse_speed > 0.f ? 0.65f + 0.35f * std::sin(time * a.pulse_speed * 6.2831853f) : 1.f;

    setV3(g, g.lView, eye);

    if (a.outline) {
        rlDrawRenderBatchActive();
        if (a.through_walls) rlDisableDepthTest();
        rlSetCullFace(RL_CULL_FACE_FRONT);
        setF(g, g.lFlat, 1.f);
        setF(g, g.lExtrude, a.outline_thickness);
        setV3(g, g.lBase, colorVec(mixColor(outline, a.hit_color, flash)));
        drawBotParts(g, b, cfg.bot);
        rlSetCullFace(RL_CULL_FACE_BACK);
        if (a.through_walls) rlEnableDepthTest();
    }

    setF(g, g.lFlat, 0.f);
    setF(g, g.lExtrude, 0.f);
    setV3(g, g.lBase, colorVec(mixColor(cfg.bot_color, a.hit_color, flash)));
    setV3(g, g.lRim, colorVec(aura));
    setF(g, g.lRimPow, a.rim_power);
    setF(g, g.lRimInt, a.rim ? a.rim_intensity * pulse * (1.f + flash) : 0.f);
    drawBotParts(g, b, cfg.bot);
}

// ====================================================================== misc helpers

struct Tracer {
    sim::V3 a, b;
    float age = 0;
};

// Distance along a ray from inside the arena to its square boundary.
float wallDistance(sim::V3 o, sim::V3 d, float half) {
    float t = 1e9f;
    if (std::fabs(d.x) > 1e-6f) t = std::min(t, ((d.x > 0 ? half : -half) - o.x) / d.x);
    if (std::fabs(d.z) > 1e-6f) t = std::min(t, ((d.z > 0 ? half : -half) - o.z) / d.z);
    return std::max(0.f, t);
}

void drawHitMarker(float cx, float cy, float size, float gap, float thick, Color c) {
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            DrawLineEx(Vector2{cx + sx * gap, cy + sy * gap}, Vector2{cx + sx * (gap + size), cy + sy * (gap + size)}, thick, c);
}

void drawCrosshair(float cx, float cy, const Config& c) {
    const float s = c.ch_size, g = c.ch_gap, t = c.ch_thick;
    DrawRectangleRec(Rectangle{cx - g - s, cy - t / 2, s, t}, c.ch_color);
    DrawRectangleRec(Rectangle{cx + g, cy - t / 2, s, t}, c.ch_color);
    DrawRectangleRec(Rectangle{cx - t / 2, cy - g - s, t, s}, c.ch_color);
    DrawRectangleRec(Rectangle{cx - t / 2, cy + g, t, s}, c.ch_color);
    if (c.ch_dot) DrawRectangleRec(Rectangle{cx - t / 2, cy - t / 2, t, t}, c.ch_color);
}

struct World {
    sim::Map map;
    std::vector<sim::Bot> bots;
    sim::Rng rng{12345};
};

void buildWorld(World& w, const Config& c) {
    if (!loadMap(c.map_file, w.map)) w.map = defaultMap();
    w.bots.assign(static_cast<size_t>(c.bot_count), sim::Bot{});
    for (auto& b : w.bots) sim::spawn(b, w.map, w.rng, c.bot);
}

}  // namespace

// ====================================================================== main

int main() {
    ChangeDirectory(GetApplicationDirectory());  // assets sit next to the executable

    Config cfg;
    loadConfig("config.ini", cfg);

    unsigned flags = 0;
    if (cfg.vsync) flags |= FLAG_VSYNC_HINT;
    if (cfg.msaa > 1) flags |= FLAG_MSAA_4X_HINT;
    if (cfg.fullscreen) flags |= FLAG_FULLSCREEN_MODE;
    SetConfigFlags(flags);
    InitWindow(cfg.width, cfg.height, "Aim Trainer");
    if (cfg.fps > 0) SetTargetFPS(cfg.fps);
    DisableCursor();

    InitAudioDevice();
    const bool audioOk = IsAudioDeviceReady();

    Gfx gfx;
    loadGfx(gfx, cfg);

    struct Sounds {
        Sfx shot, hit, head, kill, death;
    } snd;
    auto loadSounds = [&]() {
        if (!audioOk) return;
        snd.shot = sfxLoad("shot", synth(900, 120, 55, 0.7f, 2.0f), cfg.vol_shot);
        snd.hit = sfxLoad("hit", synth(1300, 1000, 60, 0.f, 1.5f), cfg.vol_hit);
        snd.head = sfxLoad("headshot", synth(2000, 1500, 90, 0.f, 1.5f), cfg.vol_head);
        snd.kill = sfxLoad("kill", synth(600, 1200, 160, 0.f, 1.2f), cfg.vol_kill);
        snd.death = sfxLoad("death", synth(320, 70, 240, 0.35f, 1.4f), cfg.vol_death);
    };
    auto unloadSounds = [&]() {
        if (!audioOk) return;
        sfxUnload(snd.shot);
        sfxUnload(snd.hit);
        sfxUnload(snd.head);
        sfxUnload(snd.kill);
        sfxUnload(snd.death);
    };
    loadSounds();

    World world;
    buildWorld(world, cfg);

    sim::Stats stats;
    std::vector<Tracer> tracers;
    float yaw = 0, pitch = 0, shotTimer = 0, hmTimer = 0, hmKind = 0;  // hmKind: 0 hit, 1 head, 2 kill
    float time = 0;
    const sim::V3 eye{0, 1.65f, 0};

    while (!WindowShouldClose()) {
        const float dt = std::min(GetFrameTime(), 0.05f);
        time += dt;

        // ---- hot reload
        if (IsKeyPressed(KEY_F5)) {
            unloadGfx(gfx);
            unloadSounds();
            loadConfig("config.ini", cfg);
            loadGfx(gfx, cfg);
            loadSounds();
            buildWorld(world, cfg);
            tracers.clear();
        }
        if (IsKeyPressed(KEY_R)) stats = sim::Stats{};

        // ---- look
        float degPerCount = cfg.sensitivity * 0.022f;
        if (cfg.cm360 > 0.f && cfg.dpi > 0.f) degPerCount = 360.f / (cfg.cm360 / 2.54f * cfg.dpi);
        Vector2 md = GetMouseDelta();
        yaw += md.x * degPerCount;
        pitch = std::clamp(pitch - md.y * degPerCount, -89.f, 89.f);
        if (yaw > 360.f) yaw -= 360.f;
        if (yaw < -360.f) yaw += 360.f;

        auto dirFrom = [](float yawDeg, float pitchDeg) {
            float y = yawDeg * DEG2RAD, p = pitchDeg * DEG2RAD;
            return sim::V3{std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)};
        };
        const sim::V3 fwd = dirFrom(yaw, pitch);
        const sim::V3 right = sim::normalize(sim::cross(fwd, sim::V3{0, 1, 0}));

        // ---- shoot
        shotTimer = std::max(0.f, shotTimer - dt);
        const bool wantFire = cfg.auto_fire ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (wantFire && shotTimer <= 0.f) {
            shotTimer = 60.f / std::max(1.f, cfg.rpm);
            sim::V3 d = fwd;
            if (cfg.spread > 0.f) {
                d = dirFrom(yaw + world.rng.range(-cfg.spread, cfg.spread), pitch + world.rng.range(-cfg.spread, cfg.spread));
            }
            stats.shots++;
            sim::Hit h = sim::raycast(world.map, world.bots, cfg.bot, eye, d);
            if (audioOk) sfxPlay(snd.shot, cfg.master);
            if (h.bot >= 0) {
                stats.hits++;
                const bool headshot = h.part == sim::Part::Head;
                if (headshot) stats.headshots++;
                const float dmg = cfg.damage * (headshot ? cfg.head_mult : 1.f);
                const bool killed = sim::applyHit(world.bots[static_cast<size_t>(h.bot)], dmg, cfg.bot);
                hmTimer = cfg.hm_time;
                hmKind = killed ? 2.f : (headshot ? 1.f : 0.f);
                if (killed) {
                    stats.kills++;
                    if (audioOk) {
                        sfxPlay(snd.kill, cfg.master);
                        sfxPlay(snd.death, cfg.master);
                    }
                } else if (audioOk) {
                    sfxPlay(headshot ? snd.head : snd.hit, cfg.master);
                }
            }
            if (cfg.tracer) {
                float len = std::min(h.t, wallDistance(eye, d, world.map.half));
                sim::V3 muzzle = eye + right * 0.18f + sim::V3{0, -0.14f, 0} + d * 0.6f;
                tracers.push_back(Tracer{muzzle, eye + d * len, 0.f});
            }
        }

        // ---- simulate
        for (auto& b : world.bots) sim::update(b, dt, world.map, world.rng, cfg.bot);
        hmTimer = std::max(0.f, hmTimer - dt);
        for (auto& t : tracers) t.age += dt;
        tracers.erase(std::remove_if(tracers.begin(), tracers.end(),
                                     [&](const Tracer& t) {
                                         float dist = sim::length(t.b - t.a);
                                         return t.age * cfg.tracer_speed - cfg.tracer_length > dist;
                                     }),
                      tracers.end());

        // ---- draw
        const int sw = GetScreenWidth(), sh = GetScreenHeight();
        const float aspect = static_cast<float>(sw) / static_cast<float>(std::max(1, sh));
        Camera3D cam{};
        cam.position = toRl(eye);
        cam.target = toRl(eye + fwd);
        cam.up = Vector3{0, 1, 0};
        cam.fovy = 2.f * std::atan(std::tan(cfg.hfov * DEG2RAD / 2.f) / aspect) * RAD2DEG;
        cam.projection = CAMERA_PERSPECTIVE;

        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangleGradientV(0, 0, sw, sh, cfg.sky_top, cfg.sky_bottom);

        BeginMode3D(cam);
        if (gfx.haveSky) {
            rlDrawRenderBatchActive();
            rlSetCullFace(RL_CULL_FACE_FRONT);  // we are inside the sphere
            DrawModel(gfx.sky, cam.position, 200.f, WHITE);
            rlSetCullFace(RL_CULL_FACE_BACK);
        }
        const float half = world.map.half;
        DrawPlane(Vector3{0, 0, 0}, Vector2{half * 2, half * 2}, cfg.floor_col);
        DrawGrid(static_cast<int>(half), 2.f);
        const float wallH = 8.f;
        DrawCube(Vector3{0, wallH / 2, -half - 0.25f}, half * 2 + 1, wallH, 0.5f, cfg.wall_col);
        DrawCube(Vector3{0, wallH / 2, half + 0.25f}, half * 2 + 1, wallH, 0.5f, cfg.wall_col);
        DrawCube(Vector3{-half - 0.25f, wallH / 2, 0}, 0.5f, wallH, half * 2 + 1, cfg.wall_col);
        DrawCube(Vector3{half + 0.25f, wallH / 2, 0}, 0.5f, wallH, half * 2 + 1, cfg.wall_col);
        for (const auto& p : world.map.pillars) DrawCylinder(Vector3{p.x, 0, p.z}, p.r, p.r, p.h, 24, cfg.pillar_col);

        rlDrawRenderBatchActive();
        for (const auto& b : world.bots)
            if (b.alive) drawBot(gfx, cfg, b, cam.position, time);

        if (cfg.tracer) {
            rlDrawRenderBatchActive();
            for (const auto& t : tracers) {
                sim::V3 dir = t.b - t.a;
                float dist = sim::length(dir);
                if (dist < 1e-4f) continue;
                dir = dir * (1.f / dist);
                float headD = std::min(dist, t.age * cfg.tracer_speed);
                float tailD = std::max(0.f, headD - cfg.tracer_length);
                if (headD <= tailD) continue;
                DrawCylinderEx(toRl(t.a + dir * tailD), toRl(t.a + dir * headD), cfg.tracer_width, cfg.tracer_width, 6, cfg.tracer_color);
            }
        }
        EndMode3D();

        // ---- HUD
        const float cx = static_cast<float>(sw) / 2.f, cy = static_cast<float>(sh) / 2.f;
        if (cfg.crosshair) drawCrosshair(cx, cy, cfg);
        if (cfg.hitmarker && hmTimer > 0.f) {
            float k = hmTimer / std::max(0.001f, cfg.hm_time);
            Color c = hmKind == 2.f ? cfg.hm_kill : (hmKind == 1.f ? cfg.hm_head : cfg.hm_color);
            c.a = static_cast<unsigned char>(255.f * std::clamp(k * 1.5f, 0.f, 1.f));
            float grow = hmKind == 2.f ? 1.4f : 1.f;
            drawHitMarker(cx, cy, cfg.hm_size * grow, cfg.hm_gap, cfg.hm_thick, c);
        }
        DrawText(TextFormat("acc %.1f%%   hits %d/%d   hs %d   kills %d", stats.accuracy(), stats.hits, stats.shots, stats.headshots, stats.kills), 14, sh - 52, 20, WHITE);
        DrawText("LMB fire   R reset stats   F5 reload config   ESC quit", 14, sh - 26, 16, Fade(WHITE, 0.6f));
        DrawText(TextFormat("%d fps", GetFPS()), 14, 10, 18, Fade(WHITE, 0.7f));
        EndDrawing();
    }

    unloadSounds();
    unloadGfx(gfx);
    if (audioOk) CloseAudioDevice();
    CloseWindow();
    return 0;
}
