// WinNX v0.4 - Windows 8.1-style tablet shell for Nintendo Switch (SDL2 + libnx)
// Touch-first. Black "bezel" strip at the bottom holds the Windows button (OLED friendly).
// Everything you write (Notes, Word, Excel) autosaves to sdmc:/switch/WinNX/ and reloads next time.
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>
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
static const Theme THEMES[4] = {
    {{58, 16, 140}, {28, 4, 88}, {122, 62, 224}},
    {{10, 72, 164}, {4, 28, 92}, {0, 120, 215}},
    {{0, 112, 112}, {0, 48, 60}, {0, 150, 140}},
    {{150, 20, 62}, {68, 4, 34}, {200, 40, 70}},
};
static int gTheme = 0;
static bool gBurn = true, g24 = true;
static int gDocFont = F22;
static bool gBold = false;
static int gEngine = 0; // CMrhoe search engine: 0 DuckDuckGo, 1 Google, 2 Bing
static bool gLockScr = true, gBootAnim = true;

static std::string gUser = "User";
static u32 gBat = 0;
static const std::string DIR_ = "sdmc:/switch/WinNX/";

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
static bool readFile(const std::string& p, std::string& out) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    char buf[4096]; size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}
static bool writeFile(const std::string& p, const std::string& d) {
    FILE* f = fopen(p.c_str(), "wb");
    if (!f) return false;
    fwrite(d.data(), 1, d.size(), f);
    fclose(f);
    return true;
}
static void backupFile(const std::string& p) { // keep the old copy as .bak before "New"
    std::string old;
    if (readFile(p, old) && !old.empty()) writeFile(p + ".bak", old);
}
static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool endsWith(const std::string& s, const std::string& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }

// ---------------------------------------------------------------- tiles / apps
enum { A_MAIL, A_MUSIC, A_PHOTOS, A_DESKTOP, A_CALENDAR, A_STORE, A_CALC, A_NOTES, A_WEATHER, A_CLOCK, A_SETTINGS, A_POWER, A_GAMES, A_WORD, A_EXCEL,
       A_BROWSER, A_PAINT, A_SOLITAIRE, A_2048, A_MINES, A_SNAKE, A_TODO, A_FILES, A_TASKMGR, A_TIMER, A_COUNT };

// Apps that can be installed from the Store (all built in; "installing" adds a Start tile)
struct SApp { int app; const char* name; const char* cat; const char* d1; const char* d2; Col col; int w; const char* glyph; int stars; const char* rate; };
static const SApp STORE[] = {
    {A_BROWSER, "CMrhoe", "Internet", "Search the web or type an address.", "Bookmarks, history and three search engines (DuckDuckGo, Google, Bing). Pages open in the Switch web engine, so you need Wi-Fi.", {219, 68, 55}, 2, "C", 5, "2,431"},
    {A_WORD, "Word 2016", "Productivity", "Write letters and documents.", "Autosaves to your SD card. Bold, text size, word count.", {43, 87, 154}, 2, "W", 5, "9,812"},
    {A_PAINT, "Paint", "Art", "Draw with your finger.", "Brush, eraser, fill, line, rectangle and oval tools, 16 colours, undo. Save pictures you can open in Photos or put into Word.", {200, 70, 160}, 1, "P", 4, "5,320"},
    {A_SOLITAIRE, "Solitaire", "Games", "The classic card game.", "Klondike. Drag cards or tap to auto-move, undo, draw 1 or 3. Your game is saved.", {20, 110, 60}, 1, "SO", 5, "21,386"},
    {A_2048, "2048", "Games", "Swipe to merge tiles.", "Swipe or use the D-pad. Your board and best score are saved.", {237, 150, 50}, 1, "2048", 5, "12,904"},
    {A_MINES, "Minesweeper", "Games", "Clear the board without hitting a mine.", "Tap to dig, switch to Flag mode to mark mines. The first tap is always safe.", {90, 100, 125}, 1, "M", 4, "8,477"},
    {A_SNAKE, "Snake", "Games", "Swipe to steer, eat, grow.", "Swipe or use the D-pad. Your best score is saved.", {60, 150, 80}, 1, "SN", 4, "6,230"},
    {A_EXCEL, "Excel 2016", "Productivity", "Spreadsheets with formulas.", "SUM, AVERAGE, MIN, MAX, COUNT and cell references like =A1+B2. Autosaves.", {33, 115, 70}, 2, "X", 5, "7,104"},
};
static const int NSTORE = (int)(sizeof(STORE) / sizeof(STORE[0]));
static bool gInst[A_COUNT];
static const char* CATS[5] = {"Games", "Productivity", "Internet", "Art", "Tools"};

struct Tile { int col, row, w, h; Col color; const char* label; int app; };
static const Tile BASE[] = {
    {0, 0, 2, 1, {30, 144, 224}, "Mail", A_MAIL},
    {0, 1, 1, 1, {222, 89, 26}, "Music", A_MUSIC},
    {1, 1, 1, 1, {0, 150, 160}, "Photos", A_PHOTOS},
    {0, 2, 2, 1, {110, 70, 215}, "Calendar", A_CALENDAR},
    {2, 0, 2, 2, {16, 160, 50}, "Store", A_STORE},
    {2, 2, 1, 1, {60, 140, 70}, "Calculator", A_CALC},
    {3, 2, 1, 1, {200, 35, 75}, "Notes", A_NOTES},
    {4, 0, 2, 1, {50, 150, 230}, "Weather", A_WEATHER},
    {4, 1, 2, 2, {0, 110, 110}, "Clock", A_CLOCK},
    {6, 0, 1, 1, {90, 90, 100}, "Settings", A_SETTINGS},
    {7, 0, 1, 1, {200, 50, 50}, "Power", A_POWER},
    {6, 1, 2, 2, {130, 50, 160}, "Games", A_GAMES},
};
static std::vector<Tile> gTiles;
static const int U = 156, G = 8, LEFT = 90, TOP = 140;
static float scrollX = 0, scrollV = 0, maxScroll = 0;
static int sel = 0;

static int groupExtra(int col) { return 36 * (col / 2); }
static SDL_Rect tileBase(int i) {
    const Tile& t = gTiles[i];
    SDL_Rect r;
    r.x = LEFT + t.col * (U + G) + groupExtra(t.col);
    r.y = TOP + t.row * (U + G);
    r.w = t.w * U + (t.w - 1) * G;
    r.h = t.h * U + (t.h - 1) * G;
    return r;
}
static void rebuildTiles() {
    gTiles.assign(BASE, BASE + sizeof(BASE) / sizeof(BASE[0]));
    // installed Store apps are packed into extra groups (2 columns x 3 rows each) to the right
    std::vector<char> occ; // 6 cells per group
    for (int i = 0; i < NSTORE; i++) {
        const SApp& a = STORE[i];
        if (!gInst[a.app]) continue;
        bool placed = false;
        for (int g = 0; !placed; g++) {
            if ((int)occ.size() < 6 * (g + 1)) occ.resize(6 * (g + 1), 0);
            for (int r = 0; r < 3 && !placed; r++)
                for (int c = 0; c + a.w <= 2 && !placed; c++) {
                    bool ok = true;
                    for (int k = 0; k < a.w; k++) if (occ[6 * g + r * 2 + c + k]) ok = false;
                    if (!ok) continue;
                    for (int k = 0; k < a.w; k++) occ[6 * g + r * 2 + c + k] = 1;
                    gTiles.push_back(Tile{8 + 2 * g + c, r, a.w, 1, a.col, a.name, a.app});
                    placed = true;
                }
        }
    }
    int m = 0;
    for (int i = 0; i < (int)gTiles.size(); i++) { SDL_Rect r = tileBase(i); m = std::max(m, r.x + r.w); }
    maxScroll = std::max(0.0f, (float)(m + 90 - W));
    scrollX = clampf(scrollX, 0, maxScroll);
    if (sel >= (int)gTiles.size()) sel = 0;
}
static int tileIndexFor(int app) {
    for (int i = 0; i < (int)gTiles.size(); i++) if (gTiles[i].app == app) return i;
    return -1;
}
static std::string appName(int app) {
    for (auto& t : gTiles) if (t.app == app) return t.label;
    return "App";
}

// ---------------------------------------------------------------- state
static int curApp = -1, lastApp = -1, animTile = -1, lastTile = -1, appDir = 0, pendingTile = -1;
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
static bool overlayOpen() { return charmsOpen || switchOpen || searchOpen; }
static void closeOverlays() { charmsOpen = switchOpen = searchOpen = false; }
static void searchGo();
static void hbRefresh();

static Col appBg(int app) {
    switch (app) {
        case A_CALC: return Col{31, 31, 31};
        case A_CLOCK: return Col{0, 84, 88};
        case A_SETTINGS: return Col{38, 38, 38};
        case A_POWER: return Col{27, 27, 27};
        case A_PHOTOS: return Col{0, 0, 0};
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
    for (auto& t : gTiles) if (t.app == app) return t.color;
    return Col{40, 40, 40};
}

// ---------------------------------------------------------------- settings persistence
static void saveCfg() {
    std::string b;
    b += (char)0xA9; b += (char)gTheme; b += (char)gBurn; b += (char)g24;
    b += (char)gDocFont; b += (char)gBold; b += (char)gEngine; b += (char)gLockScr; b += (char)gBootAnim;
    for (int a = 0; a < A_COUNT; a++) b += (char)gInst[a];
    writeFile(DIR_ + "settings.cfg", b);
}
static void loadCfg() {
    std::string s;
    if (!readFile(DIR_ + "settings.cfg", s) || s.size() < 4) return;
    unsigned char m = (unsigned char)s[0];
    if (m == 0xA7) { // v0.2 format
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        if (s.size() >= 8) {
            gInst[A_WORD] = s[4] != 0; gInst[A_EXCEL] = s[5] != 0;
            gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[6]));
            gBold = s[7] != 0;
        }
    } else if (m == 0xA9 && s.size() >= 9) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[4]));
        gBold = s[5] != 0; gEngine = (unsigned char)s[6] % 3; gLockScr = s[7] != 0; gBootAnim = s[8] != 0;
        for (int a = 0; a < A_COUNT && 9 + a < (int)s.size(); a++) gInst[a] = s[9 + a] != 0;
    } else if (m == 0xA8 && s.size() >= 7) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[4]));
        gBold = s[5] != 0; gEngine = (unsigned char)s[6] % 3;
        for (int a = 0; a < A_COUNT && 7 + a < (int)s.size(); a++) gInst[a] = s[7 + a] != 0;
    }
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
static const int XR = 60, XC = 10, XVC = 8, XROWH = 38, XTOP = 172, XHDRW = 50, XCOLW = 150;
static std::string xs[XR][XC];
static int xr = 0, xc = 0, xScrR = 0, xScrC = 0;
static std::string xEdit;
static bool xEditing = false, xDirty = false;

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
static bool kbOpen = false;
static std::string gUrl, storeSearch;
static void browserGo();
static void wType(const std::string& t);
static void wBack();
static void appTouch(int phase, int x, int y);
static void appUpdate(float dt);
static void appKeys(u64 kd);
static int kbTarget = 0; // 1 notes, 2 word, 3 excel cell, 4 browser address, 5 store search, 6 search charm
static bool kbShift = false, kbSym = false;
static int xRows() { int bottom = kbOpen ? KBTOP : UIH; return std::max(1, (bottom - XTOP) / XROWH); }
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
}
static void kbBackspace() {
    if (kbTarget == 1) { if (!gNote.s.empty()) { gNote.s.pop_back(); gNote.dirty = true; gNote.follow = true; } }
    else if (kbTarget == 2) wBack();
    else if (kbTarget == 3) { startEdit(); if (!xEdit.empty()) xEdit.pop_back(); }
    else if (kbTarget == 4) { if (!gUrl.empty()) gUrl.pop_back(); }
    else if (kbTarget == 5) { if (!storeSearch.empty()) storeSearch.pop_back(); }
    else if (kbTarget == 6) { if (!gQuery.empty()) gQuery.pop_back(); }
}
static void openKb(int target) { kbOpen = true; kbTarget = target; kbShift = false; kbSym = false; }
static void hideKb() { if (kbTarget == 3) commitCell(); kbOpen = false; }
static void kbAction(const std::string& a) {
    if (a == "SHIFT") kbShift = !kbShift;
    else if (a == "SYM") kbSym = !kbSym;
    else if (a == "BS") kbBackspace();
    else if (a == "ENTER") {
        if (kbTarget == 3) { commitCell(); xr = std::min(xr + 1, XR - 1); xEnsureVisible(); }
        else if (kbTarget == 4) browserGo();
        else if (kbTarget == 5) hideKb();
        else if (kbTarget == 6) searchGo();
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
static bool isImg(const std::string& n) {
    std::string l = lower(n);
    return endsWith(l, ".jpg") || endsWith(l, ".jpeg") || endsWith(l, ".png") || endsWith(l, ".bmp");
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
// @@APPS_A@@
// ---------------------------------------------------------------- autosave + app lifecycle
static void saveAll() {
    if (gNote.dirty) { writeFile(DIR_ + "notes.txt", gNote.s); gNote.dirty = false; }
    if (wDirty) { wSave(); wDirty = false; }
    if (xDirty) { writeFile(DIR_ + "sheet1.csv", sheetToCsv()); xDirty = false; }
    if (pDirty && !pDown) { paintSaveFile(DIR_ + "paint.bmp"); pDirty = false; }
    if (g2dirty) { g2Save(); g2dirty = false; }
    if (solDirty) { writeFile(DIR_ + "solitaire.dat", solSer()); solDirty = false; }
    // @@SAVE@@
}
static void appInit(int app) {
    std::string d;
    switch (app) {
        case A_NOTES: readFile(DIR_ + "notes.txt", gNote.s); gNote.scroll = 0; gNote.follow = true; gNote.dirty = false; break;
        case A_WORD: wLoad(); wTab = 0; wView = 0; break;
        case A_EXCEL:
            if (readFile(DIR_ + "sheet1.csv", d)) csvToSheet(d);
            xr = xc = xScrR = xScrC = 0; xEditing = false; xDirty = false; break;
        case A_PHOTOS:
            gPhotos.clear();
            scanDir("sdmc:/Nintendo/Album", 6, false, gPhotos);
            scanDir("sdmc:/switch/WinNX/photos", 2, false, gPhotos);
            scanDir("sdmc:/Pictures", 3, false, gPhotos);
            std::sort(gPhotos.begin(), gPhotos.end());
            phIdx = gPhotos.empty() ? 0 : (int)gPhotos.size() - 1;
            loadPhoto();
            break;
        case A_GAMES: scanGames(); gamesScroll = 0; break;
        case A_STORE: storeStart = SDL_GetTicks(); storeView = 0; storeCat = -1; storeScroll = 0; storeSearch.clear(); break;
        case A_BROWSER: loadHist(); brMsg.clear(); break;
        case A_SETTINGS: hbRefresh(); gHbMsg.clear(); gHbWhich = 0; break;
        case A_PAINT: paintLoad(); break;
        case A_2048: g2Load(); break;
        case A_MINES: mNew(); break;
        case A_SNAKE: snakeLoad(); break;
        case A_SOLITAIRE: solLoad(); solPressed = solDragging = false; break;
        // @@INIT@@
        default: break;
    }
}
static void appReset(int app) {
    saveAll();
    switch (app) {
        case A_NOTES: gNote.s.clear(); break;
        case A_WORD: wDoc.clear(); wFreeImgs(); break;
        case A_EXCEL: for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear(); break;
        case A_PHOTOS: if (phTex) { SDL_DestroyTexture(phTex); phTex = nullptr; } gPhotos.clear(); break;
        case A_GAMES: gGames.clear(); break;
        case A_STORE: storeView = 0; break;
        case A_PAINT: paintFree(); break;
        // @@RESET@@
        case A_CALC: cCur = "0"; cAcc = 0; cOp = 0; cFresh = true; cErr = false; break;
        default: break;
    }
    running[app] = false;
}

// ---------------------------------------------------------------- navigation
static void launch(int tile) {
    if (appDir != 0 || curApp >= 0 || tile < 0 || tile >= (int)gTiles.size()) return;
    animTile = tile; lastTile = tile;
    curApp = gTiles[tile].app; lastApp = curApp;
    if (!running[curApp]) { appInit(curApp); running[curApp] = true; }
    appDir = 1; appT = 0; barT = 0; barP = 0;
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
    else if (lastTile >= 0) launch(lastTile);
}
static SDL_Rect tileScreen(int i) {
    SDL_Rect r = tileBase(i);
    r.x -= (int)scrollX;
    return r;
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
        default: break;
    }
}

// ---------------------------------------------------------------- start screen
static void drawTile(int i, float introOff, bool pressed, bool selected) {
    const Tile& t = gTiles[i];
    SDL_Rect r = tileScreen(i);
    r.x += (int)introOff;
    if (r.x > W || r.x + r.w < 0) return;
    if (pressed) { r.x += 5; r.y += 5; r.w -= 10; r.h -= 10; }
    rect(r.x, r.y, r.w, r.h, t.color);
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2 - 12;
    std::string label = t.label;
    if (t.app == A_CALENDAR) {
        text(std::to_string(gNow.d), F72, r.x + 14, r.y + 8, WHITE);
        label = WD[dow(gNow.y, gNow.mo, gNow.d)];
    } else if (t.app == A_CLOCK) {
        int ccx = cx, ccy = r.y + r.h / 2 - 22;
        ring(ccx, ccy, 96, 5, WHITE, t.color);
        for (int k = 0; k < 12; k++) {
            float a = k * 0.523599f;
            line(ccx + (int)(sinf(a) * 78), ccy - (int)(cosf(a) * 78), ccx + (int)(sinf(a) * 88), ccy - (int)(cosf(a) * 88), 3, WHITE);
        }
        float ah = ((gNow.h % 12) + gNow.mi / 60.0f) * 0.523599f, am = gNow.mi * 0.104720f, as = gNow.s * 0.104720f;
        line(ccx, ccy, ccx + (int)(sinf(ah) * 46), ccy - (int)(cosf(ah) * 46), 6, WHITE);
        line(ccx, ccy, ccx + (int)(sinf(am) * 70), ccy - (int)(cosf(am) * 70), 4, WHITE);
        line(ccx, ccy, ccx + (int)(sinf(as) * 76), ccy - (int)(cosf(as) * 76), 2, WHITE, 200);
        disc(ccx, ccy, 6, WHITE);
        text(timeStr(true), F22, r.x + r.w - 12, r.y + r.h - 34, WHITE, 2);
    } else {
        drawIcon(t.app, cx, cy, t.color);
    }
    text(label, t.app == A_CALENDAR ? F22 : F16, r.x + 10, r.y + r.h - (t.app == A_CALENDAR ? 34 : 26), WHITE);
    if (running[t.app]) disc(r.x + r.w - 14, r.y + 14, 5, WHITE);
    if (selected) outline(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 3, WHITE);
}

static void drawStart(bool tDown, bool tMoved, int pressTile) {
    const Theme& th = THEMES[gTheme];
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(th.top, th.bottom, y / (float)UIH));
    text("Start", F72, 88, 30, WHITE);
    int ax = W - 100, ay = 40;
    rect(ax, ay, 48, 48, Col{70, 70, 90});
    disc(ax + 24, ay + 19, 9, Col{200, 200, 215});
    rect(ax + 10, ay + 32, 28, 16, Col{200, 200, 215});
    text(gUser, F22, W - 112, 42, WHITE, 2);
    char bb[16]; snprintf(bb, sizeof bb, "%u%%", (unsigned)gBat);
    text(bb, F16, W - 112, 72, WHITE, 2, 190);
    for (int i = 0; i < (int)gTiles.size(); i++) {
        float t = ease((introT - 0.05f * i) / 0.45f);
        float off = (1.0f - t) * 320.0f;
        drawTile(i, off, tDown && !tMoved && pressTile == i, navMode && sel == i);
    }
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
    if (gInst[a.app]) { pendingTile = tileIndexFor(a.app); closeApp(false); }
    else storeProg[a.app] = 0.001f;
}
static void storeUninstall(int idx) {
    const SApp& a = STORE[idx];
    if (!gInst[a.app]) return;
    if (running[a.app]) appReset(a.app);
    gInst[a.app] = false; rebuildTiles(); saveCfg();
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

// ---------------------------------------------------------------- Word (UI)
struct WB { int id, x, w; const char* l; };
static std::vector<WB> wRibbon() {
    std::vector<WB> v;
    if (wTab == 0) {
        v = {{1, 20, 60, "B"}, {2, 86, 60, "I"}, {3, 152, 60, "U"}, {4, 236, 64, "A-"}, {5, 306, 64, "A+"},
             {6, 400, 100, "Normal"}, {7, 506, 100, "Title"}, {8, 612, 100, "Head"},
             {9, 740, 60, "Left"}, {10, 806, 60, "Mid"}, {11, 872, 60, "Right"}, {12, 960, 60, "\xE2\x80\xA2"}, {13, 1026, 60, "1."}};
    } else if (wTab == 1) {
        v = {{20, 20, 160, "Picture"}, {21, 190, 160, "Symbol"}, {22, 360, 160, "Date"}, {23, 530, 160, "Time"}, {24, 700, 160, "Divider"}};
    } else {
        v = {{30, 20, 150, "New"}, {31, 180, 150, "Save"}};
    }
    return v;
}
static const int WTOP = 154, WPX = 230, WPW = 820;
static std::vector<int> wY, wH;
static int wFontOf(const WPara& p) {
    if (p.style == 1) return std::min(F72, gDocFont + 2);
    if (p.style == 2) return std::min(F72, gDocFont + 1);
    return gDocFont;
}
static const std::vector<std::string>& wLines(WPara& p, int f, int w) {
    if (p.cf != f || p.cw != w || p.cs != p.t) {
        int st = (p.b || p.style == 1 ? 1 : 0) | (p.i ? 2 : 0);
        (void)st;
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
        case 20: wScanPics(); wView = 1; hideKb(); return;
        case 21: wView = 2; hideKb(); return;
        case 22: wType(dateStr()); openKb(2); break;
        case 23: wType(timeStr()); openKb(2); break;
        case 24: { WPara np; np.style = 3; wInsertAfter(np); WPara t; wInsertAfter(t); } break;
        case 30: backupFile(DIR_ + "document1.wdoc"); wDoc.clear(); wEnsure(); wScroll = 0; break;
        case 31: break;
        default: break;
    }
    wDirty = true; wFollow = true;
}
static const char* SYMS[40] = {"\xC2\xA9", "\xC2\xAE", "\xE2\x84\xA2", "\xC2\xB0", "\xC2\xB1", "\xC3\x97", "\xC3\xB7", "\xE2\x82\xAC", "\xC2\xA3", "\xC2\xA5",
    "\xC2\xA7", "\xC2\xB6", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94", "\xE2\x80\xA6", "\xC2\xAB", "\xC2\xBB", "\xC2\xBD", "\xC2\xBC",
    "\xC2\xBE", "\xC2\xB2", "\xC2\xB3", "\xC2\xB5", "\xCF\x80", "\xCE\xA9", "\xCE\xB1", "\xCE\xB2", "\xE2\x89\xA4", "\xE2\x89\xA5",
    "\xE2\x89\xA0", "\xE2\x88\x9E", "\xE2\x88\x9A", "\xE2\x86\x90", "\xE2\x86\x92", "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x9C\x93", "\xE2\x98\x85", "\xE2\x99\xA5"};
static void drawWord(int bottom) {
    Col blue = {43, 87, 154};
    rect(0, 0, W, UIH, Col{225, 226, 230});
    wEnsure();
    // ---- document area (drawn first so the ribbon overlaps cleanly)
    SDL_Rect clip = {0, WTOP, W, std::max(1, bottom - WTOP)};
    rect(190, WTOP, 900, bottom - WTOP, WHITE);
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
            if (im) { float sc = std::min(std::min(1.0f, (float)WPW / im->w), 320.0f / im->h); h = (int)(im->h * sc) + 8; } else h = 50;
        } else {
            int n = (int)wLines(p, f, WPW - indent).size();
            h = std::max(1, n) * lh;
        }
        wY[i] = y; wH[i] = h; y += h + 8;
    }
    int total = y + 80;
    int view = bottom - WTOP - 20;
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
        if (py + wH[i] < WTOP || py > bottom) { if (p.list == 2 && p.style < 3) num++; continue; }
        bool cur = (i == wCur);
        if (cur) rect(WPX - 24, py - 2, 4, wH[i] + 4, Col{120, 160, 220});
        if (p.style == 3) { rect(WPX, py + 10, WPW, 3, Col{110, 110, 110}); continue; }
        if (p.style == 4) {
            WImg* im = wImage(p.img);
            if (im) {
                float sc = std::min(std::min(1.0f, (float)WPW / im->w), 320.0f / im->h);
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
    // ---- title bar + tabs
    rect(0, 34, W, 56, blue);
    text("Word 2016", F30, 20, 46, WHITE);
    static const char* TABS[3] = {"Home", "Insert", "File"};
    for (int i = 0; i < 3; i++) {
        int x = 230 + i * 120;
        if (wTab == i) { rect(x, 44, 110, 46, Col{245, 245, 247}); text(TABS[i], F22, x + 55, 54, blue, 1); }
        else text(TABS[i], F22, x + 55, 54, WHITE, 1);
    }
    text(std::to_string(wWords()) + " words", F22, W - 20, 52, WHITE, 2, 210);
    // ---- ribbon
    rect(0, 90, W, 64, Col{245, 245, 247});
    rect(0, 153, W, 1, Col{190, 190, 195});
    WPara& cp = wDoc[wCur];
    for (auto& b : wRibbon()) {
        bool on = false;
        switch (b.id) {
            case 1: on = cp.b; break; case 2: on = cp.i; break; case 3: on = cp.u; break;
            case 6: on = cp.style == 0; break; case 7: on = cp.style == 1; break; case 8: on = cp.style == 2; break;
            case 9: on = cp.align == 0; break; case 10: on = cp.align == 1; break; case 11: on = cp.align == 2; break;
            case 12: on = cp.list == 1; break; case 13: on = cp.list == 2; break;
            default: break;
        }
        rect(b.x, 98, b.w, 48, on ? blue : Col{220, 222, 228});
        int st = b.id == 1 ? 1 : (b.id == 2 ? 2 : (b.id == 3 ? 4 : 0));
        textS(b.l, F22, b.x + b.w / 2, 108, on ? WHITE : DARK, st, 1);
    }
    if (wTab == 2) text("Autosaved to switch/WinNX/document1.wdoc", F16, 350, 112, Col{90, 90, 100});
    if (wTab == 1) text("Pictures come from Pictures, switch/WinNX/photos and the Album", F16, 890, 112, Col{90, 90, 100}, 0, 0);
    // ---- overlays
    if (wView == 1) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170);
        rect(140, 50, 1000, 560, Col{245, 245, 247});
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
        rect(140, 50, 1000, 560, Col{245, 245, 247});
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
    if (y >= 44 && y < 90) {
        for (int i = 0; i < 3; i++) if (inR(x, y, 230 + i * 120, 44, 110, 46)) { wTab = i; return; }
        return;
    }
    if (y >= 90 && y < 154) {
        for (auto& b : wRibbon()) if (inR(x, y, b.x, 98, b.w, 48)) { wApply(b.id); return; }
        return;
    }
    if (y >= WTOP) {
        int ry = y - WTOP - 18 + wScroll;
        int hit = -1;
        for (int i = 0; i < (int)wY.size(); i++) if (ry >= wY[i] - 4 && ry < wY[i] + wH[i] + 4) { hit = i; break; }
        if (hit < 0 && !wY.empty() && ry >= wY.back()) hit = (int)wY.size() - 1;
        if (hit >= 0) wCur = hit;
        openKb(2); wFollow = true;
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
// @@APPS_B@@
static void appTouch(int phase, int x, int y) {
    switch (curApp) {
        case A_PAINT: paintTouch(phase, x, y); break;
        case A_2048: if (phase == 0) { swX = x; swY = y; } else if (phase == 2) { int d = swipeOf(x, y, 50); if (d >= 0) g2Move(d); } break;
        case A_SNAKE:
            if (phase == 0) { swX = x; swY = y; }
            else if (phase == 1) { int d = swipeOf(x, y, 36); if (d >= 0) { snakeTurn(d); swX = x; swY = y; } }
            break;
        case A_SOLITAIRE: solTouch(phase, x, y); break;
        // @@TOUCH@@
        default: break;
    }
}

// ---------------------------------------------------------------- homebrew menu (hbmenu) replace / backup / restore
static const char WNX_MARK[] = "WINNX-SELF-MARKER-9c41e7";
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
    if (self == "sdmc:/hbmenu.nro" || self == "/hbmenu.nro") { gHbMsg = "WinNX is already running as your homebrew menu."; hbRefresh(); return; }
    bool have = fileSizeOf(HB_PATH) > 0;
    if (have && !fileHasMarker(HB_PATH)) { // the original hbmenu: back it up first
        if (!safeReplace(HB_PATH, hbBackupPath())) { gHbMsg = "Backup of hbmenu failed, nothing was changed."; hbRefresh(); return; }
    } else if (!have && fileSizeOf(hbBackupPath()) <= 0) {
        gHbMsg = "No hbmenu.nro found on the SD card root, so there is nothing to back up. Nothing was changed.";
        return;
    }
    if (!safeReplace(self, HB_PATH)) { gHbMsg = "Couldn't write sdmc:/hbmenu.nro. Use Restore, or copy it back from a PC."; hbRefresh(); return; }
    hbRefresh();
    gHbMsg = "Done. Launching homebrew from the Album now opens WinNX. Original hbmenu saved to switch/WinNX/hbmenu_backup.nro.";
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
    if (gHbConfirm > 0) { gHbConfirm -= dt; if (gHbConfirm <= 0) gHbWhich = 0; }
    if (curApp == A_MINES && appDir == 0 && mState == 1) mTime += dt;
    if (curApp == A_SNAKE && appDir == 0 && sState == 1) {
        sAcc += dt;
        float stepT = std::max(0.07f, 0.14f - sScore * 0.003f);
        while (sAcc >= stepT && sState == 1) { sAcc -= stepT; snakeStep(); }
    }
    // @@UPDATE@@
}
static void appKeys(u64 kd) {
    int d = -1;
    if (kd & HidNpadButton_AnyLeft) d = 0; else if (kd & HidNpadButton_AnyRight) d = 1;
    else if (kd & HidNpadButton_AnyUp) d = 2; else if (kd & HidNpadButton_AnyDown) d = 3;
    if (curApp == A_2048 && d >= 0) g2Move(d);
    if (curApp == A_SNAKE && d >= 0) snakeTurn(d);
    // @@KEYS@@
}
static void drawApp() {
    const Theme& th = THEMES[gTheme];
    Col bg = appBg(curApp);
    int bottom = areaBottom();
    switch (curApp) {
        case A_CALC: {
            rect(0, 0, W, UIH, bg);
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
            break;
        }
        case A_CLOCK: {
            rect(0, 0, W, UIH, bg);
            text(timeStr(), F130, W / 2, 130, WHITE, 1);
            char s[8]; snprintf(s, sizeof s, "%02d", gNow.s);
            text(s, F44, W / 2, 300, WHITE, 1, 200);
            if (!g24) text(gNow.h >= 12 ? "PM" : "AM", F30, W / 2, 370, WHITE, 1, 200);
            text(dateStr(), F44, W / 2, 440, WHITE, 1);
            break;
        }
        case A_CALENDAR: {
            rect(0, 0, W, UIH, bg);
            text(std::string(MONTHS[gNow.mo - 1]) + " " + std::to_string(gNow.y), F72, 70, 34, WHITE);
            for (int c = 0; c < 7; c++) text(WD3[c], F22, 150 + c * 140 + 70, 150, WHITE, 1, 200);
            int first = dow(gNow.y, gNow.mo, 1), dim = daysIn(gNow.y, gNow.mo);
            for (int d = 1; d <= dim; d++) {
                int idx = first + d - 1, col = idx % 7, row = idx / 7;
                int x = 150 + col * 140, y = 196 + row * 76;
                bool today = d == gNow.d;
                rect(x + 3, y + 3, 134, 70, WHITE, today ? 255 : 30);
                text(std::to_string(d), F30, x + 70, y + 18, today ? bg : WHITE, 1);
            }
            break;
        }
        case A_SETTINGS: {
            rect(0, 0, W, UIH, bg);
            appTitle("Settings");
            text("Background", F30, 70, 130, WHITE);
            for (int i = 0; i < 4; i++) {
                int x = 70 + i * 100;
                rect(x, 178, 80, 80, mix(THEMES[i].top, THEMES[i].bottom, 0.3f));
                if (i == gTheme) outline(x - 5, 173, 90, 90, 4, WHITE);
            }
            static const char* ST[4] = {"Burn-in protection", "24-hour clock", "Lock screen", "Boot animation"};
            static const char* SD[4] = {"Dims and shifts the screen when idle (OLED)", "Off shows 12-hour time", "Show the lock screen at start-up", "Windows logo animation at start-up"};
            bool sv[4] = {gBurn, g24, gLockScr, gBootAnim};
            for (int i = 0; i < 4; i++) {
                int y = 285 + i * 78;
                text(ST[i], F30, 70, y, WHITE);
                text(SD[i], F16, 70, y + 38, WHITE, 0, 160);
                toggle(520, y + 4, sv[i], th.accent);
            }
            text("WinNX 0.4 - Windows 8.1-style shell", F16, 70, 618, WHITE, 0, 130);
            text("Your files autosave to sdmc:/switch/WinNX/", F16, 70, 640, WHITE, 0, 130);
            rect(690, 118, 2, 520, WHITE, 40);
            text("Homebrew menu", F30, 730, 130, WHITE);
            text("Replace hbmenu so the Album launch opens WinNX.", F16, 730, 174, WHITE, 0, 190);
            text("Your original hbmenu is backed up first.", F16, 730, 198, WHITE, 0, 190);
            bool c1 = gHbConfirm > 0 && gHbWhich == 1, c2 = gHbConfirm > 0 && gHbWhich == 2;
            btn(730, 240, 480, 60, c1 ? "Tap again to confirm" : "Make WinNX my homebrew menu", c1 ? Col{209, 52, 56} : th.accent, WHITE);
            btn(730, 314, 480, 60, c2 ? "Tap again to confirm" : "Restore original hbmenu", c2 ? Col{209, 52, 56} : Col{90, 90, 96}, WHITE);
            btn(730, 388, 480, 60, "Open original hbmenu now", Col{90, 90, 96}, WHITE);
            text(std::string("WinNX is the homebrew menu: ") + (gHbIsWinNX ? "Yes" : "No"), F22, 730, 468, WHITE);
            text(std::string("Backup of original: ") + (gHbBackup ? "saved" : "none"), F22, 730, 500, WHITE);
            int my = 540;
            for (auto& l : wrapText(gHbMsg, F16, 480)) { text(l, F16, 730, my, Col{255, 230, 140}); my += 22; }
            text("Rescue: hold B + Y while WinNX starts to jump to the original hbmenu.", F16, 730, 626, WHITE, 0, 140);
            break;
        }
        case A_POWER: {
            rect(0, 0, W, UIH, bg);
            appTitle("Power");
            text("Exit WinNX, lock the screen, or open the original hbmenu.", F30, 70, 170, WHITE);
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
        case A_EXCEL: {
            Col green = Col{33, 115, 70};
            rect(0, 0, W, UIH, WHITE);
            rect(0, 34, W, 50, green);
            text("Excel 2016", F30, 24, 42, WHITE);
            text("Book1 - autosaved", F22, 250, 46, WHITE, 0, 200);
            btn(1000, 40, 90, 38, "New", Col{60, 150, 100}, WHITE);
            btn(1100, 40, 90, 38, "Save", Col{60, 150, 100}, WHITE);
            rect(0, 90, W, 44, Col{245, 245, 245});
            rect(0, 133, W, 1, Col{200, 200, 200});
            text("fx", F22, 16, 98, green);
            std::string fb = xEditing ? xEdit : xs[xr][xc];
            if (xEditing && ((SDL_GetTicks() / 500) % 2 == 0)) fb += "|";
            text(fb, F22, 64, 98, DARK);
            rect(0, 140, W, 32, Col{232, 232, 232});
            int rows = xRows();
            for (int vc = 0; vc < XVC; vc++) {
                int c = xScrC + vc;
                if (c >= XC) break;
                std::string h(1, (char)('A' + c));
                text(h, F22, XHDRW + vc * XCOLW + XCOLW / 2, 144, Col{70, 70, 70}, 1);
                rect(XHDRW + vc * XCOLW, 140, 1, 32, Col{200, 200, 200});
            }
            for (int vr = 0; vr < rows; vr++) {
                int r = xScrR + vr;
                if (r >= XR) break;
                int y = XTOP + vr * XROWH;
                rect(0, y, XHDRW, XROWH, Col{232, 232, 232});
                text(std::to_string(r + 1), F22, XHDRW / 2, y + 6, Col{70, 70, 70}, 1);
                rect(0, y + XROWH - 1, W, 1, Col{215, 215, 215});
                for (int vc = 0; vc < XVC; vc++) {
                    int c = xScrC + vc;
                    if (c >= XC) break;
                    int x = XHDRW + vc * XCOLW;
                    rect(x, y, 1, XROWH, Col{215, 215, 215});
                    bool isSel = (r == xr && c == xc);
                    std::string d = (isSel && xEditing) ? xEdit : cellDisplay(r, c);
                    SDL_Rect cr = {x + 1, y + 1, XCOLW - 1, XROWH - 1};
                    SDL_RenderSetClipRect(gR, &cr);
                    if (isSel && xEditing) rect(cr.x, cr.y, cr.w, cr.h, WHITE);
                    if (looksNumber(d) && !(isSel && xEditing)) text(d, F22, x + XCOLW - 8, y + 6, DARK, 2);
                    else text(d, F22, x + 8, y + 6, DARK);
                    SDL_RenderSetClipRect(gR, nullptr);
                }
            }
            if (xr >= xScrR && xr < xScrR + rows && xc >= xScrC && xc < xScrC + XVC)
                outline(XHDRW + (xc - xScrC) * XCOLW, XTOP + (xr - xScrR) * XROWH, XCOLW + 1, XROWH + 1, 3, green);
            break;
        }
        case A_STORE: drawStore(th, bottom); break;
        case A_BROWSER: drawBrowser(bottom); break;
        case A_PAINT: drawPaint(); break;
        case A_2048: draw2048(); break;
        case A_MINES: drawMines(); break;
        case A_SNAKE: drawSnake(); break;
        case A_SOLITAIRE: drawSolitaire(); break;
        // @@DRAW@@
        case A_PHOTOS: {
            rect(0, 0, W, UIH, BLACK);
            if (phTex) {
                float s = std::min((float)W / phW, (float)UIH / phH);
                int dw = (int)(phW * s), dh = (int)(phH * s);
                SDL_Rect d = {(W - dw) / 2, (UIH - dh) / 2, dw, dh};
                SDL_RenderCopy(gR, phTex, nullptr, &d);
            } else if (gPhotos.empty()) {
                text("No photos found", F44, W / 2, 240, WHITE, 1);
                text("Looked in Nintendo/Album, Pictures and switch/WinNX/photos on your SD card.", F22, W / 2, 310, WHITE, 1, 170);
            } else text("Couldn't open this picture", F30, W / 2, 300, WHITE, 1);
            if (!gPhotos.empty()) {
                rect(0, UIH - 40, W, 40, BLACK, 150);
                size_t s = gPhotos[phIdx].find_last_of('/');
                text(std::to_string(phIdx + 1) + " / " + std::to_string(gPhotos.size()) + "   " + gPhotos[phIdx].substr(s + 1), F16, 16, UIH - 31, WHITE, 0, 220);
                text("Swipe or tap the sides", F16, W - 16, UIH - 31, WHITE, 2, 140);
            }
            break;
        }
        case A_GAMES: {
            rect(0, 0, W, UIH, bg);
            appTitle("Games");
            text("Tap to launch homebrew from /switch", F22, 76, 124, WHITE, 0, 190);
            int n = (int)gGames.size();
            int maxS = std::max(0, n * 70 - (UIH - 170));
            gamesScroll = std::max(0, std::min(gamesScroll, maxS));
            SDL_Rect clip = {0, 170, W, UIH - 170};
            SDL_RenderSetClipRect(gR, &clip);
            for (int i = 0; i < n; i++) {
                int y = 170 + i * 70 - gamesScroll;
                if (y + 64 < 170 || y > UIH) continue;
                rect(70, y, 1140, 64, WHITE, 28);
                text(gGames[i].name, F30, 90, y + 14, WHITE);
                text(gGames[i].path.substr(5), F16, 1190, y + 22, WHITE, 2, 150);
            }
            SDL_RenderSetClipRect(gR, nullptr);
            if (n == 0) text("No .nro files found in /switch", F30, 76, 200, WHITE, 0, 200);
            break;
        }
        default: {
            rect(0, 0, W, UIH, bg);
            appTitle(appName(curApp).c_str());
            const char* msg = "This app isn't available yet.";
            if (curApp == A_MAIL || curApp == A_WEATHER) msg = "Needs an internet connection. Coming in a later version.";
            if (curApp == A_MUSIC) msg = "Audio playback is coming in a later version.";
            text(msg, F30, 70, 190, WHITE);
            text("Press + or tap the Windows button to go back.", F22, 70, 240, WHITE, 0, 180);
            drawIcon(curApp, W - 200, 200, bg);
            break;
        }
    }
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

static void settingsTap(int x, int y, const Theme& th) {
    (void)th;
    for (int i = 0; i < 4; i++) if (inR(x, y, 70 + i * 100, 178, 80, 80)) { gTheme = i; saveCfg(); return; }
    for (int i = 0; i < 4; i++) {
        int ry = 285 + i * 78;
        if (inR(x, y, 70, ry - 6, 600, 74)) {
            if (i == 0) gBurn = !gBurn; else if (i == 1) g24 = !g24; else if (i == 2) gLockScr = !gLockScr; else gBootAnim = !gBootAnim;
            saveCfg(); return;
        }
    }
    if (inR(x, y, 730, 240, 480, 60)) {
        if (gHbConfirm > 0 && gHbWhich == 1) { gHbConfirm = 0; gHbWhich = 0; hbInstall(); }
        else { gHbWhich = 1; gHbConfirm = 4; gHbMsg.clear(); }
    } else if (inR(x, y, 730, 314, 480, 60)) {
        if (gHbConfirm > 0 && gHbWhich == 2) { gHbConfirm = 0; gHbWhich = 0; hbRestore(); }
        else { gHbWhich = 2; gHbConfirm = 4; gHbMsg.clear(); }
    } else if (inR(x, y, 730, 388, 480, 60)) hbOpenOriginal();
}
static void appTap(int x, int y) {
    switch (curApp) {
        case A_CALC:
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
        case A_EXCEL:
            if (inR(x, y, 1000, 40, 90, 38)) {
                commitCell(); saveAll();
                backupFile(DIR_ + "sheet1.csv");
                for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear();
                xr = xc = xScrR = xScrC = 0; xDirty = true;
            } else if (inR(x, y, 1100, 40, 90, 38)) { commitCell(); xDirty = true; saveAll(); }
            else if (y >= 90 && y < 134) { openKb(3); startEdit(); }
            else if (x >= XHDRW && y >= XTOP) {
                int vc = (x - XHDRW) / XCOLW, vr = (y - XTOP) / XROWH;
                int r = xScrR + vr, c = xScrC + vc;
                if (vr < xRows() && vc < XVC && r < XR && c < XC) {
                    if (r == xr && c == xc) { openKb(3); startEdit(); }
                    else { commitCell(); xr = r; xc = c; xEnsureVisible(); }
                }
            }
            break;
        case A_STORE: storeTap(x, y); break;
        case A_BROWSER: browserTap(x, y); break;
        case A_PAINT: paintTap(x, y); break;
        case A_2048: tap2048(x, y); break;
        case A_MINES: tapMines(x, y); break;
        case A_SNAKE: tapSnake(x, y); break;
        // @@TAP@@
        case A_PHOTOS:
            if (x < W / 3) photoNav(-1);
            else if (x > 2 * W / 3) photoNav(1);
            break;
        case A_GAMES: {
            if (y < 170) break;
            int i = (y - 170 + gamesScroll) / 70;
            if (i >= 0 && i < (int)gGames.size() && ((y - 170 + gamesScroll) % 70) < 64) {
                saveAll();
                if (R_SUCCEEDED(envSetNextLoad(gGames[i].path.c_str(), gGames[i].path.c_str()))) gRun = false;
            }
            break;
        }
        default: break;
    }
}
static void appDrag(int dx, int dy) {
    static int accX = 0, accY = 0;
    switch (curApp) {
        case A_NOTES: gNote.scroll -= dy; break;
        case A_WORD: if (wView == 1) wPicScroll -= dy; else wScroll -= dy; break;
        case A_GAMES: gamesScroll -= dy; break;
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
static SDL_Texture* gLockTex = nullptr;
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
    text(bb, F22, 56, o + UIH - 100, WHITE, 0, 220);
    float bounce = sinf(SDL_GetTicks() / 350.0f) * 5.0f;
    int hy = o + UIH - 62 + (int)bounce;
    line(W / 2 - 14, hy + 8, W / 2, hy - 4, 3, WHITE, 200);
    line(W / 2 + 14, hy + 8, W / 2, hy - 4, 3, WHITE, 200);
    text("Swipe up to unlock", F16, W / 2, o + UIH - 40, WHITE, 1, 200);
    if (o < 0) rect(0, o + UIH, W, 5, BLACK, 70);
}
static void unlockNow() {
    if (uiState == 1 && !lockAnim) { lockAnim = true; introT = 0; }
}

// ---- search charm helpers
static std::vector<int> searchHits() {
    std::vector<int> v;
    std::string q = lower(gQuery);
    if (q.empty()) return v;
    for (int i = 0; i < (int)gTiles.size(); i++) if (lower(gTiles[i].label).find(q) != std::string::npos) v.push_back(i);
    return v;
}
static void gotoTile(int t) {
    closeOverlays();
    if (kbTarget == 6) kbOpen = false;
    if (t < 0 || t >= (int)gTiles.size()) return;
    if (curApp == gTiles[t].app) return;
    pendingTile = t;
    if (curApp >= 0) closeApp(false);
}
static void searchGo() {
    std::vector<int> h = searchHits();
    if (!h.empty()) { gotoTile(h[0]); return; }
    if (gQuery.find_first_not_of(' ') == std::string::npos) return;
    closeOverlays(); kbOpen = false;
    gUrl = gQuery;
    browserGo();
}

static const int CH_X = W - 120;
static void drawCharms() {
    if (charmsP <= 0.01f) return;
    float e = ease(charmsP);
    int off = (int)((1.0f - e) * 120);
    Col pb = {14, 14, 16};
    rect(CH_X + off, 0, 120, UIH, pb, 245);
    static const char* CN[5] = {"Search", "Share", "Start", "Devices", "Settings"};
    for (int i = 0; i < 5; i++) {
        int cy = 90 + i * 110, cx = CH_X + off + 60;
        Col c = WHITE;
        switch (i) {
            case 0: ring(cx - 6, cy - 6, 18, 4, c, pb); line(cx + 7, cy + 7, cx + 20, cy + 20, 5, c); break;
            case 1: disc(cx - 16, cy, 7, c); disc(cx + 14, cy - 14, 7, c); disc(cx + 14, cy + 14, 7, c);
                    line(cx - 16, cy, cx + 14, cy - 14, 2, c); line(cx - 16, cy, cx + 14, cy + 14, 2, c); break;
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
        const Tile& t = gTiles[h[i]];
        rect(px + 20, y, 400, 58, Col{44, 44, 52});
        rect(px + 20, y, 58, 58, t.color);
        text(t.label, F22, px + 92, y + 14, WHITE);
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
        y += 140;
    }
}
static void overlayTap(int x, int y) {
    if (searchOpen) {
        if (x < W - 440) { closeOverlays(); if (kbTarget == 6) kbOpen = false; return; }
        if (inR(x, y, W - 420, 92, 400, 52)) { openKb(6); return; }
        std::vector<int> h = searchHits();
        int bottom = kbOpen ? KBTOP : UIH;
        int maxRows = std::max(1, (bottom - 170) / 64);
        int row = (y - 164) / 64;
        if (y >= 164 && row >= 0) {
            if (row < (int)h.size() && row < maxRows - 1) gotoTile(h[row]);
            else if (row == std::min((int)h.size(), maxRows - 1) && !gQuery.empty()) {
                closeOverlays(); kbOpen = false; gUrl = gQuery; browserGo();
            }
        }
        return;
    }
    if (charmsOpen) {
        if (x < CH_X) { closeOverlays(); return; }
        int i = (y - 35) / 110;
        if (y < 35 || i < 0 || i > 4) return;
        switch (i) {
            case 0: charmsOpen = false; searchOpen = true; gQuery.clear(); openKb(6); break;
            case 1: toast("Nothing to share here"); break;
            case 2: closeOverlays(); if (curApp >= 0) closeApp(false); break;
            case 3: toast("Pair controllers in the Switch Controllers menu"); break;
            default: {
                closeOverlays();
                int t = tileIndexFor(A_SETTINGS);
                if (t >= 0 && curApp != A_SETTINGS) { pendingTile = t; if (curApp >= 0) closeApp(false); }
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
                closeOverlays();
                if (a != curApp) { int t = tileIndexFor(a); if (t >= 0) { pendingTile = t; if (curApp >= 0) closeApp(false); } }
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
int main(int argc, char** argv) {
    if (argc > 0 && argv && argv[0]) gArg0 = argv[0];
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
    loadCfg();
    loadUser();
    rebuildTiles();

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    gWin = SDL_CreateWindow("WinNX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
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
    uiState = gBootAnim ? 0 : (gLockScr ? 1 : 2);
    updateNow();
    srand((unsigned)SDL_GetTicks());
    lastInput = SDL_GetTicks();
    Uint32 prev = lastInput, lastClock = 0, lastBat = 0, lastSave = 0, kbHeldT = 0, kbRep = 0;
    bool wasDown = false, tMoved = false, tPressWin = false, tTop = false, tKb = false, tApp = false, tOv = false, wasRaw = false, lkDrag = false;
    int tEdge = 0, lkY0 = 0, lkTapMoved = 0;
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
        if (kdRaw) { lastInput = now; dimA = 0; }
        HidTouchScreenState ts = {0};
        hidGetTouchScreenStates(&ts, 1);
        bool downRaw = ts.count > 0;
        int rawX = downRaw ? (int)ts.touches[0].x : 0, rawY = downRaw ? (int)ts.touches[0].y : 0;
        // ---- boot animation and lock screen
        if (uiState == 0) {
            bootT += dt;
            if ((kdRaw || (downRaw && !wasRaw)) && bootT > 0.4f && bootT < BOOT_END - 0.5f) bootT = BOOT_END - 0.5f;
            if (bootT >= BOOT_END) { uiState = gLockScr ? 1 : 2; lockOff = 0; lockAnim = false; introT = 0; }
        } else if (uiState == 1) {
            if (lockAnim) {
                lockOff -= (UIH * 2.4f + 200) * dt;
                if (lockOff <= -UIH) { lockOff = 0; lockAnim = false; uiState = 2; introT = 0; }
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
        if (kd & HidNpadButton_Plus) winPress();
        if (curApp < 0 && appDir == 0) {
            if (kd & HidNpadButton_AnyLeft) { navMode = true; navigate(0); }
            if (kd & HidNpadButton_AnyRight) { navMode = true; navigate(1); }
            if (kd & HidNpadButton_AnyUp) { navMode = true; navigate(2); }
            if (kd & HidNpadButton_AnyDown) { navMode = true; navigate(3); }
            if ((kd & HidNpadButton_A) && navMode) launch(sel);
        } else if (curApp >= 0 && appDir == 0) {
            if (kd & HidNpadButton_B) closeApp(false);
            if (kd & HidNpadButton_X) closeApp(true);
            appKeys(kd);
            if (curApp == A_PHOTOS) {
                if (kd & HidNpadButton_AnyLeft) photoNav(-1);
                if (kd & HidNpadButton_AnyRight) photoNav(1);
            }
        }

        // ---- touch
        bool down = downRaw && uiState == 2;
        int tx = down ? rawX : tLX, ty = down ? rawY : tLY;
        bool appOpen = curApp >= 0 && appDir == 0;
        if (down && !wasDown) {
            lastInput = now; dimA = 0; navMode = false;
            tDX = tx; tDY = ty; tLX = tx; tLY = ty; tMoved = false; scrollV = 0;
            tTop = false; tKb = false; tApp = false; kbHeld = -1; tOv = false; tEdge = 0;
            tPressWin = inWinBtn(tx, ty);
            if (!tPressWin && overlayOpen()) {
                if (kbOpen && ty >= KBTOP && ty < UIH) {
                    buildKeys();
                    int k = kbHit(tx, ty);
                    if (k >= 0) { kbHeld = k; kbHeldT = now; kbRep = now; kbAction(gKeys[k].act); }
                    tKb = true;
                } else { tOv = true; tKb = true; }
            } else if (!tPressWin && ty < UIH && tx >= W - 12) tEdge = 1;
            else if (!tPressWin && ty < UIH && tx <= 12) tEdge = 2;
            pressTile = (curApp < 0 && appDir == 0 && !tPressWin && !tKb && !tEdge) ? hitTile(tx, ty) : -1;
            if (appOpen && !tPressWin && !tKb && !tEdge) {
                if (ty < 30) { barT = 3.5f; tTop = true; }
                else if (barP > 0.5f && ty < 44) tTop = true;
                else if (kbOpen && ty >= KBTOP && ty < UIH) {
                    buildKeys();
                    int k = kbHit(tx, ty);
                    if (k >= 0) { kbHeld = k; kbHeldT = now; kbRep = now; kbAction(gKeys[k].act); }
                    tKb = true;
                } else { tApp = true; appTouch(0, tx, ty); }
            }
        } else if (down && wasDown) {
            lastInput = now;
            int dx = tx - tLX, dy = ty - tLY;
            if (!tMoved && (abs(tx - tDX) > 14 || abs(ty - tDY) > 14)) tMoved = true;
            if (tKb && kbHeld >= 0 && kbHeld < (int)gKeys.size() && gKeys[kbHeld].act == "BS" && now - kbHeldT > 450 && now - kbRep > 70) {
                kbAction("BS"); kbRep = now;
            }
            if (tEdge == 1 && tx < tDX - 36) { charmsOpen = true; tEdge = 3; }
            if (tEdge == 2 && tx > tDX + 36) { switchOpen = true; tEdge = 3; }
            if (tApp && tMoved) appTouch(1, tx, ty);
            if (tMoved && !tKb && !tPressWin && !tEdge) {
                if (curApp < 0 && appDir == 0) {
                    scrollX = clampf(scrollX - dx, 0, maxScroll);
                    scrollV = (float)-dx;
                    pressTile = -1;
                } else if (appOpen && !tTop) appDrag(dx, dy);
            }
            tLX = tx; tLY = ty;
        } else if (!down && wasDown) {
            if (tApp) { appTouch(2, tLX, tLY); tApp = false; }
            if (tOv) { if (!tMoved) overlayTap(tLX, tLY); tOv = false; tKb = false; kbHeld = -1; }
            else if (tEdge) { if (!tMoved && tEdge == 1) charmsOpen = true; if (!tMoved && tEdge == 2) switchOpen = true; tEdge = 0; }
            else if (tKb) { kbHeld = -1; }
            else if (tTop) {
                if (!tMoved && barP > 0.5f && tLY < 44 && appDir == 0) {
                    if (tLX >= W - 90) closeApp(true);
                    else if (tLX >= W - 180) closeApp(false);
                }
            } else if (!tMoved && appDir == 0) {
                if (inWinBtn(tLX, tLY)) winPress();
                else if (curApp < 0) { int t = hitTile(tLX, tLY); if (t >= 0) { sel = t; launch(t); } }
                else appTap(tLX, tLY);
            } else if (tMoved && appOpen && curApp == A_PHOTOS) {
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
            appT += appDir * dt * 3.6f;
            if (appT >= 1) { appT = 1; appDir = 0; }
            else if (appT <= 0) {
                appT = 0; appDir = 0;
                if (gCloseReset) appReset(curApp);
                gCloseReset = false;
                curApp = -1;
            }
        }
        if (appDir == 0 && curApp < 0 && pendingTile >= 0) { int t = pendingTile; pendingTile = -1; launch(t); }
        for (int a = 0; a < A_COUNT; a++) {
            if (storeProg[a] > 0) {
                storeProg[a] += dt * 0.7f;
                if (storeProg[a] >= 1) { storeProg[a] = 0; gInst[a] = true; rebuildTiles(); saveCfg(); }
            }
        }
        appUpdate(dt);
        charmsP += ((charmsOpen ? 1.0f : 0.0f) - charmsP) * std::min(1.0f, dt * 14.0f);
        switchP += ((switchOpen ? 1.0f : 0.0f) - switchP) * std::min(1.0f, dt * 14.0f);
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

        // ---- render
        if (gTarget) SDL_SetRenderTarget(gR, gTarget);
        SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
        SDL_RenderClear(gR);
        drawStart(down, tMoved, pressTile);
        if (curApp >= 0) {
            if (appT < 1) {
                SDL_Rect a = tileScreen(animTile < 0 ? 0 : animTile);
                float e = ease(appT);
                int rx = (int)(a.x + (0 - a.x) * e), ry = (int)(a.y + (0 - a.y) * e);
                int rw = (int)(a.w + (W - a.w) * e), rh = (int)(a.h + (UIH - a.h) * e);
                rect(rx, ry, rw, rh, appBg(curApp));
            } else {
                drawApp();
                if (kbOpen) drawKb();
                drawTopBar();
            }
        }
        if (uiState == 2 || lockAnim) {
            if (charmsP > 0.01f || switchP > 0.01f || searchP > 0.01f) rect(0, 0, W, UIH, BLACK, 70);
            drawSwitcher();
            drawCharms();
            drawSearchCharm();
            if (kbOpen && kbTarget == 6) drawKb();
            drawToast();
        }
        if (uiState == 0) drawBoot(bootT);
        else if (uiState == 1) drawLock(lockOff);
        rect(0, UIH, W, H - UIH, BLACK);
        bool wb = downRaw && tPressWin;
        winLogo(W / 2, UIH + (H - UIH) / 2, 26, WHITE, wb ? 255 : 105);
        if (dimA > 1) rect(0, 0, W, H, BLACK, (int)dimA);

        if (gTarget) {
            SDL_SetRenderTarget(gR, nullptr);
            SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
            SDL_RenderClear(gR);
            SDL_Rect dst = {sx, sy, W, H};
            SDL_RenderCopy(gR, gTarget, nullptr, &dst);
        }
        SDL_RenderPresent(gR);
    }

    commitCell();
    saveAll();
    saveCfg();
    if (phTex) SDL_DestroyTexture(phTex);
    if (gLockTex) SDL_DestroyTexture(gLockTex);
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
