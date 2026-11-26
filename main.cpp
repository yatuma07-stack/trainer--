// Copyright (c) 2026 yatuma. All rights reserved. See LICENSE.
// Developer: yatuma
// Customizable aim trainer. raylib (OpenGL 3.3) + C++17.
// Game rules live in sim.h / strafe.h (unit tested). This file is window, rendering, audio, input, menus, config.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ini.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim.h"
#include "strafe.h"

namespace {

constexpr const char* kDeveloper = "yatuma";

// ====================================================================== config

struct Aura {
    bool outline = true;
    Color outline_color{80, 200, 255, 255};
    float outline_thickness = 0.03f;  // meters
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
    // mouse: degrees per count = sensitivity * 0.022 (Source style), or cm360 + dpi when cm360 > 0.
    float sensitivity = 1.0f, cm360 = 0.f, dpi = 800.f;
    // weapons: switch with 1 / 2
    struct Weapon {
        std::string name = "Weapon";
        float damage = 34.f, head_mult = 2.0f, rpm = 600.f, spread = 0.f;
        bool auto_fire = true;
        int pellets = 1;             // >1 = shotgun-style, several pellets per shot
        float ads_fov = 55.f;        // horizontal FOV while aiming down sights
        float ads_delay_ms = 150.f;  // input lag before ADS actually kicks in (and lets go)
        float ads_spread_mult = 0.35f, ads_sens_mult = 0.7f;
        Color tracer_color{255, 230, 120, 255};
    };
    std::vector<Weapon> weapons;
    size_t weapon_index = 0;
    // movement (WASD) and the A/D timing readout
    bool strafe_enabled = true, strafe_hud = true;
    float move_speed = 5.5f;
    float accel_ms = 60.f, decel_ms = 60.f;
    float move_spread = 3.0f;
    float strafe_target_ms = 30.f, strafe_window_ms = 600.f;
    // tracer
    bool tracer = true;
    float tracer_width = 0.012f, tracer_length = 4.f, tracer_speed = 220.f;
    // hit marker
    bool hitmarker = true;
    Color hm_color{255, 255, 255, 255}, hm_head{255, 90, 60, 255}, hm_kill{255, 40, 40, 255};
    float hm_size = 9.f, hm_gap = 4.f, hm_thick = 2.f, hm_time = 0.18f;
    // crosshair
    bool crosshair = true, ch_dot = true;
    Color ch_color{60, 255, 120, 255};
    float ch_size = 7.f, ch_gap = 4.f, ch_thick = 2.f;
    // world / graphics
    Color sky_top{70, 120, 190, 255}, sky_bottom{190, 215, 235, 255};
    Color floor_col{74, 80, 90, 255}, grid_col{40, 44, 52, 255}, pillar_col{120, 126, 140, 255}, wall_col{52, 58, 68, 255};
    sim::V3 sun{0.45f, 0.8f, 0.35f};
    Color sun_col{255, 240, 215, 255};
    float fog = 0.012f;  // density, 0 = off
    bool shadows = true;
    float tile = 2.0f;  // floor tile size in meters
    std::string sky_image = "sky/panorama.png";  // optional equirectangular image
    std::string map_file = "maps/arena.map";
    // bots
    int bot_count = 6;
    sim::BotCfg bot;
    Color bot_color{200, 70, 70, 255}, bot_pants{45, 50, 66, 255}, bot_skin{224, 186, 152, 255};
    std::string bot_model = "models/bot.glb";  // optional glTF, scaled to bot height
    float bot_model_yaw = 0.f;                 // rotate your model so it faces its walking direction
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
    if (!i.load(path)) std::printf("config: %s not found, using defaults\n", path.c_str());

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

    c.weapons.clear();
    for (const char* key : {"weapon1", "weapon2"}) {
        Config::Weapon w;
        const std::string k = key;
        if (k == "weapon1") {
            w.name = "Carbine Mk2";
            w.damage = 34.f; w.rpm = 700.f; w.pellets = 1; w.spread = 0.25f;
            w.ads_fov = 50.f; w.ads_delay_ms = 150.f; w.ads_spread_mult = 0.3f; w.ads_sens_mult = 0.65f;
        } else {
            w.name = "Heavy Shotgun";
            w.damage = 18.f; w.rpm = 70.f; w.pellets = 8; w.spread = 4.0f; w.auto_fire = false;
            w.ads_fov = 65.f; w.ads_delay_ms = 220.f; w.ads_spread_mult = 0.6f; w.ads_sens_mult = 0.8f;
            w.tracer_color = Color{255, 160, 60, 255};
            w.head_mult = 1.5f;
        }
        w.name = i.str(k + ".name", w.name);
        w.damage = i.num(k + ".damage", w.damage);
        w.head_mult = i.num(k + ".head_multiplier", w.head_mult);
        w.rpm = i.num(k + ".rpm", w.rpm);
        w.spread = i.num(k + ".spread", w.spread);
        w.auto_fire = i.boolean(k + ".auto", w.auto_fire);
        w.pellets = std::max(1, i.integer(k + ".pellets", w.pellets));
        w.ads_fov = i.num(k + ".ads_fov", w.ads_fov);
        w.ads_delay_ms = std::max(0.f, i.num(k + ".ads_delay_ms", w.ads_delay_ms));
        w.ads_spread_mult = i.num(k + ".ads_spread_mult", w.ads_spread_mult);
        w.ads_sens_mult = i.num(k + ".ads_sens_mult", w.ads_sens_mult);
        w.tracer_color = readColor(i, k + ".tracer_color", w.tracer_color);
        c.weapons.push_back(w);
    }
    c.weapon_index = std::min(c.weapon_index, c.weapons.size() - 1);

    c.tracer = i.boolean("tracer.enabled", c.tracer);
    c.tracer_width = i.num("tracer.width", c.tracer_width);
    c.tracer_length = i.num("tracer.length", c.tracer_length);
    c.tracer_speed = i.num("tracer.speed", c.tracer_speed);

    c.strafe_enabled = i.boolean("movement.enabled", c.strafe_enabled);
    c.strafe_hud = i.boolean("movement.show_gap", c.strafe_hud);
    c.move_speed = std::max(0.1f, i.num("movement.speed", c.move_speed));
    c.accel_ms = std::max(0.f, i.num("movement.accel_ms", c.accel_ms));
    c.decel_ms = std::max(0.f, i.num("movement.decel_ms", c.decel_ms));
    c.move_spread = i.num("movement.move_spread", c.move_spread);
    c.strafe_target_ms = i.num("movement.target_ms", c.strafe_target_ms);
    c.strafe_window_ms = i.num("movement.window_ms", c.strafe_window_ms);

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
    c.tile = std::max(0.25f, i.num("world.tile", c.tile));
    {
        auto s = i.list("graphics.sun_dir");
        if (s.size() >= 3) c.sun = sim::normalize({s[0], s[1], s[2]});
    }
    c.sun_col = readColor(i, "graphics.sun_color", c.sun_col);
    c.fog = std::max(0.f, i.num("graphics.fog", c.fog));
    c.shadows = i.boolean("graphics.shadows", c.shadows);

    c.bot_count = std::clamp(i.integer("bots.count", c.bot_count), 0, 64);
    c.bot_color = readColor(i, "bots.color", c.bot_color);
    c.bot_pants = readColor(i, "bots.pants_color", c.bot_pants);
    c.bot_skin = readColor(i, "bots.skin_color", c.bot_skin);
    c.bot_model = i.str("bots.model", c.bot_model);
    c.bot_model_yaw = i.num("bots.model_yaw", c.bot_model_yaw);
    sim::BotCfg& b = c.bot;
    b.height = i.num("bots.height", b.height);
    b.radius = i.num("bots.radius", b.radius);
    b.head_radius = i.num("bots.head_radius", b.head_radius);
    b.hit_radius = i.num("bots.hit_radius", b.hit_radius);
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

// ---------------------------------------------------------------- maps
// Map file lines: "size <half>", "pillar <x> <z> <radius> <height>", "box <x> <z> <halfW> <halfD> <height>", '#' comments.

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
            if (ss >> p.x >> p.z >> p.r >> p.h && p.r > 0.f && p.h > 0.f) out.pillars.push_back(p);
        } else if (cmd == "box") {
            sim::Box b;
            if (ss >> b.x >> b.z >> b.hw >> b.hd >> b.h && b.hw > 0.f && b.hd > 0.f && b.h > 0.f) out.boxes.push_back(b);
        }
    }
    m = out;
    return true;
}

bool saveMap(const std::string& path, const sim::Map& m) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "# Aim trainer map. Edit in game with F1.\n";
    f << "size " << m.half << "\n";
    for (const auto& p : m.pillars) f << "pillar " << p.x << " " << p.z << " " << p.r << " " << p.h << "\n";
    for (const auto& b : m.boxes) f << "box " << b.x << " " << b.z << " " << b.hw << " " << b.hd << " " << b.h << "\n";
    return static_cast<bool>(f);
}

sim::Map defaultMap() {
    sim::Map m;
    m.half = 30;
    m.pillars = {{-12, -10, 1.5f, 4}, {10, -14, 2.0f, 5}, {0, 8, 1.2f, 3}, {-15, 12, 2.5f, 6}, {16, 10, 1.5f, 4}, {3, -2, 1.0f, 3}};
    m.boxes = {{-4, -18, 3.0f, 0.6f, 3}, {18, -2, 0.6f, 3.5f, 3}};
    return m;
}

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

// ====================================================================== rendering

// One shader for the whole scene. Modes: 0 lit, 1 flat colour (outline), 2 blob shadow, 3 procedural sky,
// 4 tiled floor, 5 panorama sky. Lighting: hemisphere ambient + sun with soft wrap + specular + rim + fog.
const char* kVert = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float uExtrude;
out vec3 vN;
out vec3 vP;
out vec2 vUV;
void main() {
    vec4 wp = matModel * vec4(vertexPosition, 1.0);
    vec3 wn = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    wp.xyz += wn * uExtrude;
    vP = wp.xyz;
    vN = wn;
    vUV = vertexTexCoord;
    gl_Position = matProjection * matView * wp;
}
)";

const char* kFrag = R"(#version 330
in vec3 vN;
in vec3 vP;
in vec2 vUV;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float uMode;
uniform vec3 uView;
uniform vec3 uBase;
uniform vec3 uRim;
uniform vec3 uSun;
uniform vec3 uSunCol;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uAmbSky;
uniform vec3 uAmbGnd;
uniform vec3 uFogCol;
uniform vec3 uGrid;
uniform float uRimPow;
uniform float uRimInt;
uniform float uAlpha;
uniform float uAO;
uniform float uSpec;
uniform float uFog;
uniform float uTile;
void main() {
    int mode = int(uMode + 0.5);
    vec3 toEye = uView - vP;
    float dist = length(toEye);
    vec3 V = toEye / max(dist, 1e-4);
    if (mode == 1) { finalColor = vec4(uBase, 1.0); return; }
    if (mode == 2) {
        float r = length(vUV - vec2(0.5)) * 2.0;
        finalColor = vec4(0.0, 0.0, 0.0, uAlpha * (1.0 - smoothstep(0.1, 1.0, r)));
        return;
    }
    if (mode == 3 || mode == 5) {
        vec3 dir = normalize(vP - uView);
        vec3 c;
        if (mode == 5) {
            vec2 uv = vec2(atan(dir.z, dir.x) / 6.2831853 + 0.5, acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265);
            c = texture(texture0, uv).rgb;
        } else {
            float h = clamp(dir.y, 0.0, 1.0);
            c = mix(uHorizon, uZenith, pow(h, 0.5));
            c = mix(c, uHorizon * 0.75, clamp(-dir.y * 4.0, 0.0, 1.0));
            float sd = max(dot(dir, normalize(uSun)), 0.0);
            c += uSunCol * (pow(sd, 900.0) * 4.0 + pow(sd, 16.0) * 0.18);
        }
        finalColor = vec4(c, 1.0);
        return;
    }
    vec3 N = normalize(vN);
    vec3 base = uBase * texture(texture0, vUV).rgb;
    if (mode == 4) {
        vec2 g = vP.xz / uTile;
        float ck = mod(floor(g.x) + floor(g.y), 2.0);
        base = mix(uBase * 0.9, uBase * 1.1, ck);
        vec2 f = abs(fract(g - 0.5) - 0.5) / max(fwidth(g), vec2(1e-4));
        float line = 1.0 - clamp(min(f.x, f.y), 0.0, 1.0);
        base = mix(base, uGrid, line * 0.55);
    }
    vec3 L = normalize(uSun);
    float ndl = dot(N, L);
    float diff = max((ndl + 0.3) / 1.3, 0.0);
    vec3 hemi = mix(uAmbGnd, uAmbSky, N.y * 0.5 + 0.5);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 48.0) * uSpec * step(0.0, ndl);
    vec3 col = base * (hemi + uSunCol * diff) + uSunCol * spec;
    col *= mix(uAO, 1.0, smoothstep(0.0, 1.0, vP.y));
    col += uRim * pow(1.0 - clamp(dot(N, V), 0.0, 1.0), uRimPow) * uRimInt;
    float f = clamp(1.0 - exp(-dist * uFog), 0.0, 1.0);
    col = mix(col, uFogCol, f);
    finalColor = vec4(col, uAlpha);
}
)";

struct Gfx {
    Shader sh{};
    int lMode = -1, lView = -1, lBase = -1, lRim = -1, lSun = -1, lSunCol = -1, lHorizon = -1, lZenith = -1, lAmbSky = -1,
        lAmbGnd = -1, lFogCol = -1, lGrid = -1, lRimPow = -1, lRimInt = -1, lAlpha = -1, lAO = -1, lSpec = -1, lFog = -1,
        lTile = -1, lExtrude = -1;
    Model cyl{}, sphere{}, cube{}, plane{}, sky{}, custom{};
    bool haveCustom = false, haveSky = false;
    float customScale = 1.f, customYOffset = 0.f;
    Matrix customBase = MatrixIdentity();
    Texture2D skyTex{};
};

Vector3 colorVec(Color c) { return Vector3{c.r / 255.f, c.g / 255.f, c.b / 255.f}; }
Vector3 vscale(Vector3 a, float s) { return Vector3{a.x * s, a.y * s, a.z * s}; }
Vector3 vadd(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
void setF(Gfx& g, int loc, float v) { SetShaderValue(g.sh, loc, &v, SHADER_UNIFORM_FLOAT); }
void setV3(Gfx& g, int loc, Vector3 v) { SetShaderValue(g.sh, loc, &v, SHADER_UNIFORM_VEC3); }

Model makeModel(const Gfx& g, Mesh mesh) {
    Model m = LoadModelFromMesh(mesh);
    m.materials[0].shader = g.sh;
    return m;
}

void loadGfx(Gfx& g, const Config& c) {
    g.sh = LoadShaderFromMemory(kVert, kFrag);
    auto loc = [&](const char* n) { return GetShaderLocation(g.sh, n); };
    g.lMode = loc("uMode"); g.lView = loc("uView"); g.lBase = loc("uBase"); g.lRim = loc("uRim");
    g.lSun = loc("uSun"); g.lSunCol = loc("uSunCol"); g.lHorizon = loc("uHorizon"); g.lZenith = loc("uZenith");
    g.lAmbSky = loc("uAmbSky"); g.lAmbGnd = loc("uAmbGnd"); g.lFogCol = loc("uFogCol"); g.lGrid = loc("uGrid");
    g.lRimPow = loc("uRimPow"); g.lRimInt = loc("uRimInt"); g.lAlpha = loc("uAlpha"); g.lAO = loc("uAO");
    g.lSpec = loc("uSpec"); g.lFog = loc("uFog"); g.lTile = loc("uTile"); g.lExtrude = loc("uExtrude");

    g.cyl = makeModel(g, GenMeshCylinder(1.f, 1.f, 24));  // base at y = 0, top at y = 1
    g.sphere = makeModel(g, GenMeshSphere(1.f, 20, 20));
    g.cube = makeModel(g, GenMeshCube(1.f, 1.f, 1.f));
    g.plane = makeModel(g, GenMeshPlane(1.f, 1.f, 1, 1));
    g.sky = makeModel(g, GenMeshSphere(1.f, 48, 48));

    g.haveSky = false;
    if (FileExists(c.sky_image.c_str())) {
        g.skyTex = LoadTexture(c.sky_image.c_str());
        if (g.skyTex.id != 0) {
            SetTextureFilter(g.skyTex, TEXTURE_FILTER_BILINEAR);
            g.sky.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g.skyTex;
            g.haveSky = true;
        }
    }

    g.haveCustom = false;
    if (FileExists(c.bot_model.c_str())) {
        g.custom = LoadModel(c.bot_model.c_str());
        if (g.custom.meshCount > 0) {
            for (int i = 0; i < g.custom.materialCount; i++) g.custom.materials[i].shader = g.sh;
            BoundingBox bb = GetModelBoundingBox(g.custom);
            float h = bb.max.y - bb.min.y;
            if (h > 1e-4f) {
                g.customBase = g.custom.transform;
                g.customScale = c.bot.height / h;
                g.customYOffset = -bb.min.y * g.customScale;
                g.haveCustom = true;
            }
        }
        if (!g.haveCustom) std::printf("bots.model: %s has no usable geometry, using the built-in character\n", c.bot_model.c_str());
    }
}

void unloadGfx(Gfx& g) {
    // UnloadModel frees meshes and map arrays only; the shader and textures are ours to free.
    UnloadModel(g.cyl);
    UnloadModel(g.sphere);
    UnloadModel(g.cube);
    UnloadModel(g.plane);
    UnloadModel(g.sky);
    if (g.haveCustom) {
        for (int i = 0; i < g.custom.materialCount; i++)
            for (int k = 0; k <= MATERIAL_MAP_BRDF; k++)
                if (g.custom.materials[i].maps[k].texture.id > 1) UnloadTexture(g.custom.materials[i].maps[k].texture);
        UnloadModel(g.custom);
    }
    if (g.haveSky) UnloadTexture(g.skyTex);
    UnloadShader(g.sh);
    g = Gfx{};
}

struct Look {
    float mode = 0.f;
    Vector3 base{1, 1, 1};
    float rimInt = 0.f, ao = 1.f, spec = 0.08f, alpha = 1.f, extrude = 0.f;
};

void setLook(Gfx& g, const Look& l) {
    setF(g, g.lMode, l.mode);
    setV3(g, g.lBase, l.base);
    setF(g, g.lRimInt, l.rimInt);
    setF(g, g.lAO, l.ao);
    setF(g, g.lSpec, l.spec);
    setF(g, g.lAlpha, l.alpha);
    setF(g, g.lExtrude, l.extrude);
}

void drawModel(Model& m, const Matrix& t) {
    m.transform = t;
    DrawModel(m, Vector3{0, 0, 0}, 1.0f, WHITE);
}

Matrix compose(Vector3 s, Vector3 t, const Matrix& after) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixTranslate(t.x, t.y, t.z)), after);
}

// Per-frame scene constants (sun, sky, fog, ambient).
void setScene(Gfx& g, const Config& c, Vector3 view, bool editor) {
    const Vector3 horizon = colorVec(c.sky_bottom), zenith = colorVec(c.sky_top);
    setV3(g, g.lView, view);
    setV3(g, g.lSun, Vector3{c.sun.x, c.sun.y, c.sun.z});
    setV3(g, g.lSunCol, colorVec(c.sun_col));
    setV3(g, g.lHorizon, horizon);
    setV3(g, g.lZenith, zenith);
    setV3(g, g.lAmbSky, vadd(vscale(horizon, 0.55f), Vector3{0.08f, 0.09f, 0.11f}));
    setV3(g, g.lAmbGnd, vscale(colorVec(c.floor_col), 0.6f));
    setV3(g, g.lFogCol, horizon);
    setV3(g, g.lGrid, colorVec(c.grid_col));
    setF(g, g.lFog, editor ? 0.f : c.fog);
    setF(g, g.lTile, c.tile);
    setF(g, g.lRimPow, 2.5f);
}

// ---------------------------------------------------------------- characters

struct Part {
    Model* m;
    Matrix t;
    Color c;
};

struct BotViz {
    sim::V3 last;
    float phase = 0.f, amp = 0.f;
    bool init = false;
};

void updateViz(std::vector<BotViz>& viz, const std::vector<sim::Bot>& bots, float dt) {
    viz.resize(bots.size());
    for (size_t i = 0; i < bots.size(); i++) {
        BotViz& v = viz[i];
        const sim::Bot& b = bots[i];
        const float d = sim::length(b.pos - v.last);
        if (!v.init || !b.alive || d > 1.5f) {  // first frame, dead, or respawn teleport
            v.last = b.pos;
            v.init = b.alive;
            v.amp = 0.f;
            continue;
        }
        const float speed = d / std::max(dt, 1e-4f);
        const float target = std::clamp(speed / 3.5f, 0.f, 1.f);
        v.amp += (target - v.amp) * std::min(1.f, dt * 10.f);
        v.phase += speed * dt * 2.4f;
        v.last = b.pos;
    }
}

// A jointed mannequin: head, neck, torso, pelvis, swinging arms and legs, hands and shoes.
// Built from smooth capsules and ellipsoids so the aura outline wraps it cleanly.
void buildBotParts(Gfx& g, const Config& cfg, const sim::Bot& b, const BotViz& v, std::vector<Part>& out) {
    const sim::BotCfg& c = cfg.bot;
    const Matrix root = MatrixMultiply(MatrixRotateY(b.yaw * DEG2RAD), MatrixTranslate(b.pos.x, b.pos.y, b.pos.z));
    if (g.haveCustom) {
        Matrix t = MatrixMultiply(g.customBase, MatrixScale(g.customScale, g.customScale, g.customScale));
        t = MatrixMultiply(t, MatrixRotateY(cfg.bot_model_yaw * DEG2RAD));
        t = MatrixMultiply(t, MatrixTranslate(0, g.customYOffset, 0));
        out.push_back({&g.custom, MatrixMultiply(t, root), WHITE});
        return;
    }
    const float H = c.height, hr = c.head_radius;
    const float hipY = H * 0.50f, shY = H * 0.81f;
    const Color shirt = cfg.bot_color, pants = cfg.bot_pants, skin = cfg.bot_skin, shoe{28, 28, 32, 255};
    const float bob = std::fabs(std::sin(v.phase)) * 0.025f * v.amp;
    const Matrix body = MatrixMultiply(MatrixTranslate(0, bob, 0), root);

    auto prim = [&](Model* m, Vector3 s, Vector3 p, Color col) { out.push_back({m, compose(s, p, body), col}); };
    prim(&g.sphere, {0.205f, (shY - hipY) * 0.56f, 0.125f}, {0, (hipY + shY) * 0.5f, 0}, shirt);  // torso
    prim(&g.sphere, {0.185f, 0.13f, 0.12f}, {0, hipY, 0}, pants);                                    // pelvis
    prim(&g.cyl, {0.052f, 0.12f, 0.052f}, {0, shY - 0.02f, 0}, skin);                                // neck
    prim(&g.sphere, {hr, hr * 1.12f, hr}, {0, H - hr * 1.12f + bob, 0}, skin);                        // head (hitbox centre matches)

    auto limb = [&](float x, float topY, float len, float r, float swing, Color col, Color endCol, Vector3 endScale, Vector3 endAt) {
        Matrix joint = MatrixMultiply(MatrixMultiply(MatrixRotateX(swing), MatrixTranslate(x, topY, 0)), body);
        out.push_back({&g.cyl, MatrixMultiply(MatrixMultiply(MatrixScale(r, len, r), MatrixTranslate(0, -len, 0)), joint), col});
        out.push_back({&g.sphere, MatrixMultiply(MatrixScale(r * 1.18f, r * 1.18f, r * 1.18f), joint), col});
        out.push_back({&g.sphere, compose(endScale, endAt, joint), endCol});
    };
    const float legSwing = std::sin(v.phase) * 0.75f * v.amp;
    const float legLen = hipY - 0.05f, armLen = H * 0.33f;
    limb(-0.105f, hipY, legLen, 0.078f, legSwing, pants, shoe, {0.072f, 0.05f, 0.14f}, {0, -legLen + 0.03f, 0.06f});
    limb(0.105f, hipY, legLen, 0.078f, -legSwing, pants, shoe, {0.072f, 0.05f, 0.14f}, {0, -legLen + 0.03f, 0.06f});
    limb(-0.265f, shY - 0.05f, armLen, 0.052f, -legSwing * 0.9f, shirt, skin, {0.058f, 0.058f, 0.058f}, {0, -armLen, 0});
    limb(0.265f, shY - 0.05f, armLen, 0.052f, legSwing * 0.9f, shirt, skin, {0.058f, 0.058f, 0.058f}, {0, -armLen, 0});
}

Color mixColor(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t); };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255};
}

void drawBot(Gfx& g, const Config& cfg, const sim::Bot& b, const BotViz& v, float time) {
    static std::vector<Part> parts;
    parts.clear();
    buildBotParts(g, cfg, b, v, parts);

    const Aura& a = cfg.aura;
    const float flash = std::clamp(b.hitFlash / sim::kHitFlash, 0.f, 1.f);
    const Color aura = a.rainbow ? ColorFromHSV(std::fmod(time * a.rainbow_speed, 1.f) * 360.f, 0.85f, 1.f) : a.rim_color;
    const Color outline = a.rainbow ? aura : a.outline_color;
    const float pulse = a.pulse_speed > 0.f ? 0.65f + 0.35f * std::sin(time * a.pulse_speed * 6.2831853f) : 1.f;

    if (a.outline) {
        rlDrawRenderBatchActive();
        if (a.through_walls) rlDisableDepthTest();
        rlSetCullFace(RL_CULL_FACE_FRONT);
        Look l;
        l.mode = 1.f;
        l.base = colorVec(mixColor(outline, a.hit_color, flash));
        l.extrude = a.outline_thickness;
        for (Part& p : parts) {
            setLook(g, l);
            drawModel(*p.m, p.t);
        }
        rlSetCullFace(RL_CULL_FACE_BACK);
        if (a.through_walls) rlEnableDepthTest();
    }

    setV3(g, g.lRim, colorVec(aura));
    setF(g, g.lRimPow, a.rim_power);
    Look l;
    l.ao = 0.72f;
    l.spec = 0.22f;
    l.rimInt = a.rim ? a.rim_intensity * pulse * (1.f + flash) : 0.f;
    for (Part& p : parts) {
        l.base = colorVec(mixColor(p.c, a.hit_color, flash * 0.75f));
        setLook(g, l);
        drawModel(*p.m, p.t);
    }
    setF(g, g.lRimPow, 2.5f);
}

// ---------------------------------------------------------------- world

void drawWorld(Gfx& g, const Config& cfg, const sim::Map& map, const std::vector<sim::Bot>& bots, Vector3 camPos, bool editor) {
    const float half = map.half;
    if (!editor) {
        Look l;
        l.mode = g.haveSky ? 5.f : 3.f;
        setLook(g, l);
        rlDrawRenderBatchActive();
        rlSetCullFace(RL_CULL_FACE_FRONT);  // we are inside the sphere
        drawModel(g.sky, compose({400.f, 400.f, 400.f}, camPos, MatrixIdentity()));
        rlSetCullFace(RL_CULL_FACE_BACK);
    }

    Look floorL;
    floorL.mode = 4.f;
    floorL.base = colorVec(cfg.floor_col);
    floorL.spec = 0.05f;
    setLook(g, floorL);
    drawModel(g.plane, MatrixScale(half * 2.f, 1.f, half * 2.f));

    Look solid;
    solid.base = colorVec(cfg.wall_col);
    solid.ao = 0.8f;
    solid.spec = 0.06f;
    setLook(g, solid);
    const float wh = 8.f, wt = 0.5f;
    drawModel(g.cube, compose({half * 2 + wt * 2, wh, wt}, {0, wh / 2, -half - wt / 2}, MatrixIdentity()));
    drawModel(g.cube, compose({half * 2 + wt * 2, wh, wt}, {0, wh / 2, half + wt / 2}, MatrixIdentity()));
    drawModel(g.cube, compose({wt, wh, half * 2}, {-half - wt / 2, wh / 2, 0}, MatrixIdentity()));
    drawModel(g.cube, compose({wt, wh, half * 2}, {half + wt / 2, wh / 2, 0}, MatrixIdentity()));

    Look obj;
    obj.base = colorVec(cfg.pillar_col);
    obj.ao = 0.55f;
    obj.spec = 0.18f;
    setLook(g, obj);
    for (const auto& p : map.pillars) drawModel(g.cyl, compose({p.r, p.h, p.r}, {p.x, 0, p.z}, MatrixIdentity()));
    for (const auto& b : map.boxes) drawModel(g.cube, compose({b.hw * 2, b.h, b.hd * 2}, {b.x, b.h / 2, b.z}, MatrixIdentity()));

    if (cfg.shadows) {  // soft blob shadows on the floor
        Look sh;
        sh.mode = 2.f;
        sh.alpha = 0.34f;
        setLook(g, sh);
        for (const auto& p : map.pillars) drawModel(g.plane, compose({p.r * 3.2f, 1.f, p.r * 3.2f}, {p.x, 0.02f, p.z}, MatrixIdentity()));
        for (const auto& b : map.boxes) drawModel(g.plane, compose({b.hw * 2 + 2.2f, 1.f, b.hd * 2 + 2.2f}, {b.x, 0.02f, b.z}, MatrixIdentity()));
        sh.alpha = 0.5f;
        setLook(g, sh);
        for (const auto& b : bots)
            if (b.alive) drawModel(g.plane, compose({1.5f, 1.f, 1.5f}, {b.pos.x, 0.025f, b.pos.z}, MatrixIdentity()));
    }
}

// ====================================================================== misc helpers

struct Tracer {
    sim::V3 a, b;
    float age = 0;
    Color col{255, 230, 120, 255};
};

float wallDistance(sim::V3 o, sim::V3 d, float half) {  // distance along a ray to the arena boundary
    float t = 1e9f;
    if (std::fabs(d.x) > 1e-6f) t = std::min(t, ((d.x > 0 ? half : -half) - o.x) / d.x);
    if (std::fabs(d.z) > 1e-6f) t = std::min(t, ((d.z > 0 ? half : -half) - o.z) / d.z);
    return std::max(0.f, t);
}

Vector3 toRl(sim::V3 v) { return Vector3{v.x, v.y, v.z}; }

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

// ---------------------------------------------------------------- tiny immediate-mode UI

bool uiButton(Rectangle r, const char* text) {
    const Vector2 m = GetMousePosition();
    const bool hot = CheckCollisionPointRec(m, r);
    DrawRectangleRec(r, hot ? Color{70, 110, 170, 255} : Color{45, 52, 66, 255});
    DrawRectangleLinesEx(r, 1.f, Color{110, 125, 150, 255});
    DrawText(text, static_cast<int>(r.x + (r.width - static_cast<float>(MeasureText(text, 20))) / 2), static_cast<int>(r.y + (r.height - 20) / 2), 20, WHITE);
    return hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// Drag the bar, or hover and use the mouse wheel for single-step fine tuning.
bool uiSlider(int id, Rectangle r, const char* label, float& v, float lo, float hi, float step, const char* valueText, bool enabled = true) {
    static int active = -1;
    const Vector2 m = GetMousePosition();
    const Rectangle bar{r.x, r.y + 24, r.width, 14};
    const bool over = CheckCollisionPointRec(m, Rectangle{r.x, r.y, r.width, r.height});
    bool changed = false;
    if (enabled) {
        if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.y >= bar.y - 6) active = id;
        if (active == id) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                float t = std::clamp((m.x - bar.x) / bar.width, 0.f, 1.f);
                float nv = lo + t * (hi - lo);
                nv = std::round(nv / step) * step;
                if (nv != v) { v = std::clamp(nv, lo, hi); changed = true; }
            } else {
                active = -1;
            }
        }
        const float wheel = over ? GetMouseWheelMove() : 0.f;
        if (wheel != 0.f) { v = std::clamp(v + wheel * step, lo, hi); changed = true; }
    }
    const float t = std::clamp((v - lo) / (hi - lo), 0.f, 1.f);
    const Color txt = enabled ? WHITE : Color{130, 136, 148, 255};
    DrawText(label, static_cast<int>(r.x), static_cast<int>(r.y), 18, txt);
    DrawText(valueText, static_cast<int>(r.x + r.width) - MeasureText(valueText, 18), static_cast<int>(r.y), 18, enabled ? Color{140, 220, 255, 255} : txt);
    DrawRectangleRec(bar, Color{30, 34, 44, 255});
    DrawRectangleRec(Rectangle{bar.x, bar.y, bar.width * t, bar.height}, enabled ? Color{70, 140, 220, 255} : Color{70, 76, 88, 255});
    DrawRectangleRec(Rectangle{bar.x + bar.width * t - 4, bar.y - 3, 8, bar.height + 6}, enabled ? WHITE : Color{150, 156, 168, 255});
    return changed;
}

std::string fmtNum(float v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%g", static_cast<double>(v));
    return b;
}

// -1 = nothing within reach. Otherwise the index, and isBox says which list it belongs to.
int pickObject(const sim::Map& m, float gx, float gz, bool& isBox) {
    float best = 1e9f;
    int idx = -1;
    for (size_t i = 0; i < m.pillars.size(); i++) {
        const auto& p = m.pillars[i];
        float d = std::max(0.f, std::sqrt((gx - p.x) * (gx - p.x) + (gz - p.z) * (gz - p.z)) - p.r);
        if (d < best) { best = d; idx = static_cast<int>(i); isBox = false; }
    }
    for (size_t i = 0; i < m.boxes.size(); i++) {
        const auto& b = m.boxes[i];
        float dx = std::max(0.f, std::fabs(gx - b.x) - b.hw), dz = std::max(0.f, std::fabs(gz - b.z) - b.hd);
        float d = std::sqrt(dx * dx + dz * dz);
        if (d < best) { best = d; idx = static_cast<int>(i); isBox = true; }
    }
    return best <= 1.0f ? idx : -1;
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
    InitWindow(cfg.width, cfg.height, TextFormat("Aim Trainer -- by %s", kDeveloper));
    SetExitKey(KEY_NULL);  // ESC opens the settings menu instead of quitting
    rlSetClipPlanes(0.05, 800.0);
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
    std::vector<BotViz> viz;

    sim::Stats stats;
    std::vector<Tracer> tracers;
    float yaw = 0, pitch = 0, shotTimer = 0, hmTimer = 0, hmKind = 0;  // hmKind: 0 hit, 1 head, 2 kill
    float time = 0, adsT = 0;
    sim::V3 eye{0, 1.65f, 0};
    float velR = 0, velF = 0;
    strafe::Tracker strafeTrk;

    bool menuOpen = false, quitRequested = false, cursorFree = false, skipLook = false;
    std::string toast;
    float toastT = 0;

    // map editor
    bool editMode = false, snapOn = true;
    int tool = 0;  // 0 pillar, 1 block
    float editR = 1.2f, editW = 3.f, editD = 1.5f, editH = 3.5f;
    float editX = 0, editZ = 0, editView = 70.f;
    std::vector<sim::Map> undo;
    auto pushUndo = [&]() {
        undo.push_back(world.map);
        if (undo.size() > 64) undo.erase(undo.begin());
    };

    while (!WindowShouldClose() && !quitRequested) {
        const float dt = std::min(GetFrameTime(), 0.05f);
        time += dt;
        toastT = std::max(0.f, toastT - dt);

        // ---- global keys
        if (IsKeyPressed(KEY_F5)) {
            unloadGfx(gfx);
            unloadSounds();
            loadConfig("config.ini", cfg);
            loadGfx(gfx, cfg);
            loadSounds();
            buildWorld(world, cfg);
            viz.clear();
            tracers.clear();
            undo.clear();
            toast = "config reloaded";
            toastT = 2.f;
        }
        if (IsKeyPressed(KEY_ESCAPE)) menuOpen = !menuOpen;
        if (!menuOpen) {
            if (IsKeyPressed(KEY_R)) stats = sim::Stats{};
            if (IsKeyPressed(KEY_F1)) {
                editMode = !editMode;
                if (!editMode) {  // never leave the player inside something that was just built
                    sim::resolve(world.map, eye, 0.4f);
                    eye.y = 1.65f;
                }
            }
            if (!editMode) {
                if (IsKeyPressed(KEY_ONE)) cfg.weapon_index = 0;
                if (IsKeyPressed(KEY_TWO) && cfg.weapons.size() > 1) cfg.weapon_index = 1;
            }
        }
        Config::Weapon& wpn = cfg.weapons[cfg.weapon_index];

        // ---- cursor: free while a menu or the editor is open, captured for play
        const bool wantFree = menuOpen || editMode;
        if (wantFree != cursorFree) {
            cursorFree = wantFree;
            if (cursorFree) EnableCursor(); else DisableCursor();
            skipLook = true;
        }
        const bool playing = !menuOpen && !editMode;
        const double now = GetTime();

        // ---- editor camera (top-down, orthographic)
        if (editMode && !menuOpen) {
            const float pan = editView * 0.7f * dt;
            if (IsKeyDown(KEY_W)) editZ -= pan;
            if (IsKeyDown(KEY_S)) editZ += pan;
            if (IsKeyDown(KEY_A)) editX -= pan;
            if (IsKeyDown(KEY_D)) editX += pan;
            if (IsKeyDown(KEY_Q)) editView *= 1.f + dt * 1.4f;
            if (IsKeyDown(KEY_E)) editView /= 1.f + dt * 1.4f;
            editView = std::clamp(editView, 10.f, 200.f);
        }

        // ---- ADS: holding RMB ramps toward full aim over ads_delay_ms, a real input lag, not instant
        const bool adsHeld = playing && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        const float adsRate = wpn.ads_delay_ms > 0.f ? dt / (wpn.ads_delay_ms / 1000.f) : 1e6f;
        adsT = std::clamp(adsT + (adsHeld ? adsRate : -adsRate), 0.f, 1.f);

        // ---- look
        const Vector2 md = GetMouseDelta();
        float degPerCount = cfg.sensitivity * 0.022f;
        if (cfg.cm360 > 0.f && cfg.dpi > 0.f) degPerCount = 360.f / (cfg.cm360 / 2.54f * cfg.dpi);
        if (playing && !skipLook) {
            const float k = degPerCount * (1.f - adsT * (1.f - wpn.ads_sens_mult));
            yaw += md.x * k;
            pitch = std::clamp(pitch - md.y * k, -89.f, 89.f);
            if (yaw > 360.f) yaw -= 360.f;
            if (yaw < -360.f) yaw += 360.f;
        }
        skipLook = false;

        // ---- movement (WASD) + A/D timing. Key events are polled once per frame, so the timing
        // resolution is one frame: turn vsync off and fps = 0 in config.ini for the finest readout.
        float speedFrac = 0.f;
        if (playing && cfg.strafe_enabled) {
            strafeTrk.setWindowMs(cfg.strafe_window_ms);
            if (IsKeyReleased(KEY_A)) strafeTrk.release(strafe::Tracker::A, now);
            if (IsKeyReleased(KEY_D)) strafeTrk.release(strafe::Tracker::D, now);
            if (IsKeyPressed(KEY_A)) strafeTrk.press(strafe::Tracker::A, now);
            if (IsKeyPressed(KEY_D)) strafeTrk.press(strafe::Tracker::D, now);
            const float wantR = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
            const float wantF = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
            const float wl = std::sqrt(wantR * wantR + wantF * wantF);
            const float tR = wl > 0.f ? wantR / wl * cfg.move_speed : 0.f;
            const float tF = wl > 0.f ? wantF / wl * cfg.move_speed : 0.f;
            auto stepFor = [&](float target, float cur) {
                const float ms = std::fabs(target) > 1e-4f ? cfg.accel_ms : cfg.decel_ms;
                const float rate = ms > 0.f ? cfg.move_speed / (ms / 1000.f) : 1e6f;
                return strafe::moveToward(cur, target, rate * dt);
            };
            velR = stepFor(tR, velR);
            velF = stepFor(tF, velF);
            const float yr = yaw * DEG2RAD;
            const sim::V3 flatFwd{std::sin(yr), 0, -std::cos(yr)};
            const sim::V3 flatRight{std::cos(yr), 0, std::sin(yr)};
            eye = eye + flatRight * (velR * dt) + flatFwd * (velF * dt);
            sim::resolve(world.map, eye, 0.4f);
            eye.y = 1.65f;
            speedFrac = std::min(1.f, std::sqrt(velR * velR + velF * velF) / cfg.move_speed);
        } else {
            velR = velF = 0.f;
        }

        auto dirFrom = [](float yawDeg, float pitchDeg) {
            float y = yawDeg * DEG2RAD, p = pitchDeg * DEG2RAD;
            return sim::V3{std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)};
        };
        const sim::V3 fwd = dirFrom(yaw, pitch);
        const sim::V3 right = sim::normalize(sim::cross(fwd, sim::V3{0, 1, 0}));

        // ---- camera
        const int sw = GetScreenWidth(), sh = GetScreenHeight();
        const float aspect = static_cast<float>(sw) / static_cast<float>(std::max(1, sh));
        Camera3D cam{};
        if (editMode) {
            cam.position = Vector3{editX, 120.f, editZ};
            cam.target = Vector3{editX, 0.f, editZ};
            cam.up = Vector3{0, 0, -1};
            cam.fovy = editView;  // orthographic: visible height in meters
            cam.projection = CAMERA_ORTHOGRAPHIC;
        } else {
            const float fov = cfg.hfov + (wpn.ads_fov - cfg.hfov) * adsT;
            cam.position = toRl(eye);
            cam.target = toRl(eye + fwd);
            cam.up = Vector3{0, 1, 0};
            cam.fovy = 2.f * std::atan(std::tan(fov * DEG2RAD / 2.f) / aspect) * RAD2DEG;
            cam.projection = CAMERA_PERSPECTIVE;
        }

        // ---- shoot
        shotTimer = std::max(0.f, shotTimer - dt);
        const bool wantFire = playing && (wpn.auto_fire ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        if (wantFire && shotTimer <= 0.f) {
            shotTimer = 60.f / std::max(1.f, wpn.rpm);
            const float spreadDeg = wpn.spread * (1.f - adsT * (1.f - wpn.ads_spread_mult)) + cfg.move_spread * speedFrac;
            for (int p = 0; p < std::max(1, wpn.pellets); p++) {
                sim::V3 d = fwd;
                if (spreadDeg > 0.f) d = dirFrom(yaw + world.rng.range(-spreadDeg, spreadDeg), pitch + world.rng.range(-spreadDeg, spreadDeg));
                stats.shots++;
                sim::Hit h = sim::raycast(world.map, world.bots, cfg.bot, eye, d);
                if (h.bot >= 0) {
                    stats.hits++;
                    const bool headshot = h.part == sim::Part::Head;
                    if (headshot) stats.headshots++;
                    const float dmg = wpn.damage * (headshot ? wpn.head_mult : 1.f);
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
                    tracers.push_back(Tracer{muzzle, eye + d * len, 0.f, wpn.tracer_color});
                }
            }
            if (audioOk) sfxPlay(snd.shot, cfg.master);
        }

        // ---- map editor interaction
        float gx = 0, gz = 0;
        bool gridHit = false;
        if (editMode) {
            const Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
            if (std::fabs(ray.direction.y) > 1e-5f) {
                const float t = -ray.position.y / ray.direction.y;
                if (t > 0.f) {
                    gx = ray.position.x + ray.direction.x * t;
                    gz = ray.position.z + ray.direction.z * t;
                    if (snapOn) { gx = std::round(gx * 2.f) / 2.f; gz = std::round(gz * 2.f) / 2.f; }
                    gridHit = true;
                }
            }
        }
        if (editMode && !menuOpen) {
            const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.f) {
                if (shift) editH = std::clamp(editH + wheel * 0.5f, 0.5f, 20.f);
                else if (ctrl && tool == 1) editD = std::clamp(editD + wheel * 0.5f, 0.5f, 40.f);
                else if (tool == 0) editR = std::clamp(editR + wheel * 0.1f, 0.2f, 8.f);
                else editW = std::clamp(editW + wheel * 0.5f, 0.5f, 40.f);
            }
            if (IsKeyPressed(KEY_ONE)) tool = 0;
            if (IsKeyPressed(KEY_TWO)) tool = 1;
            if (IsKeyPressed(KEY_TAB)) tool = 1 - tool;
            if (IsKeyPressed(KEY_G)) snapOn = !snapOn;
            if (ctrl && IsKeyPressed(KEY_Z) && !undo.empty()) {
                world.map = undo.back();
                undo.pop_back();
                toast = "undo";
                toastT = 1.2f;
            }
            if (IsKeyPressed(KEY_C)) {
                pushUndo();
                world.map.pillars.clear();
                world.map.boxes.clear();
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) { pushUndo(); world.map.half = std::max(10.f, world.map.half - 2.f); }
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) { pushUndo(); world.map.half = std::min(80.f, world.map.half + 2.f); }
            if (gridHit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                pushUndo();
                if (tool == 0) world.map.pillars.push_back({gx, gz, editR, editH});
                else world.map.boxes.push_back({gx, gz, editW / 2.f, editD / 2.f, editH});
            }
            if (gridHit && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                bool isBox = false;
                int idx = pickObject(world.map, gx, gz, isBox);
                if (idx >= 0) {
                    pushUndo();
                    if (isBox) world.map.boxes.erase(world.map.boxes.begin() + idx);
                    else world.map.pillars.erase(world.map.pillars.begin() + idx);
                }
            }
            if (IsKeyPressed(KEY_F6)) {
                toast = saveMap(cfg.map_file, world.map) ? "map saved: " + cfg.map_file : "could not write " + cfg.map_file;
                toastT = 2.5f;
            }
        }

        // ---- simulate (paused while a menu is open)
        if (!menuOpen) {
            for (auto& b : world.bots) sim::update(b, dt, world.map, world.rng, cfg.bot);
            hmTimer = std::max(0.f, hmTimer - dt);
            for (auto& t : tracers) t.age += dt;
            tracers.erase(std::remove_if(tracers.begin(), tracers.end(),
                                         [&](const Tracer& t) {
                                             float dist = sim::length(t.b - t.a);
                                             return t.age * cfg.tracer_speed - cfg.tracer_length > dist;
                                         }),
                          tracers.end());
            updateViz(viz, world.bots, dt);
        }
        viz.resize(world.bots.size());

        // ================================================================ draw
        BeginDrawing();
        ClearBackground(editMode ? Color{22, 26, 32, 255} : BLACK);

        BeginMode3D(cam);
        setScene(gfx, cfg, cam.position, editMode);
        drawWorld(gfx, cfg, world.map, world.bots, cam.position, editMode);
        rlDrawRenderBatchActive();
        for (size_t i = 0; i < world.bots.size(); i++)
            if (world.bots[i].alive) drawBot(gfx, cfg, world.bots[i], viz[i], time);

        if (cfg.tracer && !editMode) {
            rlDrawRenderBatchActive();
            for (const auto& t : tracers) {
                sim::V3 dir = t.b - t.a;
                float dist = sim::length(dir);
                if (dist < 1e-4f) continue;
                dir = dir * (1.f / dist);
                float headD = std::min(dist, t.age * cfg.tracer_speed);
                float tailD = std::max(0.f, headD - cfg.tracer_length);
                if (headD <= tailD) continue;
                DrawCylinderEx(toRl(t.a + dir * tailD), toRl(t.a + dir * headD), cfg.tracer_width, cfg.tracer_width, 6, t.col);
            }
        }

        if (editMode && gridHit && !menuOpen) {  // translucent preview of what LMB will place
            Look gl;
            gl.base = Vector3{0.3f, 1.f, 0.5f};
            gl.alpha = 0.55f;
            gl.ao = 1.f;
            setLook(gfx, gl);
            if (tool == 0) drawModel(gfx.cyl, compose({editR, editH, editR}, {gx, 0, gz}, MatrixIdentity()));
            else drawModel(gfx.cube, compose({editW, editH, editD}, {gx, editH / 2.f, gz}, MatrixIdentity()));
        }
        EndMode3D();

        // ---- HUD
        const float cx = static_cast<float>(sw) / 2.f, cy = static_cast<float>(sh) / 2.f;
        if (!editMode) {
            if (cfg.crosshair) drawCrosshair(cx, cy, cfg);
            if (cfg.hitmarker && hmTimer > 0.f) {
                float k = hmTimer / std::max(0.001f, cfg.hm_time);
                Color c = hmKind == 2.f ? cfg.hm_kill : (hmKind == 1.f ? cfg.hm_head : cfg.hm_color);
                c.a = static_cast<unsigned char>(255.f * std::clamp(k * 1.5f, 0.f, 1.f));
                float grow = hmKind == 2.f ? 1.4f : 1.f;
                drawHitMarker(cx, cy, cfg.hm_size * grow, cfg.hm_gap, cfg.hm_thick, c);
            }
            DrawText(TextFormat("acc %.1f%%   hits %d/%d   hs %d   kills %d", stats.accuracy(), stats.hits, stats.shots, stats.headshots, stats.kills), 14, sh - 74, 20, WHITE);
            DrawText(TextFormat("%s%s", wpn.name.c_str(), adsT > 0.05f ? "  [ADS]" : ""), 14, sh - 48, 18, Fade(WHITE, 0.85f));
            DrawText("WASD move   LMB fire   RMB ADS   1/2 weapon   R reset   F1 map editor   ESC menu", 14, sh - 26, 16, Fade(WHITE, 0.6f));
            if (cfg.strafe_enabled && cfg.strafe_hud) {
                const int hx = static_cast<int>(cx) + 70, hy = static_cast<int>(cy) + 30;
                if (strafeTrk.hasGap()) {
                    const float g = strafeTrk.lastGapMs(), ag = std::fabs(g);
                    const Color gc = ag <= cfg.strafe_target_ms         ? Color{60, 255, 120, 255}
                                     : ag <= cfg.strafe_target_ms * 2.f ? Color{255, 210, 60, 255}
                                                                        : Color{255, 90, 70, 255};
                    DrawText(TextFormat("A/D gap %+.0f ms", g), hx, hy, 26, gc);
                    DrawText(TextFormat("avg %.0f ms   + gap  - overlap   (res ~%.1f ms)", strafeTrk.avgAbsMs(), GetFrameTime() * 1000.f), hx, hy + 30, 14, Fade(WHITE, 0.6f));
                    char buf[128];
                    int n = 0;
                    buf[0] = 0;
                    const auto& hist = strafeTrk.history();
                    const size_t start = hist.size() > 6 ? hist.size() - 6 : 0;
                    for (size_t k = start; k < hist.size() && n < 110; k++)
                        n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "%+.0f  ", static_cast<double>(hist[k]));
                    DrawText(buf, hx, hy + 48, 14, Fade(WHITE, 0.5f));
                } else {
                    DrawText("A/D gap: strafe A then D", hx, hy, 18, Fade(WHITE, 0.4f));
                }
            }
        } else {
            DrawRectangle(10, 10, 640, 130, Fade(BLACK, 0.55f));
            DrawText("MAP EDITOR   (F1 back to game, F6 save)", 22, 18, 20, WHITE);
            DrawText(TextFormat("Tool: %s   [1] pillar  [2] block  [Tab] switch     Snap %s [G]", tool == 0 ? "PILLAR" : "BLOCK", snapOn ? "ON" : "off"), 22, 44, 16, Color{140, 220, 255, 255});
            if (tool == 0) DrawText(TextFormat("radius %.1f m (wheel)   height %.1f m (Shift+wheel)", static_cast<double>(editR), static_cast<double>(editH)), 22, 66, 16, WHITE);
            else DrawText(TextFormat("width %.1f (wheel)  depth %.1f (Ctrl+wheel)  height %.1f (Shift+wheel)", static_cast<double>(editW), static_cast<double>(editD), static_cast<double>(editH)), 22, 66, 16, WHITE);
            DrawText("LMB place   RMB delete   Ctrl+Z undo   C clear   - / = arena size   WASD pan   Q/E zoom", 22, 88, 16, Fade(WHITE, 0.75f));
            DrawText(TextFormat("%d pillars, %d blocks, arena %.0f x %.0f m", static_cast<int>(world.map.pillars.size()), static_cast<int>(world.map.boxes.size()), static_cast<double>(world.map.half * 2), static_cast<double>(world.map.half * 2)), 22, 110, 16, Fade(WHITE, 0.75f));
            if (gridHit && !menuOpen) {
                const Vector2 mp = GetMousePosition();
                DrawText(TextFormat("%.1f, %.1f", static_cast<double>(gx), static_cast<double>(gz)), static_cast<int>(mp.x) + 16, static_cast<int>(mp.y) + 14, 16, WHITE);
            }
        }
        DrawText(TextFormat("%d fps", GetFPS()), 14, editMode ? sh - 26 : 10, 18, Fade(WHITE, 0.7f));
        {
            const char* tag = TextFormat("by %s", kDeveloper);
            DrawText(tag, sw - MeasureText(tag, 14) - 12, sh - 24, 14, Fade(WHITE, 0.35f));
        }
        if (toastT > 0.f) {
            const int w = MeasureText(toast.c_str(), 20);
            DrawRectangle(sw / 2 - w / 2 - 14, 18, w + 28, 36, Fade(BLACK, 0.7f));
            DrawText(toast.c_str(), sw / 2 - w / 2, 26, 20, WHITE);
        }

        // ---- settings menu (two columns: you / bots)
        if (menuOpen) {
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55f));
            const float pw = std::min(1060.f, static_cast<float>(sw) - 20.f), ph = 560.f;
            const float px = (static_cast<float>(sw) - pw) / 2.f, py = std::max(10.f, (static_cast<float>(sh) - ph) / 2.f);
            DrawRectangleRec(Rectangle{px, py, pw, ph}, Color{28, 32, 42, 245});
            DrawRectangleLinesEx(Rectangle{px, py, pw, ph}, 2.f, Color{90, 110, 150, 255});
            DrawText("SETTINGS", static_cast<int>(px) + 24, static_cast<int>(py) + 14, 26, WHITE);
            DrawText("ESC to resume", static_cast<int>(px + pw) - 24 - MeasureText("ESC to resume", 16), static_cast<int>(py) + 20, 16, Fade(WHITE, 0.55f));

            const float gap = 40.f, colW = (pw - 48.f - gap) / 2.f, rowH = 52.f;
            const float lx = px + 24, rx = lx + colW + gap;
            DrawText("YOU", static_cast<int>(lx), static_cast<int>(py) + 50, 16, Color{140, 220, 255, 255});
            DrawText("BOTS", static_cast<int>(rx), static_cast<int>(py) + 50, 16, Color{255, 170, 120, 255});
            char val[48];

            // left column: mouse, view, sound, movement
            float y = py + 74;
            std::snprintf(val, sizeof(val), "%.2f", static_cast<double>(cfg.sensitivity));
            uiSlider(1, Rectangle{lx, y, colW, rowH}, cfg.cm360 > 0.f ? "Sensitivity (off: cm/360 is set)" : "Sensitivity", cfg.sensitivity, 0.05f, 10.f, 0.01f, val, cfg.cm360 <= 0.f);
            y += rowH;
            std::snprintf(val, sizeof(val), cfg.cm360 > 0.f ? "%.1f cm" : "off", static_cast<double>(cfg.cm360));
            uiSlider(2, Rectangle{lx, y, colW, rowH}, "cm per 360 (0 = use sensitivity)", cfg.cm360, 0.f, 100.f, 0.5f, val);
            y += rowH;
            std::snprintf(val, sizeof(val), "%.0f", static_cast<double>(cfg.dpi));
            uiSlider(3, Rectangle{lx, y, colW, rowH}, "Mouse DPI", cfg.dpi, 100.f, 4000.f, 50.f, val);
            y += rowH;
            std::snprintf(val, sizeof(val), "%.0f", static_cast<double>(cfg.hfov));
            uiSlider(4, Rectangle{lx, y, colW, rowH}, "Field of view (horizontal)", cfg.hfov, 60.f, 130.f, 1.f, val);
            y += rowH;
            std::snprintf(val, sizeof(val), "%.0f%%", static_cast<double>(cfg.master * 100.f));
            uiSlider(5, Rectangle{lx, y, colW, rowH}, "Master volume", cfg.master, 0.f, 1.f, 0.05f, val);
            y += rowH;
            std::snprintf(val, sizeof(val), "%.0f ms", static_cast<double>(wpn.ads_delay_ms));
            uiSlider(6, Rectangle{lx, y, colW, rowH}, TextFormat("ADS delay, %s", wpn.name.c_str()), wpn.ads_delay_ms, 0.f, 500.f, 5.f, val);
            y += rowH;
            std::snprintf(val, sizeof(val), "%.2f m/s", static_cast<double>(cfg.move_speed));
            uiSlider(7, Rectangle{lx, y, colW, rowH}, "Your walk speed", cfg.move_speed, 1.f, 12.f, 0.25f, val);
            y += rowH;

            const float dpc = cfg.cm360 > 0.f && cfg.dpi > 0.f ? 360.f / (cfg.cm360 / 2.54f * cfg.dpi) : cfg.sensitivity * 0.022f;
            const float cm360 = dpc > 0.f ? 360.f / dpc / cfg.dpi * 2.54f : 0.f;
            DrawText(TextFormat("= %.4f deg per count  =  %.1f cm per 360 at %.0f DPI", static_cast<double>(dpc), static_cast<double>(cm360), static_cast<double>(cfg.dpi)), static_cast<int>(lx), static_cast<int>(y) + 2, 15, Color{140, 220, 255, 255});
            DrawText("Tip: hover a slider and use the mouse wheel for single steps.", static_cast<int>(lx), static_cast<int>(y) + 22, 14, Fade(WHITE, 0.5f));

            // right column: bots
            float ry = py + 74;
            float countF = static_cast<float>(cfg.bot_count);
            std::snprintf(val, sizeof(val), "%d", cfg.bot_count);
            if (uiSlider(8, Rectangle{rx, ry, colW, rowH}, "Bots in the arena", countF, 1.f, 20.f, 1.f, val)) {
                cfg.bot_count = static_cast<int>(countF);
                const size_t old = world.bots.size();
                world.bots.resize(static_cast<size_t>(cfg.bot_count));
                for (size_t i = old; i < world.bots.size(); i++) sim::spawn(world.bots[i], world.map, world.rng, cfg.bot);
            }
            ry += rowH;
            std::snprintf(val, sizeof(val), "%.0f", static_cast<double>(cfg.bot.hp));
            if (uiSlider(9, Rectangle{rx, ry, colW, rowH}, "Bot HP", cfg.bot.hp, 10.f, 500.f, 10.f, val)) {
                for (auto& b : world.bots)
                    if (b.alive) b.hp = cfg.bot.hp;
            }
            ry += rowH;
            std::snprintf(val, sizeof(val), "%.2f m/s", static_cast<double>(cfg.bot.speed_min));
            if (uiSlider(10, Rectangle{rx, ry, colW, rowH}, "Bot walk speed, slowest", cfg.bot.speed_min, 0.5f, 12.f, 0.25f, val))
                cfg.bot.speed_max = std::max(cfg.bot.speed_max, cfg.bot.speed_min);
            ry += rowH;
            std::snprintf(val, sizeof(val), "%.2f m/s", static_cast<double>(cfg.bot.speed_max));
            if (uiSlider(11, Rectangle{rx, ry, colW, rowH}, "Bot walk speed, fastest", cfg.bot.speed_max, 0.5f, 12.f, 0.25f, val))
                cfg.bot.speed_min = std::min(cfg.bot.speed_min, cfg.bot.speed_max);
            ry += rowH;
            std::snprintf(val, sizeof(val), "%.1f s", static_cast<double>(cfg.bot.respawn));
            uiSlider(12, Rectangle{rx, ry, colW, rowH}, "Respawn time after a kill", cfg.bot.respawn, 0.f, 5.f, 0.1f, val);
            ry += rowH;
            DrawText("Each bot picks a random speed between slowest and fastest", static_cast<int>(rx), static_cast<int>(ry) + 4, 14, Fade(WHITE, 0.5f));
            DrawText("every time it chooses a new destination.", static_cast<int>(rx), static_cast<int>(ry) + 22, 14, Fade(WHITE, 0.5f));

            const float bw = (pw - 48.f - 20.f) / 3.f, by = py + ph - 60.f;
            if (uiButton(Rectangle{lx, by, bw, 44}, "Resume")) menuOpen = false;
            if (uiButton(Rectangle{lx + bw + 10, by, bw, 44}, "Save settings")) {
                std::unordered_map<std::string, std::string> kv{
                    {"mouse.sensitivity", fmtNum(cfg.sensitivity)}, {"mouse.cm360", fmtNum(cfg.cm360)}, {"mouse.dpi", fmtNum(cfg.dpi)},
                    {"window.hfov", fmtNum(cfg.hfov)},              {"audio.master", fmtNum(cfg.master)},
                    {"movement.speed", fmtNum(cfg.move_speed)},     {"bots.count", fmtNum(static_cast<float>(cfg.bot_count))},
                    {"bots.hp", fmtNum(cfg.bot.hp)},                {"bots.speed_min", fmtNum(cfg.bot.speed_min)},
                    {"bots.speed_max", fmtNum(cfg.bot.speed_max)},  {"bots.respawn", fmtNum(cfg.bot.respawn)}};
                for (size_t i = 0; i < cfg.weapons.size(); i++) kv["weapon" + std::to_string(i + 1) + ".ads_delay_ms"] = fmtNum(cfg.weapons[i].ads_delay_ms);
                const int n = Ini::updateFile("config.ini", kv);
                toast = n >= 0 ? "settings saved to config.ini" : "could not write config.ini";
                toastT = 2.5f;
            }
            if (uiButton(Rectangle{lx + (bw + 10) * 2, by, bw, 44}, "Quit")) quitRequested = true;
        }
        EndDrawing();
    }

    unloadSounds();
    unloadGfx(gfx);
    if (audioOk) CloseAudioDevice();
    CloseWindow();
    return 0;
}
