// WinNX v1.0.0 - Windows 8.1-style tablet shell for Nintendo Switch (SDL2 + libnx)
// v0.9 additions (Cortana, Skype, OneDrive, Task Manager, Desktop, Help+Tips, sound, flip tiles) live in win81.h
// Touch-first. Black "bezel" strip at the bottom holds the Windows button (OLED friendly).
// Everything you write (Notes, Word, Excel) autosaves to sdmc:/switch/WinNX/ and reloads next time.
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#ifdef WINNX_AUDIO
#include <SDL2/SDL_mixer.h>
#endif
#ifdef WINNX_NET
#include <curl/curl.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>
#include <set>
#include <functional>
#include <unistd.h>
#include <sys/statvfs.h>
#include <exception>
#include <map>
#include <algorithm>
#include <ctime>
#include "bootanim.h"
#include "font_sel_reg.h"
#include "font_sel_bold.h"

static const int W = 1280, H = 720, UIH = 668; // UIH = screen area above the black bezel strip
static const int KBH = 300, KBTOP = UIH - KBH;

struct Col { Uint8 r, g, b; };
static Col mix(Col a, Col b, float t) {
    return Col{ (Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t) };
}
static const Col WHITE = {255, 255, 255}, BLACK = {0, 0, 0}, DARK = {30, 30, 30};
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float ease(float t) { t = clampf(t, 0, 1); return 1.0f - (1 - t) * (1 - t) * (1 - t); }
static bool inR(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

// ---------------------------------------------------------------- globals
static SDL_Window* gWin = nullptr;
static SDL_Renderer* gR = nullptr;
static SDL_Texture* gTarget = nullptr;
static PadState gPad;
static bool gRun = true;

static const int F16 = 0, F22 = 1, F30 = 2, F44 = 3, F72 = 4, F130 = 5, FCOUNT = 6;
static const int FSZ[FCOUNT] = {16, 22, 30, 44, 72, 130};
static TTF_Font* gFont[FCOUNT];
static TTF_Font* gFontB[FCOUNT];   // bold
static TTF_Font* gFontSys[FCOUNT]; // Switch system font: fallback for symbols the UI font lacks
struct TT { SDL_Texture* t; int w, h; };
static std::map<std::string, TT> gTC;

struct Theme { Col top, bottom, accent; };
static const int NTHEME = 12, NPAT = 6;
static Theme THEMES[44] = {
    {{58, 16, 140}, {28, 4, 88}, {122, 62, 224}},
    {{10, 72, 164}, {4, 28, 92}, {0, 120, 215}},
    {{0, 112, 112}, {0, 48, 60}, {0, 150, 140}},
    {{150, 20, 62}, {68, 4, 34}, {200, 40, 70}},
    {{22, 110, 44}, {6, 50, 22}, {30, 160, 60}},
    {{196, 92, 20}, {100, 40, 8}, {230, 120, 20}},
    {{196, 40, 120}, {90, 10, 60}, {226, 60, 150}},
    {{60, 72, 94}, {24, 28, 42}, {90, 110, 150}},
    {{92, 122, 18}, {40, 56, 8}, {130, 170, 20}},
    {{122, 24, 152}, {56, 8, 78}, {160, 50, 200}},
    {{0, 120, 190}, {0, 50, 92}, {0, 160, 230}},
    {{120, 70, 40}, {54, 28, 14}, {170, 100, 56}},
};
static int gTheme = 0;
struct StoreTheme { const char* name; Col top, bottom, accent; int pat; int tag; };   // tag bits: 1 dark, 2 bright, 4 nature
static const StoreTheme STHEMES[] = {
    {"Aurora", {10, 60, 70}, {4, 20, 40}, {0, 200, 150}, 0, 5},
    {"Sunset", {230, 110, 50}, {120, 30, 60}, {255, 140, 60}, 2, 2},
    {"Midnight", {16, 16, 40}, {4, 4, 16}, {90, 90, 220}, 1, 1},
    {"Ocean Breeze", {0, 150, 200}, {0, 70, 130}, {0, 190, 230}, 3, 6},
    {"Forest", {30, 90, 40}, {8, 40, 20}, {100, 180, 70}, 4, 5},
    {"Cherry Blossom", {240, 150, 180}, {170, 70, 110}, {255, 120, 170}, 5, 2},
    {"Lemon", {230, 200, 40}, {160, 120, 10}, {250, 220, 0}, 2, 2},
    {"Graphite", {70, 70, 76}, {24, 24, 28}, {160, 160, 170}, 1, 1},
    {"Lavender", {150, 120, 210}, {80, 50, 140}, {190, 150, 255}, 3, 2},
    {"Crimson", {170, 20, 40}, {60, 4, 16}, {230, 40, 60}, 0, 1},
    {"Mint", {80, 200, 160}, {20, 110, 100}, {60, 230, 170}, 4, 6},
    {"Desert", {210, 160, 100}, {120, 70, 40}, {240, 180, 90}, 5, 4},
    {"Neon", {20, 10, 50}, {0, 0, 20}, {255, 0, 200}, 1, 1},
    {"Sky", {120, 190, 250}, {40, 100, 190}, {255, 255, 255}, 3, 2},
    {"Coffee", {110, 70, 50}, {44, 26, 18}, {190, 130, 90}, 2, 1},
    {"Galaxy", {40, 10, 90}, {8, 0, 40}, {180, 80, 255}, 0, 1},
    {"Rose Gold", {200, 140, 130}, {110, 60, 70}, {240, 170, 150}, 5, 2},
    {"Emerald", {0, 120, 70}, {0, 50, 30}, {20, 200, 110}, 4, 5},
    {"Ice", {170, 220, 240}, {60, 120, 160}, {120, 220, 255}, 3, 6},
    {"Volcano", {200, 40, 0}, {70, 10, 0}, {255, 90, 0}, 2, 1},
    {"Retro Arcade", {30, 0, 70}, {0, 20, 60}, {255, 220, 0}, 1, 1},
    {"Slate Blue", {50, 70, 120}, {20, 28, 60}, {100, 140, 230}, 0, 1},
    {"Spring", {140, 200, 60}, {50, 110, 30}, {200, 240, 80}, 4, 6},
    {"Plum", {110, 30, 90}, {44, 8, 40}, {200, 60, 160}, 5, 1},
    {"Midnight", {20, 24, 60}, {4, 6, 24}, {90, 110, 255}, 0, 1},
    {"Sunset", {250, 130, 60}, {120, 30, 80}, {255, 190, 90}, 2, 2},
    {"Forest", {40, 90, 50}, {10, 36, 20}, {120, 200, 90}, 4, 5},
    {"Lavender", {170, 150, 230}, {80, 60, 150}, {220, 200, 255}, 5, 2},
    {"Mint", {120, 220, 190}, {20, 120, 110}, {200, 255, 235}, 3, 6},
    {"Graphite", {70, 74, 82}, {20, 22, 26}, {160, 170, 190}, 0, 1},
    {"Cherry", {190, 20, 60}, {70, 4, 24}, {255, 90, 120}, 2, 1},
    {"Desert", {220, 170, 100}, {120, 70, 40}, {255, 220, 150}, 2, 2},
};
static const int NSTH = (int)(sizeof(STHEMES) / sizeof(STHEMES[0]));
static unsigned gThOwned = 0;
static struct ThemeFill { ThemeFill() { for (int i = 0; i < NSTH && 12 + i < 44; i++) THEMES[12 + i] = Theme{STHEMES[i].top, STHEMES[i].bottom, STHEMES[i].accent}; } } gThemeFill;

static SDL_Texture* gLockTex = nullptr;
static int setPage = 0, setSub = 0;
static bool gBurn = true, g24 = true;
static int gDocFont = F22;
static bool gBold = false;
static int gEngine = 0; // CMrhoe search engine: 0 DuckDuckGo, 1 Google, 2 Bing
static bool gLockScr = true, gBootAnim = true;

static std::string gUser = "User";
static u32 gBat = 0;
static const std::string DIR_ = "sdmc:/switch/WinNX/";

// ---------------------------------------------------------------- v0.9 shared state (the code is in win81.h)
static int gFlipMode = 1;            // live tile animation: 0 off, 1 flip, 2 slide
static int gFlipSecs = 7;            // seconds between tile animations
static bool gTileNames = true;       // app names on tiles
static int gSndScheme = 0;           // 0 Windows Default, 1 Chimes, 2 Retro, 3 No sounds
static int gSndVol = 7;              // 0..10
static bool gSndStartup = true, gSndNotify = true, gSndTouch = false;
static bool gBootDesk = false;       // open the Desktop after signing in
static bool gCoSumm = true;          // Cortana summaries
static int gCoInt = 63;              // Cortana interests (bitmask)
static bool gOdAuto = true;          // OneDrive: keep a copy of Word, Excel and Notes
static bool gV10 = false;
static bool fxReady(); static void fxAppTurn(float); static void fxTileTurn(const struct Tile&, SDL_Rect, float); static void eggCount();
static bool gV9 = false;             // first run of v0.9 pins the new apps
static bool gSkyAway = false;
enum { SND_STARTUP, SND_NOTIFY, SND_UNLOCK, SND_ERROR, SND_CLICK, SND_RING, SND_COUNT };
static void sndPlay(int id);
static void win9Save();
static const int NTINT = 13;         // tile colours: 0 = the app's own colour
static const Col TINTS[NTINT] = {{0, 0, 0}, {162, 0, 255}, {255, 0, 151}, {0, 171, 169}, {140, 191, 38}, {160, 80, 0}, {230, 113, 184}, {240, 150, 9}, {27, 161, 226}, {229, 20, 0}, {51, 153, 51}, {0, 80, 239}, {96, 60, 186}};

struct Now { int y = 2026, mo = 1, d = 1, h = 0, mi = 0, s = 0; };
static Now gNow;

// ---------------------------------------------------------------- drawing helpers
static void rect(int x, int y, int w, int h, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(gR, &r);
}
static void disc(int cx, int cy, int r, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(gR, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}
static void ring(int cx, int cy, int r, int t, Col fg, Col bg) { disc(cx, cy, r, fg); disc(cx, cy, r - t, bg); }
static void line(int x1, int y1, int x2, int y2, int t, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    bool steep = abs(y2 - y1) > abs(x2 - x1);
    for (int i = -t / 2; i <= t / 2; i++) {
        if (steep) SDL_RenderDrawLine(gR, x1 + i, y1, x2 + i, y2);
        else SDL_RenderDrawLine(gR, x1, y1 + i, x2, y2 + i);
    }
}
static void tri(float x1, float y1, float x2, float y2, float x3, float y3, Col c, int a = 255) {
    SDL_Vertex v[3];
    SDL_Color sc = {c.r, c.g, c.b, (Uint8)a};
    v[0].position.x = x1; v[0].position.y = y1;
    v[1].position.x = x2; v[1].position.y = y2;
    v[2].position.x = x3; v[2].position.y = y3;
    for (int i = 0; i < 3; i++) { v[i].color = sc; v[i].tex_coord.x = 0; v[i].tex_coord.y = 0; }
    SDL_RenderGeometry(gR, nullptr, v, 3, nullptr, 0);
}
static void outline(int x, int y, int w, int h, int t, Col c) {
    rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
}
static void quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, Col c, int a = 255) {
    tri(x1, y1, x2, y2, x3, y3, c, a);
    tri(x1, y1, x3, y3, x4, y4, c, a);
}
// The Windows 8 logo: four perspective panes. h = total logo height in pixels, (cx,cy) = its centre.
static void logoPanes(float cx, float cy, float h, Col c, int a = 255) {
    // traced from the official Windows 8 logo: four panes with perspective, 8 px gaps
    static const float P[4][4][2] = {
        {{55.2f, 77.5f}, {107.2f, 67.0f}, {107.8f, 124.2f}, {55.5f, 124.5f}},     // top-left
        {{115.8f, 65.2f}, {200.5f, 49.5f}, {200.2f, 124.5f}, {115.8f, 124.2f}},   // top-right
        {{55.8f, 131.8f}, {107.8f, 132.2f}, {107.2f, 188.8f}, {55.2f, 178.2f}},   // bottom-left
        {{115.8f, 132.2f}, {200.0f, 131.8f}, {200.5f, 206.2f}, {115.8f, 195.5f}}}; // bottom-right
    float k = h / 156.7f;
    for (int i = 0; i < 4; i++) {
        float X[4], Y[4];
        for (int j = 0; j < 4; j++) { X[j] = cx + (P[i][j][0] - 127.9f) * k; Y[j] = cy + (P[i][j][1] - 127.9f) * k; }
        quad(X[0], Y[0], X[1], Y[1], X[2], Y[2], X[3], Y[3], c, a);
    }
}
static void winLogo(int cx, int cy, int s, Col c, int a = 255) { logoPanes((float)cx, (float)cy, (float)s, c, a); }
// choose the font for a string: UI font normally, system font if the string has glyphs the UI font lacks
static TTF_Font* pickFont(const std::string& s, int f, bool bold = false) {
    TTF_Font* base = (bold && gFontB[f]) ? gFontB[f] : gFont[f];
    if (!base || !gFontSys[f] || base == gFontSys[f]) return base;
    bool ascii = true;
    for (unsigned char ch : s) if (ch >= 0x80) { ascii = false; break; }
    if (ascii) return base;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i]; Uint32 cp = 0xFFFF; size_t n = 1;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); n = 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) { cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F); n = 3; }
        else if ((c & 0xF8) == 0xF0) n = 4;
        if (cp < 0x10000 && cp >= 0x20 && !TTF_GlyphIsProvided(base, (Uint16)cp)) return gFontSys[f];
        i += n;
    }
    return base;
}
static int tw(const std::string& s, int f) {
    int w = 0, h = 0;
    TTF_Font* ft = pickFont(s, f);
    if (ft && !s.empty()) TTF_SizeUTF8(ft, s.c_str(), &w, &h);
    return w;
}
// white text is cached; colour/alpha applied through texture mods. align: 0 left, 1 centre, 2 right
static void text(const std::string& s, int f, int x, int y, Col c, int align = 0, int a = 255) {
    if (s.empty() || !gFont[f]) return;
    std::string key = std::to_string(f) + "|" + s;
    auto it = gTC.find(key);
    if (it == gTC.end()) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* sf = TTF_RenderUTF8_Blended(pickFont(s, f), s.c_str(), white);
        if (!sf) return;
        TT tt;
        tt.t = SDL_CreateTextureFromSurface(gR, sf);
        tt.w = sf->w; tt.h = sf->h;
        SDL_FreeSurface(sf);
        if (!tt.t) return;
        SDL_SetTextureBlendMode(tt.t, SDL_BLENDMODE_BLEND);
        it = gTC.emplace(key, tt).first;
    }
    TT& tt = it->second;
    int dx = x;
    if (align == 1) dx = x - tt.w / 2;
    else if (align == 2) dx = x - tt.w;
    SDL_SetTextureColorMod(tt.t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tt.t, (Uint8)a);
    SDL_Rect d = {dx, y, tt.w, tt.h};
    SDL_RenderCopy(gR, tt.t, nullptr, &d);
}
// styled text: style bits 1 bold, 2 italic, 4 underline (SDL_ttf synthesises them)
static int twS(const std::string& s, int f, int style) {
    int w = 0, h = 0;
    bool rb = (style & 1) && gFontB[f];
    TTF_Font* ft = pickFont(s, f, rb);
    int st = (rb && ft == gFontB[f]) ? (style & ~1) : style;
    if (ft && !s.empty()) {
        TTF_SetFontStyle(ft, st);
        TTF_SizeUTF8(ft, s.c_str(), &w, &h);
        TTF_SetFontStyle(ft, 0);
    }
    return w;
}
static void textS(const std::string& s, int f, int x, int y, Col c, int style, int align = 0, int a = 255) {
    if (style == 0) { text(s, f, x, y, c, align, a); return; }
    if (s.empty() || !gFont[f]) return;
    std::string key = "S" + std::to_string(style) + "|" + std::to_string(f) + "|" + s;
    auto it = gTC.find(key);
    if (it == gTC.end()) {
        SDL_Color white = {255, 255, 255, 255};
        bool rb = (style & 1) && gFontB[f];
        TTF_Font* ft = pickFont(s, f, rb);
        int st = (rb && ft == gFontB[f]) ? (style & ~1) : style;
        TTF_SetFontStyle(ft, st);
        SDL_Surface* sf = TTF_RenderUTF8_Blended(ft, s.c_str(), white);
        TTF_SetFontStyle(ft, 0);
        if (!sf) return;
        TT tt;
        tt.t = SDL_CreateTextureFromSurface(gR, sf);
        tt.w = sf->w; tt.h = sf->h;
        SDL_FreeSurface(sf);
        if (!tt.t) return;
        SDL_SetTextureBlendMode(tt.t, SDL_BLENDMODE_BLEND);
        it = gTC.emplace(key, tt).first;
    }
    TT& tt = it->second;
    int dx = x;
    if (align == 1) dx = x - tt.w / 2;
    else if (align == 2) dx = x - tt.w;
    SDL_SetTextureColorMod(tt.t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tt.t, (Uint8)a);
    SDL_Rect d = {dx, y, tt.w, tt.h};
    SDL_RenderCopy(gR, tt.t, nullptr, &d);
}
static void btn(int x, int y, int w, int h, const char* l, Col bg, Col fg) {
    rect(x, y, w, h, bg);
    text(l, F22, x + w / 2, y + h / 2 - 15, fg, 1);
}

// ---------------------------------------------------------------- time helpers
static const char* MONTHS[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
static const char* WD[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* WD3[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
static int dow(int y, int m, int d) {
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
static int daysIn(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}
static void updateNow() {
    u64 t = 0;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_Default, &t))) {
        TimeCalendarTime ct;
        TimeCalendarAdditionalInfo ai;
        if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(t, &ct, &ai))) {
            gNow.y = ct.year; gNow.mo = ct.month; gNow.d = ct.day;
            gNow.h = ct.hour; gNow.mi = ct.minute; gNow.s = ct.second;
            return;
        }
    }
    time_t tt = time(NULL);
    struct tm* lt = localtime(&tt);
    if (lt) {
        gNow.y = lt->tm_year + 1900; gNow.mo = lt->tm_mon + 1; gNow.d = lt->tm_mday;
        gNow.h = lt->tm_hour; gNow.mi = lt->tm_min; gNow.s = lt->tm_sec;
    }
}
static std::string timeStr(bool ampm = false) {
    char b[32];
    if (g24) snprintf(b, sizeof b, "%02d:%02d", gNow.h, gNow.mi);
    else {
        int h = gNow.h % 12; if (h == 0) h = 12;
        if (ampm) snprintf(b, sizeof b, "%d:%02d %s", h, gNow.mi, gNow.h >= 12 ? "PM" : "AM");
        else snprintf(b, sizeof b, "%d:%02d", h, gNow.mi);
    }
    return b;
}
static std::string dateStr() {
    char b[64];
    snprintf(b, sizeof b, "%s, %s %d", WD[dow(gNow.y, gNow.mo, gNow.d)], MONTHS[gNow.mo - 1], gNow.d);
    return b;
}

// ---------------------------------------------------------------- files
static bool readRaw(const std::string& p, std::string& out) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    char buf[4096]; size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}
static bool readFile(const std::string& p, std::string& out) { // falls back to the .tmp copy if a save was cut off by a crash
    if (readRaw(p, out)) return true;
    return readRaw(p + ".tmp", out);
}
static bool writeFile(const std::string& p, const std::string& d) { // atomic: write .tmp, then swap it in
    std::string t = p + ".tmp";
    FILE* f = fopen(t.c_str(), "wb");
    if (!f) return false;
    size_t n = fwrite(d.data(), 1, d.size(), f);
    fclose(f);
    if (n != d.size()) { remove(t.c_str()); return false; }
    remove(p.c_str());
    if (rename(t.c_str(), p.c_str()) != 0) { FILE* g = fopen(p.c_str(), "wb"); if (!g) return false; fwrite(d.data(), 1, d.size(), g); fclose(g); remove(t.c_str()); }
    return true;
}
static void backupFile(const std::string& p) { // keep the old copy as .bak before "New"
    std::string old;
    if (readFile(p, old) && !old.empty()) writeFile(p + ".bak", old);
}
static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool endsWith(const std::string& s, const std::string& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }

// ---------------------------------------------------------------- apps / tiles / start layout
enum { A_MAIL, A_MUSIC, A_PHOTOS, A_DESKTOP, A_CALENDAR, A_STORE, A_CALC, A_NOTES, A_WEATHER, A_CLOCK, A_SETTINGS, A_POWER, A_GAMES, A_WORD, A_EXCEL,
       A_BROWSER, A_PAINT, A_SOLITAIRE, A_2048, A_MINES, A_SNAKE, A_TODO, A_FILES, A_TASKMGR, A_TIMER, A_NEWS, A_ROBLOX,
       A_MAPS, A_CAMERA, A_PODCASTS, A_RADIO, A_PDF, A_TERM, A_PS, A_CHESS, A_THEMES, A_CONV, A_BLOCKS, A_LIGHT, A_WORLD, A_DICT, A_CORTANA,
       A_SKYPE, A_ONEDRIVE, A_HELP, A_RECORDER, A_FOOD, A_HEALTH, A_PET, A_CITY, A_COUNT };

// key = name used in layout.dat, sys = built in (always there, can only be unpinned), w,h = default tile size in half-cells
struct AppDef { const char* key; const char* name; Col col; int w, h; bool sys; };
static const AppDef ADEF[A_COUNT] = {
    /* A_MAIL     */ {"mail", "Mail", {0, 114, 198}, 4, 2, true},
    /* A_MUSIC    */ {"music", "Music", {222, 89, 26}, 2, 2, true},
    /* A_PHOTOS   */ {"photos", "Photos", {0, 150, 160}, 2, 2, true},
    /* A_DESKTOP  */ {"desktop", "Desktop", {19, 94, 170}, 2, 2, true},
    /* A_CALENDAR */ {"calendar", "Calendar", {110, 70, 215}, 4, 2, true},
    /* A_STORE    */ {"store", "Store", {16, 160, 50}, 4, 4, true},
    /* A_CALC     */ {"calc", "Calculator", {60, 140, 70}, 2, 2, true},
    /* A_NOTES    */ {"notes", "Notes", {200, 35, 75}, 2, 2, true},
    /* A_WEATHER  */ {"weather", "Weather", {50, 150, 230}, 4, 2, true},
    /* A_CLOCK    */ {"clock", "Clock", {0, 110, 110}, 4, 4, true},
    /* A_SETTINGS */ {"settings", "PC settings", {90, 90, 100}, 2, 2, true},
    /* A_POWER    */ {"power", "Power", {200, 50, 50}, 2, 2, true},
    /* A_GAMES    */ {"games", "Games", {130, 50, 160}, 4, 4, true},
    /* A_WORD     */ {"word", "Word 2016", {43, 87, 154}, 4, 2, false},
    /* A_EXCEL    */ {"excel", "Excel 2016", {33, 115, 70}, 4, 2, false},
    /* A_BROWSER  */ {"browser", "CMrhoe", {219, 68, 55}, 4, 2, false},
    /* A_PAINT    */ {"paint", "Paint", {200, 70, 160}, 2, 2, false},
    /* A_SOLITAIRE*/ {"solitaire", "Solitaire", {20, 110, 60}, 2, 2, false},
    /* A_2048     */ {"2048", "2048", {237, 150, 50}, 2, 2, false},
    /* A_MINES    */ {"mines", "Minesweeper", {90, 100, 125}, 2, 2, false},
    /* A_SNAKE    */ {"snake", "Snake", {60, 150, 80}, 2, 2, false},
    /* A_TODO     */ {"todo", "To-do", {20, 140, 160}, 2, 2, false},
    /* A_FILES    */ {"files", "Files", {214, 160, 20}, 2, 2, true},
    /* A_TASKMGR  */ {"taskmgr", "Task Manager", {0, 100, 180}, 2, 2, true},
    /* A_TIMER    */ {"timer", "Timer", {160, 60, 70}, 2, 2, false},
    /* A_NEWS     */ {"news", "News", {196, 38, 38}, 4, 2, false},
    /* A_ROBLOX   */ {"roblox", "Roblox", {226, 35, 26}, 2, 2, false},
    /* A_MAPS     */ {"maps", "Maps", {30, 150, 90}, 2, 2, false},
    /* A_CAMERA   */ {"camera", "Camera", {70, 74, 86}, 2, 2, false},
    /* A_PODCASTS */ {"podcasts", "Podcasts", {150, 60, 200}, 2, 2, false},
    /* A_RADIO    */ {"radio", "Radio", {230, 110, 20}, 2, 2, false},
    /* A_PDF      */ {"pdf", "Acrobat Reader", {200, 30, 40}, 2, 2, true},
    /* A_TERM     */ {"term", "Terminal", {34, 34, 38}, 2, 2, false},
    /* A_PS       */ {"ps", "PowerShell", {1, 36, 86}, 2, 2, false},
    /* A_CHESS    */ {"chess", "Chess", {120, 84, 50}, 2, 2, false},
    /* A_THEMES   */ {"themes", "Theme Store", {196, 40, 110}, 2, 2, true},
    /* A_CONV     */ {"conv", "Converter", {40, 110, 160}, 2, 2, false},
    /* A_BLOCKS   */ {"blocks", "Blocks", {30, 140, 190}, 2, 2, false},
    /* A_LIGHT    */ {"light", "Flashlight", {220, 170, 20}, 2, 2, false},
    /* A_WORLD    */ {"world", "World Clock", {70, 60, 160}, 2, 2, false},
    /* A_DICT     */ {"dict", "Dictionary", {140, 30, 40}, 2, 2, false},
    /* A_CORTANA  */ {"cortana", "Cortana", {0, 110, 200}, 2, 2, true},
    /* A_SKYPE    */ {"skype", "Skype", {0, 175, 240}, 2, 2, true},
    /* A_ONEDRIVE */ {"onedrive", "OneDrive", {20, 90, 180}, 2, 2, true},
    /* A_HELP     */ {"help", "Help+Tips", {232, 120, 20}, 2, 2, true},
    /* A_RECORDER */ {"recorder", "Sound Recorder", {196, 30, 60}, 2, 2, true},
    /* A_FOOD     */ {"food", "Food & Drink", {200, 90, 20}, 4, 2, true},
    /* A_HEALTH   */ {"health", "Health & Fitness", {0, 150, 100}, 4, 2, true},
    /* A_PET      */ {"pet", "Pixel Pet", {230, 120, 160}, 2, 2, true},
    /* A_CITY     */ {"city", "Metropolis", {30, 120, 150}, 4, 2, true},
};
static int appByKey(const std::string& k) {
    for (int i = 0; i < A_COUNT; i++) if (k == ADEF[i].key) return i;
    return -1;
}

// Apps that can be installed from the Store (all built in; "installing" adds a Start tile)
struct SApp { int app; const char* name; const char* cat; const char* d1; const char* d2; Col col; int w; const char* glyph; int stars; const char* rate; };
static const SApp STORE[] = {
    {A_BROWSER, "CMrhoe", "Internet", "Search the web or type an address.", "Bookmarks, history and three search engines (DuckDuckGo, Google, Bing). Pages open in the Switch web engine, so you need Wi-Fi.", {219, 68, 55}, 2, "C", 5, "2,431"},
    {A_NEWS, "News", "Internet", "Headlines from around the world.", "Reads live RSS feeds over Wi-Fi: BBC, Al Jazeera, The Verge, Hacker News and more. Tap a story to read the summary, or open the full page in CMrhoe.", {196, 38, 38}, 2, "N", 4, "3,812"},
    {A_ROBLOX, "Roblox", "Games", "Look up players and build with blocks.", "A companion app: search any Roblox player by name over Wi-Fi, open roblox.com in the Switch browser, and a block builder that saves automatically. The real Roblox game cannot run on the Switch.", {226, 35, 26}, 1, "R", 4, "1,204"},
    {A_WORD, "Word 2016", "Productivity", "Write letters and documents.", "Autosaves to your SD card. Bold, text size, word count.", {43, 87, 154}, 2, "W", 5, "9,812"},
    {A_PAINT, "Paint", "Art", "Draw with your finger.", "Brush, eraser, fill, line, rectangle and oval tools, 16 colours, undo. Save pictures you can open in Photos or put into Word.", {200, 70, 160}, 1, "P", 4, "5,320"},
    {A_SOLITAIRE, "Solitaire", "Games", "The classic card game.", "Klondike. Drag cards or tap to auto-move, undo, draw 1 or 3. Your game is saved.", {20, 110, 60}, 1, "SO", 5, "21,386"},
    {A_2048, "2048", "Games", "Swipe to merge tiles.", "Swipe or use the D-pad. Your board and best score are saved.", {237, 150, 50}, 1, "2048", 5, "12,904"},
    {A_MINES, "Minesweeper", "Games", "Clear the board without hitting a mine.", "Tap to dig, switch to Flag mode to mark mines. The first tap is always safe.", {90, 100, 125}, 1, "M", 4, "8,477"},
    {A_SNAKE, "Snake", "Games", "Swipe to steer, eat, grow.", "Swipe or use the D-pad. Your best score is saved.", {60, 150, 80}, 1, "SN", 4, "6,230"},
    {A_EXCEL, "Excel 2016", "Productivity", "Spreadsheets with formulas.", "SUM, AVERAGE, MIN, MAX, COUNT and cell references like =A1+B2. Autosaves.", {33, 115, 70}, 2, "X", 5, "7,104"},
    {A_MAPS, "Maps", "Internet", "Streets and places from OpenStreetMap.", "Drag to pan, pinch or double-tap to zoom, search for any place, and see where you are. Needs Wi-Fi; tiles you've seen are kept on the SD card.", {30, 150, 90}, 2, "M", 4, "5,044"},
    {A_CAMERA, "Camera", "Tools", "Take pictures with the Joy-Con IR camera.", "Uses the infrared sensor in the right Joy-Con as a very low-resolution camera. Snapshots are saved to Pictures. Beta: needs a Joy-Con attached.", {70, 74, 86}, 1, "C", 3, "1,882"},
    {A_PODCASTS, "Podcasts", "Internet", "Subscribe, download and listen.", "Add any podcast RSS link or pick a starter show. Episodes download to your SD card and play in the Music engine.", {150, 60, 200}, 2, "P", 4, "3,610"},
    {A_RADIO, "Radio", "Internet", "Internet radio stations.", "Beta: streams station URLs over Wi-Fi. Add your own streams.", {230, 110, 20}, 1, "R", 3, "2,277"},
    {A_TERM, "Terminal", "Tools", "A command line for your SD card.", "ls, cd, cat, cp, mv, rm, mkdir, ps, launch and more.", {34, 34, 38}, 1, ">_", 4, "2,904"},
    {A_PS, "PowerShell", "Tools", "Windows-style commands for your SD card.", "Get-ChildItem, Set-Location, Get-Content, Copy-Item and friends, plus the Terminal commands.", {1, 36, 86}, 1, "PS", 4, "2,311"},
    {A_CHESS, "Chess", "Games", "Play chess against the computer.", "Full rules: castling, en passant, promotion, check and checkmate. Three difficulty levels. Your game is saved.", {120, 84, 50}, 1, "K", 5, "9,431"},
    {A_CONV, "Converter", "Tools", "Convert units quickly.", "Length, mass, temperature, volume, speed, data and time. Tap a unit, type a number.", {40, 110, 160}, 1, "U", 4, "4,120"},
    {A_BLOCKS, "Blocks", "Games", "Stack falling blocks.", "Touch buttons or the D-pad. Clear lines, level up. Your best score is saved.", {30, 140, 190}, 1, "B", 5, "15,208"},
    {A_LIGHT, "Flashlight", "Tools", "Turn the screen into a lamp.", "A bright full-screen light with brightness and colour. Tap to change colour.", {220, 170, 20}, 1, "F", 4, "3,350"},
    {A_WORLD, "World Clock", "Tools", "Time in cities around the world.", "Pick cities and see their local time at a glance.", {70, 60, 160}, 1, "W", 4, "2,640"},
    {A_DICT, "Dictionary", "Productivity", "Look up English words.", "Type a word to see its meaning and examples. Needs Wi-Fi.", {140, 30, 40}, 1, "D", 4, "2,018"},
};
static const int NSTORE = (int)(sizeof(STORE) / sizeof(STORE[0]));
static bool gInst[A_COUNT];
static float gAppMemMB[A_COUNT]; static bool gSkyDirty = false; static int gSetOpenPage = -1; static int gCalcMode = 0; static void calcDrawMode(const Theme&); static void calcModeBtn(int y); static void calcMenuDraw(); static bool calcTapMode(int, int); static void calcDragMode(int, int); static bool calcBack(); static void gmInit(); static void gmUpdate(); static void gamesTap(int, int); static bool gamesBack(); static void skySave(); static float tmUsedMB();
static const char* CATS[5] = {"Games", "Productivity", "Internet", "Art", "Tools"};
static bool appAvail(int a) { return a >= 0 && a < A_COUNT && (ADEF[a].sys || gInst[a]); }
static std::string appName(int app) { return (app >= 0 && app < A_COUNT) ? ADEF[app].name : "App"; }

// ---- Start screen tiles live on a grid of half-cells: small 1x1, medium 2x2, wide 4x2, large 4x4. A group is 4 columns wide and 6 rows tall.
struct Tile { int c, r, w, h, app; bool live; int tint = 0; };
static std::vector<Tile> gTiles;
static bool liveAvailable(const Tile& t, int rw, int rh);
static Col tileColor(const Tile& t) { return (t.tint > 0 && t.tint < NTINT) ? TINTS[t.tint] : ADEF[t.app].col; }
static std::vector<std::string> gGroups;   // group names; group g is half-columns 4g..4g+3
static const int PIT = 82, GAP = 8, LEFT = 90, TOP = 140, GGAP = 36, GCOLS = 4, GROWS = 6;
static float scrollX = 0, scrollV = 0, maxScroll = 0;
static int sel = 0;

static int colX(int c) { return LEFT + c * PIT + GGAP * (c / GCOLS); }
static float xToCol(float x) {
    float gw = GCOLS * PIT + GGAP, rel = x - LEFT;
    int g = (int)floorf(rel / gw);
    float rem = rel - g * gw;
    if (rem > GCOLS * PIT) rem = (float)(GCOLS * PIT);
    return g * GCOLS + rem / PIT;
}
static SDL_Rect tileRectCells(int c, int r, int w, int h) {
    SDL_Rect q;
    q.x = colX(c); q.y = TOP + r * PIT; q.w = w * PIT - GAP; q.h = h * PIT - GAP;
    return q;
}
static SDL_Rect tileBase(int i) { const Tile& t = gTiles[i]; return tileRectCells(t.c, t.r, t.w, t.h); }
static int numGroups() {
    int g = 0;
    for (auto& t : gTiles) g = std::max(g, t.c / GCOLS + 1);
    return g;
}
static bool slotFree(int c, int r, int w, int h, int ignore) {
    if (c < 0 || r < 0 || r + h > GROWS || (c % GCOLS) + w > GCOLS) return false;
    for (int i = 0; i < (int)gTiles.size(); i++) {
        if (i == ignore) continue;
        const Tile& t = gTiles[i];
        if (c < t.c + t.w && t.c < c + w && r < t.r + t.h && t.r < r + h) return false;
    }
    return true;
}
static void autoPlace(int w, int h, int& oc, int& orow) {
    for (int c = 0;; c++) {
        for (int r = 0; r + h <= GROWS; r++) if (slotFree(c, r, w, h, -1)) { oc = c; orow = r; return; }
    }
}
// nearest valid slot to a wanted pixel position (top-left of the tile)
static bool snapSlot(int idx, int w, int h, float wantX, float wantY, int& oc, int& orow) {
    int ng = numGroups();
    float best = 1e18f; bool found = false;
    for (int c = 0; c < GCOLS * (ng + 1); c++)
        for (int r = 0; r + h <= GROWS; r++) {
            if (!slotFree(c, r, w, h, idx)) continue;
            float dx = colX(c) - wantX, dy = (float)(TOP + r * PIT) - wantY, d = dx * dx + dy * dy;
            if (d < best) { best = d; oc = c; orow = r; found = true; }
        }
    return found;
}
static void compactGroups() {
    int ng = numGroups();
    std::vector<int> map(ng + 1, -1);
    std::vector<bool> used(ng + 1, false);
    for (auto& t : gTiles) used[t.c / GCOLS] = true;
    int n = 0;
    for (int g = 0; g < ng; g++) if (used[g]) map[g] = n++;
    std::vector<std::string> names(n);
    for (int g = 0; g < ng; g++) if (used[g] && g < (int)gGroups.size()) names[map[g]] = gGroups[g];
    for (auto& t : gTiles) t.c = map[t.c / GCOLS] * GCOLS + t.c % GCOLS;
    gGroups = names;
}
static void moveGroup(int from, int to) {
    int ng = numGroups();
    if (from == to || from < 0 || to < 0 || from >= ng || to >= ng) return;
    std::vector<int> order;
    for (int g = 0; g < ng; g++) if (g != from) order.push_back(g);
    order.insert(order.begin() + to, from);
    std::vector<int> pos(ng);
    for (int i = 0; i < ng; i++) pos[order[i]] = i;
    gGroups.resize(ng);
    std::vector<std::string> names(ng);
    for (int g = 0; g < ng; g++) names[pos[g]] = gGroups[g];
    for (auto& t : gTiles) t.c = pos[t.c / GCOLS] * GCOLS + t.c % GCOLS;
    gGroups = names;
}
static int tileIndexFor(int app) {
    for (int i = 0; i < (int)gTiles.size(); i++) if (gTiles[i].app == app) return i;
    return -1;
}
static void recalcScroll() {
    int m = 0;
    for (int i = 0; i < (int)gTiles.size(); i++) { SDL_Rect r = tileBase(i); m = std::max(m, r.x + r.w); }
    maxScroll = std::max(0.0f, (float)(m + 90 - W));
    scrollX = clampf(scrollX, 0, maxScroll);
    if (sel >= (int)gTiles.size()) sel = 0;
}
static void pinApp(int app) {
    if (!appAvail(app) || tileIndexFor(app) >= 0) return;
    Tile t; t.app = app; t.w = ADEF[app].w; t.h = ADEF[app].h; t.live = true;
    autoPlace(t.w, t.h, t.c, t.r);
    gTiles.push_back(t);
    while ((int)gGroups.size() < numGroups()) gGroups.push_back("");
    recalcScroll();
}
static void unpinApp(int app) {
    int i = tileIndexFor(app);
    if (i < 0) return;
    gTiles.erase(gTiles.begin() + i);
    compactGroups();
    recalcScroll();
}
static void defaultLayout() {
    static const int D[][5] = {{A_MAIL, 0, 0, 4, 2}, {A_MUSIC, 0, 2, 2, 2}, {A_PHOTOS, 2, 2, 2, 2}, {A_CALENDAR, 0, 4, 4, 2},
                               {A_STORE, 4, 0, 4, 4}, {A_CALC, 4, 4, 2, 2}, {A_NOTES, 6, 4, 2, 2}, {A_WEATHER, 8, 0, 4, 2},
                               {A_CLOCK, 8, 2, 4, 4}, {A_SETTINGS, 12, 0, 2, 2}, {A_POWER, 14, 0, 2, 2}, {A_GAMES, 12, 2, 4, 4}};
    gTiles.clear();
    for (auto& d : D) gTiles.push_back(Tile{d[1], d[2], d[3], d[4], d[0], true});
    gGroups.assign(4, "");
    gGroups[0] = "Life at a glance"; gGroups[1] = "Play and explore";
    for (int a = 0; a < A_COUNT; a++) if (gInst[a] && !ADEF[a].sys) pinApp(a);
}
static std::string layoutText() {
    std::string d;
    for (int g = 0; g < (int)gGroups.size(); g++) d += "G\t" + std::to_string(g) + "\t" + gGroups[g] + "\n";
    for (auto& t : gTiles) d += std::string("T\t") + ADEF[t.app].key + "\t" + std::to_string(t.c) + "\t" + std::to_string(t.r) + "\t" + std::to_string(t.w) + "\t" + std::to_string(t.h) + "\t" + (t.live ? "1" : "0") + "\t" + std::to_string(t.tint) + "\n";
    return d;
}
static bool gLayoutDirty = false;
static void saveLayout() { writeFile(DIR_ + "layout.txt", layoutText()); gLayoutDirty = false; }
static bool loadLayout() {
    std::string d;
    if (!readFile(DIR_ + "layout.txt", d)) return false;
    gTiles.clear(); gGroups.clear();
    std::vector<Tile> pending;
    size_t pos = 0;
    while (pos < d.size()) {
        size_t e = d.find('\n', pos);
        if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(pos, e - pos); pos = e + 1;
        std::vector<std::string> f; size_t a = 0;
        while (true) { size_t b = ln.find('\t', a); if (b == std::string::npos) { f.push_back(ln.substr(a)); break; } f.push_back(ln.substr(a, b - a)); a = b + 1; }
        if (f.size() >= 3 && f[0] == "G") {
            int g = atoi(f[1].c_str());
            if (g >= 0 && g < 64) { if ((int)gGroups.size() <= g) gGroups.resize(g + 1); gGroups[g] = f[2]; }
        } else if (f.size() >= 7 && f[0] == "T") {
            int app = appByKey(f[1]);
            if (!appAvail(app)) continue;
            Tile t; t.app = app; t.c = atoi(f[2].c_str()); t.r = atoi(f[3].c_str()); t.w = atoi(f[4].c_str()); t.h = atoi(f[5].c_str()); t.live = f[6] != "0"; if (f.size() >= 8) { t.tint = atoi(f[7].c_str()); if (t.tint < 0 || t.tint >= NTINT) t.tint = 0; }
            if (!((t.w == 1 && t.h == 1) || (t.w == 2 && t.h == 2) || (t.w == 4 && t.h == 2) || (t.w == 4 && t.h == 4))) { t.w = ADEF[app].w; t.h = ADEF[app].h; }
            if (tileIndexFor(app) >= 0) continue;
            if (slotFree(t.c, t.r, t.w, t.h, -1)) gTiles.push_back(t); else pending.push_back(t);
        }
    }
    for (auto& t : pending) { Tile n = t; autoPlace(n.w, n.h, n.c, n.r); gTiles.push_back(n); }
    if (gTiles.empty()) return false;
    while ((int)gGroups.size() < numGroups()) gGroups.push_back("");
    return true;
}
static void rebuildTiles() {
    if (!loadLayout()) defaultLayout();
    compactGroups();
    recalcScroll();
}

// ---------------------------------------------------------------- state
static int curApp = -1, lastApp = -1, lastTile = -1, appDir = 0;
static int gRenameG = -1;
static bool gCloseReset = false;
static bool running[A_COUNT];
static float appT = 0, introT = 0, barT = 0, barP = 0;
static bool navMode = false;
static Uint32 lastInput = 0;
static float dimA = 0;
// ---- Windows 8 shell state: boot -> lock screen -> start; charms bar, app switcher, search charm
static int uiState = 0;                  // 0 boot animation, 1 lock screen, 2 desktop/start
static float bootT = 0, lockOff = 0;
static bool lockAnim = false;
static bool charmsOpen = false, switchOpen = false, searchOpen = false;
static float charmsP = 0, switchP = 0, searchP = 0;
static std::string gQuery, gToastS, gArg0, gHbMsg;
static int gHbWhich = 0;
static Uint32 gToastT = 0;
static void toast(const std::string& m) { gToastS = m; gToastT = SDL_GetTicks(); }
static bool notifOpen = false;
static void notify(int app, const std::string& title, const std::string& body);
static bool overlayOpen() { return charmsOpen || switchOpen || searchOpen || notifOpen; }
static void closeOverlays() { charmsOpen = switchOpen = searchOpen = notifOpen = false; }
static void searchGo();
static void hbRefresh();

static Col appBg(int app) {
    switch (app) {
        case A_CALC: return Col{31, 31, 31};
        case A_CLOCK: return Col{0, 84, 88};
        case A_SETTINGS: return Col{38, 38, 38};
        case A_POWER: return Col{27, 27, 27};
        case A_PHOTOS: return Col{0, 0, 0};
        case A_MUSIC: return Col{222, 89, 26};
        case A_WORD: return Col{43, 87, 154};
        case A_EXCEL: return Col{33, 115, 70};
        case A_STORE: return Col{0, 140, 0};
        case A_BROWSER: return Col{219, 68, 55};
        case A_PAINT: return Col{58, 58, 64};
        case A_2048: return Col{250, 248, 239};
        case A_SOLITAIRE: return Col{20, 110, 60};
        case A_MINES: return Col{44, 48, 60};
        case A_SNAKE: return Col{24, 34, 28};
        default: break;
    }
    if (app >= 0 && app < A_COUNT) return ADEF[app].col;
    return Col{40, 40, 40};
}

// ---------------------------------------------------------------- settings persistence (settings.ini; the old binary settings.cfg is still read once)
static bool gAnim = true, gNotif = true, gOffline = false, gLocOK = true, gTempF = false, gHiCon = false;
static int gLockPic = 0, gBgPat = 1, gAvatar = 0, gWeekStart = 0, gMailLock = 1, gWeatherLock = 1, gCalLock = 1, gMusicLock = 1;
static std::string gPin;
static std::string gUserStatus;
static bool gAuth = false, gSetup = false;   // gAuth: sign-in on the lock screen; gSetup: changing it in PC settings
static void authTouch(int phase, int x, int y); static void authUpdate(float dt); static void authKeys(u64 kd); static void authCancel(); static bool gAvInitial = false;
static bool gV8 = false, gPhone = false, gPhoneFlip = false; static int swDragApp = -1, swDragX = 0, swDragY = 0; static int gSnap = -1; static bool gStartPic = false, gSci = false, gSciDeg = true;
static bool gInstLegacy = false;
static void saveCfg() {
    std::string b;
    auto kv = [&](const char* k, int v) { b += std::string(k) + "=" + std::to_string(v) + "\n"; };
    kv("v8", gV8); kv("phone", gPhone); kv("phoneflip", gPhoneFlip); kv("theme", gTheme); kv("thowned", (int)gThOwned); kv("burn", gBurn); kv("h24", g24); kv("docfont", gDocFont); kv("bold", gBold); kv("engine", gEngine);
    kv("lockscreen", gLockScr); kv("bootanim", gBootAnim); kv("anim", gAnim); kv("notif", gNotif); kv("lockpic", gLockPic);
    kv("bgpat", gBgPat); kv("avatar", gAvatar); kv("weekstart", gWeekStart); kv("offline", gOffline); kv("location", gLocOK);
    kv("tempf", gTempF); kv("hicon", gHiCon); kv("lockmail", gMailLock); kv("lockweather", gWeatherLock);
    kv("lockcal", gCalLock); kv("lockmusic", gMusicLock); kv("startpic", gStartPic); kv("sci", gSci); kv("scideg", gSciDeg);
    b += "pin=" + gPin + "\n";
    b += "installed=";
    for (int a = 0; a < A_COUNT; a++) if (gInst[a] && !ADEF[a].sys) b += std::string(ADEF[a].key) + ",";
    b += "\n";
    writeFile(DIR_ + "settings.ini", b);
}
static void loadLegacyCfg(const std::string& s) {
    unsigned char m = (unsigned char)s[0];
    if (m == 0xA7) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        if (s.size() >= 8) {
            gInst[A_WORD] = s[4] != 0; gInst[A_EXCEL] = s[5] != 0;
            gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[6]));
            gBold = s[7] != 0;
        }
    } else if (m == 0xA8 && s.size() >= 7) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[4]));
        gBold = s[5] != 0; gEngine = (unsigned char)s[6] % 3;
        for (int a = 0; a < 25 && 7 + a < (int)s.size(); a++) gInst[a] = s[7 + a] != 0;
    } else if ((m == 0xA9 || m == 0xAA) && s.size() >= 9) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[4]));
        gBold = s[5] != 0; gEngine = (unsigned char)s[6] % 3; gLockScr = s[7] != 0; gBootAnim = s[8] != 0;
        size_t off = 9;
        if (m == 0xAA && s.size() >= 12) { gAnim = s[9] != 0; gNotif = s[10] != 0; gLockPic = (unsigned char)s[11] % 6; off = 12; }
        for (int a = 0; a < 25 && off + a < s.size(); a++) gInst[a] = s[off + a] != 0;
    }
}
static void loadCfg() {
    std::string s;
    if (readFile(DIR_ + "settings.ini", s)) {
        size_t pos = 0;
        while (pos < s.size()) {
            size_t e = s.find('\n', pos);
            if (e == std::string::npos) e = s.size();
            std::string ln = s.substr(pos, e - pos); pos = e + 1;
            size_t q = ln.find('=');
            if (q == std::string::npos) continue;
            std::string k = ln.substr(0, q), v = ln.substr(q + 1);
            int n = atoi(v.c_str());
            if (k == "theme") gTheme = std::max(0, std::min(12 + NSTH - 1, n)); else if (k == "thowned") gThOwned = (unsigned)n; else if (k == "v8") gV8 = n != 0; else if (k == "phone") gPhone = false; else if (k == "phoneflip") gPhoneFlip = n != 0;
            else if (k == "burn") gBurn = n != 0; else if (k == "h24") g24 = n != 0;
            else if (k == "docfont") gDocFont = std::min(F44, std::max(F16, n));
            else if (k == "bold") gBold = n != 0; else if (k == "engine") gEngine = std::max(0, std::min(2, n));
            else if (k == "lockscreen") gLockScr = n != 0; else if (k == "bootanim") gBootAnim = n != 0;
            else if (k == "anim") gAnim = n != 0; else if (k == "notif") gNotif = n != 0;
            else if (k == "lockpic") gLockPic = std::max(0, std::min(5, n));
            else if (k == "bgpat") gBgPat = std::max(0, std::min(NPAT - 1, n));
            else if (k == "avatar") gAvatar = std::max(0, std::min(7, n));
            else if (k == "weekstart") gWeekStart = (n == 1 || n == 6) ? n : 0;
            else if (k == "offline") gOffline = n != 0; else if (k == "location") gLocOK = n != 0;
            else if (k == "tempf") gTempF = n != 0; else if (k == "hicon") gHiCon = n != 0;
            else if (k == "lockmail") gMailLock = n; else if (k == "lockweather") gWeatherLock = n;
            else if (k == "startpic") gStartPic = n != 0; else if (k == "sci") gSci = n != 0; else if (k == "scideg") gSciDeg = n != 0; else if (k == "lockcal") gCalLock = n; else if (k == "lockmusic") gMusicLock = n;
            else if (k == "pin") gPin = v;
            else if (k == "installed") {
                size_t a = 0;
                while (a < v.size()) {
                    size_t b = v.find(',', a);
                    if (b == std::string::npos) b = v.size();
                    int app = appByKey(v.substr(a, b - a));
                    if (app >= 0) gInst[app] = true;
                    a = b + 1;
                }
            }
        }
        return;
    }
    if (readFile(DIR_ + "settings.cfg", s) && s.size() >= 4) { loadLegacyCfg(s); gInstLegacy = true; }
}
static void loadUser() {
    AccountUid uid;
    if (R_FAILED(accountGetPreselectedUser(&uid))) {
        if (R_FAILED(accountGetLastOpenedUser(&uid))) return;
    }
    AccountProfile prof;
    if (R_SUCCEEDED(accountGetProfile(&prof, uid))) {
        AccountProfileBase base;
        memset(&base, 0, sizeof base);
        if (R_SUCCEEDED(accountProfileGet(&prof, NULL, &base)) && base.nickname[0]) gUser = base.nickname;
        accountProfileClose(&prof);
    }
}

// ---------------------------------------------------------------- text editors (Notes, Word)
struct Editor {
    std::string s;
    int scroll = 0;
    bool dirty = false, follow = false;
    std::string cacheS; int cacheF = -1, cacheW = -1;
    std::vector<std::string> lines;
};
static Editor gNote, gDoc;

static std::vector<std::string> wrapText(const std::string& s, int f, int maxW) {
    std::vector<std::string> out;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string t = line.empty() ? word : line + " " + word;
        if (line.empty() || tw(t, f) <= maxW) line = t;
        else { out.push_back(line); line = word; }
        word.clear();
    };
    for (char c : s) {
        if (c == '\n') { flushWord(); out.push_back(line); line.clear(); }
        else if (c == ' ') flushWord();
        else {
            word += c;
            if (word.size() >= 8 && tw(word, f) > maxW) {
                std::string head = word.substr(0, word.size() - 1);
                if (!line.empty()) { out.push_back(line); line.clear(); }
                out.push_back(head);
                word = std::string(1, c);
            }
        }
    }
    flushWord();
    out.push_back(line);
    return out;
}
static const std::vector<std::string>& wrapCached(Editor& e, int f, int w) {
    if (e.cacheF != f || e.cacheW != w || e.cacheS != e.s) {
        e.lines = wrapText(e.s, f, w);
        e.cacheS = e.s; e.cacheF = f; e.cacheW = w;
    }
    return e.lines;
}
static void drawEditor(Editor& e, SDL_Rect a, int f, Col fg, bool bold, bool focused, const char* placeholder) {
    if (!gFont[f]) return;
    const std::vector<std::string>& L = wrapCached(e, f, a.w);
    int lh = TTF_FontLineSkip(gFont[f]);
    int total = (int)L.size() * lh;
    int maxS = std::max(0, total - a.h + lh / 2);
    if (e.follow) { e.scroll = maxS; e.follow = false; }
    e.scroll = std::max(0, std::min(e.scroll, maxS));
    SDL_RenderSetClipRect(gR, &a);
    if (e.s.empty() && placeholder) text(placeholder, f, a.x, a.y, fg, 0, 110);
    for (int i = 0; i < (int)L.size(); i++) {
        int y = a.y + i * lh - e.scroll;
        if (y + lh < a.y || y > a.y + a.h) continue;
        text(L[i], f, a.x, y, fg);
        if (bold) text(L[i], f, a.x + 1, y, fg);
    }
    if (focused && ((SDL_GetTicks() / 500) % 2 == 0)) {
        int y = a.y + ((int)L.size() - 1) * lh - e.scroll;
        rect(a.x + tw(L.back(), f) + 2, y + 3, 2, lh - 6, fg);
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static int wordCount(const std::string& s) {
    int n = 0; bool in = false;
    for (char c : s) {
        bool sp = (c == ' ' || c == '\n');
        if (!sp && !in) { n++; in = true; } else if (sp) in = false;
    }
    return n;
}

// ---------------------------------------------------------------- Excel engine
static const int XR = 60, XC = 10, XVC = 8, XROWH = 34, XTOP = 236, XHDRW = 50, XCOLW = 150;
static std::string xsAll[3][XR][XC];          // three worksheets
static unsigned short xfAll[3][XR][XC];       // cell format bits (see below)
static int xSheet = 0;
#define xs (xsAll[xSheet])
#define xf (xfAll[xSheet])
// xf bits: 0 bold, 1 italic, 2-3 align (0 auto, 1 left, 2 centre, 3 right), 4-5 number format (0 general, 1 0.00, 2 $0.00, 3 %), 6-8 fill colour
static int xr = 0, xc = 0, xScrR = 0, xScrC = 0;
static std::string xEdit;
static bool xEditing = false, xDirty = false, xGrid = true;
static int xTab = 1;
static std::string xClip;
static unsigned short xClipF = 0;

static bool cellVal(int r, int c, int depth, double& v, bool& err);
struct XP {
    std::string s; size_t p; int depth; bool ok;
    void ws() { while (p < s.size() && s[p] == ' ') p++; }
    bool ref(int& r, int& c) {
        size_t q = p;
        if (q < s.size() && isalpha((unsigned char)s[q])) {
            int cc = toupper((unsigned char)s[q]) - 'A';
            q++;
            if (cc < 0 || cc >= XC) return false;
            size_t st = q; int rr = 0;
            while (q < s.size() && isdigit((unsigned char)s[q])) { rr = rr * 10 + (s[q] - '0'); q++; }
            if (q == st || rr < 1 || rr > XR) return false;
            r = rr - 1; c = cc; p = q;
            return true;
        }
        return false;
    }
    double expr() {
        double v = term();
        for (;;) {
            ws();
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) { char o = s[p++]; double t = term(); v = (o == '+') ? v + t : v - t; }
            else break;
        }
        return v;
    }
    double term() {
        double v = fac();
        for (;;) {
            ws();
            if (p < s.size() && (s[p] == '*' || s[p] == '/')) {
                char o = s[p++]; double t = fac();
                if (o == '*') v *= t; else { if (t == 0) ok = false; else v /= t; }
            } else break;
        }
        return v;
    }
    double fac() {
        ws();
        if (p >= s.size()) { ok = false; return 0; }
        char ch = s[p];
        if (ch == '-') { p++; return -fac(); }
        if (ch == '(') {
            p++; double v = expr(); ws();
            if (p < s.size() && s[p] == ')') p++; else ok = false;
            return v;
        }
        if (isdigit((unsigned char)ch) || ch == '.') {
            char* e; double v = strtod(s.c_str() + p, &e);
            p = (size_t)(e - s.c_str());
            return v;
        }
        if (isalpha((unsigned char)ch)) {
            size_t q = p; std::string name;
            while (q < s.size() && isalpha((unsigned char)s[q])) name += (char)toupper((unsigned char)s[q++]);
            if (q < s.size() && s[q] == '(') {
                p = q + 1; ws();
                int r0 = 0, c0 = 0, r1 = 0, c1 = 0;
                if (!ref(r0, c0)) { ok = false; return 0; }
                r1 = r0; c1 = c0; ws();
                if (p < s.size() && s[p] == ':') { p++; ws(); if (!ref(r1, c1)) { ok = false; return 0; } }
                ws();
                if (p < s.size() && s[p] == ')') p++; else { ok = false; return 0; }
                std::vector<double> vals;
                for (int r = std::min(r0, r1); r <= std::max(r0, r1); r++)
                    for (int c = std::min(c0, c1); c <= std::max(c0, c1); c++) {
                        double v = 0; bool er = false;
                        if (cellVal(r, c, depth + 1, v, er)) vals.push_back(v);
                        if (er) ok = false;
                    }
                double sum = 0, mn = 0, mx = 0;
                for (size_t i = 0; i < vals.size(); i++) { sum += vals[i]; if (i == 0 || vals[i] < mn) mn = vals[i]; if (i == 0 || vals[i] > mx) mx = vals[i]; }
                if (name == "SUM") return sum;
                if (name == "AVERAGE" || name == "AVG") { if (vals.empty()) { ok = false; return 0; } return sum / vals.size(); }
                if (name == "MIN") return mn;
                if (name == "MAX") return mx;
                if (name == "COUNT") return (double)vals.size();
                ok = false; return 0;
            }
            int r, c;
            if (ref(r, c)) {
                double v = 0; bool er = false;
                if (cellVal(r, c, depth + 1, v, er)) return v;
                if (er) ok = false;
                return 0;
            }
            ok = false; return 0;
        }
        ok = false; return 0;
    }
};
static bool cellVal(int r, int c, int depth, double& v, bool& err) {
    const std::string& raw = xs[r][c];
    if (raw.empty()) return false;
    if (raw[0] == '=') {
        if (depth > 16) { err = true; return false; }
        XP p; p.s = raw.substr(1); p.p = 0; p.depth = depth; p.ok = true;
        double x = p.expr(); p.ws();
        if (!p.ok || p.p != p.s.size()) { err = true; return false; }
        v = x; return true;
    }
    char* e; const char* st = raw.c_str();
    double x = strtod(st, &e);
    if (e != st && *e == '\0') { v = x; return true; }
    return false;
}
static std::string fmtNum(double v) { char b[40]; snprintf(b, sizeof b, "%.10g", v); return b; }
static std::string cellDisplay(int r, int c) {
    const std::string& raw = xs[r][c];
    if (raw.empty() || raw[0] != '=') return raw;
    double v = 0; bool er = false;
    if (cellVal(r, c, 0, v, er)) return fmtNum(v);
    return "#ERR";
}
static bool looksNumber(const std::string& s) {
    if (s.empty()) return false;
    char* e; const char* st = s.c_str();
    strtod(st, &e);
    return e != st && *e == '\0';
}
static std::string sheetToCsv() {
    int last = -1;
    for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) if (!xs[r][c].empty()) last = r;
    std::string out;
    for (int r = 0; r <= last; r++) {
        for (int c = 0; c < XC; c++) {
            const std::string& v = xs[r][c];
            if (c) out += ',';
            if (v.find_first_of(",\"\n") != std::string::npos) {
                out += '"';
                for (char ch : v) { if (ch == '"') out += '"'; out += ch; }
                out += '"';
            } else out += v;
        }
        out += '\n';
    }
    return out;
}
static void csvToSheet(const std::string& d) {
    for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear();
    int r = 0, c = 0; bool q = false; std::string cur;
    auto put = [&]() { if (r < XR && c < XC) xs[r][c] = cur; cur.clear(); };
    for (size_t i = 0; i < d.size(); i++) {
        char ch = d[i];
        if (q) {
            if (ch == '"') { if (i + 1 < d.size() && d[i + 1] == '"') { cur += '"'; i++; } else q = false; }
            else cur += ch;
        } else if (ch == '"') q = true;
        else if (ch == ',') { put(); c++; }
        else if (ch == '\n') { put(); c = 0; r++; }
        else if (ch != '\r') cur += ch;
    }
    if (!cur.empty()) put();
}
static int xVisRows() { int bottom = 0; (void)bottom; return 1; }

static std::string xFile(int sh, const char* ext) { return DIR_ + "sheet" + std::to_string(sh + 1) + ext; }
static void xfSave() {
    std::string d;
    for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) if (xf[r][c]) d += std::to_string(r) + " " + std::to_string(c) + " " + std::to_string((int)xf[r][c]) + "\n";
    if (d.empty()) { remove(xFile(xSheet, ".fmt").c_str()); return; }
    writeFile(xFile(xSheet, ".fmt"), d);
}
static void xfLoad() {
    memset(xf, 0, sizeof(xf));
    std::string d;
    if (!readFile(xFile(xSheet, ".fmt"), d)) return;
    int r, c, v, off = 0, n;
    const char* p = d.c_str();
    while (sscanf(p + off, "%d %d %d%n", &r, &c, &v, &n) == 3) { if (r >= 0 && r < XR && c >= 0 && c < XC) xf[r][c] = (unsigned short)v; off += n; }
}
static void xSaveSheet() { writeFile(xFile(xSheet, ".csv"), sheetToCsv()); xfSave(); }
static void xLoadAll() {
    for (int sh = 0; sh < 3; sh++) {
        xSheet = sh;
        std::string d;
        for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear();
        if (readFile(xFile(sh, ".csv"), d)) csvToSheet(d);
        xfLoad();
    }
    xSheet = 0;
}
static std::string xShown(int r, int c) { // display text honouring the number format
    std::string d = cellDisplay(r, c);
    int nf = (xf[r][c] >> 4) & 3;
    if (nf && looksNumber(d)) {
        double v = strtod(d.c_str(), nullptr);
        char b[48];
        if (nf == 1) snprintf(b, sizeof b, "%.2f", v);
        else if (nf == 2) snprintf(b, sizeof b, "$%.2f", v);
        else snprintf(b, sizeof b, "%.0f%%", v * 100);
        return b;
    }
    return d;
}
static std::string xAddr(int r, int c) { return std::string(1, (char)('A' + c)) + std::to_string(r + 1); }

static bool kbOpen = false;
static std::string gUrl, storeSearch;
static void browserGo();
static std::string gCalIn;
static void calAdd();
static void wType(const std::string& t);
static void wBack();
static void appTouch(int phase, int x, int y);
static void appUpdate(float dt);
static void appKeys(u64 kd);
static std::string* gKbStr = nullptr;
static size_t gKbMax = 100;
static bool gKbMulti = false;
static void (*gKbEnter)() = nullptr;
static int kbTarget = 0; // 1 notes, 2 word, 3 excel cell, 4 browser address, 5 store search, 6 search charm
static bool kbShift = false, kbSym = false;
static int xRows() { int bottom = kbOpen ? KBTOP : UIH - 28; return std::max(1, (bottom - XTOP) / XROWH); }
static void xEnsureVisible() {
    int rows = xRows();
    if (xr < xScrR) xScrR = xr;
    if (xr >= xScrR + rows) xScrR = xr - rows + 1;
    if (xc < xScrC) xScrC = xc;
    if (xc >= xScrC + XVC) xScrC = xc - XVC + 1;
    xScrR = std::max(0, std::min(xScrR, XR - 1));
    xScrC = std::max(0, std::min(xScrC, XC - XVC));
}
static void commitCell() {
    if (xEditing) { xs[xr][xc] = xEdit; xEditing = false; xDirty = true; }
}
static void startEdit() { if (!xEditing) { xEdit = xs[xr][xc]; xEditing = true; } }

// ---------------------------------------------------------------- on-screen keyboard
struct Key { SDL_Rect r; std::string label, act; };
static std::vector<Key> gKeys;
static void buildKeys() {
    gKeys.clear();
    const char* r1 = kbSym ? "1234567890" : "qwertyuiop";
    const char* r2 = kbSym ? "-/:;()$&@" : "asdfghjkl";
    const char* r3 = kbSym ? "%+*=#!?" : "zxcvbnm";
    auto add = [&](int x, int y, int w, const std::string& lab, const std::string& act) {
        Key k; k.r.x = x; k.r.y = y; k.r.w = w; k.r.h = 64; k.label = lab; k.act = act; gKeys.push_back(k);
    };
    int y0 = KBTOP + 10;
    for (int i = 0; i < 10; i++) { char c = r1[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(20 + i * 124, y0, 116, l, l); }
    for (int i = 0; i < 9; i++) { char c = r2[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(82 + i * 124, y0 + 72, 116, l, l); }
    add(20, y0 + 144, 160, "Shift", "SHIFT");
    for (int i = 0; i < 7; i++) { char c = r3[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(188 + i * 124, y0 + 144, 116, l, l); }
    add(1056, y0 + 144, 204, "Del", "BS");
    add(20, y0 + 216, 160, kbSym ? "abc" : "123", "SYM");
    add(188, y0 + 216, 100, ",", ",");
    add(296, y0 + 216, 520, "", " ");
    add(824, y0 + 216, 100, ".", ".");
    add(932, y0 + 216, 200, "Enter", "ENTER");
    add(1140, y0 + 216, 120, "Hide", "HIDE");
}
static int kbHit(int x, int y) {
    for (int i = 0; i < (int)gKeys.size(); i++) if (inR(x, y, gKeys[i].r.x, gKeys[i].r.y, gKeys[i].r.w, gKeys[i].r.h + 8)) return i;
    return -1;
}
static void kbType(const std::string& s) {
    if (kbTarget == 1) { gNote.s += s; gNote.dirty = true; gNote.follow = true; }
    else if (kbTarget == 2) wType(s);
    else if (kbTarget == 3) { if (!xEditing) { xEdit.clear(); xEditing = true; } xEdit += s; }
    else if (kbTarget == 4) gUrl += s;
    else if (kbTarget == 5) storeSearch += s;
    else if (kbTarget == 6) gQuery += s;
    else if (kbTarget == 7) gCalIn += s;
    else if (kbTarget == 99) { if (gKbStr && gKbStr->size() < gKbMax) *gKbStr += s; }
    else if (kbTarget == 9) { if (gRenameG >= 0 && gRenameG < (int)gGroups.size() && gGroups[gRenameG].size() < 40) gGroups[gRenameG] += s; }
}
static void kbBackspace() {
    if (kbTarget == 1) { if (!gNote.s.empty()) { gNote.s.pop_back(); gNote.dirty = true; gNote.follow = true; } }
    else if (kbTarget == 2) wBack();
    else if (kbTarget == 3) { startEdit(); if (!xEdit.empty()) xEdit.pop_back(); }
    else if (kbTarget == 4) { if (!gUrl.empty()) gUrl.pop_back(); }
    else if (kbTarget == 5) { if (!storeSearch.empty()) storeSearch.pop_back(); }
    else if (kbTarget == 6) { if (!gQuery.empty()) gQuery.pop_back(); }
    else if (kbTarget == 7) { if (!gCalIn.empty()) gCalIn.pop_back(); }
    else if (kbTarget == 99) { if (gKbStr && !gKbStr->empty()) { do gKbStr->pop_back(); while (!gKbStr->empty() && ((unsigned char)gKbStr->back() & 0xC0) == 0x80); } }
    else if (kbTarget == 9) { if (gRenameG >= 0 && gRenameG < (int)gGroups.size() && !gGroups[gRenameG].empty()) gGroups[gRenameG].pop_back(); }
}
static void openKb(int target) { kbOpen = true; kbTarget = target; kbShift = false; kbSym = false; }
static void hideKb() { if (kbTarget == 3) commitCell(); if (kbTarget == 9) { saveLayout(); gRenameG = -1; } kbOpen = false; }
static void kbAction(const std::string& a) {
    if (a == "SHIFT") kbShift = !kbShift;
    else if (a == "SYM") kbSym = !kbSym;
    else if (a == "BS") kbBackspace();
    else if (a == "ENTER") {
        if (kbTarget == 3) { commitCell(); xr = std::min(xr + 1, XR - 1); xEnsureVisible(); }
        else if (kbTarget == 4) browserGo();
        else if (kbTarget == 5) hideKb();
        else if (kbTarget == 6) searchGo();
        else if (kbTarget == 7) calAdd();
        else if (kbTarget == 9) hideKb();
        else if (kbTarget == 99) { if (gKbMulti) kbType("\n"); else if (gKbEnter) gKbEnter(); else hideKb(); }
        else kbType("\n");
    }
    else if (a == "HIDE") hideKb();
    else { kbType(a); if (kbShift && !kbSym) kbShift = false; }
}
static void drawKb() {
    rect(0, KBTOP, W, KBH, Col{30, 30, 34});
    buildKeys();
    for (auto& k : gKeys) {
        bool special = k.act.size() > 1;
        Col bg = special ? Col{52, 52, 58} : Col{68, 68, 74};
        if (k.act == "SHIFT" && kbShift) bg = THEMES[gTheme].accent;
        if (k.act == "ENTER") bg = THEMES[gTheme].accent;
        rect(k.r.x, k.r.y, k.r.w, k.r.h, bg);
        text(k.label, special ? F22 : F30, k.r.x + k.r.w / 2, k.r.y + (special ? 18 : 14), WHITE, 1);
    }
}

// ---------------------------------------------------------------- photos and games
static std::vector<std::string> gPhotos;
static int phIdx = 0, phW = 0, phH = 0;
static SDL_Texture* phTex = nullptr;
static float phZ = 1, phOX = 0, phOY = 0, phSlideT = 0, phDelArm = 0;
static Uint32 phTapT = 0; static int phTapX = 0, phTapY = 0;
static bool phSlide = false; static std::string phAlbum;
static SDL_Texture* gStartPicTex = nullptr;
static void lockRebuild();
static bool savePicCover(const std::string& src, const std::string& dst, int tw_, int th_) {
    SDL_Surface* sf = IMG_Load(src.c_str());
    if (!sf) return false;
    SDL_Surface* rgb = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(sf);
    if (!rgb) return false;
    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, tw_, th_, 32, SDL_PIXELFORMAT_RGBA32);
    if (!out) { SDL_FreeSurface(rgb); return false; }
    float k = std::max(tw_ / (float)rgb->w, th_ / (float)rgb->h);
    int dw = (int)(rgb->w * k + 0.5f), dh = (int)(rgb->h * k + 0.5f);
    SDL_Rect d = {(tw_ - dw) / 2, (th_ - dh) / 2, dw, dh};
    SDL_SetSurfaceBlendMode(rgb, SDL_BLENDMODE_NONE);
    SDL_BlitScaled(rgb, nullptr, out, &d);
    int r = IMG_SavePNG(out, dst.c_str());
    SDL_FreeSurface(rgb); SDL_FreeSurface(out);
    return r == 0;
}
static void startPicLoad() {
    if (gStartPicTex) { SDL_DestroyTexture(gStartPicTex); gStartPicTex = nullptr; }
    if (!gStartPic) return;
    SDL_Surface* sf = IMG_Load((DIR_ + "startbg.png").c_str());
    if (!sf) { gStartPic = false; return; }
    gStartPicTex = SDL_CreateTextureFromSurface(gR, sf);
    SDL_FreeSurface(sf);
}
static bool isImg(const std::string& n) {
    std::string l = lower(n);
    return endsWith(l, ".jpg") || endsWith(l, ".jpeg") || endsWith(l, ".jpe") || endsWith(l, ".png") || endsWith(l, ".bmp") || endsWith(l, ".gif") ||
           endsWith(l, ".webp") || endsWith(l, ".tga") || endsWith(l, ".tif") || endsWith(l, ".tiff") || endsWith(l, ".jfif");
}
static void scanDir(const std::string& dir, int depth, bool nro, std::vector<std::string>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = dir + "/" + n;
        struct stat st;
        if (stat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (depth > 0) scanDir(p, depth - 1, nro, out); }
        else if (nro ? (endsWith(lower(n), ".nro") && n != "WinNX.nro") : isImg(n)) out.push_back(p);
        if (out.size() > 3000) break;
    }
    closedir(d);
}
static void loadPhoto() {
    if (phTex) { SDL_DestroyTexture(phTex); phTex = nullptr; }
    if (gPhotos.empty()) return;
    SDL_Surface* sf = IMG_Load(gPhotos[phIdx].c_str());
    if (!sf) return;
    phW = sf->w; phH = sf->h;
    phTex = SDL_CreateTextureFromSurface(gR, sf);
    SDL_FreeSurface(sf);
    phZ = 1; phOX = phOY = 0; phSlideT = 0;
}
static void photoNav(int d) {
    if (gPhotos.empty()) return;
    phIdx = (phIdx + d + (int)gPhotos.size()) % (int)gPhotos.size();
    loadPhoto();
}
struct Game { std::string name, path; };
static std::vector<Game> gGames;
static int gamesScroll = 0;
static void scanGames() {
    std::vector<std::string> files;
    scanDir("sdmc:/switch", 2, true, files);
    gGames.clear();
    for (auto& p : files) {
        size_t s = p.find_last_of('/');
        std::string n = p.substr(s + 1);
        n = n.substr(0, n.size() - 4);
        gGames.push_back(Game{n, p});
    }
    std::sort(gGames.begin(), gGames.end(), [](const Game& a, const Game& b) { return lower(a.name) < lower(b.name); });
}

// ---------------------------------------------------------------- calculator
static std::string cCur = "0";
static double cAcc = 0;
static int cOp = 0; // 1 + 2 - 3 x 4 /
static bool cFresh = true, cErr = false;
struct CBtn { const char* l; int c, r, cw, rh; };
static const CBtn CBTNS[] = {
    {"C", 0, 0, 1, 1}, {"DEL", 1, 0, 1, 1}, {"\xC3\xB7", 2, 0, 1, 1}, {"\xC3\x97", 3, 0, 1, 1},
    {"7", 0, 1, 1, 1}, {"8", 1, 1, 1, 1}, {"9", 2, 1, 1, 1}, {"-", 3, 1, 1, 1},
    {"4", 0, 2, 1, 1}, {"5", 1, 2, 1, 1}, {"6", 2, 2, 1, 1}, {"+", 3, 2, 1, 1},
    {"1", 0, 3, 1, 1}, {"2", 1, 3, 1, 1}, {"3", 2, 3, 1, 1}, {"=", 3, 3, 1, 2},
    {"0", 0, 4, 2, 1}, {".", 2, 4, 1, 1},
};
static const int NCB = (int)(sizeof(CBTNS) / sizeof(CBTNS[0]));
static SDL_Rect calcRect(const CBtn& b) {
    SDL_Rect r;
    r.x = 348 + b.c * 146; r.y = 170 + b.r * 94;
    r.w = b.cw * 140 + (b.cw - 1) * 6; r.h = b.rh * 88 + (b.rh - 1) * 6;
    return r;
}
static void calcDo() {
    double b = atof(cCur.c_str()), r = cAcc;
    switch (cOp) {
        case 1: r += b; break;
        case 2: r -= b; break;
        case 3: r *= b; break;
        case 4:
            if (b == 0) { cErr = true; cCur = "Error"; return; }
            r /= b; break;
        default: return;
    }
    cAcc = r;
    char buf[48];
    snprintf(buf, sizeof buf, "%.10g", r);
    cCur = buf;
    if (cCur.size() > 14) { snprintf(buf, sizeof buf, "%.5e", r); cCur = buf; }
}
static void calcPress(const std::string& b) {
    if (cErr) { cCur = "0"; cAcc = 0; cOp = 0; cFresh = true; cErr = false; }
    if (b.size() == 1 && b[0] >= '0' && b[0] <= '9') {
        if (cFresh || cCur == "0") { cCur = b; cFresh = false; }
        else if (cCur.size() < 14) cCur += b;
    } else if (b == ".") {
        if (cFresh) { cCur = "0."; cFresh = false; }
        else if (cCur.find('.') == std::string::npos) cCur += ".";
    } else if (b == "C") {
        cCur = "0"; cAcc = 0; cOp = 0; cFresh = true;
    } else if (b == "DEL") {
        if (!cFresh && cCur.size() > 1) cCur.pop_back(); else { cCur = "0"; cFresh = true; }
        if (cCur == "-") cCur = "0";
    } else if (b == "=") {
        if (cOp) { calcDo(); cOp = 0; }
        cFresh = true;
    } else {
        int n = b == "+" ? 1 : (b == "-" ? 2 : (b == "\xC3\x97" ? 3 : 4));
        if (cOp && !cFresh) calcDo(); else cAcc = atof(cCur.c_str());
        if (!cErr) { cOp = n; cFresh = true; }
    }
}

// ---------------------------------------------------------------- store
static float storeProg[A_COUNT];

// ---------------------------------------------------------------- store / browser state
static int storeView = 0, storeSel = 0, storeCat = -1, storeScroll = 0;
static Uint32 storeStart = 0;
static std::vector<std::string> gHist;
static std::string brMsg;
static Uint32 brMsgT = 0;
static void loadHist() {
    gHist.clear();
    std::string d;
    if (!readFile(DIR_ + "history.txt", d)) return;
    std::string line;
    for (char c : d) { if (c == '\n') { if (!line.empty()) gHist.push_back(line); line.clear(); } else line += c; }
    if (!line.empty()) gHist.push_back(line);
}
static void saveHist() {
    std::string d;
    for (auto& h : gHist) d += h + "\n";
    writeFile(DIR_ + "history.txt", d);
}


// ---------------------------------------------------------------- Paint (state + pixel engine)
static const int PCX = 20, PCY = 170, PCW = 1240, PCH = 488; // canvas rectangle on screen
static std::vector<Uint32> pPix, pBase;
static std::vector<std::vector<Uint32>> pUndo;
static SDL_Texture* pTex = nullptr;
static bool pTexDirty = true, pDirty = false, pDown = false;
static int pTool = 0, pSizeIdx = 2, pCol = 0, pSX = 0, pSY = 0, pLX = 0, pLY = 0;
static const int PSIZES[6] = {2, 4, 8, 14, 22, 32};
static std::string pMsg;
static Uint32 pMsgT = 0;
static const Col PAL[16] = {{0, 0, 0}, {90, 90, 90}, {170, 170, 170}, {255, 255, 255}, {200, 30, 30}, {240, 120, 20}, {250, 210, 30}, {130, 200, 40},
                            {20, 150, 60}, {20, 180, 190}, {40, 110, 220}, {40, 50, 150}, {130, 60, 190}, {230, 80, 170}, {130, 80, 40}, {250, 190, 150}};
static Uint32 packCol(Col c) { return 0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8) | c.b; }
static void pSet(int x, int y, Uint32 v) { if (x >= 0 && y >= 0 && x < PCW && y < PCH) pPix[(size_t)y * PCW + x] = v; }
static void pDisc(int cx, int cy, int r, Uint32 v) {
    for (int dy = -r; dy <= r; dy++) {
        int w = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = cx - w; x <= cx + w; x++) pSet(x, cy + dy, v);
    }
}
static void pLine(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    int n = std::max(abs(x1 - x0), abs(y1 - y0));
    if (n == 0) { pDisc(x0, y0, r, v); return; }
    int step = r > 6 ? std::max(1, r / 4) : 1;
    for (int i = 0; i <= n; i += step) pDisc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, r, v);
    pDisc(x1, y1, r, v);
}
static void pRectO(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    pLine(x0, y0, x1, y0, r, v); pLine(x1, y0, x1, y1, r, v); pLine(x1, y1, x0, y1, r, v); pLine(x0, y1, x0, y0, r, v);
}
static void pOval(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f, rx = fabsf((x1 - x0) / 2.0f), ry = fabsf((y1 - y0) / 2.0f);
    int px = (int)(cx + rx), py = (int)cy;
    for (int i = 1; i <= 96; i++) {
        float a = i * 6.2831853f / 96.0f;
        int nx = (int)(cx + cosf(a) * rx), ny = (int)(cy + sinf(a) * ry);
        pLine(px, py, nx, ny, r, v); px = nx; py = ny;
    }
}
static void pFill(int x, int y, Uint32 nv) {
    if (x < 0 || y < 0 || x >= PCW || y >= PCH) return;
    Uint32 old = pPix[(size_t)y * PCW + x];
    if (old == nv) return;
    std::vector<int> st; st.push_back(y * PCW + x);
    while (!st.empty()) {
        int i = st.back(); st.pop_back();
        int px = i % PCW, py = i / PCW;
        if (pPix[i] != old) continue;
        int l = px, r = px;
        while (l > 0 && pPix[(size_t)py * PCW + l - 1] == old) l--;
        while (r < PCW - 1 && pPix[(size_t)py * PCW + r + 1] == old) r++;
        for (int k = l; k <= r; k++) pPix[(size_t)py * PCW + k] = nv;
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = py + dy;
            if (ny < 0 || ny >= PCH) continue;
            bool run = false;
            for (int k = l; k <= r; k++) {
                bool m = pPix[(size_t)ny * PCW + k] == old;
                if (m && !run) st.push_back(ny * PCW + k);
                run = m;
            }
        }
    }
}
static void paintEnsure() { if (pPix.empty()) pPix.assign((size_t)PCW * PCH, 0xFFFFFFFFu); }
static void paintPush() {
    paintEnsure();
    pUndo.push_back(pPix);
    if (pUndo.size() > 8) pUndo.erase(pUndo.begin());
}
static void paintSaveFile(const std::string& path) {
    if (pPix.empty()) return;
    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(pPix.data(), PCW, PCH, 32, PCW * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (sf) { SDL_SaveBMP(sf, path.c_str()); SDL_FreeSurface(sf); }
}
static void paintLoad() {
    pPix.assign((size_t)PCW * PCH, 0xFFFFFFFFu);
    pUndo.clear(); pTexDirty = true; pDirty = false; pDown = false;
    SDL_Surface* sf = IMG_Load((DIR_ + "paint.bmp").c_str());
    if (!sf) return;
    SDL_Surface* c = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ARGB8888, 0);
    if (c) {
        if (c->w == PCW && c->h == PCH) for (int y = 0; y < PCH; y++) memcpy(&pPix[(size_t)y * PCW], (Uint8*)c->pixels + (size_t)y * c->pitch, PCW * 4);
        SDL_FreeSurface(c);
    }
    SDL_FreeSurface(sf);
}
static void paintFree() {
    if (pTex) { SDL_DestroyTexture(pTex); pTex = nullptr; }
    pPix.clear(); pBase.clear(); pUndo.clear();
}
static void paintExport() {
    mkdir((DIR_ + "photos").c_str(), 0777);
    char nm[64]; snprintf(nm, sizeof nm, "paint_%04d%02d%02d_%02d%02d%02d.bmp", gNow.y, gNow.mo, gNow.d, gNow.h, gNow.mi, gNow.s);
    paintSaveFile(DIR_ + "photos/" + nm);
    pMsg = std::string("Saved: switch/WinNX/photos/") + nm; pMsgT = SDL_GetTicks();
}

// ---------------------------------------------------------------- Word (document model)
struct WPara {
    std::string t;
    int style = 0;          // 0 normal, 1 heading 1, 2 heading 2, 3 divider line, 4 picture
    bool b = false, i = false, u = false;
    int align = 0;          // 0 left, 1 centre, 2 right
    int list = 0;           // 0 none, 1 bullets, 2 numbers
    std::string img;        // picture path (style 4)
    std::vector<std::string> lines; std::string cs; int cf = -1, cw = -1; // layout cache
};
static std::vector<WPara> wDoc;
static int wCur = 0, wScroll = 0, wTab = 0, wView = 0, wPicScroll = 0;
static bool wDirty = false, wFollow = false;
static std::vector<std::string> wPics;
struct WImg { SDL_Texture* t; int w, h; };
static std::map<std::string, WImg> wImgs;
static void wEnsure() { if (wDoc.empty()) { wDoc.push_back(WPara()); wCur = 0; } wCur = std::max(0, std::min(wCur, (int)wDoc.size() - 1)); }
static void wFreeImgs() { for (auto& p : wImgs) if (p.second.t) SDL_DestroyTexture(p.second.t); wImgs.clear(); }
static WImg* wImage(const std::string& path) {
    auto it = wImgs.find(path);
    if (it != wImgs.end()) return it->second.t ? &it->second : nullptr;
    if (wImgs.size() >= 8) wFreeImgs();
    WImg wi = {nullptr, 0, 0};
    SDL_Surface* sf = IMG_Load(path.c_str());
    if (sf) { wi.w = sf->w; wi.h = sf->h; wi.t = SDL_CreateTextureFromSurface(gR, sf); SDL_FreeSurface(sf); }
    WImg& slot = (wImgs[path] = wi);
    return slot.t ? &slot : nullptr;
}
static void wSave() {
    std::string d;
    for (auto& p : wDoc) {
        char hd[64];
        snprintf(hd, sizeof hd, "P|%d|%d|%d|%d|", p.style, (p.b ? 1 : 0) | (p.i ? 2 : 0) | (p.u ? 4 : 0), p.align, p.list);
        d += hd; d += p.img; d += "|"; d += p.t; d += "\n";
    }
    writeFile(DIR_ + "document1.wdoc", d);
}
static void wLoad() {
    wDoc.clear(); wCur = 0; wScroll = 0; wDirty = false; wFollow = true;
    std::string d;
    if (readFile(DIR_ + "document1.wdoc", d)) {
        size_t pos = 0;
        while (pos < d.size()) {
            size_t e = d.find('\n', pos);
            if (e == std::string::npos) e = d.size();
            std::string ln = d.substr(pos, e - pos); pos = e + 1;
            if (ln.size() < 2 || ln[0] != 'P' || ln[1] != '|') continue;
            std::vector<std::string> f; size_t a = 2;
            for (int k = 0; k < 5; k++) { size_t b = ln.find('|', a); if (b == std::string::npos) break; f.push_back(ln.substr(a, b - a)); a = b + 1; }
            if (f.size() < 5) continue;
            WPara p;
            p.style = atoi(f[0].c_str()); int fl = atoi(f[1].c_str());
            p.b = fl & 1; p.i = fl & 2; p.u = fl & 4;
            p.align = atoi(f[2].c_str()); p.list = atoi(f[3].c_str()); p.img = f[4];
            p.t = ln.substr(a);
            wDoc.push_back(p);
        }
    } else if (readFile(DIR_ + "document1.txt", d)) { // v0.2 plain-text document
        std::string cur;
        for (char c : d) { if (c == '\n') { WPara p; p.t = cur; wDoc.push_back(p); cur.clear(); } else cur += c; }
        WPara p; p.t = cur; wDoc.push_back(p);
    }
    wEnsure();
}
static int wWords() {
    int n = 0;
    for (auto& p : wDoc) if (p.style < 3) n += wordCount(p.t);
    return n;
}
static void wInsertAfter(const WPara& np) {
    wEnsure();
    wDoc.insert(wDoc.begin() + wCur + 1, np);
    wCur++; wDirty = true; wFollow = true;
}
static void wType(const std::string& t) {
    wEnsure();
    if (wDoc[wCur].style >= 3) { WPara np; wInsertAfter(np); }
    WPara& p = wDoc[wCur];
    if (t == "\n") {
        if (p.list && p.t.empty()) { p.list = 0; wDirty = true; return; } // Enter on an empty list item ends the list
        WPara np;
        np.align = p.align; np.list = p.list; np.b = p.b; np.i = p.i; np.u = p.u;
        if (p.style == 0) np.style = 0;
        wInsertAfter(np);
        return;
    }
    p.t += t; wDirty = true; wFollow = true;
}
static void wBack() {
    wEnsure();
    WPara& p = wDoc[wCur];
    if (p.style >= 3 || p.t.empty()) {
        if (p.list && p.style < 3) { p.list = 0; wDirty = true; return; }
        if (wDoc.size() > 1) { wDoc.erase(wDoc.begin() + wCur); wCur = std::max(0, wCur - 1); }
        else { p = WPara(); }
        wDirty = true; wFollow = true;
        return;
    }
    while (!p.t.empty() && ((unsigned char)p.t.back() & 0xC0) == 0x80) p.t.pop_back();
    if (!p.t.empty()) p.t.pop_back();
    wDirty = true; wFollow = true;
}
static void wScanPics() {
    wPics.clear();
    scanDir("sdmc:/switch/WinNX/photos", 2, false, wPics);
    scanDir("sdmc:/Pictures", 3, false, wPics);
    scanDir("sdmc:/Nintendo/Album", 6, false, wPics);
    std::sort(wPics.begin(), wPics.end(), [](const std::string& a, const std::string& b) { return a > b; });
    wPicScroll = 0;
}

// ---------------------------------------------------------------- 2048 / Minesweeper / Snake (state + logic)
static int g2[4][4], g2score = 0, g2best = 0;
static bool g2over = false, g2dirty = false;
static int swX = 0, swY = 0;
static void g2Save() {
    std::string d = std::to_string(g2score) + " " + std::to_string(g2best) + " " + (g2over ? "1" : "0");
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) d += " " + std::to_string(g2[r][c]);
    writeFile(DIR_ + "2048.dat", d);
}
static void g2Spawn() {
    std::vector<int> e;
    for (int i = 0; i < 16; i++) if (!g2[i / 4][i % 4]) e.push_back(i);
    if (e.empty()) return;
    int k = e[rand() % e.size()];
    g2[k / 4][k % 4] = (rand() % 10 == 0) ? 4 : 2;
}
static bool g2CanMove() {
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        if (!g2[r][c]) return true;
        if (r < 3 && g2[r][c] == g2[r + 1][c]) return true;
        if (c < 3 && g2[r][c] == g2[r][c + 1]) return true;
    }
    return false;
}
static void g2New() {
    memset(g2, 0, sizeof g2); g2score = 0; g2over = false;
    g2Spawn(); g2Spawn(); g2dirty = true;
}
static void g2Load() {
    memset(g2, 0, sizeof g2); g2score = 0; g2over = false;
    std::string d; bool ok = false;
    if (readFile(DIR_ + "2048.dat", d)) {
        int v[19]; int n = 0; const char* p = d.c_str(); char* e;
        while (n < 19) { long x = strtol(p, &e, 10); if (e == p) break; v[n++] = (int)x; p = e; }
        if (n == 19) { g2score = v[0]; g2best = v[1]; g2over = v[2] != 0; for (int i = 0; i < 16; i++) g2[i / 4][i % 4] = v[3 + i]; ok = true; }
    }
    if (!ok) g2New();
    g2dirty = false;
}
static void g2Move(int dir) { // 0 left 1 right 2 up 3 down
    if (g2over) return;
    bool moved = false;
    for (int k = 0; k < 4; k++) {
        int rr[4], cc[4];
        for (int i = 0; i < 4; i++) {
            if (dir == 0) { rr[i] = k; cc[i] = i; }
            else if (dir == 1) { rr[i] = k; cc[i] = 3 - i; }
            else if (dir == 2) { rr[i] = i; cc[i] = k; }
            else { rr[i] = 3 - i; cc[i] = k; }
        }
        std::vector<int> v;
        for (int i = 0; i < 4; i++) if (g2[rr[i]][cc[i]]) v.push_back(g2[rr[i]][cc[i]]);
        std::vector<int> o;
        for (size_t i = 0; i < v.size(); i++) {
            if (i + 1 < v.size() && v[i] == v[i + 1]) { o.push_back(v[i] * 2); g2score += v[i] * 2; i++; }
            else o.push_back(v[i]);
        }
        while (o.size() < 4) o.push_back(0);
        for (int i = 0; i < 4; i++) { if (g2[rr[i]][cc[i]] != o[i]) moved = true; g2[rr[i]][cc[i]] = o[i]; }
    }
    if (!moved) return;
    g2Spawn();
    if (g2score > g2best) g2best = g2score;
    if (!g2CanMove()) g2over = true;
    g2dirty = true;
}

// Minesweeper
static const int MCOLS = 16, MROWS = 9, MMINES = 22, MCELL = 60, MX0 = 160, MY0 = 104;
struct MCell { bool mine = false, open = false, flag = false; int n = 0; };
static MCell mc[MROWS][MCOLS];
static int mState = 0; // 0 ready, 1 playing, 2 won, 3 lost
static bool mFlagMode = false;
static float mTime = 0;
static void mNew() {
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) mc[r][c] = MCell();
    mState = 0; mTime = 0;
}
static int mFlags() { int n = 0; for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) if (mc[r][c].flag) n++; return n; }
static void mPlace(int sr, int sc) {
    int placed = 0;
    while (placed < MMINES) {
        int r = rand() % MROWS, c = rand() % MCOLS;
        if (mc[r][c].mine || (abs(r - sr) <= 1 && abs(c - sc) <= 1)) continue;
        mc[r][c].mine = true; placed++;
    }
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) {
        int n = 0;
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
            int a = r + dr, b = c + dc;
            if (a >= 0 && b >= 0 && a < MROWS && b < MCOLS && mc[a][b].mine) n++;
        }
        mc[r][c].n = n;
    }
}
static void mCheckWin() {
    int closed = 0;
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) if (!mc[r][c].open) closed++;
    if (closed == MMINES) mState = 2;
}
static void mDig(int r, int c) {
    if (mState >= 2 || mc[r][c].flag || mc[r][c].open) return;
    if (mState == 0) { mPlace(r, c); mState = 1; }
    if (mc[r][c].mine) {
        mState = 3;
        for (int a = 0; a < MROWS; a++) for (int b = 0; b < MCOLS; b++) if (mc[a][b].mine) mc[a][b].open = true;
        return;
    }
    std::vector<int> st; st.push_back(r * MCOLS + c);
    while (!st.empty()) {
        int i = st.back(); st.pop_back();
        int rr = i / MCOLS, cc = i % MCOLS;
        if (mc[rr][cc].open || mc[rr][cc].flag) continue;
        mc[rr][cc].open = true;
        if (mc[rr][cc].n == 0)
            for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
                int a = rr + dr, b = cc + dc;
                if (a >= 0 && b >= 0 && a < MROWS && b < MCOLS && !mc[a][b].open) st.push_back(a * MCOLS + b);
            }
    }
    mCheckWin();
}

// Snake
static const int SCOLS = 30, SROWS = 16, SCELL = 36, SX0 = 100, SY0 = 76;
static std::vector<std::pair<int, int>> snake;
static int sDir = 0, sNext = 0, sState = 0, sScore = 0, sBest = 0, sFx = 0, sFy = 0; // dir 0 L 1 R 2 U 3 D
static float sAcc = 0;
static void snakeFood() {
    for (int tries = 0; tries < 500; tries++) {
        int x = rand() % SCOLS, y = rand() % SROWS; bool ok = true;
        for (auto& b : snake) if (b.first == x && b.second == y) ok = false;
        if (ok) { sFx = x; sFy = y; return; }
    }
}
static void snakeNew() {
    snake.clear();
    for (int i = 0; i < 4; i++) snake.push_back({10 - i, 8});
    sDir = sNext = 1; sState = 0; sScore = 0; sAcc = 0;
    snakeFood();
}
static void snakeLoad() { std::string d; if (readFile(DIR_ + "snake.dat", d)) sBest = atoi(d.c_str()); snakeNew(); }
static void snakeTurn(int d) {
    if ((d == 0 && sDir == 1) || (d == 1 && sDir == 0) || (d == 2 && sDir == 3) || (d == 3 && sDir == 2)) return;
    sNext = d;
    if (sState == 0) sState = 1;
}
static void snakeStep() {
    sDir = sNext;
    int hx = snake[0].first + (sDir == 0 ? -1 : sDir == 1 ? 1 : 0), hy = snake[0].second + (sDir == 2 ? -1 : sDir == 3 ? 1 : 0);
    bool dead = hx < 0 || hy < 0 || hx >= SCOLS || hy >= SROWS;
    for (size_t i = 0; i + 1 < snake.size() && !dead; i++) if (snake[i].first == hx && snake[i].second == hy) dead = true;
    if (dead) {
        sState = 2;
        if (sScore > sBest) { sBest = sScore; writeFile(DIR_ + "snake.dat", std::to_string(sBest)); }
        return;
    }
    snake.insert(snake.begin(), {hx, hy});
    if (hx == sFx && hy == sFy) { sScore++; snakeFood(); }
    else snake.pop_back();
}

// ---------------------------------------------------------------- Solitaire (Klondike): state + rules
struct SCard { int r, s; bool up; };   // rank 1..13, suit 0 spades 1 clubs 2 hearts 3 diamonds
struct SolState { std::vector<SCard> stock, waste, found[4], tab[7]; int score, moves; };
static SolState sol;
static std::vector<SolState> solUndo;
static int solDraw = 1;
static bool solWon = false, solDirty = false, solDragging = false, solPressed = false;
static Uint32 solStartT = 0;
static const int SOLCW = 118, SOLCH = 160, SOLX0 = 137, SOLDX = 148, SOLY0 = 52, SOLTY = 236;
static inline bool solRed(const SCard& c) { return c.s >= 2; }
static bool solCanFound(const SCard& c) { return (int)sol.found[c.s].size() + 1 == c.r; }
static bool solCanTab(const SCard& c, int col) {
    const std::vector<SCard>& t = sol.tab[col];
    if (t.empty()) return c.r == 13;
    const SCard& b = t.back();
    return b.up && b.r == c.r + 1 && solRed(b) != solRed(c);
}
static void solPush() { solUndo.push_back(sol); if (solUndo.size() > 60) solUndo.erase(solUndo.begin()); }
static void solCheckWin() {
    int n = 0; for (int i = 0; i < 4; i++) n += (int)sol.found[i].size();
    solWon = (n == 52);
}
static void solNew() {
    std::vector<SCard> d;
    for (int s2 = 0; s2 < 4; s2++) for (int r = 1; r <= 13; r++) d.push_back(SCard{r, s2, false});
    for (int i = 51; i > 0; i--) std::swap(d[i], d[rand() % (i + 1)]);
    sol = SolState(); sol.score = 0; sol.moves = 0;
    int k = 0;
    for (int c = 0; c < 7; c++) for (int i = 0; i <= c; i++) { SCard x = d[k++]; x.up = (i == c); sol.tab[c].push_back(x); }
    while (k < 52) sol.stock.push_back(d[k++]);
    solUndo.clear(); solWon = false; solDirty = true; solStartT = SDL_GetTicks();
}
static std::string solSer() {
    std::string d;
    auto put = [&](const std::vector<SCard>& v) { for (auto& c : v) { d += std::to_string(c.r * 8 + c.s * 2 + (c.up ? 1 : 0)); d += ','; } d += '|'; };
    put(sol.stock); put(sol.waste);
    for (int i = 0; i < 4; i++) put(sol.found[i]);
    for (int i = 0; i < 7; i++) put(sol.tab[i]);
    d += std::to_string(sol.score) + "," + std::to_string(sol.moves) + "," + std::to_string(solDraw);
    return d;
}
static void solLoad() {
    std::string d;
    bool ok = false;
    if (readFile(DIR_ + "solitaire.dat", d)) {
        std::vector<std::string> parts; std::string cur;
        for (char c : d) { if (c == '|') { parts.push_back(cur); cur.clear(); } else cur += c; }
        parts.push_back(cur);
        if (parts.size() == 14) {
            SolState t; t.score = t.moves = 0;
            auto get = [&](const std::string& str, std::vector<SCard>& v) {
                size_t a = 0;
                while (a < str.size()) {
                    size_t b = str.find(',', a); if (b == std::string::npos) break;
                    int x = atoi(str.substr(a, b - a).c_str());
                    v.push_back(SCard{x / 8, (x / 2) % 4, (x & 1) != 0}); a = b + 1;
                }
            };
            get(parts[0], t.stock); get(parts[1], t.waste);
            for (int i = 0; i < 4; i++) get(parts[2 + i], t.found[i]);
            for (int i = 0; i < 7; i++) get(parts[6 + i], t.tab[i]);
            int n = (int)(t.stock.size() + t.waste.size());
            for (int i = 0; i < 4; i++) n += (int)t.found[i].size();
            for (int i = 0; i < 7; i++) n += (int)t.tab[i].size();
            int a1 = 0, a2 = 0, a3 = 1;
            sscanf(parts[13].c_str(), "%d,%d,%d", &a1, &a2, &a3);
            if (n == 52) { sol = t; sol.score = a1; sol.moves = a2; solDraw = (a3 == 3) ? 3 : 1; ok = true; solUndo.clear(); solCheckWin(); }
        }
    }
    if (!ok) solNew();
    solDirty = false;
}
static void solFlipTop(int col) {
    std::vector<SCard>& t = sol.tab[col];
    if (!t.empty() && !t.back().up) { t.back().up = true; sol.score += 5; }
}
static void solStockTap() {
    solPush();
    if (!sol.stock.empty()) {
        for (int i = 0; i < solDraw && !sol.stock.empty(); i++) { SCard c = sol.stock.back(); sol.stock.pop_back(); c.up = true; sol.waste.push_back(c); }
    } else if (!sol.waste.empty()) {
        while (!sol.waste.empty()) { SCard c = sol.waste.back(); sol.waste.pop_back(); c.up = false; sol.stock.push_back(c); }
        sol.score = std::max(0, sol.score - (solDraw == 1 ? 100 : 20));
    } else { solUndo.pop_back(); return; }
    sol.moves++; solDirty = true;
}
// src: 0 waste, 1 tableau (col, idx = first card of the run), 2 foundation slot (col)
static bool solMove(int src, int col, int idx, int dstType, int dstCol) {
    std::vector<SCard> run;
    if (src == 0) { if (sol.waste.empty()) return false; run.push_back(sol.waste.back()); }
    else if (src == 1) { if (idx < 0 || idx >= (int)sol.tab[col].size()) return false; run.assign(sol.tab[col].begin() + idx, sol.tab[col].end()); }
    else { if (sol.found[col].empty()) return false; run.push_back(sol.found[col].back()); }
    if (run.empty() || !run[0].up) return false;
    bool ok = false;
    if (dstType == 2) ok = (run.size() == 1) && solCanFound(run[0]) && !(src == 2);
    else if (dstType == 1) ok = solCanTab(run[0], dstCol) && !(src == 1 && col == dstCol);
    if (!ok) return false;
    solPush();
    if (src == 0) sol.waste.pop_back();
    else if (src == 1) { sol.tab[col].erase(sol.tab[col].begin() + idx, sol.tab[col].end()); solFlipTop(col); }
    else sol.found[col].pop_back();
    if (dstType == 2) { sol.found[run[0].s].push_back(run[0]); sol.score += (src == 2 ? 0 : 10); }
    else { for (auto& c : run) sol.tab[dstCol].push_back(c); if (src == 0) sol.score += 5; if (src == 2) sol.score = std::max(0, sol.score - 15); }
    sol.moves++; solDirty = true; solCheckWin();
    return true;
}
static bool solAuto(int src, int col, int idx) { // smart tap: foundation first, then any tableau column
    SCard c;
    if (src == 0) { if (sol.waste.empty()) return false; c = sol.waste.back(); }
    else if (src == 1) { if (idx < 0 || idx >= (int)sol.tab[col].size()) return false; c = sol.tab[col][idx]; }
    else return false;
    if (!c.up) return false;
    bool isTop = (src == 0) || (idx == (int)sol.tab[col].size() - 1);
    if (isTop && solCanFound(c) && solMove(src, col, idx, 2, c.s)) return true;
    for (int d = 0; d < 7; d++) {
        if (src == 1 && d == col) continue;
        if (solCanTab(c, d)) {
            // don't shuffle a king that is already at the bottom of its column
            if (src == 1 && c.r == 13 && idx == 0) continue;
            if (solMove(src, col, idx, 1, d)) return true;
        }
    }
    return false;
}
static void solUndoMove() { if (solUndo.empty()) return; sol = solUndo.back(); solUndo.pop_back(); solWon = false; solDirty = true; }

// ---------------------------------------------------------------- Photos gallery (state)
struct GPic { std::string path; long long mt; };
static std::vector<GPic> gAllPics;
static int phView = 0, phChip = 0, phScroll = 0, phBudget = 0;
static bool phOverlay = true;
static std::map<int, SDL_Texture*> phThumb;
static void phFreeThumbs() { for (auto& t : phThumb) if (t.second) SDL_DestroyTexture(t.second); phThumb.clear(); }
static bool picSkipDir(const std::string& path, const std::string& name) {
    std::string n = lower(name);
    if (n == "atmosphere" || n == "bootloader" || n == "emummc" || n == "contents" || n == "save" || n == "saveimages" || n == "sys" ||
        n == "retroarch" || n == "themes" || n == "romfs" || n == "exefs" || n == "config" || n == "cache" || n == "sd:" || n[0] == '.') return true;
    if (path == "sdmc:/switch") return true;           // homebrew folders are full of tiny icons
    if (path.find("/Nintendo/") != std::string::npos && n != "album") return true;
    return false;
}
static void scanPics(const std::string& dir, int depth, std::vector<GPic>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (out.size() > 6000) break;
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = (dir == "sdmc:/" ? dir : dir + "/") + n;
        struct stat st;
        if (stat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth > 0 && !picSkipDir(p, n)) scanPics(p, depth - 1, out);
        } else if (isImg(n) && st.st_size > 6000) out.push_back(GPic{p, (long long)st.st_mtime});
    }
    closedir(d);
}
static void photoFilter() {
    gPhotos.clear();
    for (auto& g : gAllPics) {
        bool shot = g.path.find("Nintendo/Album") != std::string::npos;
        bool pics = g.path.find("/Pictures") != std::string::npos || g.path.find("switch/WinNX/photos") != std::string::npos;
        if (phChip == 1 && !shot) continue;
        if (phChip == 2 && !pics) continue;
        if (phChip == 3 && (shot || pics)) continue;
        if (phChip == 5 && g.path.substr(0, g.path.find_last_of('/')) != phAlbum) continue;
        gPhotos.push_back(g.path);
    }
    phFreeThumbs(); phScroll = 0;
}
static void photoScanAll() {
    gAllPics.clear();
    scanPics("sdmc:/", 8, gAllPics);
    std::sort(gAllPics.begin(), gAllPics.end(), [](const GPic& a, const GPic& b) { return a.mt > b.mt; });
    photoFilter();
}

// ================================================================ Scientific calculator
static bool gSciErr = false;
static std::string gSciExpr; static double gSciAns = 0; static bool gSciDone = false;
struct SciP { const char* s; size_t i; bool err; };
static double sciExpr(SciP& p);
static bool sciExt(SciP& p, double& out);   // extra functions, constants and the graphing variable x (w82_calc.h)
static void sciWs(SciP& p) { while (p.s[p.i] == ' ') p.i++; }
static double sciFact(double x) { if (x < 0 || x > 170 || x != floor(x)) return NAN; double r = 1; for (int i = 2; i <= (int)x; i++) r *= i; return r; }
static double sciPrimary(SciP& p) {
    sciWs(p);
    char c = p.s[p.i];
    if (c == '(') { p.i++; double v = sciExpr(p); sciWs(p); if (p.s[p.i] == ')') p.i++; return v; }
    if ((c >= '0' && c <= '9') || c == '.') { char* e; double v = strtod(p.s + p.i, &e); p.i = e - p.s; return v; }
    if (strncmp(p.s + p.i, "pi", 2) == 0) { p.i += 2; return M_PI; }
    if (strncmp(p.s + p.i, "Ans", 3) == 0) { p.i += 3; return gSciAns; }
    { double xv; if (sciExt(p, xv)) return xv; }
    if (c == 'e' && p.s[p.i + 1] != 'x') { p.i++; return M_E; }
    static const char* FN[] = {"asin", "acos", "atan", "sin", "cos", "tan", "ln", "log", "sqrt", "abs", "cbrt", nullptr};
    for (int k = 0; FN[k]; k++) {
        size_t n = strlen(FN[k]);
        if (strncmp(p.s + p.i, FN[k], n) == 0 && p.s[p.i + n] == '(') {
            p.i += n + 1; double v = sciExpr(p); sciWs(p); if (p.s[p.i] == ')') p.i++;
            double d = gSciDeg ? M_PI / 180.0 : 1.0;
            switch (k) {
                case 0: return asin(v) / d; case 1: return acos(v) / d; case 2: return atan(v) / d;
                case 3: { if (gSciDeg && fmod(v, 180.0) == 0) return 0; return sin(v * d); }
                case 4: { if (gSciDeg && fmod(v - 90.0, 180.0) == 0) return 0; return cos(v * d); }
                case 5: return tan(v * d);
                case 6: return log(v); case 7: return log10(v); case 8: return sqrt(v); case 9: return fabs(v); default: return cbrt(v);
            }
        }
    }
    p.err = true; return 0;
}
static double sciPost(SciP& p) {
    double v = sciPrimary(p);
    for (;;) {
        sciWs(p);
        if (p.s[p.i] == '!') { p.i++; v = sciFact(v); }
        else if (p.s[p.i] == '%') { p.i++; v /= 100.0; }
        else break;
    }
    return v;
}
static double sciUnary(SciP& p);
static double sciPow(SciP& p) {
    double b = sciPost(p);
    sciWs(p);
    if (p.s[p.i] == '^') { p.i++; double e = sciUnary(p); return pow(b, e); }
    return b;
}
static double sciUnary(SciP& p) {
    sciWs(p);
    if (p.s[p.i] == '-') { p.i++; return -sciUnary(p); }
    if (p.s[p.i] == '+') { p.i++; return sciUnary(p); }
    return sciPow(p);
}
static double sciTerm(SciP& p) {
    double v = sciUnary(p);
    for (;;) {
        sciWs(p);
        char c = p.s[p.i];
        if (c == '*') { p.i++; v *= sciUnary(p); }
        else if (c == '/') { p.i++; v /= sciUnary(p); }
        else if (strncmp(p.s + p.i, "mod", 3) == 0) { p.i += 3; v = fmod(v, sciUnary(p)); }
        else if (c == '(' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.') v *= sciPow(p); // 2pi, 3(4)
        else break;
    }
    return v;
}
static double sciExpr(SciP& p) {
    double v = sciTerm(p);
    for (;;) {
        sciWs(p);
        if (p.s[p.i] == '+') { p.i++; v += sciTerm(p); }
        else if (p.s[p.i] == '-') { p.i++; v -= sciTerm(p); }
        else break;
    }
    return v;
}
static bool sciEval(const std::string& e, double& out) {
    if (e.empty()) return false;
    SciP p{e.c_str(), 0, false};
    double v = sciExpr(p);
    sciWs(p);
    if (p.err || p.s[p.i] != 0 || std::isnan(v) || std::isinf(v)) return false;
    out = v; return true;
}
static std::string sciFmt(double v) {
    char b[48]; snprintf(b, sizeof b, "%.12g", v);
    if (strlen(b) > 16) snprintf(b, sizeof b, "%.8e", v);
    return b;
}
static std::string sciShow(const std::string& e) {
    std::string o;
    for (size_t i = 0; i < e.size(); i++) {
        if (e.compare(i, 1, "*") == 0) o += "\xC3\x97";
        else if (e[i] == '/') o += "\xC3\xB7";
        else o += e[i];
    }
    return o;
}
struct SciB { const char* l; const char* ins; int c, r; };
static const SciB SCIB[] = {
    {"sin", "sin(", 0, 0}, {"cos", "cos(", 1, 0}, {"tan", "tan(", 2, 0}, {"pi", "pi", 3, 0}, {"C", "C", 4, 0}, {"DEL", "DEL", 5, 0}, {"(", "(", 6, 0}, {")", ")", 7, 0},
    {"asin", "asin(", 0, 1}, {"acos", "acos(", 1, 1}, {"atan", "atan(", 2, 1}, {"e", "e", 3, 1}, {"7", "7", 4, 1}, {"8", "8", 5, 1}, {"9", "9", 6, 1}, {"\xC3\xB7", "/", 7, 1},
    {"ln", "ln(", 0, 2}, {"log", "log(", 1, 2}, {"sqrt", "sqrt(", 2, 2}, {"x\xC2\xB2", "^2", 3, 2}, {"4", "4", 4, 2}, {"5", "5", 5, 2}, {"6", "6", 6, 2}, {"\xC3\x97", "*", 7, 2},
    {"x^y", "^", 0, 3}, {"n!", "!", 1, 3}, {"1/x", "^(-1)", 2, 3}, {"|x|", "abs(", 3, 3}, {"1", "1", 4, 3}, {"2", "2", 5, 3}, {"3", "3", 6, 3}, {"-", "-", 7, 3},
    {"DEG", "DEG", 0, 4}, {"10^x", "10^(", 1, 4}, {"Ans", "Ans", 2, 4}, {"%", "%", 3, 4}, {"0", "0", 4, 4}, {".", ".", 5, 4}, {"=", "=", 6, 4}, {"+", "+", 7, 4},
};
static const int NSCIB = (int)(sizeof(SCIB) / sizeof(SCIB[0]));
static SDL_Rect sciRect(const SciB& b) { return SDL_Rect{56 + b.c * 146, 186 + b.r * 94, 140, 88}; }
static void sciPress(const std::string& ins) {
    if (ins == "C") { gSciExpr.clear(); gSciErr = false; gSciDone = false; return; }
    if (ins == "DEL") { if (!gSciExpr.empty()) { // remove a whole token (function name) or one char
            static const char* TK[] = {"asin(", "acos(", "atan(", "sqrt(", "sin(", "cos(", "tan(", "abs(", "log(", "ln(", "Ans", "pi", "^(-1)", nullptr};
            bool done = false;
            for (int i = 0; TK[i] && !done; i++) { size_t n = strlen(TK[i]); if (gSciExpr.size() >= n && gSciExpr.compare(gSciExpr.size() - n, n, TK[i]) == 0) { gSciExpr.erase(gSciExpr.size() - n); done = true; } }
            if (!done) gSciExpr.pop_back();
        } gSciErr = false; gSciDone = false; return; }
    if (ins == "DEG") { gSciDeg = !gSciDeg; return; }
    if (ins == "=") {
        double v;
        if (sciEval(gSciExpr, v)) { gSciAns = v; gSciExpr = sciFmt(v); gSciDone = true; gSciErr = false; }
        else gSciErr = true;
        return;
    }
    bool op = ins == "/" || ins == "*" || ins == "^" || ins == "!" || ins == "%" || ins == "^2" || ins == "^(-1)" || ins == "+" || ins == "-";
    if (gSciDone && !op) gSciExpr.clear();   // typing after a result starts fresh; operators continue from it
    gSciDone = false; gSciErr = false;
    if (gSciExpr.size() < 120) gSciExpr += ins;
}
static void drawSci(const Theme& th) {
    rect(0, 0, W, UIH, Col{31, 31, 31});
    rect(56, 70, 1168, 106, Col{42, 42, 42});
    std::string ex = sciShow(gSciExpr);
    while (ex.size() > 3 && tw(ex, F30) > 1120) ex.erase(0, 1);
    text(ex.empty() ? "0" : ex, F30, 1208, 78, WHITE, 2, ex.empty() ? 140 : 255);
    double v; std::string pre;
    if (gSciErr) pre = "Error";
    else if (!gSciDone && sciEval(gSciExpr, v)) pre = "= " + sciFmt(v);
    text(pre, F44, 1208, 120, gSciErr ? Col{255, 120, 120} : WHITE, 2, 200);
    for (int i = 0; i < NSCIB; i++) {
        SDL_Rect r = sciRect(SCIB[i]);
        std::string l = SCIB[i].l;
        Col c = Col{58, 58, 58};
        if (l == "=") c = th.accent;
        else if (l == "C") c = Col{170, 50, 50};
        else if (SCIB[i].c < 4) c = Col{44, 52, 72};
        else if (!(l[0] >= '0' && l[0] <= '9') && l != ".") c = Col{86, 86, 86};
        rect(r.x, r.y, r.w, r.h, c);
        if (l == "DEG") l = gSciDeg ? "DEG" : "RAD";
        text(l, l.size() > 3 ? F30 : F44, r.x + r.w / 2, r.y + r.h / 2 - (l.size() > 3 ? 18 : 28), WHITE, 1);
    }
}
static bool sciTap(int x, int y) {
    for (int i = 0; i < NSCIB; i++) { SDL_Rect r = sciRect(SCIB[i]); if (inR(x, y, r.x, r.y, r.w, r.h)) { sciPress(SCIB[i].ins); return true; } }
    return false;
}


static const int SCI_TX = 56, SCI_TY = 16, SCI_TW = 200, SCI_TH = 40;
// ---------------------------------------------------------------- Calendar (state)
struct CalEvent { int id = 0, y = 2026, m = 1, d = 1, sh = 9, sm = 0, dur = 60; bool allday = false; int rep = 0, rem = -1, col = 0; std::string title, place; };
static std::vector<CalEvent> gCal; static bool gCalLoaded = false; static int gCalNextId = 1;
static int calY = 2026, calM = 1, calSel = 1, calView = 0, calScroll = 0, calAgendaH = 0, calEi = -1;
static bool calEdit = false; static CalEvent calE;
static std::set<std::string> gCalFired;
static std::vector<std::pair<int, int>> calAgendaRows;
static void calLoad(); static void calMonth(int d); static std::string calTileLine(); static void calTick(); static bool calBack();

// ================================================================ Music (real audio via SDL2_mixer): library scan with tags + cover art, queue, shuffle/repeat, seek, background play
struct Song { std::string path, title, artist, album; int track = 0; };
struct Album { std::string name, artist; std::vector<int> songs; SDL_Texture* cover = nullptr; bool tried = false; Col col; };
static std::vector<Song> gSongs;
static std::vector<Album> gAlbumsV;
static std::vector<std::string> gArtistsV;
static std::vector<int> musQueue;
static int musQPos = -1, musIdx = -1;
static bool musPlaying = false, musPaused = false, musFinished = false, musShuffle = false;
static int musRepeat = 0;                          // 0 off, 1 all, 2 one
static float musVol = 0.8f, musPos = 0, musLen = 0;
static std::string musMsg;
static bool musAudioOk = false;
static int musView = 1;                            // 0 now playing, 1 albums, 2 artists, 3 songs
static int musAlb = -1, musArt = -1;               // open album / artist filter
static int musScroll = 0, musScrollMax = 0;
static SDL_Texture* musNowCover = nullptr; static int musNowCoverFor = -2;
static float musLvl[32]; static volatile float musLvlIn[32]; static volatile int musLvlPos = 0;
static volatile int musScanState = 0, musScanCount = 0;   // 0 idle, 1 running, 2 finished (ready to adopt)
static std::vector<Song> musScanOut;
static Thread musScanTh; static bool musScanThOn = false;
static bool musSeeking = false, musVolDrag = false;
#ifdef WINNX_AUDIO
static Mix_Music* musCur = nullptr;
static void musDone() { musFinished = true; }
static void musPost(void*, Uint8* st, int len) {          // taps the final mix: real level meter
    const Sint16* s = (const Sint16*)st; int n = len / 2;
    for (int i = 0; i + 512 <= n; i += 512) {
        float a = 0; for (int k = 0; k < 512; k += 2) a += fabsf((float)s[i + k]);
        musLvlIn[musLvlPos & 31] = std::min(1.0f, a / 256.0f / 9000.0f);
        musLvlPos = (musLvlPos + 1) & 31;
    }
}
#endif
static bool isAudio(const std::string& n) {
    std::string l = lower(n);
    return endsWith(l, ".mp3") || endsWith(l, ".ogg") || endsWith(l, ".flac") || endsWith(l, ".wav") || endsWith(l, ".opus") || endsWith(l, ".mod") || endsWith(l, ".xm") || endsWith(l, ".it") || endsWith(l, ".s3m") || endsWith(l, ".mid");
}
// ---- tag reading (ID3v2 / ID3v1 / Vorbis comments)
static std::string u16to8(const std::string& s, bool be) {
    std::string o;
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        unsigned cp = be ? ((unsigned char)s[i] << 8 | (unsigned char)s[i + 1]) : ((unsigned char)s[i + 1] << 8 | (unsigned char)s[i]);
        if (cp == 0xFEFF) continue;
        if (cp >= 0xD800 && cp < 0xDC00 && i + 3 < s.size()) {
            unsigned lo = be ? ((unsigned char)s[i + 2] << 8 | (unsigned char)s[i + 3]) : ((unsigned char)s[i + 3] << 8 | (unsigned char)s[i + 2]);
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 2;
        }
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 63)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
    }
    return o;
}
static std::string latin1to8(const std::string& s) {
    std::string o;
    for (unsigned char c : s) { if (c == 0) break; if (c < 0x80) o += (char)c; else { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); } }
    return o;
}
static std::string id3Text(const std::string& d) {          // d = frame body (encoding byte + text)
    if (d.empty()) return "";
    int enc = (unsigned char)d[0];
    std::string t = d.substr(1), r;
    if (enc == 0) r = latin1to8(t);
    else if (enc == 3) { size_t z = t.find('\0'); r = z == std::string::npos ? t : t.substr(0, z); }
    else {
        bool be = enc == 2;
        if (t.size() >= 2 && (unsigned char)t[0] == 0xFF && (unsigned char)t[1] == 0xFE) be = false;
        else if (t.size() >= 2 && (unsigned char)t[0] == 0xFE && (unsigned char)t[1] == 0xFF) be = true;
        size_t e = 0; while (e + 1 < t.size() && !(t[e] == 0 && t[e + 1] == 0)) e += 2;
        r = u16to8(t.substr(0, e), be);
    }
    while (!r.empty() && (r.back() == ' ' || r.back() == '\0' || r.back() == '\r' || r.back() == '\n')) r.pop_back();
    return r;
}
static bool readHead(const std::string& p, std::string& out, size_t maxb) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    out.resize(maxb);
    size_t n = fread(&out[0], 1, maxb, f);
    out.resize(n);
    fclose(f);
    return true;
}
static unsigned syncsafe(const unsigned char* p) { return (p[0] & 127) << 21 | (p[1] & 127) << 14 | (p[2] & 127) << 7 | (p[3] & 127); }
// walks the ID3v2 frames; if want=="APIC" returns the picture bytes in `pic`
static bool id3Parse(const std::string& h, Song* sg, std::string* pic) {
    if (h.size() < 10 || h.compare(0, 3, "ID3") != 0) return false;
    int ver = (unsigned char)h[3];
    unsigned tagSz = syncsafe((const unsigned char*)&h[6]);
    size_t end = std::min<size_t>(h.size(), 10 + tagSz), p = 10;
    if (h[5] & 0x40) { unsigned ex = syncsafe((const unsigned char*)&h[10]); if (ver == 3) ex = ((unsigned char)h[10] << 24 | (unsigned char)h[11] << 16 | (unsigned char)h[12] << 8 | (unsigned char)h[13]) + 4; p += ex; }
    int idl = ver == 2 ? 3 : 4, hl = ver == 2 ? 6 : 10;
    while (p + hl <= end) {
        std::string id = h.substr(p, idl);
        if (id[0] == 0) break;
        const unsigned char* q = (const unsigned char*)&h[p + idl];
        unsigned sz = ver == 2 ? (q[0] << 16 | q[1] << 8 | q[2]) : ver == 4 ? syncsafe(q) : (q[0] << 24 | q[1] << 16 | q[2] << 8 | q[3]);
        if (p + hl + sz > h.size() || sz == 0) break;
        std::string body = h.substr(p + hl, sz);
        if (sg) {
            if (id == "TIT2" || id == "TT2") sg->title = id3Text(body);
            else if (id == "TPE1" || id == "TP1") sg->artist = id3Text(body);
            else if (id == "TALB" || id == "TAL") sg->album = id3Text(body);
            else if (id == "TRCK" || id == "TRK") sg->track = atoi(id3Text(body).c_str());
        }
        if (pic && (id == "APIC" || id == "PIC") && pic->empty() && body.size() > 8) {
            int enc = (unsigned char)body[0]; size_t i = 1;
            if (ver == 2) i += 3; else { while (i < body.size() && body[i]) i++; i++; }
            i++;                                                           // picture type
            if (enc == 1 || enc == 2) { while (i + 1 < body.size() && !(body[i] == 0 && body[i + 1] == 0)) i += 2; i += 2; }
            else { while (i < body.size() && body[i]) i++; i++; }
            if (i < body.size()) *pic = body.substr(i);
        }
        p += hl + sz;
    }
    return true;
}
static std::string vorbisField(const std::string& h, const char* key) {
    std::string k = std::string(key) + "=";
    std::string lh = lower(h);
    size_t p = 0;
    while ((p = lh.find(k, p)) != std::string::npos) {
        if (p >= 4) {
            const unsigned char* q = (const unsigned char*)&h[p - 4];
            unsigned len = q[0] | q[1] << 8 | q[2] << 16 | (unsigned)q[3] << 24;
            if (len >= k.size() && len < 400 && p + len <= h.size()) return h.substr(p + k.size(), len - k.size());
        }
        p += k.size();
    }
    return "";
}
static void readTags(Song& sg) {
    std::string h;
    std::string l = lower(sg.path);
    if (!readHead(sg.path, h, endsWith(l, ".mp3") ? 400000 : 70000)) return;
    if (h.compare(0, 3, "ID3") == 0) {
        // the tag may be longer than what we read; id3Parse stops at the buffer end
        id3Parse(h, &sg, nullptr);
    } else if (h.compare(0, 4, "fLaC") == 0 || h.compare(0, 4, "OggS") == 0) {
        sg.title = vorbisField(h, "title"); sg.artist = vorbisField(h, "artist"); sg.album = vorbisField(h, "album");
        sg.track = atoi(vorbisField(h, "tracknumber").c_str());
    } else if (endsWith(l, ".mp3")) {   // ID3v1 at the very end
        FILE* f = fopen(sg.path.c_str(), "rb");
        if (f) {
            char b[128];
            if (fseek(f, -128, SEEK_END) == 0 && fread(b, 1, 128, f) == 128 && memcmp(b, "TAG", 3) == 0) {
                auto fld = [&](int o, int n) { std::string s(b + o, n); size_t z = s.find('\0'); if (z != std::string::npos) s.resize(z); while (!s.empty() && s.back() == ' ') s.pop_back(); return latin1to8(s); };
                sg.title = fld(3, 30); sg.artist = fld(33, 30); sg.album = fld(63, 30);
            }
            fclose(f);
        }
    }
}
static void musScanDir(const std::string& dir, int depth, std::vector<std::string>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (out.size() > 5000) break;
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = (dir == "sdmc:/" ? dir : dir + "/") + n;
        struct stat st;
        if (stat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (depth > 0 && !picSkipDir(p, n)) musScanDir(p, depth - 1, out); }
        else if (isAudio(n) && st.st_size > 20000) out.push_back(p);
    }
    closedir(d);
}
static std::string fileStem(const std::string& f) {
    size_t sl = f.find_last_of('/'); std::string fn = f.substr(sl == std::string::npos ? 0 : sl + 1);
    size_t dot = fn.find_last_of('.');
    return dot == std::string::npos ? fn : fn.substr(0, dot);
}
static std::string folderName(const std::string& f) {
    size_t sl = f.find_last_of('/');
    std::string dir = sl == std::string::npos ? "" : f.substr(0, sl);
    size_t sl2 = dir.find_last_of('/');
    std::string a = sl2 == std::string::npos ? dir : dir.substr(sl2 + 1);
    return (a.empty() || a == "sdmc:") ? "SD card" : a;
}
static std::vector<Song> gMusCache;  // read before the scan thread starts, never touched by it afterwards except for lookups
static void musScanWorker(void*) {
    std::vector<std::string> files;
    musScanDir("sdmc:/", 7, files);
    std::sort(files.begin(), files.end());
    std::vector<Song> out;
    for (auto& f : files) {
        Song sg; bool hit = false;
        for (auto& c : gMusCache) if (c.path == f) { sg = c; hit = true; break; }
        if (!hit) {
            sg.path = f; readTags(sg);
            if (sg.title.empty()) sg.title = fileStem(f);
            if (sg.album.empty()) sg.album = folderName(f);
            if (sg.artist.empty()) sg.artist = "Unknown artist";
        }
        out.push_back(sg);
        musScanCount = (int)out.size();
    }
    musScanOut.swap(out);
    musScanState = 2;
}
static void musBuildLib() {
    gAlbumsV.clear(); gArtistsV.clear();
    std::vector<int> ord(gSongs.size());
    for (size_t i = 0; i < ord.size(); i++) ord[i] = (int)i;
    std::stable_sort(ord.begin(), ord.end(), [](int a, int b) {
        const Song &x = gSongs[a], &y = gSongs[b];
        if (x.album != y.album) return x.album < y.album;
        if (x.track != y.track) return x.track < y.track;
        return x.title < y.title;
    });
    for (int i : ord) {
        const Song& s = gSongs[i];
        int ai = -1;
        for (size_t k = 0; k < gAlbumsV.size(); k++) if (gAlbumsV[k].name == s.album) { ai = (int)k; break; }
        if (ai < 0) {
            Album a; a.name = s.album; a.artist = s.artist;
            unsigned h = 7; for (char c : s.album) h = h * 31 + (unsigned char)c;
            static const Col P[8] = {{222, 89, 26}, {0, 150, 160}, {110, 70, 215}, {200, 35, 75}, {16, 160, 50}, {30, 144, 224}, {236, 150, 10}, {140, 60, 160}};
            a.col = P[h % 8]; gAlbumsV.push_back(a); ai = (int)gAlbumsV.size() - 1;
        } else if (gAlbumsV[ai].artist != s.artist) gAlbumsV[ai].artist = "Various artists";
        gAlbumsV[ai].songs.push_back(i);
        if (std::find(gArtistsV.begin(), gArtistsV.end(), s.artist) == gArtistsV.end()) gArtistsV.push_back(s.artist);
    }
    std::sort(gArtistsV.begin(), gArtistsV.end());
}
static void musSaveCache() {
    std::string o;
    for (auto& s : gSongs) o += s.path + "\t" + s.title + "\t" + s.artist + "\t" + s.album + "\t" + std::to_string(s.track) + "\n";
    writeFile(DIR_ + "music.cache", o);
}
static void musLoadCache() {
    gMusCache.clear();
    std::string d;
    if (!readFile(DIR_ + "music.cache", d)) return;
    size_t p = 0;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(p, e - p); p = e + 1;
        std::vector<std::string> f; size_t q = 0;
        for (int k = 0; k < 5; k++) { size_t t = ln.find('\t', q); if (t == std::string::npos) { f.push_back(ln.substr(q)); break; } f.push_back(ln.substr(q, t - q)); q = t + 1; }
        if (f.size() < 5) continue;
        Song s; s.path = f[0]; s.title = f[1]; s.artist = f[2]; s.album = f[3]; s.track = atoi(f[4].c_str());
        struct stat st; if (stat(s.path.c_str(), &st) == 0) gMusCache.push_back(s);
    }
    gSongs = gMusCache; musBuildLib();
}
static void musStartScan() {
    if (musScanState == 1) return;
    if (musScanThOn) { threadWaitForExit(&musScanTh); threadClose(&musScanTh); musScanThOn = false; }
    musScanState = 1; musScanCount = 0;
    if (R_FAILED(threadCreate(&musScanTh, musScanWorker, nullptr, NULL, 0x40000, 0x2C, -2)) || R_FAILED(threadStart(&musScanTh))) { musScanState = 0; return; }
    musScanThOn = true;
}
static void musCoverFree() {
    for (auto& a : gAlbumsV) { if (a.cover) SDL_DestroyTexture(a.cover); a.cover = nullptr; a.tried = false; }
    if (musNowCover) { SDL_DestroyTexture(musNowCover); musNowCover = nullptr; } musNowCoverFor = -2;
}
static void musAdopt() {
    if (musScanThOn) { threadWaitForExit(&musScanTh); threadClose(&musScanTh); musScanThOn = false; }
    std::string curPath = musIdx >= 0 && musIdx < (int)gSongs.size() ? gSongs[musIdx].path : "";
    musCoverFree();
    gSongs.swap(musScanOut); musScanOut.clear();
    musBuildLib(); musSaveCache();
    musIdx = -1;
    for (size_t i = 0; i < gSongs.size(); i++) if (gSongs[i].path == curPath) musIdx = (int)i;
    musQueue.clear(); musQPos = -1;
    if (musIdx >= 0) { musQueue.push_back(musIdx); musQPos = 0; }
    musAlb = -1; musArt = -1; musScanState = 0;
}
// ---- cover art: embedded picture or folder image, scaled to <=256px
static SDL_Texture* makeCoverTex(SDL_Surface* sf) {
    if (!sf) return nullptr;
    SDL_Surface* sm = sf;
    if (sf->w > 256 || sf->h > 256) {
        float k = 256.0f / std::max(sf->w, sf->h);
        SDL_Surface* d = SDL_CreateRGBSurfaceWithFormat(0, std::max(1, (int)(sf->w * k)), std::max(1, (int)(sf->h * k)), 32, SDL_PIXELFORMAT_RGBA32);
        if (d) { SDL_BlitScaled(sf, nullptr, d, nullptr); sm = d; }
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(gR, sm);
    if (sm != sf) SDL_FreeSurface(sm);
    SDL_FreeSurface(sf);
    return t;
}
static SDL_Texture* songCover(const Song& sg) {
    std::string h, pic;
    if (endsWith(lower(sg.path), ".mp3") && readHead(sg.path, h, 3000000)) {
        id3Parse(h, nullptr, &pic);
        if (!pic.empty()) {
            SDL_Surface* sf = IMG_Load_RW(SDL_RWFromConstMem(pic.data(), (int)pic.size()), 1);
            if (sf) return makeCoverTex(sf);
        }
    }
    size_t sl = sg.path.find_last_of('/');
    std::string dir = sl == std::string::npos ? "" : sg.path.substr(0, sl + 1);
    static const char* N[] = {"cover.jpg", "folder.jpg", "cover.png", "folder.png", "front.jpg", "albumart.jpg", "Cover.jpg", "Folder.jpg", "AlbumArt.jpg"};
    for (const char* n : N) {
        struct stat st; std::string p = dir + n;
        if (stat(p.c_str(), &st) == 0) { SDL_Surface* sf = IMG_Load(p.c_str()); if (sf) return makeCoverTex(sf); }
    }
    return nullptr;
}
static SDL_Texture* albumCover(int ai, bool& budget) {
    if (ai < 0 || ai >= (int)gAlbumsV.size()) return nullptr;
    Album& a = gAlbumsV[ai];
    if (!a.tried && budget && !a.songs.empty()) {
        budget = false; a.tried = true;
        for (int si : a.songs) { a.cover = songCover(gSongs[si]); if (a.cover) break; }
    }
    return a.cover;
}
static int albumOfSong(int si) {
    if (si < 0 || si >= (int)gSongs.size()) return -1;
    for (size_t k = 0; k < gAlbumsV.size(); k++) if (gAlbumsV[k].name == gSongs[si].album) return (int)k;
    return -1;
}
// ---- playback
static void musSettingsSave() { writeFile(DIR_ + "music.ini", std::to_string((int)(musVol * 100)) + " " + std::to_string((int)musShuffle) + " " + std::to_string(musRepeat) + "\n"); }
static void musInit() {
#ifdef WINNX_AUDIO
    if (!musAudioOk) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
            Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_OPUS);
            if (Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 4096) == 0) { musAudioOk = true; Mix_HookMusicFinished(musDone); Mix_SetPostMix(musPost, nullptr); }
            else musMsg = std::string("Audio error: ") + Mix_GetError();
        } else musMsg = "Couldn't start audio";
    }
#else
    musMsg = "This build has no audio library (install switch-sdl2_mixer and rebuild).";
#endif
}
static void musStop() {
#ifdef WINNX_AUDIO
    if (musAudioOk) Mix_HaltMusic();
    if (musCur) { Mix_FreeMusic(musCur); musCur = nullptr; }
#endif
    musPlaying = false; musPaused = false; musPos = 0; musLen = 0; musFinished = false;
}
static void musPlayIdx(int idx) {
    if (idx < 0 || idx >= (int)gSongs.size()) return;
#ifdef WINNX_AUDIO
    musInit();
    if (!musAudioOk) return;
    musStop();
    musCur = Mix_LoadMUS(gSongs[idx].path.c_str());
    if (!musCur) { musMsg = std::string("Can't play this file: ") + Mix_GetError(); musIdx = idx; return; }
    Mix_VolumeMusic((int)(musVol * MIX_MAX_VOLUME));
    musFinished = false;
    if (Mix_PlayMusic(musCur, 1) != 0) { musMsg = Mix_GetError(); return; }
    musIdx = idx; musPlaying = true; musPaused = false; musMsg.clear(); musPos = 0;
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
    musLen = (float)Mix_MusicDuration(musCur);
    if (musLen < 0) musLen = 0;
#endif
#else
    (void)idx;
#endif
}
static void musShuffleQueue(int keepFirst) {      // shuffles everything after position keepFirst
    for (int i = (int)musQueue.size() - 1; i > keepFirst + 1; i--) { int j = keepFirst + 1 + rand() % (i - keepFirst); std::swap(musQueue[i], musQueue[j]); }
}
static void musPlayList(const std::vector<int>& l, int start, bool shuffle) {
    if (l.empty()) return;
    musQueue = l; musShuffle = shuffle;
    if (shuffle) { start = rand() % (int)l.size(); std::swap(musQueue[0], musQueue[start]); musQPos = 0; musShuffleQueue(0); }
    else musQPos = std::max(0, std::min(start, (int)l.size() - 1));
    musPlayIdx(musQueue[musQPos]);
    musSettingsSave();
}
static void musPlayFile(const std::string& path) {
#ifdef WINNX_AUDIO
    int idx = -1;
    for (size_t i = 0; i < gSongs.size(); i++) if (gSongs[i].path == path) idx = (int)i;
    if (idx < 0) {
        Song sg; sg.path = path; size_t sl = path.find_last_of('/'); sg.title = sl == std::string::npos ? path : path.substr(sl + 1);
        size_t dt = sg.title.find_last_of('.'); if (dt != std::string::npos) sg.title = sg.title.substr(0, dt);
        sg.artist = "Unknown artist"; sg.album = "Files";
        gSongs.push_back(sg); musBuildLib(); idx = (int)gSongs.size() - 1;
        for (size_t i = 0; i < gSongs.size(); i++) if (gSongs[i].path == path) idx = (int)i;
    }
    std::vector<int> l = {idx}; musPlayList(l, 0, false);
#else
    (void)path;
#endif
}
static void musNextTrack(int dir, bool auto_) {
    if (musQueue.empty()) return;
    int n = (int)musQueue.size();
    if (auto_ && musRepeat == 2) { musPlayIdx(musQueue[musQPos]); return; }
    if (!auto_ && dir < 0 && musPos > 3.0f) { musPlayIdx(musQueue[musQPos]); return; }
    int np = musQPos + dir;
    if (np >= n || np < 0) {
        if (musRepeat == 1 || !auto_) { np = (np + n) % n; if (musShuffle && np == 0 && dir > 0) musShuffleQueue(-1); }
        else { musStop(); return; }
    }
    musQPos = np; musPlayIdx(musQueue[np]);
}
static void musToggle() {
#ifdef WINNX_AUDIO
    if (!musAudioOk || !musCur) { if (!musQueue.empty()) musPlayIdx(musQueue[std::max(0, musQPos)]); return; }
    if (musPaused) { Mix_ResumeMusic(); musPaused = false; musPlaying = true; }
    else { Mix_PauseMusic(); musPaused = true; }
#endif
}
static void musSeek(float frac) {
#ifdef WINNX_AUDIO
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
    if (musCur && musLen > 0) { float t = std::max(0.0f, std::min(musLen - 0.5f, frac * musLen)); if (Mix_SetMusicPosition(t) == 0) musPos = t; }
#endif
#endif
}
static void musSetVol(float v) {
    musVol = std::max(0.0f, std::min(1.0f, v));
#ifdef WINNX_AUDIO
    if (musAudioOk) Mix_VolumeMusic((int)(musVol * MIX_MAX_VOLUME));
#endif
}
// runs every frame, also while the app is closed (background playback)
static void musTick(float dt) {
    if (musScanState == 2) musAdopt();
    for (int i = 0; i < 32; i++) {
        float t = (musPlaying && !musPaused) ? musLvlIn[(musLvlPos + i) & 31] : 0;
        musLvl[i] += (t - musLvl[i]) * std::min(1.0f, dt * 14);
    }
#ifdef WINNX_AUDIO
    if (!musAudioOk || !musCur) return;
    if (musFinished) { musFinished = false; musNextTrack(1, true); return; }
    if (musPlaying && !musPaused && !musSeeking) {
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
        double p = Mix_GetMusicPosition(musCur);
        if (p >= 0) musPos = (float)p; else musPos += dt;
#else
        musPos += dt;
#endif
    }
#else
    (void)dt;
#endif
}
static void musOpenApp() {
    std::string s;
    if (readFile(DIR_ + "music.ini", s)) { int v = 80, sh = 0, rp = 0; sscanf(s.c_str(), "%d %d %d", &v, &sh, &rp); if (!musAudioOk || !musPlaying) { musVol = v / 100.0f; musShuffle = sh != 0; musRepeat = rp; } }
    if (gSongs.empty()) musLoadCache();
    musStartScan();
    musMsg.clear(); musScroll = 0;
    if (musView == 0 && musIdx < 0) musView = 1;
}
static void musCloseApp(bool full) {
    musSettingsSave();
    musCoverFree();
    if (full) { musStop(); musQueue.clear(); musQPos = -1; musIdx = -1; gSongs.clear(); gAlbumsV.clear(); gArtistsV.clear(); musAlb = musArt = -1; }
}
static std::string musTime(float t) { int m = (int)t / 60, sc = (int)t % 60; char b[16]; snprintf(b, sizeof b, "%d:%02d", m, sc); return b; }

// ---- UI
static const int MUS_BAR = 92, MUS_NAV = 200, MUS_TOP = 96;
static const Col MUS_ACC = {222, 89, 26};
static std::string fitTxt(std::string s, int f, int w) {
    if (tw(s, f) <= w) return s;
    while (s.size() > 1 && tw(s + "...", f) > w) { s.pop_back(); while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back(); }
    return s + "...";
}
static void coverBox(SDL_Texture* t, Col c, int x, int y, int s, const std::string& name, bool big) {
    if (t) { SDL_Rect d = {x, y, s, s}; SDL_RenderCopy(gR, t, nullptr, &d); }
    else {
        rect(x, y, s, s, c);
        rect(x, y + s * 6 / 10, s, s * 4 / 10, BLACK, 40);
        std::string ini = name.empty() ? "?" : name.substr(0, 1);
        text(ini, big ? F130 : F72, x + s / 2, y + s / 2 - (big ? 80 : 46), WHITE, 1, 230);
    }
}
static std::vector<int> musListSongs() {
    std::vector<int> L;
    if (musAlb >= 0 && musAlb < (int)gAlbumsV.size()) return gAlbumsV[musAlb].songs;
    for (int i = 0; i < (int)gSongs.size(); i++) if (musArt < 0 || gSongs[i].artist == gArtistsV[musArt]) L.push_back(i);
    if (musView == 3) std::stable_sort(L.begin(), L.end(), [](int a, int b) { return lower(gSongs[a].title) < lower(gSongs[b].title); });
    return L;
}
static void drawVisualizer(int x, int y, int w, int h, Col c) {
    int n = 32, bw = w / n;
    for (int i = 0; i < n; i++) {
        int bh = 4 + (int)(musLvl[i] * (h - 4));
        rect(x + i * bw, y + h - bh, bw - 3, bh, c, 150 + (int)(100 * musLvl[i]));
    }
}
static void drawMusic() {
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(Col{58, 58, 66}, Col{30, 30, 36}, y / (float)UIH));
    // left navigation
    rect(0, 0, MUS_NAV, UIH - MUS_BAR, Col{20, 20, 24});
    text("music", F30, 24, 22, MUS_ACC);
    static const char* NAV[4] = {"Now playing", "Albums", "Artists", "Songs"};
    for (int i = 0; i < 4; i++) {
        bool on = musView == i;
        if (on) { rect(0, 90 + i * 54, MUS_NAV, 50, Col{255, 255, 255}, 22); rect(0, 90 + i * 54, 6, 50, MUS_ACC); }
        text(NAV[i], F22, 26, 98 + i * 54, WHITE, 0, on ? 255 : 190);
    }
    text(std::to_string(gSongs.size()) + " songs", F16, 26, UIH - MUS_BAR - 66, WHITE, 0, 150);
    text(musScanState == 1 ? ("Scanning... " + std::to_string((int)musScanCount)) : "Rescan", F16, 26, UIH - MUS_BAR - 38, musScanState == 1 ? Col{255, 214, 120} : Col{120, 190, 255});
    int cx0 = MUS_NAV + 30, cw = W - cx0 - 24, top = MUS_TOP, bot = UIH - MUS_BAR;
    bool budget = true;
    const char* title = musView == 0 ? "now playing" : musView == 1 ? "albums" : musView == 2 ? "artists" : "songs";
    // content views
    SDL_Rect cl = {MUS_NAV, top, W - MUS_NAV, bot - top};
    if (musView == 0) {
        text(title, F44, cx0, 24, WHITE);
        int ai = albumOfSong(musIdx);
        if (musNowCoverFor != musIdx) { if (musNowCover) SDL_DestroyTexture(musNowCover); musNowCover = (musIdx >= 0) ? songCover(gSongs[musIdx]) : nullptr; musNowCoverFor = musIdx; }
        Col ac = ai >= 0 ? gAlbumsV[ai].col : MUS_ACC;
        coverBox(musNowCover, ac, cx0, 100, 320, ai >= 0 ? gAlbumsV[ai].name : "", true);
        if (musIdx >= 0) {
            text(fitTxt(gSongs[musIdx].title, F30, 360), F30, cx0 + 344, 100, WHITE);
            text(fitTxt(gSongs[musIdx].artist, F22, 360), F22, cx0 + 344, 142, WHITE, 0, 200);
            text(fitTxt(gSongs[musIdx].album, F16, 360), F16, cx0 + 344, 176, WHITE, 0, 150);
        } else text("Nothing playing", F30, cx0 + 344, 100, WHITE, 0, 190);
        drawVisualizer(cx0 + 344, 300, 360, 110, ac);
        text("up next", F22, cx0 + 344, 424, WHITE, 0, 200);
        SDL_RenderSetClipRect(gR, &cl);
        for (int k = 0; k < 3; k++) {
            int q = musQPos + 1 + k;
            if (q >= 0 && q < (int)musQueue.size()) text(fitTxt(gSongs[musQueue[q]].title, F16, 360), F16, cx0 + 344, 456 + k * 26, WHITE, 0, 170);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (!musMsg.empty()) text(fitTxt(musMsg, F16, cw), F16, cx0, bot - 28, Col{255, 214, 120});
    } else if (musView == 1 && musAlb < 0) {
        text(title, F44, cx0, 24, WHITE);
        if (musArt >= 0) text(fitTxt(gArtistsV[musArt], F22, 500), F22, cx0 + 200, 40, WHITE, 0, 190);
        int cols = 5, tile = 168, gap = (cw - cols * tile) / (cols - 1);
        std::vector<int> A;
        for (int i = 0; i < (int)gAlbumsV.size(); i++) if (musArt < 0 || gAlbumsV[i].artist == gArtistsV[musArt] || gSongs[gAlbumsV[i].songs[0]].artist == gArtistsV[musArt]) A.push_back(i);
        int rows = ((int)A.size() + cols - 1) / cols, rowH = tile + 62;
        musScrollMax = std::max(0, rows * rowH - (bot - top) + 10);
        musScroll = std::max(0, std::min(musScroll, musScrollMax));
        SDL_RenderSetClipRect(gR, &cl);
        for (int k = 0; k < (int)A.size(); k++) {
            int x = cx0 + (k % cols) * (tile + gap), y = top + (k / cols) * rowH - musScroll;
            if (y + rowH < top || y > bot) continue;
            Album& a = gAlbumsV[A[k]];
            coverBox(albumCover(A[k], budget), a.col, x, y, tile, a.name, false);
            text(fitTxt(a.name, F16, tile), F16, x, y + tile + 6, WHITE);
            text(fitTxt(a.artist, F16, tile), F16, x, y + tile + 30, WHITE, 0, 140);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (A.empty()) text(musScanState == 1 ? "Looking for music..." : "No music found", F44, MUS_NAV + (W - MUS_NAV) / 2, 300, WHITE, 1);
        if (A.empty()) text("Put mp3, ogg, flac, wav or opus files anywhere on your SD card.", F22, MUS_NAV + (W - MUS_NAV) / 2, 360, WHITE, 1, 170);
    } else if (musView == 2) {
        text(title, F44, cx0, 24, WHITE);
        int rowH = 60;
        musScrollMax = std::max(0, (int)gArtistsV.size() * rowH - (bot - top) + 10);
        musScroll = std::max(0, std::min(musScroll, musScrollMax));
        SDL_RenderSetClipRect(gR, &cl);
        for (int k = 0; k < (int)gArtistsV.size(); k++) {
            int y = top + k * rowH - musScroll;
            if (y + rowH < top || y > bot) continue;
            int n = 0; for (auto& s : gSongs) if (s.artist == gArtistsV[k]) n++;
            rect(cx0, y, cw, rowH - 4, WHITE, k % 2 ? 10 : 20);
            disc(cx0 + 28, y + 28, 20, Col{90, 90, 100});
            text(gArtistsV[k].substr(0, 1), F22, cx0 + 28, y + 14, WHITE, 1);
            text(fitTxt(gArtistsV[k], F22, 600), F22, cx0 + 66, y + 14, WHITE);
            text(std::to_string(n) + (n == 1 ? " song" : " songs"), F16, cx0 + cw - 16, y + 20, WHITE, 2, 160);
        }
        SDL_RenderSetClipRect(gR, nullptr);
    } else {       // song table (also the track list of an open album / artist)
        std::vector<int> L = musListSongs();
        int hy = 24;
        if (musAlb >= 0) {
            Album& a = gAlbumsV[musAlb];
            coverBox(albumCover(musAlb, budget), a.col, cx0, 30, 200, a.name, false);
            text(fitTxt(a.name, F30, 500), F30, cx0 + 224, 34, WHITE);
            text(fitTxt(a.artist, F22, 500), F22, cx0 + 224, 76, WHITE, 0, 200);
            text(std::to_string(L.size()) + " tracks", F16, cx0 + 224, 110, WHITE, 0, 150);
            rect(cx0 + 224, 144, 150, 44, MUS_ACC); text("Play all", F22, cx0 + 299, 150, WHITE, 1);
            rect(cx0 + 388, 144, 150, 44, Col{255, 255, 255}, 40); text("Shuffle", F22, cx0 + 463, 150, WHITE, 1);
            text("< albums", F16, cx0 + 224, 204, Col{120, 190, 255});
            hy = 236;
        } else text(title, F44, cx0, 24, WHITE);
        if (musArt >= 0 && musAlb < 0) text(fitTxt(gArtistsV[musArt], F22, 500), F22, cx0 + 200, 40, WHITE, 0, 190);
        int ty = musAlb >= 0 ? hy : top;
        rect(cx0, ty, cw, 28, WHITE, 18);
        text("#", F16, cx0 + 12, ty + 3, WHITE, 0, 190); text("title", F16, cx0 + 56, ty + 3, WHITE, 0, 190);
        text("artist", F16, cx0 + 560, ty + 3, WHITE, 0, 190); text("album", F16, cx0 + 820, ty + 3, WHITE, 0, 190);
        int listTop = ty + 30, rowH = 44;
        musScrollMax = std::max(0, (int)L.size() * rowH - (bot - listTop) + 6);
        musScroll = std::max(0, std::min(musScroll, musScrollMax));
        SDL_Rect cl2 = {MUS_NAV, listTop, W - MUS_NAV, bot - listTop};
        SDL_RenderSetClipRect(gR, &cl2);
        for (int k = 0; k < (int)L.size(); k++) {
            int y = listTop + k * rowH - musScroll;
            if (y + rowH < listTop || y > bot) continue;
            const Song& s = gSongs[L[k]];
            bool cur = L[k] == musIdx;
            if (cur) rect(cx0, y, cw, rowH - 3, MUS_ACC, 190); else if (k % 2) rect(cx0, y, cw, rowH - 3, WHITE, 10);
            if (cur && musPlaying && !musPaused) { for (int b = 0; b < 3; b++) rect(cx0 + 10 + b * 6, y + 26 - (int)(musLvl[b * 5] * 18) - 4, 4, 6 + (int)(musLvl[b * 5] * 18), WHITE); }
            else text(std::to_string(k + 1), F16, cx0 + 12, y + 12, WHITE, 0, 160);
            text(fitTxt(s.title, F22, 480), F22, cx0 + 56, y + 8, WHITE);
            text(fitTxt(s.artist, F16, 240), F16, cx0 + 560, y + 14, WHITE, 0, 170);
            text(fitTxt(s.album, F16, 240), F16, cx0 + 820, y + 14, WHITE, 0, 170);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (L.empty()) text(musScanState == 1 ? "Looking for music..." : "No music found", F44, MUS_NAV + (W - MUS_NAV) / 2, 300, WHITE, 1);
    }
    // player bar
    int by = UIH - MUS_BAR;
    rect(0, by, W, MUS_BAR, Col{16, 16, 20});
    rect(0, by, W, 2, MUS_ACC, 160);
    if (musIdx >= 0 && musIdx < (int)gSongs.size()) {
        int ai = albumOfSong(musIdx);
        if (musNowCoverFor != musIdx) { if (musNowCover) SDL_DestroyTexture(musNowCover); musNowCover = songCover(gSongs[musIdx]); musNowCoverFor = musIdx; }
        coverBox(musNowCover, ai >= 0 ? gAlbumsV[ai].col : MUS_ACC, 14, by + 10, 72, ai >= 0 ? gAlbumsV[ai].name : "", false);
        text(fitTxt(gSongs[musIdx].title, F22, 230), F22, 98, by + 16, WHITE);
        text(fitTxt(gSongs[musIdx].artist, F16, 230), F16, 98, by + 50, WHITE, 0, 160);
    } else text("Pick something to play", F16, 24, by + 36, WHITE, 0, 150);
    int cx = 430, cy = by + 46;
    // shuffle, prev, play, next, repeat
    Col sc = musShuffle ? Col{0, 160, 235} : Col{170, 170, 180};
    line(cx - 100, cy + 8, cx - 76, cy - 8, 3, sc); line(cx - 100, cy - 8, cx - 76, cy + 8, 3, sc); tri((float)cx - 76, (float)cy - 14, (float)cx - 76, (float)cy - 2, (float)cx - 66, (float)cy - 8, sc);
    tri((float)cx - 26, (float)cy - 14, (float)cx - 26, (float)cy + 14, (float)cx - 50, (float)cy, WHITE); rect(cx - 56, cy - 14, 4, 28, WHITE);
    disc(cx + 20, cy, 28, MUS_ACC);
    if (musPlaying && !musPaused) { rect(cx + 10, cy - 12, 7, 24, WHITE); rect(cx + 23, cy - 12, 7, 24, WHITE); }
    else tri((float)cx + 12, (float)cy - 14, (float)cx + 12, (float)cy + 14, (float)cx + 36, (float)cy, WHITE);
    tri((float)cx + 66, (float)cy - 14, (float)cx + 66, (float)cy + 14, (float)cx + 90, (float)cy, WHITE); rect(cx + 92, cy - 14, 4, 28, WHITE);
    Col rc = musRepeat ? Col{0, 160, 235} : Col{170, 170, 180};
    ring(cx + 130, cy, 11, 3, rc, Col{16, 16, 20}); if (musRepeat == 2) text("1", F16, cx + 130, cy - 11, rc, 1);
    // seek + volume
    text(musTime(musPos), F16, 600, by + 16, WHITE, 0, 200);
    int sx = 654, sw = 400;
    rect(sx, by + 28, sw, 6, Col{80, 80, 88});
    float fr = musLen > 0 ? std::min(1.0f, musPos / musLen) : 0;
    rect(sx, by + 28, (int)(sw * fr), 6, MUS_ACC); disc(sx + (int)(sw * fr), by + 31, 8, WHITE);
    text(musLen > 0 ? musTime(musLen) : "--:--", F16, sx + sw + 12, by + 16, WHITE, 0, 200);
    text("vol", F16, 1150, by + 16, WHITE, 0, 180);
    rect(1150, by + 52, 100, 6, Col{80, 80, 88}); rect(1150, by + 52, (int)(100 * musVol), 6, WHITE); disc(1150 + (int)(100 * musVol), by + 55, 8, WHITE);
    if (musView != 0 && !musMsg.empty()) text(fitTxt(musMsg, F16, 520), F16, 600, by + 56, Col{255, 214, 120});
}
static void musClickSong(int si) {
    std::vector<int> L = musListSongs();
    int pos = 0; for (size_t i = 0; i < L.size(); i++) if (L[i] == si) pos = (int)i;
    musPlayList(L, pos, false);
}
static void musicTouch(int phase, int x, int y) {
    int by = UIH - MUS_BAR;
    if (phase == 0) {
        musSeeking = musVolDrag = false;
        if (y >= by + 8 && y < by + 62 && x >= 640 && x <= 1070 && y < by + 44) musSeeking = true;
        else if (y >= by + 40 && x >= 1130 && x <= 1270) musVolDrag = true;
    }
    if (musSeeking) { float f = (x - 654) / 400.0f; f = std::max(0.0f, std::min(1.0f, f)); if (musLen > 0) musPos = f * musLen; if (phase == 2) { musSeeking = false; musSeek(f); } }
    if (musVolDrag) { musSetVol((x - 1150) / 100.0f); if (phase == 2) { musVolDrag = false; musSettingsSave(); } }
}
static void musicTap(int x, int y) {
    int by = UIH - MUS_BAR;
    if (musSeeking || musVolDrag) return;
    if (y >= by) {
        int cx = 430;
        if (inR(x, y, cx - 112, by + 14, 50, 64)) { musShuffle = !musShuffle; if (musShuffle && !musQueue.empty()) musShuffleQueue(std::max(0, musQPos)); musSettingsSave(); }
        else if (inR(x, y, cx - 64, by + 14, 56, 64)) musNextTrack(-1, false);
        else if (inR(x, y, cx - 14, by + 8, 68, 76)) { if (musIdx < 0 && !gSongs.empty()) { std::vector<int> all; for (int i = 0; i < (int)gSongs.size(); i++) all.push_back(i); musPlayList(all, 0, false); } else musToggle(); }
        else if (inR(x, y, cx + 56, by + 14, 56, 64)) musNextTrack(1, false);
        else if (inR(x, y, cx + 112, by + 14, 46, 64)) { musRepeat = (musRepeat + 1) % 3; musSettingsSave(); }
        else if (inR(x, y, 14, by + 6, 300, 80) && musIdx >= 0) { musView = 0; musScroll = 0; }
        return;
    }
    if (x < MUS_NAV) {
        for (int i = 0; i < 4; i++) if (inR(x, y, 0, 90 + i * 54, MUS_NAV, 50)) { musView = i; musAlb = -1; musArt = -1; musScroll = 0; return; }
        if (inR(x, y, 0, by - 48, MUS_NAV, 40)) { musStartScan(); }
        return;
    }
    int cx0 = MUS_NAV + 30, cw = W - cx0 - 24, top = MUS_TOP;
    if (musView == 0) return;
    if (musView == 1 && musAlb < 0) {
        int cols = 5, tile = 168, gap = (cw - cols * tile) / (cols - 1), rowH = tile + 62;
        if (y < top) { if (musArt >= 0 && inR(x, y, cx0 + 200, 20, 400, 60)) musArt = -1; return; }
        int c = -1; for (int i = 0; i < cols; i++) if (x >= cx0 + i * (tile + gap) && x < cx0 + i * (tile + gap) + tile) c = i;
        int r = (y - top + musScroll) / rowH;
        if (c < 0) return;
        std::vector<int> A;
        for (int i = 0; i < (int)gAlbumsV.size(); i++) if (musArt < 0 || gAlbumsV[i].artist == gArtistsV[musArt] || gSongs[gAlbumsV[i].songs[0]].artist == gArtistsV[musArt]) A.push_back(i);
        int k = r * cols + c;
        if (k >= 0 && k < (int)A.size()) { musAlb = A[k]; musScroll = 0; }
        return;
    }
    if (musView == 2) {
        if (y < top) return;
        int k = (y - top + musScroll) / 60;
        if (k >= 0 && k < (int)gArtistsV.size()) { musArt = k; musView = 1; musAlb = -1; musScroll = 0; }
        return;
    }
    // song table
    std::vector<int> L = musListSongs();
    int listTop = musAlb >= 0 ? 236 + 30 : top + 30;
    if (musAlb >= 0) {
        if (inR(x, y, cx0 + 224, 144, 150, 44)) { musPlayList(L, 0, false); return; }
        if (inR(x, y, cx0 + 388, 144, 150, 44)) { musPlayList(L, 0, true); return; }
        if (inR(x, y, cx0 + 224, 196, 120, 34)) { musAlb = -1; musScroll = 0; return; }
    }
    if (y >= listTop) {
        int k = (y - listTop + musScroll) / 44;
        if (k >= 0 && k < (int)L.size()) musClickSong(L[k]);
    }
}
static bool musBack() { if (musAlb >= 0) { musAlb = -1; musScroll = 0; return true; } if (musArt >= 0) { musArt = -1; musScroll = 0; return true; } return false; }
// @@APPS_A@@
// ---------------------------------------------------------------- autosave + app lifecycle
static void saveAll() {
    if (gNote.dirty) { writeFile(DIR_ + "notes.txt", gNote.s); gNote.dirty = false; }
    if (wDirty) { wSave(); wDirty = false; }
    if (xDirty) { xSaveSheet(); xDirty = false; }
    if (pDirty && !pDown) { paintSaveFile(DIR_ + "paint.bmp"); pDirty = false; }
    if (g2dirty) { g2Save(); g2dirty = false; }
    if (solDirty) { writeFile(DIR_ + "solitaire.dat", solSer()); solDirty = false; }
    if (gSkyDirty) { skySave(); gSkyDirty = false; }
    // @@SAVE@@
}
static std::string gPhotoOpenPath, gPdfOpenPath;
static void bsodNow(const std::string& code, const std::string& detail); static void cortanaInit(); static void blocksInit(); static void camInit(); static void camStop(); static void pdfInit(); static void chessInit(); static void filesInit(); static void termInit(int which); static void podInit(); static void radioInit(); static void mapsInit(); static void mapsClose(); static void weatherInit(); static void mailInit(); static void newsInit(); static void robloxInit(); static void skyInit(); static void odInit(); static void clkInit(); static void recInit(); static void foodInit(); static void hlInit(); static void petInit(); static void cityInit();
static void appInit(int app) {
    std::string d;
    switch (app) {
        case A_NOTES: readFile(DIR_ + "notes.txt", gNote.s); gNote.scroll = 0; gNote.follow = true; gNote.dirty = false; break;
        case A_WORD: wLoad(); wTab = 1; wView = 0; break;
        case A_EXCEL:
            xLoadAll();
            xr = xc = xScrR = xScrC = 0; xEditing = false; xDirty = false; xTab = 1; break;
        case A_PHOTOS: photoScanAll(); phView = 0; phChip = 0; phScroll = 0;
            if (!gPhotoOpenPath.empty()) { gPhotos.assign(1, gPhotoOpenPath); gPhotoOpenPath.clear(); phIdx = 0; loadPhoto(); phView = 1; phOverlay = true; phSlide = false; }
            break;
        case A_GAMES: scanGames(); gamesScroll = 0; gmInit(); break;
        case A_STORE: storeStart = SDL_GetTicks(); storeView = 0; storeCat = -1; storeScroll = 0; storeSearch.clear(); break;
        case A_BROWSER: loadHist(); brMsg.clear(); break;
        case A_SETTINGS: hbRefresh(); gHbMsg.clear(); gHbWhich = 0; setPage = gSetOpenPage >= 0 ? gSetOpenPage : 0; gSetOpenPage = -1; setSub = 0; break;
        case A_PAINT: paintLoad(); break;
        case A_2048: g2Load(); break;
        case A_MINES: mNew(); break;
        case A_SNAKE: snakeLoad(); break;
        case A_SOLITAIRE: solLoad(); solPressed = solDragging = false; break;
        case A_CALENDAR: calLoad(); break;
        case A_MUSIC: musOpenApp(); break;
        case A_WEATHER: weatherInit(); break;
        case A_MAIL: mailInit(); break;
        case A_NEWS: newsInit(); break;
        case A_ROBLOX: robloxInit(); break;
        case A_FILES: filesInit(); break;
        case A_MAPS: mapsInit(); break;
        case A_PODCASTS: podInit(); break;
        case A_RADIO: radioInit(); break;
        case A_CORTANA: cortanaInit(); break;
        case A_BLOCKS: blocksInit(); break;
        case A_CAMERA: camInit(); break;
        case A_PDF: pdfInit(); break;
        case A_CHESS: chessInit(); break;
        case A_TERM: termInit(0); break;
        case A_PS: termInit(1); break;
        case A_SKYPE: skyInit(); break;
        case A_ONEDRIVE: odInit(); break;
        case A_CLOCK: clkInit(); break;
        case A_RECORDER: recInit(); break;
        case A_FOOD: foodInit(); break;
        case A_HEALTH: hlInit(); break;
        case A_PET: petInit(); break;
        case A_CITY: cityInit(); break;
        // @@INIT@@
        default: break;
    }
}
static void appReset(int app) {
    saveAll();
    switch (app) {
        case A_NOTES: gNote.s.clear(); break;
        case A_WORD: wDoc.clear(); wFreeImgs(); break;
        case A_EXCEL: for (int sh = 0; sh < 3; sh++) for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) { xsAll[sh][r][c].clear(); xfAll[sh][r][c] = 0; } xSheet = 0; break;
        case A_PHOTOS: if (phTex) { SDL_DestroyTexture(phTex); phTex = nullptr; } phFreeThumbs(); gPhotos.clear(); gAllPics.clear(); phView = 0; break;
        case A_GAMES: gGames.clear(); break;
        case A_STORE: storeView = 0; break;
        case A_PAINT: paintFree(); break;
        case A_MUSIC: musCloseApp(true); break;
        case A_MAPS: mapsClose(); break;
        case A_CAMERA: camStop(); break;
        // @@RESET@@
        case A_CALC: cCur = "0"; cAcc = 0; cOp = 0; cFresh = true; cErr = false; break;
        default: break;
    }
    running[app] = false;
}

// ---------------------------------------------------------------- navigation
static bool startBackOut();
static SDL_Rect gAnimRect = {0, 0, 160, 160};
static float openT = 0;           // seconds since the current app finished opening (drives the content fade-in)
static int pendingApp = -1;
static SDL_Rect tileScreen(int i) {
    SDL_Rect r = tileBase(i);
    r.x -= (int)scrollX;
    return r;
}
static void launchApp(int app, SDL_Rect from) {
    if (appDir != 0 || curApp >= 0 || !appAvail(app)) return;
    gAnimRect = from; lastApp = app;
    curApp = app;
    if (!running[curApp]) { float m0 = tmUsedMB(); appInit(curApp); running[curApp] = true; gAppMemMB[curApp] = std::max(1.0f, tmUsedMB() - m0); }
    appDir = 1; appT = 0; barT = 0; barP = 0; openT = 0;
}
static void launch(int tile) {
    if (tile < 0 || tile >= (int)gTiles.size()) return;
    lastTile = tile;
    launchApp(gTiles[tile].app, tileScreen(tile));
}
static void launchAppAuto(int app) {
    int t = tileIndexFor(app);
    if (t >= 0) launch(t);
    else launchApp(app, SDL_Rect{W / 2 - 80, UIH / 2 - 80, 160, 160});
}
static void closeApp(bool reset) {
    if (appDir != 0 || curApp < 0) return;
    if (kbTarget == 3) commitCell();
    kbOpen = false;
    saveAll();
    appDir = -1; appT = 1; gCloseReset = reset;
}
static void winPress() {
    if (overlayOpen()) { closeOverlays(); if (kbTarget == 6) kbOpen = false; if (curApp >= 0) closeApp(false); return; }
    if (curApp >= 0) closeApp(false);
    else if (startBackOut()) {}
    else if (lastApp >= 0 && appAvail(lastApp)) launchAppAuto(lastApp);
}
static int hitTile(int x, int y) {
    if (y >= UIH) return -1;
    for (int i = 0; i < (int)gTiles.size(); i++) {
        SDL_Rect r = tileScreen(i);
        if (inR(x, y, r.x, r.y, r.w, r.h)) return i;
    }
    return -1;
}
static bool inWinBtn(int x, int y) { return abs(x - W / 2) <= 120 && y >= UIH - 6; }
static void showTile(int i) {
    SDL_Rect r = tileBase(i);
    if (r.x - scrollX < 40) scrollX = (float)(r.x - 40);
    if (r.x + r.w - scrollX > W - 40) scrollX = (float)(r.x + r.w - (W - 40));
    scrollX = clampf(scrollX, 0, maxScroll);
}
static void navigate(int dir) { // 0 left 1 right 2 up 3 down
    if (gTiles.empty()) return;
    if (sel < 0 || sel >= (int)gTiles.size()) sel = 0;
    SDL_Rect c = tileBase(sel);
    float cx = c.x + c.w / 2.0f, cy = c.y + c.h / 2.0f;
    int best = -1; float bs = 1e9f;
    for (int i = 0; i < (int)gTiles.size(); i++) {
        if (i == sel) continue;
        SDL_Rect r = tileBase(i);
        float dx = r.x + r.w / 2.0f - cx, dy = r.y + r.h / 2.0f - cy, s;
        if (dir == 0 && dx < -10) s = -dx + 2 * fabsf(dy);
        else if (dir == 1 && dx > 10) s = dx + 2 * fabsf(dy);
        else if (dir == 2 && dy < -10) s = -dy + 2 * fabsf(dx);
        else if (dir == 3 && dy > 10) s = dy + 2 * fabsf(dx);
        else continue;
        if (s < bs) { bs = s; best = i; }
    }
    if (best >= 0) { sel = best; showTile(sel); }
}

// ---------------------------------------------------------------- icons
static void drawIcon(int app, int cx, int cy, Col bg) {
    switch (app) {
        case A_MAIL:
            rect(cx - 29, cy - 20, 58, 4, WHITE); rect(cx - 29, cy + 16, 58, 4, WHITE);
            rect(cx - 29, cy - 20, 4, 40, WHITE); rect(cx + 25, cy - 20, 4, 40, WHITE);
            line(cx - 29, cy - 18, cx, cy + 2, 4, WHITE); line(cx + 29, cy - 18, cx, cy + 2, 4, WHITE);
            break;
        case A_MUSIC:
            ring(cx, cy, 24, 6, WHITE, bg);
            rect(cx - 32, cy + 2, 64, 32, bg);
            rect(cx - 32, cy + 2, 14, 26, WHITE); rect(cx + 18, cy + 2, 14, 26, WHITE);
            break;
        case A_PHOTOS:
            outline(cx - 30, cy - 22, 60, 44, 4, WHITE);
            disc(cx + 14, cy - 8, 6, WHITE);
            line(cx - 26, cy + 16, cx - 6, cy - 4, 4, WHITE); line(cx - 6, cy - 4, cx + 8, cy + 10, 4, WHITE);
            break;
        case A_STORE:
            ring(cx, cy - 8, 14, 5, WHITE, bg);
            rect(cx - 24, cy - 8, 48, 38, WHITE);
            winLogo(cx, cy + 12, 22, bg);
            break;
        case A_CALC:
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) rect(cx - 27 + c * 20, cy - 27 + r * 20, 14, 14, WHITE);
            break;
        case A_NOTES:
            for (int i = 0; i < 4; i++) rect(cx - 28, cy - 24 + i * 14, i == 3 ? 36 : 56, 5, WHITE);
            break;
        case A_WEATHER:
            disc(cx, cy, 16, WHITE);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785398f;
                line(cx + (int)(sinf(a) * 23), cy - (int)(cosf(a) * 23), cx + (int)(sinf(a) * 33), cy - (int)(cosf(a) * 33), 4, WHITE);
            }
            break;
        case A_SETTINGS:
            ring(cx, cy, 20, 8, WHITE, bg);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785398f;
                disc(cx + (int)(sinf(a) * 23), cy - (int)(cosf(a) * 23), 5, WHITE);
            }
            disc(cx, cy, 9, bg);
            break;
        case A_POWER:
            ring(cx, cy + 2, 22, 6, WHITE, bg);
            rect(cx - 8, cy - 28, 16, 22, bg);
            line(cx, cy - 26, cx, cy + 2, 6, WHITE);
            break;
        case A_GAMES:
            rect(cx - 30, cy - 14, 60, 28, WHITE);
            disc(cx - 30, cy, 14, WHITE); disc(cx + 30, cy, 14, WHITE);
            rect(cx - 36, cy - 3, 16, 6, bg); rect(cx - 31, cy - 8, 6, 16, bg);
            disc(cx + 26, cy - 4, 3, bg); disc(cx + 34, cy + 4, 3, bg);
            break;
        case A_SOLITAIRE:
            rect(cx - 26, cy - 30, 40, 56, WHITE); rect(cx - 8, cy - 22, 40, 56, WHITE, 255); outline(cx - 8, cy - 22, 40, 56, 2, bg);
            disc(cx + 12, cy + 6, 9, bg);
            break;
        case A_2048:
            text("2048", F30, cx, cy - 18, WHITE, 1);
            break;
        case A_MINES:
            disc(cx, cy, 15, WHITE);
            for (int k = 0; k < 8; k++) { float a = k * 0.785398f; line(cx + (int)(sinf(a) * 12), cy - (int)(cosf(a) * 12), cx + (int)(sinf(a) * 26), cy - (int)(cosf(a) * 26), 4, WHITE); }
            break;
        case A_SNAKE:
            line(cx - 28, cy + 18, cx - 10, cy + 18, 10, WHITE); line(cx - 10, cy + 18, cx - 10, cy - 4, 10, WHITE);
            line(cx - 10, cy - 4, cx + 22, cy - 4, 10, WHITE); disc(cx + 26, cy - 4, 9, WHITE); disc(cx + 28, cy - 7, 2, bg);
            break;
        case A_PAINT:
            line(cx - 26, cy + 24, cx + 20, cy - 22, 10, WHITE);
            disc(cx - 26, cy + 24, 8, WHITE); disc(cx + 24, cy - 26, 8, bg);
            disc(cx + 10, cy + 20, 8, WHITE); disc(cx + 26, cy + 8, 6, WHITE);
            break;
        case A_BROWSER:
            ring(cx, cy, 28, 5, WHITE, bg);
            rect(cx - 28, cy - 2, 56, 4, WHITE); rect(cx - 2, cy - 28, 4, 56, WHITE);
            rect(cx - 22, cy - 15, 44, 3, WHITE); rect(cx - 22, cy + 12, 44, 3, WHITE);
            break;
        case A_WORD:
            text("W", F72, cx, cy - 46, WHITE, 1);
            break;
        case A_EXCEL:
            text("X", F72, cx, cy - 46, WHITE, 1);
            break;
        case A_CALENDAR:
            outline(cx - 28, cy - 24, 56, 52, 4, WHITE); rect(cx - 28, cy - 24, 56, 14, WHITE);
            for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) rect(cx - 20 + c * 15, cy - 2 + r * 14, 9, 8, WHITE);
            break;
        case A_CLOCK: {
            ring(cx, cy, 28, 4, WHITE, bg);
            line(cx, cy, cx, cy - 17, 4, WHITE); line(cx, cy, cx + 12, cy + 6, 4, WHITE);
            break;
        }
        case A_NEWS:
            outline(cx - 30, cy - 24, 60, 48, 4, WHITE);
            rect(cx - 22, cy - 16, 22, 16, WHITE);
            for (int i = 0; i < 3; i++) rect(cx + 4, cy - 16 + i * 8, 18, 4, WHITE);
            for (int i = 0; i < 2; i++) rect(cx - 22, cy + 4 + i * 8, 44, 4, WHITE);
            break;
        case A_ROBLOX:
            for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) if ((i + k) % 2 == 0) rect(cx - 27 + i * 19, cy - 27 + k * 19, 17, 17, WHITE);
            break;
        case A_MAPS:
            quad(cx - 30, cy - 20, cx - 10, cy - 26, cx - 10, cy + 22, cx - 30, cy + 28, WHITE, 255);
            quad(cx - 6, cy - 26, cx + 14, cy - 20, cx + 14, cy + 28, cx - 6, cy + 22, WHITE, 255);
            quad(cx + 18, cy - 20, cx + 32, cy - 26, cx + 32, cy + 22, cx + 18, cy + 28, WHITE, 255);
            disc(cx, cy - 6, 11, bg); disc(cx, cy - 6, 5, WHITE);
            break;
        case A_CAMERA:
            rect(cx - 30, cy - 16, 60, 40, WHITE); rect(cx - 12, cy - 24, 24, 10, WHITE);
            disc(cx, cy + 4, 14, bg); disc(cx, cy + 4, 8, WHITE);
            break;
        case A_PODCASTS:
            disc(cx, cy - 8, 8, WHITE); ring(cx, cy - 8, 18, 4, WHITE, bg); ring(cx, cy - 8, 28, 4, WHITE, bg);
            rect(cx - 5, cy + 6, 10, 22, WHITE);
            rect(cx - 30, cy - 40, 60, 24, bg); disc(cx, cy - 8, 8, WHITE);
            break;
        case A_RADIO:
            rect(cx - 30, cy - 12, 60, 38, WHITE); line(cx - 20, cy - 12, cx + 18, cy - 32, 3, WHITE);
            disc(cx - 12, cy + 7, 10, bg); for (int i = 0; i < 3; i++) rect(cx + 6, cy - 2 + i * 8, 18, 3, bg);
            break;
        case A_PDF:
            rect(cx - 22, cy - 30, 44, 58, WHITE); tri(cx + 8, cy - 30, cx + 22, cy - 30, cx + 22, cy - 16, bg);
            text("PDF", F22, cx, cy - 8, bg, 1);
            break;
        case A_TERM: case A_PS:
            outline(cx - 30, cy - 22, 60, 44, 4, WHITE);
            line(cx - 20, cy - 10, cx - 8, cy, 4, WHITE); line(cx - 8, cy, cx - 20, cy + 10, 4, WHITE); rect(cx - 2, cy + 8, 18, 4, WHITE);
            break;
        case A_THEMES:
            disc(cx - 14, cy - 8, 12, WHITE); disc(cx + 14, cy - 8, 12, WHITE, 200); disc(cx, cy + 14, 12, WHITE, 150);
            break;
        case A_CONV:
            line(cx - 26, cy - 12, cx + 26, cy - 12, 4, WHITE); tri(cx + 26, cy - 22, cx + 26, cy - 2, cx + 38, cy - 12, WHITE);
            line(cx + 26, cy + 14, cx - 26, cy + 14, 4, WHITE); tri(cx - 26, cy + 4, cx - 26, cy + 24, cx - 38, cy + 14, WHITE);
            break;
        case A_BLOCKS:
            rect(cx - 24, cy - 24, 24, 24, WHITE); rect(cx, cy - 24, 24, 24, WHITE, 200); rect(cx, cy, 24, 24, WHITE); rect(cx + 24, cy, 24, 24, WHITE, 160);
            break;
        case A_LIGHT:
            disc(cx, cy - 6, 16, WHITE); rect(cx - 8, cy + 10, 16, 14, WHITE);
            for (int i = 0; i < 5; i++) { float a = -1.2f + i * 0.6f; line(cx + (int)(sinf(a) * 24), cy - 6 - (int)(cosf(a) * 24), cx + (int)(sinf(a) * 34), cy - 6 - (int)(cosf(a) * 34), 3, WHITE); }
            break;
        case A_WORLD:
            disc(cx, cy, 28, WHITE); disc(cx, cy, 24, bg);
            line(cx - 24, cy, cx + 24, cy, 2, WHITE); line(cx, cy - 24, cx, cy + 24, 2, WHITE);
            line(cx - 18, cy - 14, cx + 18, cy - 14, 2, WHITE); line(cx - 18, cy + 14, cx + 18, cy + 14, 2, WHITE);
            break;
        case A_DICT:
            rect(cx - 22, cy - 28, 44, 56, WHITE); rect(cx - 22, cy - 28, 6, 56, bg, 90);
            for (int i = 0; i < 3; i++) rect(cx - 8, cy - 16 + i * 12, 24, 4, bg);
            break;
        case A_CORTANA:
            disc(cx, cy, 30, WHITE); disc(cx, cy, 22, bg); disc(cx, cy, 12, WHITE);
            break;
        case A_CHESS:
            disc(cx, cy - 16, 11, WHITE); rect(cx - 8, cy - 6, 16, 4, WHITE);
            quad(cx - 8, cy - 2, cx + 8, cy - 2, cx + 16, cy + 22, cx - 16, cy + 22, WHITE, 255);
            rect(cx - 22, cy + 22, 44, 8, WHITE);
            break;
        case A_TODO:
            for (int i = 0; i < 3; i++) {
                int y = cy - 22 + i * 22;
                line(cx - 28, y + 2, cx - 22, y + 8, 4, WHITE); line(cx - 22, y + 8, cx - 12, y - 4, 4, WHITE);
                rect(cx - 4, y, 32, 5, WHITE);
            }
            break;
        case A_FILES:
            rect(cx - 30, cy - 18, 24, 8, WHITE); rect(cx - 30, cy - 10, 60, 32, WHITE);
            break;
        case A_TASKMGR:
            rect(cx - 28, cy + 4, 12, 20, WHITE); rect(cx - 8, cy - 12, 12, 36, WHITE); rect(cx + 12, cy - 24, 12, 48, WHITE);
            break;
        case A_SKYPE:
            disc(cx, cy, 28, WHITE); tri(cx - 20, cy + 14, cx - 28, cy + 30, cx - 6, cy + 24, WHITE);
            text("S", F44, cx, cy - 24, bg, 1);
            break;
        case A_ONEDRIVE:
            disc(cx - 14, cy + 6, 14, WHITE); disc(cx + 4, cy - 6, 20, WHITE); disc(cx + 20, cy + 8, 13, WHITE);
            rect(cx - 14, cy + 6, 34, 15, WHITE);
            break;
        case A_HELP:
            text("?", F72, cx, cy - 46, WHITE, 1);
            break;
        case A_DESKTOP:
            rect(cx - 30, cy - 24, 60, 40, WHITE); rect(cx - 25, cy - 19, 50, 30, bg);
            rect(cx - 4, cy + 16, 8, 8, WHITE); rect(cx - 16, cy + 24, 32, 4, WHITE);
            rect(cx - 25, cy - 19, 50, 8, WHITE, 70);
            break;
        case A_RECORDER:
            rect(cx - 9, cy - 26, 18, 32, WHITE); ring(cx, cy + 6, 20, 4, WHITE, bg); rect(cx - 2, cy + 24, 4, 10, WHITE); rect(cx - 12, cy + 32, 24, 4, WHITE);
            break;
        case A_FOOD:
            ring(cx, cy, 28, 4, WHITE, bg); rect(cx - 36, cy - 26, 4, 52, WHITE); rect(cx + 34, cy - 26, 4, 52, WHITE); rect(cx - 6, cy - 14, 12, 28, WHITE, 120);
            break;
        case A_HEALTH:
            line(cx - 30, cy + 4, cx - 14, cy + 4, 4, WHITE); line(cx - 14, cy + 4, cx - 6, cy - 22, 4, WHITE); line(cx - 6, cy - 22, cx + 6, cy + 24, 4, WHITE); line(cx + 6, cy + 24, cx + 14, cy + 4, 4, WHITE); line(cx + 14, cy + 4, cx + 30, cy + 4, 4, WHITE);
            break;
        case A_PET:
            disc(cx, cy + 4, 26, WHITE); tri(cx - 24, cy - 12, cx - 12, cy - 34, cx - 4, cy - 14, WHITE); tri(cx + 24, cy - 12, cx + 12, cy - 34, cx + 4, cy - 14, WHITE);
            disc(cx - 9, cy, 4, bg); disc(cx + 9, cy, 4, bg); rect(cx - 3, cy + 8, 6, 3, bg);
            break;
        case A_CITY:
            rect(cx - 32, cy - 4, 20, 36, WHITE); rect(cx - 8, cy - 28, 22, 60, WHITE); rect(cx + 18, cy + 6, 16, 26, WHITE);
            for (int i = 0; i < 4; i++) rect(cx - 3, cy - 22 + i * 12, 8, 6, bg);
            break;
        case A_TIMER:
            ring(cx, cy + 4, 24, 4, WHITE, bg); rect(cx - 6, cy - 26, 12, 6, WHITE);
            line(cx, cy + 4, cx + 10, cy - 8, 4, WHITE);
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- start screen: tiles, editing, all apps, zoom
static int gWifiBars = 0;                  // 0 = offline, 1..3 = connected (filled in by the network module)
static bool liveTileDraw(const Tile& t, SDL_Rect r, float phase, bool& drewAll);
static bool liveCapable(int app) {
    switch (app) { case A_WEATHER: case A_CALENDAR: case A_MAIL: case A_MUSIC: case A_PHOTOS: case A_NEWS: case A_CLOCK: case A_SKYPE: case A_TASKMGR: case A_ONEDRIVE: case A_CORTANA: case A_PET: case A_CITY: case A_HEALTH: case A_FOOD: case A_RECORDER: return true; default: return false; }
}
static void fdiscA(float cx, float cy, float r, Col c, int a) {
    const int N = 48;
    SDL_Vertex v[N + 1];
    int idx[N * 3];
    SDL_Color sc = {c.r, c.g, c.b, (Uint8)a};
    v[0].position.x = cx; v[0].position.y = cy; v[0].color = sc; v[0].tex_coord.x = 0; v[0].tex_coord.y = 0;
    for (int i = 0; i < N; i++) {
        float an = 6.2831853f * i / N;
        v[i + 1].position.x = cx + cosf(an) * r; v[i + 1].position.y = cy + sinf(an) * r;
        v[i + 1].color = sc; v[i + 1].tex_coord.x = 0; v[i + 1].tex_coord.y = 0;
        idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = (i + 1) % N + 1;
    }
    SDL_RenderGeometry(gR, nullptr, v, N + 1, idx, N * 3);
}
// thin transparent ring drawn as a triangle strip
static void ringA(float cx, float cy, float r, float t, Col c, int a) {
    const int N = 72;
    SDL_Vertex v[(N + 1) * 2];
    int idx[N * 6];
    SDL_Color sc = {c.r, c.g, c.b, (Uint8)a};
    for (int i = 0; i <= N; i++) {
        float an = 6.2831853f * i / N, co = cosf(an), si = sinf(an);
        v[i * 2].position.x = cx + co * r; v[i * 2].position.y = cy + si * r;
        v[i * 2 + 1].position.x = cx + co * (r - t); v[i * 2 + 1].position.y = cy + si * (r - t);
        v[i * 2].color = v[i * 2 + 1].color = sc;
        v[i * 2].tex_coord.x = v[i * 2].tex_coord.y = v[i * 2 + 1].tex_coord.x = v[i * 2 + 1].tex_coord.y = 0;
    }
    for (int i = 0; i < N; i++) {
        int k = i * 2;
        idx[i * 6] = k; idx[i * 6 + 1] = k + 1; idx[i * 6 + 2] = k + 2;
        idx[i * 6 + 3] = k + 1; idx[i * 6 + 4] = k + 3; idx[i * 6 + 5] = k + 2;
    }
    SDL_RenderGeometry(gR, nullptr, v, (N + 1) * 2, idx, N * 6);
}
// round button with a chevron: dir 0 down, 1 up, 2 left (the Windows 8 "back" arrow)
static void arrowCircle(int cx, int cy, int r, int dir, Col fg, int a) {
    ringA((float)cx, (float)cy, (float)r, 2.5f, fg, a);
    int s = r / 2;
    if (dir == 0) { line(cx - s, cy - s / 2, cx, cy + s / 2, 3, fg, a); line(cx + s, cy - s / 2, cx, cy + s / 2, 3, fg, a); }
    else if (dir == 1) { line(cx - s, cy + s / 2, cx, cy - s / 2, 3, fg, a); line(cx + s, cy + s / 2, cx, cy - s / 2, 3, fg, a); }
    else { line(cx + s / 2, cy - s, cx - s / 2, cy, 3, fg, a); line(cx + s / 2, cy + s, cx - s / 2, cy, 3, fg, a); line(cx - s / 2, cy, cx + s, cy, 3, fg, a); }
}

// ---- background patterns, drawn with parallax
static const char* PATNAME[NPAT] = {"None", "Circles", "Triangles", "Stripes", "Squares", "Dots"};
static void drawPattern(int pat, float par, int h) {
    if (!pat) return;
    const int PER = 1500;
    float off = fmodf(par, (float)PER); if (off < 0) off += PER;
    Col w = {255, 255, 255};
    for (int k = 0; k < 3; k++) {
        float ox = k * PER - off - PER;
        switch (pat) {
            case 1: {
                static const int C[6][3] = {{180, 170, 150}, {520, 470, 230}, {860, 200, 110}, {1180, 520, 170}, {1380, 120, 90}, {700, 640, 70}};
                for (auto& c : C) fdiscA(ox + c[0], (float)c[1] * h / 668, (float)c[2], w, 16);
                break;
            }
            case 2:
                for (int i = 0; i < 6; i++) {
                    float x = ox + i * 250;
                    tri(x, (float)h, x + 250, (float)h, x + 125, (float)h - 150 - (i % 3) * 120, w, 14);
                    tri(x + 60, 0, x + 310, 0, x + 185, 120 + (i % 2) * 110, w, 9);
                }
                break;
            case 3:
                for (int i = 0; i < 10; i++) { float x = ox + i * 150; quad(x, 0, x + 56, 0, x + 56 - 260, (float)h, x - 260, (float)h, w, 12); }
                break;
            case 4:
                for (int i = 0; i < 7; i++) { int s = 90 + (i * 53) % 150; rect((int)(ox + i * 215), 80 + (i * 197) % (h - s - 80), s, s, w, 14); }
                break;
            default:
                for (int x = 0; x < PER; x += 60) for (int y = 30; y < h; y += 60) rect((int)(ox + x), y, 6, 6, w, 34);
                break;
        }
    }
}
static void drawStartBg(const Theme& th) {
    if (gStartPic && gStartPicTex) {
        float f = maxScroll > 1 ? clampf(scrollX / (float)maxScroll, 0, 1) : 0;
        SDL_Rect d = {-(int)(f * 240), 0, W + 240, UIH};
        SDL_RenderCopy(gR, gStartPicTex, nullptr, &d);
        rect(0, 0, W, UIH, BLACK, 70);
        return;
    }
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(th.top, th.bottom, y / (float)UIH));
    drawPattern(gBgPat, -scrollX * 0.35f, UIH);
}

// ---- account picture
static const Col AVCOL[8] = {{70, 70, 90}, {0, 114, 198}, {16, 160, 50}, {222, 89, 26}, {200, 35, 75}, {110, 70, 215}, {0, 150, 160}, {90, 90, 100}};
static SDL_Texture* gAvatarTex = nullptr;
static void avatarLoad() { if (gAvatarTex) { SDL_DestroyTexture(gAvatarTex); gAvatarTex = nullptr; } SDL_Surface* sf = IMG_Load((DIR_ + "avatar.png").c_str()); if (sf) { gAvatarTex = SDL_CreateTextureFromSurface(gR, sf); SDL_FreeSurface(sf); } }
static void drawAvatar(int x, int y, int sz) {
    if (gAvatarTex) { SDL_Rect d = {x, y, sz, sz}; SDL_RenderCopy(gR, gAvatarTex, nullptr, &d); return; }
    if (gAvInitial) { rect(x, y, sz, sz, AVCOL[gAvatar & 7]); std::string ini = gUser.empty() ? "?" : gUser.substr(0, 1); for (auto& c : ini) c = (char)toupper((unsigned char)c); text(ini, sz >= 120 ? F72 : (sz >= 60 ? F44 : F22), x + sz / 2, y + sz / 2 - (sz >= 120 ? 46 : (sz >= 60 ? 28 : 14)), WHITE, 1); return; }
    rect(x, y, sz, sz, AVCOL[gAvatar & 7]);
    Col f = {215, 215, 228};
    disc(x + sz / 2, y + sz * 2 / 5, sz / 5, f);
    rect(x + sz / 5, y + sz * 3 / 4, sz * 3 / 5, sz / 4, f);
    disc(x + sz / 2, y + sz * 3 / 4 + 1, sz * 3 / 10, f);
    rect(x, y + sz * 3 / 4 + 2, sz, sz / 4 + 1, AVCOL[gAvatar & 7]);
}
static void drawWifiIcon(int x, int y, int bars, Col c, int a = 255) { // x,y = bottom-left
    for (int i = 0; i < 3; i++) rect(x + i * 7, y - 6 - i * 5, 5, 6 + i * 5, c, i < bars ? a : a / 4);
}
static void drawBattery(int x, int y, int pct, Col c, int a = 255) {
    outline(x, y, 26, 14, 2, c); rect(x + 26, y + 4, 3, 6, c, a);
    rect(x + 3, y + 3, std::max(1, 20 * pct / 100), 8, c, a);
}

// ---- state
static bool gEdit = false, gResizeMenu = false, gDrag = false, zoomOut = false, appsOpen = false;
static int gEditSel = -1, gDragTile = -1, gDragX = 0, gDragY = 0, gGrpFrom = -1, gGrpX = 0;
static float gDragOX = 0, gDragOY = 0, zoomP = 0, appsP = 0;
static int appsScroll = 0, appsSort = 0, appsSel = -1;
static SDL_Texture* gStartTex = nullptr;

static void exitEdit() { gEdit = false; gEditSel = -1; gResizeMenu = false; gDrag = false; if (gLayoutDirty) saveLayout(); }
static void commitLayout() { compactGroups(); recalcScroll(); saveLayout(); }

// ---- all apps list
static int appCat(int a) {
    switch (a) {
        case A_MAIL: case A_CALENDAR: case A_NOTES: case A_CALC: case A_WORD: case A_EXCEL: case A_TODO: case A_FILES: return 0;
        case A_MUSIC: case A_PHOTOS: case A_GAMES: case A_ROBLOX: case A_SOLITAIRE: case A_2048: case A_MINES: case A_SNAKE: case A_PAINT: return 1;
        case A_BROWSER: case A_NEWS: case A_WEATHER: case A_STORE: return 2;
        default: return 3;
    }
}
static const char* ACAT[4] = {"Productivity", "Entertainment", "Internet", "Tools"};
struct AppItem { int app; SDL_Rect r; };
static const int AV_X0 = 70, AV_CW = 290, AV_RH = 92, AV_COLS = 4, AV_TOP = 146;
static std::vector<int> appsListSorted() {
    std::vector<int> v;
    for (int a = 0; a < A_COUNT; a++) if (appAvail(a)) v.push_back(a);
    std::sort(v.begin(), v.end(), [](int a, int b) {
        if (appsSort == 1 && appCat(a) != appCat(b)) return appCat(a) < appCat(b);
        return lower(appName(a)) < lower(appName(b));
    });
    return v;
}
// layout of the all-apps view: items and category headers (y already includes the scroll)
static void appsLayout(std::vector<AppItem>& items, std::vector<std::pair<int, int>>& heads, int& total) {
    items.clear(); heads.clear();
    std::vector<int> L = appsListSorted();
    int y = AV_TOP - appsScroll, col = 0, lastCat = -1, y0 = y;
    for (int a : L) {
        if (appsSort == 1 && appCat(a) != lastCat) {
            if (col != 0) { y += AV_RH; col = 0; }
            if (lastCat >= 0) y += 14;
            heads.push_back({appCat(a), y}); y += 44; lastCat = appCat(a);
        }
        items.push_back({a, SDL_Rect{AV_X0 + col * AV_CW, y, AV_CW - 12, AV_RH - 12}});
        if (++col == AV_COLS) { col = 0; y += AV_RH; }
    }
    if (col != 0) y += AV_RH;
    total = y - y0 + 90;
}
static int appsMaxScroll() {
    std::vector<AppItem> it; std::vector<std::pair<int, int>> hd; int total;
    appsLayout(it, hd, total);
    return std::max(0, total - (UIH - AV_TOP - 60));
}
static int appsHit(int x, int y) {
    std::vector<AppItem> it; std::vector<std::pair<int, int>> hd; int total;
    appsLayout(it, hd, total);
    if (y < AV_TOP - 10 || y > UIH - 70) return -1;
    for (auto& i : it) if (inR(x, y, i.r.x, i.r.y, i.r.w, i.r.h)) return i.app;
    return -1;
}

// ---- tile actions
static void setTileSize(int idx, int w, int h) {
    Tile& t = gTiles[idx];
    if (t.w == w && t.h == h) return;
    if (slotFree(t.c, t.r, w, h, idx)) { t.w = w; t.h = h; }
    else {
        int c, r;
        SDL_Rect q = tileBase(idx);
        int ow = t.w, oh = t.h; t.w = 0; t.h = 0; // take the tile out of the way while searching
        if (snapSlot(idx, w, h, (float)q.x, (float)q.y, c, r)) { t.c = c; t.r = r; t.w = w; t.h = h; }
        else { t.w = ow; t.h = oh; }
    }
    commitLayout();
}
static void unpinSelected() {
    if (gEditSel < 0) return;
    int app = gTiles[gEditSel].app;
    gEditSel = -1; gResizeMenu = false;
    unpinApp(app); saveLayout();
    if (sel >= (int)gTiles.size()) sel = 0;
    if (gTiles.empty()) exitEdit();
}
static void uninstallApp(int app) {
    if (ADEF[app].sys || !gInst[app]) return;
    if (running[app]) appReset(app);
    gInst[app] = false;
    unpinApp(app); saveLayout(); saveCfg();
}
static const int BAR_H = 110;
struct BarBtn { int id; std::string label; };
static std::vector<BarBtn> barButtons(bool apps) {
    std::vector<BarBtn> v;
    if (apps) {
        if (appsSel < 0) return v;
        v.push_back({10, tileIndexFor(appsSel) >= 0 ? "Unpin from Start" : "Pin to Start"});
        if (!ADEF[appsSel].sys) v.push_back({2, "Uninstall"});
        return v;
    }
    if (gEditSel < 0 || gEditSel >= (int)gTiles.size()) return v;
    const Tile& t = gTiles[gEditSel];
    v.push_back({1, "Unpin from Start"});
    if (!ADEF[t.app].sys) v.push_back({2, "Uninstall"});
    v.push_back({3, "Resize"});
    if (liveCapable(t.app)) v.push_back({4, t.live ? "Turn live tile off" : "Turn live tile on"});
    v.push_back({5, "Colour"});
    return v;
}
static int barBtnX(int i, int n) { return W / 2 + (int)((i - (n - 1) / 2.0f) * 200); }
static void drawBarIcon(int id, int cx, int cy, Col fg) {
    ring(cx, cy, 27, 3, fg, Col{236, 236, 240});
    switch (id) {
        case 1: case 10: // pin
            line(cx - 9, cy + 9, cx + 9, cy - 9, 3, fg);
            rect(cx - 8, cy - 11, 16, 5, fg); rect(cx - 3, cy - 6, 6, 8, fg);
            if (id == 1) line(cx - 14, cy - 14, cx + 14, cy + 14, 3, Col{196, 43, 28});
            break;
        case 2: // trash
            rect(cx - 10, cy - 9, 20, 3, fg); rect(cx - 4, cy - 13, 8, 4, fg);
            outline(cx - 8, cy - 6, 16, 18, 3, fg); line(cx - 3, cy - 2, cx - 3, cy + 8, 2, fg); line(cx + 3, cy - 2, cx + 3, cy + 8, 2, fg);
            break;
        case 5: // colour swatches
            disc(cx - 6, cy - 4, 8, fg); disc(cx + 6, cy - 4, 8, Col{196, 43, 28}); disc(cx, cy + 7, 8, Col{30, 140, 60});
            break;
        case 3: // resize arrows
            line(cx - 10, cy + 10, cx + 10, cy - 10, 3, fg);
            line(cx + 10, cy - 10, cx + 2, cy - 10, 3, fg); line(cx + 10, cy - 10, cx + 10, cy - 2, 3, fg);
            line(cx - 10, cy + 10, cx - 2, cy + 10, 3, fg); line(cx - 10, cy + 10, cx - 10, cy + 2, 3, fg);
            break;
        default: // live tile: a small tile with a flip arrow
            outline(cx - 11, cy - 9, 22, 18, 3, fg); line(cx - 5, cy + 3, cx + 5, cy + 3, 3, fg); line(cx - 5, cy - 3, cx + 5, cy - 3, 3, fg);
            break;
    }
}
static void drawAppBar(bool apps, float p) {
    std::vector<BarBtn> v = barButtons(apps);
    if (v.empty() && p <= 0) return;
    int off = (int)((1.0f - ease(p)) * BAR_H);
    int y0 = UIH - BAR_H + off;
    rect(0, y0, W, BAR_H, Col{236, 236, 240}, 250);
    rect(0, y0, W, 2, Col{200, 200, 206});
    Col fg = {40, 40, 44};
    int n = (int)v.size();
    for (int i = 0; i < n; i++) {
        int cx = barBtnX(i, n);
        drawBarIcon(v[i].id, cx, y0 + 40, fg);
        text(v[i].label, F16, cx, y0 + 74, fg, 1);
    }
    if (!apps && gResizeMenu && gEditSel >= 0) {
        int ri = 0;
        for (int i = 0; i < n; i++) if (v[i].id == 3) ri = i;
        int cx = barBtnX(ri, n);
        static const char* SZ[4] = {"Small", "Medium", "Wide", "Large"};
        static const int SW[4] = {1, 2, 4, 4}, SH2[4] = {1, 2, 2, 4};
        const Tile& t = gTiles[gEditSel];
        rect(cx - 90, y0 - 4 * 48 - 12, 180, 4 * 48 + 8, Col{236, 236, 240}, 250);
        outline(cx - 90, y0 - 4 * 48 - 12, 180, 4 * 48 + 8, 1, Col{190, 190, 196});
        for (int k = 0; k < 4; k++) {
            int yy = y0 - 4 * 48 - 8 + k * 48;
            bool cur = (t.w == SW[k] && t.h == SH2[k]);
            if (cur) rect(cx - 86, yy, 172, 44, Col{0, 114, 198}, 60);
            text(SZ[k], F22, cx - 74, yy + 9, fg);
        }
    }
}

// ---- tiles
static void drawCheck(int cx, int cy, Col accent) {
    disc(cx, cy, 15, accent); disc(cx, cy, 12, WHITE);
    line(cx - 6, cy, cx - 2, cy + 5, 3, accent); line(cx - 2, cy + 5, cx + 7, cy - 5, 3, accent);
}
static void drawClockFace(SDL_Rect r, Col col, bool withTime) {
    int ccx = r.x + r.w / 2, ccy = r.y + r.h / 2 - (withTime ? 14 : 0);
    int R = std::min(r.w, r.h) / 2 - (withTime ? 40 : 18);
    if (R < 12) return;
    ring(ccx, ccy, R, 5, WHITE, col);
    for (int k = 0; k < 12; k++) {
        float a = k * 0.523599f;
        line(ccx + (int)(sinf(a) * (R - 18)), ccy - (int)(cosf(a) * (R - 18)), ccx + (int)(sinf(a) * (R - 8)), ccy - (int)(cosf(a) * (R - 8)), 3, WHITE);
    }
    float ah = ((gNow.h % 12) + gNow.mi / 60.0f) * 0.523599f, am = gNow.mi * 0.104720f, as = gNow.s * 0.104720f;
    line(ccx, ccy, ccx + (int)(sinf(ah) * R * 0.48f), ccy - (int)(cosf(ah) * R * 0.48f), 6, WHITE);
    line(ccx, ccy, ccx + (int)(sinf(am) * R * 0.72f), ccy - (int)(cosf(am) * R * 0.72f), 4, WHITE);
    line(ccx, ccy, ccx + (int)(sinf(as) * R * 0.8f), ccy - (int)(cosf(as) * R * 0.8f), 2, WHITE, 200);
    disc(ccx, ccy, 6, WHITE);
    if (withTime) text(timeStr(true), F22, r.x + r.w - 12, r.y + r.h - 34, WHITE, 2);
}
static void tileFace(const Tile& t, SDL_Rect r, Col col, bool editing, bool wantLive, bool& anyLive) {
    rect(r.x, r.y, r.w, r.h, col);
    bool all = false;
    SDL_Rect clip = r;
    SDL_Rect old; SDL_RenderGetClipRect(gR, &old);
    SDL_RenderSetClipRect(gR, &clip);
    bool live = wantLive ? liveTileDraw(t, r, 0, all) : false;
    anyLive = live;
    if (!live) {
        if (t.app == A_CLOCK) drawClockFace(r, col, r.h >= 150);
        else drawIcon(t.app, r.x + r.w / 2, r.y + r.h / 2 - (r.h >= 150 ? 12 : 0), col);
    }
    if (gTileNames && !all && (r.h >= 150 || r.w >= 150)) text(ADEF[t.app].name, F16, r.x + 10, r.y + r.h - 26, WHITE);
    if (running[t.app] && !editing) disc(r.x + r.w - 14, r.y + 14, 5, WHITE);
    SDL_RenderSetClipRect(gR, old.w > 0 ? &old : nullptr);
}
static void drawTileRect(const Tile& t, SDL_Rect r, float pressP, bool selected, bool editing, bool shadow) {
    Col col = tileColor(t);
    if (shadow) rect(r.x + 5, r.y + 7, r.w, r.h, BLACK, 90);
    if (pressP > 0) { // pressed tiles shrink a little
        int dx = (int)(r.w * 0.045f * pressP), dy = (int)(r.h * 0.045f * pressP);
        r.x += dx; r.y += dy; r.w -= 2 * dx; r.h -= 2 * dy;
    }
    bool any = false;
    bool alt = t.live && gFlipMode > 0 && !editing && liveCapable(t.app);
    if (!alt) {
        tileFace(t, r, col, editing, t.live, any);
    } else {
        // live face and plain face take turns; the switch is a flip (squeeze) or a slide
        float per = (float)std::max(2, gFlipSecs), T = 0.5f;
        float tm = SDL_GetTicks() / 1000.0f + t.c * 1.3f + t.r * 0.7f + t.app * 0.37f;
        float u = fmodf(tm, per * 2.0f); int face = u < per ? 0 : 1; float m = fmodf(u, per);
        bool probe = false; { SDL_Rect cl = {0, 0, 0, 0}; (void)cl; }
        if (gFlipMode == 1) {
            float sc = 1.0f;
            if (m < T / 2) sc = m / (T / 2); else if (m > per - T / 2) sc = (per - m) / (T / 2);
            sc = std::max(0.02f, sc * sc * (3 - 2 * sc));
            SDL_Rect rr = r; rr.h = std::max(2, (int)(r.h * sc)); rr.y = r.y + (r.h - rr.h) / 2;
            tileFace(t, rr, col, editing, face == 0, probe);
            if (face == 0 && !probe && false) {}
        } else {
            float p = m > per - T ? (m - (per - T)) / T : 0.0f; p = p * p * (3 - 2 * p);
            SDL_Rect clip = r; SDL_Rect old; SDL_RenderGetClipRect(gR, &old); SDL_RenderSetClipRect(gR, &clip);
            SDL_Rect a = r, b = r; a.y = r.y - (int)(p * r.h); b.y = a.y + r.h;
            tileFace(t, a, col, editing, face == 0, probe); if (p > 0) tileFace(t, b, col, editing, face != 0, probe);
            SDL_RenderSetClipRect(gR, old.w > 0 ? &old : nullptr);
        }
    }
    if (selected) { outline(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 3, WHITE); if (editing) drawCheck(r.x + r.w - 20, r.y + 20, THEMES[gTheme].accent); }
}
static void drawTiles(int pressTile, float pressP, float introOffBase, bool useIntro) {
    for (int i = 0; i < (int)gTiles.size(); i++) {
        if (gDrag && i == gDragTile) continue;
        SDL_Rect r = tileScreen(i);
        float tIn = 1.0f;
        if (useIntro) {
            float t = ease((introT - 0.06f * (r.x + scrollX - LEFT) / 160.0f - 0.04f * (r.y / 160.0f)) / 0.55f);
            if (!gAnim) t = 1;
            tIn = t; r.x += (int)((1.0f - t) * 140.0f);
        }
        r.x += (int)introOffBase;
        if (r.x > W || r.x + r.w < 0) continue;
        bool pr = pressTile == i;
        if (useIntro && tIn < 1.0f && gAnim) { fxTileTurn(gTiles[i], r, tIn); continue; }
        drawTileRect(gTiles[i], r, pr ? pressP : 0, (navMode && sel == i && !gEdit) || (gEdit && gEditSel == i), gEdit, false);
    }
}
static void drawGroupHeads(int alpha) {
    int ng = numGroups();
    for (int g = 0; g < ng; g++) {
        int x = colX(g * GCOLS) - (int)scrollX;
        if (x > W || x + 320 < 0) continue;
        std::string nm = g < (int)gGroups.size() ? gGroups[g] : "";
        if (!nm.empty()) text(nm, F22, x, TOP - 36, WHITE, 0, alpha);
        else if (gEdit) { text("Name group", F22, x, TOP - 36, WHITE, 0, alpha / 3); }
        if (gEdit) rect(x, TOP - 8, 320, 2, WHITE, 60);
    }
}

// ---- semantic zoom
static float zoomS0() {
    int ng = std::max(1, numGroups());
    float cw = (float)(ng * (GCOLS * PIT + GGAP) - GGAP);
    return std::max(0.07f, std::min(0.5f, (W - 160) / cw));
}
static void zoomMap(float e, float& A, float& B, float& s) {
    float s0 = zoomS0();
    int ng = std::max(1, numGroups());
    float cw = (float)(ng * (GCOLS * PIT + GGAP) - GGAP);
    float X0 = std::max(60.0f, (W - cw * s0) / 2.0f);
    s = 1 + (s0 - 1) * e;
    A = -scrollX + (X0 - LEFT * s0 + scrollX) * e;
    B = ((UIH - (GROWS * PIT - GAP) * s0) / 2.0f - 10 - TOP * s0) * e;
}
static int zoomGroupAt(int x, int y) {
    float A, B, s; zoomMap(1.0f, A, B, s);
    int ng = numGroups();
    for (int g = 0; g < ng; g++) {
        float gx = A + colX(g * GCOLS) * s, gw = (GCOLS * PIT - GAP) * s;
        float gy = B + TOP * s, gh = (GROWS * PIT - GAP) * s;
        if (x >= gx - 12 && x < gx + gw + 12 && y >= gy - 30 && y < gy + gh + 60) return g;
    }
    return -1;
}
static void drawZoomed(float e, int pressG) {
    int ng = numGroups();
    if (!gStartTex) {
        gStartTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, UIH);
        if (gStartTex) SDL_SetTextureBlendMode(gStartTex, SDL_BLENDMODE_BLEND);
    }
    if (!gStartTex) return;
    float A, B, s; zoomMap(e, A, B, s);
    int ext = colX(ng * GCOLS) + 90;
    int segs = std::max(1, (ext + W - 1) / W);
    float savedScroll = scrollX;
    for (int k = 0; k < segs; k++) {
        SDL_SetRenderTarget(gR, gStartTex);
        SDL_SetRenderDrawColor(gR, 0, 0, 0, 0);
        SDL_RenderClear(gR);
        scrollX = (float)(k * W);
        drawTiles(-1, 0, 0, false);
        SDL_SetRenderTarget(gR, gTarget);
        SDL_Rect dst = {(int)(A + k * W * s), (int)B, (int)(W * s + 1), (int)(UIH * s + 1)};
        SDL_RenderCopy(gR, gStartTex, nullptr, &dst);
    }
    scrollX = savedScroll;
    // group names under the miniature groups
    float A1, B1, s1; zoomMap(1.0f, A1, B1, s1);
    for (int g = 0; g < ng; g++) {
        float gx = A + colX(g * GCOLS) * s, gw = (GCOLS * PIT - GAP) * s;
        float gy = B + TOP * s, gh = (GROWS * PIT - GAP) * s;
        int a = (int)(255 * e);
        if (g == pressG) rect((int)gx - 6, (int)gy - 6, (int)gw + 12, (int)gh + 12, WHITE, (int)(50 * e));
        if (gGrpFrom == g) outline((int)gx - 6, (int)gy - 6, (int)gw + 12, (int)gh + 12, 3, WHITE);
        std::string nm = g < (int)gGroups.size() ? gGroups[g] : "";
        if (!nm.empty()) {
            int maxw = (int)(gw + GGAP * s) - 8;
            while (nm.size() > 3 && tw(nm, F16) > maxw) { nm.pop_back(); }
            if (nm != gGroups[g]) nm += "..";
            text(nm, F16, (int)(gx + gw / 2), (int)(gy + gh + 14), WHITE, 1, a);
        }
    }
    if (gGrpFrom >= 0) { // preview of where the group lands
        float gw = (GCOLS * PIT - GAP) * s1;
        int tg = (int)floorf((gGrpX - (A1 + colX(0) * s1)) / ((GCOLS * PIT + GGAP) * s1) + 0.5f);
        tg = std::max(0, std::min(ng - 1, tg));
        float gx = A1 + colX(tg * GCOLS) * s1, gy = B1 + TOP * s1;
        rect((int)gx - 4, (int)gy - 4, (int)gw + 8, 6, WHITE, 200);
    }
}

// ---- touch state machine for the start screen
static bool stDown = false, stMoved = false, stLift = false, stFired = false;
static int stX0 = 0, stY0 = 0, stLX = 0, stLY = 0, stTile = -1, stMode = 0; // mode 0 undecided, 1 scroll, 2 tile drag, 3 group drag, 4 apps scroll
static Uint32 stT0 = 0;
static float stPress = 0;
static int stGroupPress = -1;
static void startOpenApps() { appsOpen = true; appsScroll = 0; appsSel = -1; if (gEdit) exitEdit(); zoomOut = false; }
static void startCloseApps() { appsOpen = false; appsSel = -1; }
static void startRenameBegin(int g) { gRenameG = g; openKb(9); }
static void startTouch(int phase, int x, int y) {
    if (phase == 0) {
        stDown = true; stMoved = false; stLift = false; stFired = false; stMode = 0; stX0 = stLX = x; stY0 = stLY = y; stT0 = SDL_GetTicks();
        stTile = -1; stGroupPress = -1;
        if (appsOpen) return;
        if (zoomOut) { stGroupPress = zoomGroupAt(x, y); return; }
        bool onBar = (gEditSel >= 0 && y >= UIH - BAR_H);
        if (!onBar) stTile = hitTile(x, y);
        return;
    }
    if (!stDown) return;
    if (phase == 1) {
        int dx = x - stLX, dy = y - stLY;
        if (!stMoved && (abs(x - stX0) > 14 || abs(y - stY0) > 14)) stMoved = true;
        if (stMoved && stMode == 0) {
            if (appsOpen) stMode = 4;
            else if (zoomOut) stMode = (stGroupPress >= 0 && abs(x - stX0) > abs(y - stY0)) ? 3 : 1;
            else if (stTile >= 0 && (gEdit || stLift)) { stMode = 2; gDrag = true; gDragTile = stTile; gEditSel = stTile; gEdit = true; SDL_Rect r = tileScreen(stTile); gDragOX = (float)(stX0 - r.x); gDragOY = (float)(stY0 - r.y); gResizeMenu = false; }
            else stMode = 1;
            if (stMode == 3) { gGrpFrom = stGroupPress; }
        }
        if (stMode == 1) { scrollX = clampf(scrollX - dx, 0, maxScroll); scrollV = (float)-dx; }
        else if (stMode == 2) { gDragX = x; gDragY = y; }
        else if (stMode == 3) gGrpX = x;
        else if (stMode == 4) { appsScroll = std::max(0, std::min(appsMaxScroll(), appsScroll - dy)); }
        stLX = x; stLY = y;
        return;
    }
    // phase 2: release
    stDown = false;
    if (stMode == 2 && gDrag) {
        Tile& t = gTiles[gDragTile];
        int c, r;
        if (snapSlot(gDragTile, t.w, t.h, gDragX - gDragOX + scrollX, gDragY - gDragOY, c, r)) { t.c = c; t.r = r; }
        gDrag = false; gDragTile = -1;
        commitLayout();
        return;
    }
    if (stMode == 3 && gGrpFrom >= 0) {
        float A, B, s; zoomMap(1.0f, A, B, s);
        int ng = numGroups();
        int tg = (int)floorf((gGrpX - (A + colX(0) * s)) / ((GCOLS * PIT + GGAP) * s) + 0.5f);
        tg = std::max(0, std::min(ng - 1, tg));
        moveGroup(gGrpFrom, tg); gGrpFrom = -1;
        commitLayout();
        return;
    }
    gGrpFrom = -1;
    if (stMoved) return;
    if (stFired) { stFired = false; return; }
    // ---- taps
    if (appsOpen) {
        if (appsSel >= 0 && y >= UIH - BAR_H) {
            std::vector<BarBtn> v = barButtons(true); int n = (int)v.size();
            for (int i = 0; i < n; i++) if (abs(x - barBtnX(i, n)) < 90) {
                if (v[i].id == 10) { if (tileIndexFor(appsSel) >= 0) unpinApp(appsSel); else pinApp(appsSel); saveLayout(); }
                else if (v[i].id == 2) { uninstallApp(appsSel); appsSel = -1; }
            }
            return;
        }
        if (inR(x, y, 20, UIH - 78, 60, 60)) { startCloseApps(); return; }
        if (inR(x, y, W - 330, 40, 260, 56)) { appsSort = 1 - appsSort; appsScroll = 0; return; }
        int a = appsHit(x, y);
        if (a >= 0) {
            if (appsSel >= 0) appsSel = (appsSel == a) ? -1 : a;
            else { startCloseApps(); launchAppAuto(a); }
        } else appsSel = -1;
        return;
    }
    if (zoomOut) {
        if (stGroupPress >= 0) { zoomOut = false; scrollX = clampf((float)(colX(stGroupPress * GCOLS) - LEFT), 0, maxScroll); }
        else zoomOut = false;
        return;
    }
    if (gEditSel >= 0 && y >= UIH - BAR_H) {
        std::vector<BarBtn> v = barButtons(false); int n = (int)v.size();
        for (int i = 0; i < n; i++) if (abs(x - barBtnX(i, n)) < 90) {
            int id = v[i].id;
            if (id == 1) unpinSelected();
            else if (id == 2) { int app = gTiles[gEditSel].app; gEditSel = -1; uninstallApp(app); }
            else if (id == 3) gResizeMenu = !gResizeMenu;
            else if (id == 4) { gTiles[gEditSel].live = !gTiles[gEditSel].live; saveLayout(); }
            else if (id == 5) { Tile& tt = gTiles[gEditSel]; tt.tint = (tt.tint + 1) % NTINT; saveLayout(); }
        }
        return;
    }
    if (gEdit && gResizeMenu && gEditSel >= 0) {
        std::vector<BarBtn> v = barButtons(false); int n = (int)v.size(), ri = 0;
        for (int i = 0; i < n; i++) if (v[i].id == 3) ri = i;
        int cx = barBtnX(ri, n), y0 = UIH - BAR_H;
        static const int SW[4] = {1, 2, 4, 4}, SH2[4] = {1, 2, 2, 4};
        for (int k = 0; k < 4; k++) {
            int yy = y0 - 4 * 48 - 8 + k * 48;
            if (inR(x, y, cx - 86, yy, 172, 44)) { setTileSize(gEditSel, SW[k], SH2[k]); gResizeMenu = false; return; }
        }
        gResizeMenu = false;
        return;
    }
    // round buttons
    if (inR(x, y, 14, UIH - 84, 70, 70) && !gEdit) { startOpenApps(); return; }
    if (inR(x, y, W - 84, UIH - 84, 70, 70)) { if (gEdit) exitEdit(); zoomOut = true; return; }
    int t = hitTile(x, y);
    if (gEdit) {
        if (t >= 0) { gEditSel = (gEditSel == t) ? -1 : t; if (gEditSel < 0) exitEdit(); }
        else {
            int ng = numGroups();
            for (int g = 0; g < ng; g++) {
                int gx = colX(g * GCOLS) - (int)scrollX;
                if (inR(x, y, gx, TOP - 50, 320, 44)) { startRenameBegin(g); return; }
            }
            exitEdit();
        }
        return;
    }
    if (t >= 0) { sel = t; launch(t); }
}
static void startUpdate(float dt) {
    // long press: select the tile and enter edit mode
    if (stDown && !stMoved && stTile >= 0 && !gEdit && !zoomOut && !appsOpen && SDL_GetTicks() - stT0 > 450) {
        gEdit = true; gEditSel = stTile; stLift = true; stFired = true;
    }
    if (stDown && !stMoved && appsOpen && appsSel < 0 && SDL_GetTicks() - stT0 > 450) {
        int a = appsHit(stX0, stY0);
        if (a >= 0) { appsSel = a; stMoved = true; }
    }
    // dragging near the edges scrolls the start screen
    if (gDrag) {
        if (gDragX > W - 110) scrollX = clampf(scrollX + 520 * dt, 0, maxScroll);
        else if (gDragX < 110) scrollX = clampf(scrollX - 520 * dt, 0, maxScroll);
    }
    float tz = zoomOut ? 1.0f : 0.0f, ta = appsOpen ? 1.0f : 0.0f;
    float k = gAnim ? std::min(1.0f, dt * 12.0f) : 1.0f;
    zoomP += (tz - zoomP) * k; if (fabsf(zoomP - tz) < 0.004f) zoomP = tz;
    appsP += (ta - appsP) * k; if (fabsf(appsP - ta) < 0.004f) appsP = ta;
    if (stDown && stTile >= 0 && !stMoved) stPress = std::min(1.0f, stPress + dt * 12); else stPress = std::max(0.0f, stPress - dt * 10);
}
static void startPinch(float dist, bool begin) {
    static float d0 = 0;
    if (curApp >= 0 || appsOpen) return;
    if (begin) { d0 = dist; stDown = false; return; }
    if (d0 < 20) return;
    if (dist < d0 * 0.72f) { zoomOut = true; if (gEdit) exitEdit(); }
    else if (dist > d0 * 1.38f) zoomOut = false;
}
// controller: Y edit, D-pad moves the selected tile, ZL/ZR resize, X unpin, R all apps, L zoom
static void startPad(u64 kd) {
    if (appsOpen) { if (kd & HidNpadButton_B) startCloseApps(); return; }
    if (kd & HidNpadButton_R) { startOpenApps(); return; }
    if (kd & HidNpadButton_L) { zoomOut = !zoomOut; if (gEdit) exitEdit(); return; }
    if (zoomOut) { if (kd & HidNpadButton_B) zoomOut = false; return; }
    if (kd & HidNpadButton_Y) {
        if (gEdit) exitEdit(); else if (!gTiles.empty()) { gEdit = true; gEditSel = std::max(0, std::min(sel, (int)gTiles.size() - 1)); navMode = true; }
        return;
    }
    if (!gEdit || gEditSel < 0 || gEditSel >= (int)gTiles.size()) return;
    if (kd & HidNpadButton_B) { exitEdit(); return; }
    if (kd & HidNpadButton_X) { unpinSelected(); return; }
    static const int SW[4] = {1, 2, 4, 4}, SH2[4] = {1, 2, 2, 4};
    if (kd & (HidNpadButton_ZL | HidNpadButton_ZR)) {
        Tile& t = gTiles[gEditSel];
        int cur = 1;
        for (int k = 0; k < 4; k++) if (t.w == SW[k] && t.h == SH2[k]) cur = k;
        cur = (kd & HidNpadButton_ZR) ? std::min(3, cur + 1) : std::max(0, cur - 1);
        setTileSize(gEditSel, SW[cur], SH2[cur]);
        return;
    }
    int dx = 0, dy = 0;
    if (kd & HidNpadButton_AnyLeft) dx = -1; else if (kd & HidNpadButton_AnyRight) dx = 1;
    else if (kd & HidNpadButton_AnyUp) dy = -1; else if (kd & HidNpadButton_AnyDown) dy = 1;
    if (!dx && !dy) return;
    Tile& t = gTiles[gEditSel];
    SDL_Rect q = tileBase(gEditSel);
    for (int step = 1; step <= 8; step++) {
        float wx = (float)q.x + dx * step * PIT + (dx ? 0 : 0), wy = (float)q.y + dy * step * PIT;
        if (dx > 0) wx += (step >= 2 ? GGAP : 0);
        int c, r;
        if (!snapSlot(gEditSel, t.w, t.h, wx, wy, c, r)) break;
        if (c != t.c || r != t.r) {
            bool okDir = (dx < 0 && colX(c) < q.x) || (dx > 0 && colX(c) > q.x) || (dy < 0 && TOP + r * PIT < q.y) || (dy > 0 && TOP + r * PIT > q.y);
            if (!okDir) continue;
            t.c = c; t.r = r; commitLayout(); showTile(gEditSel); sel = gEditSel;
            break;
        }
    }
}

// ---- drawing
static void drawAppsView(float p) {
    float e = ease(p);
    int oy = (int)((1.0f - e) * UIH);
    const Theme& th = THEMES[gTheme];
    Col top = mix(th.top, BLACK, 0.45f), bot = mix(th.bottom, BLACK, 0.45f);
    for (int y = 0; y < UIH; y += 6) rect(0, y + oy, W, 6, mix(top, bot, y / (float)UIH));
    SDL_Rect clip = {0, oy, W, UIH};
    SDL_RenderSetClipRect(gR, &clip);
    text("Apps", F72, 88, 10 + oy, WHITE);
    text(appsSort == 0 ? "by name" : "by category", F22, W - 100, 56 + oy, WHITE, 2, 230);
    line(W - 84, 66 + oy, W - 74, 76 + oy, 3, WHITE); line(W - 64, 66 + oy, W - 74, 76 + oy, 3, WHITE);
    std::vector<AppItem> items; std::vector<std::pair<int, int>> heads; int total;
    appsScroll = std::max(0, std::min(appsScroll, appsMaxScroll()));
    appsLayout(items, heads, total);
    SDL_Rect list = {0, AV_TOP - 12 + oy, W, UIH - AV_TOP + 12 - 70};
    SDL_RenderSetClipRect(gR, &list);
    for (auto& h : heads) text(ACAT[h.first], F30, AV_X0, h.second + oy, WHITE, 0, 230);
    for (auto& it : items) {
        SDL_Rect r = it.r; r.y += oy;
        if (r.y + r.h < 0 || r.y > UIH) continue;
        bool s = (appsSel == it.app);
        rect(r.x, r.y, r.w, r.h, s ? Col{255, 255, 255} : Col{0, 0, 0}, s ? 60 : 70);
        rect(r.x, r.y, r.h, r.h, ADEF[it.app].col);
        SDL_Rect ic = {r.x, r.y, r.h, r.h}, icc;
        if (!SDL_IntersectRect(&ic, &list, &icc)) continue;
        SDL_RenderSetClipRect(gR, &icc);
        drawIcon(it.app, r.x + r.h / 2, r.y + r.h / 2 - (it.app == A_CLOCK ? 0 : 0), ADEF[it.app].col);
        SDL_RenderSetClipRect(gR, &list);
        text(appName(it.app), F22, r.x + r.h + 12, r.y + 12, WHITE);
        if (tileIndexFor(it.app) >= 0) text("Pinned", F16, r.x + r.h + 12, r.y + 44, WHITE, 0, 140);
        if (s) { outline(r.x - 2, r.y - 2, r.w + 4, r.h + 4, 2, WHITE); drawCheck(r.x + r.w - 18, r.y + 18, th.accent); }
    }
    SDL_RenderSetClipRect(gR, &clip);
    arrowCircle(50, UIH - 48 + oy, 24, 1, WHITE, 255);
    SDL_RenderSetClipRect(gR, nullptr);
}
static void drawStart(bool tDown, bool tMoved, int pressTile) {
    (void)tDown; (void)tMoved; (void)pressTile;
    const Theme& th = THEMES[gTheme];
    drawStartBg(th);
    // header
    text("Start", F72, 88, 10, WHITE);
    drawAvatar(W - 100, 40, 48);
    text(gUser, F22, W - 112, 40, WHITE, 2);
    char bb[16]; snprintf(bb, sizeof bb, "%u%%", (unsigned)gBat);
    text(bb, F16, W - 112, 72, WHITE, 2, 190);
    int bw = tw(bb, F16);
    drawBattery(W - 112 - bw - 40, 75, (int)gBat, WHITE, 190);
    drawWifiIcon(W - 112 - bw - 40 - 34, 90, gWifiBars, WHITE, 190);
    if (zoomP < 0.01f) {
        drawGroupHeads(gEdit ? 230 : 200);
        float pp = ease(stPress);
        drawTiles(stDown && !stMoved ? stTile : -1, pp, 0, uiState == 2 && introT < 1.4f);
    } else {
        drawZoomed(ease(zoomP), stMode == 3 || (stDown && stGroupPress >= 0) ? stGroupPress : -1);
    }
    // lifted tile while dragging
    if (gDrag && gDragTile >= 0) {
        const Tile& t = gTiles[gDragTile];
        SDL_Rect r = tileBase(gDragTile);
        r.x = (int)(gDragX - gDragOX); r.y = (int)(gDragY - gDragOY);
        // preview of the slot the tile will snap to
        int c, rr;
        if (snapSlot(gDragTile, t.w, t.h, gDragX - gDragOX + scrollX, gDragY - gDragOY, c, rr)) {
            SDL_Rect s = tileRectCells(c, rr, t.w, t.h); s.x -= (int)scrollX;
            rect(s.x, s.y, s.w, s.h, WHITE, 55); outline(s.x, s.y, s.w, s.h, 2, WHITE);
        }
        int gx = r.w / 20, gy = r.h / 20;
        r.x -= gx; r.y -= gy; r.w += 2 * gx; r.h += 2 * gy;
        drawTileRect(t, r, 0, true, false, true);
    }
    if (zoomP < 0.5f) {
        if (!gEdit && appsP < 0.5f) arrowCircle(50, UIH - 48, 24, 0, WHITE, (int)(255 * (1.0f - zoomP * 2)));
        // zoom-out button
        int zx = W - 50, zy = UIH - 48;
        ringA((float)zx, (float)zy, 20, 2.5f, WHITE, (int)(255 * (1.0f - zoomP * 2)));
        line(zx - 8, zy, zx + 8, zy, 3, WHITE, (int)(255 * (1.0f - zoomP * 2)));
    }
    bool bar = gEdit && gEditSel >= 0;
    static float barA = 0;
    barA += ((bar ? 1.0f : 0.0f) - barA) * 0.35f;
    if (barA > 0.02f) drawAppBar(false, barA);
    if (appsP > 0.01f) {
        drawAppsView(appsP);
        static float barB = 0;
        barB += ((appsSel >= 0 ? 1.0f : 0.0f) - barB) * 0.35f;
        if (barB > 0.02f) { drawAppBar(true, barB); }
    }
}

static bool startBackOut() {
    if (appsOpen) { startCloseApps(); return true; }
    if (zoomOut) { zoomOut = false; return true; }
    if (gEdit) { exitEdit(); return true; }
    return false;
}

// ---------------------------------------------------------------- apps
static void toggle(int x, int y, bool on, Col accent) {
    rect(x, y, 100, 40, on ? accent : Col{90, 90, 90});
    rect(x + (on ? 62 : 4), y + 4, 34, 32, WHITE);
}
static void appTitle(const char* s) { text(s, F72, 70, 34, WHITE); }
static int areaBottom() { return kbOpen ? KBTOP : UIH; }

// ---------------------------------------------------------------- Store (Windows 8.1 look)
static const Col STG = {0, 138, 0};            // store green
static const Col GREYT = {80, 80, 80};
static void star(int cx, int cy, int r, Col c) {
    tri((float)cx, cy - (float)r, cx - r * 0.87f, cy + r * 0.5f, cx + r * 0.87f, cy + r * 0.5f, c);
    tri((float)cx, cy + (float)r, cx - r * 0.87f, cy - r * 0.5f, cx + r * 0.87f, cy - r * 0.5f, c);
}
static void stars(int x, int y, int n) {
    for (int i = 0; i < 5; i++) star(x + 8 + i * 18, y + 8, 8, i < n ? Col{70, 70, 70} : Col{200, 200, 200});
}
static void storeIcon(const SApp& a, int x, int y, int sz) {
    rect(x, y, sz, sz, a.col);
    std::string g = a.glyph;
    int f = sz >= 150 ? (g.size() > 2 ? F44 : F130) : (sz >= 90 ? (g.size() > 2 ? F30 : F72) : (g.size() > 2 ? F16 : F30));
    int fh = f == F130 ? 130 : f == F72 ? 72 : f == F44 ? 44 : f == F30 ? 30 : 16;
    text(g, f, x + sz / 2, y + sz / 2 - (int)(fh * 0.62f), WHITE, 1);
}
static void drawBag(int cx, int cy) {
    ring(cx, cy - 38, 30, 6, WHITE, STG);
    rect(cx - 52, cy - 38, 104, 92, WHITE);
    winLogo(cx, cy + 8, 46, STG);
}
static int storeFeat() { return (int)((SDL_GetTicks() / 6000) % std::max(1, NSTORE)); }
static std::vector<int> storeList() {
    std::vector<int> v;
    std::string q = lower(storeSearch);
    for (int i = 0; i < NSTORE; i++) {
        if (storeCat >= 0 && std::string(STORE[i].cat) != CATS[storeCat]) continue;
        if (!q.empty() && lower(STORE[i].name).find(q) == std::string::npos) continue;
        v.push_back(i);
    }
    return v;
}
static const int ST_LIST_X = 660, ST_LIST_Y = 206, ST_ROW = 84;
static SDL_Rect storeChip(int i) { // i = 0 "All", 1.. categories
    int x = ST_LIST_X;
    for (int k = 0; k < i; k++) x += tw(k == 0 ? "All" : CATS[k - 1], F16) + 28 + 6;
    SDL_Rect r = {x, 150, tw(i == 0 ? "All" : CATS[i - 1], F16) + 28, 38};
    return r;
}
static void storeAct(int idx) {
    const SApp& a = STORE[idx];
    if (storeProg[a.app] > 0) return;
    if (gInst[a.app]) { pendingApp = a.app; closeApp(false); }
    else storeProg[a.app] = 0.001f;
}
static void storeUninstall(int idx) {
    const SApp& a = STORE[idx];
    if (!gInst[a.app]) return;
    if (running[a.app]) appReset(a.app);
    gInst[a.app] = false; unpinApp(a.app); saveLayout(); saveCfg();
}
static void drawStoreBtn(int x, int y, int w, int idx, const Theme& th, bool primary) {
    const SApp& a = STORE[idx];
    if (storeProg[a.app] > 0) {
        rect(x, y + 16, w, 14, Col{210, 210, 210});
        rect(x, y + 16, (int)(w * storeProg[a.app]), 14, STG);
        text("Installing...", F16, x, y - 8, GREYT);
    } else if (primary) {
        btn(x, y, w, 52, gInst[a.app] ? "Open" : "Install", STG, WHITE);
    }
    (void)th;
}
static void drawStore(const Theme& th, int bottom) {
    float st = (SDL_GetTicks() - storeStart) / 1000.0f;
    if (st < 1.4f) { // splash screen
        rect(0, 0, W, UIH, Col{0, 140, 0});
        drawBag(W / 2, UIH / 2 - 40);
        for (int k = 0; k < 8; k++) {
            float ang = st * 6.0f - k * 0.38f;
            disc(W / 2 + (int)(sinf(ang) * 22), UIH / 2 + 110 - (int)(cosf(ang) * 22), 4 - k / 3, WHITE, 255 - k * 28);
        }
        return;
    }
    rect(0, 0, W, UIH, Col{243, 243, 243});
    tri(560, 0, 720, 0, 620, 92, Col{120, 190, 70});
    tri(560, 0, 620, 92, 520, 0, Col{40, 150, 40}, 160);
    // diagonal light wedge in the background
    tri(0, UIH, 340, UIH, 0, UIH - 250, Col{255, 255, 255}, 120);
    if (storeView == 1) { // ---------------- detail page
        const SApp& a = STORE[storeSel];
        ring(60, 70, 26, 3, GREYT, Col{243, 243, 243});
        line(70, 70, 50, 70, 3, GREYT); line(50, 70, 58, 62, 3, GREYT); line(50, 70, 58, 78, 3, GREYT);
        text("Store", F44, 118, 46, GREYT);
        storeIcon(a, 70, 150, 220);
        text(a.name, F44, 320, 146, Col{50, 50, 50});
        text(a.cat, F22, 320, 202, STG);
        stars(320, 238, a.stars);
        text(std::string("(") + a.rate + " ratings)", F16, 320 + 100, 240, GREYT);
        text("Free", F22, 320, 272, GREYT);
        drawStoreBtn(320, 318, 220, storeSel, th, true);
        if (storeProg[a.app] <= 0 && gInst[a.app]) btn(560, 318, 220, 52, "Uninstall", Col{110, 110, 110}, WHITE);
        text("Description", F30, 70, 410, Col{50, 50, 50});
        text(a.d1, F22, 70, 456, Col{60, 60, 60});
        int y = 494;
        for (auto& l : wrapText(a.d2, F22, 1100)) { text(l, F22, 70, y, GREYT); y += 30; }
        return;
    }
    // ---------------- home page
    text("Store", F72, 76, 24, Col{70, 70, 70});
    rect(900, 56, 310, 42, WHITE);
    outline(900, 56, 310, 42, 2, Col{150, 150, 150});
    if (storeSearch.empty() && !(kbOpen && kbTarget == 5)) text("Search for apps", F22, 912, 64, Col{150, 150, 150});
    else text(storeSearch + ((kbOpen && kbTarget == 5 && (SDL_GetTicks() / 500) % 2 == 0) ? "|" : ""), F22, 912, 64, DARK);
    ring(1190, 75, 9, 3, GREYT, WHITE); line(1196, 82, 1203, 90, 3, GREYT);

    std::vector<int> L = storeList();
    int f = storeFeat();
    const SApp& fa = STORE[f];
    // featured banner
    rect(70, 130, 560, 240, fa.col);
    tri(70, 370, 350, 130, 70, 130, Col{255, 255, 255}, 28);
    tri(630, 130, 630, 370, 430, 370, Col{0, 0, 0}, 40);
    text(fa.name, F44, 96, 150, WHITE);
    text(fa.cat, F22, 96, 204, WHITE, 0, 210);
    text(fa.glyph, std::string(fa.glyph).size() > 2 ? F72 : F130, 560, 160, WHITE, 2, 90);
    for (int i = 0; i < NSTORE; i++) rect(96 + i * 18, 346, 12, 5, WHITE, i == f ? 255 : 90);
    // info card under the banner
    rect(92, 384, 120, 120, WHITE);
    storeIcon(fa, 98, 390, 108);
    text(fa.name, F30, 232, 392, STG);
    text(fa.cat, F22, 232, 432, GREYT);
    stars(232, 466, fa.stars);
    int yy = 500;
    for (auto& l : wrapText(fa.d1, F16, 390)) { text(l, F16, 232, yy, GREYT); yy += 22; }
    drawStoreBtn(232, 580, 200, f, th, true);

    // right: category chips + list
    text(storeSearch.empty() ? "Top free apps" : "Results", F30, ST_LIST_X, 106, Col{70, 70, 70});
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = storeChip(i);
        bool on = (storeCat == i - 1);
        rect(r.x, r.y, r.w, r.h, on ? STG : Col{222, 222, 222});
        text(i == 0 ? "All" : CATS[i - 1], F16, r.x + r.w / 2, r.y + 8, on ? WHITE : Col{60, 60, 60}, 1);
    }
    int n = (int)L.size();
    int maxS = std::max(0, n * ST_ROW - (bottom - ST_LIST_Y));
    storeScroll = std::max(0, std::min(storeScroll, maxS));
    SDL_Rect clip = {ST_LIST_X - 10, ST_LIST_Y, W - ST_LIST_X + 10, std::max(1, bottom - ST_LIST_Y)};
    SDL_RenderSetClipRect(gR, &clip);
    for (int i = 0; i < n; i++) {
        int y = ST_LIST_Y + i * ST_ROW - storeScroll;
        if (y + ST_ROW < ST_LIST_Y || y > bottom) continue;
        const SApp& a = STORE[L[i]];
        storeIcon(a, ST_LIST_X, y + 6, 64);
        text(a.name, F22, ST_LIST_X + 82, y + 4, Col{40, 40, 40});
        text("Free", F16, ST_LIST_X + 82, y + 36, GREYT);
        stars(ST_LIST_X + 130, y + 33, a.stars);
        text(a.cat, F16, ST_LIST_X + 82, y + 58, Col{140, 140, 140});
        if (storeProg[a.app] > 0) {
            rect(W - 190, y + 30, 150, 10, Col{210, 210, 210});
            rect(W - 190, y + 30, (int)(150 * storeProg[a.app]), 10, STG);
        } else if (gInst[a.app]) text("Installed", F16, W - 40, y + 26, STG, 2);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (n == 0) text("No apps found", F30, ST_LIST_X, ST_LIST_Y + 20, GREYT);
}
static void storeTap(int x, int y) {
    if (SDL_GetTicks() - storeStart < 1400) return;
    if (storeView == 1) {
        const SApp& a = STORE[storeSel];
        if ((x - 60) * (x - 60) + (y - 70) * (y - 70) < 40 * 40) { storeView = 0; return; }
        if (inR(x, y, 320, 318, 220, 52)) storeAct(storeSel);
        else if (inR(x, y, 560, 318, 220, 52) && gInst[a.app] && storeProg[a.app] <= 0) storeUninstall(storeSel);
        return;
    }
    if (kbOpen && kbTarget == 5 && !inR(x, y, 900, 56, 310, 42)) hideKb();
    if (inR(x, y, 900, 56, 310, 42)) { openKb(5); return; }
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = storeChip(i);
        if (inR(x, y, r.x, r.y, r.w, r.h)) { storeCat = i - 1; storeScroll = 0; return; }
    }
    int f = storeFeat();
    if (inR(x, y, 232, 580, 200, 52)) { storeAct(f); return; }
    if (inR(x, y, 70, 130, 560, 240) || inR(x, y, 92, 384, 520, 190)) { storeSel = f; storeView = 1; hideKb(); return; }
    if (x >= ST_LIST_X - 10 && y >= ST_LIST_Y && y < areaBottom()) {
        std::vector<int> L = storeList();
        int i = (y - ST_LIST_Y + storeScroll) / ST_ROW;
        if (i >= 0 && i < (int)L.size()) { storeSel = L[i]; storeView = 1; hideKb(); }
    }
}

// ---------------------------------------------------------------- CMrhoe browser
struct Bm { const char* name; const char* url; Col c; };
static const Bm BMS[8] = {
    {"DuckDuckGo", "https://duckduckgo.com", {222, 88, 51}},
    {"Google", "https://www.google.com", {66, 133, 244}},
    {"Wikipedia", "https://en.wikipedia.org", {90, 90, 100}},
    {"YouTube", "https://m.youtube.com", {204, 24, 30}},
    {"Reddit", "https://old.reddit.com", {255, 87, 34}},
    {"GitHub", "https://github.com", {36, 41, 46}},
    {"Nintendo", "https://www.nintendo.com", {230, 0, 18}},
    {"News", "https://news.google.com", {20, 130, 120}},
};
static const char* ENG_NAME[3] = {"DDG", "Google", "Bing"};
static const char* ENG_URL[3] = {"https://duckduckgo.com/?q=", "https://www.google.com/search?q=", "https://www.bing.com/search?q="};
static std::string urlEncode(const std::string& in) {
    std::string o; char b[8];
    for (unsigned char c : in) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else if (c == ' ') o += '+';
        else { snprintf(b, sizeof b, "%%%02X", c); o += b; }
    }
    return o;
}
static std::string buildUrl(std::string in) {
    while (!in.empty() && in.back() == ' ') in.pop_back();
    size_t st = in.find_first_not_of(' ');
    if (st == std::string::npos) return "";
    in = in.substr(st);
    std::string l = lower(in);
    if (l.rfind("http://", 0) == 0 || l.rfind("https://", 0) == 0) return in;
    if (in.find(' ') == std::string::npos && in.find('.') != std::string::npos) return "https://" + in;
    return std::string(ENG_URL[gEngine]) + urlEncode(in);
}
static void openUrl(const std::string& url) {
    gHist.erase(std::remove(gHist.begin(), gHist.end(), url), gHist.end());
    gHist.insert(gHist.begin(), url);
    if (gHist.size() > 12) gHist.resize(12);
    saveHist();
    saveAll();
    Result rc;
    if (appletGetAppletType() == AppletType_Application) {
        WebCommonConfig cfg;
        rc = webPageCreate(&cfg, url.c_str());
        if (R_SUCCEEDED(rc)) { webConfigSetWhitelist(&cfg, "^http*"); rc = webConfigShow(&cfg, NULL); }
    } else {
        WebWifiConfig cfg;
        Uuid u; memset(&u, 0, sizeof u);
        webWifiCreate(&cfg, NULL, url.c_str(), u, 0);
        WebWifiReturnValue out;
        rc = webWifiShow(&cfg, &out);
    }
    if (R_FAILED(rc)) {
        char b[96]; snprintf(b, sizeof b, "Couldn't open the page (error 0x%X). Check Wi-Fi.", (unsigned)rc);
        brMsg = b; brMsgT = SDL_GetTicks();
    }
}
static void browserGo() {
    std::string u = buildUrl(gUrl);
    if (u.empty()) return;
    hideKb();
    openUrl(u);
}
static void drawBrowser(int bottom) {
    (void)bottom;
    Col red = {219, 68, 55};
    rect(0, 0, W, UIH, Col{244, 244, 246});
    rect(0, 0, W, 90, red);
    ring(62, 50, 20, 4, WHITE, red);
    rect(42, 48, 40, 3, WHITE); rect(61, 30, 3, 40, WHITE);
    text("CMrhoe", F44, 96, 22, WHITE);
    
    // address bar
    rect(70, 112, 920, 58, WHITE);
    outline(70, 112, 920, 58, 2, kbOpen && kbTarget == 4 ? red : Col{170, 170, 170});
    if (gUrl.empty() && !(kbOpen && kbTarget == 4)) text("Search or enter address", F30, 88, 124, Col{150, 150, 150});
    else {
        std::string t = gUrl;
        while (tw(t, F30) > 880 && t.size() > 1) t.erase(0, 1);
        text(t + ((kbOpen && kbTarget == 4 && (SDL_GetTicks() / 500) % 2 == 0) ? "|" : ""), F30, 88, 124, DARK);
    }
    btn(1000, 112, 90, 58, "Go", red, WHITE);
    btn(1100, 112, 110, 58, ENG_NAME[gEngine], Col{90, 90, 100}, WHITE);
    if (!brMsg.empty()) {
        if (SDL_GetTicks() - brMsgT > 6000) brMsg.clear();
        else text(brMsg, F22, 70, 180, Col{190, 40, 30});
    }
    text("Bookmarks", F30, 70, 214, Col{60, 60, 60});
    for (int i = 0; i < 8; i++) {
        int x = 70 + (i % 4) * 290, y = 260 + (i / 4) * 124;
        rect(x, y, 270, 108, BMS[i].c);
        text(BMS[i].name, F30, x + 16, y + 62, WHITE);
        text(std::string(BMS[i].name).substr(0, 1), F44, x + 16, y + 8, WHITE, 0, 200);
    }
    text("Recent", F30, 70, 512, Col{60, 60, 60});
    int n = std::min<int>(3, gHist.size());
    for (int i = 0; i < n; i++) {
        std::string h = gHist[i];
        while (tw(h, F22) > 1100 && h.size() > 4) h.pop_back();
        rect(70, 556 + i * 34, 1140, 30, WHITE);
        text(h, F22, 82, 558 + i * 34, Col{50, 90, 160});
    }
    if (n == 0) text("Pages you open will show up here.", F22, 70, 558, Col{130, 130, 130});
}
static void browserTap(int x, int y) {
    if (kbOpen && kbTarget == 4 && y < KBTOP && !inR(x, y, 70, 112, 920, 58)) {
        if (inR(x, y, 1000, 112, 90, 58)) { browserGo(); return; }
        hideKb();
    }
    if (inR(x, y, 70, 112, 920, 58)) { openKb(4); return; }
    if (inR(x, y, 1000, 112, 90, 58)) { browserGo(); return; }
    if (inR(x, y, 1100, 112, 110, 58)) { gEngine = (gEngine + 1) % 3; saveCfg(); return; }
    for (int i = 0; i < 8; i++) {
        int bx = 70 + (i % 4) * 290, by = 260 + (i / 4) * 124;
        if (inR(x, y, bx, by, 270, 108)) { openUrl(BMS[i].url); return; }
    }
    int n = std::min<int>(3, gHist.size());
    for (int i = 0; i < n; i++)
        if (inR(x, y, 70, 556 + i * 34, 1140, 30)) { std::string u = gHist[i]; openUrl(u); return; }
}


// ---------------------------------------------------------------- Paint (UI)
static void paintShape(int x0, int y0, int x1, int y1) {
    Uint32 v = packCol(PAL[pCol]);
    int r = std::max(1, PSIZES[pSizeIdx] / 2);
    if (pTool == 3) pLine(x0, y0, x1, y1, r, v);
    else if (pTool == 4) pRectO(x0, y0, x1, y1, r, v);
    else if (pTool == 5) pOval(x0, y0, x1, y1, r, v);
}
static void paintTouch(int ph, int x, int y) {
    paintEnsure();
    int cx = std::max(0, std::min(PCW - 1, x - PCX)), cy = std::max(0, std::min(PCH - 1, y - PCY));
    Uint32 v = packCol(PAL[pCol]), white = 0xFFFFFFFFu;
    int r = std::max(1, PSIZES[pSizeIdx] / 2);
    if (ph == 0) {
        if (!inR(x, y, PCX, PCY, PCW, PCH)) { pDown = false; return; }
        paintPush();
        pDown = true; pSX = pLX = cx; pSY = pLY = cy;
        if (pTool == 0) pDisc(cx, cy, r, v);
        else if (pTool == 1) pDisc(cx, cy, r + 3, white);
        else if (pTool == 2) { pFill(cx, cy, v); pDown = false; }
        else pBase = pPix;
        pTexDirty = pDirty = true;
    } else if (ph == 1 && pDown) {
        if (pTool == 0) pLine(pLX, pLY, cx, cy, r, v);
        else if (pTool == 1) pLine(pLX, pLY, cx, cy, r + 3, white);
        else if (pTool >= 3) { pPix = pBase; paintShape(pSX, pSY, cx, cy); }
        pLX = cx; pLY = cy; pTexDirty = pDirty = true;
    } else if (ph == 2 && pDown) {
        if (pTool >= 3) { pPix = pBase; paintShape(pSX, pSY, cx, cy); pTexDirty = pDirty = true; }
        pDown = false;
    }
}
static void paintTap(int x, int y) {
    paintEnsure();
    if (inR(x, y, PCX, PCY, PCW, PCH)) return;
    for (int i = 0; i < 6; i++) if (inR(x, y, 20 + i * 104, 48, 100, 56)) { pTool = i; return; }
    if (inR(x, y, 660, 48, 50, 56)) { pSizeIdx = std::max(0, pSizeIdx - 1); return; }
    if (inR(x, y, 774, 48, 50, 56)) { pSizeIdx = std::min(5, pSizeIdx + 1); return; }
    if (inR(x, y, 840, 48, 100, 56)) { if (!pUndo.empty()) { pPix = pUndo.back(); pUndo.pop_back(); pTexDirty = pDirty = true; } return; }
    if (inR(x, y, 950, 48, 100, 56)) { paintPush(); std::fill(pPix.begin(), pPix.end(), 0xFFFFFFFFu); pTexDirty = pDirty = true; return; }
    if (inR(x, y, 1060, 48, 100, 56)) { paintExport(); return; }
    for (int i = 0; i < 16; i++) if (inR(x, y, 20 + i * 77, 110, 72, 52)) { pCol = i; if (pTool == 1) pTool = 0; return; }
}
static void drawPaint() {
    paintEnsure();
    rect(0, 0, W, UIH, Col{58, 58, 64});
    static const char* TN[6] = {"Brush", "Eraser", "Fill", "Line", "Rect", "Oval"};
    for (int i = 0; i < 6; i++) btn(20 + i * 104, 48, 100, 56, TN[i], pTool == i ? WHITE : Col{86, 86, 94}, pTool == i ? Col{40, 40, 44} : WHITE);
    btn(660, 48, 50, 56, "-", Col{86, 86, 94}, WHITE);
    rect(714, 48, 56, 56, Col{40, 40, 44});
    text(std::to_string(PSIZES[pSizeIdx]), F22, 742, 62, WHITE, 1);
    btn(774, 48, 50, 56, "+", Col{86, 86, 94}, WHITE);
    btn(840, 48, 100, 56, "Undo", pUndo.empty() ? Col{70, 70, 76} : Col{86, 86, 94}, WHITE);
    btn(950, 48, 100, 56, "Clear", Col{150, 50, 55}, WHITE);
    btn(1060, 48, 100, 56, "Save", Col{200, 70, 160}, WHITE);
    for (int i = 0; i < 16; i++) {
        rect(20 + i * 77, 110, 72, 52, PAL[i]);
        if (i == pCol) { outline(20 + i * 77 - 3, 107, 78, 58, 3, WHITE); }
    }
    if (!pTex) pTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, PCW, PCH);
    if (pTex && pTexDirty) { SDL_UpdateTexture(pTex, nullptr, pPix.data(), PCW * 4); pTexDirty = false; }
    if (pTex) { SDL_Rect d = {PCX, PCY, PCW, PCH}; SDL_RenderCopy(gR, pTex, nullptr, &d); }
    outline(PCX - 2, PCY - 2, PCW + 4, PCH + 4, 2, Col{20, 20, 24});
    if (!pMsg.empty()) {
        if (SDL_GetTicks() - pMsgT > 3500) pMsg.clear();
        else { int w = tw(pMsg, F22) + 40; rect(W / 2 - w / 2, UIH - 70, w, 46, Col{20, 20, 24}, 230); text(pMsg, F22, W / 2, UIH - 59, WHITE, 1); }
    }
}

// ---------------------------------------------------------------- Word (UI) - Office 2016 look
struct WB { int id, x, y, w, h; const char* l; };
struct WG { int x, w; const char* name; };
static int gWPage = 0, gWMargin = 1;
static WPara wClip;
static bool wHasClip = false;
static const int WTOP = 178;
static int WPX = 230, WPW = 820;
static std::vector<int> wY, wH;
static const Col WBLUE = {43, 87, 154};
static const Col WPAGE[5] = {{255, 255, 255}, {255, 248, 225}, {232, 240, 250}, {228, 246, 232}, {245, 232, 245}};
static const char* WTABS[6] = {"File", "Home", "Insert", "Design", "Layout", "View"};
static SDL_Rect wTabRect(int i) { return i == 0 ? SDL_Rect{0, 32, 84, 36} : SDL_Rect{100 + (i - 1) * 112, 32, 104, 36}; }
static void wRibbon(std::vector<WB>& b, std::vector<WG>& g) {
    if (wTab == 1) {
        g = {{8, 162, "Clipboard"}, {176, 214, "Font"}, {396, 262, "Paragraph"}, {664, 372, "Styles"}};
        b = {{16, 16, 76, 70, 72, "Paste"}, {15, 92, 76, 70, 34, "Cut"}, {14, 92, 114, 70, 34, "Copy"},
             {4, 184, 76, 50, 34, "A-"}, {5, 300, 76, 50, 34, "A+"},
             {1, 184, 114, 50, 34, "B"}, {2, 238, 114, 50, 34, "I"}, {3, 292, 114, 50, 34, "U"},
             {12, 404, 76, 50, 34, "\xE2\x80\xA2"}, {13, 458, 76, 50, 34, "1."},
             {9, 404, 114, 50, 34, "L"}, {10, 458, 114, 50, 34, "C"}, {11, 512, 114, 50, 34, "R"},
             {17, 566, 76, 84, 34, "New"}, {18, 566, 114, 84, 34, "Delete"},
             {6, 672, 76, 112, 72, "Normal"}, {7, 792, 76, 112, 72, "Heading 1"}, {8, 912, 76, 112, 72, "Heading 2"}};
    } else if (wTab == 2) {
        g = {{8, 124, "Pages"}, {138, 124, "Illustrations"}, {268, 124, "Symbols"}, {398, 240, "Text"}};
        b = {{24, 16, 76, 108, 72, "Divider"}, {20, 146, 76, 108, 72, "Picture"}, {21, 276, 76, 108, 72, "Symbol"},
             {22, 406, 76, 108, 72, "Date"}, {23, 522, 76, 108, 72, "Time"}};
    } else if (wTab == 3) {
        g = {{8, 460, "Page Color"}};
        for (int i = 0; i < 5; i++) b.push_back({40 + i, 16 + i * 88, 76, 80, 72, ""});
    } else if (wTab == 4) {
        g = {{8, 360, "Margins"}};
        b = {{45, 16, 76, 110, 72, "Narrow"}, {46, 132, 76, 110, 72, "Normal"}, {47, 248, 76, 110, 72, "Wide"}};
    } else if (wTab == 5) {
        g = {{8, 360, "Zoom"}};
        b = {{50, 16, 76, 110, 72, "75%"}, {51, 132, 76, 110, 72, "100%"}, {52, 248, 76, 110, 72, "125%"}};
    } else {
        g = {{8, 270, "Document"}};
        b = {{30, 16, 76, 110, 72, "New"}, {31, 132, 76, 110, 72, "Save"}};
    }
}
static int wFontOf(const WPara& p) {
    if (p.style == 1) return std::min(F72, gDocFont + 2);
    if (p.style == 2) return std::min(F72, gDocFont + 1);
    return gDocFont;
}
static const std::vector<std::string>& wLines(WPara& p, int f, int w) {
    if (p.cf != f || p.cw != w || p.cs != p.t) {
        p.lines = wrapText(p.t, f, w);
        p.cs = p.t; p.cf = f; p.cw = w;
    }
    return p.lines;
}
static void wApply(int id) {
    wEnsure();
    WPara& p = wDoc[wCur];
    bool textual = p.style < 3;
    switch (id) {
        case 1: p.b = !p.b; break;
        case 2: p.i = !p.i; break;
        case 3: p.u = !p.u; break;
        case 4: gDocFont = std::max(F16, gDocFont - 1); saveCfg(); break;
        case 5: gDocFont = std::min(F44, gDocFont + 1); saveCfg(); break;
        case 6: if (textual) p.style = 0; break;
        case 7: if (textual) p.style = (p.style == 1 ? 0 : 1); break;
        case 8: if (textual) p.style = (p.style == 2 ? 0 : 2); break;
        case 9: p.align = 0; break;
        case 10: p.align = 1; break;
        case 11: p.align = 2; break;
        case 12: if (textual) p.list = (p.list == 1 ? 0 : 1); break;
        case 13: if (textual) p.list = (p.list == 2 ? 0 : 2); break;
        case 14: wClip = p; wHasClip = true; return;
        case 15: wClip = p; wHasClip = true; if (wDoc.size() > 1) { wDoc.erase(wDoc.begin() + wCur); wCur = std::max(0, wCur - 1); } else p = WPara(); break;
        case 16: if (wHasClip) { WPara np = wClip; np.cf = -1; wInsertAfter(np); } break;
        case 17: { WPara np; np.align = p.align; wInsertAfter(np); } break;
        case 18: if (wDoc.size() > 1) { wDoc.erase(wDoc.begin() + wCur); wCur = std::max(0, wCur - 1); } else p = WPara(); break;
        case 20: wScanPics(); wView = 1; hideKb(); return;
        case 21: wView = 2; hideKb(); return;
        case 22: wType(dateStr()); openKb(2); break;
        case 23: wType(timeStr()); openKb(2); break;
        case 24: { WPara np; np.style = 3; wInsertAfter(np); WPara t; wInsertAfter(t); } break;
        case 30: backupFile(DIR_ + "document1.wdoc"); wDoc.clear(); wEnsure(); wScroll = 0; break;
        case 31: break;
        case 40: case 41: case 42: case 43: case 44: gWPage = id - 40; return;
        case 45: gWMargin = 0; return;
        case 46: gWMargin = 1; return;
        case 47: gWMargin = 2; return;
        case 50: gDocFont = F16; saveCfg(); return;
        case 51: gDocFont = F22; saveCfg(); return;
        case 52: gDocFont = F30; saveCfg(); return;
        default: break;
    }
    wDirty = true; wFollow = true;
}
static const char* SYMS[40] = {"\xC2\xA9", "\xC2\xAE", "\xE2\x84\xA2", "\xC2\xB0", "\xC2\xB1", "\xC3\x97", "\xC3\xB7", "\xE2\x82\xAC", "\xC2\xA3", "\xC2\xA5",
    "\xC2\xA7", "\xC2\xB6", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94", "\xE2\x80\xA6", "\xC2\xAB", "\xC2\xBB", "\xC2\xBD", "\xC2\xBC",
    "\xC2\xBE", "\xC2\xB2", "\xC2\xB3", "\xC2\xB5", "\xCF\x80", "\xCE\xA9", "\xCE\xB1", "\xCE\xB2", "\xE2\x89\xA4", "\xE2\x89\xA5",
    "\xE2\x89\xA0", "\xE2\x88\x9E", "\xE2\x88\x9A", "\xE2\x86\x90", "\xE2\x86\x92", "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x9C\x93", "\xE2\x98\x85", "\xE2\x99\xA5"};
static void drawWord(int bottom) {
    (void)bottom;
    Col blue = WBLUE;
    int docBottom = kbOpen ? KBTOP : UIH - 28;
    int margin = gWMargin == 0 ? 20 : (gWMargin == 1 ? 40 : 80);
    WPX = 190 + margin; WPW = 900 - 2 * margin;
    Col pageCol = WPAGE[gWPage];
    rect(0, 0, W, UIH, Col{229, 231, 235});
    wEnsure();
    // ---- document area (drawn first so the ribbon overlaps cleanly)
    SDL_Rect clip = {0, WTOP, W, std::max(1, docBottom - WTOP)};
    rect(190, WTOP, 900, docBottom - WTOP, pageCol);
    wY.assign(wDoc.size(), 0); wH.assign(wDoc.size(), 0);
    int y = 0, num = 0;
    for (int i = 0; i < (int)wDoc.size(); i++) {
        WPara& p = wDoc[i];
        int f = wFontOf(p);
        int lh = gFont[f] ? TTF_FontLineSkip(gFont[f]) : 30;
        int indent = p.list ? 44 : 0;
        int h;
        if (p.style == 3) h = 24;
        else if (p.style == 4) {
            WImg* im = wImage(p.img);
            if (im) { float sc = std::min(std::min(1.0f, (float)WPW / im->w), 300.0f / im->h); h = (int)(im->h * sc) + 8; } else h = 50;
        } else {
            int n = (int)wLines(p, f, WPW - indent).size();
            h = std::max(1, n) * lh;
        }
        wY[i] = y; wH[i] = h; y += h + 8;
    }
    int total = y + 80;
    int view = docBottom - WTOP - 20;
    int maxS = std::max(0, total - view);
    if (wFollow) {
        int top = wY[wCur], bot = wY[wCur] + wH[wCur];
        if (bot - wScroll > view - 20) wScroll = bot - view + 20;
        if (top - wScroll < 0) wScroll = top;
        wFollow = false;
    }
    wScroll = std::max(0, std::min(wScroll, maxS));
    SDL_RenderSetClipRect(gR, &clip);
    num = 0;
    for (int i = 0; i < (int)wDoc.size(); i++) {
        WPara& p = wDoc[i];
        int py = WTOP + 18 + wY[i] - wScroll;
        if (!(p.style < 3 && p.list == 2)) num = 0;
        if (py + wH[i] < WTOP || py > docBottom) { if (p.list == 2 && p.style < 3) num++; continue; }
        bool cur = (i == wCur);
        if (cur) rect(WPX - 24, py - 2, 3, wH[i] + 4, Col{120, 160, 220});
        if (p.style == 3) { rect(WPX, py + 10, WPW, 2, Col{110, 110, 110}); continue; }
        if (p.style == 4) {
            WImg* im = wImage(p.img);
            if (im) {
                float sc = std::min(std::min(1.0f, (float)WPW / im->w), 300.0f / im->h);
                int dw = (int)(im->w * sc), dh = (int)(im->h * sc);
                int dx = WPX + (p.align == 1 ? (WPW - dw) / 2 : (p.align == 2 ? WPW - dw : 0));
                SDL_Rect d = {dx, py + 4, dw, dh};
                SDL_RenderCopy(gR, im->t, nullptr, &d);
                if (cur) outline(dx - 2, py + 2, dw + 4, dh + 4, 2, Col{120, 160, 220});
            } else text("[picture not found]", F22, WPX, py + 8, Col{170, 60, 60});
            continue;
        }
        int f = wFontOf(p);
        int lh = gFont[f] ? TTF_FontLineSkip(gFont[f]) : 30;
        int indent = p.list ? 44 : 0;
        int st = (p.b || p.style == 1 ? 1 : 0) | (p.i ? 2 : 0) | (p.u ? 4 : 0);
        Col ink = p.style == 1 ? blue : (p.style == 2 ? Col{70, 110, 170} : DARK);
        const std::vector<std::string>& L = wLines(p, f, WPW - indent);
        if (p.list == 1) disc(WPX + 14, py + lh / 2, 5, ink);
        if (p.list == 2) { num++; text(std::to_string(num) + ".", f, WPX + 34, py, ink, 2); }
        if (p.t.empty() && wDoc.size() == 1 && !p.list && p.style == 0) text("Tap here to start typing", f, WPX, py, Col{170, 170, 170});
        for (int k = 0; k < (int)L.size(); k++) {
            int ly = py + k * lh;
            int x = WPX + indent, al = 0;
            if (p.align == 1) { x = WPX + indent + (WPW - indent) / 2; al = 1; }
            else if (p.align == 2) { x = WPX + WPW; al = 2; }
            textS(L[k], f, x, ly, ink, st, al);
        }
        if (cur && kbOpen && kbTarget == 2 && ((SDL_GetTicks() / 500) % 2 == 0)) {
            const std::string& last = L.empty() ? std::string() : L.back();
            int lw = twS(last, f, st);
            int ly = py + (std::max(1, (int)L.size()) - 1) * lh;
            int cx = WPX + indent + lw + 2;
            if (p.align == 1) cx = WPX + indent + (WPW - indent) / 2 + lw / 2 + 2;
            else if (p.align == 2) cx = WPX + WPW + 2;
            rect(cx, ly + 3, 2, lh - 6, ink);
        }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    // ---- title bar + tab row (Office 2016)
    rect(0, 0, W, 68, blue);
    text("Document1 - Word", F22, W / 2, 4, WHITE, 1);
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = wTabRect(i);
        if (i == 0) { rect(r.x, r.y, r.w, r.h, Col{30, 66, 122}); text(WTABS[i], F22, r.x + r.w / 2, r.y + 4, WHITE, 1); }
        else if (wTab == i) { rect(r.x, r.y + 2, r.w, r.h - 2, Col{246, 246, 248}); text(WTABS[i], F22, r.x + r.w / 2, r.y + 4, blue, 1); }
        else text(WTABS[i], F22, r.x + r.w / 2, r.y + 4, WHITE, 1);
    }
    // ---- ribbon
    rect(0, 68, W, 110, Col{246, 246, 248});
    rect(0, 177, W, 1, Col{200, 200, 206});
    std::vector<WB> bs; std::vector<WG> gs;
    wRibbon(bs, gs);
    for (auto& gp : gs) {
        text(gp.name, F16, gp.x + gp.w / 2, 154, Col{100, 100, 110}, 1);
        rect(gp.x + gp.w + 2, 76, 1, 92, Col{210, 210, 216});
    }
    WPara& cp = wDoc[wCur];
    for (auto& bt : bs) {
        bool on = false;
        switch (bt.id) {
            case 1: on = cp.b; break; case 2: on = cp.i; break; case 3: on = cp.u; break;
            case 6: on = cp.style == 0; break; case 7: on = cp.style == 1; break; case 8: on = cp.style == 2; break;
            case 9: on = cp.align == 0; break; case 10: on = cp.align == 1; break; case 11: on = cp.align == 2; break;
            case 12: on = cp.list == 1; break; case 13: on = cp.list == 2; break;
            case 40: case 41: case 42: case 43: case 44: on = (gWPage == bt.id - 40); break;
            case 45: on = gWMargin == 0; break; case 46: on = gWMargin == 1; break; case 47: on = gWMargin == 2; break;
            case 50: on = gDocFont == F16; break; case 51: on = gDocFont == F22; break; case 52: on = gDocFont == F30; break;
            default: break;
        }
        rect(bt.x, bt.y, bt.w, bt.h, on ? Col{198, 218, 246} : Col{236, 238, 243});
        if (on) outline(bt.x, bt.y, bt.w, bt.h, 2, Col{110, 150, 215});
        if (bt.id >= 40 && bt.id <= 44) { rect(bt.x + 8, bt.y + 8, bt.w - 16, bt.h - 16, WPAGE[bt.id - 40]); outline(bt.x + 8, bt.y + 8, bt.w - 16, bt.h - 16, 1, Col{160, 160, 170}); continue; }
        if (bt.id >= 6 && bt.id <= 8) { // styles gallery tile: sample + name
            Col sc = bt.id == 6 ? DARK : (bt.id == 7 ? blue : Col{70, 110, 170});
            textS("AaBbCc", bt.id == 7 ? F30 : F22, bt.x + bt.w / 2, bt.y + 10, sc, 0, 1);
            text(bt.l, F16, bt.x + bt.w / 2, bt.y + bt.h - 24, Col{90, 90, 100}, 1);
            continue;
        }
        int st = bt.id == 1 ? 1 : (bt.id == 2 ? 2 : (bt.id == 3 ? 4 : 0));
        bool big = bt.h > 40;
        textS(bt.l, F22, bt.x + bt.w / 2, bt.y + (big ? bt.h / 2 - 15 : 3), DARK, st, 1);
    }
    if (wTab == 1) { // font size read-out between A- and A+
        static const char* SZ[4] = {"11", "14", "18", "26"};
        text(SZ[std::max(0, std::min(3, gDocFont))], F22, 270, 80, DARK, 1);
        text("Font size", F16, 270, 114, Col{120, 120, 130}, 1);
    }
    if (wTab == 0) {
        text("Autosaved to switch/WinNX/document1.wdoc", F16, 270, 100, Col{90, 90, 100}, 0);
        text(std::to_string(wWords()) + " words, " + std::to_string((int)wDoc.size()) + " paragraphs", F16, 270, 124, Col{90, 90, 100}, 0);
    }
    // ---- status bar
    if (!kbOpen) {
        rect(0, UIH - 28, W, 28, blue);
        text("Page 1 of 1", F16, 16, UIH - 24, WHITE);
        text(std::to_string(wWords()) + " words", F16, 160, UIH - 24, WHITE);
        static const char* ZN[4] = {"75%", "90%", "100%", "125%"};
        text(ZN[std::max(0, std::min(3, gDocFont))], F16, W - 16, UIH - 24, WHITE, 2);
    }
    // ---- overlays
    if (wView == 1) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170);
        rect(140, 50, 1000, 560, Col{246, 246, 248});
        text("Insert picture", F30, 170, 64, blue);
        btn(980, 58, 130, 44, "Cancel", Col{150, 150, 160}, WHITE);
        SDL_Rect pc = {140, 116, 1000, 480};
        SDL_RenderSetClipRect(gR, &pc);
        int n = (int)wPics.size();
        wPicScroll = std::max(0, std::min(wPicScroll, std::max(0, n * 50 - 470)));
        for (int i = 0; i < n; i++) {
            int ry = 116 + i * 50 - wPicScroll;
            if (ry + 46 < 116 || ry > 596) continue;
            rect(160, ry, 960, 44, i % 2 ? Col{235, 236, 240} : WHITE);
            std::string nm = wPics[i].substr(wPics[i].find_last_of('/') + 1);
            text(nm, F22, 176, ry + 8, DARK);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (n == 0) text("No pictures found. Draw one in Paint and press Save.", F22, 170, 140, Col{90, 90, 100});
    } else if (wView == 2) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170);
        rect(140, 50, 1000, 560, Col{246, 246, 248});
        text("Insert symbol", F30, 170, 64, blue);
        btn(980, 58, 130, 44, "Cancel", Col{150, 150, 160}, WHITE);
        for (int i = 0; i < 40; i++) {
            int sx = 170 + (i % 10) * 94, sy = 130 + (i / 10) * 112;
            rect(sx, sy, 88, 100, WHITE);
            text(SYMS[i], F44, sx + 44, sy + 24, DARK, 1);
        }
    }
}
static void wordTap(int x, int y) {
    wEnsure();
    if (wView == 1) {
        if (inR(x, y, 980, 58, 130, 44) || !inR(x, y, 140, 50, 1000, 560)) { wView = 0; return; }
        if (y >= 116 && y < 596) {
            int i = (y - 116 + wPicScroll) / 50;
            if (i >= 0 && i < (int)wPics.size()) {
                WPara np; np.style = 4; np.img = wPics[i]; np.align = 1;
                wInsertAfter(np); WPara t; wInsertAfter(t);
                wView = 0; openKb(2);
            }
        }
        return;
    }
    if (wView == 2) {
        if (inR(x, y, 980, 58, 130, 44) || !inR(x, y, 140, 50, 1000, 560)) { wView = 0; return; }
        for (int i = 0; i < 40; i++) {
            int sx = 170 + (i % 10) * 94, sy = 130 + (i / 10) * 112;
            if (inR(x, y, sx, sy, 88, 100)) { wType(SYMS[i]); wView = 0; openKb(2); return; }
        }
        return;
    }
    if (y >= 32 && y < 68) {
        for (int i = 0; i < 6; i++) { SDL_Rect r = wTabRect(i); if (inR(x, y, r.x, r.y, r.w, r.h)) { wTab = i; return; } }
        return;
    }
    if (y >= 68 && y < WTOP) {
        std::vector<WB> bs; std::vector<WG> gs;
        wRibbon(bs, gs);
        for (auto& bt : bs) if (inR(x, y, bt.x, bt.y, bt.w, bt.h)) { wApply(bt.id); return; }
        return;
    }
    if (y >= WTOP && y < (kbOpen ? KBTOP : UIH - 28)) {
        int ry = y - WTOP - 18 + wScroll;
        int hit = -1;
        for (int i = 0; i < (int)wY.size(); i++) if (ry >= wY[i] - 4 && ry < wY[i] + wH[i] + 4) { hit = i; break; }
        if (hit < 0 && !wY.empty() && ry >= wY.back()) hit = (int)wY.size() - 1;
        if (hit >= 0) wCur = hit;
        openKb(2); wFollow = true;
    }
}


// ---------------------------------------------------------------- Excel (UI) - Office 2016 look
static const Col XGREEN = {33, 115, 70};
static const char* XTABS[6] = {"File", "Home", "Insert", "Formulas", "Data", "View"};
static const Col XFILL[7] = {{255, 255, 255}, {255, 242, 153}, {198, 239, 206}, {255, 199, 206}, {189, 215, 238}, {252, 213, 180}, {217, 217, 217}};
static SDL_Rect xTabRect(int i) { return i == 0 ? SDL_Rect{0, 32, 84, 36} : SDL_Rect{100 + (i - 1) * 118, 32, 110, 36}; }
struct XB { int id, x, y, w, h; const char* l; };
static void xRibbon(std::vector<XB>& b, std::vector<WG>& g) {
    if (xTab == 1) {
        g = {{8, 162, "Clipboard"}, {176, 212, "Font"}, {394, 168, "Alignment"}, {568, 180, "Number"}, {754, 232, "Editing"}, {992, 150, "Cells"}};
        b = {{16, 16, 76, 70, 72, "Paste"}, {15, 92, 76, 70, 34, "Cut"}, {14, 92, 114, 70, 34, "Copy"},
             {1, 184, 76, 60, 34, "B"}, {2, 250, 76, 60, 34, "I"}, {3, 316, 76, 66, 72, "Fill"},
             {4, 402, 76, 46, 34, "L"}, {5, 452, 76, 46, 34, "C"}, {6, 502, 76, 46, 34, "R"},
             {7, 576, 76, 164, 34, "General"},
             {8, 762, 76, 120, 72, "AutoSum"}, {9, 888, 76, 90, 34, "Clear"},
             {10, 1000, 76, 134, 34, "Insert row"}, {11, 1000, 114, 134, 34, "Delete row"}};
    } else if (xTab == 2) {
        g = {{8, 270, "Text"}};
        b = {{20, 16, 76, 118, 72, "Date"}, {21, 140, 76, 118, 72, "Time"}};
    } else if (xTab == 3) {
        g = {{8, 640, "Function Library"}};
        b = {{30, 16, 76, 118, 72, "Sum"}, {31, 140, 76, 118, 72, "Average"}, {32, 264, 76, 118, 72, "Count"}, {33, 388, 76, 118, 72, "Max"}, {34, 512, 76, 118, 72, "Min"}};
    } else if (xTab == 4) {
        g = {{8, 290, "Sort"}};
        b = {{40, 16, 76, 130, 72, "Sort A to Z"}, {41, 152, 76, 130, 72, "Sort Z to A"}};
    } else if (xTab == 5) {
        g = {{8, 270, "Show"}};
        b = {{50, 16, 76, 118, 72, "Gridlines"}};
    } else {
        g = {{8, 270, "Workbook"}};
        b = {{60, 16, 76, 118, 72, "New"}, {61, 140, 76, 118, 72, "Save"}};
    }
}
static int xLastRow() { int last = -1; for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) if (!xs[r][c].empty()) last = r; return last; }
static std::string xAutoRange() { // numbers directly above the cell, else directly to the left
    double v; bool er = false;
    int top = xr;
    while (top > 0 && !xs[top - 1][xc].empty() && cellVal(top - 1, xc, 0, v, er)) top--;
    if (top < xr) return xAddr(top, xc) + ":" + xAddr(xr - 1, xc);
    int left = xc;
    while (left > 0 && !xs[xr][left - 1].empty() && cellVal(xr, left - 1, 0, v, er)) left--;
    if (left < xc) return xAddr(xr, left) + ":" + xAddr(xr, xc - 1);
    return xAddr(std::max(0, xr - 1), xc) + ":" + xAddr(std::max(0, xr - 1), xc);
}
static void xSortCol(bool asc) {
    int last = xLastRow();
    if (last < 1) return;
    std::vector<int> idx;
    for (int r = 0; r <= last; r++) idx.push_back(r);
    int col = xc;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        const std::string& A = xs[a][col]; const std::string& B = xs[b][col];
        bool na = looksNumber(A), nb = looksNumber(B);
        bool less;
        if (na && nb) less = strtod(A.c_str(), nullptr) < strtod(B.c_str(), nullptr);
        else if (na != nb) less = na;
        else less = lower(A) < lower(B);
        return asc ? less : (!less && A != B);
    });
    std::vector<std::vector<std::string>> cells(last + 1, std::vector<std::string>(XC));
    std::vector<std::vector<unsigned short>> fm(last + 1, std::vector<unsigned short>(XC));
    for (int r = 0; r <= last; r++) for (int c = 0; c < XC; c++) { cells[r][c] = xs[idx[r]][c]; fm[r][c] = xf[idx[r]][c]; }
    for (int r = 0; r <= last; r++) for (int c = 0; c < XC; c++) { xs[r][c] = cells[r][c]; xf[r][c] = fm[r][c]; }
    xDirty = true;
}
static void xSwitchSheet(int n) {
    commitCell();
    if (xDirty) { xSaveSheet(); xDirty = false; }
    xSheet = std::max(0, std::min(2, n));
    xr = xc = xScrR = xScrC = 0;
}
static void xApply(int id) {
    commitCell();
    unsigned short& f = xf[xr][xc];
    switch (id) {
        case 1: f ^= 1; break;
        case 2: f ^= 2; break;
        case 3: { int fl = (f >> 6) & 7; fl = (fl + 1) % 7; f = (unsigned short)((f & ~(7 << 6)) | (fl << 6)); } break;
        case 4: f = (unsigned short)((f & ~(3 << 2)) | (1 << 2)); break;
        case 5: f = (unsigned short)((f & ~(3 << 2)) | (2 << 2)); break;
        case 6: f = (unsigned short)((f & ~(3 << 2)) | (3 << 2)); break;
        case 7: { int nf = (((f >> 4) & 3) + 1) % 4; f = (unsigned short)((f & ~(3 << 4)) | (nf << 4)); } break;
        case 8: xs[xr][xc] = "=SUM(" + xAutoRange() + ")"; break;
        case 9: xs[xr][xc].clear(); f = 0; break;
        case 10:
            for (int r = XR - 1; r > xr; r--) for (int c = 0; c < XC; c++) { xs[r][c] = xs[r - 1][c]; xf[r][c] = xf[r - 1][c]; }
            for (int c = 0; c < XC; c++) { xs[xr][c].clear(); xf[xr][c] = 0; }
            break;
        case 11:
            for (int r = xr; r < XR - 1; r++) for (int c = 0; c < XC; c++) { xs[r][c] = xs[r + 1][c]; xf[r][c] = xf[r + 1][c]; }
            for (int c = 0; c < XC; c++) { xs[XR - 1][c].clear(); xf[XR - 1][c] = 0; }
            break;
        case 14: xClip = xs[xr][xc]; xClipF = f; return;
        case 15: xClip = xs[xr][xc]; xClipF = f; xs[xr][xc].clear(); f = 0; break;
        case 16: xs[xr][xc] = xClip; f = xClipF; break;
        case 20: xs[xr][xc] = dateStr(); break;
        case 21: xs[xr][xc] = timeStr(); break;
        case 30: xs[xr][xc] = "=SUM(" + xAutoRange() + ")"; break;
        case 31: xs[xr][xc] = "=AVERAGE(" + xAutoRange() + ")"; break;
        case 32: xs[xr][xc] = "=COUNT(" + xAutoRange() + ")"; break;
        case 33: xs[xr][xc] = "=MAX(" + xAutoRange() + ")"; break;
        case 34: xs[xr][xc] = "=MIN(" + xAutoRange() + ")"; break;
        case 40: xSortCol(true); break;
        case 41: xSortCol(false); break;
        case 50: xGrid = !xGrid; return;
        case 60: xSaveSheet(); backupFile(xFile(xSheet, ".csv")); for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) { xs[r][c].clear(); xf[r][c] = 0; } xr = xc = xScrR = xScrC = 0; break;
        case 61: break;
        default: break;
    }
    xDirty = true;
}
static void drawExcel() {
    Col green = XGREEN;
    int gridBottom = kbOpen ? KBTOP : UIH - 28;
    rect(0, 0, W, UIH, WHITE);
    // ---- grid
    int rows = xRows();
    rect(0, 210, W, 26, Col{233, 233, 233});
    rect(0, 210, XHDRW, gridBottom - 210, Col{233, 233, 233});
    for (int vc = 0; vc < XVC; vc++) {
        int c = xScrC + vc;
        if (c >= XC) break;
        bool hs = (c == xc);
        if (hs) rect(XHDRW + vc * XCOLW, 210, XCOLW, 26, Col{210, 228, 216});
        textS(std::string(1, (char)('A' + c)), F22, XHDRW + vc * XCOLW + XCOLW / 2, 212, hs ? green : Col{70, 70, 70}, hs ? 1 : 0, 1);
        rect(XHDRW + vc * XCOLW, 210, 1, 26, Col{200, 200, 200});
    }
    SDL_Rect gclip = {0, XTOP, W, std::max(1, gridBottom - XTOP)};
    SDL_RenderSetClipRect(gR, &gclip);
    for (int vr = 0; vr < rows; vr++) {
        int r = xScrR + vr;
        if (r >= XR) break;
        int y = XTOP + vr * XROWH;
        bool hs = (r == xr);
        rect(0, y, XHDRW, XROWH, hs ? Col{210, 228, 216} : Col{233, 233, 233});
        textS(std::to_string(r + 1), F22, XHDRW / 2, y + 4, hs ? green : Col{70, 70, 70}, hs ? 1 : 0, 1);
        if (xGrid) rect(0, y + XROWH - 1, W, 1, Col{218, 218, 218});
        for (int vc = 0; vc < XVC; vc++) {
            int c = xScrC + vc;
            if (c >= XC) break;
            int x = XHDRW + vc * XCOLW;
            unsigned short f = xf[r][c];
            int fl = (f >> 6) & 7;
            if (fl) rect(x + 1, y + 1, XCOLW - 1, XROWH - 2, XFILL[fl]);
            if (xGrid) rect(x, y, 1, XROWH, Col{218, 218, 218});
            bool isSel = (r == xr && c == xc);
            std::string d = (isSel && xEditing) ? xEdit : xShown(r, c);
            SDL_Rect cr = {x + 1, y + 1, XCOLW - 1, XROWH - 1};
            SDL_RenderSetClipRect(gR, &cr);
            if (isSel && xEditing) rect(cr.x, cr.y, cr.w, cr.h, WHITE);
            int st = (f & 1 ? 1 : 0) | (f & 2 ? 2 : 0);
            int al = (f >> 2) & 3;
            bool num = looksNumber(cellDisplay(r, c)) || (!d.empty() && (d[0] == '$' || d.back() == '%') && looksNumber(cellDisplay(r, c)));
            if (isSel && xEditing) al = 1;
            if (al == 0) al = num ? 3 : 1;
            if (al == 1) textS(d, F22, x + 6, y + 3, DARK, st);
            else if (al == 2) textS(d, F22, x + XCOLW / 2, y + 3, DARK, st, 1);
            else textS(d, F22, x + XCOLW - 6, y + 3, DARK, st, 2);
            SDL_RenderSetClipRect(gR, &gclip);
        }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (xr >= xScrR && xr < xScrR + rows && xc >= xScrC && xc < xScrC + XVC) {
        int sx = XHDRW + (xc - xScrC) * XCOLW, sy = XTOP + (xr - xScrR) * XROWH;
        SDL_Rect sc = {0, XTOP, W, std::max(1, gridBottom - XTOP)};
        SDL_RenderSetClipRect(gR, &sc);
        outline(sx, sy, XCOLW + 1, XROWH + 1, 3, green);
        rect(sx + XCOLW - 5, sy + XROWH - 5, 9, 9, green);
        SDL_RenderSetClipRect(gR, nullptr);
    }
    // ---- title bar, tabs, ribbon
    rect(0, 0, W, 68, green);
    text("Book1 - Excel", F22, W / 2, 4, WHITE, 1);
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = xTabRect(i);
        if (i == 0) { rect(r.x, r.y, r.w, r.h, Col{24, 86, 52}); text(XTABS[i], F22, r.x + r.w / 2, r.y + 4, WHITE, 1); }
        else if (xTab == i) { rect(r.x, r.y + 2, r.w, r.h - 2, Col{246, 246, 248}); text(XTABS[i], F22, r.x + r.w / 2, r.y + 4, green, 1); }
        else text(XTABS[i], F22, r.x + r.w / 2, r.y + 4, WHITE, 1);
    }
    rect(0, 68, W, 110, Col{246, 246, 248});
    std::vector<XB> bs; std::vector<WG> gs;
    xRibbon(bs, gs);
    for (auto& gp : gs) {
        text(gp.name, F16, gp.x + gp.w / 2, 154, Col{100, 100, 110}, 1);
        rect(gp.x + gp.w + 2, 76, 1, 92, Col{210, 210, 216});
    }
    unsigned short cf = xf[xr][xc];
    for (auto& bt : bs) {
        bool on = false;
        std::string lab = bt.l;
        switch (bt.id) {
            case 1: on = cf & 1; break; case 2: on = cf & 2; break;
            case 4: on = ((cf >> 2) & 3) == 1; break; case 5: on = ((cf >> 2) & 3) == 2; break; case 6: on = ((cf >> 2) & 3) == 3; break;
            case 7: { static const char* NF[4] = {"General", "Number 0.00", "Currency $", "Percentage %"}; lab = NF[(cf >> 4) & 3]; } break;
            case 50: on = xGrid; break;
            default: break;
        }
        rect(bt.x, bt.y, bt.w, bt.h, on ? Col{198, 232, 210} : Col{236, 238, 243});
        if (on) outline(bt.x, bt.y, bt.w, bt.h, 2, Col{90, 170, 120});
        if (bt.id == 3) { rect(bt.x + 18, bt.y + 10, bt.w - 36, 26, XFILL[(cf >> 6) & 7]); outline(bt.x + 18, bt.y + 10, bt.w - 36, 26, 1, Col{150, 150, 160}); text("Fill", F22, bt.x + bt.w / 2, bt.y + 40, DARK, 1); continue; }
        int st = bt.id == 1 ? 1 : (bt.id == 2 ? 2 : 0);
        bool big = bt.h > 40;
        textS(lab, F22, bt.x + bt.w / 2, bt.y + (big ? bt.h / 2 - 15 : 3), DARK, st, 1);
    }
    if (xTab == 0) text("Autosaved to switch/WinNX/sheet1.csv, sheet2.csv, sheet3.csv", F16, 270, 100, Col{90, 90, 100});
    if (xTab == 4 || xTab == 3) text("Uses the selected column / the numbers above or left of the cell", F16, 680, 100, Col{110, 110, 120});
    rect(0, 177, W, 1, Col{200, 200, 206});
    // ---- formula bar
    rect(0, 178, W, 32, Col{250, 250, 250});
    rect(6, 182, 96, 24, WHITE); outline(6, 182, 96, 24, 1, Col{190, 190, 196});
    text(xAddr(xr, xc), F16, 54, 184, DARK, 1);
    rect(110, 182, 36, 24, Col{238, 238, 238}); textS("fx", F16, 128, 184, green, 2, 1);
    rect(152, 182, W - 160, 24, WHITE); outline(152, 182, W - 160, 24, 1, Col{190, 190, 196});
    std::string fb = xEditing ? xEdit : xs[xr][xc];
    if (xEditing && ((SDL_GetTicks() / 500) % 2 == 0)) fb += "|";
    text(fb, F16, 160, 184, DARK);
    // ---- sheet tabs + status bar
    if (!kbOpen) {
        rect(0, UIH - 28, W, 28, Col{240, 240, 240});
        rect(0, UIH - 28, W, 1, Col{200, 200, 200});
        for (int i = 0; i < 3; i++) {
            int x = 8 + i * 104;
            bool a = (i == xSheet);
            rect(x, UIH - 28, 100, 28, a ? WHITE : Col{225, 225, 225});
            if (a) rect(x, UIH - 3, 100, 3, green);
            textS("Sheet" + std::to_string(i + 1), F16, x + 50, UIH - 25, a ? green : Col{80, 80, 80}, a ? 1 : 0, 1);
        }
        text("Ready", F16, W - 16, UIH - 25, Col{80, 80, 80}, 2);
    }
}
static void excelTap(int x, int y) {
    if (y >= 32 && y < 68) {
        for (int i = 0; i < 6; i++) { SDL_Rect r = xTabRect(i); if (inR(x, y, r.x, r.y, r.w, r.h)) { commitCell(); xTab = i; return; } }
        return;
    }
    if (y >= 68 && y < 178) {
        std::vector<XB> bs; std::vector<WG> gs;
        xRibbon(bs, gs);
        for (auto& bt : bs) if (inR(x, y, bt.x, bt.y, bt.w, bt.h)) { xApply(bt.id); return; }
        return;
    }
    if (y >= 178 && y < 210) { openKb(3); startEdit(); return; }
    if (!kbOpen && y >= UIH - 28) {
        for (int i = 0; i < 3; i++) if (inR(x, y, 8 + i * 104, UIH - 28, 100, 28)) { xSwitchSheet(i); return; }
        return;
    }
    if (x >= XHDRW && y >= XTOP) {
        int vc = (x - XHDRW) / XCOLW, vr = (y - XTOP) / XROWH;
        int r = xScrR + vr, c = xScrC + vc;
        if (vr < xRows() && vc < XVC && r < XR && c < XC) {
            if (r == xr && c == xc) { openKb(3); startEdit(); }
            else { commitCell(); xr = r; xc = c; xEnsureVisible(); }
        }
    }
}

// ---------------------------------------------------------------- 2048 / Minesweeper / Snake (UI)
static Col g2Col(int v) {
    switch (v) {
        case 0: return Col{205, 193, 180}; case 2: return Col{238, 228, 218}; case 4: return Col{237, 224, 200};
        case 8: return Col{242, 177, 121}; case 16: return Col{245, 149, 99}; case 32: return Col{246, 124, 95};
        case 64: return Col{246, 94, 59}; case 128: return Col{237, 207, 114}; case 256: return Col{237, 204, 97};
        case 512: return Col{237, 200, 80}; case 1024: return Col{237, 197, 63}; default: return Col{237, 194, 46};
    }
}
static void draw2048() {
    rect(0, 0, W, UIH, Col{250, 248, 239});
    text("2048", F72, 70, 50, Col{119, 110, 101});
    rect(70, 170, 220, 90, Col{187, 173, 160}); text("SCORE", F16, 180, 180, Col{238, 228, 218}, 1); text(std::to_string(g2score), F44, 180, 204, WHITE, 1);
    rect(70, 280, 220, 90, Col{187, 173, 160}); text("BEST", F16, 180, 290, Col{238, 228, 218}, 1); text(std::to_string(g2best), F44, 180, 314, WHITE, 1);
    btn(70, 400, 220, 60, "New game", Col{143, 122, 102}, WHITE);
    text("Swipe on the board", F16, 70, 480, Col{150, 140, 130});
    text("or use the D-pad", F16, 70, 504, Col{150, 140, 130});
    int bx = 370, by = 100;
    rect(bx, by, 540, 540, Col{187, 173, 160});
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        int v = g2[r][c], x = bx + 12 + c * 132, y = by + 12 + r * 132;
        rect(x, y, 120, 120, g2Col(v));
        if (v) text(std::to_string(v), v >= 1000 ? F30 : F44, x + 60, y + (v >= 1000 ? 42 : 36), v <= 4 ? Col{119, 110, 101} : WHITE, 1);
    }
    if (g2over) { rect(bx, by, 540, 540, Col{238, 228, 218}, 190); text("Game over", F72, bx + 270, by + 220, Col{119, 110, 101}, 1); }
}
static void tap2048(int x, int y) { if (inR(x, y, 70, 400, 220, 60)) g2New(); }
static int swipeOf(int x, int y, int minD) {
    int dx = x - swX, dy = y - swY;
    if (abs(dx) < minD && abs(dy) < minD) return -1;
    if (abs(dx) > abs(dy)) return dx < 0 ? 0 : 1;
    return dy < 0 ? 2 : 3;
}
static Col mNumCol(int n) {
    static const Col C[9] = {{0, 0, 0}, {80, 150, 255}, {70, 200, 110}, {240, 90, 90}, {120, 120, 255}, {220, 110, 70}, {60, 200, 200}, {240, 240, 240}, {180, 180, 180}};
    return C[n];
}
static void drawMines() {
    rect(0, 0, W, UIH, Col{44, 48, 60});
    char b[48];
    snprintf(b, sizeof b, "Mines  %d", MMINES - mFlags());
    text(b, F30, MX0, 52, WHITE);
    snprintf(b, sizeof b, "%d s", (int)mTime);
    text(b, F30, W / 2, 52, WHITE, 1);
    btn(830, 46, 150, 48, mFlagMode ? "Flag" : "Dig", mFlagMode ? Col{200, 120, 20} : Col{40, 130, 190}, WHITE);
    btn(990, 46, 130, 48, "New", Col{90, 100, 125}, WHITE);
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) {
        const MCell& m = mc[r][c];
        int x = MX0 + c * MCELL, y = MY0 + r * MCELL;
        if (m.open) {
            rect(x, y, MCELL - 2, MCELL - 2, m.mine ? Col{170, 40, 40} : Col{28, 31, 40});
            if (m.mine) disc(x + 29, y + 29, 12, WHITE);
            else if (m.n) text(std::to_string(m.n), F30, x + 29, y + 12, mNumCol(m.n), 1);
        } else {
            rect(x, y, MCELL - 2, MCELL - 2, Col{104, 116, 146});
            rect(x, y, MCELL - 2, 4, Col{140, 152, 182});
            if (m.flag) { rect(x + 28, y + 12, 4, 30, WHITE); tri((float)x + 30, y + 12.0f, x + 30.0f, y + 28.0f, x + 46.0f, y + 20.0f, Col{240, 80, 70}); }
        }
    }
    if (mState >= 2) {
        rect(MX0, MY0 + 200, MCOLS * MCELL, 100, Col{0, 0, 0}, 170);
        text(mState == 2 ? "You win!" : "Boom! Game over", F72, W / 2, MY0 + 206, mState == 2 ? Col{120, 230, 140} : Col{240, 110, 100}, 1);
    }
}
static void tapMines(int x, int y) {
    if (inR(x, y, 830, 46, 150, 48)) { mFlagMode = !mFlagMode; return; }
    if (inR(x, y, 990, 46, 130, 48)) { mNew(); return; }
    int c = (x - MX0) / MCELL, r = (y - MY0) / MCELL;
    if (x < MX0 || y < MY0 || c >= MCOLS || r >= MROWS) return;
    if (mState >= 2) return;
    if (mFlagMode) { if (!mc[r][c].open) mc[r][c].flag = !mc[r][c].flag; }
    else if (mc[r][c].open && mc[r][c].n > 0) { // chord: dig neighbours if flags match
        int f = 0;
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { int a = r + dr, b = c + dc; if (a >= 0 && b >= 0 && a < MROWS && b < MCOLS && mc[a][b].flag) f++; }
        if (f == mc[r][c].n)
            for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { int a = r + dr, b = c + dc; if (a >= 0 && b >= 0 && a < MROWS && b < MCOLS) mDig(a, b); }
    } else mDig(r, c);
}
static void drawSnake() {
    rect(0, 0, W, UIH, Col{24, 34, 28});
    text("Score " + std::to_string(sScore), F30, 100, 40, WHITE);
    text("Best " + std::to_string(sBest), F30, 400, 40, WHITE, 0, 200);
    btn(1060, 38, 120, 44, "New", Col{60, 150, 80}, WHITE);
    rect(SX0, SY0, SCOLS * SCELL, SROWS * SCELL, Col{34, 48, 38});
    for (size_t i = 0; i < snake.size(); i++) {
        int x = SX0 + snake[i].first * SCELL, y = SY0 + snake[i].second * SCELL;
        rect(x + 2, y + 2, SCELL - 4, SCELL - 4, i == 0 ? Col{150, 240, 120} : Col{80, 190, 90});
    }
    disc(SX0 + sFx * SCELL + SCELL / 2, SY0 + sFy * SCELL + SCELL / 2, SCELL / 2 - 4, Col{235, 70, 70});
    outline(SX0 - 2, SY0 - 2, SCOLS * SCELL + 4, SROWS * SCELL + 4, 2, Col{90, 140, 100});
    if (sState == 0) text("Swipe to start", F44, W / 2, SY0 + 230, WHITE, 1);
    if (sState == 2) { rect(SX0, SY0 + 200, SCOLS * SCELL, 120, Col{0, 0, 0}, 160); text("Game over", F72, W / 2, SY0 + 205, Col{240, 110, 100}, 1); }
}
static void tapSnake(int x, int y) { if (inR(x, y, 1060, 38, 120, 44)) snakeNew(); }

// ---------------------------------------------------------------- Solitaire (UI)
static void drawSuit(int su, int cx, int cy, int z, Col c) {
    switch (su) {
        case 2:
            disc(cx - z / 2, cy - z / 4, z / 2 + 1, c); disc(cx + z / 2, cy - z / 4, z / 2 + 1, c);
            tri((float)cx - z - 1, (float)cy - z / 8, (float)cx + z + 1, (float)cy - z / 8, (float)cx, (float)cy + z, c); break;
        case 3: quad((float)cx, (float)cy - z, cx + z * 0.72f, (float)cy, (float)cx, (float)cy + z, cx - z * 0.72f, (float)cy, c); break;
        case 0:
            tri((float)cx - z - 1, (float)cy + z / 4, (float)cx + z + 1, (float)cy + z / 4, (float)cx, (float)cy - z, c);
            disc(cx - z / 2, cy + z / 4, z / 2 + 1, c); disc(cx + z / 2, cy + z / 4, z / 2 + 1, c);
            tri((float)cx, (float)cy, cx - z / 3.0f, (float)cy + z, cx + z / 3.0f, (float)cy + z, c); break;
        default:
            disc(cx, cy - z / 2, z / 2 + 1, c); disc(cx - z / 2, cy + z / 5, z / 2 + 1, c); disc(cx + z / 2, cy + z / 5, z / 2 + 1, c);
            tri((float)cx, (float)cy, cx - z / 3.0f, (float)cy + z, cx + z / 3.0f, (float)cy + z, c); break;
    }
}
static std::string solRank(int r) {
    switch (r) { case 1: return "A"; case 11: return "J"; case 12: return "Q"; case 13: return "K"; default: break; }
    return std::to_string(r);
}
static void drawCard(int x, int y, const SCard& c) {
    rect(x + 2, y + 3, SOLCW, SOLCH, BLACK, 60);
    if (!c.up) {
        rect(x, y, SOLCW, SOLCH, WHITE);
        rect(x + 6, y + 6, SOLCW - 12, SOLCH - 12, Col{34, 70, 160});
        for (int i = 0; i < 9; i++) { line(x + 6, y + 6 + i * 18, x + 6 + std::min(SOLCW - 12, (i + 1) * 18), y + 6, 1, Col{90, 130, 220}, 160); }
        outline(x + 6, y + 6, SOLCW - 12, SOLCH - 12, 2, Col{150, 180, 240});
        return;
    }
    rect(x, y, SOLCW, SOLCH, Col{252, 252, 248});
    outline(x, y, SOLCW, SOLCH, 1, Col{170, 170, 170});
    Col ink = solRed(c) ? Col{200, 30, 40} : Col{30, 30, 30};
    textS(solRank(c.r), F30, x + 8, y + 2, ink, 1);
    drawSuit(c.s, x + 20, y + 52, 9, ink);
    if (c.r >= 11) {
        text(solRank(c.r), F72, x + SOLCW / 2 + 10, y + 40, ink, 1, 230);
        drawSuit(c.s, x + SOLCW - 24, y + SOLCH - 26, 10, ink);
    } else drawSuit(c.s, x + SOLCW / 2 + 6, y + SOLCH / 2 + 10, c.r == 1 ? 28 : 20, ink);
}
// fan-out offsets for a tableau column so even long columns fit on screen
static void solOffsets(int col, std::vector<int>& off) {
    const std::vector<SCard>& t = sol.tab[col];
    off.assign(t.size(), 0);
    float down = 18, up = 42, total = 0;
    for (size_t i = 0; i + 1 < t.size(); i++) total += t[i].up ? up : down;
    float avail = (float)(UIH - SOLTY - SOLCH - 8);
    float k = (total > avail && total > 0) ? avail / total : 1.0f;
    int y = 0;
    for (size_t i = 0; i < t.size(); i++) { off[i] = y; y += (int)((t[i].up ? up : down) * k); }
}
static int solColX(int c) { return SOLX0 + c * SOLDX; }
static int solSrc = -1, solSCol = 0, solSIdx = 0, solDX = 0, solDY = 0, solGX = 0, solGY = 0, solPX = 0, solPY = 0;
static bool solHit(int x, int y, int& src, int& col, int& idx) {
    // waste
    if (!sol.waste.empty() && inR(x, y, solColX(1), SOLY0, SOLCW, SOLCH)) { src = 0; col = 0; idx = (int)sol.waste.size() - 1; return true; }
    for (int f = 0; f < 4; f++) if (!sol.found[f].empty() && inR(x, y, solColX(3 + f), SOLY0, SOLCW, SOLCH)) { src = 2; col = f; idx = (int)sol.found[f].size() - 1; return true; }
    for (int c = 0; c < 7; c++) {
        std::vector<int> off; solOffsets(c, off);
        for (int i = (int)off.size() - 1; i >= 0; i--) {
            int h = (i == (int)off.size() - 1) ? SOLCH : std::max(18, (i + 1 < (int)off.size() ? off[i + 1] - off[i] : SOLCH));
            if (inR(x, y, solColX(c), SOLTY + off[i], SOLCW, h)) { src = 1; col = c; idx = i; return true; }
        }
    }
    return false;
}
static void solTouch(int ph, int x, int y) {
    if (ph == 0) {
        solPressed = true; solDragging = false; solSrc = -1; solPX = x; solPY = y;
        if (x >= 1150) return;
        int s2, c2, i2;
        if (!solWon && solHit(x, y, s2, c2, i2)) {
            bool up = (s2 == 0) ? sol.waste.back().up : (s2 == 2 ? true : sol.tab[c2][i2].up);
            if (up) { solSrc = s2; solSCol = c2; solSIdx = i2; solGX = x - (s2 == 0 ? solColX(1) : (s2 == 2 ? solColX(3 + c2) : solColX(c2))); solGY = 0; }
            else solSrc = 100 + c2; // face-down card: tap flips if it is the top one
            if (solSrc < 100) {
                int top = (s2 == 0) ? SOLY0 : (s2 == 2 ? SOLY0 : 0);
                if (s2 == 1) { std::vector<int> off; solOffsets(c2, off); top = SOLTY + off[i2]; }
                solGY = y - top;
            }
        }
    } else if (ph == 1 && solPressed && solSrc >= 0 && solSrc < 100) {
        if (!solDragging && (abs(x - solPX) > 14 || abs(y - solPY) > 14)) solDragging = true;
        solDX = x; solDY = y;
    } else if (ph == 2 && solPressed) {
        solPressed = false;
        bool moved = abs(x - solPX) > 14 || abs(y - solPY) > 14;
        if (solDragging && solSrc >= 0 && solSrc < 100) {
            solDragging = false;
            int cx0 = x - solGX + SOLCW / 2, cy0 = y - solGY;
            // drop on foundation?
            bool done = false;
            if (cy0 < SOLTY - 20) {
                for (int f = 0; f < 4 && !done; f++) if (cx0 >= solColX(3 + f) - 40 && cx0 < solColX(3 + f) + SOLCW + 40) done = solMove(solSrc, solSCol, solSIdx, 2, f);
            } else {
                int best = -1, bd = 99999;
                for (int c = 0; c < 7; c++) { int d = abs(cx0 - (solColX(c) + SOLCW / 2)); if (d < bd) { bd = d; best = c; } }
                if (best >= 0 && bd < SOLCW) done = solMove(solSrc, solSCol, solSIdx, 1, best);
            }
            (void)done;
            return;
        }
        solDragging = false;
        if (moved) return;
        // ---- taps
        if (x >= 1150) {
            if (inR(x, y, 1152, 60, 118, 48)) { solNew(); }
            else if (inR(x, y, 1152, 120, 118, 48)) { solUndoMove(); }
            else if (inR(x, y, 1152, 180, 118, 48)) { solDraw = (solDraw == 1) ? 3 : 1; solDirty = true; }
            return;
        }
        if (solWon) { if (inR(x, y, 440, 300, 400, 80)) solNew(); return; }
        if (inR(x, y, solColX(0), SOLY0, SOLCW, SOLCH)) { solStockTap(); return; }
        if (solSrc >= 100) {
            int c = solSrc - 100;
            if (!sol.tab[c].empty() && !sol.tab[c].back().up && solHit(x, y, solSrc, solSCol, solSIdx) && solSIdx == (int)sol.tab[c].size() - 1) {
                solPush(); solFlipTop(c); sol.moves++; solDirty = true;
            }
            return;
        }
        if (solSrc >= 0) solAuto(solSrc, solSCol, solSIdx);
    } else if (ph == 2) { solPressed = false; solDragging = false; }
}
static void drawSolitaire() {
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(Col{24, 124, 70}, Col{12, 82, 46}, y / (float)UIH));
    // empty slots
    rect(solColX(0), SOLY0, SOLCW, SOLCH, WHITE, 24); outline(solColX(0), SOLY0, SOLCW, SOLCH, 2, Col{255, 255, 255});
    rect(solColX(1), SOLY0, SOLCW, SOLCH, WHITE, 16);
    for (int f = 0; f < 4; f++) {
        rect(solColX(3 + f), SOLY0, SOLCW, SOLCH, WHITE, 24);
        drawSuit(f, solColX(3 + f) + SOLCW / 2, SOLY0 + SOLCH / 2, 24, Col{255, 255, 255});
        rect(solColX(3 + f) + 8, SOLY0 + 8, SOLCW - 16, SOLCH - 16, Col{12, 82, 46}, 70);
    }
    for (int c = 0; c < 7; c++) rect(solColX(c), SOLTY, SOLCW, SOLCH, BLACK, 40);
    if (sol.stock.empty()) { ring(solColX(0) + SOLCW / 2, SOLY0 + SOLCH / 2, 26, 6, Col{255, 255, 255}, Col{20, 100, 58}); }
    else drawCard(solColX(0), SOLY0, sol.stock.back());
    if (!sol.waste.empty()) {
        int n = (solDraw == 3) ? std::min<int>(3, sol.waste.size()) : 1;
        for (int i = 0; i < n; i++) {
            int idx = (int)sol.waste.size() - n + i;
            bool dragThis = solDragging && solSrc == 0 && idx == (int)sol.waste.size() - 1;
            if (!dragThis) drawCard(solColX(1) + i * 26, SOLY0, sol.waste[idx]);
        }
    }
    for (int f = 0; f < 4; f++) {
        const std::vector<SCard>& fp = sol.found[f];
        if (fp.empty()) continue;
        bool dragThis = solDragging && solSrc == 2 && solSCol == f;
        if (fp.size() >= 2 && dragThis) drawCard(solColX(3 + f), SOLY0, fp[fp.size() - 2]);
        else if (!dragThis) drawCard(solColX(3 + f), SOLY0, fp.back());
    }
    for (int c = 0; c < 7; c++) {
        std::vector<int> off; solOffsets(c, off);
        for (int i = 0; i < (int)off.size(); i++) {
            if (solDragging && solSrc == 1 && solSCol == c && i >= solSIdx) break;
            drawCard(solColX(c), SOLTY + off[i], sol.tab[c][i]);
        }
    }
    // dragged cards follow the finger
    if (solDragging && solSrc >= 0 && solSrc < 100) {
        int bx = solDX - solGX, by = solDY - solGY;
        if (solSrc == 0) drawCard(bx, by, sol.waste.back());
        else if (solSrc == 2) drawCard(bx, by, sol.found[solSCol].back());
        else {
            std::vector<int> off; solOffsets(solSCol, off);
            for (int i = solSIdx; i < (int)sol.tab[solSCol].size(); i++) drawCard(bx, by + (off[i] - off[solSIdx]), sol.tab[solSCol][i]);
        }
    }
    // side panel
    btn(1152, 60, 118, 48, "New", Col{14, 90, 50}, WHITE);
    btn(1152, 120, 118, 48, "Undo", solUndo.empty() ? Col{40, 110, 70} : Col{14, 90, 50}, WHITE);
    btn(1152, 180, 118, 48, solDraw == 1 ? "Draw 1" : "Draw 3", Col{14, 90, 50}, WHITE);
    text("Score", F16, 1211, 256, WHITE, 1, 200); text(std::to_string(sol.score), F30, 1211, 276, WHITE, 1);
    text("Moves", F16, 1211, 320, WHITE, 1, 200); text(std::to_string(sol.moves), F30, 1211, 340, WHITE, 1);
    if (solWon) {
        float t = (SDL_GetTicks() - solStartT) / 1000.0f;
        for (int i = 0; i < 40; i++) {
            float ph = fmodf(t * (0.4f + (i % 5) * 0.15f) + i * 0.137f, 1.0f);
            Col cc = (i % 3 == 0) ? Col{255, 210, 60} : (i % 3 == 1) ? Col{255, 255, 255} : Col{120, 220, 150};
            rect((i * 97) % (W - 20), (int)(ph * UIH), 12, 12, cc, 220);
        }
        rect(340, 250, 600, 180, BLACK, 170);
        text("You win!", F72, W / 2, 262, WHITE, 1);
        btn(440, 350, 400, 60, "Play again", Col{20, 140, 80}, WHITE);
    }
}

// ---------------------------------------------------------------- Photos gallery (UI)
static const int PH_X0 = 39, PH_Y0 = 156, PH_W = 192, PH_H = 108, PH_GAP = 10, PH_COLS = 6;
static SDL_Texture* phThumbFor(int i) {
    auto it = phThumb.find(i);
    if (it != phThumb.end()) return it->second;
    if (phBudget <= 0 || i < 0 || i >= (int)gPhotos.size()) return nullptr;
    phBudget--;
    if (phThumb.size() > 90) {   // keep memory bounded: drop thumbnails far from the viewport
        int keep = phScroll / (PH_H + PH_GAP) * PH_COLS;
        for (auto t = phThumb.begin(); t != phThumb.end();) {
            if (std::abs(t->first - keep) > 60) { if (t->second) SDL_DestroyTexture(t->second); t = phThumb.erase(t); } else ++t;
        }
    }
    SDL_Texture* tex = nullptr;
    SDL_Surface* sf = IMG_Load(gPhotos[i].c_str());
    if (sf) {
        int tw2 = 384, th2 = 216;
        SDL_Surface* dst = SDL_CreateRGBSurfaceWithFormat(0, tw2, th2, 32, SDL_PIXELFORMAT_RGBA32);
        if (dst) {
            SDL_FillRect(dst, nullptr, SDL_MapRGBA(dst->format, 20, 20, 20, 255));
            float sc = std::max((float)tw2 / sf->w, (float)th2 / sf->h);       // cover-crop
            int sw = (int)(tw2 / sc), sh = (int)(th2 / sc);
            SDL_Rect src = {(sf->w - sw) / 2, (sf->h - sh) / 2, sw, sh};
            SDL_SetSurfaceBlendMode(sf, SDL_BLENDMODE_NONE);
            SDL_BlitScaled(sf, &src, dst, nullptr);
            tex = SDL_CreateTextureFromSurface(gR, dst);
            SDL_FreeSurface(dst);
        }
        SDL_FreeSurface(sf);
    }
    phThumb[i] = tex;
    return tex;
}
static std::vector<std::string> phChips() {
    std::vector<std::string> v = {"All", "Screenshots", "Pictures", "Other", "Albums"};
    if (phChip == 5) { size_t s = phAlbum.find_last_of('/'); v.push_back(s == std::string::npos ? phAlbum : phAlbum.substr(s + 1)); }
    return v;
}
static std::vector<std::pair<std::string, int>> phAlbums() {
    std::map<std::string, int> m;
    for (auto& g : gAllPics) { size_t s = g.path.find_last_of('/'); m[g.path.substr(0, s)]++; }
    std::vector<std::pair<std::string, int>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { return a.second > b.second; });
    return v;
}
static void phSetPic(bool lock) {
    if (gPhotos.empty()) return;
    bool ok;
    if (lock) { ok = savePicCover(gPhotos[phIdx], DIR_ + "lockscreen.png", W, UIH); if (ok) { remove((DIR_ + "lockscreen.jpg").c_str()); lockRebuild(); } }
    else { ok = savePicCover(gPhotos[phIdx], DIR_ + "startbg.png", W + 240, UIH); if (ok) { gStartPic = true; startPicLoad(); saveCfg(); } }
    toast(ok ? (lock ? "Set as your lock screen picture" : "Set as your Start background") : "Couldn't use this picture");
}
static void phClamp() {
    if (!phTex) return;
    float fit = std::min((float)W / phW, (float)UIH / phH) * phZ;
    float mx = std::max(0.0f, (phW * fit - W) / 2), my = std::max(0.0f, (phH * fit - UIH) / 2);
    phOX = std::max(-mx, std::min(mx, phOX)); phOY = std::max(-my, std::min(my, phOY));
}
static void photosPinch(float sc, int cx, int cy, bool begin) {
    (void)cx; (void)cy;
    if (phView != 1) return;
    static float z0 = 1;
    if (begin) { z0 = phZ; return; }
    phZ = std::max(1.0f, std::min(8.0f, z0 * sc));
    if (phZ < 1.02f) { phZ = 1; phOX = phOY = 0; }
    phClamp();
}
static void photosUpdate(float dt) {
    if (curApp != A_PHOTOS || phView != 1 || !phSlide || phZ > 1.02f) return;
    phSlideT += dt;
    if (phSlideT > 4.0f) { phSlideT = 0; photoNav(1); }
}
static void drawPhotos() {
    rect(0, 0, W, UIH, BLACK);
    if (phView == 1) {
        if (phTex) {
            float sc = std::min((float)W / phW, (float)UIH / phH) * phZ;
            int dw = (int)(phW * sc), dh = (int)(phH * sc);
            SDL_Rect d = {(int)(W / 2 + phOX - dw / 2), (int)(UIH / 2 + phOY - dh / 2), dw, dh};
            SDL_RenderCopy(gR, phTex, nullptr, &d);
        } else text("Couldn't open this picture", F30, W / 2, 300, WHITE, 1);
        if (phOverlay) {
            rect(0, 0, W, 90, BLACK, 140); rect(0, UIH - 78, W, 78, BLACK, 160);
            ring(60, 56, 24, 3, WHITE, Col{20, 20, 20});
            line(70, 56, 50, 56, 3, WHITE); line(50, 56, 58, 48, 3, WHITE); line(50, 56, 58, 64, 3, WHITE);
            size_t sl = gPhotos[phIdx].find_last_of('/');
            text(gPhotos[phIdx].substr(sl + 1), F22, 110, 44, WHITE);
            text(std::to_string(phIdx + 1) + " / " + std::to_string(gPhotos.size()) + (phZ > 1.02f ? "   " + std::to_string((int)(phZ * 100)) + "%" : ""), F22, W - 30, 44, WHITE, 2, 220);
            const char* bl[5] = {phSlide ? "Pause" : "Slideshow", "Lock screen", "Start bg", "Account pic", phDelArm > 0 ? "Tap again" : "Delete"};
            for (int i = 0; i < 5; i++) {
                int x = 30 + i * 204;
                rect(x, UIH - 66, 196, 54, i == 4 && phDelArm > 0 ? Col{190, 40, 60} : Col{255, 255, 255}, i == 4 && phDelArm > 0 ? 255 : 40);
                text(bl[i], F22, x + 98, UIH - 52, WHITE, 1);
            }
        }
        return;
    }
    rect(0, 0, W, UIH, Col{28, 28, 32});
    text("Photos", F72, 56, 14, WHITE);
    text(std::to_string(gPhotos.size()) + " pictures", F22, W - 50, 60, WHITE, 2, 170);
    auto ch = phChips();
    int cx = 60;
    for (int i = 0; i < (int)ch.size(); i++) {
        int w = tw(ch[i], F22) + 36;
        rect(cx, 108, w, 38, i == phChip ? Col{0, 150, 160} : Col{60, 60, 66});
        text(ch[i], F22, cx + w / 2, 113, WHITE, 1);
        cx += w + 8;
    }
    if (phChip == 4) {
        auto al = phAlbums();
        int rows = ((int)al.size() + 2) / 3;
        int maxS = std::max(0, rows * 82 - (UIH - PH_Y0) + 10);
        phScroll = std::max(0, std::min(phScroll, maxS));
        SDL_Rect clip = {0, PH_Y0, W, UIH - PH_Y0}; SDL_RenderSetClipRect(gR, &clip);
        for (int i = 0; i < (int)al.size(); i++) {
            int x = 40 + (i % 3) * 400, y = PH_Y0 + (i / 3) * 82 - phScroll;
            if (y + 74 < PH_Y0 || y > UIH) continue;
            rect(x, y, 390, 74, Col{50, 50, 58}); rect(x, y, 8, 74, Col{0, 150, 160});
            size_t s = al[i].first.find_last_of('/');
            std::string nm = s == std::string::npos ? al[i].first : al[i].first.substr(s + 1);
            if (nm.empty() || nm == "sdmc:") nm = "SD card";
            while (nm.size() > 3 && tw(nm, F22) > 330) nm.pop_back();
            text(nm, F22, x + 24, y + 10, WHITE);
            text(std::to_string(al[i].second) + " pictures", F16, x + 24, y + 42, WHITE, 0, 170);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        return;
    }
    int rows = ((int)gPhotos.size() + PH_COLS - 1) / PH_COLS;
    int maxS = std::max(0, rows * (PH_H + PH_GAP) - (UIH - PH_Y0) + 10);
    phScroll = std::max(0, std::min(phScroll, maxS));
    phBudget = 2;
    SDL_Rect clip = {0, PH_Y0, W, UIH - PH_Y0};
    SDL_RenderSetClipRect(gR, &clip);
    int first = std::max(0, phScroll / (PH_H + PH_GAP)) * PH_COLS;
    for (int i = first; i < (int)gPhotos.size(); i++) {
        int r = i / PH_COLS, c = i % PH_COLS;
        int x = PH_X0 + c * (PH_W + PH_GAP), y = PH_Y0 + r * (PH_H + PH_GAP) - phScroll;
        if (y > UIH) break;
        if (y + PH_H < PH_Y0) continue;
        SDL_Texture* t = phThumbFor(i);
        if (t) { SDL_Rect d = {x, y, PH_W, PH_H}; SDL_RenderCopy(gR, t, nullptr, &d); }
        else rect(x, y, PH_W, PH_H, Col{50, 50, 56});
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (gPhotos.empty()) {
        text("No pictures found", F44, W / 2, 260, WHITE, 1);
        text("Windows 8.1 looks through your whole SD card for jpg, png, bmp, gif, webp, tga and tif files.", F22, W / 2, 330, WHITE, 1, 170);
        text("(Switch screenshots live in Nintendo/Album.)", F22, W / 2, 364, WHITE, 1, 170);
    }
}
static void photoTap(int x, int y) {
    if (phView == 1) {
        Uint32 now = SDL_GetTicks();
        bool dbl = now - phTapT < 380 && abs(x - phTapX) < 40 && abs(y - phTapY) < 40;
        phTapT = now; phTapX = x; phTapY = y;
        if (phOverlay && y >= UIH - 66 && !dbl) {
            int i = (x - 30) / 204;
            if (i >= 0 && i < 5 && (x - 30) % 204 < 196) {
                if (i == 0) { phSlide = !phSlide; phSlideT = 0; if (phSlide) phOverlay = false; }
                else if (i == 1) phSetPic(true);
                else if (i == 2) phSetPic(false);
                else if (i == 3) { bool ok = savePicCover(gPhotos[phIdx], DIR_ + "avatar.png", 256, 256); if (ok) avatarLoad(); toast(ok ? "Account picture changed" : "Couldn't use this picture"); }
                else {
                    if (phDelArm > 0) {
                        std::string p = gPhotos[phIdx];
                        if (remove(p.c_str()) == 0) {
                            for (size_t k = 0; k < gAllPics.size(); k++) if (gAllPics[k].path == p) { gAllPics.erase(gAllPics.begin() + k); break; }
                            phFreeThumbs(); gPhotos.erase(gPhotos.begin() + phIdx);
                            if (gPhotos.empty()) phView = 0; else { phIdx %= (int)gPhotos.size(); loadPhoto(); }
                            toast("Picture deleted");
                        } else toast("Couldn't delete it");
                        phDelArm = 0;
                    } else phDelArm = 3.0f;
                }
            }
            return;
        }
        if ((x - 60) * (x - 60) + (y - 56) * (y - 56) < 40 * 40 && phOverlay) { phView = 0; phSlide = false; return; }
        if (dbl) { phOverlay = !phOverlay; if (phZ > 1.02f) { phZ = 1; phOX = phOY = 0; } else { phZ = 2.5f; phOX = (W / 2 - x) * 1.5f; phOY = (UIH / 2 - y) * 1.5f; phClamp(); } return; }
        if (phZ <= 1.02f && x < W / 4) photoNav(-1);
        else if (phZ <= 1.02f && x > 3 * W / 4) photoNav(1);
        else phOverlay = !phOverlay;
        return;
    }
    if (y >= 104 && y < 150) {
        auto ch = phChips();
        int cx = 60;
        for (int i = 0; i < (int)ch.size(); i++) {
            int w = tw(ch[i], F22) + 36;
            if (inR(x, y, cx, 108, w, 38)) { if (i < 5) { phChip = i; photoFilter(); } return; }
            cx += w + 8;
        }
        return;
    }
    if (phChip == 4 && y >= PH_Y0) {
        auto al = phAlbums();
        int c = (x - 40) / 400, r = (y - PH_Y0 + phScroll) / 82;
        int i = r * 3 + c;
        if (c >= 0 && c < 3 && i >= 0 && i < (int)al.size()) { phAlbum = al[i].first; phChip = 5; photoFilter(); }
        return;
    }
    if (y >= PH_Y0) {
        int c = (x - PH_X0) / (PH_W + PH_GAP), r = (y - PH_Y0 + phScroll) / (PH_H + PH_GAP);
        int i = r * PH_COLS + c;
        if (c >= 0 && c < PH_COLS && i >= 0 && i < (int)gPhotos.size()) { phIdx = i; loadPhoto(); phView = 1; phOverlay = true; phSlide = false; }
    }
}

// ---------------------------------------------------------------- Calendar (UI)
static const Col CAL_PURPLE = {110, 70, 215};
static const int CAL_X = 56, CAL_Y = 168, CAL_CW = 130, CAL_RH = 80;
// @@APPS_B@@
// ================================================================ network layer (Wi-Fi), JSON helpers, generic text-field keyboard target
struct NetJob {
    int kind = 0;                       // 0 plain GET, 1 IMAP (custom commands), 2 SMTP send
    std::string url, a, b, c, d, e, f;  // free parameters
    std::string user, pass, from, to, payload, cmd1, cmd2;   // mail jobs
    std::string out;
    std::string savePath; long long maxBytes = 0; Uint32 maxMs = 0, t0 = 0;   // download straight to a file (stops quietly after maxBytes if set)
    volatile long long dlNow = 0, dlTotal = 0; volatile bool cancel = false; FILE* fp = nullptr;
    int code = 0;
    bool ok = false;
    volatile int state = 0;             // 1 running, 2 finished
    Thread th;
    void (*done)(NetJob&) = nullptr;
};
static std::vector<NetJob*> gJobs;
static bool gNetInit = false, gNetUp = false;
static Uint32 gNetTickT = 0;
static void netInit() {
    if (gNetInit) return;
    gNetInit = true;
#ifdef WINNX_NET
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    nifmInitialize(NifmServiceType_User);
}
static void netTick(Uint32 now) {
    if (gNetTickT != 0 && now - gNetTickT < 3000) return;
    gNetTickT = now;
    netInit();
    NifmInternetConnectionType t; u32 str = 0; NifmInternetConnectionStatus st;
    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&t, &str, &st)) && st == NifmInternetConnectionStatus_Connected) {
        gNetUp = true; gWifiBars = (int)std::max(1u, std::min(3u, str));
        if (t == NifmInternetConnectionType_Ethernet) gWifiBars = 3;
    } else { gNetUp = false; gWifiBars = 0; }
    if (gOffline) gWifiBars = 0;
}
static bool netUp() { return gNetUp && !gOffline; }
#ifdef WINNX_NET
static size_t curlWrite(char* p, size_t sz, size_t n, void* u) {
    std::string* o = (std::string*)u;
    if (o->size() + sz * n > (4u << 20)) return 0;
    o->append(p, sz * n);
    return sz * n;
}
#endif
static std::string (*gImapSecond)(const std::string&) = nullptr;   // builds the 2nd IMAP command from the 1st answer
#ifdef WNX_HOST
extern bool hostHttp(NetJob& j);
#endif
#ifdef WINNX_NET
struct PayloadCtx { const std::string* s; size_t pos; };
static size_t curlRead(char* p, size_t sz, size_t n, void* u) {
    PayloadCtx* c = (PayloadCtx*)u;
    size_t want = sz * n, left = c->s->size() - c->pos;
    if (want > left) want = left;
    memcpy(p, c->s->data() + c->pos, want); c->pos += want;
    return want;
}
static size_t curlWriteFile(char* p, size_t sz, size_t n, void* u) {
    NetJob* j = (NetJob*)u; size_t len = sz * n;
    if (j->cancel) return 0;
    if (j->fp) fwrite(p, 1, len, j->fp);
    j->dlNow += (long long)len;
    if (j->maxBytes && j->dlNow >= j->maxBytes) return 0;
    return len;
}
static int curlProg(void* u, curl_off_t dlt, curl_off_t, curl_off_t, curl_off_t) { NetJob* j = (NetJob*)u; j->dlTotal = (long long)dlt; if (j->maxMs && SDL_GetTicks() - j->t0 > j->maxMs) return 1; return j->cancel ? 1 : 0; }
static void curlCommon(CURL* c, NetJob& j) {
    curl_easy_setopt(c, CURLOPT_URL, j.url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &j.out);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 40L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);   // the Switch has no CA bundle; see the About page
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Windows81/1.0.0 (Nintendo Switch)");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    if (!j.savePath.empty()) {
        j.t0 = SDL_GetTicks();
        j.fp = fopen((j.savePath + ".part").c_str(), "wb");
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteFile);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &j);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, curlProg);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &j);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1000L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    }
}
#endif
static bool httpDo(NetJob& j) {
#ifdef WNX_HOST
    return hostHttp(j);
#elif defined(WINNX_NET)
    CURL* c = curl_easy_init();
    if (!c) return false;
    curlCommon(c, j);
    CURLcode rc;
    long code = 0;
    if (j.kind == 0) {
        rc = curl_easy_perform(c);
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
        j.code = (int)code;
        curl_easy_cleanup(c);
        bool good = (rc == CURLE_OK || (rc == CURLE_WRITE_ERROR && j.maxBytes && j.dlNow >= j.maxBytes) || (rc == CURLE_ABORTED_BY_CALLBACK && j.maxMs && j.dlNow > 20000)) && code >= 200 && code < 300;
        if (j.fp) { fclose(j.fp); j.fp = nullptr; if (good) { remove(j.savePath.c_str()); rename((j.savePath + ".part").c_str(), j.savePath.c_str()); } else remove((j.savePath + ".part").c_str()); }
        return good;
    }
    curl_easy_setopt(c, CURLOPT_USERNAME, j.user.c_str());
    curl_easy_setopt(c, CURLOPT_PASSWORD, j.pass.c_str());
    if (j.kind == 1) {                                   // IMAP: up to two custom commands on one connection, else a plain fetch
        bool ok = true;
        if (!j.cmd1.empty()) {
            curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, j.cmd1.c_str());
            rc = curl_easy_perform(c); ok = rc == CURLE_OK;
            j.code = (int)rc;
            if (ok && !j.cmd2.empty()) {
                // the first answer tells us which messages to ask for: let the caller's hook build the second command
                std::string first = j.out; j.out.clear();
                std::string c2 = gImapSecond ? gImapSecond(first) : j.cmd2;
                j.a = first;                             // keep the first answer for the caller
                curl_easy_setopt(c, CURLOPT_URL, j.b.c_str());
                curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, c2.c_str());
                rc = curl_easy_perform(c); ok = rc == CURLE_OK; j.code = (int)rc;
            }
        } else { rc = curl_easy_perform(c); ok = rc == CURLE_OK; j.code = (int)rc; }
        curl_easy_cleanup(c);
        return ok;
    }
    // SMTP
    if (j.url.compare(0, 7, "smtp://") == 0) curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(c, CURLOPT_MAIL_FROM, ("<" + j.from + ">").c_str());
    struct curl_slist* rcpt = nullptr;
    rcpt = curl_slist_append(rcpt, ("<" + j.to + ">").c_str());
    curl_easy_setopt(c, CURLOPT_MAIL_RCPT, rcpt);
    PayloadCtx pc = {&j.payload, 0};
    curl_easy_setopt(c, CURLOPT_READFUNCTION, curlRead);
    curl_easy_setopt(c, CURLOPT_READDATA, &pc);
    curl_easy_setopt(c, CURLOPT_UPLOAD, 1L);
    rc = curl_easy_perform(c);
    j.code = (int)rc;
    curl_slist_free_all(rcpt);
    curl_easy_cleanup(c);
    return rc == CURLE_OK;
#else
    (void)j; j.code = -1; return false;   // built without libcurl
#endif
}
static void netWorker(void* p) {
    NetJob* j = (NetJob*)p;
    j->ok = httpDo(*j);
    j->state = 2;
}
static bool netStart(NetJob* j) {
    netInit();
    if (!netUp()) { delete j; return false; }
    j->state = 1;
    if (R_FAILED(threadCreate(&j->th, netWorker, j, NULL, 0x80000, 0x2C, -2)) || R_FAILED(threadStart(&j->th))) { delete j; return false; }
    gJobs.push_back(j);
    return true;
}
static void netPoll() {
    for (size_t i = 0; i < gJobs.size();) {
        NetJob* j = gJobs[i];
        if (j->state == 2) {
            threadWaitForExit(&j->th); threadClose(&j->th);
            gJobs.erase(gJobs.begin() + i);
            if (j->done) j->done(*j);
            delete j;
        } else i++;
    }
}
static std::string netStatusText() {
    if (gOffline) return "Offline mode is on";
    if (!gNetUp) return "Not connected to Wi-Fi";
    u32 ip = 0; char b[48] = "Connected";
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip))) snprintf(b, sizeof b, "Connected  (%u.%u.%u.%u)", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24);
    return b;
}

// ---- tiny JSON reader: enough for flat API answers
static bool jfind(const std::string& j, const char* key, size_t from, size_t& pos) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = from;
    while ((p = j.find(k, p)) != std::string::npos) {
        size_t q = p + k.size();
        while (q < j.size() && (j[q] == ' ' || j[q] == '\n' || j[q] == '\t')) q++;
        if (q < j.size() && j[q] == ':') { q++; while (q < j.size() && (j[q] == ' ' || j[q] == '\n' || j[q] == '\t')) q++; pos = q; return true; }
        p += k.size();
    }
    return false;
}
static std::string jUnescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            if (c == 'n') o += '\n'; else if (c == 't') o += ' '; else if (c == 'u' && i + 4 < s.size()) {
                unsigned cp = (unsigned)strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16); i += 4;
                if (cp < 0x80) o += (char)cp;
                else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
                else { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
            } else o += c;
        } else o += s[i];
    }
    return o;
}
static std::string jstr(const std::string& j, const char* key, size_t from = 0, const std::string& def = "") {
    size_t p;
    if (!jfind(j, key, from, p) || p >= j.size() || j[p] != '"') return def;
    size_t e = ++p;
    while (e < j.size() && !(j[e] == '"' && j[e - 1] != '\\')) e++;
    return jUnescape(j.substr(p, e - p));
}
static double jnum(const std::string& j, const char* key, size_t from = 0, double def = 0) {
    size_t p;
    if (!jfind(j, key, from, p)) return def;
    char* e;
    double v = strtod(j.c_str() + p, &e);
    return e == j.c_str() + p ? def : v;
}
static std::vector<std::string> jarr(const std::string& j, const char* key, size_t from = 0) {
    std::vector<std::string> out;
    size_t p;
    if (!jfind(j, key, from, p) || p >= j.size() || j[p] != '[') return out;
    p++;
    while (p < j.size() && j[p] != ']') {
        while (p < j.size() && (j[p] == ' ' || j[p] == ',' || j[p] == '\n')) p++;
        if (p >= j.size() || j[p] == ']') break;
        if (j[p] == '"') { size_t e = ++p; while (e < j.size() && !(j[e] == '"' && j[e - 1] != '\\')) e++; out.push_back(j.substr(p, e - p)); p = e + 1; }
        else { size_t e = p; while (e < j.size() && j[e] != ',' && j[e] != ']') e++; out.push_back(j.substr(p, e - p)); p = e; }
        if (out.size() > 400) break;
    }
    return out;
}
static long long daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static long long nowMinutes() { return daysFromCivil(gNow.y, gNow.mo, gNow.d) * 1440 + gNow.h * 60 + gNow.mi; }

// ---- generic text field on the on-screen keyboard (kbTarget 99); globals are declared next to the keyboard
static void openKbStr(std::string* s, size_t maxLen, void (*enter)() = nullptr, bool multiline = false) {
    gKbStr = s; gKbMax = maxLen; gKbEnter = enter; gKbMulti = multiline;
    openKb(99);
}
static bool kbStrActive(const std::string* s) { return kbOpen && kbTarget == 99 && gKbStr == s; }
// text box helper: draws a box with text, placeholder and caret; returns nothing (taps are handled by the caller)
static void drawField(int x, int y, int w, int h, std::string* s, const char* ph, int f, Col fg, Col bg, Col border, bool pass = false) {
    bool act = kbStrActive(s);
    rect(x, y, w, h, bg);
    outline(x, y, w, h, 2, act ? THEMES[gTheme].accent : border);
    std::string t = *s;
    if (pass) t = std::string(t.size(), '*');
    int fh = FSZ[f];
    if (t.empty() && !act) { text(ph, f, x + 12, y + (h - fh) / 2 - 3, fg, 0, 110); return; }
    while (t.size() > 1 && tw(t, f) > w - 28) t.erase(0, 1);
    text(t, f, x + 12, y + (h - fh) / 2 - 3, fg);
    if (act && (SDL_GetTicks() / 500) % 2 == 0) rect(x + 14 + tw(t, f), y + 8, 2, h - 16, fg);
}

// ================================================================ Weather (open-meteo, no key needed)
struct WxDay { std::string date; int code = 0; float hi = 0, lo = 0; int pop = 0; };
struct WxHour { std::string t; float temp = 0; int code = 0; };
struct Wx { bool have = false; float temp = 0, feels = 0, wind = 0; int hum = 0, code = 0, isDay = 1; std::string time; std::vector<WxDay> days; std::vector<WxHour> hours; };
static Wx gWx;
static std::string gWxCity, gWxCountry, gWxErr, gWxSearch;
static double gWxLat = 0, gWxLon = 0;
static bool gWxHaveLoc = false, gWxBusy = false, gWxSearching = false;
static long long gWxStamp = 0;     // minutes since epoch at the last successful update
struct WxPlace { std::string name, country, admin; double lat, lon; };
static std::vector<WxPlace> gWxPlaces;
static int gWxView = 0;            // 0 weather, 1 change location
static Uint32 gWxOpenT = 0;

static std::string wxTemp(float c) {
    float v = gTempF ? c * 9.0f / 5.0f + 32.0f : c;
    char b[24]; snprintf(b, sizeof b, "%d\xC2\xB0", (int)lroundf(v));
    return b;
}
static const char* wxText(int code) {
    switch (code) {
        case 0: return "Clear"; case 1: return "Mostly clear"; case 2: return "Partly cloudy"; case 3: return "Cloudy";
        case 45: case 48: return "Foggy";
        case 51: case 53: case 55: case 56: case 57: return "Drizzle";
        case 61: return "Light rain"; case 63: return "Rain"; case 65: return "Heavy rain"; case 66: case 67: return "Freezing rain";
        case 71: return "Light snow"; case 73: return "Snow"; case 75: return "Heavy snow"; case 77: return "Snow grains";
        case 80: case 81: return "Showers"; case 82: return "Heavy showers"; case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm"; case 96: case 99: return "Thunderstorm, hail";
        default: return "Unknown";
    }
}
static int wxKind(int code) { // 0 clear 1 partly 2 cloud 3 fog 4 rain 5 snow 6 storm
    if (code == 0) return 0; if (code == 1 || code == 2) return 1; if (code == 3) return 2; if (code == 45 || code == 48) return 3;
    if (code >= 95) return 6; if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 5;
    return 4;
}
static void wxColors(int code, int isDay, Col& top, Col& bot) {
    switch (wxKind(code)) {
        case 0: top = Col{34, 124, 214}; bot = Col{120, 196, 244}; break;
        case 1: top = Col{66, 128, 190}; bot = Col{150, 190, 222}; break;
        case 2: top = Col{96, 108, 124}; bot = Col{150, 160, 172}; break;
        case 3: top = Col{118, 128, 138}; bot = Col{172, 178, 184}; break;
        case 4: top = Col{44, 64, 96}; bot = Col{88, 108, 140}; break;
        case 5: top = Col{104, 126, 158}; bot = Col{190, 206, 226}; break;
        default: top = Col{34, 38, 58}; bot = Col{70, 70, 96}; break;
    }
    if (!isDay) { top = mix(top, Col{4, 8, 30}, 0.7f); bot = mix(bot, Col{10, 16, 50}, 0.6f); }
}
static void cloudShape(float cx, float cy, float w, Col c, int a) {
    fdiscA(cx - w * 0.24f, cy + w * 0.06f, w * 0.17f, c, a);
    fdiscA(cx - w * 0.04f, cy - w * 0.06f, w * 0.24f, c, a);
    fdiscA(cx + w * 0.2f, cy + w * 0.02f, w * 0.2f, c, a);
    rect((int)(cx - w * 0.24f), (int)(cy + w * 0.06f), (int)(w * 0.44f), (int)(w * 0.17f), c, a);
    fdiscA(cx - w * 0.24f, cy + w * 0.23f, 0.0f, c, a);
}
// weather icon; sz = overall width, t = animation time in seconds
static void wxIcon(int code, int isDay, int cx, int cy, int sz, float t) {
    int k = wxKind(code);
    Col cloud = {236, 241, 247}, sun = {255, 204, 48};
    float s = (float)sz;
    auto drawSun = [&](float x, float y, float r) {
        for (int i = 0; i < 8; i++) {
            float a = i * 0.785398f + t * 0.5f;
            line((int)(x + sinf(a) * r * 1.35f), (int)(y - cosf(a) * r * 1.35f), (int)(x + sinf(a) * r * 1.9f), (int)(y - cosf(a) * r * 1.9f), std::max(2, (int)(r * 0.18f)), sun);
        }
        fdiscA(x, y, r, sun, 255);
    };
    auto drawMoon = [&](float x, float y, float r) {
        fdiscA(x, y, r, Col{232, 236, 246}, 255);
        fdiscA(x - r * 0.35f, y - r * 0.2f, r * 0.2f, Col{206, 212, 228}, 255);
        fdiscA(x + r * 0.3f, y + r * 0.3f, r * 0.14f, Col{206, 212, 228}, 255);
    };
    switch (k) {
        case 0: if (isDay) drawSun((float)cx, (float)cy, s * 0.22f); else drawMoon((float)cx, (float)cy, s * 0.26f); break;
        case 1:
            if (isDay) drawSun(cx - s * 0.16f, cy - s * 0.14f, s * 0.16f); else drawMoon(cx - s * 0.16f, cy - s * 0.14f, s * 0.2f);
            cloudShape(cx + s * 0.06f, cy + s * 0.1f, s * 0.8f, cloud, 255);
            break;
        case 2: cloudShape((float)cx, (float)cy, s * 0.9f, Col{216, 222, 230}, 255); break;
        case 3:
            for (int i = 0; i < 4; i++) rect(cx - (int)(s * 0.36f) + (i % 2) * (int)(s * 0.08f), cy - (int)(s * 0.2f) + i * (int)(s * 0.14f), (int)(s * 0.64f), std::max(3, (int)(s * 0.06f)), cloud, 220);
            break;
        case 4:
            cloudShape((float)cx, cy - s * 0.12f, s * 0.82f, Col{200, 208, 220}, 255);
            for (int i = 0; i < 3; i++) {
                float ph = fmodf(t * 1.4f + i * 0.33f, 1.0f);
                float x = cx - s * 0.22f + i * s * 0.22f - ph * s * 0.06f, y = cy + s * 0.1f + ph * s * 0.3f;
                line((int)x, (int)y, (int)(x - s * 0.05f), (int)(y + s * 0.14f), std::max(2, (int)(s * 0.04f)), Col{110, 176, 245}, (int)(255 * (1.0f - ph * 0.6f)));
            }
            break;
        case 5:
            cloudShape((float)cx, cy - s * 0.12f, s * 0.82f, Col{214, 222, 234}, 255);
            for (int i = 0; i < 3; i++) {
                float ph = fmodf(t * 0.6f + i * 0.33f, 1.0f);
                fdiscA(cx - s * 0.22f + i * s * 0.22f + sinf(t * 2 + i) * s * 0.03f, cy + s * 0.1f + ph * s * 0.3f, std::max(2.0f, s * 0.035f), WHITE, (int)(255 * (1.0f - ph * 0.5f)));
            }
            break;
        default:
            cloudShape((float)cx, cy - s * 0.12f, s * 0.82f, Col{120, 128, 150}, 255);
            tri(cx + s * 0.04f, cy + s * 0.0f, cx - s * 0.12f, cy + s * 0.2f, cx + s * 0.02f, cy + s * 0.2f, sun);
            tri(cx + s * 0.02f, cy + s * 0.2f, cx + s * 0.14f, cy + s * 0.2f, cx - s * 0.04f, cy + s * 0.4f, sun);
            break;
    }
}

// ---- parsing / persistence
static bool wxParse(const std::string& j) {
    Wx w;
    size_t cp;
    if (!jfind(j, "current", 0, cp)) return false;
    w.temp = (float)jnum(j, "temperature_2m", cp, 0);
    w.feels = (float)jnum(j, "apparent_temperature", cp, w.temp);
    w.hum = (int)jnum(j, "relative_humidity_2m", cp, 0);
    w.code = (int)jnum(j, "weather_code", cp, 0);
    w.isDay = (int)jnum(j, "is_day", cp, 1);
    w.wind = (float)jnum(j, "wind_speed_10m", cp, 0);
    w.time = jstr(j, "time", cp);
    size_t dp, hp;
    if (jfind(j, "daily", 0, dp)) {
        auto dt = jarr(j, "time", dp), dc = jarr(j, "weather_code", dp), dh = jarr(j, "temperature_2m_max", dp), dl = jarr(j, "temperature_2m_min", dp), dpp = jarr(j, "precipitation_probability_max", dp);
        for (size_t i = 0; i < dt.size() && i < 8; i++) {
            WxDay d; d.date = dt[i];
            d.code = i < dc.size() ? atoi(dc[i].c_str()) : 0; d.hi = i < dh.size() ? (float)atof(dh[i].c_str()) : 0; d.lo = i < dl.size() ? (float)atof(dl[i].c_str()) : 0;
            d.pop = i < dpp.size() ? atoi(dpp[i].c_str()) : 0;
            w.days.push_back(d);
        }
    }
    if (jfind(j, "hourly", 0, hp)) {
        auto ht = jarr(j, "time", hp), hc = jarr(j, "weather_code", hp), hv = jarr(j, "temperature_2m", hp);
        // start at the current hour
        size_t start = 0;
        std::string cur = w.time.substr(0, 13);
        for (size_t i = 0; i < ht.size(); i++) if (ht[i].substr(0, 13) >= cur) { start = i; break; }
        for (size_t i = start; i < ht.size() && w.hours.size() < 24; i++) {
            WxHour h; h.t = ht[i]; h.temp = i < hv.size() ? (float)atof(hv[i].c_str()) : 0; h.code = i < hc.size() ? atoi(hc[i].c_str()) : 0;
            w.hours.push_back(h);
        }
    }
    w.have = true;
    gWx = w;
    return true;
}
static void wxSaveLoc() {
    writeFile(DIR_ + "weather_loc.txt", gWxCity + "\t" + gWxCountry + "\t" + std::to_string(gWxLat) + "\t" + std::to_string(gWxLon) + "\n");
}
static void wxLoad() {
    std::string d;
    if (readFile(DIR_ + "weather_loc.txt", d)) {
        std::vector<std::string> f; size_t a = 0;
        while (true) { size_t b = d.find('\t', a); if (b == std::string::npos) { f.push_back(d.substr(a)); break; } f.push_back(d.substr(a, b - a)); a = b + 1; }
        if (f.size() >= 4) { gWxCity = f[0]; gWxCountry = f[1]; gWxLat = atof(f[2].c_str()); gWxLon = atof(f[3].c_str()); gWxHaveLoc = true; }
    }
    std::string j;
    if (readFile(DIR_ + "weather_cache.json", j)) {
        size_t p = j.find('\n');
        if (p != std::string::npos) { gWxStamp = atoll(j.substr(0, p).c_str()); wxParse(j.substr(p + 1)); }
    }
}
static void wxGeoDone(NetJob& j);
static void wxWeatherDone(NetJob& j);
static void wxPlacesDone(NetJob& j);
static void wxFetch(bool force) {
    if (gWxBusy) return;
    if (!force && gWx.have && nowMinutes() - gWxStamp < 30) return;
    if (!netUp()) { gWxErr = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob();
    if (!gWxHaveLoc) {
        if (!gLocOK) { gWxErr = "Choose a city to see the weather."; delete j; return; }
        j->url = "http://ip-api.com/json/?fields=status,city,country,lat,lon";
        j->done = wxGeoDone;
    } else {
        char u[420];
        snprintf(u, sizeof u, "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m&hourly=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=7", gWxLat, gWxLon);
        j->url = u; j->done = wxWeatherDone;
    }
    if (netStart(j)) { gWxBusy = true; gWxErr.clear(); }
}
static void wxGeoDone(NetJob& j) {
    gWxBusy = false;
    if (j.ok && jstr(j.out, "status") == "success") {
        gWxCity = jstr(j.out, "city"); gWxCountry = jstr(j.out, "country");
        gWxLat = jnum(j.out, "lat"); gWxLon = jnum(j.out, "lon"); gWxHaveLoc = true;
        if (gWxCity.empty()) gWxCity = "My location";
        wxSaveLoc();
        wxFetch(true);
    } else gWxErr = "Couldn't find your location. Search for a city instead.";
}
static void wxWeatherDone(NetJob& j) {
    gWxBusy = false;
    if (j.ok && wxParse(j.out)) {
        gWxStamp = nowMinutes();
        writeFile(DIR_ + "weather_cache.json", std::to_string(gWxStamp) + "\n" + j.out);
        gWxErr.clear();
    } else gWxErr = "Couldn't get the forecast. Check your connection.";
}
static void wxSearchGo() {
    if (gWxSearch.empty()) return;
    if (!netUp()) { gWxErr = "No internet connection."; return; }
    NetJob* j = new NetJob();
    j->url = "https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name=" + urlEncode(gWxSearch);
    j->done = wxPlacesDone;
    if (netStart(j)) gWxSearching = true;
}
static void wxPlacesDone(NetJob& j) {
    gWxSearching = false;
    gWxPlaces.clear();
    if (!j.ok) { gWxErr = "Search failed."; return; }
    size_t p = 0;
    while ((p = j.out.find("\"id\"", p)) != std::string::npos && gWxPlaces.size() < 6) {
        size_t nx = j.out.find("\"id\"", p + 4);
        std::string seg = j.out.substr(p, nx == std::string::npos ? std::string::npos : nx - p);
        WxPlace pl;
        pl.name = jstr(seg, "name"); pl.lat = jnum(seg, "latitude"); pl.lon = jnum(seg, "longitude");
        pl.country = jstr(seg, "country"); pl.admin = jstr(seg, "admin1");
        if (!pl.name.empty()) gWxPlaces.push_back(pl);
        p += 4;
    }
    if (gWxPlaces.empty()) gWxErr = "No city found.";
    else gWxErr.clear();
}

// ---- UI
static SDL_Texture* gCloudTex = nullptr;
static void wxBgCloud(float cx, float cy, float w, int a) {          // one opaque cloud drawn once, then blended with alpha so overlaps don't show
    if (!gCloudTex) {
        gCloudTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 400, 160);
        if (gCloudTex) {
            SDL_SetTextureBlendMode(gCloudTex, SDL_BLENDMODE_BLEND);
            SDL_Texture* prev = SDL_GetRenderTarget(gR);
            SDL_SetRenderTarget(gR, gCloudTex);
            SDL_SetRenderDrawColor(gR, 255, 255, 255, 0); SDL_RenderClear(gR);
            cloudShape(200, 84, 360, WHITE, 255);
            SDL_SetRenderTarget(gR, prev);
        }
    }
    if (!gCloudTex) return;
    SDL_SetTextureAlphaMod(gCloudTex, (Uint8)a);
    SDL_Rect d = {(int)(cx - w / 2), (int)(cy - w * 0.2f), (int)w, (int)(w * 0.4f)};
    SDL_RenderCopy(gR, gCloudTex, nullptr, &d);
}
static void wxParticles(int code, int isDay, float t) {
    int k = wxKind(code);
    if (k == 4 || k == 6) {
        for (int i = 0; i < 70; i++) {
            float sp = 500 + (i * 37) % 300, ph = fmodf(t * sp / 700.0f + i * 0.137f, 1.0f);
            int x = (i * 193) % (W + 100) - 50 + (int)(ph * 60), y = (int)(ph * (UIH + 60)) - 30;
            line(x, y, x - 8, y + 26, 2, Col{200, 220, 255}, 90);
        }
        if (k == 6 && fmodf(t, 5.0f) < 0.18f) rect(0, 0, W, UIH, WHITE, 70);
    } else if (k == 5) {
        for (int i = 0; i < 60; i++) {
            float sp = 60 + (i * 29) % 70, ph = fmodf(t * sp / 700.0f + i * 0.173f, 1.0f);
            int x = (i * 211) % W + (int)(sinf(t + i) * 24), y = (int)(ph * (UIH + 20)) - 10;
            fdiscA((float)x, (float)y, 2.0f + (i % 3), WHITE, 190);
        }
    } else if (k >= 1) {
        for (int i = 0; i < 4; i++) {
            float x = fmodf(t * (8 + i * 5) + i * 420, W + 500) - 250;
            wxBgCloud(x, 120.0f + i * 130, 360.0f + i * 40, k == 2 ? 44 : 28);
        }
    } else if (isDay) {
        for (int i = 0; i < 3; i++) fdiscA(W - 200.0f, 140.0f, 150.0f + i * 110 + sinf(t * 0.6f + i) * 12, WHITE, 16 - i * 4);
    } else {
        for (int i = 0; i < 40; i++) { float a = 100 + 120 * (0.5f + 0.5f * sinf(t * 1.5f + i)); rect((i * 389) % W, (i * 127) % 300, 3, 3, WHITE, (int)a); }
    }
}
static const char* wxDayName(const std::string& date, int i) {
    if (i == 0) return "Today";
    if (date.size() < 10) return "";
    return WD3[dow(atoi(date.substr(0, 4).c_str()), atoi(date.substr(5, 2).c_str()), atoi(date.substr(8, 2).c_str()))];
}
static std::string wxHourName(const std::string& t) {
    if (t.size() < 13) return "";
    int h = atoi(t.substr(11, 2).c_str());
    char b[16];
    if (g24) snprintf(b, sizeof b, "%02d:00", h);
    else snprintf(b, sizeof b, "%d %s", h % 12 == 0 ? 12 : h % 12, h >= 12 ? "PM" : "AM");
    return b;
}
static void drawWeather(int bottom) {
    (void)bottom;
    float t = (SDL_GetTicks() - gWxOpenT) / 1000.0f;
    Col top = {40, 90, 160}, bot = {90, 140, 200};
    if (gWx.have) wxColors(gWx.code, gWx.isDay, top, bot);
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(top, bot, y / (float)UIH));
    if (gWx.have && gAnim) wxParticles(gWx.code, gWx.isDay, t);
    Col fg = WHITE;
    if (gWxView == 1) { // ---- change location
        arrowCircle(60, 70, 24, 2, WHITE, 255);
        text("Choose a location", F44, 110, 44, WHITE);
        drawField(70, 130, 900, 58, &gWxSearch, "Search for a city", F30, DARK, WHITE, Col{200, 200, 200});
        btn(990, 130, 220, 58, "Search", Col{0, 114, 198}, WHITE);
        if (gLocOK) btn(70, 206, 420, 52, "Use my current location", Col{0, 0, 0}, WHITE);
        int y = 280;
        for (size_t i = 0; i < gWxPlaces.size(); i++) {
            rect(70, y, 1140, 62, Col{0, 0, 0}, 90);
            text(gWxPlaces[i].name, F30, 90, y + 10, WHITE);
            text(gWxPlaces[i].admin + (gWxPlaces[i].admin.empty() ? "" : ", ") + gWxPlaces[i].country, F22, 1190, y + 18, WHITE, 2, 200);
            y += 70;
        }
        if (gWxSearching) text("Searching...", F22, 70, 280, WHITE, 0, 200);
        if (!gWxErr.empty()) text(gWxErr, F22, 70, UIH - 60, Col{255, 220, 150});
        return;
    }
    // header
    text(gWxCity.empty() ? std::string("Weather") : gWxCity, F44, 70, 30, fg);
    if (!gWxCountry.empty()) text(gWxCountry, F22, 72, 86, fg, 0, 200);
    // buttons: search, units, refresh
    rect(W - 260, 34, 56, 46, Col{0, 0, 0}, 70); ringA(W - 238, 55, 10, 3, WHITE, 255); line(W - 230, 63, W - 222, 71, 3, WHITE);
    rect(W - 196, 34, 56, 46, Col{0, 0, 0}, 70); text(gTempF ? "\xC2\xB0""F" : "\xC2\xB0""C", F22, W - 168, 44, WHITE, 1);
    rect(W - 132, 34, 56, 46, Col{0, 0, 0}, 70);
    {
        float rot = gWxBusy ? t * 6.0f : 0;
        for (int i = 0; i < 10; i++) { float a = rot + i * 0.5f; fdiscA(W - 104 + cosf(a) * 11, 57 + sinf(a) * 11, 2.4f, WHITE, 255 - i * 22); }
    }
    if (!gWx.have) {
        text(gWxBusy ? "Getting the weather..." : (gWxErr.empty() ? "No weather yet" : gWxErr), F44, 70, 200, fg);
        if (!gWxBusy) { text("Tap the refresh button when you are on Wi-Fi, or search for a city.", F22, 70, 262, fg, 0, 210); }
        return;
    }
    // main reading
    wxIcon(gWx.code, gWx.isDay, 330, 200, 190, t);
    text(wxTemp(gWx.temp), F130, 60, 100, fg);
    text(wxText(gWx.code), F44, 70, 250, fg);
    char b[128];
    snprintf(b, sizeof b, "Feels like %s     Humidity %d%%     Wind %d km/h", wxTemp(gWx.feels).c_str(), gWx.hum, (int)lroundf(gWx.wind));
    text(b, F22, 72, 308, fg, 0, 220);
    if (!gWx.days.empty()) {
        snprintf(b, sizeof b, "High %s   Low %s   Rain %d%%", wxTemp(gWx.days[0].hi).c_str(), wxTemp(gWx.days[0].lo).c_str(), gWx.days[0].pop);
        text(b, F22, 72, 340, fg, 0, 220);
    }
    // hourly strip
    rect(0, 388, W, 108, Col{0, 0, 0}, 58);
    for (int i = 0; i < 9 && i < (int)gWx.hours.size(); i++) {
        int x = 70 + i * 128;
        text(i == 0 ? "Now" : wxHourName(gWx.hours[i * 2 < (int)gWx.hours.size() ? i * 2 : i].t), F16, x + 40, 398, fg, 1, 220);
        const WxHour& h = gWx.hours[i * 2 < (int)gWx.hours.size() ? i * 2 : i];
        int hr = atoi(h.t.substr(11, 2).c_str());
        wxIcon(h.code, hr >= 6 && hr < 20, x + 40, 440, 40, t);
        text(wxTemp(h.temp), F22, x + 40, 460, fg, 1);
    }
    // 7-day forecast
    int n = std::min<int>(7, gWx.days.size());
    for (int i = 0; i < n; i++) {
        const WxDay& d = gWx.days[i];
        int x = 60 + i * 164;
        rect(x, 510, 152, 148, Col{0, 0, 0}, i == 0 ? 90 : 56);
        text(wxDayName(d.date, i), F22, x + 76, 518, fg, 1);
        wxIcon(d.code, 1, x + 76, 580, 56, t);
        text(wxTemp(d.hi) + "  " + wxTemp(d.lo), F16, x + 76, 620, fg, 1, 230);
    }
    // status line
    long long age = nowMinutes() - gWxStamp;
    if (!gWxErr.empty()) text(gWxErr, F16, W - 20, 100, Col{255, 220, 150}, 2);
    else if (gWxBusy) text("Updating...", F16, W - 20, 100, fg, 2, 200);
    else { snprintf(b, sizeof b, age < 1 ? "Updated just now" : (age < 120 ? "Updated %lld min ago" : "Updated %lld h ago"), age < 120 ? age : age / 60); text(b, F16, W - 20, 100, fg, 2, 180); }
}
static void weatherTap(int x, int y) {
    if (gWxView == 1) {
        if (inR(x, y, 20, 40, 80, 60)) { gWxView = 0; hideKb(); return; }
        if (inR(x, y, 70, 130, 900, 58)) { openKbStr(&gWxSearch, 40, wxSearchGo); return; }
        if (inR(x, y, 990, 130, 220, 58)) { hideKb(); wxSearchGo(); return; }
        if (gLocOK && inR(x, y, 70, 206, 420, 52)) { gWxHaveLoc = false; hideKb(); gWxView = 0; wxFetch(true); return; }
        for (size_t i = 0; i < gWxPlaces.size(); i++) if (inR(x, y, 70, 280 + (int)i * 70, 1140, 62)) {
            gWxCity = gWxPlaces[i].name; gWxCountry = gWxPlaces[i].country; gWxLat = gWxPlaces[i].lat; gWxLon = gWxPlaces[i].lon; gWxHaveLoc = true;
            wxSaveLoc(); gWx.have = false; hideKb(); gWxView = 0; wxFetch(true); return;
        }
        return;
    }
    if (inR(x, y, W - 260, 34, 56, 46)) { gWxView = 1; gWxPlaces.clear(); gWxErr.clear(); openKbStr(&gWxSearch, 40, wxSearchGo); return; }
    if (inR(x, y, W - 196, 34, 56, 46)) { gTempF = !gTempF; saveCfg(); return; }
    if (inR(x, y, W - 132, 34, 56, 46)) { wxFetch(true); if (!netUp()) toast(gOffline ? "Offline mode is on" : "No internet connection"); return; }
}
static void weatherInit() { gWxView = 0; gWxOpenT = SDL_GetTicks(); if (!gWxHaveLoc && !gWx.have) wxLoad(); wxFetch(false); }

// ================================================================ Mail (IMAPS / SMTP through libcurl) -- Outlook-style three panes
struct MailMsg { int uid = 0; bool seen = true; std::string from, addr, subj, date, time, body; bool haveBody = false; };
static std::vector<MailMsg> gMail;
static std::string gMAddr, gMPass, gMImap, gMSmtp;          // account (stored in mail.ini on the SD card)
static std::string gMTo, gMSubj, gMBody, gMMsg;
static int gMView = 0;                                      // 0 inbox, 1 compose, 2 account
static int gMSel = -1, gMScroll = 0, gMBodyScroll = 0, gMFilter = 0, gMUnread = 0, gMTx = 0;
static bool gMBusy = false, gMSending = false;
static long long gMStamp = 0;
static const Col MAIL_BLUE = {0, 114, 198};

static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string b64dec(const std::string& in) {
    std::string o; int v = 0, b = -8;
    for (unsigned char c : in) {
        const char* p = c ? strchr(B64, c) : nullptr;
        if (!p) continue;
        v = (v << 6) | (int)(p - B64); b += 6;
        if (b >= 0) { o += (char)((v >> b) & 255); b -= 8; }
    }
    return o;
}
static std::string b64enc(const std::string& in) {
    std::string o; int v = 0, b = -6;
    for (unsigned char c : in) { v = (v << 8) | c; b += 8; while (b >= 0) { o += B64[(v >> b) & 63]; b -= 6; } }
    if (b > -6) o += B64[((v << 8) >> (b + 8)) & 63];
    while (o.size() % 4) o += '=';
    return o;
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static std::string qpdec(const std::string& in, bool header) {
    std::string o;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '=' && i + 2 < in.size() + 0 && hexv(in[i + 1]) >= 0 && hexv(in[i + 2]) >= 0) { o += (char)(hexv(in[i + 1]) * 16 + hexv(in[i + 2])); i += 2; }
        else if (in[i] == '=' && i + 1 < in.size() && (in[i + 1] == '\r' || in[i + 1] == '\n')) { i++; if (in[i] == '\r' && i + 1 < in.size() && in[i + 1] == '\n') i++; }
        else if (header && in[i] == '_') o += ' ';
        else o += in[i];
    }
    return o;
}
static bool isLatin(const std::string& cs) { std::string l = lower(cs); return l.find("8859") != std::string::npos || l.find("1252") != std::string::npos || l == "latin1"; }
static std::string hdrDecode(const std::string& h) {            // RFC 2047 encoded words
    std::string o; size_t i = 0;
    while (i < h.size()) {
        size_t a = h.find("=?", i);
        if (a == std::string::npos) { o += h.substr(i); break; }
        size_t q1 = h.find('?', a + 2), q2 = q1 == std::string::npos ? q1 : h.find('?', q1 + 1), e = q2 == std::string::npos ? q2 : h.find("?=", q2 + 1);
        if (e == std::string::npos) { o += h.substr(i); break; }
        std::string pre = h.substr(i, a - i);
        bool onlySp = true; for (char c : pre) if (!isspace((unsigned char)c)) onlySp = false;
        if (!(onlySp && !o.empty() && o.size() >= 1 && i > 0 && h.compare(i > 2 ? i - 2 : 0, 2, "?=") == 0)) o += pre;
        std::string cs = h.substr(a + 2, q1 - a - 2), enc = h.substr(q1 + 1, q2 - q1 - 1), txt = h.substr(q2 + 1, e - q2 - 1);
        std::string d = (enc == "B" || enc == "b") ? b64dec(txt) : qpdec(txt, true);
        o += isLatin(cs) ? latin1to8(d) : d;
        i = e + 2;
    }
    return o;
}
static std::string htmlToText(const std::string& h) {
    std::string o; bool tag = false; std::string tn;
    for (size_t i = 0; i < h.size(); i++) {
        char c = h[i];
        if (c == '<') { tag = true; tn.clear(); continue; }
        if (tag) {
            if (c == '>') { tag = false; std::string t = lower(tn); if (t.compare(0, 2, "br") == 0 || t.compare(0, 1, "p") == 0 || t.compare(0, 3, "/p") == 0 || t.compare(0, 3, "div") == 0 || t.compare(0, 2, "tr") == 0 || t.compare(0, 2, "li") == 0 || t.compare(0, 2, "h1") == 0) { if (!o.empty() && o.back() != '\n') o += '\n'; } }
            else if (tn.size() < 12) tn += c;
            continue;
        }
        if (c == '&') {
            size_t e = h.find(';', i);
            if (e != std::string::npos && e - i < 8) {
                std::string en = h.substr(i + 1, e - i - 1);
                if (en == "nbsp") o += ' '; else if (en == "amp") o += '&'; else if (en == "lt") o += '<'; else if (en == "gt") o += '>'; else if (en == "quot") o += '"'; else if (en == "#39" || en == "apos") o += '\'';
                else o += ' ';
                i = e; continue;
            }
        }
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = ' ';
        if (c == ' ' && !o.empty() && (o.back() == ' ' || o.back() == '\n')) continue;
        o += c;
    }
    // drop style/script remnants is not attempted; collapse blank lines
    std::string r; int nl = 0;
    for (char c : o) { if (c == '\n') { if (++nl > 2) continue; } else nl = 0; r += c; }
    return r;
}
static std::string mimeHeader(const std::string& hdrs, const char* name) {      // unfolded value of a header
    std::string l = lower(hdrs), k = std::string("\n") + name + ":";
    std::string src = "\n" + hdrs; l = "\n" + l;
    size_t p = l.find(k);
    if (p == std::string::npos) return "";
    size_t s = p + k.size(), e = s;
    while (e < src.size()) {
        size_t nl = src.find('\n', e);
        if (nl == std::string::npos) { e = src.size(); break; }
        if (nl + 1 < src.size() && (src[nl + 1] == ' ' || src[nl + 1] == '\t')) { e = nl + 1; continue; }
        e = nl; break;
    }
    std::string v = src.substr(s, e - s), o;
    for (char c : v) { if (c == '\r' || c == '\n' || c == '\t') c = ' '; if (c == ' ' && !o.empty() && o.back() == ' ') continue; o += c; }
    while (!o.empty() && o[0] == ' ') o.erase(0, 1);
    while (!o.empty() && o.back() == ' ') o.pop_back();
    return o;
}
static std::string mimeParam(const std::string& v, const char* key) {
    std::string l = lower(v), k = std::string(key) + "=";
    size_t p = l.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    if (p < v.size() && v[p] == '"') { size_t e = v.find('"', p + 1); return v.substr(p + 1, e == std::string::npos ? std::string::npos : e - p - 1); }
    size_t e = p; while (e < v.size() && v[e] != ';' && v[e] != ' ') e++;
    return v.substr(p, e - p);
}
static bool mimeText(const std::string& raw, std::string& out, int depth = 0) {   // finds the best readable part
    size_t sp = raw.find("\r\n\r\n"), skip = 4;
    if (sp == std::string::npos) { sp = raw.find("\n\n"); skip = 2; }
    if (sp == std::string::npos) return false;
    std::string hd = raw.substr(0, sp), body = raw.substr(sp + skip);
    std::string ct = mimeHeader(hd, "content-type"), lct = lower(ct), cte = lower(mimeHeader(hd, "content-transfer-encoding"));
    if (lct.compare(0, 9, "multipart") == 0 && depth < 4) {
        std::string bd = mimeParam(ct, "boundary");
        if (bd.empty()) return false;
        std::string del = "--" + bd;
        std::string plain, html; size_t p = body.find(del);
        while (p != std::string::npos) {
            size_t st = body.find('\n', p);
            if (st == std::string::npos) break;
            size_t nx = body.find(del, st);
            std::string part = body.substr(st + 1, (nx == std::string::npos ? body.size() : nx) - st - 1);
            std::string t;
            std::string pct = lower(mimeHeader(part.substr(0, std::min(part.size(), part.find("\n\n") == std::string::npos ? part.size() : part.find("\n\n"))), "content-type"));
            if (pct.compare(0, 10, "text/plain") == 0 || pct.empty()) { if (mimeText(part, t, depth + 1) && plain.empty()) plain = t; }
            else if (pct.compare(0, 9, "text/html") == 0) { if (mimeText(part, t, depth + 1) && html.empty()) html = t; }
            else if (pct.compare(0, 9, "multipart") == 0) { if (mimeText(part, t, depth + 1) && plain.empty()) plain = t; }
            p = nx;
            if (p != std::string::npos && body.compare(p + del.size(), 2, "--") == 0) break;
        }
        out = !plain.empty() ? plain : html;
        return !out.empty();
    }
    std::string d = body;
    if (cte == "base64") d = b64dec(body); else if (cte == "quoted-printable") d = qpdec(body, false);
    std::string cs = mimeParam(ct, "charset");
    if (!cs.empty() && isLatin(cs)) d = latin1to8(d);
    if (lct.compare(0, 9, "text/html") == 0) d = htmlToText(d);
    for (size_t i = 0; i < d.size();) { if (d[i] == '\r') d.erase(i, 1); else i++; }
    out = d;
    return true;
}
// ---- account + cache
static void mailDefaults() {
    size_t at = gMAddr.find('@');
    if (at == std::string::npos) return;
    std::string dom = lower(gMAddr.substr(at + 1));
    if (gMImap.empty()) {
        if (dom == "gmail.com" || dom == "googlemail.com") gMImap = "imap.gmail.com";
        else if (dom == "outlook.com" || dom == "hotmail.com" || dom == "live.com" || dom == "msn.com") gMImap = "outlook.office365.com";
        else if (dom.find("yahoo.") != std::string::npos) gMImap = "imap.mail.yahoo.com";
        else if (dom == "icloud.com" || dom == "me.com") gMImap = "imap.mail.me.com";
        else gMImap = "imap." + dom;
    }
    if (gMSmtp.empty()) {
        if (dom == "gmail.com" || dom == "googlemail.com") gMSmtp = "smtps://smtp.gmail.com:465";
        else if (dom == "outlook.com" || dom == "hotmail.com" || dom == "live.com" || dom == "msn.com") gMSmtp = "smtp://smtp-mail.outlook.com:587";
        else if (dom.find("yahoo.") != std::string::npos) gMSmtp = "smtps://smtp.mail.yahoo.com:465";
        else if (dom == "icloud.com" || dom == "me.com") gMSmtp = "smtp://smtp.mail.me.com:587";
        else gMSmtp = "smtps://smtp." + dom + ":465";
    }
}
static void mailSaveAcct() { writeFile(DIR_ + "mail.ini", "addr=" + gMAddr + "\npass=" + gMPass + "\nimap=" + gMImap + "\nsmtp=" + gMSmtp + "\n"); }
static void mailLoadAcct() {
    std::string d;
    if (!readFile(DIR_ + "mail.ini", d)) return;
    size_t p = 0;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(p, e - p); p = e + 1;
        size_t q = ln.find('=');
        if (q == std::string::npos) continue;
        std::string k = ln.substr(0, q), v = ln.substr(q + 1);
        if (k == "addr") gMAddr = v; else if (k == "pass") gMPass = v; else if (k == "imap") gMImap = v; else if (k == "smtp") gMSmtp = v;
    }
}
static void mailSaveCache() {
    std::string o = std::to_string(gMStamp) + "\n";
    for (auto& m : gMail) o += std::to_string(m.uid) + "\t" + (m.seen ? "1" : "0") + "\t" + m.from + "\t" + m.addr + "\t" + m.subj + "\t" + m.date + "\t" + m.time + "\n";
    writeFile(DIR_ + "mail_cache.txt", o);
}
static void mailCountUnread() { gMUnread = 0; for (auto& m : gMail) if (!m.seen) gMUnread++; }
static void mailLoadCache() {
    gMail.clear();
    std::string d;
    if (!readFile(DIR_ + "mail_cache.txt", d)) return;
    size_t p = d.find('\n');
    if (p == std::string::npos) return;
    gMStamp = atoll(d.substr(0, p).c_str()); p++;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(p, e - p); p = e + 1;
        std::vector<std::string> f; size_t q = 0;
        for (int k = 0; k < 7; k++) { size_t t = ln.find('\t', q); if (t == std::string::npos) { f.push_back(ln.substr(q)); break; } f.push_back(ln.substr(q, t - q)); q = t + 1; }
        if (f.size() < 7) continue;
        MailMsg m; m.uid = atoi(f[0].c_str()); m.seen = f[1] == "1"; m.from = f[2]; m.addr = f[3]; m.subj = f[4]; m.date = f[5]; m.time = f[6];
        std::string b; if (readFile(DIR_ + "mail/" + std::to_string(m.uid) + ".txt", b)) { m.body = b; m.haveBody = true; }
        gMail.push_back(m);
    }
    mailCountUnread();
}
// ---- fetching
static std::string mailSecond(const std::string& first) {
    size_t p = first.find("MESSAGES ");
    int n = p == std::string::npos ? 0 : atoi(first.c_str() + p + 9);
    if (n <= 0) return "NOOP";
    char b[160]; snprintf(b, sizeof b, "FETCH %d:%d (UID FLAGS BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])", std::max(1, n - 29), n);
    return b;
}
static void mailParseDate(const std::string& d, std::string& date, std::string& tm) {
    std::vector<std::string> t; std::string c;
    for (char ch : d + " ") { if (ch == ' ' || ch == ',') { if (!c.empty()) t.push_back(c); c.clear(); } else c += ch; }
    size_t i = (!t.empty() && !isdigit((unsigned char)t[0][0])) ? 1 : 0;
    if (t.size() >= i + 4) { date = t[i] + " " + t[i + 1]; tm = t[i + 3].substr(0, 5); } else { date = d.substr(0, 10); tm = ""; }
}
static void mailParseList(const std::string& first, const std::string& second) {
    std::vector<MailMsg> v;
    size_t p = 0;
    while ((p = second.find(" FETCH (", p)) != std::string::npos) {
        size_t nxt = second.find("\n* ", p + 8);
        std::string ch = second.substr(p, nxt == std::string::npos ? std::string::npos : nxt - p);
        p += 8;
        MailMsg m;
        size_t u = ch.find("UID "); if (u != std::string::npos) m.uid = atoi(ch.c_str() + u + 4);
        m.seen = ch.find("\\Seen") != std::string::npos;
        size_t lit = ch.find("}\r\n"); if (lit == std::string::npos) lit = ch.find("}\n");
        std::string hd = lit == std::string::npos ? ch : ch.substr(lit + 2);
        std::string fr = hdrDecode(mimeHeader(hd, "from"));
        m.subj = hdrDecode(mimeHeader(hd, "subject"));
        size_t lt = fr.find('<'), gt = fr.find('>');
        if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
            m.addr = fr.substr(lt + 1, gt - lt - 1);
            std::string nm = fr.substr(0, lt);
            while (!nm.empty() && (nm.back() == ' ' || nm.back() == '"')) nm.pop_back();
            while (!nm.empty() && (nm[0] == ' ' || nm[0] == '"')) nm.erase(0, 1);
            m.from = nm.empty() ? m.addr : nm;
        } else m.from = m.addr = fr;
        if (m.subj.empty()) m.subj = "(no subject)";
        mailParseDate(mimeHeader(hd, "date"), m.date, m.time);
        if (m.uid > 0) v.push_back(m);
    }
    (void)first;
    std::reverse(v.begin(), v.end());                       // newest first
    for (auto& m : v) for (auto& o : gMail) if (o.uid == m.uid && o.haveBody) { m.body = o.body; m.haveBody = true; }
    gMail = v;
}
static void mailListDone(NetJob& j) {
    gMBusy = false;
    if (!j.ok) { gMMsg = j.code == 67 || j.code == 9 ? "Sign-in failed. Check your address and app password." : "Couldn't reach the mail server (code " + std::to_string(j.code) + ")."; return; }
    mailParseList(j.a, j.out);
    size_t u = j.a.find("UNSEEN "); int unseen = u == std::string::npos ? -1 : atoi(j.a.c_str() + u + 7);
    mailCountUnread();
    if (unseen >= 0) gMUnread = unseen;
    gMStamp = nowMinutes(); gMMsg.clear(); gMSel = -1;
    mailSaveCache();
}
static void mailRefresh() {
    if (gMBusy) return;
    if (gMAddr.empty() || gMPass.empty()) { gMView = 2; return; }
    if (!netUp()) { gMMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    mailDefaults();
    NetJob* j = new NetJob();
    j->kind = 1; j->url = "imaps://" + gMImap + "/"; j->b = "imaps://" + gMImap + "/INBOX";
    j->user = gMAddr; j->pass = gMPass; j->cmd1 = "STATUS INBOX (MESSAGES UNSEEN)"; j->cmd2 = "x";
    gImapSecond = mailSecond;
    j->done = mailListDone;
    if (netStart(j)) { gMBusy = true; gMMsg = "Checking mail..."; }
}
static int gMBodyUid = 0;
static void mailBodyDone(NetJob& j) {
    gMBusy = false;
    for (auto& m : gMail) if (m.uid == gMBodyUid) {
        if (j.ok) {
            std::string t;
            if (!mimeText(j.out, t)) t = "(This message has no readable text.)";
            if (t.size() > 30000) t.resize(30000);
            m.body = t; m.haveBody = true; m.seen = true;
            writeFile(DIR_ + "mail/" + std::to_string(m.uid) + ".txt", t);
            mailCountUnread(); mailSaveCache();
            gMMsg.clear();
        } else gMMsg = "Couldn't download this message.";
    }
}
static void mailOpenMsg(int i) {
    gMSel = i; gMBodyScroll = 0;
    MailMsg& m = gMail[i];
    if (m.haveBody) { if (!m.seen) { m.seen = true; mailCountUnread(); mailSaveCache(); } return; }
    if (gMBusy) return;
    if (!netUp()) { gMMsg = "No internet connection."; return; }
    NetJob* j = new NetJob();
    j->kind = 1; j->url = "imaps://" + gMImap + "/INBOX/;UID=" + std::to_string(m.uid);
    j->user = gMAddr; j->pass = gMPass; gMBodyUid = m.uid;
    j->done = mailBodyDone;
    if (netStart(j)) { gMBusy = true; gMMsg = "Downloading..."; }
}
static void mailSendDone(NetJob& j) {
    gMBusy = false; gMSending = false;
    if (j.ok) { gMMsg = "Message sent."; gMView = 0; gMTo.clear(); gMSubj.clear(); gMBody.clear(); toast("Message sent"); }
    else gMMsg = "Couldn't send (code " + std::to_string(j.code) + "). Check the account settings.";
}
static void mailSend() {
    if (gMBusy) return;
    if (gMTo.find('@') == std::string::npos) { gMMsg = "Enter a valid recipient address."; return; }
    if (!netUp()) { gMMsg = "No internet connection."; return; }
    mailDefaults();
    NetJob* j = new NetJob();
    j->kind = 2; j->url = gMSmtp; j->user = gMAddr; j->pass = gMPass; j->from = gMAddr; j->to = gMTo;
    bool ascii = true; for (unsigned char c : gMSubj) if (c >= 128) ascii = false;
    std::string subj = ascii ? gMSubj : "=?UTF-8?B?" + b64enc(gMSubj) + "?=";
    std::string body; for (char c : gMBody) { if (c == '\n') body += "\r\n"; else body += c; }
    j->payload = "From: <" + gMAddr + ">\r\nTo: <" + gMTo + ">\r\nSubject: " + subj + "\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: 8bit\r\n\r\n" + body + "\r\n";
    j->done = mailSendDone;
    if (netStart(j)) { gMBusy = true; gMSending = true; gMMsg = "Sending..."; }
}
static void mailInit() {
    if (gMAddr.empty()) mailLoadAcct();
    if (gMail.empty()) mailLoadCache();
    gMView = gMAddr.empty() ? 2 : 0; gMSel = -1; gMScroll = 0; gMFilter = 0;
    if (!gMAddr.empty() && (nowMinutes() - gMStamp > 5 || gMail.empty())) mailRefresh();
}
// ---- UI
static void mailSaveGo() { mailDefaults(); mailSaveAcct(); gMView = 0; hideKb(); gMail.clear(); gMStamp = 0; mailRefresh(); }
static void drawMail() {
    rect(0, 0, W, UIH, Col{255, 255, 255});
    int LW = 230;
    rect(0, 0, LW, UIH, MAIL_BLUE);
    text("Mail", F44, 24, 22, WHITE);
    if (!gMAddr.empty()) text(fitTxt(gMAddr, F16, LW - 40), F16, 24, 78, WHITE, 0, 200);
    struct NavI { const char* n; int cnt; };
    NavI nav[2] = {{"Inbox", gMUnread}, {"Unread", gMUnread}};
    for (int i = 0; i < 2; i++) {
        bool on = gMView == 0 && gMFilter == i;
        if (on) rect(0, 118 + i * 52, LW, 48, Col{255, 255, 255}, 40);
        text(nav[i].n, F22, 24, 126 + i * 52, WHITE);
        if (nav[i].cnt > 0) text(std::to_string(nav[i].cnt), F22, LW - 20, 126 + i * 52, WHITE, 2);
    }
    rect(20, 236, LW - 40, 50, WHITE); text("+  New mail", F22, LW / 2, 245, MAIL_BLUE, 1);
    text("Refresh", F22, 24, 312, WHITE, 0, gMBusy ? 130 : 230);
    text("Account", F22, 24, 360, WHITE, 0, gMView == 2 ? 255 : 230);
    if (gMBusy) { float t = SDL_GetTicks() / 1000.0f; for (int i = 0; i < 8; i++) { float a = t * 6 + i * 0.78f; fdiscA(LW / 2 + cosf(a) * 16, UIH - 60 + sinf(a) * 16, 3.0f, WHITE, 255 - i * 28); } }
    if (gMView == 2) {
        int x = LW + 50;
        text("Add your account", F44, x, 24, DARK);
        text("Gmail, Outlook, Yahoo, iCloud and most providers (IMAP)", F16, x + 2, 84, Col{90, 90, 95});
        rect(W - 300, 20, 270, 54, MAIL_BLUE); text("Save and connect", F22, W - 165, 30, WHITE, 1);
        drawField(x, 110, 700, 52, &gMAddr, "Email address", F22, DARK, WHITE, Col{190, 190, 195});
        drawField(x, 172, 700, 52, &gMPass, "App password", F22, DARK, WHITE, Col{190, 190, 195}, true);
        drawField(x, 234, 700, 52, &gMImap, "IMAP server (optional)", F22, DARK, WHITE, Col{190, 190, 195});
        drawField(x, 296, 700, 52, &gMSmtp, "SMTP server (optional)", F22, DARK, WHITE, Col{190, 190, 195});
        if (!kbOpen) {
            text("Gmail, Yahoo and iCloud need an app password, not your normal one.", F16, x, 380, Col{90, 90, 95});
            text("Your password is stored as plain text in switch/WinNX/mail.ini on your SD card.", F16, x, 408, Col{150, 60, 60});
        }
        if (!gMMsg.empty()) text(fitTxt(gMMsg, F22, 900), F22, x, 440, Col{180, 60, 40});
        return;
    }
    if (gMView == 1) {
        int x = LW + 50;
        text("New message", F44, x, 24, DARK);
        drawField(x, 96, 780, 50, &gMTo, "To", F22, DARK, WHITE, Col{190, 190, 195});
        drawField(x, 160, 780, 50, &gMSubj, "Subject", F22, DARK, WHITE, Col{190, 190, 195});
        bool act = kbStrActive(&gMBody);
        int bh = kbOpen ? 130 : 330;
        rect(x, 224, 780, bh, WHITE); outline(x, 224, 780, bh, 2, act ? MAIL_BLUE : Col{190, 190, 195});
        auto L = wrapText(gMBody + (act && (SDL_GetTicks() / 500) % 2 == 0 ? "|" : ""), F22, 750);
        SDL_Rect cl = {x, 226, 780, bh - 4}; SDL_RenderSetClipRect(gR, &cl);
        int nl = (bh - 16) / 32;
        int start = std::max(0, (int)L.size() - nl);
        for (int i = start; i < (int)L.size(); i++) text(L[i], F22, x + 14, 234 + (i - start) * 32, DARK);
        SDL_RenderSetClipRect(gR, nullptr);
        if (gMBody.empty() && !act) text("Write your message", F22, x + 14, 234, DARK, 0, 110);
        rect(W - 380, 22, 150, 50, gMSending ? Col{150, 150, 155} : MAIL_BLUE); text("Send", F22, W - 305, 32, WHITE, 1);
        rect(W - 220, 22, 150, 50, Col{225, 225, 228}); text("Discard", F22, W - 145, 32, DARK, 1);
        if (!gMMsg.empty()) text(fitTxt(gMMsg, F22, 760), F22, x, 70, Col{180, 60, 40});
        return;
    }
    // inbox: list + reading pane
    int LX = LW, LWD = 400, RX = LW + LWD;
    rect(LX, 0, LWD, UIH, Col{246, 246, 247});
    text(gMFilter == 1 ? "Unread" : "Inbox", F30, LX + 20, 14, MAIL_BLUE);
    if (gMStamp > 0) { long long age = nowMinutes() - gMStamp; char b[40]; snprintf(b, sizeof b, age < 1 ? "just now" : age < 120 ? "%lld min ago" : "%lld h ago", age < 120 ? age : age / 60); text(b, F16, LX + LWD - 16, 26, DARK, 2, 130); }
    std::vector<int> L; for (int i = 0; i < (int)gMail.size(); i++) if (gMFilter == 0 || !gMail[i].seen) L.push_back(i);
    int top = 62, rowH = 82, bot = UIH;
    int maxS = std::max(0, (int)L.size() * rowH - (bot - top));
    gMScroll = std::max(0, std::min(gMScroll, maxS));
    SDL_Rect cl = {LX, top, LWD, bot - top}; SDL_RenderSetClipRect(gR, &cl);
    for (int k = 0; k < (int)L.size(); k++) {
        int y = top + k * rowH - gMScroll;
        if (y + rowH < top || y > bot) continue;
        const MailMsg& m = gMail[L[k]];
        bool sel = L[k] == gMSel;
        rect(LX, y, LWD, rowH - 1, sel ? Col{200, 224, 245} : Col{255, 255, 255});
        if (!m.seen) rect(LX, y, 5, rowH - 1, MAIL_BLUE);
        text(fitTxt(m.from, F22, 270), F22, LX + 18, y + 8, m.seen ? DARK : MAIL_BLUE);
        text(m.time.empty() ? m.date : (nowMinutes() >= 0 ? m.date : m.time), F16, LX + LWD - 14, y + 12, DARK, 2, 140);
        text(fitTxt(m.subj, F16, LWD - 40), F16, LX + 18, y + 42, m.seen ? Col{90, 90, 95} : DARK);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (L.empty()) {
        text(gMBusy ? "Checking mail..." : "Nothing here", F30, LX + LWD / 2, 280, DARK, 1, 150);
        if (!gMMsg.empty()) { auto W2 = wrapText(gMMsg, F16, LWD - 40); for (size_t i = 0; i < W2.size() && i < 4; i++) text(W2[i], F16, LX + LWD / 2, 330 + (int)i * 24, Col{180, 60, 40}, 1); }
    }
    rect(RX - 1, 0, 1, UIH, Col{215, 215, 218});
    // reading pane
    int rx = RX + 30, rw = W - RX - 60;
    if (gMSel < 0 || gMSel >= (int)gMail.size()) {
        text("Select an item to read", F30, RX + (W - RX) / 2, 280, DARK, 1, 120);
        if (!gMMsg.empty() && !L.empty()) text(fitTxt(gMMsg, F16, rw), F16, rx, UIH - 40, Col{150, 60, 40});
        return;
    }
    const MailMsg& m = gMail[gMSel];
    auto SL = wrapText(m.subj, F30, rw);
    int y = 20;
    for (size_t i = 0; i < SL.size() && i < 2; i++) { text(SL[i], F30, rx, y, DARK); y += 40; }
    text(fitTxt(m.from, F22, rw - 200), F22, rx, y + 6, MAIL_BLUE);
    text(m.date + "  " + m.time, F16, rx + rw, y + 12, DARK, 2, 140);
    text(fitTxt(m.addr, F16, rw), F16, rx, y + 38, DARK, 0, 130);
    rect(rx + rw - 110, y + 36, 110, 34, MAIL_BLUE); text("Reply", F16, rx + rw - 55, y + 41, WHITE, 1);
    y += 82;
    rect(rx, y - 6, rw, 1, Col{215, 215, 218});
    SDL_Rect cl2 = {RX, y, W - RX, UIH - y}; SDL_RenderSetClipRect(gR, &cl2);
    if (!m.haveBody) text(gMBusy ? "Downloading..." : (gMMsg.empty() ? "Tap the message again to load it" : gMMsg), F22, rx, y + 10, DARK, 0, 150);
    else {
        auto BL = wrapText(m.body, F22, rw);
        int maxB = std::max(0, (int)BL.size() * 30 - (UIH - y - 20));
        gMBodyScroll = std::max(0, std::min(gMBodyScroll, maxB));
        for (size_t i = 0; i < BL.size(); i++) { int yy = y + 10 + (int)i * 30 - gMBodyScroll; if (yy + 30 < y || yy > UIH) continue; text(BL[i], F22, rx, yy, Col{40, 40, 45}); }
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static void mailTouch(int phase, int x, int y) { (void)phase; (void)y; gMTx = x; }
static void mailDrag(int dy) { if (gMView != 0) return; if (gMTx < 230 + 400) gMScroll -= dy; else gMBodyScroll -= dy; }
static void mailTap(int x, int y) {
    int LW = 230;
    if (x < LW) {
        if (inR(x, y, 0, 118, LW, 48)) { gMView = 0; gMFilter = 0; hideKb(); }
        else if (inR(x, y, 0, 170, LW, 48)) { gMView = 0; gMFilter = 1; hideKb(); }
        else if (inR(x, y, 20, 236, LW - 40, 50)) { if (gMAddr.empty()) gMView = 2; else { gMView = 1; gMMsg.clear(); } }
        else if (inR(x, y, 0, 300, LW, 44)) { gMView = 0; hideKb(); mailRefresh(); }
        else if (inR(x, y, 0, 348, LW, 44)) { gMView = 2; gMMsg.clear(); }
        return;
    }
    if (gMView == 2) {
        int bx = LW + 50;
        if (inR(x, y, bx, 110, 700, 52)) openKbStr(&gMAddr, 80);
        else if (inR(x, y, bx, 172, 700, 52)) openKbStr(&gMPass, 80);
        else if (inR(x, y, bx, 234, 700, 52)) openKbStr(&gMImap, 80);
        else if (inR(x, y, bx, 296, 700, 52)) openKbStr(&gMSmtp, 90);
        else if (inR(x, y, W - 300, 20, 270, 54)) { if (gMAddr.find('@') == std::string::npos || gMPass.empty()) gMMsg = "Enter your address and password first."; else mailSaveGo(); }
        return;
    }
    if (gMView == 1) {
        int bx = LW + 50;
        if (inR(x, y, bx, 96, 780, 50)) openKbStr(&gMTo, 80);
        else if (inR(x, y, bx, 160, 780, 50)) openKbStr(&gMSubj, 120);
        else if (inR(x, y, bx, 224, 780, kbOpen ? 130 : 330)) openKbStr(&gMBody, 4000, nullptr, true);
        else if (inR(x, y, W - 380, 22, 150, 50)) { hideKb(); mailSend(); }
        else if (inR(x, y, W - 220, 22, 150, 50)) { hideKb(); gMView = 0; }
        return;
    }
    int LX = LW, LWD = 400, RX = LW + LWD;
    if (x < RX) {
        if (y < 62) return;
        std::vector<int> L; for (int i = 0; i < (int)gMail.size(); i++) if (gMFilter == 0 || !gMail[i].seen) L.push_back(i);
        int k = (y - 62 + gMScroll) / 82;
        if (k >= 0 && k < (int)L.size()) mailOpenMsg(L[k]);
        (void)LX;
        return;
    }
    if (gMSel >= 0 && gMSel < (int)gMail.size()) {
        int rx = RX + 30, rw = W - RX - 60;
        auto SL = wrapText(gMail[gMSel].subj, F30, rw);
        int yy = 20 + 40 * (int)std::min<size_t>(2, SL.size());
        if (inR(x, y, rx + rw - 110, yy + 36, 110, 34)) {
            gMTo = gMail[gMSel].addr; gMSubj = gMail[gMSel].subj.compare(0, 3, "Re:") == 0 ? gMail[gMSel].subj : "Re: " + gMail[gMSel].subj;
            gMBody = "\n\n--- " + gMail[gMSel].from + " wrote:\n" + gMail[gMSel].body.substr(0, 600);
            gMView = 1; gMMsg.clear();
        }
    }
}

// ================================================================ News (RSS over Wi-Fi) and Roblox companion
struct NewsItem { std::string title, desc, link, date; };
struct NewsFeed { const char* name; const char* url; };
static const NewsFeed NFEEDS[4] = {
    {"BBC World", "https://feeds.bbci.co.uk/news/world/rss.xml"}, {"Al Jazeera", "https://www.aljazeera.com/xml/rss/all.xml"},
    {"The Verge", "https://www.theverge.com/rss/index.xml"}, {"Hacker News", "https://news.ycombinator.com/rss"}};
static std::vector<NewsItem> gNews;
static int gNewsFeed = 0, gNewsSel = -1, gNewsScroll = 0, gNewsBodyScroll = 0;
static bool gNewsBusy = false; static std::string gNewsMsg;
static int gNewsFetching = 0;
static std::string xmlTag(const std::string& s, const char* tag, size_t from, size_t* endp = nullptr) {
    std::string o = std::string("<") + tag, c = std::string("</") + tag + ">";
    size_t a = s.find(o, from);
    while (a != std::string::npos && a + o.size() < s.size() && s[a + o.size()] != '>' && s[a + o.size()] != ' ') a = s.find(o, a + 1);
    if (a == std::string::npos) return "";
    size_t b = s.find('>', a);
    size_t e = s.find(c, b);
    if (b == std::string::npos || e == std::string::npos) return "";
    if (endp) *endp = e + c.size();
    std::string v = s.substr(b + 1, e - b - 1);
    size_t cd = v.find("<![CDATA[");
    if (cd != std::string::npos) { size_t ce = v.find("]]>", cd); v = v.substr(cd + 9, ce == std::string::npos ? std::string::npos : ce - cd - 9); }
    return v;
}
static bool newsParse(const std::string& x) {
    std::vector<NewsItem> v;
    size_t p = 0;
    while (v.size() < 40) {
        size_t a = x.find("<item", p);
        if (a == std::string::npos) break;
        size_t b = x.find("</item>", a);
        if (b == std::string::npos) break;
        std::string it = x.substr(a, b - a);
        NewsItem n;
        n.title = htmlToText(xmlTag(it, "title", 0)); n.link = xmlTag(it, "link", 0); n.date = xmlTag(it, "pubDate", 0);
        n.desc = htmlToText(xmlTag(it, "description", 0));
        while (!n.title.empty() && n.title.back() == '\n') n.title.pop_back();
        if (n.date.size() > 22) n.date = n.date.substr(0, 22);
        if (!n.title.empty()) v.push_back(n);
        p = b + 7;
    }
    if (v.empty()) return false;
    gNews = v; return true;
}
static void newsDone(NetJob& j) {
    gNewsBusy = false;
    if (j.ok && newsParse(j.out)) { writeFile(DIR_ + "news_" + std::to_string(atoi(j.a.c_str())) + ".xml", j.out); gNewsMsg.clear(); }
    else gNewsMsg = "Couldn't load this feed.";
}
static void newsLoadFeed(int f, bool online) {
    gNewsFeed = f; gNewsSel = -1; gNewsScroll = 0;
    std::string d;
    if (readFile(DIR_ + "news_" + std::to_string(f) + ".xml", d)) newsParse(d); else gNews.clear();
    if (!online || gNewsBusy) return;
    if (!netUp()) { gNewsMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob(); j->url = NFEEDS[f].url; j->a = std::to_string(f); j->done = newsDone;
    if (netStart(j)) { gNewsBusy = true; gNewsMsg = "Updating..."; }
}
static void newsInit() { newsLoadFeed(gNewsFeed, true); }
static void drawNews() {
    Col red = {196, 38, 38};
    rect(0, 0, W, UIH, Col{245, 245, 246});
    rect(0, 0, W, 78, red);
    text("News", F44, 40, 16, WHITE);
    for (int i = 0; i < 4; i++) {
        bool on = i == gNewsFeed;
        int x = 260 + i * 240;
        if (on) rect(x, 62, 220, 6, WHITE);
        text(NFEEDS[i].name, F22, x + 110, 20, WHITE, 1, on ? 255 : 190);
    }
    if (gNewsBusy) { float t = SDL_GetTicks() / 1000.0f; for (int i = 0; i < 8; i++) { float a = t * 6 + i * 0.78f; fdiscA(W - 40 + cosf(a) * 12, 38 + sinf(a) * 12, 2.6f, WHITE, 255 - i * 28); } }
    int LW = 480;
    int top = 78, rowH = 104;
    int maxS = std::max(0, (int)gNews.size() * rowH - (UIH - top));
    gNewsScroll = std::max(0, std::min(gNewsScroll, maxS));
    SDL_Rect cl = {0, top, LW, UIH - top}; SDL_RenderSetClipRect(gR, &cl);
    for (int i = 0; i < (int)gNews.size(); i++) {
        int y = top + i * rowH - gNewsScroll;
        if (y + rowH < top || y > UIH) continue;
        rect(0, y, LW, rowH - 1, i == gNewsSel ? Col{225, 232, 242} : Col{255, 255, 255});
        auto L = wrapText(gNews[i].title, F22, LW - 36);
        for (size_t k = 0; k < L.size() && k < 2; k++) text(L[k], F22, 18, y + 8 + (int)k * 30, Col{30, 30, 34});
        text(gNews[i].date, F16, 18, y + 74, Col{130, 130, 138});
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (gNews.empty()) {
        text(gNewsBusy ? "Loading headlines..." : (gNewsMsg.empty() ? "No headlines yet" : gNewsMsg), F30, LW / 2, 300, Col{120, 120, 128}, 1);
        if (!gNewsBusy) text("Connect to Wi-Fi and tap a feed", F22, LW / 2, 350, Col{150, 150, 158}, 1);
    }
    rect(LW, top, 1, UIH - top, Col{215, 215, 218});
    if (gNewsSel < 0 || gNewsSel >= (int)gNews.size()) { text("Pick a story", F30, LW + (W - LW) / 2, 320, Col{150, 150, 158}, 1); return; }
    const NewsItem& n = gNews[gNewsSel];
    int rx = LW + 34, rw = W - LW - 68, y = top + 20;
    auto T = wrapText(n.title, F30, rw);
    for (size_t k = 0; k < T.size() && k < 3; k++) { text(T[k], F30, rx, y, Col{30, 30, 34}); y += 40; }
    text(n.date, F16, rx, y + 4, Col{130, 130, 138});
    rect(rx, y + 36, rw, 46, red); text("Open full story", F22, rx + rw / 2, y + 44, WHITE, 1);
    y += 100;
    SDL_Rect c2 = {LW, y, W - LW, UIH - y}; SDL_RenderSetClipRect(gR, &c2);
    auto B = wrapText(n.desc.empty() ? "(No summary in this feed. Use Open full story.)" : n.desc, F22, rw);
    int maxB = std::max(0, (int)B.size() * 30 - (UIH - y - 20));
    gNewsBodyScroll = std::max(0, std::min(gNewsBodyScroll, maxB));
    for (size_t k = 0; k < B.size(); k++) text(B[k], F22, rx, y + (int)k * 30 - gNewsBodyScroll, Col{50, 50, 56});
    SDL_RenderSetClipRect(gR, nullptr);
}
static int gNewsTx = 0;
static void newsDrag(int dy) { if (gNewsTx < 480) gNewsScroll -= dy; else gNewsBodyScroll -= dy; }
static void newsTap(int x, int y) {
    if (y < 78) { for (int i = 0; i < 4; i++) if (inR(x, y, 260 + i * 240, 0, 220, 78)) newsLoadFeed(i, true); return; }
    if (x < 480) { int k = (y - 78 + gNewsScroll) / 104; if (k >= 0 && k < (int)gNews.size()) { gNewsSel = k; gNewsBodyScroll = 0; } return; }
    if (gNewsSel >= 0 && gNewsSel < (int)gNews.size()) {
        int rw = W - 480 - 68, yy = 98;
        auto T = wrapText(gNews[gNewsSel].title, F30, rw);
        yy += 40 * (int)std::min<size_t>(3, T.size());
        if (inR(x, y, 514, yy + 36, rw, 46)) openUrl(gNews[gNewsSel].link);
    }
}
// ---------------------------------------------------------------- Roblox companion (player lookup + a block builder)
static const int RB_W = 26, RB_H = 12, RB_C = 10;
static int gRbTab = 0, gRbCol = 1, gRbTool = 0;
static unsigned char gRbGrid[RB_H][RB_W];
static std::string gRbName, gRbRes[4], gRbMsg;
static bool gRbBusy = false, gRbHave = false;
static const Col RBP[RB_C] = {{0, 0, 0}, {226, 35, 26}, {255, 150, 30}, {255, 214, 20}, {70, 190, 60}, {0, 170, 190}, {30, 110, 230}, {140, 80, 220}, {240, 240, 240}, {120, 84, 56}};
static void rbSave() { std::string o; for (int y = 0; y < RB_H; y++) { for (int x = 0; x < RB_W; x++) o += (char)('0' + gRbGrid[y][x]); o += '\n'; } writeFile(DIR_ + "blocks.txt", o); }
static void rbLoad() {
    memset(gRbGrid, 0, sizeof gRbGrid);
    std::string d; if (!readFile(DIR_ + "blocks.txt", d)) return;
    size_t p = 0;
    for (int y = 0; y < RB_H && p < d.size(); y++) { for (int x = 0; x < RB_W && p < d.size() && d[p] != '\n'; x++, p++) gRbGrid[y][x] = (unsigned char)std::max(0, std::min(RB_C - 1, d[p] - '0')); while (p < d.size() && d[p] != '\n') p++; p++; }
}
static void rbDone(NetJob& j) {
    gRbBusy = false;
    if (!j.ok) { gRbMsg = "Couldn't reach Roblox. Check Wi-Fi."; return; }
    size_t p = j.out.find("\"data\"");
    if (p == std::string::npos || j.out.find("\"id\"", p) == std::string::npos) { gRbMsg = "No player found."; gRbHave = false; return; }
    gRbRes[0] = jstr(j.out, "displayName", p); gRbRes[1] = "@" + jstr(j.out, "name", p);
    gRbRes[2] = "User ID " + std::to_string((long long)jnum(j.out, "id", p));
    gRbRes[3] = jstr(j.out, "hasVerifiedBadge", p).empty() && j.out.find("\"hasVerifiedBadge\":true") != std::string::npos ? "Verified" : "";
    gRbHave = true; gRbMsg.clear();
}
static void rbSearch() {
    if (gRbName.empty() || gRbBusy) return;
    if (!netUp()) { gRbMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob(); j->url = "https://users.roblox.com/v1/users/search?limit=10&keyword=" + urlEncode(gRbName); j->done = rbDone;
    if (netStart(j)) { gRbBusy = true; gRbMsg = "Searching..."; }
}
static void robloxInit() { rbLoad(); }
static void drawRoblox() {
    Col red = {226, 35, 26};
    rect(0, 0, W, UIH, Col{36, 38, 44});
    rect(0, 0, W, 86, Col{24, 26, 30});
    text("Roblox", F44, 40, 18, WHITE);
    text("Companion", F16, 214, 44, WHITE, 0, 150);
    for (int i = 0; i < 2; i++) { bool on = gRbTab == i; rect(W - 460 + i * 220, 20, 200, 46, on ? red : Col{60, 62, 70}); text(i ? "Block builder" : "Players", F22, W - 360 + i * 220, 28, WHITE, 1); }
    if (gRbTab == 0) {
        text("The real Roblox game can't run on the Switch, so this app brings the useful parts:", F22, 70, 120, WHITE, 0, 200);
        text("look up any player, and open Roblox in the Switch web browser.", F22, 70, 154, WHITE, 0, 200);
        drawField(70, 210, 820, 58, &gRbName, "Player name", F30, DARK, WHITE, Col{200, 200, 200});
        rect(910, 210, 150, 58, red); text("Search", F22, 985, 224, WHITE, 1);
        rect(1080, 210, 130, 58, Col{70, 72, 80}); text("Site", F22, 1145, 224, WHITE, 1);
        if (gRbHave) {
            rect(70, 300, 1140, 150, Col{255, 255, 255}, 24);
            disc(160, 375, 52, red); text(gRbRes[0].empty() ? "?" : gRbRes[0].substr(0, 1), F72, 160, 335, WHITE, 1);
            text(gRbRes[0], F44, 240, 322, WHITE); text(gRbRes[1], F22, 242, 378, WHITE, 0, 200);
            text(gRbRes[2] + (gRbRes[3].empty() ? "" : "   " + gRbRes[3]), F22, 242, 410, WHITE, 0, 160);
        }
        if (!gRbMsg.empty()) text(gRbMsg, F22, 70, 480, Col{255, 214, 120});
        text("Tip: Roblox opens in the Switch web applet; sign in there to play in the browser if your region allows it.", F16, 70, 520, WHITE, 0, 130);
    } else {
        int cs = 40, gx = 70, gy = 100;
        rect(gx - 6, gy - 6, RB_W * cs / 2 * 0 + RB_W * 36 + 12, RB_H * 36 + 12, Col{20, 22, 26});
        cs = 36;
        for (int y = 0; y < RB_H; y++) for (int x = 0; x < RB_W; x++) {
            int v = gRbGrid[y][x];
            rect(gx + x * cs, gy + y * cs, cs - 2, cs - 2, v ? RBP[v] : Col{50, 52, 60});
            if (v) { rect(gx + x * cs, gy + y * cs, cs - 2, 4, WHITE, 60); rect(gx + x * cs, gy + y * cs + cs - 8, cs - 2, 6, BLACK, 60); }
        }
        for (int i = 1; i < RB_C; i++) { rect(gx + (i - 1) * 56, 640 - 52 + 0, 48, 40, RBP[i]); if (gRbCol == i && gRbTool == 0) outline(gx + (i - 1) * 56 - 3, 640 - 55, 54, 46, 3, WHITE); }
        rect(gx + 9 * 56, 588, 100, 40, Col{70, 72, 80}); text(gRbTool == 1 ? "[Erase]" : "Erase", F22, gx + 9 * 56 + 50, 594, WHITE, 1);
        rect(gx + 9 * 56 + 110, 588, 100, 40, Col{70, 72, 80}); text("Clear", F22, gx + 9 * 56 + 160, 594, WHITE, 1);
    }
}
static void rbPlace(int x, int y) {
    int cs = 36, gx = 70, gy = 100;
    int cx = (x - gx) / cs, cy = (y - gy) / cs;
    if (x >= gx && y >= gy && cx < RB_W && cy < RB_H) { gRbGrid[cy][cx] = gRbTool == 1 ? 0 : (unsigned char)gRbCol; rbSave(); }
}
static void robloxTouch(int phase, int x, int y) { if (gRbTab == 1 && phase == 1) rbPlace(x, y); }
static void robloxTap(int x, int y) {
    for (int i = 0; i < 2; i++) if (inR(x, y, W - 460 + i * 220, 20, 200, 46)) { gRbTab = i; hideKb(); return; }
    if (gRbTab == 0) {
        if (inR(x, y, 70, 210, 820, 58)) openKbStr(&gRbName, 30, rbSearch);
        else if (inR(x, y, 910, 210, 150, 58)) { hideKb(); rbSearch(); }
        else if (inR(x, y, 1080, 210, 130, 58)) openUrl("https://www.roblox.com");
        return;
    }
    int gx = 70;
    for (int i = 1; i < RB_C; i++) if (inR(x, y, gx + (i - 1) * 56, 588, 48, 40)) { gRbCol = i; gRbTool = 0; return; }
    if (inR(x, y, gx + 9 * 56, 588, 100, 40)) { gRbTool = 1 - gRbTool; return; }
    if (inR(x, y, gx + 9 * 56 + 110, 588, 100, 40)) { memset(gRbGrid, 0, sizeof gRbGrid); rbSave(); return; }
    rbPlace(x, y);
}



// ================================================================ Calendar (events with times, repeats, reminders)
static const Col EVCOL[6] = {{255, 214, 90}, {255, 120, 120}, {110, 220, 140}, {90, 200, 255}, {255, 160, 60}, {220, 140, 255}};
static const char* REPN[5] = {"None", "Daily", "Weekly", "Monthly", "Yearly"};
static const int REMV[5] = {-1, 0, 5, 15, 60};
static const char* REMN[5] = {"None", "At time", "5 min", "15 min", "1 hour"};
static bool calIsToday(int y, int m, int d) { return y == gNow.y && m == gNow.mo && d == gNow.d; }
static long dayNum(int y, int m, int d) {
    y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}
static void calStepDay(int& y, int& m, int& d, int n) {
    d += n;
    while (d < 1) { m--; if (m < 1) { m = 12; y--; } d += daysIn(y, m); }
    while (d > daysIn(y, m)) { d -= daysIn(y, m); m++; if (m > 12) { m = 1; y++; } }
}
static bool calOccurs(const CalEvent& e, int y, int m, int d) {
    long a = dayNum(e.y, e.m, e.d), b = dayNum(y, m, d);
    if (b < a) return false;
    switch (e.rep) {
        case 0: return b == a;
        case 1: return true;
        case 2: return (b - a) % 7 == 0;
        case 3: return d == std::min(e.d, daysIn(y, m));
        default: return m == e.m && d == std::min(e.d, daysIn(y, m));
    }
}
static std::string hhmm(int h, int m) {
    char b[24];
    if (g24) snprintf(b, sizeof b, "%02d:%02d", h, m);
    else { int hh = h % 12; if (!hh) hh = 12; snprintf(b, sizeof b, "%d:%02d%s", hh, m, h >= 12 ? "p" : "a"); }
    return b;
}
static void calSave() {
    std::string d = "#v2\n";
    for (auto& e : gCal) {
        auto cl = [](std::string s) { for (auto& c : s) if (c == '|' || c == '\n' || c == '\r') c = ' '; return s; };
        char b[160];
        snprintf(b, sizeof b, "%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|", e.y, e.m, e.d, e.sh, e.sm, e.dur, e.allday ? 1 : 0, e.rep, e.rem, e.col);
        d += std::string(b) + cl(e.title) + "|" + cl(e.place) + "\n";
    }
    writeFile(DIR_ + "calendar.txt", d);
}
static void calEnsure() {
    if (gCalLoaded) return;
    gCalLoaded = true; gCal.clear();
    std::string d;
    if (!readFile(DIR_ + "calendar.txt", d)) return;
    size_t p = 0;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(p, e - p); p = e + 1;
        if (ln.empty() || ln[0] == '#') continue;
        CalEvent ev;
        if (ln.size() > 11 && ln[4] == '-' && ln[7] == '-' && ln[10] == '|') { // old format: date|text
            ev.y = atoi(ln.c_str()); ev.m = atoi(ln.c_str() + 5); ev.d = atoi(ln.c_str() + 8); ev.allday = true; ev.title = ln.substr(11);
            ev.id = gCalNextId++; gCal.push_back(ev); continue;
        }
        std::vector<std::string> f; size_t q = 0;
        for (int i = 0; i < 10; i++) { size_t t = ln.find('|', q); if (t == std::string::npos) break; f.push_back(ln.substr(q, t - q)); q = t + 1; }
        if (f.size() < 10) continue;
        size_t t = ln.find('|', q);
        ev.title = ln.substr(q, t == std::string::npos ? std::string::npos : t - q);
        if (t != std::string::npos) ev.place = ln.substr(t + 1);
        ev.y = atoi(f[0].c_str()); ev.m = atoi(f[1].c_str()); ev.d = atoi(f[2].c_str()); ev.sh = atoi(f[3].c_str()); ev.sm = atoi(f[4].c_str());
        ev.dur = atoi(f[5].c_str()); ev.allday = atoi(f[6].c_str()) != 0; ev.rep = std::max(0, std::min(4, atoi(f[7].c_str())));
        ev.rem = atoi(f[8].c_str()); ev.col = std::max(0, std::min(5, atoi(f[9].c_str())));
        ev.id = gCalNextId++; gCal.push_back(ev);
    }
}
static void calLoad() { calEnsure(); calY = gNow.y; calM = gNow.mo; calSel = gNow.d; calEdit = false; calView = 0; calScroll = 0; }
static void calAdd() { hideKb(); }
static void calMonth(int d) {
    calM += d;
    if (calM < 1) { calM = 12; calY--; } else if (calM > 12) { calM = 1; calY++; }
    calSel = std::min(calSel, daysIn(calY, calM));
}
static std::vector<int> calDayList(int y, int m, int d) {
    calEnsure();
    std::vector<int> v;
    for (int i = 0; i < (int)gCal.size(); i++) if (calOccurs(gCal[i], y, m, d)) v.push_back(i);
    std::sort(v.begin(), v.end(), [](int a, int b) {
        const CalEvent &x = gCal[a], &y2 = gCal[b];
        if (x.allday != y2.allday) return x.allday;
        return x.sh * 60 + x.sm < y2.sh * 60 + y2.sm;
    });
    return v;
}
static std::string calTileLine() {
    auto v = calDayList(gNow.y, gNow.mo, gNow.d);
    int nowM = gNow.h * 60 + gNow.mi;
    for (int i : v) {
        const CalEvent& e = gCal[i];
        if (e.allday) return e.title;
        if (e.sh * 60 + e.sm + std::max(15, e.dur) >= nowM) return hhmm(e.sh, e.sm) + "  " + e.title;
    }
    return v.empty() ? "No events today" : "Done for today";
}
static void calTick() {
    static Uint32 last = 0; Uint32 n = SDL_GetTicks();
    if (n - last < 5000) return;
    last = n; calEnsure();
    int nowM = gNow.h * 60 + gNow.mi;
    for (auto& e : gCal) {
        if (e.rem < 0 || e.allday || !calOccurs(e, gNow.y, gNow.mo, gNow.d)) continue;
        int st = e.sh * 60 + e.sm;
        if (nowM < st - e.rem || nowM > st) continue;
        char key[48]; snprintf(key, sizeof key, "%d:%04d%02d%02d", e.id, gNow.y, gNow.mo, gNow.d);
        if (gCalFired.count(key)) continue;
        gCalFired.insert(key);
        notify(A_CALENDAR, e.title, "Starts " + hhmm(e.sh, e.sm) + (e.place.empty() ? "" : " - " + e.place));
    }
}
// ---- editor
struct CalHit { SDL_Rect r; int id; };
static std::vector<CalHit> gCalHits;
static void calHit(int x, int y, int w, int h, int id) { gCalHits.push_back({SDL_Rect{x, y, w, h}, id}); }
static void calNewEvent() {
    calE = CalEvent(); calE.y = calY; calE.m = calM; calE.d = calSel;
    int h = gNow.h + 1; if (!calIsToday(calY, calM, calSel)) h = 9;
    calE.sh = std::min(h, 23); calE.sm = 0; calE.dur = 60; calE.rem = 15; calE.col = (int)(gCal.size() % 6);
    calEi = -1; calEdit = true;
}
static void calEditEvent(int idx) { calE = gCal[idx]; calEi = idx; calEdit = true; }
static void calSaveEdit() {
    if (calE.title.empty()) calE.title = "(No title)";
    if (calEi >= 0 && calEi < (int)gCal.size()) gCal[calEi] = calE;
    else { calE.id = gCalNextId++; gCal.push_back(calE); }
    gCalFired.clear();
    calSave(); calEdit = false; hideKb();
    calY = calE.y; calM = calE.m; calSel = calE.d;
}
static bool calBack() { if (calEdit) { calEdit = false; hideKb(); return true; } return false; }
static void drawCalEditor() {
    gCalHits.clear();
    Col bg = CAL_PURPLE;
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(Col{120, 78, 224}, Col{92, 56, 190}, y / (float)UIH));
    text(calEi >= 0 ? "Edit event" : "New event", F44, 56, 20, WHITE);
    drawField(56, 90, 760, 52, &calE.title, "Event name", F30, DARK, WHITE, Col{255, 255, 255});
    calHit(56, 90, 760, 52, 1);
    drawField(56, 152, 760, 44, &calE.place, "Place (optional)", F22, DARK, WHITE, Col{255, 255, 255});
    calHit(56, 152, 760, 44, 2);
    // date
    char db[80]; snprintf(db, sizeof db, "%s, %d %s %d", WD[dow(calE.y, calE.m, calE.d)], calE.d, MONTHS[calE.m - 1], calE.y);
    text(db, F30, 56, 214, WHITE);
    btn(560, 208, 60, 44, "-1d", Col{90, 56, 190}, WHITE); calHit(560, 208, 60, 44, 10);
    btn(626, 208, 60, 44, "+1d", Col{90, 56, 190}, WHITE); calHit(626, 208, 60, 44, 11);
    btn(692, 208, 60, 44, "-1m", Col{90, 56, 190}, WHITE); calHit(692, 208, 60, 44, 12);
    btn(758, 208, 60, 44, "+1m", Col{90, 56, 190}, WHITE); calHit(758, 208, 60, 44, 13);
    // time
    rect(56, 268, 130, 44, calE.allday ? Col{255, 255, 255} : Col{90, 56, 190});
    text("All day", F22, 121, 277, calE.allday ? bg : WHITE, 1); calHit(56, 268, 130, 44, 20);
    if (!calE.allday) {
        text(hhmm(calE.sh, calE.sm), F44, 216, 262, WHITE);
        btn(386, 268, 50, 44, "h-", Col{90, 56, 190}, WHITE); calHit(386, 268, 50, 44, 21);
        btn(440, 268, 50, 44, "h+", Col{90, 56, 190}, WHITE); calHit(440, 268, 50, 44, 22);
        btn(494, 268, 50, 44, "m-", Col{90, 56, 190}, WHITE); calHit(494, 268, 50, 44, 23);
        btn(548, 268, 50, 44, "m+", Col{90, 56, 190}, WHITE); calHit(548, 268, 50, 44, 24);
        static const int DV[3] = {30, 60, 120}; static const char* DN[3] = {"30 min", "1 h", "2 h"};
        for (int i = 0; i < 3; i++) {
            rect(620 + i * 68, 268, 64, 44, calE.dur == DV[i] ? Col{255, 255, 255} : Col{90, 56, 190});
            text(DN[i], F16, 652 + i * 68, 280, calE.dur == DV[i] ? bg : WHITE, 1); calHit(620 + i * 68, 268, 64, 44, 30 + i);
        }
    }
    text("Repeat", F22, 56, 340, WHITE, 0, 200);
    for (int i = 0; i < 5; i++) {
        int x = 56 + i * 124;
        rect(x, 372, 118, 48, calE.rep == i ? Col{255, 255, 255} : Col{90, 56, 190});
        text(REPN[i], F22, x + 59, 382, calE.rep == i ? bg : WHITE, 1); calHit(x, 372, 118, 48, 40 + i);
    }
    text("Reminder", F22, 56, 432, WHITE, 0, 200);
    for (int i = 0; i < 5; i++) {
        int x = 56 + i * 124;
        rect(x, 462, 118, 48, calE.rem == REMV[i] ? Col{255, 255, 255} : Col{90, 56, 190});
        text(REMN[i], F22, x + 59, 472, calE.rem == REMV[i] ? bg : WHITE, 1); calHit(x, 462, 118, 48, 50 + i);
    }
    for (int i = 0; i < 6; i++) {
        disc(80 + i * 64, 556, 22, EVCOL[i]);
        if (calE.col == i) ring(80 + i * 64, 556, 27, 3, WHITE, bg);
        calHit(56 + i * 64, 530, 52, 52, 60 + i);
    }
    btn(56, 598, 200, 54, "Save", Col{255, 255, 255}, bg); calHit(56, 598, 200, 54, 70);
    btn(272, 598, 200, 54, "Cancel", Col{90, 56, 190}, WHITE); calHit(272, 598, 200, 54, 72);
    if (calEi >= 0) { btn(488, 598, 200, 54, "Delete", Col{190, 40, 60}, WHITE); calHit(488, 598, 200, 54, 71); }
}
static void calEditorTap(int x, int y) {
    if (kbOpen && y >= KBTOP) return;
    int id = 0;
    for (auto& h : gCalHits) if (inR(x, y, h.r.x, h.r.y, h.r.w, h.r.h)) { id = h.id; break; }
    if (kbOpen && id != 1 && id != 2) hideKb();
    switch (id) {
        case 1: openKbStr(&calE.title, 60); break;
        case 2: openKbStr(&calE.place, 60); break;
        case 10: calStepDay(calE.y, calE.m, calE.d, -1); break;
        case 11: calStepDay(calE.y, calE.m, calE.d, 1); break;
        case 12: calStepDay(calE.y, calE.m, calE.d, -daysIn(calE.m == 1 ? calE.y - 1 : calE.y, calE.m == 1 ? 12 : calE.m - 1)); break;
        case 13: calStepDay(calE.y, calE.m, calE.d, daysIn(calE.y, calE.m)); break;
        case 20: calE.allday = !calE.allday; break;
        case 21: calE.sh = (calE.sh + 23) % 24; break;
        case 22: calE.sh = (calE.sh + 1) % 24; break;
        case 23: calE.sm = (calE.sm + 55) % 60; break;
        case 24: calE.sm = (calE.sm + 5) % 60; break;
        case 30: calE.dur = 30; break; case 31: calE.dur = 60; break; case 32: calE.dur = 120; break;
        case 70: calSaveEdit(); break;
        case 71: if (calEi >= 0 && calEi < (int)gCal.size()) { gCal.erase(gCal.begin() + calEi); calSave(); } calEdit = false; hideKb(); break;
        case 72: calEdit = false; hideKb(); break;
        default:
            if (id >= 40 && id < 45) calE.rep = id - 40;
            else if (id >= 50 && id < 55) calE.rem = REMV[id - 50];
            else if (id >= 60 && id < 66) calE.col = id - 60;
    }
}
static void drawCalendar() {
    if (calEdit) { drawCalEditor(); return; }
    Col bg = CAL_PURPLE;
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(Col{120, 78, 224}, Col{92, 56, 190}, y / (float)UIH));
    text(MONTHS[calM - 1], F72, 56, 14, WHITE);
    text(std::to_string(calY), F44, 56 + tw(MONTHS[calM - 1], F72) + 24, 44, WHITE, 0, 200);
    ring(1030, 62, 26, 3, WHITE, bg); line(1038, 62, 1022, 62, 3, WHITE); line(1022, 62, 1030, 54, 3, WHITE); line(1022, 62, 1030, 70, 3, WHITE);
    ring(1100, 62, 26, 3, WHITE, bg); line(1092, 62, 1108, 62, 3, WHITE); line(1108, 62, 1100, 54, 3, WHITE); line(1108, 62, 1100, 70, 3, WHITE);
    btn(1148, 42, 100, 40, "Today", Col{90, 56, 190}, WHITE);
    for (int i = 0; i < 2; i++) {
        rect(640 + i * 124, 42, 118, 40, calView == i ? Col{255, 255, 255} : Col{90, 56, 190});
        text(i ? "Agenda" : "Month", F22, 699 + i * 124, 48, calView == i ? bg : WHITE, 1);
    }
    if (calView == 0) {
        int ws = gWeekStart ? 1 : 0;
        for (int c = 0; c < 7; c++) text(WD3[(c + ws) % 7], F22, 56 + c * 128 + 62, 128, WHITE, 1, 200);
        int first = (dow(calY, calM, 1) + 7 - ws) % 7, dim = daysIn(calY, calM);
        int pm = calM == 1 ? 12 : calM - 1, py = calM == 1 ? calY - 1 : calY;
        int pdim = daysIn(py, pm);
        for (int idx = 0; idx < 42; idx++) {
            int col = idx % 7, row = idx / 7;
            int x = 56 + col * 128, y = CAL_Y + row * CAL_RH - 4;
            int d = idx - first + 1;
            bool inMonth = d >= 1 && d <= dim;
            int shown = inMonth ? d : (d < 1 ? pdim + d : d - dim);
            bool today = inMonth && calIsToday(calY, calM, d);
            bool selc = inMonth && d == calSel;
            rect(x, y, 124, 76, WHITE, today ? 255 : (inMonth ? 34 : 12));
            if (selc) outline(x, y, 124, 76, 3, WHITE);
            text(std::to_string(shown), F22, x + 8, y + 2, today ? bg : WHITE, 0, inMonth ? 255 : 110);
            if (inMonth) {
                auto v = calDayList(calY, calM, d);
                for (int k = 0; k < (int)v.size() && k < 2; k++) {
                    const CalEvent& e = gCal[v[k]];
                    rect(x + 6, y + 30 + k * 18, 3, 14, EVCOL[e.col]);
                    std::string t = e.title; while (t.size() > 2 && tw(t, F16) > 100) t.pop_back();
                    text(t, F16, x + 13, y + 28 + k * 18, today ? bg : WHITE, 0, 230);
                }
                if (v.size() > 2) text("+" + std::to_string(v.size() - 2), F16, x + 116, y + 4, today ? bg : WHITE, 2, 200);
            }
        }
    } else {
        // agenda: the next 60 days
        SDL_Rect clip = {0, 100, 980, UIH - 100}; SDL_RenderSetClipRect(gR, &clip);
        int y = 108 - calScroll, y0 = calY, m0 = calM, d0 = 1;
        if (calY == gNow.y && calM == gNow.mo) d0 = gNow.d;
        int yy = y0, mm = m0, dd = d0, shownRows = 0;
        calAgendaRows.clear();
        for (int n = 0; n < 62; n++, calStepDay(yy, mm, dd, 1)) {
            auto v = calDayList(yy, mm, dd);
            if (v.empty()) continue;
            char hb[64]; snprintf(hb, sizeof hb, "%s %d %s", WD[dow(yy, mm, dd)], dd, MONTHS[mm - 1]);
            if (y > 60 && y < UIH) text(hb, F22, 56, y, calIsToday(yy, mm, dd) ? Col{255, 214, 90} : WHITE, 0, 230);
            y += 34;
            for (int i : v) {
                const CalEvent& e = gCal[i];
                if (y > 60 && y < UIH) {
                    rect(56, y, 900, 54, WHITE, 28); rect(56, y, 8, 54, EVCOL[e.col]);
                    text(e.allday ? "All day" : hhmm(e.sh, e.sm), F22, 80, y + 14, WHITE);
                    text(e.title, F22, 220, y + 14, WHITE);
                    if (!e.place.empty()) text(e.place, F16, 940, y + 18, WHITE, 2, 170);
                }
                calAgendaRows.push_back({y + calScroll, i});
                y += 60; shownRows++;
            }
            y += 10;
        }
        calAgendaH = y + calScroll;
        if (!shownRows) text("Nothing planned this month", F30, 56, 150, WHITE, 0, 190);
        SDL_RenderSetClipRect(gR, nullptr);
    }
    // side panel: events of the selected day
    int px = 1000;
    if (calView == 0) {
        rect(px - 14, 108, 2, 540, WHITE, 50);
        char hd[64]; snprintf(hd, sizeof hd, "%s, %d", WD[dow(calY, calM, calSel)], calSel);
        text(hd, F30, px, 112, WHITE);
        text(std::string(MONTHS[calM - 1]) + " " + std::to_string(calY), F22, px, 152, WHITE, 0, 190);
        auto v = calDayList(calY, calM, calSel);
        int ey = 196;
        if (v.empty()) text("Nothing planned", F22, px, ey, WHITE, 0, 170);
        for (size_t k = 0; k < v.size() && ey < 520; k++) {
            const CalEvent& e = gCal[v[k]];
            rect(px, ey, 6, 46, EVCOL[e.col]);
            text(e.allday ? "All day" : hhmm(e.sh, e.sm) + " - " + hhmm((e.sh * 60 + e.sm + e.dur) / 60 % 24, (e.sh * 60 + e.sm + e.dur) % 60), F16, px + 16, ey, WHITE, 0, 190);
            std::string t = e.title; while (t.size() > 3 && tw(t, F22) > 240) t.pop_back();
            text(t, F22, px + 16, ey + 20, WHITE);
            ey += 56;
        }
        btn(px, 560, 250, 50, "+ New event", Col{90, 56, 190}, WHITE);
    } else btn(px + 10, 560, 240, 50, "+ New event", Col{90, 56, 190}, WHITE);
}
static void calendarTap(int x, int y) {
    if (calEdit) { calEditorTap(x, y); return; }
    if ((x - 1030) * (x - 1030) + (y - 62) * (y - 62) < 900) { calMonth(-1); calScroll = 0; return; }
    if ((x - 1100) * (x - 1100) + (y - 62) * (y - 62) < 900) { calMonth(1); calScroll = 0; return; }
    if (inR(x, y, 1148, 42, 100, 40)) { calY = gNow.y; calM = gNow.mo; calSel = gNow.d; calScroll = 0; return; }
    if (inR(x, y, 640, 42, 118, 40)) { calView = 0; return; }
    if (inR(x, y, 764, 42, 118, 40)) { calView = 1; calScroll = 0; return; }
    if (inR(x, y, calView ? 1010 : 1000, 560, 250, 50)) { calNewEvent(); return; }
    if (calView == 1) {
        int cy = y + calScroll;
        if (x < 980 && y >= 100) for (auto& r : calAgendaRows) if (cy >= r.first && cy < r.first + 54) { calEditEvent(r.second); return; }
        return;
    }
    auto v = calDayList(calY, calM, calSel);
    if (x >= 1000 && y >= 196) {
        int k = (y - 196) / 56;
        if (k >= 0 && k < (int)v.size() && (y - 196) % 56 < 48) { calEditEvent(v[k]); return; }
    }
    int ws = gWeekStart ? 1 : 0;
    int col = (x - 56) / 128, row = (y - (CAL_Y - 4)) / CAL_RH;
    if (x >= 56 && x < 56 + 7 * 128 && y >= CAL_Y - 4 && row >= 0 && row < 6) {
        int d = row * 7 + col - ((dow(calY, calM, 1) + 7 - ws) % 7) + 1;
        if (d >= 1 && d <= daysIn(calY, calM)) {
            if (d == calSel) { auto vv = calDayList(calY, calM, d); if (vv.empty()) calNewEvent(); }
            calSel = d;
        }
        else if (d < 1) { calMonth(-1); calSel = std::max(1, daysIn(calY, calM) + d); }
        else { calSel = d - daysIn(calY, calM); calMonth(1); calSel = std::min(calSel, daysIn(calY, calM)); }
    }
}
static void calendarDrag(int dx, int dy) {
    static int ax = 0;
    if (calEdit) return;
    if (calView == 1) { calScroll = std::max(0, std::min(std::max(0, calAgendaH - (UIH - 100)), calScroll - dy)); return; }
    ax += dx;
    if (ax > 220) { calMonth(-1); ax = 0; } else if (ax < -220) { calMonth(1); ax = 0; }
}

// ================================================================ Files (file manager)
struct FmEnt { std::string name; bool dir; long long size; long long mt; };
struct FmOp { int t; std::string a, b; };
static std::string fmPath = "sdmc:/";
static std::vector<FmEnt> fmList;
static int fmSort = 0, fmScroll = 0, fmMode = 0, fmPrompt = 0, fmTScroll = 0;
static bool fmDesc = false, fmSelMode = false, fmCut = false;
static std::set<std::string> fmSel;
static std::vector<std::string> fmClip;
static std::string fmInput, fmFrom, fmTitle;
static std::vector<std::string> fmLines;
static std::vector<FmOp> fmOps; static size_t fmOpI = 0; static FILE *fmIn = nullptr, *fmOutF = nullptr;
static bool fmBusy = false; static std::string fmJobName;
static long long fmFree = 0, fmTotalB = 0;
static std::vector<CalHit> fmHits;
static std::vector<std::pair<std::string, std::string>> gFmSegs;
static void fmHit(int x, int y, int w, int h, int id) { fmHits.push_back({SDL_Rect{x, y, w, h}, id}); }
static std::string fmJoin(const std::string& d, const std::string& n) { return d.back() == '/' ? d + n : d + "/" + n; }
static std::string fmBase(const std::string& p) { size_t s = p.find_last_of('/'); return s == std::string::npos ? p : p.substr(s + 1); }
static std::string fmParent(const std::string& p) {
    if (p == "sdmc:/" || p.size() <= 6) return "sdmc:/";
    size_t s = p.find_last_of('/');
    std::string r = p.substr(0, s);
    return r.size() < 6 ? "sdmc:/" : r;
}
static std::string fmSize(long long b) {
    char t[32];
    if (b < 1024) snprintf(t, sizeof t, "%lld B", b);
    else if (b < 1024 * 1024) snprintf(t, sizeof t, "%.1f KB", b / 1024.0);
    else if (b < 1024LL * 1024 * 1024) snprintf(t, sizeof t, "%.1f MB", b / 1048576.0);
    else snprintf(t, sizeof t, "%.2f GB", b / 1073741824.0);
    return t;
}
static std::string fmExt(const std::string& n) { size_t d = n.find_last_of('.'); return d == std::string::npos ? "" : lower(n.substr(d + 1)); }
static int fmKind(const std::string& n) { // 0 other, 1 image, 2 audio, 3 pdf, 4 text, 5 nro
    std::string e = fmExt(n);
    if (isImg(n)) return 1;
    if (e == "mp3" || e == "ogg" || e == "wav" || e == "flac" || e == "mod" || e == "mid" || e == "opus" || e == "m4a" || e == "it" || e == "xm" || e == "s3m") return 2;
    if (e == "pdf") return 3;
    if (e == "nro") return 5;
    static const char* T[] = {"txt", "md", "log", "ini", "cfg", "json", "xml", "csv", "c", "cpp", "h", "hpp", "py", "lua", "sh", "html", "css", "js", "conf", "yml", "yaml", "nfo", "srt", "toml", "bat", "rtf", "tsv", "inc", "make", "mk", nullptr};
    for (int i = 0; T[i]; i++) if (e == T[i]) return 4;
    return 0;
}
static void fmStorage() {
    struct statvfs sv;
    if (statvfs("sdmc:/", &sv) == 0) { fmFree = (long long)sv.f_bavail * sv.f_frsize; fmTotalB = (long long)sv.f_blocks * sv.f_frsize; }
}
static void fmRefresh() {
    fmList.clear();
    DIR* d = opendir(fmPath.c_str());
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            struct stat st;
            if (stat(fmJoin(fmPath, n).c_str(), &st) != 0) continue;
            fmList.push_back({n, S_ISDIR(st.st_mode), (long long)st.st_size, (long long)st.st_mtime});
            if (fmList.size() > 4000) break;
        }
        closedir(d);
    }
    std::sort(fmList.begin(), fmList.end(), [](const FmEnt& a, const FmEnt& b) {
        if (a.dir != b.dir) return a.dir;
        bool lt;
        if (fmSort == 1) lt = a.size != b.size ? a.size < b.size : lower(a.name) < lower(b.name);
        else if (fmSort == 2) lt = a.mt != b.mt ? a.mt < b.mt : lower(a.name) < lower(b.name);
        else lt = lower(a.name) < lower(b.name);
        return fmDesc ? !lt : lt;
    });
    fmStorage();
}
static void fmGo(const std::string& p) { fmPath = p; fmScroll = 0; fmSel.clear(); fmSelMode = false; fmRefresh(); }
static std::string fmUnique(const std::string& dir, const std::string& name) {
    std::string p = fmJoin(dir, name);
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return p;
    std::string stem = name, ext;
    size_t d = name.find_last_of('.');
    if (d != std::string::npos && d > 0) { stem = name.substr(0, d); ext = name.substr(d); }
    for (int i = 1; i < 1000; i++) {
        p = fmJoin(dir, stem + " - copy" + (i > 1 ? " " + std::to_string(i) : "") + ext);
        if (stat(p.c_str(), &st) != 0) return p;
    }
    return p;
}
static void fmPlanCopy(const std::string& src, const std::string& dst) {
    struct stat st;
    if (stat(src.c_str(), &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        fmOps.push_back({0, "", dst});
        DIR* d = opendir(src.c_str());
        if (!d) return;
        struct dirent* e;
        std::vector<std::string> names;
        while ((e = readdir(d)) != nullptr) { std::string n = e->d_name; if (n != "." && n != "..") names.push_back(n); }
        closedir(d);
        for (auto& n : names) fmPlanCopy(fmJoin(src, n), fmJoin(dst, n));
    } else fmOps.push_back({1, src, dst});
}
static void fmPlanDelete(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(p.c_str());
        if (d) {
            struct dirent* e; std::vector<std::string> names;
            while ((e = readdir(d)) != nullptr) { std::string n = e->d_name; if (n != "." && n != "..") names.push_back(n); }
            closedir(d);
            for (auto& n : names) fmPlanDelete(fmJoin(p, n));
        }
        fmOps.push_back({3, p, ""});
    } else fmOps.push_back({2, p, ""});
}
static void fmStartJob(const std::string& name) {
    fmJobName = name; fmOpI = 0; fmBusy = !fmOps.empty();
    if (!fmBusy) fmRefresh();
}
static void fmPaste() {
    if (fmClip.empty() || fmBusy) return;
    fmOps.clear();
    std::vector<std::string> removeAfter;
    for (auto& src : fmClip) {
        std::string dst = fmUnique(fmPath, fmBase(src));
        if (src == fmPath || dst.compare(0, src.size() + 1, src + "/") == 0) { toast("Can't paste a folder into itself"); continue; }
        if (fmCut) {
            std::string tgt = fmJoin(fmPath, fmBase(src));
            struct stat st;
            if (fmParent(src) == fmPath) continue;
            if (stat(tgt.c_str(), &st) != 0 && rename(src.c_str(), tgt.c_str()) == 0) continue;
            removeAfter.push_back(src);
        }
        fmPlanCopy(src, dst);
    }
    for (auto& r : removeAfter) fmPlanDelete(r);
    if (fmCut) fmClip.clear();
    fmStartJob(fmCut ? "Moving" : "Copying");
}
static void fmFinishJob() {
    if (fmIn) { fclose(fmIn); fmIn = nullptr; }
    if (fmOutF) { fclose(fmOutF); fmOutF = nullptr; }
    fmOps.clear(); fmBusy = false; fmSel.clear(); fmSelMode = false;
    fmRefresh();
    toast(fmJobName == "Deleting" ? "Deleted" : "Done");
}
static void filesUpdate(float dt) {
    (void)dt;
    if (!fmBusy) return;
    Uint32 t0 = SDL_GetTicks();
    static char buf[262144];
    while (fmOpI < fmOps.size() && SDL_GetTicks() - t0 < 10) {
        FmOp& o = fmOps[fmOpI];
        if (o.t == 0) { mkdir(o.b.c_str(), 0777); fmOpI++; }
        else if (o.t == 1) {
            if (!fmIn) {
                fmIn = fopen(o.a.c_str(), "rb"); fmOutF = fopen(o.b.c_str(), "wb");
                if (!fmIn || !fmOutF) { if (fmIn) fclose(fmIn); if (fmOutF) fclose(fmOutF); fmIn = fmOutF = nullptr; fmOpI++; continue; }
            }
            size_t n = fread(buf, 1, sizeof buf, fmIn);
            if (n > 0) fwrite(buf, 1, n, fmOutF);
            if (n < sizeof buf) { fclose(fmIn); fclose(fmOutF); fmIn = fmOutF = nullptr; fmOpI++; }
        }
        else if (o.t == 2) { remove(o.a.c_str()); fmOpI++; }
        else if (o.t == 3) { rmdir(o.a.c_str()); fmOpI++; }
        else fmOpI++;
    }
    if (fmOpI >= fmOps.size()) fmFinishJob();
}
static void musPlayFile(const std::string& path);
static void fmOpen(const FmEnt& e) {
    std::string p = fmJoin(fmPath, e.name);
    if (e.dir) { fmGo(p); return; }
    switch (fmKind(e.name)) {
        case 1: gPhotoOpenPath = p; if (running[A_PHOTOS]) appReset(A_PHOTOS); pendingApp = A_PHOTOS; closeApp(false); break;
        case 2: musPlayFile(p); toast("Playing " + e.name); break;
        case 3: gPdfOpenPath = p; if (running[A_PDF]) appReset(A_PDF); pendingApp = A_PDF; closeApp(false); break;
        case 5: saveAll(); toast("Launching " + e.name); if (R_SUCCEEDED(envSetNextLoad(p.c_str(), p.c_str()))) gRun = false; break;
        case 4: {
            std::string d;
            FILE* f = fopen(p.c_str(), "rb");
            if (f) { char b[4096]; size_t n; while (d.size() < 262144 && (n = fread(b, 1, sizeof b, f)) > 0) d.append(b, n); fclose(f); }
            fmLines.clear();
            std::string cur;
            for (size_t i = 0; i <= d.size() && fmLines.size() < 5000; i++) {
                if (i == d.size() || d[i] == '\n') { for (auto& l : wrapText(cur, F22, 1130)) fmLines.push_back(l); if (cur.empty()) fmLines.push_back(""); cur.clear(); }
                else if (d[i] != '\r') cur += (d[i] == '\t' ? ' ' : d[i]);
            }
            fmTitle = e.name; fmTScroll = 0; fmMode = 1;
            break;
        }
        default: toast("No app can open ." + fmExt(e.name) + " files yet");
    }
}
static void fmDoPrompt() {
    if (fmInput.empty()) { fmMode = 0; hideKb(); return; }
    std::string dst = fmJoin(fmPath, fmInput);
    struct stat st;
    if (stat(dst.c_str(), &st) == 0) { toast("That name already exists"); return; }
    bool ok = fmPrompt == 1 ? rename(fmFrom.c_str(), dst.c_str()) == 0 : mkdir(dst.c_str(), 0777) == 0;
    if (!ok) toast("That didn't work");
    fmMode = 0; hideKb(); fmSel.clear(); fmSelMode = false; fmRefresh();
}
static bool filesBack() {
    if (fmMode == 3) { hideKb(); fmMode = 0; return true; }
    if (fmMode != 0) { fmMode = 0; return true; }
    if (fmSelMode) { fmSelMode = false; fmSel.clear(); return true; }
    if (fmPath != "sdmc:/") { fmGo(fmParent(fmPath)); return true; }
    return false;
}
static void filesInit() { if (fmList.empty() || true) { fmSelMode = false; fmSel.clear(); fmMode = 0; fmRefresh(); } }
static Col fmKindCol(int k, bool dir) {
    if (dir) return Col{255, 200, 70};
    switch (k) { case 1: return Col{0, 150, 160}; case 2: return Col{222, 89, 26}; case 3: return Col{210, 50, 50}; case 4: return Col{110, 120, 140}; case 5: return Col{130, 80, 200}; default: return Col{90, 90, 100}; }
}
static const int FM_TOP = 192, FM_ROW = 54;
static void drawFiles() {
    fmHits.clear();
    rect(0, 0, W, UIH, Col{30, 32, 38});
    if (fmMode == 1) { // text viewer
        rect(0, 0, W, 70, Col{44, 46, 54});
        text(fmTitle, F30, 70, 18, WHITE);
        btn(1130, 14, 100, 42, "Close", Col{70, 72, 82}, WHITE); fmHit(1130, 14, 100, 42, 90);
        SDL_Rect clip = {0, 74, W, UIH - 74}; SDL_RenderSetClipRect(gR, &clip);
        int maxS = std::max(0, (int)fmLines.size() * 28 - (UIH - 100));
        fmTScroll = std::max(0, std::min(fmTScroll, maxS));
        for (int i = std::max(0, fmTScroll / 28); i < (int)fmLines.size(); i++) {
            int y = 84 + i * 28 - fmTScroll;
            if (y > UIH) break;
            text(fmLines[i], F22, 70, y, WHITE, 0, 235);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        return;
    }
    text("Files", F44, 56, 16, WHITE);
    // storage
    if (fmTotalB > 0) {
        float used = 1.0f - (float)fmFree / fmTotalB;
        rect(780, 38, 440, 12, WHITE, 40); rect(780, 38, (int)(440 * used), 12, used > 0.9f ? Col{209, 52, 56} : Col{0, 150, 220});
        text(fmSize(fmFree) + " free of " + fmSize(fmTotalB), F16, 1220, 56, WHITE, 2, 190);
    }
    // path bar
    rect(56, 84, 1168, 44, WHITE, 24);
    btn(56, 84, 56, 44, "Up", Col{70, 74, 86}, WHITE); fmHit(56, 84, 56, 44, 1);
    {
        std::string rest = fmPath.size() > 6 ? fmPath.substr(6) : "";
        int x = 126; std::string acc = "sdmc:/";
        std::vector<std::pair<std::string, std::string>> segs = {{"SD card", "sdmc:/"}};
        size_t p = 0;
        while (p < rest.size()) { size_t q = rest.find('/', p); if (q == std::string::npos) q = rest.size(); if (q > p) { acc = fmJoin(acc, rest.substr(p, q - p)); segs.push_back({rest.substr(p, q - p), acc}); } p = q + 1; }
        int tot = 0; for (auto& sg : segs) tot += tw(sg.first, F22) + 36;
        size_t st = 0;
        while (tot > 1060 && st + 1 < segs.size()) { tot -= tw(segs[st].first, F22) + 36; st++; }
        for (size_t i = st; i < segs.size(); i++) {
            int w = tw(segs[i].first, F22);
            text(segs[i].first, F22, x, 92, i + 1 == segs.size() ? WHITE : Col{150, 200, 255});
            fmHit(x - 8, 84, w + 16, 44, 200 + (int)i);
            x += w + 14;
            if (i + 1 < segs.size()) { text(">", F22, x, 92, WHITE, 0, 140); x += 22; }
        }
        gFmSegs = segs;
    }
    // toolbar
    int bx = 56;
    btn(bx, 138, 120, 44, fmSelMode ? "Done" : "Select", fmSelMode ? Col{0, 120, 215} : Col{70, 74, 86}, WHITE); fmHit(bx, 138, 120, 44, 2); bx += 128;
    btn(bx, 138, 170, 44, "New folder", Col{70, 74, 86}, WHITE); fmHit(bx, 138, 170, 44, 3); bx += 178;
    { static const char* SN[3] = {"Name", "Size", "Date"}; std::string l = std::string("Sort: ") + SN[fmSort] + (fmDesc ? " v" : " ^");
      btn(bx, 138, 190, 44, l.c_str(), Col{70, 74, 86}, WHITE); fmHit(bx, 138, 190, 44, 4); bx += 198; }
    if (!fmClip.empty()) { std::string l = std::string(fmCut ? "Move here (" : "Paste (") + std::to_string(fmClip.size()) + ")";
      btn(bx, 138, 210, 44, l.c_str(), Col{16, 150, 50}, WHITE); fmHit(bx, 138, 210, 44, 5); }
    text(std::to_string(fmList.size()) + " items", F16, 1224, 150, WHITE, 2, 150);
    int bottom = UIH - (fmSelMode ? 70 : 0);
    int maxS = std::max(0, (int)fmList.size() * FM_ROW - (bottom - FM_TOP) + 6);
    fmScroll = std::max(0, std::min(fmScroll, maxS));
    SDL_Rect clip = {0, FM_TOP, W, bottom - FM_TOP}; SDL_RenderSetClipRect(gR, &clip);
    for (int i = std::max(0, fmScroll / FM_ROW); i < (int)fmList.size(); i++) {
        int y = FM_TOP + i * FM_ROW - fmScroll;
        if (y > bottom) break;
        const FmEnt& e = fmList[i];
        bool sel = fmSel.count(e.name) > 0;
        rect(56, y, 1168, FM_ROW - 4, sel ? Col{0, 100, 190} : Col{44, 46, 54});
        int k = e.dir ? 0 : fmKind(e.name);
        int ix = 72;
        if (fmSelMode) { outline(70, y + 12, 26, 26, 2, WHITE); if (sel) rect(75, y + 17, 16, 16, WHITE); ix = 112; }
        Col kc = fmKindCol(k, e.dir);
        if (e.dir) { rect(ix, y + 10, 18, 7, kc); rect(ix, y + 15, 38, 26, kc); }
        else { rect(ix + 4, y + 6, 30, 38, kc); std::string ex = fmExt(e.name); if (ex.size() > 4) ex = ex.substr(0, 4); text(ex, F16, ix + 19, y + 16, WHITE, 1); }
        std::string nm = e.name; while (nm.size() > 3 && tw(nm, F22) > 700) nm.pop_back();
        text(nm, F22, ix + 56, y + 12, WHITE);
        if (!e.dir) text(fmSize(e.size), F16, 1000, y + 16, WHITE, 2, 190);
        time_t tt = (time_t)e.mt; struct tm* lt = localtime(&tt);
        if (lt && e.mt > 0) { char db[32]; snprintf(db, sizeof db, "%04d-%02d-%02d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday); text(db, F16, 1200, y + 16, WHITE, 2, 160); }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (fmList.empty()) text("This folder is empty", F30, W / 2, 330, WHITE, 1, 160);
    if (fmSelMode) {
        rect(0, UIH - 70, W, 70, Col{20, 22, 28});
        const char* L[6] = {"Copy", "Cut", "Delete", "Rename", "Select all", "Cancel"};
        for (int i = 0; i < 6; i++) { int x = 40 + i * 195; bool dis = (i == 3 && fmSel.size() != 1) || (i < 4 && fmSel.empty());
            btn(x, UIH - 62, 185, 54, L[i], i == 2 ? Col{190, 40, 60} : Col{70, 74, 86}, dis ? Col{130, 130, 135} : WHITE); fmHit(x, UIH - 62, 185, 54, 10 + i); }
    }
    if (fmBusy) {
        rect(0, 0, W, UIH, BLACK, 140);
        rect(340, 250, 600, 170, Col{40, 42, 50}); outline(340, 250, 600, 170, 2, WHITE);
        text(fmJobName + "...", F30, 640, 272, WHITE, 1);
        rect(380, 340, 520, 16, WHITE, 40); rect(380, 340, (int)(520.0f * fmOpI / std::max<size_t>(1, fmOps.size())), 16, Col{0, 150, 220});
        text(std::to_string(fmOpI) + " / " + std::to_string(fmOps.size()), F16, 640, 372, WHITE, 1, 180);
    } else if (fmMode == 2 || fmMode == 3) {
        rect(0, 0, W, UIH, BLACK, 150);
        rect(240, 100, 800, 230, Col{40, 42, 50}); outline(240, 100, 800, 230, 2, WHITE);
        if (fmMode == 2) {
            text("Delete " + std::to_string(fmSel.size()) + (fmSel.size() == 1 ? " item?" : " items?"), F30, 640, 130, WHITE, 1);
            text("This can't be undone.", F22, 640, 180, WHITE, 1, 180);
            btn(300, 250, 300, 56, "Delete", Col{190, 40, 60}, WHITE); fmHit(300, 250, 300, 56, 30);
            btn(680, 250, 300, 56, "Cancel", Col{70, 74, 86}, WHITE); fmHit(680, 250, 300, 56, 31);
        } else {
            text(fmPrompt == 1 ? "Rename" : "New folder", F30, 640, 116, WHITE, 1);
            drawField(280, 170, 720, 56, &fmInput, "Name", F30, DARK, WHITE, Col{255, 255, 255}); fmHit(280, 170, 720, 56, 32);
            btn(300, 252, 300, 54, "OK", Col{0, 120, 215}, WHITE); fmHit(300, 252, 300, 54, 33);
            btn(680, 252, 300, 54, "Cancel", Col{70, 74, 86}, WHITE); fmHit(680, 252, 300, 54, 31);
        }
    }
}
static void filesTap(int x, int y) {
    if (fmBusy) return;
    if (fmMode == 3 && kbOpen && y >= KBTOP) return;
    int id = 0;
    for (auto& h : fmHits) if (inR(x, y, h.r.x, h.r.y, h.r.w, h.r.h)) { id = h.id; break; }
    if (fmMode == 1) { if (id == 90) fmMode = 0; return; }
    if (fmMode == 2) {
        if (id == 30) {
            fmOps.clear();
            for (auto& n : fmSel) fmPlanDelete(fmJoin(fmPath, n));
            fmMode = 0; fmStartJob("Deleting");
        } else if (id == 31) fmMode = 0;
        return;
    }
    if (fmMode == 3) {
        if (id == 32) openKbStr(&fmInput, 80, fmDoPrompt);
        else if (id == 33) fmDoPrompt();
        else if (id == 31) { hideKb(); fmMode = 0; }
        return;
    }
    if (id >= 200) { if (id - 200 < (int)gFmSegs.size()) fmGo(gFmSegs[id - 200].second); return; }
    switch (id) {
        case 1: if (fmPath != "sdmc:/") fmGo(fmParent(fmPath)); return;
        case 2: fmSelMode = !fmSelMode; fmSel.clear(); return;
        case 3: fmPrompt = 2; fmInput.clear(); fmMode = 3; openKbStr(&fmInput, 80, fmDoPrompt); return;
        case 4: if (!fmDesc) fmDesc = true; else { fmDesc = false; fmSort = (fmSort + 1) % 3; } fmRefresh(); return;
        case 5: fmPaste(); return;
        case 10: case 11: if (!fmSel.empty()) { fmClip.clear(); for (auto& n : fmSel) fmClip.push_back(fmJoin(fmPath, n)); fmCut = id == 11; fmSelMode = false; fmSel.clear(); toast(fmCut ? "Cut - open a folder and tap Move here" : "Copied - open a folder and tap Paste"); } return;
        case 12: if (!fmSel.empty()) fmMode = 2; return;
        case 13: if (fmSel.size() == 1) { fmFrom = fmJoin(fmPath, *fmSel.begin()); fmInput = *fmSel.begin(); fmPrompt = 1; fmMode = 3; openKbStr(&fmInput, 80, fmDoPrompt); } return;
        case 14: if (fmSel.size() == fmList.size()) fmSel.clear(); else for (auto& e : fmList) fmSel.insert(e.name); return;
        case 15: fmSelMode = false; fmSel.clear(); return;
        default: break;
    }
    int bottom = UIH - (fmSelMode ? 70 : 0);
    if (y >= FM_TOP && y < bottom) {
        int i = (y - FM_TOP + fmScroll) / FM_ROW;
        if (i >= 0 && i < (int)fmList.size()) {
            if (fmSelMode) { const std::string& n = fmList[i].name; if (fmSel.count(n)) fmSel.erase(n); else fmSel.insert(n); }
            else fmOpen(fmList[i]);
        }
    }
}
static void filesDrag(int dx, int dy) { (void)dx; if (fmMode == 1) fmTScroll -= dy; else if (fmMode == 0) fmScroll -= dy; }

// ================================================================ Maps (OpenStreetMap tiles, cached on the SD card)
struct MTile { SDL_Texture* t = nullptr; Uint32 last = 0, failT = 0; bool loading = false; };
static std::map<long long, MTile> mpTiles;
static double mpLat = 36.75, mpLon = 3.04, mpZoom = 4;
static int mpJobsN = 0, mpDiskBudget = 0;
static std::string mpQuery, mpMsg; static bool mpBusy = false;
struct MPlace { std::string name; double lat, lon; };
static std::vector<MPlace> mpResults; static bool mpMark = false; static double mpMLat = 0, mpMLon = 0;
static Uint32 mpTapT = 0; static int mpTapX = 0, mpTapY = 0;
static double mpWorld(double zoom) { return 256.0 * pow(2.0, zoom); }
static double mpPxX(double lon, double zoom) { return (lon + 180.0) / 360.0 * mpWorld(zoom); }
static double mpPxY(double lat, double zoom) { double r = lat * M_PI / 180.0; return (1.0 - log(tan(r) + 1.0 / cos(r)) / M_PI) / 2.0 * mpWorld(zoom); }
static double mpLonOf(double px, double zoom) { return px / mpWorld(zoom) * 360.0 - 180.0; }
static double mpLatOf(double py, double zoom) { double n = M_PI - 2.0 * M_PI * py / mpWorld(zoom); return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n))); }
static long long mpKey(int z, int x, int y) { return ((long long)z << 40) | ((long long)x << 20) | (long long)y; }
static std::string mpPath(int z, int x, int y) { return DIR_ + "maptiles/" + std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".png"; }
static void mpTileDone(NetJob& j) {
    mpJobsN = std::max(0, mpJobsN - 1);
    auto it = mpTiles.find(atoll(j.a.c_str()));
    if (it == mpTiles.end()) return;
    it->second.loading = false;
    if (!j.ok) it->second.failT = SDL_GetTicks();
}
static SDL_Texture* mpTile(int z, int x, int y) {
    long long k = mpKey(z, x, y);
    MTile& m = mpTiles[k];
    m.last = SDL_GetTicks();
    if (m.t) return m.t;
    if (m.loading) return nullptr;
    if (mpDiskBudget > 0) {
        std::string p = mpPath(z, x, y);
        struct stat st;
        if (stat(p.c_str(), &st) == 0 && st.st_size > 0) {
            mpDiskBudget--;
            SDL_Surface* sf = IMG_Load(p.c_str());
            if (sf) { m.t = SDL_CreateTextureFromSurface(gR, sf); SDL_FreeSurface(sf); if (m.t) return m.t; }
            remove(p.c_str());
        }
    }
    if (mpJobsN < 4 && (m.failT == 0 || SDL_GetTicks() - m.failT > 20000) && netUp()) {
        mkdir((DIR_ + "maptiles").c_str(), 0777);
        mkdir((DIR_ + "maptiles/" + std::to_string(z)).c_str(), 0777);
        mkdir((DIR_ + "maptiles/" + std::to_string(z) + "/" + std::to_string(x)).c_str(), 0777);
        NetJob* j = new NetJob(); char u[160]; snprintf(u, sizeof u, "https://tile.openstreetmap.org/%d/%d/%d.png", z, x, y);
        j->url = u; j->savePath = mpPath(z, x, y); j->a = std::to_string(k); j->done = mpTileDone;
        if (netStart(j)) { m.loading = true; mpJobsN++; }
    }
    return nullptr;
}
static void mpTrim() {
    if (mpTiles.size() < 120) return;
    Uint32 now = SDL_GetTicks();
    for (auto it = mpTiles.begin(); it != mpTiles.end();) {
        if (!it->second.loading && now - it->second.last > 4000) { if (it->second.t) SDL_DestroyTexture(it->second.t); it = mpTiles.erase(it); } else ++it;
    }
}
static void mapsFree() { for (auto& t : mpTiles) if (t.second.t) SDL_DestroyTexture(t.second.t); mpTiles.clear(); }
static void mapsInit() {
    mpMsg.clear(); mpResults.clear();
    std::string d;
    if (readFile(DIR_ + "maps.txt", d)) sscanf(d.c_str(), "%lf %lf %lf", &mpLat, &mpLon, &mpZoom);
    else if (gWxHaveLoc) { mpLat = gWxLat; mpLon = gWxLon; mpZoom = 11; }
}
static void mapsSave() { char b[96]; snprintf(b, sizeof b, "%.6f %.6f %.2f\n", mpLat, mpLon, mpZoom); writeFile(DIR_ + "maps.txt", b); }
static void mapsClose() { mapsSave(); mapsFree(); }
static void mpPan(double dx, double dy) {
    double cx = mpPxX(mpLon, mpZoom) - dx, cy = mpPxY(mpLat, mpZoom) - dy;
    double ws = mpWorld(mpZoom);
    cy = std::max(0.0, std::min(ws - 1, cy));
    mpLon = mpLonOf(cx, mpZoom); while (mpLon > 180) mpLon -= 360; while (mpLon < -180) mpLon += 360;
    mpLat = mpLatOf(cy, mpZoom);
}
static void mpSetZoom(double z) { mpZoom = std::max(2.0, std::min(18.0, z)); }
static void mpSearchDone(NetJob& j) {
    mpBusy = false; mpResults.clear();
    size_t p = 0;
    while (mpResults.size() < 6) {
        size_t a = j.out.find("\"lat\"", p);
        if (a == std::string::npos) break;
        MPlace m; m.lat = atof(jstr(j.out, "lat", a).c_str()); m.lon = atof(jstr(j.out, "lon", a).c_str());
        m.name = jstr(j.out, "display_name", a);
        if (m.name.empty()) m.name = "Place";
        mpResults.push_back(m); p = a + 5;
    }
    mpMsg = mpResults.empty() ? "Nothing found." : "";
}
static void mpSearch() {
    hideKb();
    if (mpQuery.empty() || mpBusy) return;
    if (!netUp()) { mpMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob(); j->url = "https://nominatim.openstreetmap.org/search?format=json&limit=6&q=" + urlEncode(mpQuery); j->done = mpSearchDone;
    if (netStart(j)) { mpBusy = true; mpMsg = "Searching..."; }
}
static void mapsPinch(float sc, int, int, bool begin) {
    static double z0 = 4;
    if (begin) { z0 = mpZoom; return; }
    mpSetZoom(z0 + log2((double)std::max(0.1f, sc)));
}
static void drawMaps() {
    rect(0, 0, W, UIH, Col{226, 222, 212});
    mpDiskBudget = 3;
    int z = std::max(1, std::min(17, (int)floor(mpZoom + 0.0001)));
    double sc = pow(2.0, mpZoom - z), ts = 256.0 * sc;
    double cx = mpPxX(mpLon, z), cy = mpPxY(mpLat, z);       // centre in level-z pixels
    double left = cx - W / 2.0 / sc, top = cy - UIH / 2.0 / sc;
    int n = 1 << z;
    int tx0 = (int)floor(left / 256.0), tx1 = (int)floor((left + W / sc) / 256.0), ty0 = (int)floor(top / 256.0), ty1 = (int)floor((top + UIH / sc) / 256.0);
    SDL_Rect clip = {0, 0, W, UIH}; SDL_RenderSetClipRect(gR, &clip);
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (ty < 0 || ty >= n) continue;
        int wx = ((tx % n) + n) % n;
        int sx = (int)floor((tx * 256.0 - left) * sc), sy = (int)floor((ty * 256.0 - top) * sc);
        SDL_Rect d = {sx, sy, (int)ceil(ts) + 1, (int)ceil(ts) + 1};
        SDL_Texture* t = mpTile(z, wx, ty);
        if (t) SDL_RenderCopy(gR, t, nullptr, &d);
        else { rect(d.x, d.y, d.w, d.h, Col{232, 228, 218}); outline(d.x, d.y, d.w, d.h, 1, Col{210, 205, 195}); }
    }
    auto mark = [&](double la, double lo, Col c, bool pin) {
        double px = (mpPxX(lo, mpZoom) - mpPxX(mpLon, mpZoom)) + W / 2.0, py = (mpPxY(la, mpZoom) - mpPxY(mpLat, mpZoom)) + UIH / 2.0;
        if (px < -40 || px > W + 40 || py < -40 || py > UIH + 40) return;
        if (pin) { tri((float)px, (float)py, (float)px - 12, (float)py - 30, (float)px + 12, (float)py - 30, c); disc((int)px, (int)py - 34, 14, c); disc((int)px, (int)py - 34, 5, WHITE); }
        else { disc((int)px, (int)py, 12, WHITE); disc((int)px, (int)py, 8, c); }
    };
    if (gWxHaveLoc) mark(gWxLat, gWxLon, Col{0, 120, 215}, false);
    if (mpMark) mark(mpMLat, mpMLon, Col{209, 52, 56}, true);
    SDL_RenderSetClipRect(gR, nullptr);
    mpTrim();
    // overlay: search + zoom
    rect(0, 0, W, 66, BLACK, 150);
    drawField(70, 12, 600, 44, &mpQuery, "Search for a place", F22, DARK, WHITE, WHITE);
    btn(680, 12, 90, 44, "Go", Col{30, 150, 90}, WHITE);
    btn(780, 12, 120, 44, "My spot", Col{70, 74, 86}, WHITE);
    btn(W - 270, 12, 60, 44, "-", Col{70, 74, 86}, WHITE);
    btn(W - 200, 12, 60, 44, "+", Col{70, 74, 86}, WHITE);
    char zb[16]; snprintf(zb, sizeof zb, "z%.1f", mpZoom); text(zb, F16, W - 90, 24, WHITE, 1, 200);
    if (!mpMsg.empty()) { rect(W / 2 - 200, 76, 400, 40, BLACK, 170); text(mpMsg, F22, W / 2, 82, WHITE, 1); }
    int y = 76;
    for (size_t i = 0; i < mpResults.size() && mpMsg.empty(); i++) {
        std::string nm = mpResults[i].name; while (nm.size() > 3 && tw(nm, F16) > 540) nm.pop_back();
        rect(70, y, 600, 44, BLACK, 190); text(nm, F16, 84, y + 12, WHITE); y += 48;
    }
    rect(0, UIH - 22, 330, 22, WHITE, 190);
    text("(c) OpenStreetMap contributors", F16, 8, UIH - 20, Col{60, 60, 60});
    if (!netUp()) text("Offline: showing saved tiles only", F16, W - 14, UIH - 20, Col{120, 40, 40}, 2);
    // cross-hair
    line(W / 2 - 8, UIH / 2, W / 2 + 8, UIH / 2, 2, Col{60, 60, 60}, 140); line(W / 2, UIH / 2 - 8, W / 2, UIH / 2 + 8, 2, Col{60, 60, 60}, 140);
}
static void mapsTap(int x, int y) {
    if (kbOpen && kbTarget == 99 && y < KBTOP) hideKb();
    if (y < 66) {
        if (inR(x, y, 70, 12, 600, 44)) { openKbStr(&mpQuery, 60, mpSearch); return; }
        if (inR(x, y, 680, 12, 90, 44)) { mpSearch(); return; }
        if (inR(x, y, 780, 12, 120, 44)) { if (gWxHaveLoc) { mpLat = gWxLat; mpLon = gWxLon; mpSetZoom(13); } else toast("Set your city in Weather first"); return; }
        if (inR(x, y, W - 270, 12, 60, 44)) { mpSetZoom(floor(mpZoom + 0.01) - 1); return; }
        if (inR(x, y, W - 200, 12, 60, 44)) { mpSetZoom(floor(mpZoom + 0.01) + 1); return; }
        return;
    }
    if (!mpResults.empty() && mpMsg.empty() && x >= 70 && x < 670 && y >= 76 && y < 76 + (int)mpResults.size() * 48) {
        int i = (y - 76) / 48;
        if (i >= 0 && i < (int)mpResults.size()) { mpLat = mpResults[i].lat; mpLon = mpResults[i].lon; mpSetZoom(14); mpMark = true; mpMLat = mpLat; mpMLon = mpLon; mpResults.clear(); mapsSave(); }
        return;
    }
    mpResults.clear(); mpMsg.clear();
    Uint32 now = SDL_GetTicks();
    if (now - mpTapT < 380 && abs(x - mpTapX) < 40 && abs(y - mpTapY) < 40) { mpPan(W / 2.0 - x, UIH / 2.0 - y); mpSetZoom(mpZoom + 1); mpTapT = 0; }
    else { mpTapT = now; mpTapX = x; mpTapY = y; }
}
static void mapsDrag(int dx, int dy) { mpPan(dx, dy); }

// ================================================================ Podcasts and Internet radio
struct PodShow { std::string name, url; };
struct PodEp { std::string title, url, date; };
static std::vector<PodShow> gShows; static std::vector<PodEp> gEps;
static int podView = 0, podSel = -1, podScroll = 0; static std::string podMsg, podInput; static bool podBusy = false;
struct PodDl { NetJob* j; std::string url; };
static std::vector<PodDl> podDls;
static const PodShow POD_STARTER[3] = {{"NPR Up First", "https://feeds.npr.org/510318/podcast.xml"}, {"BBC Global News Podcast", "https://podcasts.files.bbci.co.uk/p02nq0gn.rss"}, {"99% Invisible", "https://feeds.simplecast.com/BqbsxVfO"}};
static std::string podHash(const std::string& s) { unsigned long long h = 1469598103934665603ULL; for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } char b[24]; snprintf(b, sizeof b, "%016llx", h); return std::string(b).substr(0, 12); }
static void podSave() { std::string o; for (auto& s : gShows) o += s.url + "\t" + s.name + "\n"; writeFile(DIR_ + "podcasts.txt", o); }
static void podLoad() {
    gShows.clear(); std::string d;
    if (readFile(DIR_ + "podcasts.txt", d)) {
        size_t p = 0;
        while (p < d.size()) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); p = e + 1; size_t t = l.find('\t'); if (t != std::string::npos) gShows.push_back({l.substr(t + 1), l.substr(0, t)}); }
    }
}
static std::string podAttr(const std::string& tag, const char* attr) {
    std::string k = std::string(attr) + "=";
    size_t a = tag.find(k); if (a == std::string::npos) return "";
    char q = tag[a + k.size()]; size_t b = tag.find(q, a + k.size() + 1);
    return b == std::string::npos ? "" : tag.substr(a + k.size() + 1, b - a - k.size() - 1);
}
static bool podParse(const std::string& x, std::string* showName) {
    std::vector<PodEp> v; size_t p = 0;
    size_t first = x.find("<item");
    if (showName) { std::string t = htmlToText(xmlTag(x.substr(0, first == std::string::npos ? x.size() : first), "title", 0)); while (!t.empty() && t.back() == '\n') t.pop_back(); if (!t.empty()) *showName = t; }
    while (v.size() < 60) {
        size_t a = x.find("<item", p); if (a == std::string::npos) break;
        size_t b = x.find("</item>", a); if (b == std::string::npos) break;
        std::string it = x.substr(a, b - a);
        PodEp e; e.title = htmlToText(xmlTag(it, "title", 0)); while (!e.title.empty() && e.title.back() == '\n') e.title.pop_back();
        size_t en = it.find("<enclosure");
        if (en != std::string::npos) { size_t ee = it.find('>', en); e.url = podAttr(it.substr(en, ee - en), "url"); }
        e.date = xmlTag(it, "pubDate", 0); if (e.date.size() > 16) e.date = e.date.substr(0, 16);
        if (!e.url.empty() && !e.title.empty()) {
            std::string ent; for (size_t i = 0; i < e.url.size(); i++) { if (e.url.compare(i, 5, "&amp;") == 0) { ent += '&'; i += 4; } else ent += e.url[i]; }
            e.url = ent; v.push_back(e);
        }
        p = b + 7;
    }
    if (v.empty()) return false;
    gEps = v; return true;
}
static std::string podFile(const PodEp& e) { return DIR_ + "podcasts/" + podHash(e.url) + ".mp3"; }
static bool podHave(const PodEp& e) { struct stat st; return stat(podFile(e).c_str(), &st) == 0 && st.st_size > 1000; }
static PodDl* podDlFor(const std::string& u) { for (auto& d : podDls) if (d.url == u) return &d; return nullptr; }
static void podFeedDone(NetJob& j) {
    podBusy = false;
    if (podSel < 0 || podSel >= (int)gShows.size() || gShows[podSel].url != j.url) return;
    std::string nm;
    if (j.ok && podParse(j.out, &nm)) { writeFile(DIR_ + "podcasts/" + podHash(j.url) + ".xml", j.out); if (gShows[podSel].name.empty() || gShows[podSel].name == gShows[podSel].url) { gShows[podSel].name = nm; podSave(); } podMsg.clear(); }
    else podMsg = "Couldn't load this feed.";
}
static void podOpen(int i) {
    podSel = i; podView = 1; podScroll = 0; gEps.clear(); podMsg.clear();
    mkdir((DIR_ + "podcasts").c_str(), 0777);
    std::string d;
    if (readFile(DIR_ + "podcasts/" + podHash(gShows[i].url) + ".xml", d)) podParse(d, nullptr);
    if (podBusy) return;
    if (!netUp()) { if (gEps.empty()) podMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob(); j->url = gShows[i].url; j->done = podFeedDone;
    if (netStart(j)) { podBusy = true; if (gEps.empty()) podMsg = "Loading episodes..."; }
}
static void podAddUrl(const std::string& url, const std::string& name) {
    if (url.compare(0, 4, "http") != 0) { toast("Paste a link that starts with http"); return; }
    for (auto& s : gShows) if (s.url == url) { toast("Already subscribed"); return; }
    gShows.push_back({name.empty() ? url : name, url}); podSave();
    podOpen((int)gShows.size() - 1);
}
static void podAddEnter() { hideKb(); std::string u = podInput; podInput.clear(); if (!u.empty()) podAddUrl(u, ""); }
static void podDlDone(NetJob& j) {
    for (size_t i = 0; i < podDls.size(); i++) if (podDls[i].j == &j) { podDls.erase(podDls.begin() + i); break; }
    toast(j.ok ? "Episode downloaded" : "Download failed");
}
static void podDownload(const PodEp& e) {
    if (podDlFor(e.url) || !netUp()) { if (!netUp()) toast(gOffline ? "Offline mode is on." : "No internet connection."); return; }
    mkdir((DIR_ + "podcasts").c_str(), 0777);
    NetJob* j = new NetJob(); j->url = e.url; j->savePath = podFile(e); j->done = podDlDone;
    if (netStart(j)) podDls.push_back({j, e.url});
}
static void podInit() { podLoad(); podView = 0; podScroll = 0; podMsg.clear(); }
static void podMiniBar() {
    if (musIdx < 0 || musIdx >= (int)gSongs.size()) return;
    int y = UIH - 64;
    rect(0, y, W, 64, Col{20, 22, 28});
    std::string t = gSongs[musIdx].title; while (t.size() > 3 && tw(t, F22) > 800) t.pop_back();
    text(t, F22, 70, y + 18, WHITE);
    btn(1060, y + 8, 150, 48, musPlaying && !musPaused ? "Pause" : "Play", Col{150, 60, 200}, WHITE);
}
static void drawPodcasts() {
    rect(0, 0, W, UIH, Col{36, 22, 52});
    int bottom = UIH - (musIdx >= 0 ? 64 : 0);
    if (podView == 0) {
        text("Podcasts", F72, 56, 14, WHITE);
        SDL_Rect clip = {0, 120, W, bottom - 120}; SDL_RenderSetClipRect(gR, &clip);
        int y = 124 - podScroll;
        for (size_t i = 0; i < gShows.size(); i++) {
            rect(56, y, 1168, 66, WHITE, 30); rect(56, y, 8, 66, Col{190, 110, 255});
            std::string t = gShows[i].name; while (t.size() > 3 && tw(t, F30) > 1000) t.pop_back();
            text(t, F30, 84, y + 16, WHITE); text("x", F30, 1196, y + 14, WHITE, 1, 150);
            y += 74;
        }
        if (gShows.empty()) { text("No shows yet. Pick a starter show or add a podcast link.", F22, 56, y, WHITE, 0, 190); y += 40; }
        text("Starter shows", F22, 56, y + 8, WHITE, 0, 170); y += 44;
        for (int i = 0; i < 3; i++) { rect(56, y, 1168, 54, WHITE, 16); text("+  " + POD_STARTER[i].name, F22, 84, y + 12, WHITE, 0, 220); y += 60; }
        btn(56, y + 6, 360, 54, "Add a podcast by link", Col{150, 60, 200}, WHITE);
        SDL_RenderSetClipRect(gR, nullptr);
    } else {
        btn(56, 20, 100, 44, "Back", Col{70, 50, 100}, WHITE);
        std::string t = podSel >= 0 && podSel < (int)gShows.size() ? gShows[podSel].name : ""; while (t.size() > 3 && tw(t, F44) > 900) t.pop_back();
        text(t, F44, 176, 14, WHITE);
        if (!podMsg.empty()) text(podMsg, F22, 56, 100, WHITE, 0, 200);
        SDL_Rect clip = {0, 84, W, bottom - 84}; SDL_RenderSetClipRect(gR, &clip);
        int y = 90 - podScroll;
        for (size_t i = 0; i < gEps.size(); i++) {
            if (y + 78 > 84 && y < bottom) {
                rect(56, y, 1168, 72, WHITE, 26);
                std::string tt = gEps[i].title; while (tt.size() > 3 && tw(tt, F22) > 820) tt.pop_back();
                text(tt, F22, 76, y + 10, WHITE); text(gEps[i].date, F16, 76, y + 42, WHITE, 0, 150);
                PodDl* d = podDlFor(gEps[i].url);
                if (d) { rect(960, y + 20, 240, 30, WHITE, 40); long long tot = d->j->dlTotal; int pw = tot > 0 ? (int)(240.0 * d->j->dlNow / tot) : (int)((d->j->dlNow / 40000) % 240); rect(960, y + 20, std::min(240, pw), 30, Col{190, 110, 255}); text(fmSize(d->j->dlNow), F16, 1080, y + 25, WHITE, 1); }
                else if (podHave(gEps[i])) btn(1000, y + 12, 200, 48, "Play", Col{150, 60, 200}, WHITE);
                else btn(1000, y + 12, 200, 48, "Download", Col{70, 50, 100}, WHITE);
            }
            y += 80;
        }
        SDL_RenderSetClipRect(gR, nullptr);
    }
    podMiniBar();
}
static void podcastsTap(int x, int y) {
    if (musIdx >= 0 && y >= UIH - 64) { if (inR(x, y, 1060, UIH - 56, 150, 48)) musToggle(); return; }
    if (podView == 0) {
        int yy = 124 - podScroll;
        for (size_t i = 0; i < gShows.size(); i++) {
            if (y >= yy && y < yy + 66) { if (x > 1150) { gShows.erase(gShows.begin() + i); podSave(); } else podOpen((int)i); return; }
            yy += 74;
        }
        if (gShows.empty()) yy += 40;
        yy += 44;
        for (int i = 0; i < 3; i++) { if (y >= yy && y < yy + 54) { podAddUrl(POD_STARTER[i].url, POD_STARTER[i].name); return; } yy += 60; }
        if (inR(x, y, 56, yy + 6, 360, 54)) { podInput.clear(); openKbStr(&podInput, 200, podAddEnter); }
    } else {
        if (inR(x, y, 56, 20, 100, 44)) { podView = 0; podScroll = 0; return; }
        int yy = 90 - podScroll;
        for (size_t i = 0; i < gEps.size(); i++) {
            if (y >= yy && y < yy + 72 && x >= 1000) {
                if (podDlFor(gEps[i].url)) return;
                if (podHave(gEps[i])) { musPlayFile(podFile(gEps[i])); toast("Playing"); } else podDownload(gEps[i]);
                return;
            }
            yy += 80;
        }
    }
}
static void podcastsDrag(int, int dy) { podScroll = std::max(0, podScroll - dy); }
static bool podcastsBack() { if (kbOpen) { hideKb(); return true; } if (podView == 1) { podView = 0; return true; } return false; }

// ---- radio
struct RStation { std::string name, url; };
static std::vector<RStation> gStations; static int rdCur = -1, rdState = 0, rdSlot = 0, rdScroll = 0; static NetJob* rdJob = nullptr;
static std::string rdReady, rdInput, rdMsg; static Uint32 rdRetryT = 0;
static void rdSave() { std::string o; for (auto& s : gStations) o += s.url + "\t" + s.name + "\n"; writeFile(DIR_ + "radio.txt", o); }
static void rdLoad() {
    gStations.clear(); std::string d;
    if (readFile(DIR_ + "radio.txt", d)) {
        size_t p = 0;
        while (p < d.size()) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); p = e + 1; size_t t = l.find('\t'); if (t != std::string::npos) gStations.push_back({l.substr(t + 1), l.substr(0, t)}); }
    }
    if (gStations.empty()) {
        gStations = {{"SomaFM Groove Salad", "https://ice1.somafm.com/groovesalad-128-mp3"}, {"Radio Paradise", "https://stream.radioparadise.com/mp3-128"}, {"KEXP Seattle", "https://kexp.streamguys1.com/kexp128.mp3"}, {"SomaFM Defcon", "https://ice1.somafm.com/defcon-128-mp3"}, {"BBC World Service", "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service"}};
        rdSave();
    }
}
static void rdDone(NetJob& j) {
    rdJob = nullptr;
    if (rdCur < 0) return;
    if (j.ok || j.dlNow > 20000) { rdReady = j.savePath; rdMsg.clear(); }
    else { rdMsg = "Couldn't reach this station."; rdRetryT = SDL_GetTicks() + 5000; }
}
static void rdFetch() {
    if (rdCur < 0 || rdCur >= (int)gStations.size() || rdJob) return;
    if (!netUp()) { rdMsg = gOffline ? "Offline mode is on." : "No internet connection."; rdRetryT = SDL_GetTicks() + 5000; return; }
    NetJob* j = new NetJob(); j->url = gStations[rdCur].url; j->savePath = DIR_ + (rdSlot ? "radio_b.mp3" : "radio_a.mp3"); j->maxMs = rdState == 0 ? 8000 : 12000; j->done = rdDone;
    rdSlot ^= 1;
    if (netStart(j)) rdJob = j;
}
static void rdStop() { if (rdJob) rdJob->cancel = true; rdCur = -1; rdState = 0; rdReady.clear(); rdMsg.clear(); musStop(); }
static void rdPlay(int i) { rdStop(); rdCur = i; rdState = 0; rdMsg = "Buffering..."; rdRetryT = 0; rdFetch(); }
static void radioInit() { rdLoad(); rdScroll = 0; }
static void radioUpdate(float) {
    if (rdCur < 0) return;
    if (!rdReady.empty() && !(musPlaying)) { std::string p = rdReady; rdReady.clear(); musPlayFile(p); if (!musPlaying) { rdStop(); rdMsg = "This station can't be played here."; return; } rdState = 1; rdMsg.clear(); rdFetch(); }
    else if (!rdJob && rdReady.empty() && rdRetryT && SDL_GetTicks() > rdRetryT) { rdRetryT = 0; rdFetch(); }
    else if (!rdJob && rdReady.empty() && rdState == 1 && musPlaying) rdFetch();
}
static void drawRadio() {
    rect(0, 0, W, UIH, Col{60, 30, 10});
    text("Radio", F72, 56, 14, WHITE);
    text("Beta: stations play in 12-second pieces, so there is a short delay at the start.", F16, 330, 56, WHITE, 0, 170);
    int bottom = UIH - 74;
    SDL_Rect clip = {0, 110, W, bottom - 110}; SDL_RenderSetClipRect(gR, &clip);
    int y = 114 - rdScroll;
    for (size_t i = 0; i < gStations.size(); i++) {
        bool cur = (int)i == rdCur;
        rect(56, y, 1168, 62, cur ? Col{230, 110, 20} : Col{255, 255, 255}, cur ? 255 : 30);
        std::string t = gStations[i].name; while (t.size() > 3 && tw(t, F30) > 900) t.pop_back();
        text(t, F30, 80, y + 14, WHITE); text("x", F30, 1196, y + 12, WHITE, 1, 150);
        y += 70;
    }
    btn(56, y + 8, 300, 54, "Add a station link", Col{230, 110, 20}, WHITE);
    SDL_RenderSetClipRect(gR, nullptr);
    rect(0, UIH - 74, W, 74, Col{30, 14, 4});
    std::string st = rdCur < 0 ? (rdMsg.empty() ? "Pick a station" : rdMsg) : (rdMsg.empty() ? "Playing: " + gStations[rdCur].name : rdMsg);
    text(st, F22, 56, UIH - 52, WHITE);
    if (rdCur >= 0) btn(1060, UIH - 64, 150, 54, "Stop", Col{190, 40, 60}, WHITE);
}
static void radioAddEnter() { hideKb(); std::string u = rdInput; rdInput.clear(); if (u.compare(0, 4, "http") != 0) { toast("The link must start with http"); return; } size_t s = u.find("//"); std::string nm = u.substr(s == std::string::npos ? 0 : s + 2); nm = nm.substr(0, nm.find('/')); gStations.push_back({nm, u}); rdSave(); }
static void radioTap(int x, int y) {
    if (y >= UIH - 74) { if (rdCur >= 0 && inR(x, y, 1060, UIH - 64, 150, 54)) rdStop(); return; }
    int yy = 114 - rdScroll;
    for (size_t i = 0; i < gStations.size(); i++) {
        if (y >= yy && y < yy + 62) { if (x > 1150) { if ((int)i == rdCur) rdStop(); else if (rdCur > (int)i) rdCur--; gStations.erase(gStations.begin() + i); rdSave(); } else rdPlay((int)i); return; }
        yy += 70;
    }
    if (inR(x, y, 56, yy + 8, 300, 54)) { rdInput.clear(); openKbStr(&rdInput, 200, radioAddEnter); }
}
static void radioDrag(int, int dy) { rdScroll = std::max(0, rdScroll - dy); }

// ================================================================ Terminal and PowerShell
struct TLine { std::string s; int kind; };   // kind 0 normal, 1 prompt/input, 2 error, 3 info
struct TermSess { std::vector<TLine> lines; std::string input, cwd = "sdmc:/"; std::vector<std::string> hist; int hpos = 0, scroll = 0; bool init = false; };
static TermSess gTerm[2];
static int gTermCur = 0;   // 0 Terminal, 1 PowerShell
static void tOut(TermSess& t, const std::string& s, int kind = 0) {
    size_t p = 0;
    while (p <= s.size()) { size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size(); t.lines.push_back({s.substr(p, e - p), kind}); p = e + 1; if (e == s.size()) break; }
    if (t.lines.size() > 600) t.lines.erase(t.lines.begin(), t.lines.begin() + (t.lines.size() - 600));
    t.scroll = 0;
}
static std::string tResolve(const TermSess& t, std::string p) {
    if (p.compare(0, 6, "sdmc:/") != 0) {
        if (!p.empty() && p[0] == '/') p = "sdmc:" + p;
        else p = fmJoin(t.cwd, p);
    }
    for (auto& c : p) if (c == '\\') c = '/';
    std::string rest = p.size() > 6 ? p.substr(6) : "";
    std::vector<std::string> seg; size_t a = 0;
    while (a <= rest.size()) { size_t b = rest.find('/', a); if (b == std::string::npos) b = rest.size(); std::string s = rest.substr(a, b - a); if (s == "..") { if (!seg.empty()) seg.pop_back(); } else if (!s.empty() && s != ".") seg.push_back(s); a = b + 1; }
    std::string o = "sdmc:/";
    for (size_t i = 0; i < seg.size(); i++) o += (i ? "/" : "") + seg[i];
    return o;
}
static std::vector<std::string> tTokens(const std::string& l) {
    std::vector<std::string> v; std::string cur; bool q = false, has = false; char qc = 0;
    for (char c : l) {
        if (q) { if (c == qc) q = false; else cur += c; }
        else if (c == '"' || c == '\'') { q = true; qc = c; has = true; }
        else if (c == ' ' || c == '\t') { if (has || !cur.empty()) { v.push_back(cur); cur.clear(); has = false; } }
        else cur += c;
    }
    if (has || !cur.empty()) v.push_back(cur);
    return v;
}
static bool tIsDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
static bool tExists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
static void tRunOps(TermSess& t) {
    static char buf[65536];
    for (auto& o : fmOps) {
        if (o.t == 0) mkdir(o.b.c_str(), 0777);
        else if (o.t == 1) { FILE* a = fopen(o.a.c_str(), "rb"); FILE* b = fopen(o.b.c_str(), "wb"); if (a && b) { size_t n; while ((n = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, n, b); } else tOut(t, "cannot copy " + o.a, 2); if (a) fclose(a); if (b) fclose(b); }
        else if (o.t == 2) remove(o.a.c_str());
        else if (o.t == 3) rmdir(o.a.c_str());
    }
    fmOps.clear();
}
static std::string tCanon(const std::string& c0) {
    std::string c = lower(c0);
    static const char* M[][2] = {{"get-childitem", "ls"}, {"gci", "ls"}, {"dir", "ls"}, {"set-location", "cd"}, {"sl", "cd"}, {"chdir", "cd"}, {"get-content", "cat"}, {"gc", "cat"}, {"type", "cat"},
        {"new-item", "newitem"}, {"ni", "newitem"}, {"remove-item", "rm"}, {"del", "rm"}, {"ri", "rm"}, {"erase", "rm"}, {"copy-item", "cp"}, {"copy", "cp"}, {"move-item", "mv"}, {"move", "mv"},
        {"get-process", "ps"}, {"gps", "ps"}, {"start-process", "launch"}, {"start", "launch"}, {"clear-host", "clear"}, {"cls", "clear"}, {"get-date", "date"}, {"write-output", "echo"}, {"write-host", "echo"},
        {"get-location", "pwd"}, {"gl", "pwd"}, {"get-help", "help"}, {"man", "help"}, {"stop-process", "kill"}, {"get-psdrive", "df"}, {"select-string", "grep"}, {"sls", "grep"}, {"quit", "exit"}, {"ipconfig", "ip"}, {"get-netipaddress", "ip"}, {nullptr, nullptr}};
    for (int i = 0; M[i][0]; i++) if (c == M[i][0]) return M[i][1];
    return c;
}
static int tFindApp(const std::string& q0) {
    std::string q = lower(q0);
    for (int a = 0; a < A_COUNT; a++) if (lower(ADEF[a].key) == q || lower(ADEF[a].name) == q) return a;
    for (int a = 0; a < A_COUNT; a++) if (lower(ADEF[a].name).find(q) != std::string::npos) return a;
    return -1;
}
static void termExec(TermSess& t, bool ps, const std::string& line) {
    auto tk = tTokens(line);
    if (tk.empty()) return;
    std::string cmd = tCanon(tk[0]);
    std::vector<std::string> args, flags;
    for (size_t i = 1; i < tk.size(); i++) { if (tk[i].size() > 1 && tk[i][0] == '-' && !(tk[i][1] >= '0' && tk[i][1] <= '9')) flags.push_back(lower(tk[i])); else args.push_back(tk[i]); }
    auto hasF = [&](const char* a, const char* b = "") { for (auto& f : flags) if (f == a || (*b && f == b)) return true; return false; };
    if (cmd == "help") {
        tOut(t, "Commands: ls cd pwd cat head mkdir touch rm cp mv echo clear date whoami uname ps launch kill df ip wifi grep wc tree open run bsod exit", 3);
        tOut(t, "Examples:  ls -l   cd switch   cat notes.txt   launch maps   cp a.txt b.txt   rm -r folder", 3);
        if (ps) tOut(t, "PowerShell names work too: Get-ChildItem, Set-Location, Get-Content, Copy-Item, Move-Item, Remove-Item, Get-Process, Start-Process ...", 3);
    } else if (cmd == "clear") { t.lines.clear(); }
    else if (cmd == "pwd") tOut(t, t.cwd);
    else if (cmd == "cd") {
        std::string d = args.empty() ? "sdmc:/" : tResolve(t, args[0]);
        if (tIsDir(d)) t.cwd = d; else tOut(t, "cd: no such directory: " + (args.empty() ? "" : args[0]), 2);
    } else if (cmd == "ls") {
        std::string d = args.empty() ? t.cwd : tResolve(t, args[0]);
        if (!tIsDir(d)) { tOut(t, "ls: cannot access " + d, 2); return; }
        std::vector<FmEnt> v; DIR* dp = opendir(d.c_str());
        if (dp) { struct dirent* e; while ((e = readdir(dp)) != nullptr) { std::string n = e->d_name; if (n == "." || n == "..") continue; struct stat st; if (stat(fmJoin(d, n).c_str(), &st) == 0) v.push_back({n, S_ISDIR(st.st_mode), (long long)st.st_size, (long long)st.st_mtime}); } closedir(dp); }
        std::sort(v.begin(), v.end(), [](const FmEnt& a, const FmEnt& b) { if (a.dir != b.dir) return a.dir; return lower(a.name) < lower(b.name); });
        bool lg = hasF("-l") || hasF("-la") || ps;
        if (lg) { for (auto& e : v) { char b[200]; time_t tt = (time_t)e.mt; struct tm* lt = localtime(&tt); char db[32] = ""; if (lt) snprintf(db, sizeof db, "%04d-%02d-%02d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday); snprintf(b, sizeof b, "%s %10s  %s  %s%s", e.dir ? "d" : "-", e.dir ? "" : fmSize(e.size).c_str(), db, e.name.c_str(), e.dir ? "/" : ""); tOut(t, b, e.dir ? 3 : 0); } }
        else { std::string row; for (auto& e : v) { row += e.name + (e.dir ? "/" : "") + "   "; if (row.size() > 90) { tOut(t, row); row.clear(); } } if (!row.empty()) tOut(t, row); }
        if (v.empty()) tOut(t, "(empty)", 3);
    } else if (cmd == "cat" || cmd == "head") {
        if (args.empty()) { tOut(t, cmd + ": missing file", 2); return; }
        std::string d; if (!readRaw(tResolve(t, args[0]), d)) { tOut(t, cmd + ": cannot open " + args[0], 2); return; }
        int lim = cmd == "head" ? 10 : 200, n = 0; size_t p = 0;
        while (p < d.size() && n < lim) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); if (!l.empty() && l.back() == '\r') l.pop_back(); tOut(t, l); p = e + 1; n++; }
        if (p < d.size()) tOut(t, "... (output cut)", 3);
    } else if (cmd == "mkdir") {
        if (args.empty()) { tOut(t, "mkdir: missing name", 2); return; }
        for (auto& a : args) if (mkdir(tResolve(t, a).c_str(), 0777) != 0) tOut(t, "mkdir: cannot create " + a, 2);
    } else if (cmd == "newitem") {
        std::string path; bool dir = false;
        for (size_t i = 1; i < tk.size(); i++) { std::string l = lower(tk[i]); if (l == "-itemtype" || l == "-type") { if (i + 1 < tk.size()) dir = lower(tk[++i]) == "directory"; } else if (l == "-name" || l == "-path") { if (i + 1 < tk.size()) path = tk[++i]; } else if (tk[i][0] != '-') path = tk[i]; }
        if (path.empty()) { tOut(t, "New-Item: missing path", 2); return; }
        std::string r = tResolve(t, path);
        if (dir) { if (mkdir(r.c_str(), 0777) != 0) tOut(t, "cannot create " + path, 2); } else { FILE* f = fopen(r.c_str(), "ab"); if (f) fclose(f); else tOut(t, "cannot create " + path, 2); }
    } else if (cmd == "touch") {
        for (auto& a : args) { FILE* f = fopen(tResolve(t, a).c_str(), "ab"); if (f) fclose(f); else tOut(t, "touch: cannot create " + a, 2); }
    } else if (cmd == "rm" || cmd == "rmdir") {
        if (args.empty()) { tOut(t, cmd + ": missing name", 2); return; }
        bool rec = hasF("-r", "-recurse") || hasF("-rf") || hasF("-fr");
        for (auto& a : args) {
            std::string r = tResolve(t, a);
            if (r == "sdmc:/") { tOut(t, "refusing to delete the SD card root", 2); continue; }
            if (!tExists(r)) { tOut(t, cmd + ": no such file: " + a, 2); continue; }
            if (tIsDir(r) && !rec) { if (rmdir(r.c_str()) != 0) tOut(t, a + " is a folder that isn't empty. Use -r to delete it with everything inside.", 2); continue; }
            if (fmBusy) { tOut(t, "Files is busy, try again", 2); continue; }
            fmOps.clear(); fmPlanDelete(r); tRunOps(t);
        }
    } else if (cmd == "cp" || cmd == "mv") {
        if (args.size() < 2) { tOut(t, cmd + ": need a source and a destination", 2); return; }
        std::string s = tResolve(t, args[0]), d = tResolve(t, args[1]);
        if (!tExists(s)) { tOut(t, cmd + ": no such file: " + args[0], 2); return; }
        if (tIsDir(d)) d = fmJoin(d, fmBase(s));
        if (tExists(d)) { tOut(t, cmd + ": destination exists", 2); return; }
        if (d.compare(0, s.size() + 1, s + "/") == 0) { tOut(t, cmd + ": can't copy a folder into itself", 2); return; }
        if (cmd == "mv" && rename(s.c_str(), d.c_str()) == 0) return;
        if (fmBusy) { tOut(t, "Files is busy, try again", 2); return; }
        fmOps.clear(); fmPlanCopy(s, d); if (cmd == "mv") fmPlanDelete(s); tRunOps(t);
    } else if (cmd == "echo") {
        std::string o; size_t gt = std::string::npos; for (size_t i = 0; i < args.size(); i++) if (args[i] == ">" || args[i] == ">>") { gt = i; break; }
        for (size_t i = 0; i < (gt == std::string::npos ? args.size() : gt); i++) o += (i ? " " : "") + args[i];
        if (gt != std::string::npos && gt + 1 < args.size()) { FILE* f = fopen(tResolve(t, args[gt + 1]).c_str(), args[gt] == ">>" ? "ab" : "wb"); if (f) { fwrite((o + "\n").data(), 1, o.size() + 1, f); fclose(f); } else tOut(t, "echo: cannot write", 2); }
        else tOut(t, o);
    } else if (cmd == "date") { char b[64]; snprintf(b, sizeof b, "%s %d %s %d  %02d:%02d:%02d", WD[dow(gNow.y, gNow.mo, gNow.d)], gNow.d, MONTHS[gNow.mo - 1], gNow.y, gNow.h, gNow.mi, gNow.s); tOut(t, b); }
    else if (cmd == "whoami") tOut(t, gUser);
    else if (cmd == "uname" || cmd == "ver" || cmd == "version") tOut(t, "Microsoft Windows [Version 6.3.9600] on Nintendo Switch (aarch64)");
    else if (cmd == "ps") { tOut(t, "App                  State", 3); for (int a = 0; a < A_COUNT; a++) if (running[a]) { char b[100]; snprintf(b, sizeof b, "%-20s %s", ADEF[a].name, a == curApp ? "foreground" : "background"); tOut(t, b); } }
    else if (cmd == "launch" || cmd == "run" || cmd == "open") {
        if (args.empty()) { tOut(t, cmd + ": what should I open?", 2); return; }
        std::string q = args[0]; for (size_t i = 1; i < args.size(); i++) q += " " + args[i];
        std::string r = tResolve(t, args[0]);
        if (tExists(r) && !tIsDir(r)) {
            int k = fmKind(r);
            if (k == 5) { saveAll(); if (R_SUCCEEDED(envSetNextLoad(r.c_str(), r.c_str()))) gRun = false; return; }
            if (k == 1) { gPhotoOpenPath = r; if (running[A_PHOTOS]) appReset(A_PHOTOS); pendingApp = A_PHOTOS; closeApp(false); return; }
            if (k == 2) { musPlayFile(r); tOut(t, "playing", 3); return; }
            if (k == 3) { gPdfOpenPath = r; if (running[A_PDF]) appReset(A_PDF); pendingApp = A_PDF; closeApp(false); return; }
            tOut(t, "no app for this file type", 2); return;
        }
        int a = tFindApp(q);
        if (a < 0 || !appAvail(a)) { tOut(t, "unknown or not installed: " + q, 2); return; }
        if (a == curApp) return;
        pendingApp = a; closeApp(false);
    } else if (cmd == "kill") {
        if (args.empty()) { tOut(t, "kill: which app?", 2); return; }
        int a = tFindApp(args[0]);
        if (a < 0 || !running[a]) { tOut(t, "not running: " + args[0], 2); return; }
        if (a == curApp) { tOut(t, "that's this window; use exit", 2); return; }
        appReset(a); tOut(t, "closed " + std::string(ADEF[a].name), 3);
    } else if (cmd == "df") { fmStorage(); tOut(t, "SD card: " + fmSize(fmFree) + " free of " + fmSize(fmTotalB)); }
    else if (cmd == "ip" || cmd == "wifi" || cmd == "ping") { tOut(t, netUp() ? "Wi-Fi: connected, signal " + std::to_string(gWifiBars) + "/3" : (gOffline ? "Offline mode is on" : "Wi-Fi: not connected")); }
    else if (cmd == "grep") {
        if (args.size() < 2) { tOut(t, "grep: usage: grep word file", 2); return; }
        std::string d; if (!readRaw(tResolve(t, args[1]), d)) { tOut(t, "grep: cannot open " + args[1], 2); return; }
        int n = 0; size_t p = 0; std::string w = lower(args[0]);
        while (p < d.size() && n < 100) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); p = e + 1; if (lower(l).find(w) != std::string::npos) { tOut(t, l); n++; } }
        if (!n) tOut(t, "(no matches)", 3);
    } else if (cmd == "wc") {
        if (args.empty()) { tOut(t, "wc: missing file", 2); return; }
        std::string d; if (!readRaw(tResolve(t, args[0]), d)) { tOut(t, "wc: cannot open " + args[0], 2); return; }
        int lines = 0, words = 0; bool in = false; for (char c : d) { if (c == '\n') lines++; if (c == ' ' || c == '\n' || c == '\t') in = false; else if (!in) { in = true; words++; } }
        tOut(t, std::to_string(lines) + " lines  " + std::to_string(words) + " words  " + std::to_string(d.size()) + " bytes");
    } else if (cmd == "tree") {
        std::function<void(const std::string&, const std::string&, int)> walk = [&](const std::string& d, const std::string& pre, int depth) {
            if (depth > 2) return;
            std::vector<FmEnt> v; DIR* dp = opendir(d.c_str()); if (!dp) return; struct dirent* e;
            while ((e = readdir(dp)) != nullptr) { std::string n = e->d_name; if (n == "." || n == "..") continue; v.push_back({n, tIsDir(fmJoin(d, n)), 0, 0}); } closedir(dp);
            std::sort(v.begin(), v.end(), [](const FmEnt& a, const FmEnt& b) { return lower(a.name) < lower(b.name); });
            for (auto& e2 : v) { if (t.lines.size() > 400) return; tOut(t, pre + e2.name + (e2.dir ? "/" : ""), e2.dir ? 3 : 0); if (e2.dir) walk(fmJoin(d, e2.name), pre + "  ", depth + 1); }
        };
        walk(args.empty() ? t.cwd : tResolve(t, args[0]), "", 0);
    } else if (cmd == "bsod") { bsodNow("MANUALLY_INITIATED_CRASH", "typed in the terminal"); }
    else if (cmd == "exit") { closeApp(false); }
    else tOut(t, (ps ? "The term '" : "command not found: ") + tk[0] + (ps ? "' is not recognized. Type help." : ". Type help."), 2);
}
static void termEnter() {
    TermSess& t = gTerm[gTermCur]; bool ps = gTermCur == 1;
    std::string prompt = ps ? "PS " + t.cwd + "> " : "winnx:" + t.cwd + "$ ";
    tOut(t, prompt + t.input, 1);
    if (!t.input.empty()) { t.hist.push_back(t.input); if (t.hist.size() > 50) t.hist.erase(t.hist.begin()); }
    t.hpos = (int)t.hist.size();
    std::string l = t.input; t.input.clear();
    termExec(t, ps, l);
    openKbStr(&t.input, 200, termEnter);
}
static void termInit(int which) {
    gTermCur = which; TermSess& t = gTerm[which];
    if (!t.init) { t.init = true; tOut(t, which ? "Windows PowerShell" : "Command Prompt", 3); tOut(t, "Type help for commands. Tap the screen to bring up the keyboard.", 3); }
    openKbStr(&t.input, 200, termEnter);
}
static void drawTerm() {
    TermSess& t = gTerm[gTermCur]; bool ps = gTermCur == 1;
    Col bg = ps ? Col{1, 36, 86} : Col{12, 12, 12};
    Col fg = ps ? Col{238, 237, 240} : Col{204, 204, 204};
    rect(0, 0, W, UIH, bg);
    int bottom = kbOpen ? KBTOP : UIH;
    int ly = bottom - 34;
    std::string prompt = ps ? "PS " + t.cwd + "> " : "winnx:" + t.cwd + "$ ";
    std::string cur = t.input + ((SDL_GetTicks() / 500) % 2 ? "_" : "");
    std::string full = prompt + cur;
    text(full, F22, 16, ly, ps ? Col{255, 255, 120} : Col{120, 255, 120});
    int rows = (ly - 40) / 26;
    int total = (int)t.lines.size();
    int maxScroll = std::max(0, total - rows);
    t.scroll = std::max(0, std::min(t.scroll, maxScroll));
    int first = std::max(0, total - rows - t.scroll);
    int y = ly - 26 * std::min(rows, total - first);
    for (int i = first; i < std::min(total, first + rows); i++, y += 26) {
        Col c = fg;
        switch (t.lines[i].kind) { case 1: c = ps ? Col{255, 255, 120} : Col{120, 255, 120}; break; case 2: c = Col{255, 110, 110}; break; case 3: c = ps ? Col{150, 200, 255} : Col{120, 190, 255}; break; default: break; }
        text(t.lines[i].s, F22, 16, y, c);
    }
    btn(W - 130, 46, 100, 40, "Prev", ps ? Col{20, 60, 120} : Col{50, 50, 50}, WHITE);
}
static void termTap(int x, int y) {
    TermSess& t = gTerm[gTermCur];
    if (inR(x, y, W - 130, 46, 100, 40)) { if (!t.hist.empty()) { t.hpos = std::max(0, t.hpos - 1); t.input = t.hist[t.hpos]; } return; }
    if (!kbOpen) openKbStr(&t.input, 200, termEnter);
}
static void termDrag(int, int dy) { gTerm[gTermCur].scroll += dy / 26; }

// ================================================================ Chess
struct ChPos { int8_t b[64]; int turn; bool cK[2], cQ[2]; int ep; };   // turn 0 white 1 black; b>0 white
struct ChMove { int8_t f, t, pr; };
static ChPos chP; static std::vector<ChPos> chHist;
static int chSel = -1, chLevel = 2, chSide = 0, chOver = 0, chPromoFrom = -1, chPromoTo = -1, chLast[2] = {-1, -1}; // chOver 1 mate 2 stalemate
static std::string chMsg;
static void chNew() {
    static const int8_t back[8] = {4, 2, 3, 5, 6, 3, 2, 4};
    memset(&chP, 0, sizeof chP);
    for (int i = 0; i < 8; i++) { chP.b[i] = -back[i]; chP.b[8 + i] = -1; chP.b[48 + i] = 1; chP.b[56 + i] = back[i]; }
    chP.cK[0] = chP.cK[1] = chP.cQ[0] = chP.cQ[1] = true; chP.ep = -1; chP.turn = 0;
    chHist.clear(); chSel = -1; chOver = 0; chMsg = ""; chLast[0] = chLast[1] = -1; chPromoFrom = -1;
}
static bool chAttacked(const ChPos& p, int sq, int by) {   // is sq attacked by colour 'by'
    int r = sq / 8, c = sq % 8, sg = by ? -1 : 1;
    int pr = r + (by ? -1 : 1);   // pawn of 'by' sits one row behind in its moving direction
    for (int dc = -1; dc <= 1; dc += 2) { int cc = c + dc; if (pr >= 0 && pr < 8 && cc >= 0 && cc < 8 && p.b[pr * 8 + cc] == sg * 1) return true; }
    static const int kn[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
    for (auto& k : kn) { int rr = r + k[0], cc = c + k[1]; if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && p.b[rr * 8 + cc] == sg * 2) return true; }
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        if (!dr && !dc) continue;
        int rr = r + dr, cc = c + dc;
        if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && p.b[rr * 8 + cc] == sg * 6) return true;
        bool diag = dr && dc; rr = r + dr; cc = c + dc;
        while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) {
            int v = p.b[rr * 8 + cc];
            if (v) { if (v == sg * 5 || v == sg * (diag ? 3 : 4)) return true; break; }
            rr += dr; cc += dc;
        }
    }
    return false;
}
static int chKing(const ChPos& p, int col) { for (int i = 0; i < 64; i++) if (p.b[i] == (col ? -6 : 6)) return i; return -1; }
static bool chInCheck(const ChPos& p, int col) { int k = chKing(p, col); return k >= 0 && chAttacked(p, k, !col); }
static void chApply(ChPos& p, const ChMove& m) {
    int pc = p.b[m.f], col = pc > 0 ? 0 : 1, ap = abs(pc);
    int cap = p.b[m.t];
    p.b[m.t] = pc; p.b[m.f] = 0;
    if (ap == 1 && m.t == p.ep) p.b[m.t + (col ? -8 : 8)] = 0;   // en passant
    if (ap == 1 && (m.t / 8 == 0 || m.t / 8 == 7)) p.b[m.t] = (col ? -1 : 1) * (m.pr ? m.pr : 5);
    if (ap == 6 && abs(m.t - m.f) == 2) { if (m.t > m.f) { p.b[m.f + 1] = p.b[m.f + 3]; p.b[m.f + 3] = 0; } else { p.b[m.f - 1] = p.b[m.f - 4]; p.b[m.f - 4] = 0; } }
    p.ep = (ap == 1 && abs(m.t - m.f) == 16) ? (m.f + m.t) / 2 : -1;
    if (ap == 6) p.cK[col] = p.cQ[col] = false;
    if (m.f == 0 || m.t == 0) p.cQ[1] = false; if (m.f == 7 || m.t == 7) p.cK[1] = false;
    if (m.f == 56 || m.t == 56) p.cQ[0] = false; if (m.f == 63 || m.t == 63) p.cK[0] = false;
    (void)cap; p.turn = !col;
}
static void chGen(const ChPos& p, std::vector<ChMove>& out, bool legalOnly = true) {
    int col = p.turn, sg = col ? -1 : 1;
    std::vector<ChMove> ps;
    auto add = [&](int f, int t) { ps.push_back({(int8_t)f, (int8_t)t, 0}); };
    for (int i = 0; i < 64; i++) {
        int v = p.b[i] * sg; if (v <= 0) continue;
        int r = i / 8, c = i % 8;
        if (v == 1) {
            int dir = col ? 1 : -1, r1 = r + dir;
            if (r1 < 0 || r1 > 7) continue;
            if (!p.b[r1 * 8 + c]) { add(i, r1 * 8 + c); int start = col ? 1 : 6; if (r == start && !p.b[(r1 + dir) * 8 + c]) add(i, (r1 + dir) * 8 + c); }
            for (int dc = -1; dc <= 1; dc += 2) { int cc = c + dc; if (cc < 0 || cc > 7) continue; int t = r1 * 8 + cc; if ((p.b[t] * sg < 0) || t == p.ep) add(i, t); }
        } else if (v == 2 || v == 6) {
            static const int kn[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
            static const int kg[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (int k = 0; k < 8; k++) { int rr = r + (v == 2 ? kn[k][0] : kg[k][0]), cc = c + (v == 2 ? kn[k][1] : kg[k][1]); if (rr < 0 || rr > 7 || cc < 0 || cc > 7) continue; if (p.b[rr * 8 + cc] * sg > 0) continue; add(i, rr * 8 + cc); }
            if (v == 6) {
                int home = col ? 4 : 60;
                if (i == home && !chAttacked(p, home, !col)) {
                    if (p.cK[col] && !p.b[home + 1] && !p.b[home + 2] && p.b[home + 3] == sg * 4 && !chAttacked(p, home + 1, !col) && !chAttacked(p, home + 2, !col)) add(i, home + 2);
                    if (p.cQ[col] && !p.b[home - 1] && !p.b[home - 2] && !p.b[home - 3] && p.b[home - 4] == sg * 4 && !chAttacked(p, home - 1, !col) && !chAttacked(p, home - 2, !col)) add(i, home - 2);
                }
            }
        } else {
            for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
                if (!dr && !dc) continue; bool diag = dr && dc;
                if (v == 3 && !diag) continue; if (v == 4 && diag) continue;
                int rr = r + dr, cc = c + dc;
                while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) { int q = p.b[rr * 8 + cc] * sg; if (q > 0) break; add(i, rr * 8 + cc); if (q < 0) break; rr += dr; cc += dc; }
            }
        }
    }
    if (!legalOnly) { out = ps; return; }
    for (auto& m : ps) { ChPos q = p; chApply(q, m); if (!chInCheck(q, col)) out.push_back(m); }
}
static int chEval(const ChPos& p) {   // from white's view
    static const int val[7] = {0, 100, 320, 330, 500, 900, 0};
    int s = 0;
    for (int i = 0; i < 64; i++) {
        int v = p.b[i]; if (!v) continue; int a = abs(v), sg = v > 0 ? 1 : -1, r = i / 8, c = i % 8;
        int rr = sg > 0 ? 7 - r : r;   // advancement
        int bonus = 0;
        if (a == 1) bonus = rr * 8 + ((c == 3 || c == 4) ? 6 : 0);
        else if (a == 2 || a == 3) bonus = 10 - (abs(3 - c) + abs(3 - r)) * 2 + (abs(c - 3.5) < 2 && abs(r - 3.5) < 2 ? 6 : 0);
        else if (a == 5) bonus = (c > 1 && c < 6) ? 3 : 0;
        s += sg * (val[a] + bonus);
    }
    return s;
}
static int chSearch(const ChPos& p, int depth, int alpha, int beta, long& nodes) {   // negamax, returns score for side to move
    nodes++;
    std::vector<ChMove> mv; chGen(p, mv);
    if (mv.empty()) return chInCheck(p, p.turn) ? -100000 - depth : 0;
    if (depth == 0) return (p.turn ? -1 : 1) * chEval(p);
    std::sort(mv.begin(), mv.end(), [&](const ChMove& a, const ChMove& b) { return abs(p.b[a.t]) > abs(p.b[b.t]); });
    for (auto& m : mv) {
        ChPos q = p; chApply(q, m);
        int sc = -chSearch(q, depth - 1, -beta, -alpha, nodes);
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
        if (nodes > 400000) break;
    }
    return alpha;
}
static void chCheckEnd() {
    std::vector<ChMove> mv; chGen(chP, mv);
    if (mv.empty()) { if (chInCheck(chP, chP.turn)) { chOver = 1; chMsg = chP.turn ? "Checkmate. White wins." : "Checkmate. Black wins."; } else { chOver = 2; chMsg = "Stalemate. It's a draw."; } }
    else chMsg = chInCheck(chP, chP.turn) ? "Check!" : "";
}
static void chSave() {
    std::string s = std::to_string(chLevel) + " " + std::to_string(chSide) + " " + std::to_string(chP.turn) + " " + std::to_string(chP.ep) + " " + std::to_string(chP.cK[0]) + std::to_string(chP.cK[1]) + std::to_string(chP.cQ[0]) + std::to_string(chP.cQ[1]) + "\n";
    for (int i = 0; i < 64; i++) s += std::to_string((int)chP.b[i]) + " ";
    writeFile(DIR_ + "chess.txt", s);
}
static void chLoad() {
    std::string d; if (!readFile(DIR_ + "chess.txt", d)) return;
    int lv, sd, tn, ep; char fl[8] = {0}; int n = 0;
    if (sscanf(d.c_str(), "%d %d %d %d %7s%n", &lv, &sd, &tn, &ep, fl, &n) < 5 || strlen(fl) < 4) return;
    ChPos q; memset(&q, 0, sizeof q); const char* p = d.c_str() + n;
    for (int i = 0; i < 64; i++) { int v; int k; if (sscanf(p, "%d%n", &v, &k) != 1) return; q.b[i] = (int8_t)v; p += k; }
    q.turn = tn; q.ep = ep; q.cK[0] = fl[0] == '1'; q.cK[1] = fl[1] == '1'; q.cQ[0] = fl[2] == '1'; q.cQ[1] = fl[3] == '1';
    chP = q; chLevel = lv; chSide = sd; chHist.clear(); chOver = 0; chSel = -1; chCheckEnd();
}
static void chAI() {
    if (chOver || chP.turn == chSide) return;
    std::vector<ChMove> mv; chGen(chP, mv); if (mv.empty()) return;
    int depth = chLevel + 1; long nodes = 0; int best = -1000000; ChMove bm = mv[0];
    std::sort(mv.begin(), mv.end(), [&](const ChMove& a, const ChMove& b) { return abs(chP.b[a.t]) > abs(chP.b[b.t]); });
    for (auto& m : mv) {
        ChPos q = chP; chApply(q, m);
        int sc = -chSearch(q, depth - 1, -1000000, 1000000, nodes) + (chLevel == 0 ? (int)(SDL_GetTicks() % 60) : 0);
        if (sc > best) { best = sc; bm = m; }
    }
    chHist.push_back(chP); chLast[0] = bm.f; chLast[1] = bm.t; chApply(chP, bm); chCheckEnd(); chSave();
}
static int chCell() { return 72; }
static void chBoardXY(int sq, int& x, int& y) { int r = sq / 8, c = sq % 8; if (chSide) { r = 7 - r; c = 7 - c; } x = 50 + c * 72; y = 48 + r * 72; }
static void drawChess() {
    rect(0, 0, W, UIH, Col{38, 36, 34});
    for (int i = 0; i < 64; i++) {
        int x, y; chBoardXY(i, x, y); bool lt = ((i / 8) + (i % 8)) % 2 == 0;
        Col c = lt ? Col{238, 220, 186} : Col{176, 124, 82};
        if (i == chLast[0] || i == chLast[1]) c = mix(c, Col{255, 230, 80}, 0.45f);
        rect(x, y, 72, 72, c);
    }
    std::vector<ChMove> mv; if (chSel >= 0) chGen(chP, mv);
    int ks = chInCheck(chP, chP.turn) ? chKing(chP, chP.turn) : -1;
    if (ks >= 0) { int x, y; chBoardXY(ks, x, y); rect(x, y, 72, 72, Col{230, 70, 60}); }
    if (chSel >= 0) { int x, y; chBoardXY(chSel, x, y); rect(x, y, 72, 72, Col{90, 180, 255}); }
    static const char* L = " PNBRQK";
    for (int i = 0; i < 64; i++) {
        int v = chP.b[i]; if (!v) continue; int x, y; chBoardXY(i, x, y);
        bool w = v > 0; char s[2] = {L[abs(v)], 0};
        disc(x + 36, y + 38, 27, w ? Col{20, 20, 20} : Col{10, 10, 10}, 255);
        disc(x + 36, y + 36, 26, w ? Col{250, 248, 240} : Col{52, 50, 56}, 255);
        text(s, F30, x + 36, y + 20, w ? Col{30, 30, 30} : Col{240, 240, 240}, 1);
    }
    for (auto& m : mv) if (m.f == chSel) { int x, y; chBoardXY(m.t, x, y); int i = m.t; if (chP.b[m.t]) { bool lt2 = ((i / 8) + (i % 8)) % 2 == 0; ring(x + 36, y + 36, 33, 5, Col{60, 200, 90}, lt2 ? Col{238, 220, 186} : Col{176, 124, 82}); } else disc(x + 36, y + 36, 11, Col{60, 200, 90}, 200); }
    text("Chess", F44, 700, 52, WHITE);
    const char* lv[3] = {"Easy", "Medium", "Hard"};
    text(chOver ? chMsg : (chP.turn == chSide ? "Your move" + (chMsg.empty() ? std::string() : "  -  " + chMsg) : "Thinking..."), F22, 700, 120, chMsg == "Check!" ? Col{255, 120, 100} : Col{210, 210, 210});
    text("Level", F16, 700, 176, Col{170, 170, 170});
    for (int i = 0; i < 3; i++) btn(700 + i * 160, 200, 148, 48, lv[i], chLevel == i ? Col{124, 58, 220} : Col{70, 68, 74}, WHITE);
    text("You play", F16, 700, 270, Col{170, 170, 170});
    btn(700, 294, 148, 48, "White", chSide == 0 ? Col{124, 58, 220} : Col{70, 68, 74}, WHITE);
    btn(860, 294, 148, 48, "Black", chSide == 1 ? Col{124, 58, 220} : Col{70, 68, 74}, WHITE);
    btn(700, 380, 148, 52, "New game", Col{40, 120, 70}, WHITE);
    btn(860, 380, 148, 52, "Undo", chHist.size() >= 2 ? Col{70, 68, 74} : Col{50, 50, 54}, WHITE);
    if (chPromoFrom >= 0) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170); rect(300, 240, 680, 170, Col{50, 48, 54});
        text("Promote pawn to", F22, 640, 256, WHITE, 1);
        const char* nm[4] = {"Queen", "Rook", "Bishop", "Knight"};
        for (int i = 0; i < 4; i++) btn(320 + i * 160, 320, 150, 60, nm[i], Col{124, 58, 220}, WHITE);
    }
}
static void chTryMove(int from, int to, int promo) {
    std::vector<ChMove> mv; chGen(chP, mv);
    for (auto& m : mv) if (m.f == from && m.t == to) {
        chHist.push_back(chP); chLast[0] = from; chLast[1] = to;
        ChMove mm = m; mm.pr = promo; chApply(chP, mm); chSel = -1; chCheckEnd(); chSave(); return;
    }
}
static void chessTap(int x, int y) {
    if (chPromoFrom >= 0) {
        for (int i = 0; i < 4; i++) if (inR(x, y, 320 + i * 160, 320, 150, 60)) { static const int pr[4] = {5, 4, 3, 2}; chTryMove(chPromoFrom, chPromoTo, pr[i]); chPromoFrom = -1; }
        return;
    }
    for (int i = 0; i < 3; i++) if (inR(x, y, 700 + i * 160, 200, 148, 48)) { chLevel = i; chSave(); return; }
    if (inR(x, y, 700, 294, 148, 48)) { chSide = 0; chNew(); chSave(); return; }
    if (inR(x, y, 860, 294, 148, 48)) { chSide = 1; chNew(); chSave(); return; }
    if (inR(x, y, 700, 380, 148, 52)) { chNew(); chSave(); return; }
    if (inR(x, y, 860, 380, 148, 52)) { if (chHist.size() >= 2 && !chOver) { chP = chHist[chHist.size() - 2]; chHist.resize(chHist.size() - 2); chSel = -1; chMsg = ""; chLast[0] = chLast[1] = -1; chSave(); } return; }
    if (chOver || chP.turn != chSide) return;
    for (int i = 0; i < 64; i++) {
        int bx, by; chBoardXY(i, bx, by); if (!inR(x, y, bx, by, 72, 72)) continue;
        if (chSel >= 0) {
            std::vector<ChMove> mv; chGen(chP, mv); bool ok = false;
            for (auto& m : mv) if (m.f == chSel && m.t == i) ok = true;
            if (ok) {
                bool pawn = abs(chP.b[chSel]) == 1 && (i / 8 == 0 || i / 8 == 7);
                if (pawn) { chPromoFrom = chSel; chPromoTo = i; return; }
                chTryMove(chSel, i, 0); return;
            }
        }
        chSel = (chP.b[i] && (chP.b[i] > 0) == (chP.turn == 0) && chSel != i) ? i : -1;
        return;
    }
}
static void chessUpdate() { if (!chOver && chP.turn != chSide && chPromoFrom < 0) { static int delay = 0; if (++delay > 12) { delay = 0; chAI(); } } }
static void chessInit() { static bool first = true; if (first) { first = false; chNew(); chLoad(); } }

// ================================================================ Acrobat Reader (text PDFs, beta)
#include <zlib.h>
static std::vector<std::string> pdfLines; static std::string pdfName, pdfMsg; static int pdfScroll = 0, pdfFont = 0, pdfPages = 0; static std::string pdfRaw;
static bool pdfInflate(const std::string& in, std::string& out) {
    z_stream z; memset(&z, 0, sizeof z); if (inflateInit(&z) != Z_OK) return false;
    z.next_in = (Bytef*)in.data(); z.avail_in = (uInt)in.size(); char buf[32768]; int r;
    do { z.next_out = (Bytef*)buf; z.avail_out = sizeof buf; r = inflate(&z, Z_NO_FLUSH); if (r != Z_OK && r != Z_STREAM_END) break; out.append(buf, sizeof buf - z.avail_out); if (out.size() > 20000000) break; } while (r != Z_STREAM_END);
    inflateEnd(&z); return !out.empty();
}
static std::string pdfText(const std::string& c) {
    std::string o; size_t p = 0;
    while (true) {
        size_t b = c.find("BT", p); if (b == std::string::npos) break;
        size_t e = c.find("ET", b); if (e == std::string::npos) e = c.size();
        std::string seg = c.substr(b + 2, e - b - 2);
        for (size_t i = 0; i < seg.size(); i++) {
            char ch = seg[i];
            if (ch == '(') {
                int depth = 1; i++;
                while (i < seg.size() && depth > 0) {
                    char d = seg[i];
                    if (d == '\\' && i + 1 < seg.size()) { char n = seg[++i]; if (n == 'n') o += '\n'; else if (n >= '0' && n <= '7') { int v = n - '0'; int k = 0; while (k < 2 && i + 1 < seg.size() && seg[i + 1] >= '0' && seg[i + 1] <= '7') { v = v * 8 + seg[++i] - '0'; k++; } if (v >= 32 && v < 127) o += (char)v; else if (v == 146 || v == 147 || v == 148) o += '\''; } else if (n != '\r' && n != '\n') o += n; }
                    else if (d == '(') { depth++; o += d; } else if (d == ')') { if (--depth > 0) o += d; } else o += d;
                    i++;
                }
                i--;
            } else if (ch == '[') { }
            else if (ch == '-' || (ch >= '0' && ch <= '9')) { size_t j = i; while (j < seg.size() && (seg[j] == '-' || seg[j] == '.' || (seg[j] >= '0' && seg[j] <= '9'))) j++; if (j > i && j < seg.size() && seg[j - 1] != '-' ) { double v = atof(seg.substr(i, j - i).c_str()); if (v < -200 && !o.empty() && o.back() != ' ' && o.back() != '\n') o += ' '; } if (j > i) i = j - 1; }
            else if ((ch == 'T' && i + 1 < seg.size() && (seg[i + 1] == 'd' || seg[i + 1] == 'D' || seg[i + 1] == '*')) || ch == '\'' || ch == '"') { if (!o.empty() && o.back() != '\n') o += '\n'; }
        }
        if (!o.empty() && o.back() != '\n') o += '\n';
        p = e + 2;
    }
    return o;
}
static void pdfOpen(const std::string& path) {
    pdfRaw = path; pdfLines.clear(); pdfMsg.clear(); pdfScroll = 0; pdfPages = 0; pdfName = fmBase(path);
    std::string raw; if (!readRaw(path, raw)) { pdfMsg = "Couldn't open this file."; return; }
    if (raw.compare(0, 4, "%PDF") != 0) { pdfMsg = "This isn't a PDF."; return; }
    if (raw.find("/Encrypt") != std::string::npos) { pdfMsg = "This PDF is password-protected. Acrobat Reader beta can't open it."; return; }
    std::string all; size_t p = 0; int streams = 0;
    while (true) {
        size_t s = raw.find("stream", p); if (s == std::string::npos) break;
        if (s >= 3 && raw.compare(s - 3, 3, "end") == 0) { p = s + 6; continue; }
        size_t ds = raw.rfind("obj", s); std::string dict = ds == std::string::npos ? "" : raw.substr(ds, s - ds);
        size_t b = s + 6; if (b < raw.size() && raw[b] == '\r') b++; if (b < raw.size() && raw[b] == '\n') b++;
        size_t e = raw.find("endstream", b); if (e == std::string::npos) break;
        p = e + 9;
        if (dict.find("/Type/Page") != std::string::npos && dict.find("/Type/Pages") == std::string::npos) pdfPages++;
        if (dict.find("/Image") != std::string::npos || dict.find("/FontFile") != std::string::npos || dict.find("/Length1") != std::string::npos || dict.find("/XRef") != std::string::npos || dict.find("/ObjStm") != std::string::npos) continue;
        std::string data = raw.substr(b, e - b), dec;
        if (dict.find("FlateDecode") != std::string::npos) { if (!pdfInflate(data, dec)) continue; } else dec = data;
        if (dec.find("BT") == std::string::npos) continue;
        std::string t = pdfText(dec); if (t.empty()) continue;
        all += t + "\n"; if (++streams > 3000) break;
    }
    std::string cur; 
    for (char c : all) { if (c == '\n') { for (auto& l : wrapText(cur.empty() ? " " : cur, pdfFont ? F30 : F22, 1140)) pdfLines.push_back(l); cur.clear(); } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) cur += c; }
    if (pdfLines.empty()) pdfMsg = "No readable text found. This PDF may be scanned images or use embedded fonts the beta reader can't decode.";
}
static void pdfInit() { if (!gPdfOpenPath.empty()) { std::string p = gPdfOpenPath; gPdfOpenPath.clear(); pdfOpen(p); } }
static void drawPdf() {
    rect(0, 0, W, UIH, Col{82, 86, 89});
    if (pdfName.empty()) {
        text("Acrobat Reader", F44, 640, 220, WHITE, 1); text("Open a PDF from Files to read it here.", F22, 640, 290, Col{220, 220, 220}, 1);
        btn(540, 350, 200, 56, "Open Files", Col{200, 30, 40}, WHITE); return;
    }
    rect(0, 40, W, 50, Col{50, 52, 54});
    text(pdfName + (pdfPages ? "   -   " + std::to_string(pdfPages) + " pages" : ""), F22, 20, 50, WHITE);
    btn(W - 250, 46, 100, 38, "A-", Col{80, 82, 86}, WHITE); btn(W - 140, 46, 100, 38, "A+", Col{80, 82, 86}, WHITE);
    rect(60, 100, W - 120, UIH - 100, Col{252, 252, 250});
    int lh = pdfFont ? 40 : 30, rows = (UIH - 120) / lh, total = (int)pdfLines.size();
    pdfScroll = std::max(0, std::min(pdfScroll, std::max(0, total - rows)));
    if (!pdfMsg.empty()) { for (auto& l : wrapText(pdfMsg, F22, 1000)) { static int yy; (void)yy; } int y = 140; for (auto& l : wrapText(pdfMsg, F22, 1000)) { text(l, F22, 100, y, Col{60, 60, 60}); y += 32; } return; }
    for (int i = 0; i < rows && pdfScroll + i < total; i++) text(pdfLines[pdfScroll + i], pdfFont ? F30 : F22, 90, 110 + i * lh, Col{30, 30, 30});
    int sbH = std::max(30, (UIH - 100) * rows / std::max(rows, total)); int sbY = 100 + (total > rows ? (UIH - 100 - sbH) * pdfScroll / (total - rows) : 0);
    rect(W - 54, sbY, 8, sbH, Col{150, 150, 150});
}
static void pdfTap(int x, int y) {
    if (pdfName.empty()) { if (inR(x, y, 540, 350, 200, 56)) { pendingApp = A_FILES; closeApp(false); } return; }
    int old = pdfFont;
    if (inR(x, y, W - 250, 46, 100, 38)) pdfFont = 0; else if (inR(x, y, W - 140, 46, 100, 38)) pdfFont = 1;
    if (old != pdfFont) pdfOpen(pdfRaw);
}
static void pdfDrag(int, int dy) { pdfScroll -= dy / (pdfFont ? 40 : 30); }

// ================================================================ Camera (Joy-Con IR sensor, beta)
static SDL_Texture* camTex = nullptr; static bool camOn = false, camTint = true, camInv = false; static std::string camMsg; static u8* camBuf = nullptr; static int camShots = 0; static float camFlash = 0;
#ifndef WNX_HOST
static IrsIrCameraHandle camH; static bool camIrsInit = false;
#endif
static void camStop() {
#ifndef WNX_HOST
    if (camOn) irsStopImageProcessor(camH);
#endif
    camOn = false;
}
static void camStart() {
    camMsg.clear();
#ifndef WNX_HOST
    if (!camIrsInit) { if (R_FAILED(irsInitialize())) { camMsg = "The IR sensor service isn't available."; return; } camIrsInit = true; }
    if (!camBuf) camBuf = (u8*)aligned_alloc(0x1000, 0x14000);
    if (!camBuf) { camMsg = "Out of memory."; return; }
    memset(camBuf, 0, 0x14000);
    Result rc = irsGetIrCameraHandle(&camH, HidNpadIdType_No1);
    if (R_FAILED(rc)) rc = irsGetIrCameraHandle(&camH, HidNpadIdType_Handheld);
    if (R_FAILED(rc)) { camMsg = "No IR camera found. Attach the right Joy-Con."; return; }
    IrsImageTransferProcessorConfig cfg; irsGetDefaultImageTransferProcessorConfig(&cfg);
    cfg.format = IrsImageTransferProcessorFormat_320x240;
    rc = irsRunImageTransferProcessor(camH, &cfg, 0x14000);
    if (R_FAILED(rc)) { camMsg = "Couldn't start the IR camera. Attach the right Joy-Con."; return; }
    camOn = true;
#else
    camMsg = "The IR camera only works on a real Switch with the right Joy-Con attached.";
#endif
}
static void camUpdate() {
#ifndef WNX_HOST
    if (!camOn || !camBuf) return;
    IrsImageTransferProcessorState st;
    if (R_FAILED(irsGetImageTransferProcessorState(camH, camBuf, 0x14000, &st))) { camMsg = "Lost the IR camera."; camStop(); return; }
    if (!camTex) camTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (!camTex) return;
    static u32 px[320 * 240];
    for (int i = 0; i < 320 * 240; i++) { int v = camBuf[i]; if (camInv) v = 255 - v; v = std::min(255, v * 3 / 2); px[i] = camTint ? (0xFF000000u | ((u32)v << 8) | ((u32)(v / 4) << 16) | (u32)(v / 4)) : (0xFF000000u | v | (v << 8) | (v << 16)); }
    SDL_UpdateTexture(camTex, nullptr, px, 320 * 4);
#endif
}
static void camSnap() {
    if (!camTex) { camMsg = "Nothing to capture yet."; return; }
    mkdir("sdmc:/Pictures", 0777);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return;
    SDL_Texture* old = SDL_GetRenderTarget(gR);
    SDL_Texture* rt = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 320, 240);
    if (rt) { SDL_SetRenderTarget(gR, rt); SDL_RenderCopy(gR, camTex, nullptr, nullptr); SDL_RenderReadPixels(gR, nullptr, SDL_PIXELFORMAT_RGBA32, s->pixels, s->pitch); SDL_SetRenderTarget(gR, old); SDL_DestroyTexture(rt); }
    char n[96]; snprintf(n, sizeof n, "sdmc:/Pictures/IR_%04d%02d%02d_%02d%02d%02d.png", gNow.y, gNow.mo, gNow.d, gNow.h, gNow.mi, gNow.s);
    if (IMG_SavePNG(s, n) == 0) { camShots++; camFlash = 1; toast("Saved to Pictures"); } else camMsg = "Couldn't save the picture.";
    SDL_FreeSurface(s);
}
static void camInit() { camStart(); }
static void drawCamera() {
    rect(0, 0, W, UIH, Col{16, 18, 20});
    if (camTex && camOn) { SDL_Rect r{320, 90, 640, 480}; SDL_RenderCopy(gR, camTex, nullptr, &r); }
    else { rect(320, 90, 640, 480, Col{30, 32, 36}); text(camMsg.empty() ? "Starting..." : camMsg, F22, 640, 320, Col{220, 220, 220}, 1); }
    if (camFlash > 0) { rect(320, 90, 640, 480, WHITE, (int)(camFlash * 200)); camFlash -= 0.06f; }
    text("Camera", F30, 40, 50, WHITE); text("IR beta", F16, 40, 96, Col{170, 170, 170});
    btn(40, 160, 220, 56, camTint ? "Night green: on" : "Night green: off", Col{60, 64, 72}, WHITE);
    btn(40, 230, 220, 56, camInv ? "Invert: on" : "Invert: off", Col{60, 64, 72}, WHITE);
    btn(40, 300, 220, 56, camOn ? "Stop" : "Start", Col{60, 64, 72}, WHITE);
    disc(640, 622, 34, WHITE); disc(640, 622, 28, Col{200, 40, 50});
    if (camShots) text(std::to_string(camShots) + " saved", F16, 700, 612, Col{170, 170, 170});
}
static void camTap(int x, int y) {
    if (inR(x, y, 40, 160, 220, 56)) camTint = !camTint;
    else if (inR(x, y, 40, 230, 220, 56)) camInv = !camInv;
    else if (inR(x, y, 40, 300, 220, 56)) { if (camOn) camStop(); else camStart(); }
    else if (inR(x - 606, y - 588, 0, 0, 68, 68) || (x > 606 && x < 674 && y > 588 && y < 656)) camSnap();
}

// ================================================================ Theme Store
static int thTab = 0, thScroll = 0; static float thFlash = 0; static int thFlashI = -1;
static const char* TH_TABS[5] = {"All", "Dark", "Bright", "Nature", "Installed"};
static void thApply(int i) { gTheme = 12 + i; gBgPat = STHEMES[i].pat; gStartPic = false; saveCfg(); }
static bool thVisible(int i) {
    if (thTab == 0) return true;
    if (thTab == 4) return (gThOwned >> i) & 1;
    return STHEMES[i].tag & (1 << (thTab - 1));
}
static void thGrad(int x, int y, int w, int h, Col a, Col b) {
    for (int k = 0; k < h; k += 3) rect(x, y + k, w, std::min(3, h - k), mix(a, b, (float)k / (float)h));
}
static void thPreview(int x, int y, int w, int h, const Theme& t) {
    thGrad(x, y, w, h, t.top, t.bottom);
    disc(x + w - 30, y + 28, 34, WHITE, 18); disc(x + 40, y + h - 20, 46, WHITE, 14);
    int tw0 = (w - 40) / 3;
    rect(x + 14, y + 14, tw0 * 2 + 6, 56, t.accent); rect(x + 14 + tw0 * 2 + 12, y + 14, tw0 - 12, 56, mix(t.accent, t.top, 0.5f));
    rect(x + 14, y + 76, tw0 - 6, 40, mix(t.accent, WHITE, 0.25f)); rect(x + 14 + tw0, y + 76, tw0 - 6, 40, mix(t.accent, BLACK, 0.3f)); rect(x + 14 + tw0 * 2, y + 76, tw0 + 6, 40, t.accent);
}
static std::vector<int> thList() { std::vector<int> v; for (int i = 0; i < NSTH; i++) if (thVisible(i)) v.push_back(i); return v; }
static const int TH_CW = 272, TH_CH = 262, TH_GAP = 24, TH_X0 = 60, TH_Y0 = 170;
static void drawThemes() {
    const Theme& cur = THEMES[gTheme];
    thGrad(0, 0, W, UIH, mix(cur.top, BLACK, 0.55f), mix(cur.bottom, BLACK, 0.6f));
    auto v = thList();
    int rows = ((int)v.size() + 3) / 4;
    int contentH = rows * (TH_CH + TH_GAP);
    int maxS = std::max(0, TH_Y0 + contentH - UIH + 20);
    thScroll = std::max(0, std::min(thScroll, maxS));
    SDL_Rect clip{0, 150, W, UIH - 150}; SDL_RenderSetClipRect(gR, &clip);
    for (int n = 0; n < (int)v.size(); n++) {
        int i = v[n], c = n % 4, r = n / 4;
        int x = TH_X0 + c * (TH_CW + TH_GAP), y = TH_Y0 + r * (TH_CH + TH_GAP) - thScroll;
        if (y > UIH || y + TH_CH < 150) continue;
        rect(x, y, TH_CW, TH_CH, Col{20, 20, 26}, 235);
        thPreview(x, y, TH_CW, 140, THEMES[12 + i]);
        text(STHEMES[i].name, F22, x + 12, y + 148, WHITE);
        text("Windows Studio", F16, x + 12, y + 180, Col{170, 170, 180});
        bool own = (gThOwned >> i) & 1, on = gTheme == 12 + i;
        if (on) btn(x + 12, y + 212, TH_CW - 24, 38, "Applied", Col{60, 150, 80}, WHITE);
        else btn(x + 12, y + 212, TH_CW - 24, 38, own ? "Apply" : "Get", own ? Col{70, 70, 80} : cur.accent, WHITE);
        if (thFlashI == i && thFlash > 0) rect(x, y, TH_CW, TH_CH, WHITE, (int)(thFlash * 120));
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (thFlash > 0) thFlash -= 0.05f;
    rect(0, 0, W, 150, mix(cur.top, BLACK, 0.7f));
    text("Theme Store", F44, 60, 36, WHITE);
    text(std::string("Current: ") + (gTheme >= 12 ? STHEMES[gTheme - 12].name : "Classic"), F16, 60, 100, Col{200, 200, 210});
    for (int i = 0; i < 5; i++) { int bx = 420 + i * 150; btn(bx, 46, 140, 44, TH_TABS[i], thTab == i ? cur.accent : Col{50, 50, 60}, WHITE); }
    btn(W - 230, 100, 190, 34, "Reset to classic", Col{60, 60, 72}, WHITE);
}
static void themesTap(int x, int y) {
    for (int i = 0; i < 5; i++) if (inR(x, y, 420 + i * 150, 46, 140, 44)) { thTab = i; thScroll = 0; return; }
    if (inR(x, y, W - 230, 100, 190, 34)) { gTheme = 0; gBgPat = 1; gStartPic = false; saveCfg(); toast("Classic theme restored"); return; }
    if (y < 150) return;
    auto v = thList();
    for (int n = 0; n < (int)v.size(); n++) {
        int i = v[n], c = n % 4, r = n / 4;
        int cx = TH_X0 + c * (TH_CW + TH_GAP), cy = TH_Y0 + r * (TH_CH + TH_GAP) - thScroll;
        if (!inR(x, y, cx, cy, TH_CW, TH_CH)) continue;
        if (!((gThOwned >> i) & 1)) { gThOwned |= 1u << i; toast(std::string(STHEMES[i].name) + " installed"); }
        thApply(i); thFlash = 1; thFlashI = i;
        return;
    }
}
static void themesDrag(int, int dy) { thScroll -= dy; }

// ================================================================ Converter
struct CvUnit { const char* n; double f; };
struct CvCat { const char* name; std::vector<CvUnit> u; };
static const std::vector<CvCat>& cvCats() {
    static std::vector<CvCat> c = {
        {"Length", {{"mm", .001}, {"cm", .01}, {"m", 1}, {"km", 1000}, {"in", .0254}, {"ft", .3048}, {"yd", .9144}, {"mi", 1609.344}}},
        {"Mass", {{"g", .001}, {"kg", 1}, {"t", 1000}, {"oz", .0283495}, {"lb", .45359237}}},
        {"Temp", {{"C", 1}, {"F", 1}, {"K", 1}}},
        {"Volume", {{"ml", .001}, {"l", 1}, {"cup", .2365882}, {"fl oz", .0295735}, {"gal", 3.785412}}},
        {"Speed", {{"m/s", 1}, {"km/h", .2777778}, {"mph", .44704}, {"knot", .5144444}}},
        {"Data", {{"B", 1}, {"KB", 1024}, {"MB", 1048576}, {"GB", 1073741824.0}, {"TB", 1099511627776.0}}},
        {"Time", {{"sec", 1}, {"min", 60}, {"hour", 3600}, {"day", 86400}, {"week", 604800}}},
    };
    return c;
}
static int cvCat = 0, cvFrom = 2, cvTo = 3; static std::string cvIn = "1";
static double cvCompute() {
    double v = atof(cvIn.c_str());
    auto& cat = cvCats()[cvCat];
    if (cvCat == 2) {   // temperature
        double c = cvFrom == 0 ? v : (cvFrom == 1 ? (v - 32) * 5 / 9 : v - 273.15);
        return cvTo == 0 ? c : (cvTo == 1 ? c * 9 / 5 + 32 : c + 273.15);
    }
    return v * cat.u[cvFrom].f / cat.u[cvTo].f;
}
static void cvPick(int c) { cvCat = c; cvFrom = 0; cvTo = std::min(1, (int)cvCats()[c].u.size() - 1); }
static void drawConv() {
    rect(0, 0, W, UIH, Col{24, 34, 46});
    auto& cats = cvCats(); Col acc = ADEF[A_CONV].col;
    text("Converter", F30, 24, 46, WHITE);
    for (int i = 0; i < (int)cats.size(); i++) btn(24, 100 + i * 70, 170, 58, cats[i].name, cvCat == i ? acc : Col{50, 62, 78}, WHITE);
    auto& u = cats[cvCat].u;
    text("From", F16, 220, 60, Col{170, 190, 210}); text("To", F16, 410, 60, Col{170, 190, 210});
    for (int i = 0; i < (int)u.size(); i++) {
        btn(220, 90 + i * 62, 170, 54, u[i].n, cvFrom == i ? acc : Col{50, 62, 78}, WHITE);
        btn(410, 90 + i * 62, 170, 54, u[i].n, cvTo == i ? Col{60, 150, 90} : Col{50, 62, 78}, WHITE);
    }
    rect(620, 60, 630, 150, Col{14, 20, 30});
    text(cvIn + " " + u[cvFrom].n, F44, 1236, 70, WHITE, 2);
    char b[64]; double r = cvCompute(); snprintf(b, sizeof b, "%.8g", r);
    text(std::string("= ") + b + " " + u[cvTo].n, F44, 1236, 130, Col{140, 230, 160}, 2);
    static const char* K[16] = {"7", "8", "9", "DEL", "4", "5", "6", "C", "1", "2", "3", "+/-", "0", ".", "Swap", ""};
    for (int i = 0; i < 15; i++) { int c = i % 4, rw = i / 4; btn(620 + c * 160, 240 + rw * 100, 150, 90, K[i], i % 4 == 3 ? Col{70, 84, 104} : Col{44, 56, 72}, WHITE); }
}
static void convTap(int x, int y) {
    auto& cats = cvCats(); auto& u = cats[cvCat].u;
    for (int i = 0; i < (int)cats.size(); i++) if (inR(x, y, 24, 100 + i * 70, 170, 58)) { cvPick(i); return; }
    for (int i = 0; i < (int)u.size(); i++) { if (inR(x, y, 220, 90 + i * 62, 170, 54)) cvFrom = i; if (inR(x, y, 410, 90 + i * 62, 170, 54)) cvTo = i; }
    static const char* K[16] = {"7", "8", "9", "DEL", "4", "5", "6", "C", "1", "2", "3", "+/-", "0", ".", "Swap", ""};
    for (int i = 0; i < 15; i++) {
        int c = i % 4, rw = i / 4; if (!inR(x, y, 620 + c * 160, 240 + rw * 100, 150, 90)) continue;
        std::string k = K[i];
        if (k == "DEL") { if (!cvIn.empty()) cvIn.pop_back(); if (cvIn.empty()) cvIn = "0"; }
        else if (k == "C") cvIn = "0";
        else if (k == "+/-") { if (!cvIn.empty() && cvIn[0] == '-') cvIn.erase(0, 1); else if (cvIn != "0") cvIn = "-" + cvIn; }
        else if (k == "Swap") { std::swap(cvFrom, cvTo); }
        else if (k == ".") { if (cvIn.find('.') == std::string::npos) cvIn += "."; }
        else { if (cvIn == "0") cvIn = k; else if (cvIn.size() < 14) cvIn += k; }
    }
}

// ================================================================ Blocks (falling blocks)
static const int BW = 10, BH = 20, BC = 30, BX0 = 480, BY0 = 34;
static int bBoard[BH][BW]; static int bCur[4][4], bNext = 0, bCurT = 0, bX = 3, bY = 0, bScore = 0, bLines = 0, bLevel = 1, bBest = 0; static bool bOver = false, bPaused = false, bInit = false; static float bTimer = 0; static int bTX = 0, bTY = 0; static float bTT = 0;
static const int BSHAPES[7][4][4] = {
    {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, {{2,0,0,0},{2,2,2,0},{0,0,0,0},{0,0,0,0}}, {{0,0,3,0},{3,3,3,0},{0,0,0,0},{0,0,0,0}},
    {{0,4,4,0},{0,4,4,0},{0,0,0,0},{0,0,0,0}}, {{0,5,5,0},{5,5,0,0},{0,0,0,0},{0,0,0,0}}, {{0,6,0,0},{6,6,6,0},{0,0,0,0},{0,0,0,0}}, {{7,7,0,0},{0,7,7,0},{0,0,0,0},{0,0,0,0}} };
static Col bCol(int v) { static const Col c[8] = {{0, 0, 0}, {0, 210, 230}, {40, 80, 230}, {240, 150, 30}, {240, 220, 40}, {60, 200, 80}, {170, 60, 210}, {230, 50, 50}}; return c[v & 7]; }
static bool bFits(int sx, int sy, int m[4][4]) {
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (m[r][c]) { int x = sx + c, y = sy + r; if (x < 0 || x >= BW || y >= BH) return false; if (y >= 0 && bBoard[y][x]) return false; }
    return true;
}
static void bSave() { writeFile(DIR_ + "blocks.txt", std::to_string(bBest)); }
static void bSpawn() {
    bCurT = bNext; bNext = rand() % 7;
    memcpy(bCur, BSHAPES[bCurT], sizeof bCur); bX = 3; bY = 0;
    if (!bFits(bX, bY, bCur)) { bOver = true; if (bScore > bBest) { bBest = bScore; bSave(); } }
}
static void bNew() { memset(bBoard, 0, sizeof bBoard); bScore = 0; bLines = 0; bLevel = 1; bOver = false; bPaused = false; bNext = rand() % 7; bSpawn(); bTimer = 0; }
static void bRotate() {
    int t[4][4]; int n = bCurT == 0 ? 4 : (bCurT == 3 ? 2 : 3);
    if (bCurT == 3) return;
    memset(t, 0, sizeof t);
    for (int r = 0; r < n; r++) for (int c = 0; c < n; c++) t[c][n - 1 - r] = bCur[r][c];
    for (int k : {0, -1, 1, -2, 2}) if (bFits(bX + k, bY, t)) { memcpy(bCur, t, sizeof t); bX += k; return; }
}
static void bLock() {
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (bCur[r][c] && bY + r >= 0) bBoard[bY + r][bX + c] = bCur[r][c];
    int cleared = 0;
    for (int r = BH - 1; r >= 0; r--) {
        bool full = true; for (int c = 0; c < BW; c++) if (!bBoard[r][c]) full = false;
        if (full) { for (int k = r; k > 0; k--) memcpy(bBoard[k], bBoard[k - 1], sizeof bBoard[k]); memset(bBoard[0], 0, sizeof bBoard[0]); cleared++; r++; }
    }
    static const int pts[5] = {0, 100, 300, 500, 800};
    bScore += pts[cleared] * bLevel; bLines += cleared; bLevel = 1 + bLines / 10;
    if (bScore > bBest) { bBest = bScore; }
    bSpawn();
}
static bool bMove(int dx, int dy) { if (bFits(bX + dx, bY + dy, bCur)) { bX += dx; bY += dy; return true; } return false; }
static void bDrop() { while (bMove(0, 1)) bScore += 1; bLock(); }
static void blocksInit() { if (!bInit) { bInit = true; std::string d; if (readFile(DIR_ + "blocks.txt", d)) bBest = atoi(d.c_str()); bNew(); } }
static void blocksUpdate(float dt) {
    if (bOver || bPaused) return;
    bTimer += dt; float spd = std::max(0.06f, 0.8f - 0.07f * (bLevel - 1));
    while (bTimer >= spd) { bTimer -= spd; if (!bMove(0, 1)) { bLock(); break; } }
}
static void blocksKey(u64 kd) {
    if (bOver || bPaused) { if (kd & HidNpadButton_A) bNew(); return; }
    if (kd & HidNpadButton_AnyLeft) bMove(-1, 0);
    if (kd & HidNpadButton_AnyRight) bMove(1, 0);
    if (kd & HidNpadButton_AnyDown) { if (bMove(0, 1)) bScore++; }
    if (kd & (HidNpadButton_AnyUp | HidNpadButton_A)) bRotate();
    if (kd & HidNpadButton_B) bDrop();
}
static void drawBlocks() {
    rect(0, 0, W, UIH, Col{16, 20, 32});
    rect(BX0 - 4, BY0 - 4, BW * BC + 8, BH * BC + 8, Col{60, 70, 96}); rect(BX0, BY0, BW * BC, BH * BC, Col{8, 10, 18});
    for (int r = 0; r < BH; r++) for (int c = 0; c < BW; c++) if (bBoard[r][c]) { rect(BX0 + c * BC, BY0 + r * BC, BC - 1, BC - 1, bCol(bBoard[r][c])); }
    if (!bOver) {
        int gy = bY; while (bFits(bX, gy + 1, bCur)) gy++;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (bCur[r][c]) { outline(BX0 + (bX + c) * BC, BY0 + (gy + r) * BC, BC - 1, BC - 1, 1, Col{90, 100, 130}); rect(BX0 + (bX + c) * BC, BY0 + (bY + r) * BC, BC - 1, BC - 1, bCol(bCur[r][c])); }
    }
    text("Blocks", F30, 40, 40, WHITE);
    text("Score " + std::to_string(bScore), F22, 40, 110, WHITE); text("Best " + std::to_string(std::max(bBest, bScore)), F22, 40, 146, Col{170, 180, 210});
    text("Lines " + std::to_string(bLines), F22, 40, 190, WHITE); text("Level " + std::to_string(bLevel), F22, 40, 226, WHITE);
    text("Next", F16, 860, 40, Col{170, 180, 210});
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (BSHAPES[bNext][r][c]) rect(860 + c * 26, 70 + r * 26, 25, 25, bCol(BSHAPES[bNext][r][c]));
    btn(860, 200, 150, 70, "Rotate", Col{50, 62, 90}, WHITE); btn(1030, 200, 150, 70, "Drop", Col{50, 62, 90}, WHITE);
    btn(860, 290, 150, 70, "<", Col{50, 62, 90}, WHITE); btn(1030, 290, 150, 70, ">", Col{50, 62, 90}, WHITE);
    btn(860, 380, 320, 70, "Down", Col{50, 62, 90}, WHITE);
    btn(860, 480, 150, 60, bPaused ? "Resume" : "Pause", Col{70, 70, 84}, WHITE); btn(1030, 480, 150, 60, "New game", Col{40, 120, 70}, WHITE);
    text("Swipe on the board: left/right move, down drops, tap rotates", F16, 40, 600, Col{130, 140, 170});
    if (bOver || bPaused) { rect(BX0, BY0 + 220, BW * BC, 120, Col{0, 0, 0}, 200); text(bOver ? "Game over" : "Paused", F30, BX0 + BW * BC / 2, BY0 + 240, WHITE, 1); text(bOver ? "Tap New game" : "Tap Resume", F16, BX0 + BW * BC / 2, BY0 + 290, Col{190, 200, 220}, 1); }
}
static void blocksTap(int x, int y) {
    if (inR(x, y, 1030, 480, 150, 60)) { bNew(); return; }
    if (inR(x, y, 860, 480, 150, 60)) { if (!bOver) bPaused = !bPaused; return; }
    if (bOver || bPaused) return;
    if (inR(x, y, 860, 200, 150, 70)) bRotate();
    else if (inR(x, y, 1030, 200, 150, 70)) bDrop();
    else if (inR(x, y, 860, 290, 150, 70)) bMove(-1, 0);
    else if (inR(x, y, 1030, 290, 150, 70)) bMove(1, 0);
    else if (inR(x, y, 860, 380, 320, 70)) { if (bMove(0, 1)) bScore++; }
    else if (inR(x, y, BX0, BY0, BW * BC, BH * BC)) bRotate();
}
static void blocksTouch(int ph, int x, int y) {
    if (bOver || bPaused) return;
    if (ph == 0) { bTX = x; bTY = y; }
    else if (ph == 1 && inR(bTX, bTY, BX0 - 40, BY0, BW * BC + 80, BH * BC)) {
        while (x - bTX > BC) { bMove(1, 0); bTX += BC; }
        while (bTX - x > BC) { bMove(-1, 0); bTX -= BC; }
        if (y - bTY > 70) { bDrop(); bTY = 99999; }
    }
}

// ================================================================ Flashlight
static int flCol = 0; static float flBright = 1.0f; static bool flStrobe = false;
static void drawLight() {
    static const Col C[6] = {{255, 255, 255}, {255, 214, 150}, {255, 60, 40}, {80, 255, 90}, {80, 140, 255}, {255, 230, 80}};
    Col c = C[flCol % 6]; float b = flBright;
    if (flStrobe && ((SDL_GetTicks() / 120) % 2)) b = 0.05f;
    rect(0, 0, W, UIH, Col{(int)(c.r * b), (int)(c.g * b), (int)(c.b * b)});
    rect(300, UIH - 90, 680, 50, Col{0, 0, 0}, 110); rect(310 + 0, UIH - 80, (int)(660 * flBright), 30, WHITE, 230);
    text("Brightness", F16, 640, UIH - 120, Col{0, 0, 0}, 1, 180);
    btn(40, UIH - 90, 220, 50, flStrobe ? "Strobe: on" : "Strobe: off", Col{40, 40, 40}, WHITE);
    text("Tap the screen to change colour", F16, 640, 60, Col{0, 0, 0}, 1, 150);
}
static void lightTap(int x, int y) {
    if (inR(x, y, 40, UIH - 90, 220, 50)) { flStrobe = !flStrobe; return; }
    if (inR(x, y, 300, UIH - 90, 680, 50)) { flBright = std::max(0.1f, std::min(1.0f, (x - 310) / 660.0f)); return; }
    flCol = (flCol + 1) % 6;
}
static void lightDrag(int, int) {}
static void lightTouch(int ph, int x, int y) { if (ph >= 1 && inR(x, y, 300, UIH - 100, 680, 70)) flBright = std::max(0.1f, std::min(1.0f, (x - 310) / 660.0f)); }

// ================================================================ World Clock
struct WCity { const char* n; float off; char rule; };   // rule: N none, E europe, U usa, S southern (AU/NZ)
static const WCity WCITIES[] = {
    {"Algiers", 1, 'N'}, {"Casablanca", 1, 'N'}, {"Lagos", 1, 'N'}, {"Cairo", 2, 'N'}, {"Johannesburg", 2, 'N'}, {"Nairobi", 3, 'N'},
    {"London", 0, 'E'}, {"Paris", 1, 'E'}, {"Berlin", 1, 'E'}, {"Madrid", 1, 'E'}, {"Rome", 1, 'E'}, {"Istanbul", 3, 'N'}, {"Moscow", 3, 'N'},
    {"Riyadh", 3, 'N'}, {"Tehran", 3.5f, 'N'}, {"Dubai", 4, 'N'}, {"Karachi", 5, 'N'}, {"Delhi", 5.5f, 'N'}, {"Dhaka", 6, 'N'},
    {"Bangkok", 7, 'N'}, {"Jakarta", 7, 'N'}, {"Beijing", 8, 'N'}, {"Singapore", 8, 'N'}, {"Tokyo", 9, 'N'}, {"Seoul", 9, 'N'},
    {"Sydney", 10, 'S'}, {"Auckland", 12, 'S'}, {"Honolulu", -10, 'N'}, {"Anchorage", -9, 'U'}, {"Los Angeles", -8, 'U'}, {"Denver", -7, 'U'},
    {"Chicago", -6, 'U'}, {"Mexico City", -6, 'N'}, {"New York", -5, 'U'}, {"Toronto", -5, 'U'}, {"Sao Paulo", -3, 'N'}, {"Buenos Aires", -3, 'N'}, {"UTC", 0, 'N'},
};
static const int NWC = (int)(sizeof(WCITIES) / sizeof(WCITIES[0]));
static std::vector<int> wcSel; static int wcHome = 0; static bool wcPick = false, wcInit = false; static int wcScroll = 0;
static int wcNthSun(int y, int m, int n) { int d = 1 + (7 - dow(y, m, 1)) % 7; return d + 7 * (n - 1); }
static int wcLastSun(int y, int m) { static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; int last = dm[m - 1] + ((m == 2 && (y % 4 == 0 && (y % 100 || y % 400 == 0))) ? 1 : 0); return last - dow(y, m, last); }
static bool wcDst(char rule, int y, int mo, int d) {
    int v = mo * 100 + d;
    switch (rule) {
        case 'E': return v >= 300 + wcLastSun(y, 3) && v < 1000 + wcLastSun(y, 10);
        case 'U': return v >= 300 + wcNthSun(y, 3, 2) && v < 1100 + wcNthSun(y, 11, 1);
        case 'S': return v >= 1000 + wcNthSun(y, 10, 1) || v < 400 + wcNthSun(y, 4, 1);
        default: return false;
    }
}
static long long wcDays(int y, int m, int d) { y -= m <= 2; long long era = (y >= 0 ? y : y - 399) / 400; long long yoe = y - era * 400; long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; return era * 146097 + doe - 719468; }
static void wcCivil(long long z, int& y, int& m, int& d) { z += 719468; long long era = (z >= 0 ? z : z - 146096) / 146097; long long doe = z - era * 146097; long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long long yy = yoe + era * 400; long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100); long long mp = (5 * doy + 2) / 153; d = (int)(doy - (153 * mp + 2) / 5 + 1); m = (int)(mp < 10 ? mp + 3 : mp - 9); y = (int)(yy + (m <= 2)); }
static std::string wcFile() { return DIR_ + "world.txt"; }
static void wcSave() { std::string s = std::to_string(wcHome) + "\n"; for (int i : wcSel) s += std::to_string(i) + " "; writeFile(wcFile(), s); }
static void wcLoad() {
    wcInit = true; wcSel = {0, 6, 33, 23, 25}; wcHome = 0;
    std::string d; if (!readFile(wcFile(), d)) return;
    size_t nl = d.find('\n'); if (nl == std::string::npos) return;
    wcHome = atoi(d.substr(0, nl).c_str()); wcSel.clear();
    const char* p = d.c_str() + nl + 1; int v, k;
    while (sscanf(p, "%d%n", &v, &k) == 1) { if (v >= 0 && v < NWC) wcSel.push_back(v); p += k; }
    if (wcHome < 0 || wcHome >= NWC) wcHome = 0;
}
static Now wcTime(int city, int& dayDelta, float& off) {
    // local time (gNow) belongs to the home city; compute that city's UTC, then shift
    float homeOff = WCITIES[wcHome].off + (wcDst(WCITIES[wcHome].rule, gNow.y, gNow.mo, gNow.d) ? 1 : 0);
    long long baseSec = wcDays(gNow.y, gNow.mo, gNow.d) * 86400LL + gNow.h * 3600 + gNow.mi * 60 + gNow.s;
    long long utc = baseSec - (long long)(homeOff * 3600);
    float co = WCITIES[city].off;
    int gy, gm, gd; wcCivil((utc + (long long)(co * 3600)) / 86400 - ((utc + (long long)(co * 3600)) % 86400 < 0 ? 1 : 0), gy, gm, gd);
    bool dst = wcDst(WCITIES[city].rule, gy, gm, gd);
    off = co + (dst ? 1 : 0);
    long long ct = utc + (long long)(off * 3600); long long cd = ct / 86400; long long cs = ct % 86400; if (cs < 0) { cs += 86400; cd--; }
    Now n; wcCivil(cd, n.y, n.mo, n.d); n.h = (int)(cs / 3600); n.mi = (int)(cs % 3600 / 60); n.s = (int)(cs % 60);
    int a = n.y * 10000 + n.mo * 100 + n.d, b = gNow.y * 10000 + gNow.mo * 100 + gNow.d; dayDelta = a > b ? 1 : (a < b ? -1 : 0);
    return n;
}
static void drawWorld() {
    if (!wcInit) wcLoad();
    rect(0, 0, W, UIH, Col{22, 20, 48});
    text("World Clock", F44, 50, 24, WHITE);
    btn(W - 230, 34, 190, 48, wcPick ? "Done" : "Add city", Col{90, 80, 190}, WHITE);
    text("Times follow the rules of each city. Tap the star to mark where you are.", F16, 50, 94, Col{160, 160, 200});
    SDL_Rect clip{0, 130, W, UIH - 130}; SDL_RenderSetClipRect(gR, &clip);
    if (!wcPick) {
        for (int i = 0; i < (int)wcSel.size(); i++) {
            int y = 140 + i * 96 - wcScroll; int c = wcSel[i];
            int dd; float off; Now n = wcTime(c, dd, off);
            rect(40, y, W - 80, 86, Col{38, 34, 78});
            text(WCITIES[c].n, F30, 66, y + 10, WHITE);
            char b[64]; snprintf(b, sizeof b, "UTC%+.4g%s   %s", off, off == (int)off ? "" : "", dd > 0 ? "Tomorrow" : (dd < 0 ? "Yesterday" : "Today")); text(b, F16, 66, y + 52, Col{170, 170, 210});
            snprintf(b, sizeof b, g24 ? "%02d:%02d" : "%d:%02d %s", g24 ? n.h : (n.h % 12 ? n.h % 12 : 12), n.mi, n.h < 12 ? "AM" : "PM"); text(b, F44, W - 230, y + 12, WHITE, 2);
            text(c == wcHome ? "*" : "o", F30, W - 130, y + 20, c == wcHome ? Col{255, 210, 60} : Col{110, 110, 150});
            text("x", F30, W - 80, y + 18, Col{200, 100, 110});
        }
    } else {
        for (int i = 0; i < NWC; i++) {
            int col = i % 3, rw = i / 3; int x = 50 + col * 400, y = 140 + rw * 66 - wcScroll;
            bool has = std::find(wcSel.begin(), wcSel.end(), i) != wcSel.end();
            rect(x, y, 380, 56, has ? Col{70, 120, 90} : Col{44, 40, 90}); text(WCITIES[i].n, F22, x + 16, y + 12, WHITE);
        }
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static void worldTap(int x, int y) {
    if (!wcInit) wcLoad();
    if (inR(x, y, W - 230, 34, 190, 48)) { wcPick = !wcPick; wcScroll = 0; wcSave(); return; }
    if (y < 130) return;
    if (!wcPick) {
        for (int i = 0; i < (int)wcSel.size(); i++) {
            int ry = 140 + i * 96 - wcScroll;
            if (y < ry || y >= ry + 86) continue;
            if (x >= W - 170 && x < W - 105) { wcHome = wcSel[i]; wcSave(); toast(std::string(WCITIES[wcHome].n) + " is now your time zone"); }
            else if (x >= W - 105) { wcSel.erase(wcSel.begin() + i); wcSave(); }
            return;
        }
    } else {
        for (int i = 0; i < NWC; i++) {
            int col = i % 3, rw = i / 3; if (!inR(x, y, 50 + col * 400, 140 + rw * 66 - wcScroll, 380, 56)) continue;
            auto it = std::find(wcSel.begin(), wcSel.end(), i);
            if (it == wcSel.end()) wcSel.push_back(i); else wcSel.erase(it);
            wcSave(); return;
        }
    }
}
static void worldDrag(int, int dy) { wcScroll = std::max(0, wcScroll - dy); }

// ================================================================ Dictionary
static std::string dcWord, dcMsg = "Type a word and tap Look up."; static bool dcBusy = false; static int dcScroll = 0;
struct DcLine { std::string t; int kind; };   // 0 word, 1 part of speech, 2 definition, 3 example
static std::vector<DcLine> dcLines;
static void dcDone(NetJob& j) {
    dcBusy = false; dcLines.clear(); dcScroll = 0;
    if (!j.ok || j.out.empty() || j.out[0] != '[') { dcMsg = j.code == 404 || (!j.out.empty() && j.out[0] == '{') ? "No definition found for \"" + dcWord + "\"." : "Couldn't reach the dictionary."; return; }
    const std::string& b = j.out; dcMsg.clear();
    dcLines.push_back({jstr(b, "word"), 0});
    size_t p = 0; int meanings = 0;
    while (meanings < 4) {
        size_t pp; if (!jfind(b, "partOfSpeech", p, pp)) break;
        std::string pos = jstr(b, "partOfSpeech", p);
        size_t nxt; if (!jfind(b, "partOfSpeech", pp + 1, nxt)) nxt = b.size();
        dcLines.push_back({pos, 1});
        size_t q = pp; int defs = 0;
        while (defs < 3) {
            size_t dp; if (!jfind(b, "definition", q, dp) || dp > nxt) break;
            dcLines.push_back({std::to_string(defs + 1) + ". " + jstr(b, "definition", q), 2});
            size_t nd; if (!jfind(b, "definition", dp + 1, nd)) nd = b.size();
            size_t ep; if (jfind(b, "example", dp, ep) && ep < nd && ep < nxt) dcLines.push_back({"\"" + jstr(b, "example", dp) + "\"", 3});
            q = dp + 1; defs++;
        }
        p = nxt; meanings++;
    }
    if (dcLines.size() <= 1) { dcLines.clear(); dcMsg = "No definition found."; }
}
static void dcGo() {
    hideKb(); if (dcWord.empty() || dcBusy) return;
    if (!netUp()) { dcMsg = gOffline ? "Offline mode is on." : "No internet connection."; return; }
    NetJob* j = new NetJob(); j->url = "https://api.dictionaryapi.dev/api/v2/entries/en/" + urlEncode(dcWord); j->done = dcDone;
    if (netStart(j)) { dcBusy = true; dcMsg = "Looking up..."; dcLines.clear(); }
}
static void drawDict() {
    rect(0, 0, W, UIH, Col{44, 20, 26});
    text("Dictionary", F44, 50, 36, WHITE);
    drawField(50, 100, 900, 56, &dcWord, "Type a word", F30, WHITE, Col{70, 34, 42}, Col{200, 90, 100});
    btn(970, 100, 240, 56, "Look up", Col{200, 60, 70}, WHITE);
    int y = 190 - dcScroll, bottom = kbOpen ? KBTOP : UIH;
    SDL_Rect clip{0, 180, W, bottom - 180}; SDL_RenderSetClipRect(gR, &clip);
    if (!dcMsg.empty()) text(dcMsg, F22, 50, 200, Col{230, 200, 205});
    for (auto& l : dcLines) {
        int f = l.kind == 0 ? F44 : (l.kind == 1 ? F22 : F22); Col c = l.kind == 0 ? WHITE : (l.kind == 1 ? Col{255, 170, 120} : (l.kind == 3 ? Col{200, 170, 175} : Col{240, 235, 235}));
        if (l.kind <= 1) { text(l.t, f, 50, y, c); y += l.kind == 0 ? 62 : 40; }
        else for (auto& w : wrapText(l.t, F22, 1120)) { text(w, F22, 70, y, c); y += 32; }
        if (l.kind >= 2) y += 6;
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static void dictTap(int x, int y) {
    if (inR(x, y, 970, 100, 240, 56)) { dcGo(); return; }
    if (inR(x, y, 50, 100, 900, 56)) openKbStr(&dcWord, 40, dcGo);
}
static void dictDrag(int, int dy) { dcScroll = std::max(0, dcScroll - dy); }

#include "win81.h"

// ---------------------------------------------------------------- live tiles
static void liveTick() {
    static Uint32 last = 0; static bool first = true;
    Uint32 now = SDL_GetTicks();
    if (!first && now - last < 60000) return;
    first = false; last = now;
    calEnsure();
    if (!gWx.have && !gWxHaveLoc) wxLoad();
    if (gWxHaveLoc || gLocOK) wxFetch(false);
    if (gMAddr.empty()) mailLoadAcct();
    if (!gMAddr.empty()) { if (gMail.empty()) mailLoadCache(); if (!gMBusy && (gMail.empty() || nowMinutes() - gMStamp > 10)) mailRefresh(); }
    if (gNews.empty()) { std::string d; if (readFile(DIR_ + "news_0.xml", d)) newsParse(d); }
    if (!gNewsBusy && netUp() && (gNews.empty())) newsLoadFeed(0, true);
}
static bool liveTileDraw(const Tile& t, SDL_Rect r, float phase, bool& drewAll) {
    (void)phase; drewAll = false;
    float tm = SDL_GetTicks() / 1000.0f;
    int page = ((int)(tm / 5.0f)) % 2;
    bool small = r.w < 150, wide = r.w >= 300, tall = r.h >= 300;
    int cx = r.x, cy = r.y;
    switch (t.app) {
        case A_CALENDAR: {
            const char* dn = WD[dow(gNow.y, gNow.mo, gNow.d)];
            if (small) { text(std::to_string(gNow.d), F44, cx + r.w / 2, cy + 10, WHITE, 1); text(std::string(dn).substr(0, 3), F16, cx + r.w / 2, cy + r.h - 24, WHITE, 1); drewAll = true; return true; }
            text(std::to_string(gNow.d), F72, cx + 14, cy + 6, WHITE);
            text(dn, F22, cx + 14, cy + 82, WHITE);
            text(MONTHS[gNow.mo - 1], F16, cx + 16, cy + 114, WHITE, 0, 200);
            if (wide) {
                std::string ev = calTileLine();
                rect(cx + 170, cy + 16, 2, r.h - 32, WHITE, 90);
                auto L = wrapText(ev, F22, r.w - 210);
                text("Today", F16, cx + 190, cy + 16, WHITE, 0, 190);
                for (size_t i = 0; i < L.size() && i < 3; i++) text(L[i], F22, cx + 190, cy + 44 + (int)i * 28, WHITE);
            }
            drewAll = true; return true;
        }
        case A_WEATHER: {
            if (!gWx.have) return false;
            Col top, bot; wxColors(gWx.code, gWx.isDay, top, bot);
            for (int y = 0; y < r.h; y += 4) rect(cx, cy + y, r.w, 4, mix(top, bot, y / (float)r.h));
            if (small) { wxIcon(gWx.code, gWx.isDay, cx + r.w / 2, cy + 28, 40, tm); text(wxTemp(gWx.temp), F22, cx + r.w / 2, cy + r.h - 32, WHITE, 1); drewAll = true; return true; }
            if (wide && page == 1 && gWx.days.size() >= 4) {
                for (int i = 1; i < 4; i++) {
                    int x = cx + (i - 1) * (r.w / 3) + r.w / 6;
                    text(wxDayName(gWx.days[i].date, i), F16, x, cy + 12, WHITE, 1);
                    wxIcon(gWx.days[i].code, 1, x, cy + 70, 56, tm);
                    text(wxTemp(gWx.days[i].hi) + " " + wxTemp(gWx.days[i].lo), F16, x, cy + r.h - 36, WHITE, 1);
                }
                drewAll = true; return true;
            }
            wxIcon(gWx.code, gWx.isDay, cx + (wide ? 90 : r.w / 2), cy + (wide ? 80 : 56), wide ? 110 : 80, tm);
            if (wide) {
                text(wxTemp(gWx.temp), F72, cx + 170, cy + 8, WHITE);
                text(gWxCity, F22, cx + 172, cy + 88, WHITE);
                text(wxText(gWx.code), F16, cx + 172, cy + 120, WHITE, 0, 210);
            } else {
                text(wxTemp(gWx.temp), F44, cx + r.w / 2, cy + 88, WHITE, 1);
                text(gWxCity, F16, cx + 10, cy + r.h - 26, WHITE);
            }
            (void)tall; drewAll = true; return true;
        }
        case A_MAIL: {
            if (gMAddr.empty() || gMail.empty()) return false;
            if (small) { text(std::to_string(gMUnread), F44, cx + r.w / 2, cy + 8, WHITE, 1); text("mail", F16, cx + r.w / 2, cy + r.h - 24, WHITE, 1); drewAll = true; return true; }
            int idx = 0; for (int i = 0; i < (int)gMail.size(); i++) if (!gMail[i].seen) { idx = i; break; }
            const MailMsg& m = gMail[idx];
            text(std::to_string(gMUnread), F72, cx + 14, cy + 4, WHITE);
            text(gMUnread == 1 ? "unread" : "unread", F16, cx + 16, cy + 82, WHITE, 0, 200);
            int tx = wide ? cx + 120 : cx + 14, ty = wide ? cy + 14 : cy + 112, tw2 = wide ? r.w - 140 : r.w - 28;
            text(fitTxt(m.from, F22, tw2), F22, tx, ty, WHITE);
            auto L = wrapText(m.subj, F16, tw2);
            for (size_t i = 0; i < L.size() && i < (wide ? 2u : 1u); i++) text(L[i], F16, tx, ty + 32 + (int)i * 22, WHITE, 0, 220);
            text("Mail", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        case A_MUSIC: {
            if (musIdx < 0 || musIdx >= (int)gSongs.size()) return false;
            int ai = albumOfSong(musIdx);
            int s = std::min(r.w, r.h) - (small ? 0 : 0);
            if (small) { coverBox(nullptr, ai >= 0 ? gAlbumsV[ai].col : MUS_ACC, cx, cy, s, "", false); }
            else {
                coverBox(musNowCover, ai >= 0 ? gAlbumsV[ai].col : MUS_ACC, cx, cy, r.h, ai >= 0 ? gAlbumsV[ai].name : "", false);
                if (wide) {
                    rect(cx + r.h, cy, r.w - r.h, r.h, Col{30, 30, 34});
                    text(fitTxt(gSongs[musIdx].title, F22, r.w - r.h - 24), F22, cx + r.h + 12, cy + 30, WHITE);
                    text(fitTxt(gSongs[musIdx].artist, F16, r.w - r.h - 24), F16, cx + r.h + 12, cy + 64, WHITE, 0, 180);
                    drawVisualizer(cx + r.h + 12, cy + r.h - 56, r.w - r.h - 24, 40, MUS_ACC);
                }
            }
            if (musPlaying && !musPaused) disc(cx + 18, cy + r.h - 18, 6, WHITE);
            drewAll = true; return true;
        }
        case A_NEWS: {
            if (gNews.empty() || small) return false;
            auto L = wrapText(gNews[0].title, F22, r.w - 28);
            for (size_t i = 0; i < L.size() && i < (tall ? 6u : 3u); i++) text(L[i], F22, cx + 14, cy + 12 + (int)i * 30, WHITE);
            text("News", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        case A_SKYPE: {
            int n = skyUnread();
            if (small) { text(n ? std::to_string(n) : "S", F44, cx + r.w / 2, cy + 8, WHITE, 1); text("Skype", F16, cx + r.w / 2, cy + r.h - 24, WHITE, 1); drewAll = true; return true; }
            text(std::to_string(n), F72, cx + 14, cy + 4, WHITE);
            text(n == 1 ? "new message" : "new messages", F16, cx + 16, cy + 82, WHITE, 0, 200);
            if (wide) { text("Skype", F22, cx + 16, cy + r.h - 36, WHITE); }
            else text("Skype", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        case A_TASKMGR: {
            if (small) { char b[16]; snprintf(b, sizeof b, "%.0f%%", gTmCpu); text(b, F30, cx + r.w / 2, cy + 20, WHITE, 1); text("CPU", F16, cx + r.w / 2, cy + r.h - 24, WHITE, 1); drewAll = true; return true; }
            char b[32]; snprintf(b, sizeof b, "%.0f%%", gTmCpu); text(b, F72, cx + 14, cy + 4, WHITE);
            text("CPU", F16, cx + 16, cy + 82, WHITE, 0, 200);
            snprintf(b, sizeof b, "%.0f MB used", gTmMemUsedMB); text(b, F16, cx + 16, cy + 106, WHITE, 0, 200);
            if (wide) { int gx = cx + 190, gw = r.w - 210; for (int i = 0; i < tmHN; i++) { int hh = (int)(tmHCpu[i] / 100.0f * (r.h - 40)); rect(gx + i * gw / 60, cy + r.h - 20 - hh, std::max(2, gw / 60), hh, WHITE, 160); } }
            text("Task Manager", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        case A_ONEDRIVE: {
            if (small) return false;
            text(std::to_string((int)odRecent.size()), F72, cx + 14, cy + 4, WHITE);
            text("recent files", F16, cx + 16, cy + 82, WHITE, 0, 200);
            if (wide && !odRecent.empty()) text(fitTxt(odRecent[0].name, F22, r.w - 190), F22, cx + 180, cy + 20, WHITE);
            text("OneDrive", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        case A_PET: return petLive(t, r, drewAll);
        case A_CITY: return cityLive(t, r, drewAll);
        case A_HEALTH: return hlLive(t, r, drewAll);
        case A_FOOD: return foodLive(t, r, drewAll);
        case A_RECORDER: return recLive(t, r, drewAll);
        case A_CORTANA: {
            if (small) return false;
            disc(cx + r.w / 2, cy + r.h / 2 - 14, 30, WHITE); disc(cx + r.w / 2, cy + r.h / 2 - 14, 23, tileColor(t)); disc(cx + r.w / 2, cy + r.h / 2 - 14, 13, WHITE);
            text(wide ? "Ask me anything" : "Cortana", F16, cx + 10, cy + r.h - 26, WHITE);
            drewAll = true; return true;
        }
        default: return false;
    }
}
static void appTouch(int phase, int x, int y) {
    switch (curApp) {
        case A_PAINT: paintTouch(phase, x, y); break;
        case A_2048: if (phase == 0) { swX = x; swY = y; } else if (phase == 2) { int d = swipeOf(x, y, 50); if (d >= 0) g2Move(d); } break;
        case A_SNAKE:
            if (phase == 0) { swX = x; swY = y; }
            else if (phase == 1) { int d = swipeOf(x, y, 36); if (d >= 0) { snakeTurn(d); swX = x; swY = y; } }
            break;
        case A_SOLITAIRE: solTouch(phase, x, y); break;
        case A_MUSIC: musicTouch(phase, x, y); break;
        case A_MAIL: mailTouch(phase, x, y); break;
        case A_NEWS: gNewsTx = x; break;
        case A_ROBLOX: robloxTouch(phase, x, y); break;
        case A_SETTINGS: if (gSetup) authTouch(phase, x, y); break;
        case A_BLOCKS: blocksTouch(phase, x, y); break;
        case A_LIGHT: lightTouch(phase, x, y); break;
        case A_DESKTOP: desktopTouch(phase, x, y); break;
        case A_CITY: cityTouch(phase, x, y); break;
        case A_PET: petTouch(phase, x, y); break;
        case A_SKYPE: if (phase == 0) skyDownX = x; break;
        // @@TOUCH@@
        default: break;
    }
}

// ---------------------------------------------------------------- homebrew menu (hbmenu) replace / backup / restore
static const char WNX_MARK[] = "WINNX-SELF-MARKER-9c41e7";
#ifdef WINNX_AUDIO
#define WNXF_A "audio "
#else
#define WNXF_A ""
#endif
#ifdef WINNX_NET
#define WNXF_N "net "
#else
#define WNXF_N ""
#endif
__attribute__((used)) static const char WNX_FEAT[] = "WNXFEAT[" WNXF_A WNXF_N "]";
static const char* HB_PATH = "sdmc:/hbmenu.nro";
static bool gHbHave = false, gHbIsWinNX = false, gHbBackup = false;
static float gHbConfirm = 0;
static std::string hbBackupPath() { return DIR_ + "hbmenu_backup.nro"; }
static long long fileSizeOf(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return -1;
    return (long long)st.st_size;
}
static bool copyFile(const std::string& from, const std::string& to) {
    FILE* a = fopen(from.c_str(), "rb");
    if (!a) return false;
    FILE* b = fopen(to.c_str(), "wb");
    if (!b) { fclose(a); return false; }
    std::vector<char> buf(1 << 16);
    size_t n; bool ok = true;
    while ((n = fread(buf.data(), 1, buf.size(), a)) > 0) if (fwrite(buf.data(), 1, n, b) != n) { ok = false; break; }
    fclose(a);
    if (fclose(b) != 0) ok = false;
    return ok;
}
static bool fileHasMarker(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    const size_t L = strlen(WNX_MARK);
    std::string win, chunk(1 << 16, '\0');
    bool found = false;
    size_t n;
    while (!found && (n = fread(&chunk[0], 1, chunk.size(), f)) > 0) {
        win.append(chunk, 0, n);
        if (win.find(WNX_MARK) != std::string::npos) found = true;
        else if (win.size() > L) win.erase(0, win.size() - L + 1);
    }
    fclose(f);
    return found;
}
static std::string selfPath() {
    std::string c[3] = {gArg0, "sdmc:/switch/WinNX.nro", "sdmc:/switch/WinNX/WinNX.nro"};
    for (auto& x : c) if (!x.empty() && fileSizeOf(x) > 0 && fileHasMarker(x)) return x;
    return "";
}
static void hbRefresh() {
    gHbHave = fileSizeOf(HB_PATH) > 0;
    gHbIsWinNX = gHbHave && fileHasMarker(HB_PATH);
    gHbBackup = fileSizeOf(hbBackupPath()) > 0;
}
static bool safeReplace(const std::string& src, const std::string& dst) {
    long long n = fileSizeOf(src);
    if (n <= 0) return false;
    std::string tmp = dst + ".new";
    remove(tmp.c_str());
    if (!copyFile(src, tmp) || fileSizeOf(tmp) != n) { remove(tmp.c_str()); return false; }
    remove(dst.c_str());
    if (rename(tmp.c_str(), dst.c_str()) != 0) {
        bool ok = copyFile(tmp, dst) && fileSizeOf(dst) == n;
        remove(tmp.c_str());
        return ok;
    }
    return true;
}
static void hbInstall() {
    std::string self = selfPath();
    if (self.empty()) { gHbMsg = "Couldn't find WinNX.nro. Keep it at sdmc:/switch/WinNX.nro, then try again."; return; }
    if (self == "sdmc:/hbmenu.nro" || self == "/hbmenu.nro") { gHbMsg = "Windows 8.1 is already running as your homebrew menu."; hbRefresh(); return; }
    bool have = fileSizeOf(HB_PATH) > 0;
    if (have && !fileHasMarker(HB_PATH)) { // the original hbmenu: back it up first
        if (!safeReplace(HB_PATH, hbBackupPath())) { gHbMsg = "Backup of hbmenu failed, nothing was changed."; hbRefresh(); return; }
    } else if (!have && fileSizeOf(hbBackupPath()) <= 0) {
        gHbMsg = "No hbmenu.nro found on the SD card root, so there is nothing to back up. Nothing was changed.";
        return;
    }
    if (!safeReplace(self, HB_PATH)) { gHbMsg = "Couldn't write sdmc:/hbmenu.nro. Use Restore, or copy it back from a PC."; hbRefresh(); return; }
    hbRefresh();
    gHbMsg = "Done. Launching homebrew from the Album now opens Windows 8.1. Original hbmenu saved to switch/WinNX/hbmenu_backup.nro.";
}
static void hbRestore() {
    if (fileSizeOf(hbBackupPath()) <= 0) { gHbMsg = "No backup found at switch/WinNX/hbmenu_backup.nro."; return; }
    if (!safeReplace(hbBackupPath(), HB_PATH)) { gHbMsg = "Restore failed. Copy switch/WinNX/hbmenu_backup.nro to /hbmenu.nro from a PC."; hbRefresh(); return; }
    hbRefresh();
    gHbMsg = "Original hbmenu restored.";
}
static void saveAll();
static void hbOpenOriginal() {
    std::string t;
    if (fileSizeOf(hbBackupPath()) > 0) t = hbBackupPath();
    else if (fileSizeOf(HB_PATH) > 0 && !fileHasMarker(HB_PATH)) t = HB_PATH;
    if (t.empty()) { gHbMsg = "No original hbmenu found. Settings > Homebrew menu explains how to set one up."; toast("No original hbmenu found"); return; }
    saveAll();
    if (R_SUCCEEDED(envSetNextLoad(t.c_str(), t.c_str()))) gRun = false;
    else toast("Couldn't launch the original hbmenu");
}
static void settingsTap(int x, int y, const Theme& th);
static void appUpdate(float dt) {
    netTick(SDL_GetTicks()); netPoll();
    if (uiState == 2) { liveTick(); calTick(); coTick(); }
    filesUpdate(dt); photosUpdate(dt); authUpdate(dt); radioUpdate(dt); if (curApp == A_CHESS) chessUpdate(); if (curApp == A_BLOCKS) blocksUpdate(dt); if (curApp == A_CAMERA) camUpdate(); else if (camOn) camStop(); if (phDelArm > 0) phDelArm -= dt;
    if (gHbConfirm > 0) { gHbConfirm -= dt; if (gHbConfirm <= 0) gHbWhich = 0; }
    if (curApp == A_MINES && appDir == 0 && mState == 1) mTime += dt;
    if (curApp == A_SNAKE && appDir == 0 && sState == 1) {
        sAcc += dt;
        float stepT = std::max(0.07f, 0.14f - sScore * 0.003f);
        while (sAcc >= stepT && sState == 1) { sAcc -= stepT; snakeStep(); }
    }
    musTick(dt);
    tmSample(); skyUpdate(); oneDriveUpdate(dt); desktopUpdate(); gmUpdate(); eggUpdate(dt); clkUpdate(dt); recUpdate(dt); foodUpdate(dt); hlUpdate(dt); petUpdate(dt); cityUpdate(dt);
    // @@UPDATE@@
}

// ---------------------------------------------------------------- PC settings (Windows 8.1 look)
static const char* SET_PAGES[13] = {"Personalize", "Users", "Notifications", "Search", "Sound", "General", "Privacy", "Devices", "Ease of Access", "OneDrive", "PC info", "Windows Update", "Home menu"};
#define SP_N 13
#define SP_Y(i) (112 + (i) * 39)
static const Col SET_BLUE = {0, 114, 198};
static void setToggle(int x, int y, bool on) {
    rect(x, y, 90, 34, on ? SET_BLUE : Col{200, 200, 205});
    rect(x + (on ? 52 : 4), y + 4, 34, 26, WHITE);
    text(on ? "On" : "Off", F16, x + (on ? 22 : 66), y + 6, on ? WHITE : Col{90, 90, 95}, 1);
}
static void setRow(int y, const char* title, const char* desc, bool on) {
    text(title, F22, 500, y, Col{50, 50, 55});
    if (desc && *desc) text(desc, F16, 500, y + 30, Col{120, 120, 128});
    setToggle(1130, y + 2, on);
}
#include "w81_home.h"
// ================================================================ sign-in options (PIN, password, picture, pattern, button code), accounts, lock-screen widgets
struct Gest { int t = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0; };   // t: 0 tap, 1 line, 2 circle (x1,y1 = centre, x2 = radius)
static int gLockMethod = 0, gLockLen = 0;           // 0 none, 1 PIN, 2 password, 3 picture, 4 pattern, 5 button code
static std::string gLockHash;
static std::vector<Gest> gLockPic3;
static int gAuthMode = 0, gAuthMethod = 0, gSetupTarget = 0, gAuthFails = 0, gAuthPatCur = 4;
static std::string gAuthIn, gAuthTmp, gAuthMsg;
static std::vector<int> gAuthPat; static std::vector<std::string> gAuthBtns;
static std::vector<Gest> gAuthG, gAuthGTmp;
static std::vector<std::pair<int, int>> gAuthStroke;
static float gAuthShake = 0; static Uint32 gAuthLockUntil = 0;
static bool gAuthDown = false; static int gAuthX0 = 0, gAuthY0 = 0; static bool gAuthMoved = false;
static const char* LOCKN[6] = {"None", "PIN", "Password", "Picture password", "Pattern", "Button code"};
static std::string lockHash(const std::string& s) {
    unsigned long long h = 1469598103934665603ULL;
    std::string t = "winnx:" + s;
    for (unsigned char c : t) { h ^= c; h *= 1099511628211ULL; }
    char b[24]; snprintf(b, sizeof b, "%016llx", h); return b;
}
static void lockSave() {
    std::string o = "method=" + std::to_string(gLockMethod) + "\nlen=" + std::to_string(gLockLen) + "\nhash=" + gLockHash + "\npic=";
    for (auto& g : gLockPic3) o += std::to_string(g.t) + "," + std::to_string(g.x1) + "," + std::to_string(g.y1) + "," + std::to_string(g.x2) + "," + std::to_string(g.y2) + ";";
    o += "\nname=" + gUser + "\nstatus=" + gUserStatus + "\ninitial=" + std::to_string(gAvInitial ? 1 : 0) + "\n";
    writeFile(DIR_ + "lock.ini", o);
}
static void lockLoad() {
    std::string d;
    if (!readFile(DIR_ + "lock.ini", d)) return;
    size_t p = 0;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string l = d.substr(p, e - p); p = e + 1;
        size_t q = l.find('='); if (q == std::string::npos) continue;
        std::string k = l.substr(0, q), v = l.substr(q + 1);
        if (k == "method") gLockMethod = std::max(0, std::min(5, atoi(v.c_str())));
        else if (k == "len") gLockLen = atoi(v.c_str());
        else if (k == "hash") gLockHash = v;
        else if (k == "name") { if (!v.empty()) gUser = v; }
        else if (k == "status") gUserStatus = v;
        else if (k == "initial") gAvInitial = atoi(v.c_str()) != 0;
        else if (k == "pic") {
            gLockPic3.clear(); size_t a = 0;
            while (a < v.size()) { size_t s2 = v.find(';', a); if (s2 == std::string::npos) break; Gest g; sscanf(v.substr(a, s2 - a).c_str(), "%d,%d,%d,%d,%d", &g.t, &g.x1, &g.y1, &g.x2, &g.y2); gLockPic3.push_back(g); a = s2 + 1; }
        }
    }
    if (gLockMethod == 3 && gLockPic3.size() != 3) gLockMethod = 0;
    if (gLockMethod != 0 && gLockMethod != 3 && gLockHash.empty()) gLockMethod = 0;
}
static float gdist(int x1, int y1, int x2, int y2) { float dx = (float)(x1 - x2), dy = (float)(y1 - y2); return sqrtf(dx * dx + dy * dy); }
static Gest gestClassify(const std::vector<std::pair<int, int>>& st) {
    Gest g;
    if (st.empty()) return g;
    float len = 0; int mnx = 9999, mxx = -9999, mny = 9999, mxy = -9999;
    for (size_t i = 0; i < st.size(); i++) {
        if (i) len += gdist(st[i - 1].first, st[i - 1].second, st[i].first, st[i].second);
        mnx = std::min(mnx, st[i].first); mxx = std::max(mxx, st[i].first); mny = std::min(mny, st[i].second); mxy = std::max(mxy, st[i].second);
    }
    auto s = st.front(), e = st.back();
    if (len < 30) { g.t = 0; g.x1 = (mnx + mxx) / 2; g.y1 = (mny + mxy) / 2; return g; }
    float se = gdist(s.first, s.second, e.first, e.second);
    if (len > 150 && se < 0.3f * len && mxx - mnx > 40 && mxy - mny > 40) { g.t = 2; g.x1 = (mnx + mxx) / 2; g.y1 = (mny + mxy) / 2; g.x2 = ((mxx - mnx) + (mxy - mny)) / 4; return g; }
    g.t = 1; g.x1 = s.first; g.y1 = s.second; g.x2 = e.first; g.y2 = e.second; return g;
}
static bool gestMatch(const Gest& a, const Gest& b) {
    if (a.t != b.t) return false;
    if (a.t == 0) return gdist(a.x1, a.y1, b.x1, b.y1) <= 70;
    if (a.t == 1) return gdist(a.x1, a.y1, b.x1, b.y1) <= 100 && gdist(a.x2, a.y2, b.x2, b.y2) <= 100;
    return gdist(a.x1, a.y1, b.x1, b.y1) <= 90 && abs(a.x2 - b.x2) <= std::max(50, b.x2 / 2);
}
static std::string authSecret() { // the typed/drawn secret as a string, for the non-picture methods
    std::string s;
    if (gAuthMethod == 1 || gAuthMethod == 2) return gAuthIn;
    if (gAuthMethod == 4) { for (int d : gAuthPat) s += std::to_string(d) + "-"; return s; }
    for (auto& b : gAuthBtns) s += b + ","; return s;
}
static void authPwEnter();
static void authReset() { gAuthIn.clear(); gAuthPat.clear(); gAuthBtns.clear(); gAuthG.clear(); gAuthStroke.clear(); gAuthPatCur = 4; gAuthDown = false; if (kbOpen && kbTarget == 99) kbOpen = false; }
static void authBegin(int mode, int method) {
    gAuthMode = mode; gAuthMethod = method; gAuthMsg.clear(); authReset();
    if (method == 2) { openKbStr(&gAuthIn, 40, authPwEnter); }
}
static void authSuccess() { gAuth = false; authReset(); gAuthFails = 0; lockAnim = true; introT = 0; kbOpen = false; }
static void authFail() {
    gAuthShake = 0.5f; gAuthFails++; authReset();
    if (gAuthMethod == 2) openKbStr(&gAuthIn, 40, authPwEnter);
    if (gAuthFails >= 5) { gAuthLockUntil = SDL_GetTicks() + 15000; gAuthFails = 0; }
    gAuthMsg = "That didn't work. Try again.";
}
static void authFinishSetup() {
    gSetup = false; kbOpen = false; authReset();
    lockSave(); saveCfg();
}
static void authSubmit(bool pic) {
    if (SDL_GetTicks() < gAuthLockUntil) return;
    if (gAuthMode == 0) { // verify the current sign-in
        bool ok;
        if (gAuthMethod == 3) { ok = gAuthG.size() == 3 && gLockPic3.size() == 3; for (int i = 0; ok && i < 3; i++) ok = gestMatch(gAuthG[i], gLockPic3[i]); }
        else ok = lockHash(authSecret()) == gLockHash;
        if (!ok) { authFail(); return; }
        if (!gSetup) { authSuccess(); return; }
        if (gSetupTarget == 0) { gLockMethod = 0; gLockHash.clear(); gLockLen = 0; gLockPic3.clear(); authFinishSetup(); toast("Sign-in turned off"); return; }
        authBegin(1, gSetupTarget); gAuthMsg = "Create your new sign-in";
        return;
    }
    if (gAuthMode == 1) {
        if (pic) { gAuthGTmp = gAuthG; } else {
            if ((gAuthMethod == 1 && gAuthIn.size() < 4) || (gAuthMethod == 2 && gAuthIn.size() < 4) || (gAuthMethod == 4 && gAuthPat.size() < 4) || (gAuthMethod == 5 && gAuthBtns.size() < 4)) { gAuthMsg = gAuthMethod == 2 ? "Use at least 4 characters" : "Use at least 4"; authReset(); if (gAuthMethod == 2) openKbStr(&gAuthIn, 40, authPwEnter); return; }
            gAuthTmp = authSecret();
        }
        int m = gAuthMethod; authBegin(2, m); gAuthMsg = "Do it once more to confirm";
        return;
    }
    // confirm
    bool same;
    if (pic) { same = gAuthG.size() == 3 && gAuthGTmp.size() == 3; for (int i = 0; same && i < 3; i++) same = gestMatch(gAuthG[i], gAuthGTmp[i]); }
    else same = authSecret() == gAuthTmp;
    if (!same) { int m = gAuthMethod; authBegin(1, m); gAuthMsg = "They didn't match. Start again"; gAuthShake = 0.5f; return; }
    gLockMethod = gAuthMethod;
    if (pic) { gLockPic3 = gAuthGTmp; gLockHash.clear(); gLockLen = 0; }
    else { gLockHash = lockHash(gAuthTmp); gLockLen = gAuthMethod == 1 ? (int)gAuthIn.size() : gAuthMethod == 4 ? (int)gAuthPat.size() : gAuthMethod == 5 ? (int)gAuthBtns.size() : 0; gLockPic3.clear(); }
    authFinishSetup(); toast("Sign-in saved");
}
static void authCancel() {
    if (gSetup) { gSetup = false; authReset(); kbOpen = false; return; }
    gAuth = false; authReset(); kbOpen = false;
}
static void authPwEnter() { authSubmit(false); }
static const int PAT_CX = 640, PAT_CY = 410, PAT_SP = 110;
static void patDot(int i, int& x, int& y) { x = PAT_CX + (i % 3 - 1) * PAT_SP; y = PAT_CY + (i / 3 - 1) * PAT_SP; }
static int patHit(int x, int y) { for (int i = 0; i < 9; i++) { int dx, dy; patDot(i, dx, dy); if (gdist(x, y, dx, dy) < 46) return i; } return -1; }
static void patAdd(int d) {
    for (int v : gAuthPat) if (v == d) return;
    gAuthPat.push_back(d);
}
static const char* BTN_NAMES[12] = {"A", "B", "X", "Y", "Up", "Down", "Left", "Right", "L", "R", "ZL", "ZR"};
static const u64 BTN_MASK[12] = {HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y, HidNpadButton_AnyUp, HidNpadButton_AnyDown, HidNpadButton_AnyLeft, HidNpadButton_AnyRight, HidNpadButton_L, HidNpadButton_R, HidNpadButton_ZL, HidNpadButton_ZR};
static void authKeys(u64 kd) {
    if (!(gAuth || gSetup) || !kd) return;
    if (gAuthMethod == 5) {
        if (kd & HidNpadButton_Minus) { if (!gAuthBtns.empty()) gAuthBtns.pop_back(); return; }
        if ((kd & HidNpadButton_Plus) && gAuthMode != 0) { authSubmit(false); return; }
        for (int i = 0; i < 12; i++) if (kd & BTN_MASK[i]) {
            gAuthBtns.push_back(BTN_NAMES[i]);
            if (gAuthMode == 0 && (int)gAuthBtns.size() >= std::max(4, gLockLen)) authSubmit(false);
            return;
        }
    } else if (gAuthMethod == 4) {
        int cx = gAuthPatCur % 3, cy = gAuthPatCur / 3;
        if (kd & HidNpadButton_AnyLeft) cx = std::max(0, cx - 1);
        if (kd & HidNpadButton_AnyRight) cx = std::min(2, cx + 1);
        if (kd & HidNpadButton_AnyUp) cy = std::max(0, cy - 1);
        if (kd & HidNpadButton_AnyDown) cy = std::min(2, cy + 1);
        gAuthPatCur = cy * 3 + cx;
        if (kd & HidNpadButton_A) patAdd(gAuthPatCur);
        if (kd & HidNpadButton_B) gAuthPat.clear();
        if (kd & HidNpadButton_Plus) { if (gAuthPat.size() >= 4) authSubmit(false); else if (!gAuthPat.empty()) { gAuthMsg = "Connect at least 4 dots"; gAuthPat.clear(); } }
    }
}
static void authUpdate(float dt) {
    if (gAuthShake > 0) gAuthShake -= dt;
    // the left stick also moves the pattern cursor
    static bool stickHeld = false;
    if ((gAuth || gSetup) && gAuthMethod == 4) {
#ifndef WNX_HOST
        HidAnalogStickState st = padGetStickPos(&gPad, 0);
#else
        struct { int x, y; } st = {0, 0};
#endif
        bool act = abs(st.x) > 16000 || abs(st.y) > 16000;
        if (act && !stickHeld) {
            u64 k = 0;
            if (abs(st.x) > abs(st.y)) k = st.x > 0 ? HidNpadButton_Right : HidNpadButton_Left; else k = st.y > 0 ? HidNpadButton_Up : HidNpadButton_Down;
            authKeys(k);
        }
        stickHeld = act;
    }
}
static void authTouch(int phase, int x, int y) {
    if (!(gAuth || gSetup)) return;
    if (kbOpen && y >= KBTOP && gAuthMethod == 2) {
        if (phase == 0) { buildKeys(); int k = kbHit(x, y); if (k >= 0) { if (gKeys[k].act == "ENTER") authSubmit(false); else kbAction(gKeys[k].act); } }
        return;
    }
    if (phase == 0) { gAuthDown = true; gAuthMoved = false; gAuthX0 = x; gAuthY0 = y; gAuthStroke.clear(); gAuthStroke.push_back({x, y}); if (gAuthMethod == 4) { int d = patHit(x, y); if (d >= 0) patAdd(d); } return; }
    if (phase == 1) {
        if (!gAuthDown) return;
        if (gdist(x, y, gAuthX0, gAuthY0) > 14) gAuthMoved = true;
        gAuthStroke.push_back({x, y});
        if (gAuthMethod == 4) { int d = patHit(x, y); if (d >= 0) patAdd(d); }
        return;
    }
    if (!gAuthDown) return;
    gAuthDown = false;
    bool tap = !gAuthMoved;
    // ---- buttons common to every method
    if (tap) {
        if (inR(x, y, W / 2 - 100, UIH - 66, 200, 54)) { authCancel(); return; }
    }
    if (gAuthMethod == 1 && tap) {
        for (int i = 0; i < 12; i++) {
            int c = i % 3, r = i / 3, kx = W / 2 - 188 + c * 128, ky = 252 + r * 70;
            if (!inR(x, y, kx, ky, 120, 62)) continue;
            int d = i == 9 ? -1 : (i == 10 ? 0 : (i == 11 ? -2 : i + 1));
            if (d >= 0 && gAuthIn.size() < 12) { gAuthIn += (char)('0' + d); if (gAuthMode == 0 && (int)gAuthIn.size() >= std::max(4, gLockLen)) authSubmit(false); }
            else if (d == -2 && !gAuthIn.empty()) gAuthIn.pop_back();
            else if (d == -1 && gAuthMode != 0) authSubmit(false);   // OK key (not needed when signing in)
            else if (d == -1 && gAuthMode == 0) authSubmit(false);
            return;
        }
    }
    if (gAuthMethod == 2 && tap && inR(x, y, W / 2 - 100, 300, 200, 54)) { authSubmit(false); return; }
    if (gAuthMethod == 4) {
        if (!gAuthPat.empty() && !tap) {
            if (gAuthPat.size() >= 4) authSubmit(false); else { gAuthMsg = "Connect at least 4 dots"; gAuthPat.clear(); gAuthShake = 0.4f; }
        } else if (tap) gAuthPat.clear();
        return;
    }
    if (gAuthMethod == 3) {
        if (y < 100) return;
        gAuthG.push_back(gestClassify(gAuthStroke));
        gAuthStroke.clear();
        if (gAuthG.size() >= 3) authSubmit(true);
        return;
    }
}
static void drawGestSample(const Gest& g, Col c) {
    if (g.t == 0) { ring(g.x1, g.y1, 20, 4, c, Col{0, 0, 0}); }
    else if (g.t == 1) { line(g.x1, g.y1, g.x2, g.y2, 5, c); disc(g.x1, g.y1, 9, c); tri((float)g.x2, (float)g.y2, g.x2 + (g.x1 < g.x2 ? -22.0f : 22.0f), g.y2 - 14.0f, g.x2 + (g.x1 < g.x2 ? -22.0f : 22.0f), g.y2 + 14.0f, c); }
    else ring(g.x1, g.y1, std::max(12, g.x2), 4, c, Col{0, 0, 0});
}
static void drawAuthUI(bool lockBg) {
    int shake = gAuthShake > 0 ? (int)(sinf(gAuthShake * 60) * 14 * (gAuthShake / 0.5f)) : 0;
    if (lockBg) rect(0, 0, W, UIH, BLACK, gAuthMethod == 3 ? 40 : 150); else rect(0, 0, W, UIH, Col{18, 20, 26}, 255);
    int cxm = W / 2 + shake;
    drawAvatar(cxm - 40, 20, 80);
    text(gUser, F30, cxm, 104, WHITE, 1);
    const char* mt[6] = {"", "Enter your PIN", "Enter your password", "Draw your picture gestures", "Draw your pattern or use the D-pad and A", "Press your button code on the Joy-Con"};
    std::string hint = gAuthMsg;
    if (hint.empty()) {
        if (gAuthMode == 0) hint = mt[gAuthMethod];
        else hint = gAuthMode == 1 ? std::string("New: ") + mt[gAuthMethod] : std::string("Confirm: ") + mt[gAuthMethod];
    }
    if (SDL_GetTicks() < gAuthLockUntil) hint = "Too many tries. Wait " + std::to_string((gAuthLockUntil - SDL_GetTicks()) / 1000 + 1) + " seconds";
    text(hint, F22, cxm, 146, WHITE, 1, 220);
    if (gAuthMethod == 1) {
        int n = std::max(4, std::max(gLockLen, (int)gAuthIn.size()));
        if (gAuthMode != 0) n = std::max(4, (int)gAuthIn.size());
        for (int i = 0; i < n; i++) { int x = cxm - (n - 1) * 20 + i * 40; if (i < (int)gAuthIn.size()) disc(x, 214, 11, WHITE); else ring(x, 214, 11, 2, WHITE, Col{0, 0, 0}); }
        for (int i = 0; i < 12; i++) {
            int c = i % 3, r = i / 3, kx = cxm - 188 + c * 128, ky = 252 + r * 70;
            std::string l = i == 9 ? "OK" : (i == 10 ? "0" : (i == 11 ? "DEL" : std::to_string(i + 1)));
            rect(kx, ky, 120, 62, WHITE, 40); text(l, F44, kx + 60, ky + 8, WHITE, 1);
        }
    } else if (gAuthMethod == 2) {
        drawField(W / 2 - 300, 200, 600, 56, &gAuthIn, "Password", F30, DARK, WHITE, WHITE, true);
        btn(W / 2 - 100, 300, 200, 54, "OK", Col{0, 120, 215}, WHITE);
    } else if (gAuthMethod == 4) {
        for (int i = 0; i < 9; i++) {
            int x, y; patDot(i, x, y);
            bool on = false; for (int v : gAuthPat) if (v == i) on = true;
            if (i == gAuthPatCur) ring(x, y, 30, 2, Col{120, 200, 255}, Col{0, 0, 0});
            disc(x, y, on ? 16 : 10, on ? Col{0, 170, 255} : Col{230, 230, 235});
        }
        for (size_t i = 1; i < gAuthPat.size(); i++) { int x1, y1, x2, y2; patDot(gAuthPat[i - 1], x1, y1); patDot(gAuthPat[i], x2, y2); line(x1, y1, x2, y2, 6, Col{0, 170, 255}); }
        if (gAuthDown && !gAuthPat.empty() && !gAuthStroke.empty()) { int x1, y1; patDot(gAuthPat.back(), x1, y1); line(x1, y1, gAuthStroke.back().first, gAuthStroke.back().second, 4, Col{0, 170, 255}, 150); }
    } else if (gAuthMethod == 5) {
        int n = std::max(4, std::max(gLockLen, (int)gAuthBtns.size()));
        for (int i = 0; i < n; i++) { int x = cxm - (n - 1) * 24 + i * 48; if (i < (int)gAuthBtns.size()) disc(x, 280, 14, WHITE); else ring(x, 280, 14, 2, WHITE, Col{0, 0, 0}); }
        text("A B X Y, D-pad, L R ZL ZR", F22, cxm, 350, WHITE, 1, 170);
        text(gAuthMode != 0 ? "Minus deletes the last press, Plus saves it" : "Minus deletes the last press", F16, cxm, 390, WHITE, 1, 150);
    } else if (gAuthMethod == 3) {
        text("Gesture " + std::to_string(std::min<size_t>(3, gAuthG.size() + 1)) + " of 3: tap, draw a line or draw a circle", F22, cxm, 190, WHITE, 1, 200);
        for (auto& g : gAuthG) drawGestSample(g, Col{255, 255, 255});
        if (gAuthDown) for (size_t i = 1; i < gAuthStroke.size(); i++) line(gAuthStroke[i - 1].first, gAuthStroke[i - 1].second, gAuthStroke[i].first, gAuthStroke[i].second, 5, Col{0, 190, 255});
    }
    btn(W / 2 - 100, UIH - 66, 200, 54, "Cancel", Col{70, 74, 86}, WHITE);
}
static void authStartSetup(int target) {
    gSetup = true; gSetupTarget = target; gAuthFails = 0;
    if (gLockMethod == 0 && target != 0) { authBegin(1, target); gAuthMsg = "Create your new sign-in"; }
    else if (gLockMethod == 0) { gSetup = false; }
    else { authBegin(0, gLockMethod); gAuthMsg = "First confirm your current sign-in"; }
}
// ---- lock-screen widgets
static void drawLockWidgets(int o) {
    struct Wd { int kind; } items[4]; int n = 0;
    if (gWeatherLock && gWx.have) items[n++].kind = 0;
    if (gCalLock) items[n++].kind = 1;
    if (gMailLock && !gMAddr.empty()) items[n++].kind = 2;
    if (gMusicLock && musPlaying && musIdx >= 0 && musIdx < (int)gSongs.size()) items[n++].kind = 3;
    for (int k = 0; k < n; k++) {
        int x = W - 390, y = o + UIH - 40 - (k + 1) * 78;
        rect(x, y, 350, 68, BLACK, 110);
        float tm = SDL_GetTicks() / 1000.0f;
        switch (items[k].kind) {
            case 0: wxIcon(gWx.code, gWx.isDay, x + 36, y + 34, 44, tm); text(wxTemp(gWx.temp), F30, x + 76, y + 6, WHITE); text(gWxCity + "  " + wxText(gWx.code), F16, x + 76, y + 44, WHITE, 0, 200); break;
            case 1: { text("Next up", F16, x + 16, y + 6, WHITE, 0, 170); std::string t = calTileLine(); while (t.size() > 3 && tw(t, F22) > 320) t.pop_back(); text(t, F22, x + 16, y + 30, WHITE); break; }
            case 2: text(std::to_string(gMUnread) + (gMUnread == 1 ? " unread email" : " unread emails"), F22, x + 16, y + 20, WHITE); break;
            default: { std::string t = gSongs[musIdx].title; while (t.size() > 3 && tw(t, F22) > 320) t.pop_back(); text("Now playing", F16, x + 16, y + 6, WHITE, 0, 170); text(t, F22, x + 16, y + 30, WHITE); }
        }
    }
}
// ---- PC settings pages: Users / Accounts
static int gUsrTab = 0;
static std::string gNameEdit, gStatusEdit;
static void applyName() { if (!gNameEdit.empty()) gUser = gNameEdit; lockSave(); hideKb(); }
static void applyStatus() { gUserStatus = gStatusEdit; lockSave(); hideKb(); }
static void drawUsersPage() {
    int X = 500;
    static const char* SUB[2] = {"Your account", "Sign-in options"};
    int tx = X;
    for (int i = 0; i < 2; i++) {
        text(SUB[i], F22, tx, 50, i == gUsrTab ? SET_BLUE : Col{70, 70, 76});
        if (i == gUsrTab) rect(tx, 82, tw(SUB[i], F22), 3, SET_BLUE);
        tx += tw(SUB[i], F22) + 44;
    }
    if (gUsrTab == 0) {
        drawAvatar(X, 116, 150);
        text(gUser, F44, X + 180, 118, Col{50, 50, 55});
        btn(X + 180, 180, 160, 44, "Edit name", SET_BLUE, WHITE);
        text(gUserStatus.empty() ? "No status message" : gUserStatus, F22, X + 180, 238, Col{120, 120, 128});
        btn(X + 360, 180, 180, 44, "Edit status", Col{232, 233, 237}, Col{50, 50, 55});
        text("Account colour", F22, X, 290, Col{60, 60, 66});
        for (int i = 0; i < 8; i++) {
            rect(X + i * 80, 326, 68, 68, AVCOL[i]);
            if (i == (gAvatar & 7) && !gAvatarTex) outline(X + i * 80 - 4, 322, 76, 76, 4, SET_BLUE);
        }
        setRow(420, "Show my initial", "Use a letter instead of the person picture", gAvInitial);
        btn(X, 500, 380, 54, "Pick a picture from Photos", SET_BLUE, WHITE);
        if (gAvatarTex) btn(X + 400, 500, 300, 54, "Remove my picture", Col{232, 233, 237}, Col{50, 50, 55});
        text("Open a picture there and tap \"Account pic\".", F16, X, 566, Col{120, 120, 128});
    } else {
        text("Choose how to unlock the lock screen", F22, X, 110, Col{60, 60, 66});
        static const char* DESC[6] = {"Just swipe up", "4 or more digits", "Letters and numbers", "Tap, line and circle on your lock screen picture", "Connect dots by touch or with the D-pad and A", "Press 4 or more Joy-Con buttons"};
        for (int i = 0; i < 6; i++) {
            int y = 148 + i * 66;
            rect(X, y, 700, 58, i == gLockMethod ? SET_BLUE : Col{232, 233, 237});
            text(LOCKN[i], F22, X + 20, y + 6, i == gLockMethod ? WHITE : Col{50, 50, 55});
            text(DESC[i], F16, X + 20, y + 34, i == gLockMethod ? WHITE : Col{120, 120, 128}, 0, 200);
        }
        text("This only locks the Windows screen. It does not protect your SD card or your Switch.", F16, X, 556, Col{120, 120, 128});
        text("Tip: hold B + Y while starting Windows to go back to the original menu.", F16, X, 580, Col{120, 120, 128});
    }
}
static bool usersTap(int x, int y) {
    int X = 500;
    static const char* SUB[2] = {"Your account", "Sign-in options"};
    int tx = X;
    for (int i = 0; i < 2; i++) { int w = tw(SUB[i], F22); if (inR(x, y, tx - 8, 40, w + 16, 50)) { gUsrTab = i; return true; } tx += w + 44; }
    if (gUsrTab == 0) {
        if (inR(x, y, X + 180, 180, 160, 44)) { gNameEdit = gUser; openKbStr(&gNameEdit, 20, applyName); return true; }
        if (inR(x, y, X + 360, 180, 180, 44)) { gStatusEdit = gUserStatus; openKbStr(&gStatusEdit, 60, applyStatus); return true; }
        for (int i = 0; i < 8; i++) if (inR(x, y, X + i * 80, 326, 68, 68)) { gAvatar = i; if (gAvatarTex) { SDL_DestroyTexture(gAvatarTex); gAvatarTex = nullptr; remove((DIR_ + "avatar.png").c_str()); } saveCfg(); return true; }
        if (inR(x, y, 1100, 420, 160, 70)) { gAvInitial = !gAvInitial; lockSave(); return true; }
        if (inR(x, y, X, 500, 380, 54)) { pendingApp = A_PHOTOS; closeApp(false); toast("Open a picture and tap Account pic"); return true; }
        if (gAvatarTex && inR(x, y, X + 400, 500, 300, 54)) { SDL_DestroyTexture(gAvatarTex); gAvatarTex = nullptr; remove((DIR_ + "avatar.png").c_str()); return true; }
    } else {
        for (int i = 0; i < 6; i++) if (inR(x, y, X, 148 + i * 66, 700, 58)) { if (i != gLockMethod) authStartSetup(i); return true; }
    }
    return false;
}

static void drawSettings(const Theme& th) {
    (void)th;
    rect(0, 0, W, UIH, WHITE);
    rect(0, 0, 440, UIH, Col{244, 245, 247});
    text("PC settings", F72, 56, 22, Col{90, 110, 140});
    for (int i = 0; i < SP_N; i++) {
        int y = SP_Y(i);
        if (i == setPage) rect(0, y, 440, 37, SET_BLUE);
        text(SET_PAGES[i], F22, 56, y + 4, i == setPage ? WHITE : Col{60, 60, 66});
    }
    int X = 500;
    if (setPage == 0) {
        static const char* SUB[4] = {"Lock screen", "Start screen", "Tiles", "Account picture"};
        int tx = X;
        for (int i = 0; i < 4; i++) {
            text(SUB[i], F22, tx, 50, i == setSub ? SET_BLUE : Col{70, 70, 76});
            if (i == setSub) rect(tx, 82, tw(SUB[i], F22), 3, SET_BLUE);
            tx += tw(SUB[i], F22) + 44;
        }
        if (setSub == 0) {
            rect(X, 116, 500, 262, Col{230, 230, 235});
            if (gLockTex) { SDL_Rect d = {X + 4, 120, 492, 254}; SDL_RenderCopy(gR, gLockTex, nullptr, &d); }
            text(timeStr(), F44, X + 30, 300, WHITE, 0, 230);
            setRow(410, "Show the lock screen", "Swipe up to unlock when Windows starts", gLockScr);
            text("Use your own picture: put lockscreen.jpg or lockscreen.png in", F16, X, 490, Col{120, 120, 128});
            text("sdmc:/switch/WinNX/ and restart Windows.", F16, X, 512, Col{120, 120, 128});
        } else if (setSub == 1) {
            text("Colours", F30, X, 116, Col{60, 60, 66});
            static const char* TN[4] = {"Violet", "Blue", "Teal", "Crimson"};
            for (int i = 0; i < 4; i++) {
                int x = X + i * 160;
                rect(x, 170, 140, 140, mix(THEMES[i].top, THEMES[i].bottom, 0.5f));
                rect(x, 270, 140, 40, THEMES[i].accent);
                text(TN[i], F16, x + 70, 322, Col{70, 70, 76}, 1);
                if (i == gTheme) outline(x - 5, 165, 150, 150, 4, SET_BLUE);
            }
            setRow(390, "Boot animation", "Windows logo animation at start-up", gBootAnim);
        } else if (setSub == 2) {
            drawTilesSub(X);
        } else {
            rect(X, 130, 150, 150, Col{150, 150, 160});
            disc(X + 75, 190, 30, WHITE); rect(X + 35, 232, 80, 48, WHITE);
            text(gUser, F44, X + 190, 160, Col{50, 50, 55});
            text("Local account", F22, X + 190, 214, Col{120, 120, 128});
        }
    } else if (setPage == 1) {
        drawUsersPage();
    } else if (setPage == 4) {
        drawSoundPage(X);
    } else if (setPage == 9) {
        drawOneDrivePage(X);
    } else if (setPage == 10) {
        drawPcInfoPage(X);
    } else if (setPage == 12) {
        drawHomePage(X);
    } else if (setPage == 3) {
        text("Search", F44, X, 40, Col{70, 90, 120});
        text("Web search engine for CMrhoe and the Search charm", F22, X, 130, Col{60, 60, 66});
        for (int i = 0; i < 3; i++) {
            static const char* EN[3] = {"DuckDuckGo", "Google", "Bing"};
            rect(X, 180 + i * 64, 360, 52, i == gEngine ? SET_BLUE : Col{232, 233, 237});
            text(EN[i], F22, X + 20, 190 + i * 64, i == gEngine ? WHITE : Col{60, 60, 66});
        }
    } else if (setPage == 5) {
        text("General", F44, X, 40, Col{70, 90, 120});
        setRow(130, "24-hour clock", "Off shows 12-hour time", g24);
        setRow(220, "Burn-in protection", "Dims and shifts the screen when idle (OLED)", gBurn);
        setRow(310, "Boot animation", "Windows logo animation at start-up", gBootAnim);
        text("Document text size", F22, X, 400, Col{50, 50, 55});
        btn(1050, 396, 60, 40, "A-", Col{232, 233, 237}, Col{50, 50, 55});
        btn(1120, 396, 60, 40, "A+", Col{232, 233, 237}, Col{50, 50, 55});
        setRow(470, "Temperature in Fahrenheit", "Weather shows \xC2\xB0" "F instead of \xC2\xB0" "C", gTempF);
    } else if (setPage == 6) {
        text("Privacy", F44, X, 40, Col{70, 90, 120});
        text("Windows keeps your files on your SD card, in switch/WinNX/.", F22, X, 130, Col{60, 60, 66});
        btn(X, 200, 360, 52, "Clear CMrhoe history", SET_BLUE, WHITE);
        setRow(290, "Location for Weather", "Lets Weather find your city automatically (looks up your network address)", gLocOK);
        setRow(380, "Offline mode", "Turns Wi-Fi features off: Weather, Mail, News, live tiles", gOffline);
    } else if (setPage == 7) {
        text("Devices", F44, X, 40, Col{70, 90, 120});
        text("Battery", F22, X, 130, Col{60, 60, 66});
        rect(X, 170, 400, 24, Col{225, 225, 230}); rect(X, 170, (int)(400 * std::min<u32>(gBat, 100) / 100.0f), 24, SET_BLUE);
        text(std::to_string(gBat) + "%", F22, X + 420, 168, Col{60, 60, 66});
        text("Wi-Fi", F22, X, 240, Col{60, 60, 66});
        text(netStatusText(), F22, X, 280, Col{80, 80, 88});
        drawWifiIcon(X + 420, 276, gWifiBars, Col{60, 60, 66}, 255);
    } else if (setPage == 8) {
        text("Ease of Access", F44, X, 40, Col{70, 90, 120});
        text("Text size in Word", F22, X, 130, Col{60, 60, 66});
        btn(X, 180, 80, 52, "A-", Col{232, 233, 237}, Col{50, 50, 55});
        btn(X + 96, 180, 80, 52, "A+", Col{232, 233, 237}, Col{50, 50, 55});
    } else if (setPage == 11) {
        text("Windows Update", F44, X, 40, Col{70, 90, 120});
        text("Windows 8.1 - you have the latest updates", F22, X, 112, Col{60, 60, 66});
        text("Homebrew menu", F30, X, 160, Col{70, 90, 120});
        text("Replace hbmenu so the Album launch opens Windows 8.1. Your original is backed up first.", F16, X, 202, Col{120, 120, 128});
        bool c1 = gHbConfirm > 0 && gHbWhich == 1, c2 = gHbConfirm > 0 && gHbWhich == 2;
        btn(X, 236, 480, 54, c1 ? "Tap again to confirm" : "Make Windows 8.1 my homebrew menu", c1 ? Col{209, 52, 56} : SET_BLUE, WHITE);
        btn(X, 300, 480, 54, c2 ? "Tap again to confirm" : "Restore original hbmenu", c2 ? Col{209, 52, 56} : Col{110, 110, 118}, WHITE);
        btn(X, 364, 480, 54, "Open original hbmenu now", Col{110, 110, 118}, WHITE);
        text(std::string("Windows 8.1 is the homebrew menu: ") + (gHbIsWinNX ? "Yes" : "No"), F22, X, 438, Col{60, 60, 66});
        text(std::string("Backup of original: ") + (gHbBackup ? "saved" : "none"), F22, X, 470, Col{60, 60, 66});
        int my = 510;
        for (auto& l : wrapText(gHbMsg, F16, 700)) { text(l, F16, X, my, Col{190, 110, 0}); my += 22; }
        text("Rescue: hold B + Y while Windows starts to jump to the original hbmenu.", F16, X, 626, Col{120, 120, 128});
    } else {
        text(SET_PAGES[setPage], F44, X, 40, Col{70, 90, 120});
        text("There are no settings here on Switch.", F22, X, 130, Col{120, 120, 128});
    }
    if (gSetup) drawAuthUI(false);
}
static void settingsTapImpl(int x, int y) {
    if (gSetup) return;
    for (int i = 0; i < SP_N; i++) if (inR(x, y, 0, SP_Y(i), 440, 38)) { setPage = i; setSub = 0; return; }
    int X = 500;
    if (setPage == 10 && inR(x, y, X, 130, 360, 70)) { eggCount(); return; }
    if (setPage == 11 && inR(x, y, X, 100, 600, 40)) { eggCount(); return; }
    if (setPage == 1 && usersTap(x, y)) return;
    if (setPage == 0) {
        static const char* SUB[4] = {"Lock screen", "Start screen", "Tiles", "Account picture"};
        int tx = X;
        for (int i = 0; i < 4; i++) { int w = tw(SUB[i], F22); if (inR(x, y, tx - 8, 40, w + 16, 50)) { setSub = i; return; } tx += w + 44; }
        if (setSub == 0 && inR(x, y, 1100, 400, 160, 60)) { gLockScr = !gLockScr; saveCfg(); }
        else if (setSub == 1) {
            for (int i = 0; i < 4; i++) if (inR(x, y, X + i * 160, 170, 140, 140)) { gTheme = i; saveCfg(); }
            if (inR(x, y, 1100, 380, 160, 60)) { gBootAnim = !gBootAnim; saveCfg(); }
        } else if (setSub == 2) tilesSubTap(X, x, y);
    } else if (setPage == 4) { soundPageTap(X, x, y);
    } else if (setPage == 9) { oneDrivePageTap(X, x, y);
    } else if (setPage == 12) { homePageTap(X, x, y);
    } else if (setPage == 3) {
        for (int i = 0; i < 3; i++) if (inR(x, y, X, 180 + i * 64, 360, 52)) { gEngine = i; saveCfg(); }
    } else if (setPage == 5) {
        if (inR(x, y, 1100, 120, 160, 70)) { g24 = !g24; saveCfg(); }
        else if (inR(x, y, 1100, 210, 160, 70)) { gBurn = !gBurn; saveCfg(); }
        else if (inR(x, y, 1100, 300, 160, 70)) { gBootAnim = !gBootAnim; saveCfg(); }
        else if (inR(x, y, 1050, 396, 60, 40)) { gDocFont = std::max(F16, gDocFont - 1); saveCfg(); }
        else if (inR(x, y, 1120, 396, 60, 40)) { gDocFont = std::min(F44, gDocFont + 1); saveCfg(); }
        else if (inR(x, y, 1100, 460, 160, 70)) { gTempF = !gTempF; saveCfg(); }
    } else if (setPage == 6) {
        if (inR(x, y, X, 200, 360, 52)) { gHist.clear(); saveHist(); toast("Browser history cleared"); }
        else if (inR(x, y, 1100, 280, 160, 70)) { gLocOK = !gLocOK; saveCfg(); }
        else if (inR(x, y, 1100, 370, 160, 70)) { gOffline = !gOffline; saveCfg(); gNetTickT = 0; }
    } else if (setPage == 8) {
        if (inR(x, y, X, 180, 80, 52)) { gDocFont = std::max(F16, gDocFont - 1); saveCfg(); }
        else if (inR(x, y, X + 96, 180, 80, 52)) { gDocFont = std::min(F44, gDocFont + 1); saveCfg(); }
    } else if (setPage == 11) {
        if (inR(x, y, X, 236, 480, 54)) {
            if (gHbConfirm > 0 && gHbWhich == 1) { gHbConfirm = 0; gHbWhich = 0; hbInstall(); }
            else { gHbWhich = 1; gHbConfirm = 4; gHbMsg.clear(); }
        } else if (inR(x, y, X, 300, 480, 54)) {
            if (gHbConfirm > 0 && gHbWhich == 2) { gHbConfirm = 0; gHbWhich = 0; hbRestore(); }
            else { gHbWhich = 2; gHbConfirm = 4; gHbMsg.clear(); }
        } else if (inR(x, y, X, 364, 480, 54)) hbOpenOriginal();
    }
}
static void appKeys(u64 kd) {
    int d = -1;
    if (kd & HidNpadButton_AnyLeft) d = 0; else if (kd & HidNpadButton_AnyRight) d = 1;
    else if (kd & HidNpadButton_AnyUp) d = 2; else if (kd & HidNpadButton_AnyDown) d = 3;
    if (curApp == A_2048 && d >= 0) g2Move(d);
    if (curApp == A_SNAKE && d >= 0) snakeTurn(d);
    if (curApp == A_BLOCKS) blocksKey(kd);
    if (curApp == A_CALENDAR && (d == 0 || d == 1)) calMonth(d == 0 ? -1 : 1);
    if (curApp == A_MUSIC) {
        if (kd & HidNpadButton_A) musicTap(430, UIH - MUS_BAR + 46);
        if (kd & HidNpadButton_L) musNextTrack(-1, false);
        if (kd & HidNpadButton_R) musNextTrack(1, false);
        if (kd & HidNpadButton_Y) { musShuffle = !musShuffle; musSettingsSave(); }
        if (d == 0) musSetVol(musVol - 0.1f); else if (d == 1) musSetVol(musVol + 0.1f);
        if (d == 2) musScroll = std::max(0, musScroll - 120); else if (d == 3) musScroll = std::min(musScrollMax, musScroll + 120);
    }
    if (curApp == A_SETTINGS && gSetup) authKeys(kd);
    // @@KEYS@@
}
static void drawApp() {
    const Theme& th = THEMES[gTheme];
    Col bg = appBg(curApp);
    int bottom = areaBottom();
    switch (curApp) {
        case A_CALC: {
            if (gCalcMode != 0) { calcDrawMode(th); break; }
            rect(0, 0, W, UIH, bg);
            calcModeBtn(SCI_TY + 14);
            rect(348, 34, 584, 124, Col{42, 42, 42});
            if (cOp) {
                const char* sym[5] = {"", "+", "-", "\xC3\x97", "\xC3\xB7"};
                char b[48]; snprintf(b, sizeof b, "%.10g %s", cAcc, sym[cOp]);
                text(b, F22, 348 + 584 - 16, 40, WHITE, 2, 160);
            }
            text(cCur, cCur.size() > 11 ? F44 : F72, 348 + 584 - 16, 64, WHITE, 2);
            for (int i = 0; i < NCB; i++) {
                SDL_Rect r = calcRect(CBTNS[i]);
                std::string l = CBTNS[i].l;
                Col c = Col{58, 58, 58};
                if (l == "=") c = th.accent;
                else if (l == "C") c = Col{170, 50, 50};
                else if (!(l[0] >= '0' && l[0] <= '9') && l != ".") c = Col{86, 86, 86};
                rect(r.x, r.y, r.w, r.h, c);
                text(l, F44, r.x + r.w / 2, r.y + r.h / 2 - 28, WHITE, 1);
            }
            calcMenuDraw();
            break;
        }
        case A_CLOCK: drawClock(bg); break;
        case A_RECORDER: drawRec(); break;
        case A_FOOD: drawFood(); break;
        case A_HEALTH: drawHealth(); break;
        case A_PET: drawPet(); break;
        case A_CITY: drawCity(); break;
        case A_CALENDAR: drawCalendar(); break;
        case A_SETTINGS: drawSettings(th); break;
        case A_POWER: {
            rect(0, 0, W, UIH, bg);
            appTitle("Power");
            text("Exit Windows, lock the screen, or open the original hbmenu.", F30, 70, 170, WHITE);
            rect(70, 260, 260, 90, Col{209, 52, 56});
            text("Exit", F44, 200, 282, WHITE, 1);
            rect(350, 260, 260, 90, Col{40, 120, 200});
            text("Lock", F44, 480, 282, WHITE, 1);
            rect(630, 260, 260, 90, Col{80, 80, 80});
            text("Cancel", F44, 760, 282, WHITE, 1);
            rect(70, 380, 820, 90, Col{70, 70, 76});
            text("Open original hbmenu", F44, 480, 402, WHITE, 1);
            break;
        }
        case A_NOTES: {
            rect(0, 0, W, UIH, bg);
            text("Notes", F44, 70, 40, WHITE);
            btn(1110, 42, 100, 42, "Clear", Col{150, 20, 55}, WHITE);
            rect(70, 100, 1140, bottom - 116, Col{252, 246, 205});
            drawEditor(gNote, SDL_Rect{94, 118, 1092, bottom - 116 - 36}, F30, Col{50, 40, 20}, false, kbOpen && kbTarget == 1, "Tap to start typing");
            break;
        }
        case A_WORD: drawWord(bottom); break;
        case A_EXCEL: drawExcel(); break;
        case A_STORE: drawStore(th, bottom); break;
        case A_BROWSER: drawBrowser(bottom); break;
        case A_PAINT: drawPaint(); break;
        case A_2048: draw2048(); break;
        case A_MINES: drawMines(); break;
        case A_SNAKE: drawSnake(); break;
        case A_SOLITAIRE: drawSolitaire(); break;
        case A_MUSIC: drawMusic(); break;
        case A_WEATHER: drawWeather(bottom); break;
        case A_MAIL: drawMail(); break;
        case A_NEWS: drawNews(); break;
        case A_ROBLOX: drawRoblox(); break;
        case A_FILES: drawFiles(); break;
        case A_MAPS: drawMaps(); break;
        case A_PODCASTS: drawPodcasts(); break;
        case A_RADIO: drawRadio(); break;
        case A_CORTANA: drawCortana(); break;
        case A_CONV: drawConv(); break;
        case A_BLOCKS: drawBlocks(); break;
        case A_LIGHT: drawLight(); break;
        case A_WORLD: drawWorld(); break;
        case A_DICT: drawDict(); break;
        case A_THEMES: drawThemes(); break;
        case A_CAMERA: drawCamera(); break;
        case A_PDF: drawPdf(); break;
        case A_CHESS: drawChess(); break;
        case A_TERM: case A_PS: gTermCur = curApp == A_PS ? 1 : 0; drawTerm(); break;
        case A_DESKTOP: drawDesktop(); break;
        case A_HELP: drawHelp(); break;
        case A_SKYPE: drawSkype(); break;
        case A_ONEDRIVE: drawOneDrive(); break;
        case A_TASKMGR: drawTaskMgr(); break;
        // @@DRAW@@
        case A_PHOTOS: drawPhotos(); break;
        case A_GAMES: drawGames(bg); break;
        default: {
            rect(0, 0, W, UIH, bg);
            appTitle(appName(curApp).c_str());
            const char* msg = "This app isn't available yet.";
            if (curApp == A_MAIL || curApp == A_WEATHER) msg = "Needs an internet connection. Coming in a later version.";
            text(msg, F30, 70, 190, WHITE);
            text("Press + or tap the Windows button to go back.", F22, 70, 240, WHITE, 0, 180);
            drawIcon(curApp, W - 200, 200, bg);
            break;
        }
    }
}


// ---------------------------------------------------------------- snap view (two apps side by side, scaled 50%)
static void appTap(int x, int y); static void appDrag(int dx, int dy);
static SDL_Texture* gSnapT[2] = {nullptr, nullptr}; static int gSnapSide = 0;
#define SNAP_Y 100
#define SNAP_H 334
static void snapEnsure() { for (int i = 0; i < 2; i++) if (!gSnapT[i]) gSnapT[i] = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, UIH); }
static void drawSnap() {
    if (gSnap == curApp || gSnap < 0 || !running[gSnap]) { gSnap = -1; drawApp(); return; }
    snapEnsure(); if (!gSnapT[0] || !gSnapT[1]) { gSnap = -1; drawApp(); return; }
    bool kb = kbOpen; kbOpen = false; int pc = curApp; SDL_Texture* prev = gTarget;
    for (int i = 0; i < 2; i++) {
        SDL_SetRenderTarget(gR, gSnapT[i]); SDL_RenderSetClipRect(gR, nullptr);
        SDL_SetRenderDrawColor(gR, 0, 0, 0, 255); SDL_RenderClear(gR);
        curApp = i ? gSnap : pc; drawApp();
    }
    curApp = pc; kbOpen = kb; SDL_RenderSetClipRect(gR, nullptr); SDL_SetRenderTarget(gR, prev);
    rect(0, 0, W, UIH, Col{8, 8, 10});
    SDL_Rect d0{0, SNAP_Y, 640, SNAP_H}, d1{640, SNAP_Y, 640, SNAP_H};
    SDL_RenderCopy(gR, gSnapT[0], nullptr, &d0); SDL_RenderCopy(gR, gSnapT[1], nullptr, &d1);
    rect(638, SNAP_Y, 4, SNAP_H, Col{120, 120, 130});
    text(appName(pc), F22, 20, 14, WHITE); text(appName(gSnap), F22, 660, 14, WHITE);
    btn(20, 52, 140, 40, "Swap", Col{70, 68, 74}, WHITE); btn(660, 52, 180, 40, "Close snap", Col{196, 43, 28}, WHITE);
    text("Snap view is scaled to half size.", F16, 640, SNAP_Y + SNAP_H + 20, Col{140, 140, 150}, 1);
}
static void snapMapC(int side, int& x, int& y) { x = std::max(0, std::min(W - 1, (x - side * 640) * 2)); y = std::max(0, std::min(UIH - 1, (y - SNAP_Y) * 2)); }
static void snapTouch(int ph, int x, int y) {
    if (gSnap < 0) { appTouch(ph, x, y); return; }
    if (ph == 0) gSnapSide = x >= 640;
    int pc = curApp; if (gSnapSide) curApp = gSnap; snapMapC(gSnapSide, x, y); appTouch(ph, x, y); curApp = pc;
}
static void snapDrag(int dx, int dy) {
    if (gSnap < 0) { appDrag(dx, dy); return; }
    int pc = curApp; if (gSnapSide) curApp = gSnap; appDrag(dx * 2, dy * 2); curApp = pc;
}
static void snapTap(int x, int y) {
    if (gSnap < 0) { appTap(x, y); return; }
    if (y < SNAP_Y) {
        if (inR(x, y, 20, 52, 140, 40)) { std::swap(curApp, gSnap); lastApp = curApp; }
        else if (inR(x, y, 660, 52, 180, 40)) gSnap = -1;
        return;
    }
    if (y >= SNAP_Y + SNAP_H) return;
    int side = x >= 640; int pc = curApp; if (side) curApp = gSnap; snapMapC(side, x, y); appTap(x, y); curApp = pc;
}
static void drawTopBar() {
    if (barP < 0.01f) return;
    int y = (int)(-44 + 44 * barP);
    rect(0, y, W, 44, Col{18, 18, 20}, 238);
    text(appName(curApp), F22, 20, y + 8, WHITE);
    rect(W - 150, y + 21, 40, 3, WHITE);
    rect(W - 90, y, 90, 44, Col{196, 43, 28});
    line(W - 58, y + 13, W - 32, y + 31, 3, WHITE);
    line(W - 58, y + 31, W - 32, y + 13, 3, WHITE);
}

static void settingsTap(int x, int y, const Theme& th) { (void)th; settingsTapImpl(x, y); }
static void appTap(int x, int y) {
    switch (curApp) {
        case A_CALC:
            if (calcTapMode(x, y)) break;
            for (int i = 0; i < NCB; i++) {
                SDL_Rect r = calcRect(CBTNS[i]);
                if (inR(x, y, r.x, r.y, r.w, r.h)) { calcPress(CBTNS[i].l); break; }
            }
            break;
        case A_SETTINGS:
            settingsTap(x, y, THEMES[gTheme]);
            break;
        case A_POWER:
            if (inR(x, y, 70, 260, 260, 90)) gRun = false;
            else if (inR(x, y, 350, 260, 260, 90)) { closeApp(false); uiState = 1; lockOff = 0; lockAnim = false; }
            else if (inR(x, y, 630, 260, 260, 90)) closeApp(false);
            else if (inR(x, y, 70, 380, 820, 90)) hbOpenOriginal();
            break;
        case A_NOTES:
            if (inR(x, y, 1110, 42, 100, 42)) { backupFile(DIR_ + "notes.txt"); gNote.s.clear(); gNote.dirty = true; }
            else if (inR(x, y, 70, 100, 1140, areaBottom() - 116)) openKb(1);
            break;
        case A_WORD: wordTap(x, y); break;
        case A_EXCEL: excelTap(x, y); break;
        case A_STORE: storeTap(x, y); break;
        case A_BROWSER: browserTap(x, y); break;
        case A_PAINT: paintTap(x, y); break;
        case A_2048: tap2048(x, y); break;
        case A_MINES: tapMines(x, y); break;
        case A_SNAKE: tapSnake(x, y); break;
        case A_CALENDAR: calendarTap(x, y); break;
        case A_MUSIC: musicTap(x, y); break;
        case A_WEATHER: weatherTap(x, y); break;
        case A_MAIL: mailTap(x, y); break;
        case A_NEWS: newsTap(x, y); break;
        case A_ROBLOX: robloxTap(x, y); break;
        case A_FILES: filesTap(x, y); break;
        case A_MAPS: mapsTap(x, y); break;
        case A_PODCASTS: podcastsTap(x, y); break;
        case A_RADIO: radioTap(x, y); break;
        case A_CORTANA: cortanaTap(x, y); break;
        case A_CONV: convTap(x, y); break;
        case A_BLOCKS: blocksTap(x, y); break;
        case A_LIGHT: lightTap(x, y); break;
        case A_WORLD: worldTap(x, y); break;
        case A_DICT: dictTap(x, y); break;
        case A_THEMES: themesTap(x, y); break;
        case A_CAMERA: camTap(x, y); break;
        case A_PDF: pdfTap(x, y); break;
        case A_CHESS: chessTap(x, y); break;
        case A_TERM: case A_PS: gTermCur = curApp == A_PS ? 1 : 0; termTap(x, y); break;
        case A_DESKTOP: desktopTap(x, y); break;
        case A_CLOCK: clkTap(x, y); break;
        case A_RECORDER: recTap(x, y); break;
        case A_FOOD: foodTap(x, y); break;
        case A_HEALTH: hlTap(x, y); break;
        case A_PET: petTap(x, y); break;
        case A_CITY: cityTap(x, y); break;
        case A_HELP: helpTap(x, y); break;
        case A_SKYPE: skypeTap(x, y); break;
        case A_ONEDRIVE: oneDriveTap(x, y); break;
        case A_TASKMGR: taskMgrTap(x, y); break;
        // @@TAP@@
        case A_PHOTOS: photoTap(x, y); break;
        case A_GAMES: gamesTap(x, y); break;
        default: break;
    }
}
static void appDrag(int dx, int dy) {
    static int accX = 0, accY = 0;
    switch (curApp) {
        case A_NOTES: gNote.scroll -= dy; break;
        case A_WORD: if (wView == 1) wPicScroll -= dy; else wScroll -= dy; break;
        case A_GAMES: gamesScroll -= dy; break;
        case A_MUSIC: if (!musSeeking && !musVolDrag) musScroll -= dy; break;
        case A_MAIL: mailDrag(dy); break;
        case A_NEWS: newsDrag(dy); break;
        case A_CALENDAR: calendarDrag(dx, dy); break;
        case A_FILES: filesDrag(dx, dy); break;
        case A_MAPS: mapsDrag(dx, dy); break;
        case A_PODCASTS: podcastsDrag(dx, dy); break;
        case A_RADIO: radioDrag(dx, dy); break;
        case A_CORTANA: cortanaDrag(dx, dy); break;
        case A_HELP: helpDrag(dx, dy); break;
        case A_CALC: calcDragMode(dx, dy); break;
        case A_CLOCK: clkDrag(dx, dy); break;
        case A_RECORDER: recDrag(dx, dy); break;
        case A_FOOD: foodDrag(dx, dy); break;
        case A_HEALTH: hlDrag(dx, dy); break;
        case A_PET: petDrag(dx, dy); break;
        case A_CITY: cityDrag(dx, dy); break;
        case A_SKYPE: skypeDrag(dx, dy); break;
        case A_ONEDRIVE: oneDriveDrag(dx, dy); break;
        case A_TASKMGR: taskMgrDrag(dx, dy); break;
        case A_WORLD: worldDrag(dx, dy); break;
        case A_DICT: dictDrag(dx, dy); break;
        case A_THEMES: themesDrag(dx, dy); break;
        case A_PDF: pdfDrag(dx, dy); break;
        case A_TERM: case A_PS: gTermCur = curApp == A_PS ? 1 : 0; termDrag(dx, dy); break;
        case A_PHOTOS:
            if (phView == 0) phScroll -= dy;
            else if (phZ > 1.02f) { phOX += dx; phOY += dy; phClamp(); }
            break;
        case A_STORE: if (storeView == 0) storeScroll -= dy; break;
        case A_EXCEL: {
            accX += dx; accY += dy;
            int sr = accY / XROWH, sc = accX / (XCOLW / 2);
            if (sr) { xScrR = std::max(0, std::min(XR - 1, xScrR - sr)); accY -= sr * XROWH; }
            if (sc) { xScrC = std::max(0, std::min(XC - XVC, xScrC - sc)); accX -= sc * (XCOLW / 2); }
            break;
        }
        default: accX = accY = 0; break;
    }
}

// ---------------------------------------------------------------- boot animation, lock screen, charms, switcher
static const float BOOT_END = 3.9f;
static void fdisc(float cx, float cy, float r, Col c) {
    const int N = 120;
    SDL_Vertex v[N + 1];
    int idx[N * 3];
    SDL_Color sc = {c.r, c.g, c.b, 255};
    v[0].position.x = cx; v[0].position.y = cy; v[0].color = sc; v[0].tex_coord.x = 0; v[0].tex_coord.y = 0;
    for (int i = 0; i < N; i++) {
        float a = 6.2831853f * i / N;
        v[i + 1].position.x = cx + cosf(a) * r; v[i + 1].position.y = cy + sinf(a) * r;
        v[i + 1].color = sc; v[i + 1].tex_coord.x = 0; v[i + 1].tex_coord.y = 0;
        idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = (i + 1) % N + 1;
    }
    SDL_RenderGeometry(gR, nullptr, v, N + 1, idx, N * 3);
}
static void lockInit() {
    if (gLockTex) { SDL_DestroyTexture(gLockTex); gLockTex = nullptr; }
    gLockTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, UIH);
    if (!gLockTex) return;
    SDL_SetTextureBlendMode(gLockTex, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(gR, gLockTex);
    SDL_Surface* sf = IMG_Load((DIR_ + "lockscreen.jpg").c_str());
    if (!sf) sf = IMG_Load((DIR_ + "lockscreen.png").c_str());
    bool custom = false;
    if (sf) { // your own lock screen picture: put lockscreen.jpg or lockscreen.png in switch/WinNX/
        int sw = sf->w, sh = sf->h;
        SDL_Texture* t = SDL_CreateTextureFromSurface(gR, sf);
        SDL_FreeSurface(sf);
        if (t) {
            float k = std::max((float)W / sw, (float)UIH / sh);
            int dw = (int)(sw * k), dh = (int)(sh * k);
            SDL_Rect d = {(W - dw) / 2, (UIH - dh) / 2, dw, dh};
            SDL_RenderCopy(gR, t, nullptr, &d);
            SDL_DestroyTexture(t);
            custom = true;
        }
    }
    if (!custom) { // default: colourful sweeping bands like the Windows 8 lock screen
        SDL_SetRenderDrawColor(gR, 58, 58, 62, 255);
        SDL_RenderClear(gR);
        static const Col LP[] = {{190, 50, 40}, {222, 92, 30}, {238, 150, 30}, {244, 196, 50}, {236, 226, 196}, {208, 196, 170},
                                 {120, 170, 100}, {30, 120, 78}, {16, 92, 70}, {40, 112, 160}, {30, 70, 130}, {98, 60, 140},
                                 {190, 60, 100}, {210, 70, 50}, {236, 140, 40}, {240, 200, 60}, {230, 222, 190}, {70, 100, 40}};
        const int NP = (int)(sizeof(LP) / sizeof(LP[0]));
        float r = 2700;
        unsigned seed = 7;
        int i = 0;
        while (r > 950) {
            fdisc(1500, -420, r, mix(LP[i % NP], Col{36, 36, 40}, std::min(0.5f, std::max(0.0f, (r - 1500) / 1600.0f))));
            seed = seed * 1103515245u + 12345u;
            r -= 14 + (int)((seed >> 16) % 26);
            i++;
        }
        fdisc(1500, -420, r, Col{78, 72, 10});
    }
    // darken the bottom so the clock stays readable
    for (int k = 0; k < 40; k++) {
        float f = powf(k / 40.0f, 1.3f);
        rect(0, UIH - 420 + (int)(k * 10.5f), W, 11, BLACK, (int)(150 * f));
    }
    SDL_SetRenderTarget(gR, nullptr);
}
static void lockRebuild() { lockInit(); }
static SDL_Texture* gBootTex = nullptr;
static int gBootIdx = -1;
static void drawBoot(float t) {
    // plays the supplied boot GIF frame by frame (100 ms per frame), decoded on demand
    int idx = std::max(0, std::min(BOOT_NFRAMES - 1, (int)(t / 0.1f)));
    if (idx != gBootIdx) {
        int u = BOOT_FRAME_MAP[idx];
        SDL_Surface* sf = IMG_Load_RW(SDL_RWFromConstMem(BOOT_UNIQUE[u].d, (int)BOOT_UNIQUE[u].n), 1);
        if (sf) {
            if (gBootTex) SDL_DestroyTexture(gBootTex);
            gBootTex = SDL_CreateTextureFromSurface(gR, sf);
            SDL_FreeSurface(sf);
        }
        gBootIdx = idx;
    }
    rect(0, 0, W, H, BLACK);
    if (gBootTex) { SDL_Rect d = {0, 0, W, H}; SDL_RenderCopy(gR, gBootTex, nullptr, &d); }
}
static void drawLock(float off) {
    int o = (int)off;
    if (gLockTex) { SDL_Rect d = {0, o, W, UIH}; SDL_RenderCopy(gR, gLockTex, nullptr, &d); }
    else rect(0, o, W, UIH, Col{40, 40, 44});
    std::string tm = timeStr(false);
    text(tm, F130, 52, o + UIH - 340, BLACK, 0, 70);
    text(tm, F130, 50, o + UIH - 342, WHITE);
    std::string ds = dateStr();
    text(ds, F44, 56, o + UIH - 176, BLACK, 0, 70);
    text(ds, F44, 54, o + UIH - 178, WHITE);
    char bb[48];
    snprintf(bb, sizeof bb, "%s   %u%%", gUser.c_str(), (unsigned)gBat);
    drawAvatar(54, o + UIH - 118, 52);
    text(bb, F22, 118, o + UIH - 110, WHITE, 0, 230);
    if (!gUserStatus.empty()) text(gUserStatus, F16, 118, o + UIH - 80, WHITE, 0, 190);
    if (!gAuth) drawLockWidgets(o);
    float bounce = sinf(SDL_GetTicks() / 350.0f) * 5.0f;
    int hy = o + UIH - 62 + (int)bounce;
    line(W / 2 - 14, hy + 8, W / 2, hy - 4, 3, WHITE, 200);
    line(W / 2 + 14, hy + 8, W / 2, hy - 4, 3, WHITE, 200);
    text(gLockMethod ? "Swipe up to sign in" : "Swipe up to unlock", F16, W / 2, o + UIH - 40, WHITE, 1, 200);
    if (gAuth) drawAuthUI(true);
    if (o < 0) rect(0, o + UIH, W, 5, BLACK, 70);
}
static void unlockNow() {
    if (uiState != 1 || lockAnim || gAuth) return;
    if (gLockMethod != 0) { gAuth = true; gAuthFails = 0; lockOff = 0; authBegin(0, gLockMethod); return; }
    lockAnim = true; introT = 0;
}

// ---- search charm helpers
static std::vector<int> searchHits() {
    std::vector<int> v;
    std::string q = lower(gQuery);
    if (q.empty()) return v;
    for (int a = 0; a < A_COUNT; a++) if (appAvail(a) && lower(ADEF[a].name).find(q) != std::string::npos) v.push_back(a);
    return v;
}
static void gotoApp(int app) {
    closeOverlays();
    if (kbTarget == 6) kbOpen = false;
    if (!appAvail(app)) return;
    if (curApp == app) return;
    pendingApp = app;
    if (curApp >= 0) closeApp(false);
}
static void searchGo() {
    std::vector<int> h = searchHits();
    if (!h.empty()) { gotoApp(h[0]); return; }
    if (gQuery.find_first_not_of(' ') == std::string::npos) return;
    closeOverlays(); kbOpen = false;
    gUrl = gQuery;
    browserGo();
}

// ================================================================ notification center
struct Notif { int app; std::string title, body, when; bool unread; };
static std::vector<Notif> gNotifs;
static float notifP = 0; static int notifScroll = 0;
static Notif gBanner; static Uint32 gBannerT = 0; static bool gBannerOn = false;
static void notifSave() {
    std::string o;
    for (auto& n : gNotifs) {
        auto clean = [](std::string s) { for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' '; return s; };
        o += std::to_string(n.app) + "\t" + (n.unread ? "1" : "0") + "\t" + clean(n.when) + "\t" + clean(n.title) + "\t" + clean(n.body) + "\n";
    }
    writeFile(DIR_ + "notifs.txt", o);
}
static void notifLoad() {
    std::string d; gNotifs.clear();
    if (!readFile(DIR_ + "notifs.txt", d)) return;
    size_t p = 0;
    while (p < d.size() && gNotifs.size() < 60) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string l = d.substr(p, e - p); p = e + 1;
        std::vector<std::string> f; size_t q = 0;
        for (int i = 0; i < 4; i++) { size_t t = l.find('\t', q); if (t == std::string::npos) break; f.push_back(l.substr(q, t - q)); q = t + 1; }
        if (f.size() < 4) continue;
        f.push_back(l.substr(q));
        Notif n; n.app = atoi(f[0].c_str()); n.unread = f[1] == "1"; n.when = f[2]; n.title = f[3]; n.body = f[4];
        if (n.app >= 0 && n.app < A_COUNT) gNotifs.push_back(n);
    }
}
static int notifUnread() { int c = 0; for (auto& n : gNotifs) if (n.unread) c++; return c; }
static void notify(int app, const std::string& title, const std::string& body) {
    Notif n; n.app = app; n.title = title; n.body = body; n.unread = true; n.when = timeStr(true);
    gNotifs.insert(gNotifs.begin(), n);
    if (gNotifs.size() > 60) gNotifs.pop_back();
    notifSave();
    if (gNotif) sndPlay(SND_NOTIFY);
    if (gNotif && uiState == 2) { gBanner = n; gBannerT = SDL_GetTicks(); gBannerOn = true; }
}
static SDL_Rect bannerRect() {
    float age = (SDL_GetTicks() - gBannerT) / 1000.0f;
    float slide = age < 0.25f ? 1.0f - age / 0.25f : (age > 4.6f ? (age - 4.6f) / 0.4f : 0.0f);
    return SDL_Rect{W - 420 + (int)(slide * 440), 16, 400, 88};
}
static bool bannerHit(int x, int y) {
    if (!gBannerOn || notifOpen || charmsOpen) return false;
    SDL_Rect r = bannerRect();
    return SDL_GetTicks() - gBannerT < 5000 && inR(x, y, r.x, r.y, r.w, r.h);
}
static void drawBanner() {
    if (!gBannerOn) return;
    if (SDL_GetTicks() - gBannerT > 5000 || notifOpen) { gBannerOn = false; return; }
    SDL_Rect r = bannerRect();
    rect(r.x, r.y, r.w, r.h, Col{20, 20, 24}, 250);
    rect(r.x, r.y, 8, r.h, ADEF[gBanner.app].col);
    text(appName(gBanner.app), F16, r.x + 24, r.y + 8, WHITE, 0, 150);
    std::string t = gBanner.title; while (t.size() > 3 && tw(t, F22) > 350) t.pop_back();
    text(t, F22, r.x + 24, r.y + 28, WHITE);
    std::string b = gBanner.body; while (b.size() > 3 && tw(b, F16) > 350) b.pop_back();
    text(b, F16, r.x + 24, r.y + 60, WHITE, 0, 190);
}
static const int NP_W = 420, NP_ROW = 96;
static void bellIcon(int cx, int cy, Col c, Col bg) {
    disc(cx, cy - 6, 14, c); rect(cx - 14, cy - 6, 28, 18, c); rect(cx - 20, cy + 10, 40, 5, c); disc(cx, cy + 20, 5, c);
    rect(cx - 2, cy - 24, 4, 8, c);
}
static void drawNotifPanel() {
    if (notifP <= 0.01f) return;
    float e = ease(notifP);
    int px = W - NP_W + (int)((1.0f - e) * NP_W);
    rect(px, 0, NP_W, UIH, Col{22, 22, 26}, 252);
    text("Notifications", F30, px + 24, 16, WHITE);
    btn(px + NP_W - 130, 20, 106, 38, "Clear all", Col{70, 70, 78}, WHITE);
    // quick actions
    struct Q { const char* l; bool on; } q[4] = {{"Offline", gOffline}, {"Alerts", gNotif}, {"Burn-in", gBurn}, {"Settings", false}};
    for (int i = 0; i < 4; i++) {
        int x = px + 24 + i * 94;
        rect(x, 76, 86, 62, q[i].on ? Col{0, 120, 215} : Col{58, 58, 66});
        text(q[i].l, F16, x + 43, 96, WHITE, 1);
    }
    int top = 156;
    SDL_Rect clip = {px, top, NP_W, UIH - top};
    SDL_RenderSetClipRect(gR, &clip);
    int n = (int)gNotifs.size();
    int maxS = std::max(0, n * NP_ROW - (UIH - top));
    notifScroll = std::max(0, std::min(notifScroll, maxS));
    for (int i = 0; i < n; i++) {
        int y = top + i * NP_ROW - notifScroll;
        if (y + NP_ROW < top || y > UIH) continue;
        const Notif& nn = gNotifs[i];
        rect(px + 16, y + 4, NP_W - 32, NP_ROW - 8, Col{40, 40, 48});
        rect(px + 16, y + 4, 8, NP_ROW - 8, ADEF[nn.app].col);
        text(appName(nn.app) + (nn.unread ? "  \xE2\x80\xA2" : ""), F16, px + 38, y + 10, WHITE, 0, 150);
        text(nn.when, F16, px + NP_W - 70, y + 10, WHITE, 2, 130);
        std::string t = nn.title; while (t.size() > 3 && tw(t, F22) > NP_W - 110) t.pop_back();
        text(t, F22, px + 38, y + 30, WHITE);
        std::string b = nn.body; while (b.size() > 3 && tw(b, F16) > NP_W - 110) b.pop_back();
        text(b, F16, px + 38, y + 62, WHITE, 0, 190);
        text("x", F22, px + NP_W - 36, y + 30, WHITE, 1, 160);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (n == 0) text("No new notifications", F22, px + NP_W / 2, 300, WHITE, 1, 150);
}
static void notifTap(int x, int y) {
    int px = W - NP_W;
    if (x < px) { notifOpen = false; return; }
    if (inR(x, y, px + NP_W - 130, 20, 106, 38)) { gNotifs.clear(); notifSave(); return; }
    if (y >= 76 && y < 138) {
        int i = (x - px - 24) / 94;
        if (i >= 0 && i < 4 && (x - px - 24) % 94 < 86) {
            if (i == 0) { gOffline = !gOffline; saveCfg(); }
            else if (i == 1) { gNotif = !gNotif; saveCfg(); }
            else if (i == 2) { gBurn = !gBurn; saveCfg(); }
            else { notifOpen = false; pendingApp = A_SETTINGS; if (curApp >= 0 && curApp != A_SETTINGS) closeApp(false); }
        }
        return;
    }
    int top = 156;
    if (y < top) return;
    int i = (y - top + notifScroll) / NP_ROW;
    if (i < 0 || i >= (int)gNotifs.size()) return;
    if (x > px + NP_W - 64) { gNotifs.erase(gNotifs.begin() + i); notifSave(); return; }
    int a = gNotifs[i].app;
    gNotifs.erase(gNotifs.begin() + i); notifSave();
    notifOpen = false;
    if (a != curApp) { pendingApp = a; if (curApp >= 0) closeApp(false); }
}

static const int CH_X = W - 120;
static void drawCharms() {
    if (charmsP <= 0.01f) return;
    float e = ease(charmsP);
    int off = (int)((1.0f - e) * 120);
    Col pb = {14, 14, 16};
    rect(CH_X + off, 0, 120, UIH, pb, 245);
    static const char* CN[5] = {"Search", "Alerts", "Start", "Devices", "Settings"};
    for (int i = 0; i < 5; i++) {
        int cy = 90 + i * 110, cx = CH_X + off + 60;
        Col c = WHITE;
        switch (i) {
            case 0: ring(cx - 6, cy - 6, 18, 4, c, pb); line(cx + 7, cy + 7, cx + 20, cy + 20, 5, c); break;
            case 1: bellIcon(cx, cy, c, pb);
                    if (notifUnread() > 0) { disc(cx + 18, cy - 18, 12, Col{209, 52, 56}); text(std::to_string(std::min(9, notifUnread())), F16, cx + 18, cy - 28, WHITE, 1); }
                    break;
            case 2: winLogo(cx, cy, 44, c); break;
            case 3: outline(cx - 22, cy - 14, 32, 22, 3, c); outline(cx - 4, cy - 2, 28, 18, 3, c); break;
            default: ring(cx, cy, 18, 6, c, pb);
                     for (int k = 0; k < 8; k++) { float a = k * 0.785398f; disc(cx + (int)(sinf(a) * 21), cy - (int)(cosf(a) * 21), 4, c); }
                     disc(cx, cy, 8, pb); break;
        }
        text(CN[i], F16, cx, cy + 34, WHITE, 1);
    }
    int lx = (int)((1.0f - e) * -560);
    rect(30 + lx, UIH - 250, 540, 210, pb, 240);
    text(timeStr(true), F72, 56 + lx, UIH - 240, WHITE);
    text(dateStr(), F30, 56 + lx, UIH - 140, WHITE);
    char bb[32]; snprintf(bb, sizeof bb, "Battery %u%%", (unsigned)gBat);
    text(bb, F22, 56 + lx, UIH - 94, WHITE, 0, 200);
}
static void drawSearchCharm() {
    if (searchP <= 0.01f) return;
    float e = ease(searchP);
    int px = W - 440 + (int)((1.0f - e) * 440);
    rect(px, 0, 440, UIH, Col{24, 24, 28}, 250);
    text("Search", F44, px + 24, 20, WHITE);
    rect(px + 20, 92, 400, 52, WHITE);
    std::string sh = gQuery;
    if (kbOpen && kbTarget == 6 && (SDL_GetTicks() / 500) % 2 == 0) sh += "|";
    if (gQuery.empty() && !(kbOpen && kbTarget == 6)) text("Apps", F22, px + 34, 104, Col{150, 150, 150});
    else {
        while (sh.size() > 2 && tw(sh, F22) > 370) sh.erase(0, 1);
        text(sh, F22, px + 34, 104, DARK);
    }
    int bottom = kbOpen ? KBTOP : UIH;
    int maxRows = std::max(1, (bottom - 170) / 64);
    std::vector<int> h = searchHits();
    int row = 0;
    for (int i = 0; i < (int)h.size() && row < maxRows - 1; i++, row++) {
        int y = 164 + row * 64;
        rect(px + 20, y, 400, 58, Col{44, 44, 52});
        rect(px + 20, y, 58, 58, ADEF[h[i]].col);
        text(ADEF[h[i]].name, F22, px + 92, y + 14, WHITE);
    }
    if (!gQuery.empty() && row < maxRows) {
        int y = 164 + row * 64;
        rect(px + 20, y, 400, 58, Col{44, 44, 52});
        rect(px + 20, y, 58, 58, Col{219, 68, 55});
        std::string q = gQuery;
        while (q.size() > 3 && tw(q, F16) > 250) q.pop_back();
        text("Search web: " + q, F16, px + 92, y + 20, WHITE);
    }
    if (gQuery.empty()) text("Type the name of an app", F16, px + 24, 170, WHITE, 0, 150);
}
static int swTileAt(int y) {
    int yy = 164;
    for (int a = 0; a < A_COUNT && yy < UIH - 130; a++) {
        if (!running[a]) continue;
        if (y >= yy && y < yy + 100) return a;
        yy += 140;
    }
    return -1;
}
static void drawSwitcher() {
    if (switchP <= 0.01f) return;
    float e = ease(switchP);
    int px = -220 + (int)(e * 220);
    rect(px, 0, 220, UIH, Col{18, 18, 22}, 248);
    const Theme& th = THEMES[gTheme];
    int y = 24;
    rect(px + 20, y, 180, 100, mix(th.top, th.bottom, 0.4f));
    winLogo(px + 110, y + 50, 40, WHITE);
    text("Start", F16, px + 20, y + 104, WHITE);
    y += 140;
    for (int a = 0; a < A_COUNT && y < UIH - 130; a++) {
        if (!running[a]) continue;
        rect(px + 20, y, 180, 100, appBg(a));
        if (a == curApp) outline(px + 20, y, 180, 100, 3, WHITE);
        text(appName(a), F16, px + 20, y + 104, WHITE);
        if (curApp >= 0 && a != curApp) { rect(px + 130, y + 68, 70, 32, Col{0, 0, 0}, 150); text("Snap", F16, px + 165, y + 72, WHITE, 1); }
        y += 140;
    }
    if (swDragApp >= 0 && swDragX > 0) {
        if (curApp >= 0 && swDragApp != curApp) { rect(W / 2, 0, W / 2, UIH, WHITE, 30); text("Drop here to snap", F22, W * 3 / 4, UIH / 2 - 14, WHITE, 1); }
        rect(swDragX - 90, swDragY - 50, 180, 100, appBg(swDragApp), 235); outline(swDragX - 90, swDragY - 50, 180, 100, 3, WHITE);
        text(appName(swDragApp), F16, swDragX - 80, swDragY + 22, WHITE);
    }
}
static void overlayTap(int x, int y) {
    if (gEgg) { eggTap(); return; }
    if (clkRinging()) { clkOverlayTap(x, y); return; }
    if (searchOpen) {
        if (x < W - 440) { closeOverlays(); if (kbTarget == 6) kbOpen = false; return; }
        if (inR(x, y, W - 420, 92, 400, 52)) { openKb(6); return; }
        std::vector<int> h = searchHits();
        int bottom = kbOpen ? KBTOP : UIH;
        int maxRows = std::max(1, (bottom - 170) / 64);
        int row = (y - 164) / 64;
        if (y >= 164 && row >= 0) {
            if (row < (int)h.size() && row < maxRows - 1) gotoApp(h[row]);
            else if (row == std::min((int)h.size(), maxRows - 1) && !gQuery.empty()) {
                closeOverlays(); kbOpen = false; gUrl = gQuery; browserGo();
            }
        }
        return;
    }
    if (bannerHit(x, y) && !overlayOpen()) { int a = gBanner.app; gBannerOn = false; if (a != curApp) { pendingApp = a; if (curApp >= 0) closeApp(false); } return; }
    if (notifOpen) { notifTap(x, y); return; }
    if (charmsOpen) {
        if (x < CH_X) { closeOverlays(); return; }
        int i = (y - 35) / 110;
        if (y < 35 || i < 0 || i > 4) return;
        switch (i) {
            case 0: charmsOpen = false; searchOpen = true; gQuery.clear(); openKb(6); break;
            case 1: charmsOpen = false; notifOpen = true; notifScroll = 0; for (auto& n : gNotifs) n.unread = false; notifSave(); break;
            case 2: closeOverlays(); if (curApp >= 0) closeApp(false); break;
            case 3: toast("Pair controllers in the Switch Controllers menu"); break;
            default: {
                closeOverlays();
                if (curApp != A_SETTINGS) { pendingApp = A_SETTINGS; if (curApp >= 0) closeApp(false); }
            }
        }
        return;
    }
    if (switchOpen) {
        if (x >= 220) { closeOverlays(); return; }
        if (y >= 24 && y < 124) { closeOverlays(); if (curApp >= 0) closeApp(false); return; }
        int yy = 164;
        for (int a = 0; a < A_COUNT && yy < UIH - 130; a++) {
            if (!running[a]) continue;
            if (y >= yy && y < yy + 100) {
                if (curApp >= 0 && a != curApp && x >= 150 && y >= yy + 68) { closeOverlays(); gSnap = a; return; }
                closeOverlays();
                if (a != curApp) { pendingApp = a; if (curApp >= 0) closeApp(false); }
                return;
            }
            yy += 140;
        }
    }
}
static void drawToast() {
    if (gToastS.empty() || SDL_GetTicks() - gToastT > 2600) return;
    int w2 = tw(gToastS, F22) + 48;
    rect((W - w2) / 2, UIH - 110, w2, 52, Col{20, 20, 22}, 240);
    text(gToastS, F22, W / 2, UIH - 99, WHITE, 1);
}

// ---------------------------------------------------------------- main
// ================================================================ crash safety and the blue screen
static float gBsodT = 0; static std::string gBsodCode, gBsodDetail; static int gBsodApp = -1;
static int gCrashApp = -1;               // app that was open during the last hard crash (its live tile stays off)
static char gSelfPath[300] = "";
static void bsodNow(const std::string& code, const std::string& detail) {
    gBsodCode = code; gBsodDetail = detail; gBsodApp = curApp; gBsodT = 0; sndPlay(SND_ERROR);
    std::string log;
    readFile(DIR_ + "crash_log.txt", log);
    if (log.size() > 6000) log = log.substr(log.size() - 3000);
    log += "[" + std::to_string(gNow.y) + "-" + std::to_string(gNow.mo) + "-" + std::to_string(gNow.d) + "] " + code + " app=" + std::to_string(curApp) + " " + detail + "\n";
    writeFile(DIR_ + "crash_log.txt", log);
    if (curApp >= 0) { gCrashApp = curApp; }
    closeOverlays(); kbOpen = false; searchOpen = false;
    curApp = -1; appDir = 0; appT = 0; pendingApp = -1;
    for (int a = 0; a < A_COUNT; a++) running[a] = false;
    uiState = 3;
}
static void drawBsod() {
    rect(0, 0, W, H, Col{16, 114, 198});
    text(":(", F130, 120, 70, WHITE);
    text("Your PC ran into a problem and needs to restart.", F44, 120, 250, WHITE);
    text("We're just collecting some error info, and then we'll restart for you.", F30, 120, 320, WHITE);
    int pct = std::min(100, (int)(gBsodT * 18));
    text(std::to_string(pct) + "% complete", F30, 120, 400, WHITE);
    std::string nm = gBsodApp >= 0 ? appName(gBsodApp) : "Windows";
    text("If you call a support person, give them this info:", F22, 120, 520, WHITE);
    text("Stop code: " + gBsodCode, F22, 120, 552, WHITE);
    text("What failed: " + nm + (gBsodDetail.empty() ? "" : "  (" + gBsodDetail + ")"), F22, 120, 584, WHITE);
    if (pct >= 100) text("Tap the screen or press A to restart", F30, 120, 640, WHITE, 0, (int)(155 + 100 * sinf(gBsodT * 4)));
}
static void bsodLeave() {
    if (gBsodT < 2.0f) return;
    remove((DIR_ + "crash.txt").c_str());
    uiState = gLockScr ? 1 : 2; lockOff = 0; lockAnim = false; introT = 0;
}
#ifndef WNX_HOST
// A real crash (data abort etc.): write a note, then chain-load ourselves again; the next start shows the blue screen.
extern "C" void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    char b[200];
    snprintf(b, sizeof b, "%u\n%llx\n%d\n", (unsigned)ctx->error_desc, (unsigned long long)ctx->pc.x, curApp);
    FILE* f = fopen("sdmc:/switch/WinNX/crash.txt", "wb");
    if (f) { fwrite(b, 1, strlen(b), f); fclose(f); }
    if (gSelfPath[0]) envSetNextLoad(gSelfPath, gSelfPath);
    svcExitProcess();
}
#endif
static void crashCheckOnBoot() {
    std::string c;
    if (!readFile(DIR_ + "crash.txt", c)) return;
    unsigned code = 0; unsigned long long pc = 0; int app = -1;
    sscanf(c.c_str(), "%u %llx %d", &code, &pc, &app);
    char b[120]; snprintf(b, sizeof b, "0x%X", code);
    gCrashApp = app;
    gBsodCode = std::string("SYSTEM_THREAD_EXCEPTION_NOT_HANDLED ") + b;
    char d[60]; snprintf(d, sizeof d, "at 0x%llX", pc);
    gBsodDetail = d; gBsodApp = app; gBsodT = 0;
    uiState = 3;
}

static bool appBack() {
    switch (curApp) {
        case A_MUSIC: return musBack();
        case A_CALENDAR: return calBack();
        case A_FILES: return filesBack();
        case A_SETTINGS: if (gSetup) { authCancel(); return true; } return false;
        case A_PODCASTS: return podcastsBack();
        case A_DESKTOP: return desktopBack();
        case A_GAMES: return gamesBack();
        case A_CALC: return calcBack();
        case A_CLOCK: return clkBack();
        case A_RECORDER: return recBack();
        case A_FOOD: return foodBack();
        case A_HEALTH: return hlBack();
        case A_PET: return petBack();
        case A_CITY: return cityBack();
        case A_SKYPE: return skypeBack();
        case A_ONEDRIVE: return oneDriveBack();
        // @@BACK@@
        default: return false;
    }
}
static void appPinch(float sc, int cx, int cy, bool begin) {
    switch (curApp) {
        case A_PHOTOS: photosPinch(sc, cx, cy, begin); break;
        case A_MAPS: mapsPinch(sc, cx, cy, begin); break;
        // @@PINCH@@
        default: break;
    }
}
// ================================================================ Windows Phone mode (portrait, rotated display)
static SDL_Texture* gPhoneT = nullptr;
static const int PW = 720, PH = 1280, PSB = 40, PNAV = 72, PKBH = 340, PP = 170;
static const float PSC = 720.0f / 1280.0f;
struct PTile { int idx; SDL_Rect r; };
static std::vector<PTile> pnTiles; static std::vector<std::pair<int, std::string>> pnHeads; static int pnContentH = 0, pnAllY = 0;
static float pnScroll = 0, pnVel = 0; static int pnOverlay = 0, pnOvScroll = 0, pnOwner = 0, pnDX = 0, pnDY = 0, pnLX = 0, pnLY = 0, pnNav = -1, pnPress = -1;
static bool pnMoved = false, pnLong = false, pnFromBar = false; static Uint32 pnT = 0;
static void phoneFromRaw(int sx, int sy, int& px, int& py) {
    if (!gPhoneFlip) { px = sy; py = 1280 - sx; } else { px = 720 - sy; py = sx; }
    px = std::max(0, std::min(PW - 1, px)); py = std::max(0, std::min(PH - 1, py));
}
static SDL_Rect pnAppDst() { int h = (int)((kbOpen ? KBTOP : UIH) * PSC); int y = kbOpen ? PSB + 6 : PSB + (PH - PSB - PNAV - (int)(UIH * PSC)) / 2; return SDL_Rect{0, y, PW, h}; }
static SDL_Rect pnKbDst() { return SDL_Rect{0, PH - PNAV - PKBH, PW, PKBH}; }
static void phoneMapToApp(int px, int py, int& ax, int& ay) {
    ax = std::max(0, std::min(W - 1, (int)(px / PSC)));
    SDL_Rect k = pnKbDst(), a = pnAppDst();
    if (kbOpen && py >= k.y - 4) ay = KBTOP + (py - k.y) * (UIH - KBTOP) / k.h;
    else ay = (int)((py - a.y) / PSC);
    ay = std::max(2, std::min(UIH - 10, ay));
}
static int phoneOwnerAt(int px, int py) {
    if (curApp < 0 || pnOverlay) return 1;
    if (py >= PH - PNAV || py < PSB) return 1;
    SDL_Rect k = pnKbDst();
    if (kbOpen && py >= k.y) return 2;
    if (curApp == A_CORTANA) return 1;
    SDL_Rect a = pnAppDst(); if (py >= a.y && py < a.y + a.h) return 2;
    return 1;
}
static void pnLayout() {
    pnTiles.clear(); pnHeads.clear(); int ng = 0;
    for (auto& t : gTiles) ng = std::max(ng, t.c / GCOLS + 1);
    int y = 18;
    for (int g = 0; g < ng; g++) {
        int maxr = 0; bool any = false;
        for (auto& t : gTiles) if (t.c / GCOLS == g) { any = true; maxr = std::max(maxr, t.r + t.h); }
        if (!any) continue;
        std::string name = g < (int)gGroups.size() ? gGroups[g] : "";
        if (!name.empty()) { pnHeads.push_back({y, name}); y += 46; }
        for (int i = 0; i < (int)gTiles.size(); i++) { const Tile& t = gTiles[i]; if (t.c / GCOLS != g) continue; pnTiles.push_back({i, SDL_Rect{20 + (t.c % GCOLS) * PP, y + t.r * PP, t.w * PP - 8, t.h * PP - 8}}); }
        y += maxr * PP + 24;
    }
    pnAllY = y; pnContentH = y + 110;
}
static int pnUnread() { int n = 0; for (auto& x : gNotifs) if (x.unread) n++; return n; }
static void pnStatusBar() {
    rect(0, 0, PW, PSB, BLACK, 235);
    text(timeStr(true), F16, 16, 10, WHITE);
    char bb[16]; snprintf(bb, sizeof bb, "%u%%", (unsigned)gBat);
    int bw = tw(bb, F16);
    text(bb, F16, PW - 16 - bw, 10, WHITE); drawBattery(PW - 16 - bw - 38, 12, (int)gBat, WHITE, 230);
    drawWifiIcon(PW - 16 - bw - 38 - 34, 28, gWifiBars, WHITE, 230);
    if (pnUnread()) disc(PW / 2, 20, 6, Col{230, 60, 60});
}
static void pnNavBar() {
    rect(0, PH - PNAV, PW, PNAV, BLACK);
    int cy = PH - PNAV / 2;
    line(100, cy, 150, cy, 4, WHITE, pnNav == 0 ? 255 : 190); line(100, cy, 118, cy - 16, 4, WHITE, pnNav == 0 ? 255 : 190); line(100, cy, 118, cy + 16, 4, WHITE, pnNav == 0 ? 255 : 190);
    winLogo(PW / 2, cy, 26, WHITE, pnNav == 1 ? 255 : 200);
    ringA(PW - 125.0f, (float)cy - 2, 14, 3.5f, WHITE, pnNav == 2 ? 255 : 190); line(PW - 115, cy + 8, PW - 104, cy + 20, 4, WHITE, pnNav == 2 ? 255 : 190);
}
static void drawPhoneStart() {
    const Theme& th = THEMES[gTheme];
    thGrad(0, 0, PW, PH, th.top, th.bottom);
    disc(110, 300, 200, WHITE, 10); disc(640, 760, 260, WHITE, 8);
    pnLayout();
    int view = PH - PSB - PNAV, maxS = std::max(0, pnContentH - view);
    pnScroll = std::max(0.0f, std::min(pnScroll, (float)maxS));
    int oy = PSB - (int)pnScroll;
    for (auto& h : pnHeads) text(h.second, F22, 22, h.first + oy, WHITE, 0, 210);
    for (auto& t : pnTiles) {
        SDL_Rect r = t.r; r.y += oy;
        if (r.y > PH || r.y + r.h < PSB) continue;
        drawTileRect(gTiles[t.idx], r, pnPress == t.idx ? 1.0f : 0.0f, false, false, false);
    }
    int by = pnAllY + oy;
    rect(20, by, PW - 40, 70, WHITE, 40); text("All apps", F22, 40, by + 18, WHITE); line(PW - 70, by + 22, PW - 52, by + 35, 4, WHITE); line(PW - 52, by + 35, PW - 70, by + 48, 4, WHITE);
}
static void pnOverlayDraw() {
    if (!pnOverlay) return;
    rect(0, PSB, PW, PH - PSB - PNAV, Col{16, 16, 22}, 255);
    int top = PSB + 14;
    if (pnOverlay == 1) {
        text("Notifications", F30, 24, top, WHITE); btn(PW - 190, top - 4, 170, 46, "Clear all", Col{60, 60, 72}, WHITE);
        struct Q { const char* l; bool on; } q[5] = {{"Offline", gOffline}, {"Alerts", gNotif}, {"Burn-in", gBurn}, {"Flip", gPhoneFlip}, {"Landscape", false}};
        for (int i = 0; i < 5; i++) btn(18 + i * 136, top + 58, 128, 52, q[i].l, q[i].on ? THEMES[gTheme].accent : Col{56, 58, 70}, WHITE);
        int y = top + 130 - pnOvScroll;
        if (gNotifs.empty()) text("No new notifications", F22, PW / 2, 420, Col{150, 150, 160}, 1);
        for (auto& n : gNotifs) {
            if (y > PSB - 120 && y < PH) { rect(18, y, PW - 36, 112, Col{36, 38, 50}); rect(18, y, 8, 112, ADEF[n.app].col); text(n.title, F22, 44, y + 10, WHITE); int by2 = y + 44; for (auto& l : wrapText(n.body, F16, PW - 120)) { if (by2 > y + 90) break; text(l, F16, 44, by2, Col{190, 190, 200}); by2 += 22; } text(n.when, F16, PW - 36, y + 12, Col{150, 150, 160}, 2); }
            y += 122;
        }
    } else if (pnOverlay == 2) {
        text("Apps", F30, 24, top, WHITE);
        int i = 0; auto card = [&](int app, const std::string& name) {
            int cx = 18 + (i % 2) * 346, cy = top + 64 + (i / 2) * 250 - pnOvScroll; i++;
            if (cy > PH || cy + 230 < PSB) return;
            Col c = app < 0 ? THEMES[gTheme].accent : appBg(app);
            rect(cx, cy, 330, 230, c); drawIcon(app < 0 ? A_SETTINGS : app, cx + 165, cy + 100, c); if (app < 0) { rect(cx + 120, cy + 56, 90, 90, c); winLogo(cx + 165, cy + 100, 44, WHITE); }
            text(name, F22, cx + 14, cy + 190, WHITE);
            if (app >= 0) { rect(cx + 330 - 56, cy, 56, 56, BLACK, 90); line(cx + 330 - 40, cy + 16, cx + 330 - 16, cy + 40, 3, WHITE); line(cx + 330 - 40, cy + 40, cx + 330 - 16, cy + 16, 3, WHITE); }
        };
        card(-1, "Start");
        for (int a = 0; a < A_COUNT; a++) if (running[a]) card(a, appName(a));
    } else if (pnOverlay == 3) {
        text("All apps", F30, 24, top, WHITE);
        int y = top + 64 - pnOvScroll;
        for (int a = 0; a < A_COUNT; a++) {
            if (!appAvail(a) || a == A_DESKTOP) continue;
            if (y > PSB - 90 && y < PH) { rect(18, y, 84, 84, appBg(a)); drawIcon(a, 60, y + 42, appBg(a)); text(appName(a), F22, 120, y + 26, WHITE); }
            y += 94;
        }
    }
}
static int pnOvContentH() {
    if (pnOverlay == 1) return 140 + (int)gNotifs.size() * 122;
    if (pnOverlay == 2) { int n = 1; for (int a = 0; a < A_COUNT; a++) if (running[a]) n++; return 70 + ((n + 1) / 2) * 250; }
    int n = 0; for (int a = 0; a < A_COUNT; a++) if (appAvail(a) && a != A_DESKTOP) n++; return 70 + n * 94;
}
static void phoneCompose() {
    if (!gPhoneT) gPhoneT = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, PW, PH);
    if (!gPhoneT) { fprintf(stderr, "phone tex: %s\n", SDL_GetError()); return; }
    SDL_SetRenderTarget(gR, gPhoneT); SDL_RenderSetClipRect(gR, nullptr);
    SDL_SetRenderDrawColor(gR, 0, 0, 0, 255); SDL_RenderClear(gR);
    if (curApp < 0) drawPhoneStart();
    else if (curApp == A_CORTANA) {
        int chh = (kbOpen ? pnKbDst().y : PH - PNAV) - PSB; SDL_Rect vp{0, PSB, PW, chh};
        SDL_RenderSetViewport(gR, &vp); drawCortanaAt(PW, chh); SDL_RenderSetViewport(gR, nullptr);
        if (kbOpen) { SDL_Rect s{0, KBTOP, W, UIH - KBTOP}, d = pnKbDst(); SDL_RenderCopy(gR, gTarget, &s, &d); }
    } else {
        Col b = appBg(curApp); rect(0, PSB, PW, PH - PSB - PNAV, mix(b, BLACK, 0.45f));
        SDL_Rect d = pnAppDst(), s{0, 0, W, kbOpen ? KBTOP : UIH};
        SDL_RenderCopy(gR, gTarget, &s, &d);
        if (kbOpen) { SDL_Rect s2{0, KBTOP, W, UIH - KBTOP}, d2 = pnKbDst(); SDL_RenderCopy(gR, gTarget, &s2, &d2); }
    }
    pnOverlayDraw();
    pnStatusBar(); pnNavBar();
    if (!gToastS.empty() && SDL_GetTicks() - gToastT < 2200) { int w = tw(gToastS, F22) + 40; rect((PW - w) / 2, PH - PNAV - 120, w, 48, BLACK, 220); text(gToastS, F22, PW / 2, PH - PNAV - 112, WHITE, 1); }
    if (dimA > 1) rect(0, 0, PW, PH, BLACK, (int)dimA);
    SDL_SetRenderTarget(gR, nullptr);
    SDL_SetRenderDrawColor(gR, 0, 0, 0, 255); SDL_RenderClear(gR);
    SDL_Rect dst{(W - PW) / 2, (H - PH) / 2, PW, PH};
    if (SDL_RenderCopyEx(gR, gPhoneT, nullptr, &dst, gPhoneFlip ? 270.0 : 90.0, nullptr, SDL_FLIP_NONE) != 0) { static int e = 0; if (e++ < 3) fprintf(stderr, "copyex: %s\n", SDL_GetError()); }
}
static void pnNavTap(int b) {
    if (b == 0) {
        if (pnOverlay) pnOverlay = 0;
        else if (kbOpen) hideKb();
        else if (curApp >= 0) { if (!appBack()) closeApp(false); }
    } else if (b == 1) {
        pnOverlay = 0; if (kbOpen) hideKb();
        if (curApp >= 0) closeApp(false); else pnScroll = 0;
    } else if (b == 2) {
        pnOverlay = 0; if (kbOpen) hideKb();
        if (curApp == A_CORTANA) closeApp(false); else gotoApp(A_CORTANA);
    }
}
static void pnOverlayTap(int x, int y) {
    int top = PSB + 14;
    if (pnOverlay == 1) {
        if (inR(x, y, PW - 190, top - 4, 170, 46)) { gNotifs.clear(); notifSave(); return; }
        for (int i = 0; i < 5; i++) if (inR(x, y, 18 + i * 136, top + 58, 128, 52)) {
            if (i == 0) gOffline = !gOffline; else if (i == 1) gNotif = !gNotif; else if (i == 2) gBurn = !gBurn; else if (i == 3) gPhoneFlip = !gPhoneFlip; else { gPhone = false; pnOverlay = 0; }
            saveCfg(); return;
        }
        int yy = top + 130 - pnOvScroll;
        for (auto& n : gNotifs) { if (inR(x, y, 18, yy, PW - 36, 112)) { int a = n.app; pnOverlay = 0; gotoApp(a); return; } yy += 122; }
    } else if (pnOverlay == 2) {
        int i = 0; auto hit = [&](int app) -> bool {
            int cx = 18 + (i % 2) * 346, cy = top + 64 + (i / 2) * 250 - pnOvScroll; i++;
            if (!inR(x, y, cx, cy, 330, 230)) return false;
            pnOverlay = 0;
            if (app < 0) { if (curApp >= 0) closeApp(false); }
            else if (x >= cx + 330 - 56 && y < cy + 56) { if (app == curApp) closeApp(true); else appReset(app); pnOverlay = 2; }
            else gotoApp(app);
            return true;
        };
        if (hit(-1)) return;
        for (int a = 0; a < A_COUNT; a++) if (running[a]) if (hit(a)) return;
    } else if (pnOverlay == 3) {
        int yy = top + 64 - pnOvScroll;
        for (int a = 0; a < A_COUNT; a++) { if (!appAvail(a) || a == A_DESKTOP) continue; if (inR(x, y, 18, yy, PW - 36, 84)) { pnOverlay = 0; gotoApp(a); return; } yy += 94; }
    }
}
static void pnTouch(int ph, int px, int py) {
    Uint32 now = SDL_GetTicks();
    if (ph == 0) { pnDX = pnLX = px; pnDY = pnLY = py; pnMoved = false; pnLong = false; pnT = now; pnVel = 0; pnNav = -1; pnPress = -1; pnFromBar = py < PSB + 6;
        if (pnOwner == 1 && py >= PH - PNAV) pnNav = px < PW / 3 ? 0 : (px < PW * 2 / 3 ? 1 : 2);
        if (pnOwner == 1 && !pnOverlay && curApp < 0 && pnNav < 0) { int oy = PSB - (int)pnScroll; for (auto& t : pnTiles) if (inR(px, py, t.r.x, t.r.y + oy, t.r.w, t.r.h)) pnPress = t.idx; }
        return; }
    if (pnOwner != 1) return;
    if (ph == 1) {
        int dx = px - pnLX, dy = py - pnLY; pnLX = px; pnLY = py;
        if (!pnMoved && (abs(px - pnDX) > 16 || abs(py - pnDY) > 16)) { pnMoved = true; pnPress = -1; }
        if (pnMoved && pnNav < 0) {
            if (pnFromBar && !pnOverlay && py - pnDY > 70) { pnOverlay = 1; pnOvScroll = 0; for (auto& n : gNotifs) n.unread = false; notifSave(); pnFromBar = false; }
            else if (pnOverlay) { pnOvScroll = std::max(0, std::min(pnOvScroll - dy, std::max(0, pnOvContentH() - (PH - PSB - PNAV)))); if (pnDY > PH - PNAV - 120 && pnDY - py > 100 && false) pnOverlay = 0; }
            else if (curApp < 0) { pnScroll -= dy; pnVel = (float)-dy; }
            else if (curApp == A_CORTANA) cortanaDrag(0, dy);
        }
        return;
    }
    // release
    if (pnNav >= 0) { int b = px < PW / 3 ? 0 : (px < PW * 2 / 3 ? 1 : 2); if (!pnLong && !pnMoved && b == pnNav) pnNavTap(b); pnNav = -1; return; }
    if (pnMoved) { pnPress = -1; return; }
    if (pnFromBar && py < PSB + 30) { pnOverlay = pnOverlay == 1 ? 0 : 1; pnOvScroll = 0; if (pnOverlay == 1) { for (auto& n : gNotifs) n.unread = false; notifSave(); } return; }
    if (pnOverlay) { if (py >= PSB && py < PH - PNAV) pnOverlayTap(px, py); return; }
    if (curApp < 0) {
        int oy = PSB - (int)pnScroll;
        for (auto& t : pnTiles) if (inR(px, py, t.r.x, t.r.y + oy, t.r.w, t.r.h)) { int a = gTiles[t.idx].app; pnPress = -1; gotoApp(a); return; }
        if (inR(px, py, 20, pnAllY + oy, PW - 40, 70)) { pnOverlay = 3; pnOvScroll = 0; }
        pnPress = -1; return;
    }
    if (curApp == A_CORTANA) { int chh = (kbOpen ? pnKbDst().y : PH - PNAV) - PSB; cortanaTapAt(px, py - PSB, PW, chh); }
}
static void pnUpdate(bool down) {
    Uint32 now = SDL_GetTicks();
    if (down && pnOwner == 1 && pnNav == 0 && !pnMoved && !pnLong && now - pnT > 650) { pnLong = true; pnOverlay = pnOverlay == 2 ? 0 : 2; pnOvScroll = 0; }
    if (!down && curApp < 0 && !pnOverlay && fabsf(pnVel) > 0.5f) { pnScroll += pnVel; pnVel *= 0.93f; }
}

int main(int argc, char** argv) {
    if (argc > 0 && argv && argv[0]) { gArg0 = argv[0]; snprintf(gSelfPath, sizeof gSelfPath, "%s", argv[0]); }
    plInitialize(PlServiceType_User);
    timeInitialize();
    psmInitialize();
    accountInitialize(AccountServiceType_Application);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&gPad);
    hidInitializeTouchScreen();
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/WinNX", 0777);
    padUpdate(&gPad);
    { // rescue: hold B + Y while starting to jump straight to the original hbmenu
        u64 kh = padGetButtons(&gPad);
        if ((kh & HidNpadButton_B) && (kh & HidNpadButton_Y)) {
            std::string t = hbBackupPath();
            if (fileSizeOf(t) > 0 && R_SUCCEEDED(envSetNextLoad(t.c_str(), t.c_str()))) return 0;
        }
    }
    loadCfg(); win9Load(); gPhone = false;
    loadUser();
    lockLoad();
    notifLoad();
    rebuildTiles();
    if (!gV8) { pinApp(A_CORTANA); pinApp(A_THEMES); gV8 = true; saveCfg(); saveLayout(); }
    if (!gV10) { pinApp(A_RECORDER); pinApp(A_FOOD); pinApp(A_HEALTH); pinApp(A_PET); pinApp(A_CITY); gV10 = true; win9Save(); saveLayout(); }
    if (!gV9) { pinApp(A_DESKTOP); pinApp(A_SKYPE); pinApp(A_ONEDRIVE); pinApp(A_TASKMGR); pinApp(A_HELP); gV9 = true; win9Save(); saveLayout(); }
    if (gInstLegacy) { saveCfg(); saveLayout(); }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    gWin = SDL_CreateWindow("Windows 8.1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    gR = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    SDL_SetRenderDrawBlendMode(gR, SDL_BLENDMODE_BLEND);
    gTarget = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, H);

    // fonts: your own Segoe UI (sdmc:/switch/WinNX/segoeui.ttf + segoeuib.ttf) if present, else the built-in
    // Selawik (Microsoft's open-source Segoe UI look-alike); the Switch system font covers any missing symbols
    static std::string blobR, blobB;
    bool haveR = readFile(DIR_ + "segoeui.ttf", blobR) && blobR.size() > 10000;
    bool haveB = readFile(DIR_ + "segoeuib.ttf", blobB) && blobB.size() > 10000;
    PlFontData fd;
    bool haveSys = R_SUCCEEDED(plGetSharedFontByType(&fd, PlSharedFontType_Standard));
    for (int i = 0; i < FCOUNT; i++) {
        if (haveSys) gFontSys[i] = TTF_OpenFontRW(SDL_RWFromMem(fd.address, fd.size), 1, FSZ[i]);
        gFont[i] = haveR ? TTF_OpenFontRW(SDL_RWFromConstMem(blobR.data(), (int)blobR.size()), 1, FSZ[i])
                         : TTF_OpenFontRW(SDL_RWFromConstMem(SEL_REG, (int)sizeof SEL_REG), 1, FSZ[i]);
        gFontB[i] = haveB ? TTF_OpenFontRW(SDL_RWFromConstMem(blobB.data(), (int)blobB.size()), 1, FSZ[i])
                          : TTF_OpenFontRW(SDL_RWFromConstMem(SEL_BOLD, (int)sizeof SEL_BOLD), 1, FSZ[i]);
        if (!gFont[i]) gFont[i] = gFontSys[i];
        if (gFont[i]) TTF_SetFontHinting(gFont[i], TTF_HINTING_LIGHT);
        if (gFontB[i]) TTF_SetFontHinting(gFontB[i], TTF_HINTING_LIGHT);
    }

    lockInit();
    startPicLoad(); avatarLoad();
    uiState = gBootAnim ? 0 : (gLockScr ? 1 : 2);
    sndPlay(SND_STARTUP);
    static bool bootDeskDone = false;
    updateNow();
    crashCheckOnBoot();
    srand((unsigned)SDL_GetTicks());
    lastInput = SDL_GetTicks();
    Uint32 prev = lastInput, lastClock = 0, lastBat = 0, lastSave = 0, kbHeldT = 0, kbRep = 0;
    bool tStart = false, wasDown = false, tMoved = false, tPressWin = false, tTop = false, tKb = false, tApp = false, tOv = false, wasRaw = false, lkDrag = false;
    int tEdge = 0, lkY0 = 0, lkTapMoved = 0;
    bool tHoldR = false, tBot = false, tWinLong = false; Uint32 tDownT = 0;
    int tDX = 0, tDY = 0, tLX = 0, tLY = 0, pressTile = -1, kbHeld = -1;
    static const int SH[5][2] = {{0, 0}, {2, 1}, {-2, 2}, {1, -2}, {-1, -1}};

    while (appletMainLoop() && gRun) {
        Uint32 now = SDL_GetTicks();
        float dt = std::min(0.05f, (now - prev) / 1000.0f);
        prev = now;
        introT += dt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) gRun = false;

        if (now - lastClock > 250) { updateNow(); lastClock = now; }
        if (now - lastBat > 5000 || lastBat == 0) { psmGetBatteryChargePercentage(&gBat); lastBat = now; }
        if (now - lastSave > 1000) { saveAll(); lastSave = now; } // autosave

        // ---- buttons
        padUpdate(&gPad);
        u64 kdRaw = padGetButtonsDown(&gPad);
        u64 kd = (uiState == 2) ? kdRaw : 0;
#ifndef WNX_HOST
        { u64 held = padGetButtons(&gPad); if (uiState == 2 && (kdRaw & (HidNpadButton_Plus | HidNpadButton_Minus)) && (held & HidNpadButton_Plus) && (held & HidNpadButton_Minus)) { kd = 0; } }
#endif
        if (kdRaw) { lastInput = now; dimA = 0; }
        HidTouchScreenState ts = {0};
        hidGetTouchScreenStates(&ts, 1);
        bool downRaw = ts.count > 0;
        { // two fingers = pinch; the single-finger logic is paused until every finger is lifted
            static bool pinching = false; static float pd0 = 1;
            if (uiState == 2 && !gPhone && ts.count >= 2) {
                float pdx = (float)ts.touches[0].x - ts.touches[1].x, pdy = (float)ts.touches[0].y - ts.touches[1].y;
                float pd = sqrtf(pdx * pdx + pdy * pdy);
                int pcx = (int)(ts.touches[0].x + ts.touches[1].x) / 2, pcy = (int)(ts.touches[0].y + ts.touches[1].y) / 2;
                if (!pinching) { pinching = true; pd0 = std::max(pd, 1.0f); tMoved = true; startPinch(pd, true); appPinch(1.0f, pcx, pcy, true); }
                else { startPinch(pd, false); appPinch(pd / pd0, pcx, pcy, false); }
                lastInput = now; dimA = 0;
            }
            if (pinching) { if (ts.count == 0) pinching = false; else downRaw = false; }
        }
        int rawX = downRaw ? (int)ts.touches[0].x : 0, rawY = downRaw ? (int)ts.touches[0].y : 0;
        // ---- boot animation and lock screen
        if (uiState == 0) {
            bootT += dt;
            if ((kdRaw || (downRaw && !wasRaw)) && bootT > 0.4f && bootT < BOOT_END - 0.5f) bootT = BOOT_END - 0.5f;
            if (bootT >= BOOT_END) { uiState = gLockScr ? 1 : 2; lockOff = 0; lockAnim = false; introT = 0; }
        } else if (uiState == 3) {
            gBsodT += dt;
            if ((kdRaw & HidNpadButton_A) || (downRaw && !wasRaw)) bsodLeave();
        } else if (uiState == 1) {
            static int lrx = 0, lry = 0;
            if (downRaw) { lrx = rawX; lry = rawY; }
            if (gAuth) {
                lockOff = 0; authKeys(kdRaw); authUpdate(dt);
                if (downRaw && !wasRaw) authTouch(0, rawX, rawY); else if (downRaw) authTouch(1, rawX, rawY); else if (wasRaw) authTouch(2, lrx, lry);
            } else if (lockAnim) {
                lockOff -= (UIH * 2.4f + 200) * dt;
                if (lockOff <= -UIH) { lockOff = 0; lockAnim = false; uiState = 2; introT = 0; sndPlay(SND_UNLOCK); }
            } else {
                if (kdRaw & (HidNpadButton_A | HidNpadButton_Plus | HidNpadButton_B)) unlockNow();
                if (downRaw && !wasRaw) { lkDrag = true; lkY0 = rawY; lkTapMoved = 0; }
                if (downRaw && lkDrag) { lockOff = std::min(0.0f, (float)(rawY - lkY0)); lkTapMoved = std::max(lkTapMoved, lkY0 - rawY); }
                else if (!downRaw && wasRaw && lkDrag) {
                    lkDrag = false;
                    if (lockOff < -140 || lkTapMoved > 140) unlockNow();
                    else if (lkTapMoved < 14) lockOff = -50; // a tap makes the lock screen bounce
                }
                if (!lkDrag && !lockAnim) lockOff += (0 - lockOff) * std::min(1.0f, dt * 9.0f);
            }
        }
        if (kd & HidNpadButton_Minus) { if (overlayOpen()) closeOverlays(); else charmsOpen = true; }
        if ((kd & HidNpadButton_B) && overlayOpen()) { closeOverlays(); if (kbTarget == 6) kbOpen = false; kd &= ~(u64)HidNpadButton_B; }
        if ((kd & HidNpadButton_ZL) && !gEdit) { if (notifOpen) closeOverlays(); else { closeOverlays(); notifOpen = true; notifScroll = 0; for (auto& n : gNotifs) n.unread = false; notifSave(); } }
        if (kd & HidNpadButton_Plus) winPress();
        if (curApp < 0 && appDir == 0) {
            startPad(kd);
            if (!gEdit && !appsOpen && !zoomOut) {
                if (kd & HidNpadButton_AnyLeft) { navMode = true; navigate(0); }
                if (kd & HidNpadButton_AnyRight) { navMode = true; navigate(1); }
                if (kd & HidNpadButton_AnyUp) { navMode = true; navigate(2); }
                if (kd & HidNpadButton_AnyDown) { navMode = true; navigate(3); }
                if ((kd & HidNpadButton_A) && navMode) launch(sel);
            }
        } else if (curApp >= 0 && appDir == 0) {
            if (kd & HidNpadButton_B) { if (!appBack()) closeApp(false); }
            if (kd & HidNpadButton_X) closeApp(true);
            appKeys(kd);
            if (curApp == A_PHOTOS) {
                if (kd & HidNpadButton_AnyLeft) photoNav(-1);
                if (kd & HidNpadButton_AnyRight) photoNav(1);
            }
        }

        // ---- phone mode: rotate touches into portrait and decide who owns the finger
        bool physDown = downRaw; static bool pnWasRaw = false, pnWasMode = false;
        if (gPhone && uiState == 2) {
            closeOverlays();
            static int lpx = 0, lpy = 0;
            if (downRaw) {
                int px, py; phoneFromRaw((int)ts.touches[0].x, (int)ts.touches[0].y, px, py);
                lastInput = now; dimA = 0;
                if (!pnWasRaw) { pnOwner = phoneOwnerAt(px, py); pnTouch(0, px, py); } else pnTouch(1, px, py);
                lpx = px; lpy = py;
                if (pnOwner == 2) phoneMapToApp(px, py, rawX, rawY); else downRaw = false;
            } else if (pnWasRaw) { pnTouch(2, lpx, lpy); pnOwner = 0; }
            pnUpdate(physDown);
            if (appDir == 1) appT = 1; else if (appDir == -1) appT = 0;
        } else if (pnWasMode) { pnOverlay = 0; pnOwner = 0; }
        pnWasRaw = physDown; pnWasMode = gPhone && uiState == 2;
        // ---- touch
        bool down = downRaw && uiState == 2;
        int tx = down ? rawX : tLX, ty = down ? rawY : tLY;
        bool appOpen = curApp >= 0 && appDir == 0;
        if (down && !wasDown) {
            lastInput = now; dimA = 0; navMode = false;
            tDX = tx; tDY = ty; tLX = tx; tLY = ty; tMoved = false; scrollV = 0;
            tTop = false; tKb = false; tApp = false; kbHeld = -1; tOv = false; tEdge = 0;
            tPressWin = inWinBtn(tx, ty);
            tDownT = now; tHoldR = false; tBot = false; tWinLong = false; swDragApp = -1; swDragX = 0;
            tBot = ty >= UIH - 8 && !overlayOpen() && !gPhone;
            if (clkRinging() || gEgg) { tOv = true; tKb = true; }
            else if (!tPressWin && !overlayOpen() && bannerHit(tx, ty)) { tOv = true; tKb = true; }
            else if (!tPressWin && overlayOpen()) {
                if (kbOpen && ty >= KBTOP && ty < UIH) {
                    buildKeys();
                    int k = kbHit(tx, ty);
                    if (k >= 0) { kbHeld = k; kbHeldT = now; kbRep = now; kbAction(gKeys[k].act); }
                    tKb = true;
                } else { tOv = true; tKb = true; if (switchOpen && tx < 220) swDragApp = swTileAt(ty); }
            } else if (!tPressWin && !gPhone && ty < UIH && tx >= W - 12) tEdge = 1;
            else if (!tPressWin && !gPhone && ty < UIH && tx <= 12) tEdge = 2;
            if (!tPressWin && !overlayOpen() && !tEdge && !tOv && tx >= W - 60 && ty >= 180 && ty <= 500 && uiState == 2 && !gPhone) tHoldR = true;
            pressTile = -1;
            tStart = false;
            if (curApp < 0 && appDir == 0 && !tPressWin && !tKb && !tEdge) { tStart = true; startTouch(0, tx, ty); }
            if (appOpen && !tPressWin && !tKb && !tEdge) {
                if (ty < 30 && gSnap < 0 && !gPhone) { barT = 3.5f; tTop = true; }
                else if (barP > 0.5f && ty < 44 && gSnap < 0 && !gPhone) tTop = true;
                else if (kbOpen && ty >= KBTOP && ty < UIH) {
                    buildKeys();
                    int k = kbHit(tx, ty);
                    if (k >= 0) { kbHeld = k; kbHeldT = now; kbRep = now; kbAction(gKeys[k].act); }
                    tKb = true;
                } else { tApp = true; snapTouch(0, tx, ty); }
            }
        } else if (down && wasDown) {
            lastInput = now;
            int dx = tx - tLX, dy = ty - tLY;
            if (!tMoved && (abs(tx - tDX) > 14 || abs(ty - tDY) > 14)) tMoved = true;
            if (tKb && kbHeld >= 0 && kbHeld < (int)gKeys.size() && gKeys[kbHeld].act == "BS" && now - kbHeldT > 450 && now - kbRep > 70) {
                kbAction("BS"); kbRep = now;
            }
            if (tEdge == 1 && tx < tDX - 36) { notifOpen = true; notifScroll = 0; for (auto& n : gNotifs) n.unread = false; notifSave(); tEdge = 3; }
            if (tHoldR && !tMoved && now - tDownT > 450 && !overlayOpen()) { tHoldR = false; charmsOpen = true; tMoved = true; if (tStart) { startTouch(2, -9999, -9999); tStart = false; } }
            if (tPressWin && !tMoved && !tWinLong && now - tDownT > 650 && !overlayOpen() && appDir == 0) { tWinLong = true; if (curApp != A_CORTANA) gotoApp(A_CORTANA); }
            if (swDragApp >= 0 && tMoved) { swDragX = tx; swDragY = ty; }
            if (tEdge == 2 && tx > tDX + 36) { switchOpen = true; tEdge = 3; }
            if (tOv && tMoved && notifOpen) notifScroll -= dy;
            if (tApp && tMoved) snapTouch(1, tx, ty);
            if (tStart) startTouch(1, tx, ty);
            if (tMoved && !tKb && !tPressWin && !tEdge) {
                if (appOpen && !tTop) snapDrag(dx, dy);
            }
            tLX = tx; tLY = ty;
        } else if (!down && wasDown) {
            if (tApp) { snapTouch(2, tLX, tLY); tApp = false; }
            if (tStart) { startTouch(2, tLX, tLY); tStart = false; }
            if (tBot && tDY - tLY > 70 && !overlayOpen() && appDir == 0) { if (curApp >= 0) closeApp(false); else if (!appsOpen) startOpenApps(); }
            if (tOv) {
                if (swDragApp >= 0 && tMoved && switchOpen) { int a = swDragApp; if (tLX >= 260) { closeOverlays(); if (curApp >= 0 && a != curApp) gSnap = a; else if (curApp < 0) gotoApp(a); } }
                else if (!tMoved) overlayTap(tLX, tLY);
                swDragApp = -1; swDragX = 0; tOv = false; tKb = false; kbHeld = -1;
            }
            else if (tEdge) { if (!tMoved && tEdge == 1) charmsOpen = true; if (!tMoved && tEdge == 2) switchOpen = true; tEdge = 0; }
            else if (tKb) { kbHeld = -1; }
            else if (tTop) {
                if (!tMoved && barP > 0.5f && tLY < 44 && appDir == 0) {
                    if (tLX >= W - 90) closeApp(true);
                    else if (tLX >= W - 180) closeApp(false);
                }
            } else if (!tMoved && appDir == 0) {
                if (inWinBtn(tLX, tLY)) { if (!tWinLong) winPress(); }
                else if (curApp >= 0) snapTap(tLX, tLY);
            } else if (tMoved && appOpen && curApp == A_PHOTOS && phView == 1 && phZ <= 1.02f) {
                int dx = tLX - tDX;
                if (abs(dx) > 120) photoNav(dx < 0 ? 1 : -1);
            }
            tPressWin = false; pressTile = -1; tTop = false;
        }
        wasDown = down;
        wasRaw = downRaw;

        // ---- updates
        if (!down && scrollV != 0) {
            scrollX += scrollV; scrollV *= 0.93f;
            if (fabsf(scrollV) < 0.3f) scrollV = 0;
            if (scrollX < 0 || scrollX > maxScroll) { scrollX = clampf(scrollX, 0, maxScroll); scrollV = 0; }
        }
        if (appDir != 0) {
            appT += appDir * dt * 2.6f;
            if (appT >= 1) { appT = 1; appDir = 0; }
            else if (appT <= 0) {
                appT = 0; appDir = 0;
                if (gCloseReset) appReset(curApp);
                gCloseReset = false;
                curApp = -1;
            }
        }
        if (uiState == 2 && !bootDeskDone) { bootDeskDone = true; if (gBootDesk && pendingApp < 0) pendingApp = A_DESKTOP; }
        if (appDir == 0 && curApp < 0 && pendingApp >= 0) { int a = pendingApp; pendingApp = -1; launchAppAuto(a); }
        for (int a = 0; a < A_COUNT; a++) {
            if (storeProg[a] > 0) {
                storeProg[a] += dt * 0.7f;
                if (storeProg[a] >= 1) { storeProg[a] = 0; gInst[a] = true; pinApp(a); saveLayout(); saveCfg(); }
            }
        }
        if (uiState != 3) { try { appUpdate(dt); startUpdate(dt); } catch (const std::exception& e) { bsodNow("UNHANDLED_EXCEPTION", e.what()); } catch (...) { bsodNow("UNHANDLED_EXCEPTION", ""); } }
        if (curApp >= 0 && appDir == 0) openT += dt;
        charmsP += ((charmsOpen ? 1.0f : 0.0f) - charmsP) * std::min(1.0f, dt * 14.0f);
        switchP += ((switchOpen ? 1.0f : 0.0f) - switchP) * std::min(1.0f, dt * 14.0f);
        notifP += ((notifOpen ? 1.0f : 0.0f) - notifP) * std::min(1.0f, dt * 14.0f);
        searchP += ((searchOpen ? 1.0f : 0.0f) - searchP) * std::min(1.0f, dt * 14.0f);
        if (barT > 0) barT -= dt;
        float barTarget = (barT > 0 && curApp >= 0 && appDir == 0) ? 1.0f : 0.0f;
        barP += (barTarget - barP) * std::min(1.0f, dt * 14.0f);
        if (gTC.size() > 300) { for (auto& p : gTC) SDL_DestroyTexture(p.second.t); gTC.clear(); }

        // ---- burn-in protection
        float idle = (now - lastInput) / 1000.0f;
        float target = 0;
        if (gBurn) target = idle > 300 ? 215.0f : (idle > 60 ? 135.0f : 0.0f);
        dimA += (target - dimA) * 0.06f;
        int sx = 0, sy = 0;
        if (gBurn) { int k = (int)((now / 30000) % 5); sx = SH[k][0]; sy = SH[k][1]; }

        if (curApp < 0) gSnap = -1;
        // ---- render
        if (gTarget) SDL_SetRenderTarget(gR, gTarget);
        SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
        SDL_RenderClear(gR);
        bool phoneOn = gPhone && uiState == 2;
        if (uiState != 3 && !(phoneOn && curApp < 0)) try {
        bool fxOn = curApp >= 0 && appT < 1 && gAnim && gSnap < 0 && fxReady();
        if (fxOn) SDL_SetRenderTarget(gR, gTurnA);
        drawStart(down, tMoved, pressTile);
        if (curApp >= 0) {
            if (appT < 1 && fxOn) {
                SDL_SetRenderTarget(gR, gTurnB); SDL_RenderSetClipRect(gR, nullptr); SDL_SetRenderDrawColor(gR, 0, 0, 0, 255); SDL_RenderClear(gR);
                drawApp(); SDL_RenderSetClipRect(gR, nullptr);
                SDL_SetRenderTarget(gR, gTarget); SDL_SetRenderDrawColor(gR, 0, 0, 0, 255); SDL_RenderClear(gR);
                fxAppTurn(appT);
            } else if (appT < 1) {
                SDL_Rect a = gAnimRect;
                float e = ease(appT);
                int rx = (int)(a.x + (0 - a.x) * e), ry = (int)(a.y + (0 - a.y) * e);
                int rw = (int)(a.w + (W - a.w) * e), rh = (int)(a.h + (UIH - a.h) * e);
                rect(rx, ry, rw, rh, appBg(curApp));
            } else {
                if (gSnap >= 0) drawSnap(); else drawApp();
                if (kbOpen) drawKb();
                if (gAnim && openT < 0.28f) rect(0, 0, W, UIH, appBg(curApp), (int)(255 * (1.0f - ease(openT / 0.28f))));
                if (gSnap < 0) drawTopBar();
            }
        }
        if (uiState == 2 || lockAnim) {
            if (charmsP > 0.01f || switchP > 0.01f || searchP > 0.01f || notifP > 0.01f) rect(0, 0, W, UIH, BLACK, 70);
            drawSwitcher();
            drawCharms();
            drawSearchCharm();
            drawNotifPanel();
            if (kbOpen && kbTarget == 6) drawKb();
            drawBanner();
            drawToast();
            clkDrawOverlay(); eggDraw();
        }
        } catch (const std::exception& e) { SDL_RenderSetClipRect(gR, nullptr); bsodNow("UNHANDLED_EXCEPTION", e.what()); }
        catch (...) { SDL_RenderSetClipRect(gR, nullptr); bsodNow("UNHANDLED_EXCEPTION", ""); }
        if (uiState == 0) drawBoot(bootT);
        else if (uiState == 1) { drawLock(lockOff); if (gAuth && kbOpen) drawKb(); }
        if (uiState == 3) drawBsod();
        else {
        rect(0, UIH, W, H - UIH, BLACK);
        bool wb = downRaw && tPressWin;
        winLogo(W / 2, UIH + (H - UIH) / 2, 26, WHITE, wb ? 255 : 105);
        }
        if (dimA > 1) rect(0, 0, W, H, BLACK, (int)dimA);

        if (phoneOn) phoneCompose();
        else if (gTarget) {
            SDL_SetRenderTarget(gR, nullptr);
            SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
            SDL_RenderClear(gR);
            SDL_Rect dst = {sx, sy, W, H};
            SDL_RenderCopy(gR, gTarget, nullptr, &dst);
        }
        gFrameBusyMs = gFrameBusyMs * 0.85f + 0.15f * (float)(SDL_GetTicks() - now);
        SDL_RenderPresent(gR);
    }

    commitCell();
    saveAll();
    saveCfg();
    remove((DIR_ + "crash.txt").c_str());
    if (phTex) SDL_DestroyTexture(phTex);
    if (gLockTex) SDL_DestroyTexture(gLockTex);
    if (gStartTex) SDL_DestroyTexture(gStartTex);
    for (auto& p : gTC) SDL_DestroyTexture(p.second.t);
    for (int i = 0; i < FCOUNT; i++) { if (gFont[i] && gFont[i] != gFontSys[i]) TTF_CloseFont(gFont[i]); if (gFontB[i]) TTF_CloseFont(gFontB[i]); if (gFontSys[i]) TTF_CloseFont(gFontSys[i]); }
    if (gTarget) SDL_DestroyTexture(gTarget);
    SDL_DestroyRenderer(gR);
    SDL_DestroyWindow(gWin);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    accountExit();
    psmExit();
    timeExit();
    plExit();
    return 0;
}
